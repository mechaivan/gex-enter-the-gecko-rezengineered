/**
 * Static map of the game code: function entry points, extents, direct call graph and
 * import usage.
 *
 * Evidence policy (same as the rest of the project):
 *
 *   CONFIRMED  a function start is an address the original code *calls* with a direct
 *              `call rel32`, or the PE entry point. The first byte of `.text` is also
 *              a structural start. Nothing else is promoted to a confirmed start.
 *   HYPOTHESIS a `push ebp; mov ebp, esp` prologue outside every confirmed function is
 *              reported as a *candidate* start, never merged into the confirmed map.
 *   STRUCTURAL an import stub (`jmp dword ptr [IAT]`) is linker glue: it is split from
 *              the preceding function and labelled `thunk`, not promoted as game code.
 *
 * The map is built from objdump's instruction stream, not from raw `E8` bytes: a byte
 * scan finds `E8` inside immediates and inside jump tables, which invents functions.
 * That difference is measured and reported (see `reconciliation` in the summary).
 */

const TERMINATORS = /^(ret|retn|iret|iretd|jmp|ljmp|hlt|int3)\b/;

const callTarget = (text) => {
  const m = text.match(/^call\s+0x([0-9a-f]+)(?:\s|$)/);
  return m ? parseInt(m[1], 16) : null;
};

const jumpTarget = (text) => {
  const m = text.match(/^jmp\s+0x([0-9a-f]+)(?:\s|$)/);
  return m ? parseInt(m[1], 16) : null;
};

const memoryOperand = (text, mnemonic) => {
  const m = text.match(new RegExp(`^${mnemonic}\\s+dword ptr \\[0x([0-9a-f]+)\\]$`));
  return m ? parseInt(m[1], 16) : null;
};

