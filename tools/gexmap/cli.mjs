/**
 * gexmap — static function map for GEX3D.EXE (EU, Glide).
 *
 *   node tools/gexmap/cli.mjs map --exe <GEX3D.EXE> [--objdump <path>] [--out analysis/EU]
 *   node tools/gexmap/cli.mjs check [--dir analysis/EU]
 *   node tools/gexmap/cli.mjs query --function 0x43e1ba [--dir analysis/EU]
 *
 * `map` reads the original executable (never committed) and writes derived, reproducible
 * artifacts: functions.tsv, calls.tsv, anchors.json and summary.json. `check` validates
 * the committed artifacts without the binary, which is what CI runs.
 */

import { createHash } from "node:crypto";
import { readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { resolve, join, basename } from "node:path";
import { readPE, readImports } from "./pe.mjs";
import { resolveObjdump, runObjdump } from "./disasm.mjs";
import { analyze, transitiveImports } from "./analyze.mjs";

const EXPECTED = {
  size: 1557504,
  md5: "692b12825003417cc4a5a9d4db13eebe",
  sha256: "d0932549613f9c08fb895c5f8a6461ce044e1863b4cba42da4715c2b41e73886",
};

const DEFAULT_DIR = "analysis/EU";
const DEFAULT_REPORT = "report/EU/report.json";

/**
 * Addresses the RE notes already document. The map does not validate the *meaning* of
 * what happens at each one; it records which mapped function contains it, so a doc
 * anchor and the binary can be cross-checked mechanically.
 */
const ANCHORS = [
  { addr: "0x00541b20", label: "PE entry point" },
  { addr: "0x00463278", label: "WinMain (E-3)" },
  { addr: "0x0043b11d", label: "init temporal por nivel (M-13)" },
  { addr: "0x0043b36d", label: "reset + t0 = GetTickCount (M-13)" },
  { addr: "0x0043e1ba", label: "update por frame (M-13)" },
  { addr: "0x0043e5dd", label: "sello elapsed/16.666 -> +0x10C (M-13)" },
  { addr: "0x0043dc3a", label: "envio de render + drenaje de cola (M-13)" },
  { addr: "0x0043ea51", label: "espera grBufferNumPending -> grBufferSwap(3) (M-13)" },
  { addr: "0x0053fcb7", label: "housekeeping de voz (M-13)" },
  { addr: "0x004622d7", label: "grBufferSwap(2) durante init (M-13)" },
  { addr: "0x00424c7f", label: "grBufferSwap(1) (ruta 0x424C0F)" },
  { addr: "0x00537e6e", label: "grBufferSwap(1) wrapper cine" },
  { addr: "0x00418ff8", label: "handler pantalla TV (estatico: 0 llamadores)" },
  { addr: "0x00412f8b", label: "cuenta atras por frame (+0x4BC--)" },
  { addr: "0x0046324d", label: "grSstWinOpen 512x384@75Hz" },
  { addr: "0x00537ea6", label: "grSstWinOpen 640x480@60Hz (cine)" },
  { addr: "0x0053fed4", label: "R1: snapshot del cociente (M-13)" },
  { addr: "0x00451a0c", label: "R2: delta -> tabla -> WORD (M-13)" },
  { addr: "0x00423cfe", label: "snapshot contador +0x104 (M-13)" },
  { addr: "0x004721b6", label: "lector extra del contador +0x104 (M-13)" },
  { addr: "0x0043dcee", label: "segunda lectura del contador (M-13)" },
  { addr: "0x0053ff0d", label: "ruta de voz <InstallDir>\\audio\\voiceuk" },
  { addr: "0x00442f41", label: "lecturas de registro InstallDir/CDDriveName" },
  { addr: "0x00537bb0", label: "wrapper de cine (E-3)" },
];

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

const hex = (value, width = 8) => `0x${value.toString(16).padStart(width, "0")}`;
const sha256 = (value) => createHash("sha256").update(value).digest("hex");

function readExe(exePath) {
  const buffer = readFileSync(resolve(exePath));
  const md5 = createHash("md5").update(buffer).digest("hex");
  const digest = sha256(buffer);
  if (buffer.length !== EXPECTED.size || md5 !== EXPECTED.md5 || digest !== EXPECTED.sha256) {
    fail(
      `${exePath} is not the EU dump this map describes.\n` +
        `  expected size=${EXPECTED.size} md5=${EXPECTED.md5} sha256=${EXPECTED.sha256}\n` +
        `  found    size=${buffer.length} md5=${md5} sha256=${digest}`,
    );
  }
  return { buffer, md5, sha256: digest };
}

function functionAt(functions, addr) {
  let lo = 0;
  let hi = functions.length - 1;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    const fn = functions[mid];
    if (addr < fn.start) hi = mid - 1;
    else if (addr >= fn.end) lo = mid + 1;
    else return fn;
  }
  return null;
}

