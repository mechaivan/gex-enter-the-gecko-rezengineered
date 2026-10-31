/**
 * Offline tests for the function map.
 *
 *   node --test tools/gexmap/test.mjs
 *
 * The first group builds tiny synthetic `.text` programs with known instruction
 * boundaries, so every rule of `analyze()` is exercised without the game binary
 * (which is never committed). The second group checks the invariants of the
 * committed artifacts in `analysis/EU/`, which is what makes the published map
 * self-consistent. The third group checks the frame-step data in `frame.mjs`.
 */

import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { analyze, transitiveImports } from "./analyze.mjs";
import { SEGMENTS, buildSegments, loadArtifacts } from "./segments.mjs";
import { FRAME_STEP, UPDATE_CALLS, RENDER_CALLS, SEMANTICS, validateFrame } from "./frame.mjs";
import { INIT_ROUTES, DEAD_ROUTE, AUDIO_SEMANTICS, VTABLE_CALLS, DSOUND_GLOBALS, validateAudio } from "./audio.mjs";

const IMAGE_BASE = 0x400000;
const TEXT_VA = 0x401000;
const TEXT_RAW = 0x200;

/**
 * Assemble a synthetic program. Each item is { text, hex, repeat }:
 *   - `hex` is the instruction's bytes and always written to the image;
 *   - `text: null` writes data bytes that the decoder did not turn into an
 *     instruction (a gap the boundary check must inspect);
 *   - `repeat` emits the same 1-byte instruction N times (padding).
 */
function build(items, size) {
  let va = TEXT_VA;
  const instructions = [];
  const written = [];
  for (const item of items) {
    const bytes = Buffer.from(item.hex, "hex");
    for (let n = 0; n < (item.repeat ?? 1); n++) {
      written.push({ addr: va, bytes });
      if (item.text !== null) instructions.push({ addr: va, text: item.text, bytes: bytes.length });
      va += bytes.length;
    }
  }
  const virtualSize = Math.max(size ?? 0, va - TEXT_VA);
  const buffer = Buffer.alloc(TEXT_RAW + virtualSize, 0);
  for (const item of written) buffer.set(item.bytes, TEXT_RAW + (item.addr - TEXT_VA));
  const pe = {
    imageBase: IMAGE_BASE,
    entryPoint: 0,
    offsetOf: (rva) => (rva >= 0x1000 && rva - 0x1000 < virtualSize ? TEXT_RAW + (rva - 0x1000) : null),
  };
  return { buffer, pe, text: { virtualAddress: 0x1000, virtualSize }, instructions };
}

const imports = new Map([
  [0x403000, { dll: "TEST.dll", func: "Alpha" }],
  [0x403004, { dll: "TEST.dll", func: "Beta" }],
  [0x403008, { dll: "TEST.dll", func: "Gamma" }],
]);

const noRepair = () => {
  throw new Error("disassembleRange should not be called");
};

test("call target, import thunk, prologue candidate and stub block", () => {
  const program = build(
    [
      { text: "push ebp", hex: "55" },
      { text: "mov ebp, esp", hex: "8bec" },
      { text: "call 0x401010", hex: "e808000000" },
      { text: "ret", hex: "c3" },
      { text: "int3", hex: "cc", repeat: 7 },
      { text: "jmp dword ptr [0x403000]", hex: "ff2500304000" },
      { text: "int3", hex: "cc", repeat: 4 },
      { text: "push ebp", hex: "55" },
      { text: "mov ebp, esp", hex: "8bec" },
      { text: "ret", hex: "c3" },
      { text: "jmp dword ptr [0x403004]", hex: "ff2504304000" },
    ],
    0x24,
  );

  const result = analyze({ ...program, imports, disassembleRange: noRepair });
  const byStart = new Map(result.functions.map((fn) => [fn.start, fn]));

  assert.deepEqual(
    result.functions.map((fn) => fn.start),
    [0x401000, 0x401010, 0x40101a, 0x40101e],
  );
  assert.equal(byStart.get(0x401000).basis, "text-start");
  assert.equal(byStart.get(0x401010).basis, "call");
  assert.equal(byStart.get(0x401010).kind, "thunk");
  assert.equal(byStart.get(0x401010).symbol, "TEST.dll!Alpha");
  assert.equal(byStart.get(0x40101a).basis, "prologue");
  assert.equal(byStart.get(0x40101a).kind, "func");
  assert.equal(byStart.get(0x40101e).basis, "thunk");
  assert.equal(byStart.get(0x40101e).kind, "thunk");
  assert.equal(byStart.get(0x40101e).symbol, "TEST.dll!Beta");
  assert.deepEqual(result.prologueCandidates, [0x40101a]);
  assert.deepEqual(
    result.edges.map((edge) => [edge.from, edge.to, edge.kind]),
    [[0x401000, 0x401010, "call"]],
  );
  assert.equal(result.stats.thunks, 2);
  assert.equal(result.stats.startFromPrologue, 1);
  assert.equal(result.stats.startFromThunks, 1);
});

