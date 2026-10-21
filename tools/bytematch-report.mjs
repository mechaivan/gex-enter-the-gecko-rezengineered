#!/usr/bin/env node
/**
 * ByteMatch baseline report for Gex: Enter the Gecko (EU, GEX3D.EXE).
 *
 * WHY THIS EXISTS
 * ---------------
 * ByteMatch (https://bytematch.dev) reads objdiff progress reports. objdiff can only
 * produce a report from:
 *   - one target object per unit (the original code split into .obj files), and
 *   - one base object per unit (our C, compiled with the original toolchain, MSVC 5.0/6.0).
 *
 * This repository has neither yet: no splitter output, no MSVC 5.0/6.0 toolchain, no
 * reimplemented translation unit. So there is nothing for objdiff to diff, and any
 * "matched code" figure would be invented.
 *
 * This script writes the honest starting point instead. It reads the original executable
 * and records what the project has to account for:
 *
 *   total_code  = virtual size of .text          (the code to reimplement)
 *   total_data  = virtual size of .rdata/.data/.idata
 *   matched_*   = 0                              (nothing reimplemented yet)
 *
 * Every number comes from the binary itself. The function list is *measured*, not guessed:
 * the executable is stripped (no COFF symbol table, no PDB), so the only code entry points
 * that can be located without guessing are the direct call targets (E8 rel32) inside .text.
 * Those are real entry points the original code calls, so the list is a lower bound on the
 * number of functions - never an overcount of code.
 *
 * The output uses objdiff's report schema version 2, which ByteMatch parses directly
 * (src/report/parse.ts: fromObject + migrate).
 *
 * USAGE
 * -----
 *   node tools/bytematch-report.mjs --exe <path to GEX3D.EXE>
 *   node tools/bytematch-report.mjs --check [path to report.json]
 *
 * --exe    reads the original binary, verifies its SHA-256, and rewrites the report.
 * --check  validates the committed report (schema, percentages, unit sums) without the
 *          binary. This is what CI runs.
 */

import { createHash } from "node:crypto";
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";

/** The EU dump this report describes. Any other build must not reuse these numbers. */
const EXPECTED = {
  file: "GEX3D.EXE",
  size: 1557504,
  sha256: "d0932549613f9c08fb895c5f8a6461ce044e1863b4cba42da4715c2b41e73886",
  md5: "692b12825003417cc4a5a9d4db13eebe",
};

/**
 * Which sections count as code and which as data.
 * .rsrc is left out: resources are not code we reimplement in C.
 */
const CODE_SECTIONS = [".text"];
const DATA_SECTIONS = [".rdata", ".data", ".idata"];

const DEFAULT_OUT = "report/EU/report.json";

const MEASURE_KEYS = [
  "fuzzy_match_percent",
  "total_code",
  "matched_code",
  "matched_code_percent",
  "total_data",
  "matched_data",
  "matched_data_percent",
  "total_functions",
  "matched_functions",
  "matched_functions_percent",
  "complete_code",
  "complete_code_percent",
  "complete_data",
  "complete_data_percent",
  "total_units",
  "complete_units",
];

/**
 * objdiff counts a zero total as 100% ("nothing to do means done"), and ByteMatch follows it
 * (src/report/measures.ts). That rule is wrong for a category we have not measured yet: with
 * nothing matched and no total recorded, the figure must read 0%, not 100%.
 */
const percent = (part, total) => (part === 0 ? 0 : total === 0 ? 100 : (part / total) * 100);

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