function reconciliation(reportPath, functions, instructions) {
  if (!existsSync(resolve(reportPath))) return null;
  const report = JSON.parse(readFileSync(resolve(reportPath), "utf8"));
  const boundary = new Set(instructions.map((i) => i.addr));
  const byStart = new Map(functions.map((f) => [f.start, f]));
  const entries = (report.units ?? []).flatMap((unit) => unit.functions ?? []);
  let confirmed = 0;
  let prologue = 0;
  let boundaryOnly = 0;
  let notBoundary = 0;
  for (const fn of entries) {
    const addr = fn.metadata && fn.metadata.virtual_address;
    if (!addr) continue;
    const mapped = byStart.get(addr);
    if (mapped && mapped.basis !== "prologue") confirmed++;
    else if (mapped) prologue++;
    else if (boundary.has(addr)) boundaryOnly++;
    else notBoundary++;
  }
  const listed = new Set(entries.map((fn) => fn.metadata && fn.metadata.virtual_address));
  const missingFromReport = functions.filter((f) => f.basis !== "prologue" && !listed.has(f.start)).length;
  return {
    bytematch_report: reportPath,
    bytematch_functions: entries.length,
    mapped_functions: functions.length,
    report_entry_confirmed_start: confirmed,
    report_entry_prologue_start: prologue,
    report_entry_boundary_but_not_a_start: boundaryOnly,
    report_entry_not_an_instruction_boundary: notBoundary,
    report_entries_that_are_not_confirmed_starts: prologue + boundaryOnly + notBoundary,
    confirmed_starts_missing_from_report: missingFromReport,
  };
}

function writeTsv(path, header, rows) {
  const body = `${header.join("\t")}\n${rows.map((row) => row.join("\t")).join("\n")}\n`;
  writeFileSync(path, body);
  return { path, lines: rows.length, sha256: sha256(body) };
}

