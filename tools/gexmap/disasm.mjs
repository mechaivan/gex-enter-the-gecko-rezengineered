/**
 * Disassembly front-end.
 *
 * The decoder of record is `llvm-objdump` (or GNU `objdump`): this project already
 * documents its analyses against objdump output, so the map is built from the same
 * tool the rest of the RE notes use. This module only runs it and parses the result;
 * it never guesses an instruction.
 *
 * One known limitation is handled by the caller (analyze.mjs): a linear sweep decodes
 * embedded data (jump tables) as instructions and desynchronises until it happens to
 * realign. The repaired regions are listed in the generated summary.
 */

import { spawnSync } from "node:child_process";

export function resolveObjdump(explicit) {
  const candidates = [explicit, process.env.GEX_OBJDUMP, "llvm-objdump", "objdump"].filter(Boolean);
  const tried = [];
  for (const candidate of candidates) {
    const probe = spawnSync(candidate, ["--version"], { encoding: "utf8" });
    if (probe.error || probe.status !== 0) {
      tried.push(`${candidate}: ${probe.error ? probe.error.code ?? probe.error.message : `exit ${probe.status}`}`);
      continue;
    }
    const first = (probe.stdout || "").split(/\r?\n/).find((line) => line.trim());
    return { path: candidate, version: first ? first.trim() : "unknown" };
  }
  throw new Error(
    "no disassembler found; pass --objdump <path> or set GEX_OBJDUMP\n  tried:\n    " + tried.join("\n    "),
  );
}

/**
 * Parse objdump's `-d --x86-asm-syntax=intel` output.
 *
 * Accepted line shape (LLVM and GNU both fit):
 *     5403c4: ff 25 34 f3 65 00            \tjmp\tdword ptr [0x65f334]
 *
 * The raw-byte column is decoded so every instruction keeps its real length. That
 * length matters: it is what proves that only padding sits between the end of a
 * function and the next prologue or import stub. Any other line (headers, section
 * labels, blanks) is ignored.
 */
export function parseObjdump(text) {
  const instructions = [];
  for (const line of text.split(/\r?\n/)) {
    const m = line.match(/^\s*([0-9a-f]{4,16}):\s+((?:[0-9a-f]{2}\s+)+)(\S.*)$/);
    if (!m) continue;
    const addr = parseInt(m[1], 16);
    const bytes = m[2].trim().split(/\s+/).length;
    const body = m[3].trim().replace(/\t+/g, " ").replace(/ {2,}/g, " ");
    instructions.push({ addr, text: body, bytes });
  }
  return instructions;
}

export function runObjdump(objdump, exePath, { start, stop } = {}) {
  const args = ["-d", "--x86-asm-syntax=intel"];
  if (start !== undefined) args.push(`--start-address=0x${start.toString(16)}`);
  if (stop !== undefined) args.push(`--stop-address=0x${stop.toString(16)}`);
  args.push(exePath);
  const run = spawnSync(objdump, args, { encoding: "utf8", maxBuffer: 512 * 1024 * 1024 });
  if (run.error) throw new Error(`cannot run ${objdump}: ${run.error.message}`);
  const parsed = parseObjdump(run.stdout || "");
  if (run.status !== 0 && parsed.length === 0) {
    throw new Error(`${objdump} failed (exit ${run.status}): ${(run.stderr || "").trim().slice(0, 500)}`);
  }
  return parsed;
}