/** Minimal PE32 reader: headers we need, nothing else. */
function readPE(buffer) {
  if (buffer.length < 0x40 || buffer.toString("ascii", 0, 2) !== "MZ") fail("not a DOS/PE file (no MZ header)");
  const peOffset = buffer.readUInt32LE(0x3c);
  if (buffer.toString("ascii", peOffset, peOffset + 4) !== "PE\0\0") fail("not a PE file (no PE signature)");
  const machine = buffer.readUInt16LE(peOffset + 4);
  const sectionCount = buffer.readUInt16LE(peOffset + 6);
  const optionalSize = buffer.readUInt16LE(peOffset + 20);
  const opt = peOffset + 24;
  const magic = buffer.readUInt16LE(opt);
  if (machine !== 0x014c) fail(`expected machine 0x014c (i386), found 0x${machine.toString(16)}`);
  if (magic !== 0x010b) fail(`expected PE32 optional header (0x10b), found 0x${magic.toString(16)}`);
  const entryPointRva = buffer.readUInt32LE(opt + 16);
  const imageBase = buffer.readUInt32LE(opt + 28);
  const sections = [];
  const table = opt + optionalSize;
  for (let i = 0; i < sectionCount; i++) {
    const at = table + i * 40;
    sections.push({
      name: buffer.toString("ascii", at, at + 8).replace(/\0+$/, ""),
      virtualSize: buffer.readUInt32LE(at + 8),
      virtualAddress: buffer.readUInt32LE(at + 12),
      rawSize: buffer.readUInt32LE(at + 16),
      rawOffset: buffer.readUInt32LE(at + 20),
      characteristics: buffer.readUInt32LE(at + 36),
    });
  }
  return { machine, entryPointRva, imageBase, sections };
}

function measures({
  totalCode = 0,
  totalData = 0,
  totalUnits = 0,
  matchedCode = 0,
  matchedData = 0,
  totalFunctions = 0,
  matchedFunctions = 0,
  completeCode = 0,
  completeData = 0,
  completeUnits = 0,
}) {
  const m = {
    fuzzy_match_percent: 0,
    total_code: totalCode,
    matched_code: matchedCode,
    matched_code_percent: percent(matchedCode, totalCode),
    total_data: totalData,
    matched_data: matchedData,
    matched_data_percent: percent(matchedData, totalData),
    total_functions: totalFunctions,
    matched_functions: matchedFunctions,
    matched_functions_percent: percent(matchedFunctions, totalFunctions),
    complete_code: completeCode,
    complete_code_percent: percent(completeCode, totalCode),
    complete_data: completeData,
    complete_data_percent: percent(completeData, totalData),
    total_units: totalUnits,
    complete_units: completeUnits,
  };
  // No code is matched, so the code-weighted fuzzy average is 0 even though it is a percent.
  m.fuzzy_match_percent = 0;
  return m;
}

/**
 * The direct call targets (E8 rel32) inside .text: the code entry points the original
 * binary calls. Each one is a real entry point, so the set is a lower bound on the number
 * of functions - it can miss functions that are only reached by fall-through or by a
 * jump, but it cannot invent one.
 */
function findCallTargets(buffer, text) {
  const start = text.rawOffset;
  const end = start + text.rawSize;
  const targets = new Set();
  for (let i = start; i + 5 <= end; i++) {
    if (buffer[i] !== 0xe8) continue; // call rel32
    const rel = buffer.readInt32LE(i + 1);
    const rva = text.virtualAddress + (i - start) + 5 + rel;
    if (rva >= text.virtualAddress && rva < text.virtualAddress + text.virtualSize) targets.add(rva);
  }
  return [...targets].sort((a, b) => a - b);
}

/**
 * One entry per identified code entry point. The size is the distance to the next one
 * (the last one runs to the end of .text): an upper bound on the real function size, since
 * padding and jump tables between two functions are attributed to the earlier one.
 */
function buildFunctions(buffer, text, imageBase) {
  const starts = findCallTargets(buffer, text);
  const textEnd = text.virtualAddress + text.virtualSize;
  return starts.map((rva, index) => {
    const next = index + 1 < starts.length ? starts[index + 1] : textEnd;
    return {
      name: `FUN_${(imageBase + rva).toString(16).toUpperCase().padStart(8, "0")}`,
      size: next - rva,
      fuzzy_match_percent: 0,
      address: rva - text.virtualAddress,
      metadata: { virtual_address: imageBase + rva },
    };
  });
}