export function analyze({ buffer, pe, text, instructions, imports, disassembleRange, maxRepairs = 512 }) {
  const textStart = pe.imageBase + text.virtualAddress;
  const textEnd = textStart + text.virtualSize;
  const inText = (addr) => addr >= textStart && addr < textEnd;

  const insMap = new Map(instructions.map((i) => [i.addr, { text: i.text, bytes: i.bytes }]));
  const repairs = [];
  const unresolvable = [];

  const collectCalls = (map) => {
    const targets = new Set();
    for (const [addr, instruction] of map) {
      if (!inText(addr)) continue;
      const target = callTarget(instruction.text);
      if (target !== null && inText(target)) targets.add(target);
    }
    return targets;
  };

  let calls = collectCalls(insMap);
  for (let guard = 0; guard < maxRepairs; guard++) {
    const missing = [...calls].filter((t) => !insMap.has(t)).sort((a, b) => a - b);
    if (!missing.length) break;
    const start = missing[0];
    const stop = Math.min(start + 0x1000, textEnd);
    const ranged = disassembleRange(start, stop) ?? [];
    if (!ranged.length) {
      unresolvable.push(start);
      calls.delete(start);
      continue;
    }
    for (const addr of [...insMap.keys()]) if (addr >= start && addr < stop) insMap.delete(addr);
    for (const instruction of ranged) insMap.set(instruction.addr, { text: instruction.text, bytes: instruction.bytes });
    repairs.push({ start, stop, instructions: ranged.length });
    calls = collectCalls(insMap);
  }

  const ordered = [...insMap.entries()]
    .sort((a, b) => a[0] - b[0])
    .map(([addr, instruction]) => ({ addr, text: instruction.text, bytes: instruction.bytes }));
  for (let i = 0; i < ordered.length; i++) {
    if (!Number.isFinite(ordered[i].bytes) || ordered[i].bytes <= 0) {
      ordered[i].bytes = (i + 1 < ordered.length ? ordered[i + 1].addr : textEnd) - ordered[i].addr;
    }
  }
  const byAddr = new Map(ordered.map((i) => [i.addr, i]));
  const lengthAt = (index) => ordered[index].bytes;

  // CONFIRMED starts: direct call targets, the PE entry point and the first byte of .text.
  const starts = new Map();
  for (const target of calls) if (byAddr.has(target)) starts.set(target, "call");
  if (inText(pe.entryPoint) && byAddr.has(pe.entryPoint) && !starts.has(pe.entryPoint)) starts.set(pe.entryPoint, "entry");
  if (byAddr.has(textStart) && !starts.has(textStart)) starts.set(textStart, "text-start");

  // HYPOTHESIS tier: a prologue at an instruction boundary immediately after a terminator
  // that is not already a confirmed start. This is how functions reached only through a
  // table or an indirect call are found; they are labelled `prologue` in the artifact.
  //
  // Only padding (00 / CC / 90) may sit between the terminator and the candidate: an
  // embedded table before the candidate means the bytes are not a clean function boundary.
  const cleanBoundary = (index) => {
    const previous = ordered[index - 1];
    if (!previous || !TERMINATORS.test(previous.text)) return false;
    const end = previous.addr + lengthAt(index - 1);
    const candidate = ordered[index].addr;
    if (end > candidate) return false;
    for (let address = end; address < candidate; address++) {
      const offset = pe.offsetOf(address - pe.imageBase);
      if (offset === null) return false;
      const byte = buffer[offset];
      if (byte !== 0x00 && byte !== 0xcc && byte !== 0x90) return false;
    }
    return true;
  };

  const prologueCandidates = [];
  for (let i = 1; i < ordered.length; i++) {
    const instruction = ordered[i];
    if (starts.has(instruction.addr)) continue;
    const raw = pe.offsetOf(instruction.addr - pe.imageBase);
    if (raw === null || raw + 2 >= buffer.length) continue;
    if (buffer[raw] !== 0x55 || buffer[raw + 1] !== 0x8b || buffer[raw + 2] !== 0xec) continue;
    if (!cleanBoundary(i)) continue;
    prologueCandidates.push(instruction.addr);
  }
  for (const candidate of prologueCandidates) starts.set(candidate, "prologue");

  // Linker import stubs live inside .text too, in blocks of 6-byte `jmp dword ptr [IAT]`.
  // Without this pass the whole block is absorbed by the preceding function and its imports
  // are attributed to code that never calls them.
  const thunkStubs = [];
  for (let i = 1; i < ordered.length; i++) {
    const instruction = ordered[i];
    if (starts.has(instruction.addr)) continue;
    const slot = memoryOperand(instruction.text, "jmp");
    if (slot === null || !imports.has(slot)) continue;
    if (!cleanBoundary(i)) continue;
    thunkStubs.push(instruction.addr);
  }
  for (const stub of thunkStubs) starts.set(stub, "thunk");

  const startList = [...starts.keys()].sort((a, b) => a - b);
  const startSet = new Set(startList);

  // Thunks: a linker stub that forwards to an import.
  const thunkOf = new Map();
  for (const start of startList) {
    const body = byAddr.get(start).text;
    const slot = memoryOperand(body, "jmp");
    if (slot !== null && imports.has(slot)) thunkOf.set(start, imports.get(slot));
  }

  // Extents: distance to the next start (an upper bound on the function size).
  const functions = startList.map((start, index) => {
    const end = index + 1 < startList.length ? startList[index + 1] : textEnd;
    const thunk = thunkOf.get(start);
    return {
      start,
      end,
      size: end - start,
      basis: starts.get(start),
      kind: thunk ? "thunk" : "func",
      symbol: thunk ? `${thunk.dll}!${thunk.func}` : `FUN_${start.toString(16).toUpperCase().padStart(8, "0")}`,
      thunk: thunk ? `${thunk.dll}!${thunk.func}` : null,
      calls: 0,
      callers: 0,
      tailCalls: 0,
      imports: new Map(),
      callees: [],
    };
  });
  const byStart = new Map(functions.map((f) => [f.start, f]));

  // Direct graph + import usage.
  const edges = new Map();
  const unresolvedCalls = [];
  let index = 0;
  for (const fn of functions) {
    while (index < ordered.length && ordered[index].addr < fn.start) index++;
    for (let i = index; i < ordered.length && ordered[i].addr < fn.end; i++) {
      const { addr, text: body } = ordered[i];
      const direct = callTarget(body);
      if (direct !== null) {
        if (inText(direct) && startSet.has(direct)) {
          const key = `${fn.start}|${direct}|call`;
          const edge = edges.get(key) ?? { from: fn.start, to: direct, kind: "call", count: 0 };
          edge.count++;
          edges.set(key, edge);
          fn.calls++;
        } else if (inText(direct)) {
          unresolvedCalls.push({ from: addr, target: direct });
        }
        continue;
      }
      const importSlot = memoryOperand(body, "call");
      if (importSlot !== null && imports.has(importSlot)) {
        const target = imports.get(importSlot);
        const name = `${target.dll}!${target.func}`;
        fn.imports.set(name, (fn.imports.get(name) ?? 0) + 1);
        continue;
      }
      const tail = jumpTarget(body);
      if (tail !== null) {
        if (inText(tail) && startSet.has(tail) && tail !== fn.start) {
          const key = `${fn.start}|${tail}|tail`;
          const edge = edges.get(key) ?? { from: fn.start, to: tail, kind: "tail", count: 0 };
          edge.count++;
          edges.set(key, edge);
          fn.tailCalls++;
        }
        continue;
      }
      const importJump = memoryOperand(body, "jmp");
      if (importJump !== null && imports.has(importJump)) {
        const target = imports.get(importJump);
        const name = `${target.dll}!${target.func}`;
        fn.imports.set(name, (fn.imports.get(name) ?? 0) + 1);
      }
    }
  }

  for (const edge of edges.values()) byStart.get(edge.to).callers++;
  const adjacency = new Map();
  for (const edge of edges.values()) {
    if (!adjacency.has(edge.from)) adjacency.set(edge.from, []);
    adjacency.get(edge.from).push(edge.to);
  }
  for (const fn of functions) fn.callees = (adjacency.get(fn.start) ?? []).sort((a, b) => a - b);

  const functionBytes = functions.reduce((sum, f) => sum + f.size, 0);
  const thunks = functions.filter((f) => f.kind === "thunk").length;
  const unreferenced = functions.filter((f) => f.callers === 0 && f.start !== pe.entryPoint && f.kind === "func");
  const byBasis = (basis) => functions.filter((f) => f.basis === basis).length;

  return {
    textStart,
    textEnd,
    instructions: ordered,
    functions,
    edges: [...edges.values()].sort((a, b) => a.from - b.from || a.to - b.to || a.kind.localeCompare(b.kind)),
    repairs,
    unresolvable,
    unresolvedCalls,
    prologueCandidates: prologueCandidates.sort((a, b) => a - b),
    stats: {
      functions: functions.length,
      gameFunctions: functions.length - thunks,
      thunks,
      startFromCalls: byBasis("call"),
      startFromEntry: byBasis("entry"),
      startFromTextStart: byBasis("text-start"),
      startFromPrologue: byBasis("prologue"),
      startFromThunks: byBasis("thunk"),
      callEdges: [...edges.values()].filter((e) => e.kind === "call").length,
      tailEdges: [...edges.values()].filter((e) => e.kind === "tail").length,
      unresolvedCalls: unresolvedCalls.length,
      functionBytes,
      textBytes: textEnd - textStart,
      coveragePercent: (functionBytes / (textEnd - textStart)) * 100,
      unreferenced: unreferenced.length,
      prologueCandidates: prologueCandidates.length,
      repairs: repairs.length,
      instructions: ordered.length,
    },
    unreferenced,
  };
}

/**
 * Imports reachable from a function through the direct call graph (transitive), with the
 * depth at which each one first appears. Used for the subsystem table: it says what a
 * function *can* touch, not what it does.
 */
export function transitiveImports(functions, edges, start) {
  const byStart = new Map(functions.map((f) => [f.start, f]));
  const out = new Map();
  for (const edge of edges) {
    if (!out.has(edge.from)) out.set(edge.from, []);
    out.get(edge.from).push(edge.to);
  }
  const result = new Map();
  const seen = new Set([start]);
  let frontier = [start];
  let depth = 0;
  while (frontier.length && depth < 32) {
    const next = [];
    for (const addr of frontier) {
      const fn = byStart.get(addr);
      if (!fn) continue;
      for (const name of fn.imports.keys()) if (!result.has(name)) result.set(name, depth);
      for (const to of out.get(addr) ?? []) if (!seen.has(to)) { seen.add(to); next.push(to); }
    }
    frontier = next;
    depth++;
  }
  return result;
}
