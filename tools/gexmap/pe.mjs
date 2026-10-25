/**
 * Minimal PE32 reader for GEX3D.EXE.
 *
 * Only the headers this project actually needs are decoded: sections, the entry
 * point, the image base and the import table (so an import thunk can be named after
 * the function it forwards to). No heuristic, no guessing: every field comes from
 * the file.
 */

const PE_MACHINE_I386 = 0x014c;
const PE32_MAGIC = 0x010b;

export function fail(message) {
  throw new Error(message);
}

export function readPE(buffer) {
  if (buffer.length < 0x40) fail("file is too small to be a PE image");
  if (buffer.toString("ascii", 0, 2) !== "MZ") fail("not a DOS/PE file (no MZ header)");

  const peOffset = buffer.readUInt32LE(0x3c);
  if (peOffset + 24 > buffer.length) fail("PE header offset points past the end of the file");
  if (buffer.toString("ascii", peOffset, peOffset + 4) !== "PE\0\0") fail("not a PE file (no PE signature)");

  const machine = buffer.readUInt16LE(peOffset + 4);
  const sectionCount = buffer.readUInt16LE(peOffset + 6);
  const optionalSize = buffer.readUInt16LE(peOffset + 20);
  const opt = peOffset + 24;
  const magic = buffer.readUInt16LE(opt);

  if (machine !== PE_MACHINE_I386) fail(`expected machine 0x014c (i386), found 0x${machine.toString(16)}`);
  if (magic !== PE32_MAGIC) fail(`expected PE32 optional header (0x10b), found 0x${magic.toString(16)}`);

  const entryPointRva = buffer.readUInt32LE(opt + 16);
  const imageBase = buffer.readUInt32LE(opt + 28);
  const numberOfRvaAndSizes = buffer.readUInt32LE(opt + 92);
  const dataDirectories = [];
  for (let i = 0; i < numberOfRvaAndSizes; i++) {
    const at = opt + 96 + i * 8;
    if (at + 8 > opt + optionalSize) break;
    dataDirectories.push({ rva: buffer.readUInt32LE(at), size: buffer.readUInt32LE(at + 4) });
  }

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

  return {
    machine,
    entryPointRva,
    entryPoint: imageBase + entryPointRva,
    imageBase,
    dataDirectories,
    sections,
    offsetOf(rva) {
      for (const s of sections) {
        const span = Math.max(s.virtualSize, s.rawSize);
        if (rva >= s.virtualAddress && rva < s.virtualAddress + span) {
          return s.rawOffset + (rva - s.virtualAddress);
        }
      }
      return null;
    },
  };
}

/**
 * The import table as Map<IAT virtual address, { dll, func }>.
 *
 * An import thunk (`jmp dword ptr [slot]`) is named after the function held in its
 * slot, so this map is what turns an anonymous stub into `glide2x!grBufferSwap`.
 */
export function readImports(buffer, pe) {
  const imports = new Map();
  const directory = pe.dataDirectories[1];
  if (!directory || !directory.rva) return imports;

  let descriptor = pe.offsetOf(directory.rva);
  if (descriptor === null) return imports;

  for (let i = 0; i < 4096; i++) {
    if (descriptor + 20 > buffer.length) break;
    const nameRva = buffer.readUInt32LE(descriptor + 12);
    if (!nameRva) break;
    const firstThunk = buffer.readUInt32LE(descriptor + 16);

    const nameRaw = pe.offsetOf(nameRva);
    let dll = "";
    for (let p = nameRaw; p !== null && p < buffer.length && buffer[p] !== 0; p++) dll += String.fromCharCode(buffer[p]);

    let thunk = pe.offsetOf(buffer.readUInt32LE(descriptor));
    for (let k = 0; thunk !== null && thunk + 4 <= buffer.length; k++) {
      const value = buffer.readUInt32LE(thunk);
      if (value === 0) break;
      let func;
      if (value & 0x80000000) {
        func = `#${value & 0xffff}`;
      } else {
        const hint = pe.offsetOf(value);
        func = "";
        for (let q = hint + 2; q !== null && q < buffer.length && buffer[q] !== 0; q++) func += String.fromCharCode(buffer[q]);
      }
      imports.set(pe.imageBase + firstThunk + k * 4, { dll, func });
      thunk += 4;
    }
    descriptor += 20;
  }
  return imports;
}