function buildReport(pe, buffer) {
  const byName = new Map(pe.sections.map((s) => [s.name, s]));
  for (const name of [...CODE_SECTIONS, ...DATA_SECTIONS]) {
    if (!byName.has(name)) fail(`section ${name} not found in the executable`);
  }
  const totalCode = CODE_SECTIONS.reduce((sum, n) => sum + byName.get(n).virtualSize, 0);
  const totalData = DATA_SECTIONS.reduce((sum, n) => sum + byName.get(n).virtualSize, 0);

  const text = byName.get(".text");
  const functions = buildFunctions(buffer, text, pe.imageBase);

  // One unit for now: the whole game code. Units become finer as translation units are
  // defined; each one will then carry its real target/base objects.
  const unit = {
    name: "game",
    measures: measures({ totalCode, totalData, totalUnits: 1, totalFunctions: functions.length }),
    sections: [],
    functions,
    metadata: { progress_categories: [] },
  };

  return {
    version: 2,
    measures: measures({ totalCode, totalData, totalUnits: 1, totalFunctions: functions.length }),
    units: [unit],
    categories: [],
  };
}

function generate(exePath, outPath) {
  const path = resolve(exePath);
  const buffer = readFileSync(path);
  const sha256 = createHash("sha256").update(buffer).digest("hex");
  const md5 = createHash("md5").update(buffer).digest("hex");
  if (buffer.length !== EXPECTED.size || sha256 !== EXPECTED.sha256 || md5 !== EXPECTED.md5) {
    fail(
      `${path} is not the EU dump this report describes.\n` +
        `  expected size=${EXPECTED.size} md5=${EXPECTED.md5} sha256=${EXPECTED.sha256}\n` +
        `  found    size=${buffer.length} md5=${md5} sha256=${sha256}`,
    );
  }

  const pe = readPE(buffer);
  const report = buildReport(pe, buffer);

  mkdirSync(dirname(resolve(outPath)), { recursive: true });
  writeFileSync(resolve(outPath), `${JSON.stringify(report, null, 2)}\n`);

  const text = pe.sections.find((s) => s.name === ".text");
  console.log(`read    ${path}`);
  console.log(`        sha256 ${sha256}`);
  console.log(`        entry  0x${(pe.imageBase + pe.entryPointRva).toString(16)} (RVA 0x${pe.entryPointRva.toString(16)})`);
  for (const s of pe.sections) {
    const kind = CODE_SECTIONS.includes(s.name) ? "code" : DATA_SECTIONS.includes(s.name) ? "data" : "-";
    console.log(
      `        ${s.name.padEnd(8)} ${kind.padEnd(4)} vsize ${String(s.virtualSize).padStart(8)}  raw ${String(s.rawSize).padStart(8)}`,
    );
  }
  console.log(`wrote   ${resolve(outPath)}`);
  console.log(`        total_code ${report.measures.total_code} bytes (.text virtual size ${text.virtualSize})`);
  console.log(`        total_data ${report.measures.total_data} bytes (${DATA_SECTIONS.join(" + ")})`);
  console.log(`        functions  ${report.measures.total_functions} (direct call targets; lower bound)`);
  console.log(`        matched    ${report.measures.matched_code} bytes, ${report.measures.matched_functions} functions (baseline)`);
}