function commandMap(args) {
  const exeIndex = args.indexOf("--exe");
  if (exeIndex === -1 || !args[exeIndex + 1]) fail("map needs --exe <path to GEX3D.EXE>");
  const exePath = resolve(args[exeIndex + 1]);
  const outIndex = args.indexOf("--out");
  const outDir = resolve(outIndex !== -1 ? args[outIndex + 1] : DEFAULT_DIR);
  const objdumpIndex = args.indexOf("--objdump");
  const reportIndex = args.indexOf("--report");
  const reportPath = reportIndex !== -1 ? args[reportIndex + 1] : DEFAULT_REPORT;

  const { buffer, md5, sha256: digest } = readExe(exePath);
  const pe = readPE(buffer);
  const imports = readImports(buffer, pe);
  const text = pe.sections.find((s) => s.name === ".text");
  if (!text) fail(".text section not found");

  const { path: objdump, version } = resolveObjdump(objdumpIndex !== -1 ? args[objdumpIndex + 1] : undefined);
  const instructions = runObjdump(objdump, exePath, {
    start: pe.imageBase + text.virtualAddress,
    stop: pe.imageBase + text.virtualAddress + text.virtualSize,
  });
  if (!instructions.length) fail("the disassembler returned no instructions for .text");

  const result = analyze({
    buffer,
    pe,
    text,
    instructions,
    imports,
    disassembleRange: (start, stop) => runObjdump(objdump, exePath, { start, stop }),
  });

  const functions = result.functions;
  const edges = result.edges;

  mkdirSync(outDir, { recursive: true });

  const functionRows = functions.map((fn) => [
    hex(fn.start),
    String(fn.size),
    fn.kind,
    fn.basis,
    fn.symbol,
    String(fn.callers),
    String(fn.callees.length),
    [...fn.imports.keys()].sort().join(";"),
  ]);
  const callRows = edges.map((edge) => [hex(edge.from), hex(edge.to), edge.kind, String(edge.count)]);

  const functionsFile = writeTsv(
    join(outDir, "functions.tsv"),
    ["# addr", "size", "kind", "basis", "symbol", "callers", "callees", "imports"],
    functionRows,
  );
  const callsFile = writeTsv(
    join(outDir, "calls.tsv"),
    ["# from", "to", "kind", "count"],
    callRows,
  );

  const anchors = ANCHORS.map((anchor) => {
    const addr = parseInt(anchor.addr, 16);
    const fn = functionAt(functions, addr);
    return {
      addr: hex(addr),
      label: anchor.label,
      function_start: fn ? hex(fn.start) : null,
      function_symbol: fn ? fn.symbol : null,
      offset_in_function: fn ? addr - fn.start : null,
      at_function_start: fn ? addr === fn.start : false,
    };
  });
  const unreachableAnchors = anchors.filter((a) => a.function_start === null);
  const anchorsBody = `${JSON.stringify(
    { schema: 1, source: "documented anchors in REVERSE_ENGINEERING.md / TESTING.md", anchors },
    null,
    2,
  )}\n`;
  writeFileSync(join(outDir, "anchors.json"), anchorsBody);

  const summary = {
    schema: 1,
    tool: "tools/gexmap",
    binary: {
      name: "GEX3D.EXE",
      size: buffer.length,
      md5,
      sha256: digest,
      image_base: hex(pe.imageBase),
      entry: hex(pe.entryPoint),
    },
    text: {
      start: hex(result.textStart),
      end: hex(result.textEnd),
      bytes: result.textEnd - result.textStart,
      virtual_size: text.virtualSize,
      raw_size: text.rawSize,
    },
    disassembler: { command: basename(objdump), version },
    counts: result.stats,
    repairs: result.repairs.map((r) => ({ start: hex(r.start), stop: hex(r.stop), instructions: r.instructions })),
    unresolvable: result.unresolvable.map((a) => hex(a)),
    reconciliation: reconciliation(reportPath, functions, result.instructions),
    anchors: {
      file: "anchors.json",
      total: anchors.length,
      at_function_start: anchors.filter((a) => a.at_function_start).length,
      inside_function: anchors.filter((a) => !a.at_function_start && a.function_start).length,
      outside_every_function: unreachableAnchors.length,
      outside: unreachableAnchors.map((a) => a.addr),
      sha256: sha256(anchorsBody),
    },
    artifacts: {
      functions: { file: "functions.tsv", lines: functionsFile.lines, sha256: functionsFile.sha256 },
      calls: { file: "calls.tsv", lines: callsFile.lines, sha256: callsFile.sha256 },
    },
    limits: [
      "Starts are CONFIRMED only when a direct call instruction or the PE entry point reaches them; the first byte of .text is a structural start and `kind=thunk` rows are linker import stubs. Prologue-only candidates are HYPOTHESIS and are listed apart, never merged.",
      "Sizes are the distance to the next start: an upper bound that includes padding and any embedded data between functions.",
      "The disassembly is static and linear; regions where embedded tables desynchronised it are re-decoded from the affected call target in a 4 KiB window and listed in `repairs`.",
      "No dynamic evidence: the map does not prove that a function runs, nor what it means.",
      "The 132 import stubs are linker glue, not game code; they are excluded from `gameFunctions` but kept in the map so their imports are not attributed to the preceding function.",
    ],
  };
  writeFileSync(join(outDir, "summary.json"), `${JSON.stringify(summary, null, 2)}\n`);

  console.log(`read    ${exePath}`);
  console.log(`        md5 ${md5}`);
  console.log(`        sha256 ${digest}`);
  console.log(`        disassembler ${objdump} (${version})`);
  console.log(`        .text ${hex(result.textStart)}..${hex(result.textEnd)} (${result.textEnd - result.textStart} B)`);
  console.log(`        instructions ${result.stats.instructions}, repaired regions ${result.stats.repairs}`);
  console.log(`wrote   ${join(outDir, "functions.tsv")}  (${result.stats.functions} starts, ${result.stats.thunks} thunks)`);
  console.log(`wrote   ${join(outDir, "calls.tsv")}      (${edges.length} edges)`);
  console.log(`wrote   ${join(outDir, "anchors.json")}   (${anchors.length} anchors, ${unreachableAnchors.length} outside)`);
  console.log(`wrote   ${join(outDir, "summary.json")}`);
  console.log(`        coverage ${result.stats.functionBytes} B of .text = ${result.stats.coveragePercent.toFixed(3)} %`);
  console.log(`        starts by basis: call ${result.stats.startFromCalls}, entry ${result.stats.startFromEntry}, text-start ${result.stats.startFromTextStart}, prologue ${result.stats.startFromPrologue}, thunk ${result.stats.startFromThunks}`);
  if (result.stats.unresolvedCalls) console.log(`        WARNING unresolved direct calls: ${result.stats.unresolvedCalls}`);
  if (summary.reconciliation) {
    const r = summary.reconciliation;
    console.log(
      `        reconciliation: report lists ${r.bytematch_functions}; ${r.report_entry_confirmed_start} confirmed starts, ` +
        `${r.report_entry_prologue_start} prologue starts, ${r.report_entry_boundary_but_not_a_start} boundaries that are not starts, ` +
        `${r.report_entry_not_an_instruction_boundary} not even instruction boundaries`,
    );
  }
}

