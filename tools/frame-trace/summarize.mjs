/**
 * summarize.mjs - turn a frame-trace TSV log into per-call-site statistics.
 *
 *   node tools/frame-trace/summarize.mjs trace.log [trace2.log ...]
 *
 * The log format is written by tracer.cs:
 *   BP\t<seq>\t<ms>\t<tid>\t<addr>\t<esp>\t<ret>\t<a0>\t<a1>\t<a2>
 *
 * Every session includes the update entry 0x0043E1BA as the frame anchor: it is
 * called exactly once per frame step, so the hit stream can be split into frames
 * by anchor hits. That gives an exact denominator instead of a guessed one.
 *
 * `calls` counts every hit; `frames` counts frames in which the site appeared at
 * least once. A site that runs once per frame shows calls == frames.
 */

import fs from "node:fs";

export const ANCHOR = "0x0043E1BA";

export function parse(file) {
  const hits = [];
  for (const line of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    if (!line.startsWith("BP\t")) continue;
    const p = line.split("\t");
    hits.push({ seq: +p[1], ms: +p[2], tid: +p[3], addr: p[4], esp: p[5], ret: p[6], a0: p[7] });
  }
  return hits;
}

/** Split the hit stream into frames delimited by the anchor call. */
export function frames(hits, anchor = ANCHOR) {
  const out = [];
  let cur = null;
  for (const h of hits) {
    if (h.addr === anchor) {
      cur = [];
      out.push(cur);
    } else if (cur) cur.push(h);
  }
  return out;
}

export function summarize(file, anchor = ANCHOR) {
  const groups = frames(parse(file), anchor);
  const bySite = new Map();
  for (const frame of groups) {
    const seen = new Set();
    for (const h of frame) {
      const key = `${h.addr}<-${h.ret}`;
      if (!bySite.has(key)) bySite.set(key, { addr: h.addr, ret: h.ret, calls: 0, frames: 0 });
      const row = bySite.get(key);
      row.calls++;
      if (!seen.has(key)) { seen.add(key); row.frames++; }
    }
  }
  return {
    file,
    frameSteps: groups.length,
    hits: groups.reduce((sum, frame) => sum + frame.length, 0),
    sites: [...bySite.values()].sort((a, b) => b.calls - a.calls || a.addr.localeCompare(b.addr)),
  };
}

if (process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("summarize.mjs")) {
  for (const file of process.argv.slice(2)) {
    const r = summarize(file);
    console.log(`# ${file}  frameSteps=${r.frameSteps} hits=${r.hits}`);
    for (const s of r.sites) {
      console.log(`  ${s.addr} <- ${s.ret}  calls=${s.calls} frames=${s.frames}/${r.frameSteps}`);
    }
  }
}