test("payload bytes between a terminator and a prologue are not a boundary", () => {
  const program = build(
    [
      { text: "push ebp", hex: "55" },
      { text: "mov ebp, esp", hex: "8bec" },
      { text: "ret", hex: "c3" },
      { text: null, hex: "01" },
      { text: "push ebp", hex: "55" },
      { text: "mov ebp, esp", hex: "8bec" },
      { text: "ret", hex: "c3" },
    ],
    0x10,
  );

  const result = analyze({ ...program, imports, disassembleRange: noRepair });
  assert.deepEqual(result.functions.map((fn) => fn.start), [0x401000]);
  assert.deepEqual(result.prologueCandidates, []);
});

test("an import jump inside a function is not a thunk", () => {
  const program = build(
    [
      { text: "push ebp", hex: "55" },
      { text: "mov ebp, esp", hex: "8bec" },
      { text: "mov eax, 1", hex: "b801000000" },
      { text: "jmp dword ptr [0x403000]", hex: "ff2500304000" },
      { text: "ret", hex: "c3" },
    ],
    0x10,
  );

  const result = analyze({ ...program, imports, disassembleRange: noRepair });
  assert.deepEqual(result.functions.map((fn) => fn.start), [0x401000]);
  assert.equal(result.functions[0].kind, "func");
  assert.equal(result.stats.thunks, 0);
  assert.ok(result.functions[0].imports.has("TEST.dll!Alpha"));
});

test("a call into a desynchronised region is re-decoded and becomes a start", () => {
  const program = build(
    [
      { text: "call 0x401010", hex: "e80b000000" },
      { text: "ret", hex: "c3" },
      { text: "int3", hex: "cc", repeat: 10 },
    ],
    0x20,
  );

  let requested = null;
  const result = analyze({
    ...program,
    imports,
    disassembleRange: (start, stop) => {
      requested = { start, stop };
      return [
        { addr: 0x401010, text: "xor eax, eax", bytes: 2 },
        { addr: 0x401012, text: "ret", bytes: 1 },
      ];
    },
  });

  assert.deepEqual(requested, { start: 0x401010, stop: 0x401020 });
  assert.equal(result.repairs.length, 1);
  assert.equal(result.repairs[0].start, 0x401010);
  assert.deepEqual(result.functions.map((fn) => fn.start), [0x401000, 0x401010]);
  assert.equal(result.functions[1].basis, "call");
  assert.deepEqual(result.edges.map((edge) => [edge.from, edge.to]), [[0x401000, 0x401010]]);
});

test("transitiveImports reports depth through the direct call graph", () => {
  const functions = [
    { start: 0x1000, imports: new Map([["A", 1]]) },
    { start: 0x2000, imports: new Map([["B", 1]]) },
    { start: 0x3000, imports: new Map() },
  ];
  const edges = [
    { from: 0x1000, to: 0x2000 },
    { from: 0x2000, to: 0x3000 },
  ];
  const reached = transitiveImports(functions, edges, 0x1000);
  assert.deepEqual([...reached.keys()].sort(), ["A", "B"]);
  assert.equal(reached.get("A"), 0);
  assert.equal(reached.get("B"), 1);
});

const eu = new URL("../../analysis/EU/", import.meta.url);
const readJson = (name) => JSON.parse(readFileSync(new URL(name, eu), "utf8"));
const rows = (name) =>
  readFileSync(new URL(name, eu), "utf8")
    .split(/\r?\n/)
    .filter((line) => line.length && !line.startsWith("#"))
    .map((line) => line.split("\t"));

test("committed artifacts: counts, sizes and coverage agree", () => {
  const summary = readJson("summary.json");
  const functions = rows("functions.tsv");
  const calls = rows("calls.tsv");

  assert.equal(summary.schema, 1);
  assert.equal(functions.length, summary.counts.functions);
  assert.equal(calls.length, summary.counts.callEdges + summary.counts.tailEdges);
  assert.equal(
    functions.reduce((sum, row) => sum + Number.parseInt(row[1], 10), 0),
    summary.counts.functionBytes,
  );
  assert.equal(summary.counts.functionBytes, summary.text.bytes);
  assert.equal(summary.counts.coveragePercent, 100);
  assert.equal(summary.repairs.length, summary.counts.repairs);
});