function check(reportPath) {
  const path = resolve(reportPath);
  const report = JSON.parse(readFileSync(path, "utf8"));
  const problems = [];
  const at = (obj, key) => (obj && typeof obj === "object" ? obj[key] : undefined);

  if (at(report, "version") !== 2) problems.push(`version is ${at(report, "version")}, ByteMatch expects 2`);

  const top = at(report, "measures");
  if (!top || typeof top !== "object") {
    fail(`${path}: no "measures" object`);
  }
  for (const key of MEASURE_KEYS) {
    if (typeof top[key] !== "number") problems.push(`measures.${key} is missing or not a number`);
  }

  for (const key of ["matched_code", "matched_data", "matched_functions", "complete_code", "complete_data", "complete_units"]) {
    if (top[key] !== 0) problems.push(`measures.${key} is ${top[key]}; this baseline must be 0`);
  }
  if (top.total_code <= 0) problems.push("measures.total_code must be positive (it comes from .text)");
  if (top.total_data <= 0) problems.push("measures.total_data must be positive");

  const pairs = [
    ["matched_code_percent", "matched_code", "total_code"],
    ["matched_data_percent", "matched_data", "total_data"],
    ["matched_functions_percent", "matched_functions", "total_functions"],
    ["complete_code_percent", "complete_code", "total_code"],
    ["complete_data_percent", "complete_data", "total_data"],
  ];
  for (const [pctKey, partKey, totalKey] of pairs) {
    const expected = percent(top[partKey], top[totalKey]);
    if (Math.abs(top[pctKey] - expected) > 1e-4) {
      problems.push(`measures.${pctKey} is ${top[pctKey]}, expected ${expected} from ${partKey}/${totalKey}`);
    }
  }

  const units = Array.isArray(at(report, "units")) ? report.units : [];
  if (units.length === 0) problems.push('"units" is empty; ByteMatch draws the unit map from it');
  if (top.total_units !== units.length) problems.push(`measures.total_units is ${top.total_units} but there are ${units.length} units`);

  const sum = {};
  for (const key of MEASURE_KEYS) sum[key] = 0;
  for (const [i, unit] of units.entries()) {
    const um = at(unit, "measures");
    if (!um) {
      problems.push(`units[${i}] has no measures`);
      continue;
    }
    for (const key of MEASURE_KEYS) sum[key] += um[key] ?? 0;
  }
  for (const key of ["total_code", "total_data", "matched_code", "matched_data", "total_functions", "total_units", "complete_units"]) {
    if (sum[key] !== top[key]) problems.push(`sum of units' ${key} is ${sum[key]} but the report says ${top[key]}`);
  }

  // The function list is what backs total_functions: it must be there and add up.
  const functions = Array.isArray(at(units[0], "functions")) ? units[0].functions : [];
  if (top.total_functions !== functions.length) {
    problems.push(`measures.total_functions is ${top.total_functions} but the unit lists ${functions.length} functions`);
  }
  let functionBytes = 0;
  for (const [i, fn] of functions.entries()) {
    if (typeof at(fn, "name") !== "string" || !at(fn, "name")) problems.push(`functions[${i}] has no name`);
    if (typeof at(fn, "size") !== "number" || at(fn, "size") <= 0) problems.push(`functions[${i}] has no positive size`);
    functionBytes += at(fn, "size") ?? 0;
  }
  if (functionBytes > top.total_code) {
    problems.push(`the function sizes add up to ${functionBytes} bytes, more than the ${top.total_code} bytes of code`);
  }

  if (problems.length) {
    console.error(`${path}: ${problems.length} problem(s)`);
    for (const p of problems) console.error(`  - ${p}`);
    process.exit(1);
  }
  console.log(
    `${path}: ok (schema v2, ${top.total_code} code bytes, ${top.total_data} data bytes, ` +
      `${top.total_functions} functions identified, 0 matched)`,
  );
}

const args = process.argv.slice(2);
const exeIndex = args.indexOf("--exe");
const outIndex = args.indexOf("--out");
const checkIndex = args.indexOf("--check");

if (exeIndex !== -1) {
  const exe = args[exeIndex + 1];
  if (!exe) fail("--exe needs a path to GEX3D.EXE");
  generate(exe, outIndex !== -1 ? args[outIndex + 1] : DEFAULT_OUT);
} else if (checkIndex !== -1) {
  const next = args[checkIndex + 1];
  check(next && !next.startsWith("--") ? next : DEFAULT_OUT);
} else {
  console.log(`usage:
  node tools/bytematch-report.mjs --exe <GEX3D.EXE> [--out ${DEFAULT_OUT}]
  node tools/bytematch-report.mjs --check [${DEFAULT_OUT}]

--exe    regenerate the report from the original EU executable (verifies its hashes).
--check  validate the committed report; CI runs this.`);
  process.exit(args.length ? 2 : 0);
}
