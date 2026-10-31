// make_demo_sag.mjs - write a synthetic SLAG container for `sagdump`.
//
// Synthetic means exactly that: 8 bytes of 8-bit silence at 22050 Hz mono.
// No retail asset is copied, generated or required. The layout follows
// docs/SAG_CONTAINER.md §3 (header + "fmt " + "data").

import { writeFileSync } from "node:fs";

const out = process.argv[2] ?? "demo.sag";

const fmt = Buffer.alloc(18);
fmt.writeUInt16LE(1, 0); // WAVE_FORMAT_PCM
fmt.writeUInt16LE(1, 2); // mono
fmt.writeUInt32LE(22050, 4); // nSamplesPerSec
fmt.writeUInt32LE(22050, 8); // nAvgBytesPerSec
fmt.writeUInt16LE(1, 12); // nBlockAlign
fmt.writeUInt16LE(8, 14); // wBitsPerSample
fmt.writeUInt16LE(0, 16); // cbSize

const pcm = Buffer.alloc(8, 0x80); // 8-bit unsigned silence

const chunk = (tag, payload) => {
  const head = Buffer.alloc(8);
  head.write(tag, 0, 4, "latin1");
  head.writeUInt32LE(payload.length, 4);
  return Buffer.concat([head, payload]);
};

const body = Buffer.concat([
  Buffer.from("WAVE", "latin1"),
  chunk("fmt ", fmt),
  chunk("data", pcm),
]);

const header = Buffer.alloc(8);
header.write("SLAG", 0, 4, "latin1");
header.writeUInt32LE(body.length, 4); // RIFF-like form size

writeFileSync(out, Buffer.concat([header, body]));
console.log(`${out}: ${8 + body.length} bytes (synthetic)`);