test("committed artifacts: every start is ordered, non-overlapping and classified", () => {
  const summary = readJson("summary.json");
  const functions = rows("functions.tsv");
  const bases = { call: 0, entry: 0, "text-start": 0, prologue: 0, thunk: 0 };

  let previousEnd = -1;
  for (const row of functions) {
    const start = Number.parseInt(row[0], 16);
    const size = Number.parseInt(row[1], 10);
    assert.ok(start >= previousEnd, `${row[0]} overlaps the previous function`);
    assert.ok(bases[row[3]] !== undefined, `${row[0]} has unknown basis ${row[3]}`);
    bases[row[3]]++;
    if (row[2] === "thunk") {
      assert.ok(row[7], `${row[0]} is a thunk without an import`);
      assert.ok(!row[7].includes(";"), `${row[0]} forwards to more than one import`);
      assert.equal(row[4], row[7], `${row[0]} symbol does not match its import`);
      assert.ok(size >= 6, `${row[0]} is smaller than a 6 B stub`);
    }
    previousEnd = start + size;
  }

  assert.equal(bases.call, summary.counts.startFromCalls);
  assert.equal(bases.entry, summary.counts.startFromEntry);
  assert.equal(bases["text-start"], summary.counts.startFromTextStart);
  assert.equal(bases.prologue, summary.counts.startFromPrologue);
  assert.equal(bases.thunk, summary.counts.startFromThunks);
  assert.equal(
    bases.call + bases.entry + bases["text-start"] + bases.prologue + bases.thunk,
    summary.counts.functions,
  );
  assert.equal(
    functions.filter((row) => row[2] === "thunk").length,
    summary.counts.thunks,
  );
});

test("committed artifacts: every confirmed call start is a callee", () => {
  const functions = rows("functions.tsv");
  const calls = rows("calls.tsv");
  const callees = new Set(calls.map((row) => Number.parseInt(row[1], 16)));
  for (const row of functions) {
    if (row[3] !== "call") continue;
    assert.ok(callees.has(Number.parseInt(row[0], 16)), `${row[0]} is a call start but no edge reaches it`);
  }
});

test("committed artifacts: the ByteMatch reconciliation still adds up", () => {
  const summary = readJson("summary.json");
  const r = summary.reconciliation;
  assert.ok(r, "summary has no reconciliation block");
  assert.equal(
    r.report_entry_confirmed_start +
      r.report_entry_prologue_start +
      r.report_entry_boundary_but_not_a_start +
      r.report_entry_not_an_instruction_boundary,
    r.bytematch_functions,
  );
  assert.equal(r.report_entry_confirmed_start, summary.counts.startFromCalls);
  assert.equal(summary.anchors.total, 24);
  assert.equal(summary.anchors.outside_every_function, 0);
});

const euDir = fileURLToPath(new URL("../../analysis/EU/", import.meta.url));
const segmentModel = loadArtifacts(euDir);

test("segments: the committed definitions validate against the artifacts", () => {
  const result = buildSegments(segmentModel);
  assert.deepEqual(result.problems, []);
  assert.equal(result.summary.segments, 7);
  assert.equal(result.summary.members, 65);
  assert.deepEqual(result.summary.byConfidence, { ALTA: 35, MEDIA: 22, BAJA: 8 });
  assert.equal(result.summary.crossSegmentEdges, 20);
  assert.equal(result.summary.reachableFromWinMain, 840);
});

test("segments: every member is game code, unique, and no prologue is ALTA", () => {
  const byStart = new Map(segmentModel.functions.map((fn) => [fn.start, fn]));
  const seen = new Set();
  let prologues = 0;
  for (const segment of SEGMENTS) {
    assert.ok(segment.name.startsWith("HYPOTHESIS:"), `${segment.id} is not marked HYPOTHESIS`);
    for (const member of segment.members) {
      const start = Number.parseInt(member.addr, 16);
      assert.ok(!seen.has(start), `${member.addr} appears twice`);
      seen.add(start);
      const fn = byStart.get(start);
      assert.ok(fn, `${member.addr} is not a mapped start`);
      assert.equal(fn.kind, "func", `${member.addr} is a ${fn.kind}`);
      if (fn.basis === "prologue") {
        prologues++;
        assert.equal(member.confidence, "BAJA", `${member.addr} is a prologue but not BAJA`);
      }
    }
  }
  assert.equal(prologues, 7);
});