function parseTsv(path, expectedColumns) {
  if (!existsSync(path)) fail(`missing ${path}; run "gexmap map --exe <GEX3D.EXE>" first`);
  const lines = readFileSync(path, "utf8").split(/\r?\n/).filter((l) => l.length);
  if (!lines.length || !lines[0].startsWith("#")) fail(`${path}: no header line`);
  const columns = lines[0].replace(/^#\s*/, "").split("\t");
  if (columns.length !== expectedColumns) fail(`${path}: expected ${expectedColumns} columns, found ${columns.length}`);
  return lines.slice(1).map((line, index) => {
    const cells = line.split("\t");
    if (cells.length !== expectedColumns) fail(`${path}:${index + 2}: expected ${expectedColumns} cells, found ${cells.length}`);
    return cells;
  });
}

function commandCheck(args) {
  const dirIndex = args.indexOf("--dir");
  const dir = resolve(dirIndex !== -1 ? args[dirIndex + 1] : DEFAULT_DIR);
  const summaryPath = join(dir, "summary.json");
  if (!existsSync(summaryPath)) fail(`missing ${summaryPath}`);
  const summary = JSON.parse(readFileSync(summaryPath, "utf8"));
  const problems = [];

  if (summary.schema !== 1) problems.push(`summary schema is ${summary.schema}, expected 1`);

  const functionsPath = join(dir, "functions.tsv");
  const callsPath = join(dir, "calls.tsv");
  const anchorsPath = join(dir, "anchors.json");

  const functionSha = sha256(readFileSync(functionsPath));
  const callSha = sha256(readFileSync(callsPath));
  const anchorsSha = sha256(readFileSync(anchorsPath));
  if (summary.artifacts?.functions?.sha256 !== functionSha) problems.push("functions.tsv does not match the sha256 pinned in summary.json");
  if (summary.artifacts?.calls?.sha256 !== callSha) problems.push("calls.tsv does not match the sha256 pinned in summary.json");
  if (summary.anchors?.sha256 !== anchorsSha) problems.push("anchors.json does not match the sha256 pinned in summary.json");

  const functionRows = parseTsv(functionsPath, 8);
  const callRows = parseTsv(callsPath, 4);

  if (functionRows.length !== summary.counts?.functions) {
    problems.push(`functions.tsv has ${functionRows.length} rows but summary says ${summary.counts?.functions}`);
  }
  if (callRows.length !== summary.artifacts?.calls?.lines) {
    problems.push(`calls.tsv has ${callRows.length} rows but summary says ${summary.artifacts?.calls?.lines}`);
  }

  const starts = new Map();
  let previousEnd = -1;
  let bytes = 0;
  for (const [index, row] of functionRows.entries()) {
    const start = Number.parseInt(row[0], 16);
    const size = Number.parseInt(row[1], 10);
    if (!Number.isFinite(start) || !Number.isFinite(size) || size <= 0) {
      problems.push(`functions.tsv:${index + 2}: bad address/size (${row[0]}, ${row[1]})`);
      continue;
    }
    if (start < previousEnd) problems.push(`functions.tsv:${index + 2}: function ${row[0]} overlaps the previous one`);
    if (row[2] !== "func" && row[2] !== "thunk") problems.push(`functions.tsv:${index + 2}: unknown kind ${row[2]}`);
    if (row[2] === "thunk") {
      if (!row[7]) problems.push(`functions.tsv:${index + 2}: thunk without a direct import`);
      if (row[7].includes(";")) problems.push(`functions.tsv:${index + 2}: thunk forwards to more than one import`);
      if (row[7] && row[4] !== row[7]) problems.push(`functions.tsv:${index + 2}: thunk symbol ${row[4]} does not match its import ${row[7]}`);
      if (size < 6) problems.push(`functions.tsv:${index + 2}: thunk is ${size} B, smaller than the 6 B stub`);
    }
    if (!["call", "entry", "text-start", "prologue", "thunk"].includes(row[3])) problems.push(`functions.tsv:${index + 2}: unknown start basis ${row[3]}`);
    if (row[3] === "thunk" && row[2] !== "thunk") problems.push(`functions.tsv:${index + 2}: basis thunk on a ${row[2]} row`);
    starts.set(start, { size, kind: row[2] });
    previousEnd = start + size;
    bytes += size;
  }
  const textEnd = Number.parseInt(summary.text.end, 16);
  if (previousEnd > textEnd) problems.push(`last function ends at 0x${previousEnd.toString(16)}, past .text end`);

  const coverage = summary.counts?.functionBytes;
  if (bytes !== coverage) problems.push(`functions.tsv sizes add up to ${bytes} but summary says ${coverage}`);
  if (summary.counts?.callEdges + summary.counts?.tailEdges !== callRows.length) {
    problems.push(`summary edge counts do not add up to ${callRows.length} rows in calls.tsv`);
  }

  let calls = 0;
  let tails = 0;
  for (const [index, row] of callRows.entries()) {
    const from = Number.parseInt(row[0], 16);
    const to = Number.parseInt(row[1], 16);
    if (!starts.has(from)) problems.push(`calls.tsv:${index + 2}: caller ${row[0]} is not a mapped function`);
    if (!starts.has(to)) problems.push(`calls.tsv:${index + 2}: callee ${row[1]} is not a mapped function`);
    if (row[2] === "call") calls++;
    else if (row[2] === "tail") tails++;
    else problems.push(`calls.tsv:${index + 2}: unknown edge kind ${row[2]}`);
  }
  if (calls !== summary.counts?.callEdges) problems.push(`calls.tsv has ${calls} call edges, summary says ${summary.counts?.callEdges}`);
  if (tails !== summary.counts?.tailEdges) problems.push(`calls.tsv has ${tails} tail edges, summary says ${summary.counts?.tailEdges}`);

  const anchors = JSON.parse(readFileSync(anchorsPath, "utf8")).anchors ?? [];
  if (anchors.length !== summary.anchors?.total) problems.push("anchors.json does not match the anchor count in summary.json");
  for (const [index, anchor] of anchors.entries()) {
    const addr = Number.parseInt(anchor.addr, 16);
    if (anchor.function_start === null) {
      if (!(summary.anchors?.outside ?? []).includes(anchor.addr)) problems.push(`anchors.json:${index}: ${anchor.addr} is outside every function but not declared in summary.anchors.outside`);
      continue;
    }
    const start = Number.parseInt(anchor.function_start, 16);
    const fn = starts.get(start);
    if (!fn) problems.push(`anchors.json:${index}: ${anchor.addr} points at ${anchor.function_start}, which is not a mapped function`);
    else if (addr < start || addr >= start + fn.size) problems.push(`anchors.json:${index}: ${anchor.addr} is outside ${anchor.function_start}+${fn.size}`);
  }

  if (problems.length) {
    console.error(`${dir}: ${problems.length} problem(s)`);
    for (const problem of problems) console.error(`  - ${problem}`);
    process.exit(1);
  }
  console.log(
    `${dir}: ok (${starts.size} starts, ${callRows.length} edges, ${bytes} of ${summary.text.bytes} .text bytes = ` +
      `${summary.counts.coveragePercent.toFixed(3)} %, ${anchors.length} anchors)`,
  );
}

function commandQuery(args) {
  const index = args.indexOf("--function");
  if (index === -1 || !args[index + 1]) fail("query needs --function <address>");
  const addr = Number.parseInt(args[index + 1].replace(/^0x/, ""), 16);
  const dirIndex = args.indexOf("--dir");
  const dir = resolve(dirIndex !== -1 ? args[dirIndex + 1] : DEFAULT_DIR);

  const summary = JSON.parse(readFileSync(join(dir, "summary.json"), "utf8"));
  const rows = parseTsv(join(dir, "functions.tsv"), 8).map((row) => ({
    start: Number.parseInt(row[0], 16),
    size: Number.parseInt(row[1], 10),
    kind: row[2],
    basis: row[3],
    symbol: row[4],
    callers: Number.parseInt(row[5], 10),
    imports: row[7] ? row[7].split(";") : [],
  }));
  const calls = parseTsv(join(dir, "calls.tsv"), 4);
  const functions = rows.map((row) => ({ ...row, start: row.start, imports: new Map(row.imports.map((i) => [i, 1])) }));
  const fn = functions.find((f) => addr >= f.start && addr < f.start + f.size);
  if (!fn) fail(`0x${addr.toString(16)} is not inside any mapped function`);

  const edges = calls.map((row) => ({
    from: Number.parseInt(row[0], 16),
    to: Number.parseInt(row[1], 16),
    kind: row[2],
    count: Number.parseInt(row[3], 10),
  }));
  const byStart = new Map(functions.map((f) => [f.start, f]));
  const transitive = transitiveImports(functions, edges, fn.start);

  console.log(`${fn.symbol}  ${hex(fn.start)} + 0x${(addr - fn.start).toString(16)}  (${fn.size} B, ${fn.kind})`);
  console.log(`callers ${fn.callers}, direct imports ${fn.imports.size}`);
  console.log("callees:");
  for (const edge of edges.filter((e) => e.from === fn.start).sort((a, b) => a.to - b.to)) {
    const target = byStart.get(edge.to);
    console.log(`  ${hex(edge.to)}  ${target ? target.symbol : "?"}  ${edge.kind} x${edge.count}`);
  }
  console.log("direct imports:");
  for (const name of [...fn.imports.keys()].sort()) console.log(`  ${name}`);
  console.log("transitive imports (depth):");
  for (const [name, depth] of [...transitive.entries()].sort((a, b) => a[1] - b[1] || a[0].localeCompare(b[0]))) {
    console.log(`  d${depth}  ${name}`);
  }
  console.log(`text bytes ${summary.text.bytes}`);
}

const args = process.argv.slice(2);
const command = args[0];
if (command === "map") commandMap(args);
else if (command === "check") commandCheck(args);
else if (command === "query") commandQuery(args);
else {
  console.log(`usage:
  node tools/gexmap/cli.mjs map --exe <GEX3D.EXE> [--objdump <path>] [--out ${DEFAULT_DIR}]
  node tools/gexmap/cli.mjs check [--dir ${DEFAULT_DIR}]
  node tools/gexmap/cli.mjs query --function 0x43e1ba [--dir ${DEFAULT_DIR}]

map    reads the original EU executable (verified by size/MD5/SHA-256) and writes the
       derived map into ${DEFAULT_DIR}/. The binary is never copied into the repository.
check  validates the committed artifacts without the binary (CI).
query  prints one function's callees and imports from the committed artifacts.`);
  process.exit(args.length ? 2 : 0);
}