test("segments: the validator rejects an inflated confidence and a false evidence code", () => {
  const clone = () =>
    SEGMENTS.map((segment) => ({ ...segment, members: segment.members.map((member) => ({ ...member })) }));

  const inflated = clone();
  inflated[0].members[0].confidence = "BAJA";
  const inflatedResult = buildSegments(segmentModel, inflated);
  assert.ok(inflatedResult.problems.some((p) => p.includes("declares BAJA")));

  const invented = clone();
  invented[0].members[0].evidence = ["O"];
  const inventedResult = buildSegments(segmentModel, invented);
  assert.ok(inventedResult.problems.some((p) => p.includes("declares O but the artifacts do not support it")));
});

test("frame: the frame step, update and render call sites validate against the map", () => {
  assert.deepEqual(validateFrame(segmentModel), []);
  assert.equal(FRAME_STEP.size, 58);
  assert.equal(FRAME_STEP.calls.length, 5);
  assert.deepEqual(
    FRAME_STEP.calls.map((call) => call.target),
    [0x0043f80d, 0x0043e1ba, 0x0054053e, 0x005404f6, 0x0046300c],
  );
});

test("frame: 25 update call sites reach 24 distinct targets and every ret is site+5", () => {
  assert.equal(UPDATE_CALLS.length, 25);
  assert.equal(new Set(UPDATE_CALLS.map((call) => call.target)).size, 24);
  assert.equal(
    UPDATE_CALLS.filter((call) => call.target === 0x0045e5c5).length,
    2,
    "0x45E5C5 is called from two different sites",
  );
  const update = segmentModel.functions.find((fn) => fn.start === 0x0043e1ba);
  for (const call of UPDATE_CALLS) {
    assert.equal(call.site + 5, call.ret, `${call.site.toString(16)} does not precede ${call.ret.toString(16)}`);
    assert.ok(call.site >= update.start && call.site < update.start + update.size);
  }
  for (const call of RENDER_CALLS) {
    assert.equal(call.site + 5, call.ret);
  }
});

test("frame: the measured rates are plausible and the dead site stays dead", () => {
  for (const call of UPDATE_CALLS) {
    assert.ok(call.frames <= call.frameSteps, `${call.site.toString(16)} has more frames than the session`);
    assert.ok(call.calls >= call.frames, `${call.site.toString(16)} has fewer calls than frames`);
    assert.ok(call.calls - call.frames <= 1, `${call.site.toString(16)} ran more than once per frame`);
    const complete = call.frameSteps - 1;
    if (call.calls === 0) {
      assert.equal(call.target, 0x0047344b, "only the forwarder 0x47344B is expected to be dead");
      continue;
    }
    assert.ok(
      call.frames === complete || call.frames === call.frameSteps,
      `${call.site.toString(16)} ran in ${call.frames} of ${call.frameSteps} frames`,
    );
  }
  const dead = UPDATE_CALLS.find((call) => call.target === 0x0047344b);
  assert.equal(dead.calls, 0);
  assert.equal(dead.frames, 0);
});

test("frame: every semantic claim carries evidence and stays inside its confidence level", () => {
  const byStart = new Map(segmentModel.functions.map((fn) => [fn.start, fn]));
  const addresses = new Set();
  for (const entry of SEMANTICS) {
    assert.ok(!addresses.has(entry.address), `${entry.address.toString(16)} appears twice`);
    addresses.add(entry.address);
    const fn = byStart.get(entry.address);
    assert.ok(fn, `${entry.address.toString(16)} is not a mapped function`);
    assert.equal(entry.size, fn.size, `${entry.address.toString(16)} size mismatch`);
    assert.ok(["CONFIRMED", "HYPOTHESIS"].includes(entry.level));
    assert.ok(entry.evidence.length >= 2, `${entry.address.toString(16)} has too little evidence`);
    for (const line of entry.evidence) {
      assert.match(line, /0x[0-9A-Fa-f]{6}/, `${entry.address.toString(16)} evidence lacks an address: ${line}`);
    }
    if (entry.level === "CONFIRMED") {
      assert.ok(!/probablemente|quizas|podria|tal vez/i.test(entry.claim), "a CONFIRMED claim hedges");
    }
  }
  assert.ok(SEMANTICS.some((entry) => entry.level === "HYPOTHESIS"), "the hypothesis layer disappeared");
  assert.equal(SEMANTICS.filter((entry) => entry.level === "CONFIRMED").length >= 15, true);
});

test("audio: the two DirectSound chains validate against the committed artifacts", () => {
  assert.deepEqual(validateAudio(segmentModel), []);
  assert.equal(INIT_ROUTES.length, 3);
  assert.equal(DEAD_ROUTE.nodes.length, 2);
});

test("audio: 0x00538551 has exactly two callers and both are on a declared route", () => {
  const calls = segmentModel.edges.filter((edge) => edge.kind === "call" && edge.to === 0x00538551);
  assert.deepEqual(calls.map((edge) => edge.from).sort(), [0x00538731, 0x0053b10b].sort());
  for (const caller of calls.map((edge) => edge.from)) {
    assert.ok(
      INIT_ROUTES.some((route) => route.nodes.includes(caller)),
      `${caller.toString(16)} is not on a declared route`,
    );
  }
  const entry = segmentModel.functions.find((fn) => fn.basis === "entry");
  assert.equal(entry.start, INIT_ROUTES[0].nodes[0]);
  for (const route of INIT_ROUTES) {
    if (!route.partial) {
      assert.equal(route.nodes[0], entry.start, `${route.id} does not start at the entry point`);
    }
    assert.equal(route.nodes[route.nodes.length - 1], 0x00538551, `${route.id} does not end at the device init`);
  }
});

test("audio: the dead chain keeps zero callers and never creates a primary buffer", () => {
  const incoming = segmentModel.edges.filter((edge) => edge.kind === "call" && edge.to === 0x00538fd5);
  assert.equal(incoming.length, 0, "0x538FD5 gained a caller: the dead route must be re-read");
  const alt = segmentModel.edges.filter((edge) => edge.kind === "call" && edge.to === 0x00538cb0);
  assert.deepEqual(alt.map((edge) => edge.from), [0x00538fd5]);
  const slots = VTABLE_CALLS.filter((call) => call.owner === 0x00538cb0);
  assert.equal(slots.length, 1);
  assert.equal(slots[0].offset, 0x18);
  assert.ok(!slots.some((call) => call.api === "CreateSoundBuffer" || call.api === "SetFormat"));
});

test("audio: the live and dead chains share only the DirectSoundCreate thunk", () => {
  const callees = (from) =>
    new Set(segmentModel.edges.filter((edge) => edge.kind === "call" && edge.from === from).map((edge) => edge.to));
  const live = callees(0x00538551);
  const dead = callees(0x00538cb0);
  const shared = [...live].filter((target) => dead.has(target));
  assert.deepEqual(shared, [0x005404f0]);
  assert.equal(live.size, 1);
  assert.equal(dead.size, 3);
});

test("audio: no route claims a measurement and every global names a real writer", () => {
  for (const route of [...INIT_ROUTES, DEAD_ROUTE]) {
    assert.match(route.status, /NO observada|NO demostrada/, `${route.id} claims a dynamic result`);
    assert.equal(route.calls, undefined, `${route.id} carries a hit count without a session`);
    assert.equal(route.frames, undefined, `${route.id} carries a frame count without a session`);
  }
  const byStart = new Map(segmentModel.functions.map((fn) => [fn.start, fn]));
  for (const global of DSOUND_GLOBALS) {
    if (global.writer == null) continue;
    assert.ok(byStart.has(global.writer), `${global.address.toString(16)} names an unknown writer`);
  }
  assert.equal(new Set(DSOUND_GLOBALS.map((global) => global.address)).size, DSOUND_GLOBALS.length);
});

test("audio: every vtable site belongs to its owner and the slots are 4-byte aligned", () => {
  const byStart = new Map(segmentModel.functions.map((fn) => [fn.start, fn]));
  const sites = new Set();
  for (const call of VTABLE_CALLS) {
    assert.ok(!sites.has(call.site), `${call.site.toString(16)} appears twice`);
    sites.add(call.site);
    const owner = byStart.get(call.owner);
    assert.ok(owner, `${call.owner.toString(16)} is not a mapped function`);
    assert.ok(call.site >= owner.start && call.site < owner.start + owner.size);
    assert.equal(call.offset % 4, 0);
    assert.ok(call.api.length > 0);
  }
  assert.equal(AUDIO_SEMANTICS.filter((entry) => entry.level === "HYPOTHESIS").length >= 1, true);
  assert.equal(AUDIO_SEMANTICS.filter((entry) => entry.level === "CONFIRMED").length >= 12, true);
});
