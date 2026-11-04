/**
 * Evidence-based subsystem segmentation (HYPOTHESIS) over the committed EU map.
 *
 * This module does not disassemble anything and does not add facts to the map: it
 * groups mapped starts that the published artifacts already connect, and it states
 * *why* each one is in the group. Every group name is a HYPOTHESIS: the map proves
 * structure (who calls whom, which import is reachable), never meaning.
 *
 * Evidence layers, all recomputed from `analysis/EU/`:
 *
 *   I  the function's own extent references an import of the segment family (IAT);
 *   T  the function calls an import stub (`kind=thunk`) of the segment family;
 *   A  a documented anchor lies inside the function (`anchors.json`);
 *   G  the function is at call-graph distance 1, or 2 through a member of the same
 *      segment, from an anchored function or from one with I/T;
 *   O  the function shares at least 4 direct callees with an anchored function
 *      (structural overlap, used only for a start that no edge reaches).
 *
 * Two layers are manual, because CI has no binary to re-check them:
 *
 *   S  a string literal was located in the original `.rdata` and a reference to it
 *      was found inside the function's extent (the address and text are recorded);
 *   R  the call sequence was read in raw disassembly (the note says what was seen).
 *
 * Confidence is derived, not asserted:
 *
 *   ALTA   direct API of the segment family (I or T) + reachable from WinMain by
 *          direct calls + the start is not merely a prologue candidate;
 *   MEDIA  anchor, string, callee overlap, a direct API that WinMain cannot reach
 *          by direct calls, or distance 1 to an anchored/API member;
 *   BAJA   prologue-only start, or nothing beyond distance 2 / reachability.
 *
 * `W=false` (no direct-call path from WinMain) is a fact about the *direct* graph,
 * not proof of dead code: callbacks (window/dialog procedures), indirect calls and
 * data-driven dispatch are invisible to it.
 */

import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";

export const WINMAIN = 0x463278;
export const MIN_OVERLAP = 4;

export const EVIDENCE = {
  I: "import de la familia del segmento referenciado dentro de la extensión de la función",
  T: "llamada directa a un stub de importación de la familia del segmento",
  A: "ancla documental dentro de la función",
  G: "distancia 1 (o 2 a través de un miembro del segmento) a un ancla o a una función con I/T",
  O: `≥${MIN_OVERLAP} callees directos compartidos con una función anclada`,
  S: "referencia a un string verificado en el binario original (manual: dirección y texto registrados)",
  R: "secuencia de llamadas leída en desensamblado crudo (manual: la nota indica qué se vio)",
};

const MANUAL = new Set(["S", "R"]);

/**
 * The selected groups. Membership is deliberately small (65 of 2 655 starts): the
 * point is a defensible first slice, not a classification of the whole binary.
 */
export const SEGMENTS = [
  {
    id: "S1",
    name: "HYPOTHESIS: paso de frame y tiempo",
    apiFamilies: ["KERNEL32", "GLIDE2X"],
    rationale:
      "El frame step confirmado (0x0043EA36) llama a entrada, update, espera de cola y swap; " +
      "el update y la init por nivel leen GetTickCount y comparten el contador de tiempo.",
    members: [
      { addr: "0x0043EA36", confidence: "ALTA", note: "frame step: input -> update -> espera de cola -> swap(3)" },
      { addr: "0x0043E1BA", confidence: "ALTA", note: "update por frame; sella elapsed/16.666 en +0x10C" },
      { addr: "0x0043B11D", confidence: "ALTA", note: "init temporal por nivel; t0 = GetTickCount" },
      { addr: "0x00412F8B", confidence: "MEDIA", note: "cuenta atras por frame (+0x4BC--); sin import directo" },
      { addr: "0x00461FBD", confidence: "ALTA", note: "bucle alternativo con swap(2) durante init" },
      { addr: "0x0046300C", confidence: "MEDIA", note: "hoja de 5 B llamada por los dos bucles; rol sin determinar" },
      { addr: "0x004519DE", confidence: "MEDIA", note: "delta -> tabla -> WORD; ancla R2 en +46" },
      {
        addr: "0x00424A93",
        confidence: "BAJA",
        note: "candidato por prólogo, 0 llamadores; comparte 8 callees con el update (variante no llamada)",
      },
    ],
  },
  {
    id: "S2",
    name: "HYPOTHESIS: render Glide",
    apiFamilies: ["GLIDE2X"],
    rationale:
      "Las únicas 21 funciones de juego que tocan Glide se reparten entre el envío de render, " +
      "la carga de texturas y el dibujo de triángulos; aquí se agrupan las 10 que no son bucle ni shell.",
    members: [
      { addr: "0x0043DC3A", confidence: "ALTA", note: "envio de render + drenaje de cola (grBufferNumPending)" },
      { addr: "0x0046558A", confidence: "ALTA", note: "mayor consumidor Glide: 10 APIs, 17 068 B" },
      { addr: "0x00461C32", confidence: "ALTA", note: "descarga de mipmaps de textura" },
      { addr: "0x004329EF", confidence: "ALTA", note: "carga de textura 3DF (gu3dfLoad/gu3dfGetInfo)" },
      { addr: "0x00432BBD", confidence: "ALTA", note: "triángulos + estado de color/alpha" },
      { addr: "0x00432DD1", confidence: "ALTA", note: "triángulos + estado de color/alpha (variante corta)" },
      { addr: "0x00424C0F", confidence: "ALTA", note: "swap(1) de la ruta de arranque; ancla en +112" },
      { addr: "0x00424D82", confidence: "ALTA", note: "grSstQueryHardware: sonda de hardware Glide" },
      { addr: "0x00473238", confidence: "ALTA", note: "descarga de mipmap y de nivel parcial" },
      { addr: "0x00469836", confidence: "ALTA", note: "grTexNCCTable: tabla de paleta" },
    ],
  },
  {
    id: "S3",
    name: "HYPOTHESIS: audio, mezcla y voz",
    apiFamilies: ["DSOUND", "WINMM"],
    rationale:
      "Dos rutas independientes: DirectSound (creación y objetos) y WinMM (volumen auxiliar y MCI); " +
      "la ruta de voz se apoya en un string de `audio\\voiceuk` y en el ancla de housekeeping.",
    members: [
      { addr: "0x00538551", confidence: "ALTA", note: "DirectSoundCreate + creación de objeto por vtable" },
      { addr: "0x00538CB0", confidence: "MEDIA", note: "segundo DirectSoundCreate; sin camino directo desde WinMain" },
      { addr: "0x0053B10B", confidence: "MEDIA", note: "cadena de audio; alcanza DirectSound a profundidad 2" },
      { addr: "0x0053FCB7", confidence: "MEDIA", note: "housekeeping de voz (ancla); no alcanza APIs del sistema" },
      {
        addr: "0x0053FF0D",
        confidence: "MEDIA",
        evidence: ["S"],
        string: { text: "%s\\audio\\voiceuk\\%s.sag", va: "0x0055A708" },
        note: "ruta <InstallDir>\\audio\\voiceuk\\%s.sag (string verificado)",
      },
      { addr: "0x0053FE80", confidence: "MEDIA", note: "snapshot del cociente (ancla R1 en +84); llama a la ruta de voz" },
      { addr: "0x0053D6C1", confidence: "ALTA", note: "auxGetDevCaps/auxGetNumDevs/auxGetVolume" },
      { addr: "0x0053DE1A", confidence: "ALTA", note: "auxSetVolume" },
      { addr: "0x0053D822", confidence: "ALTA", note: "auxGetVolume + mciSendCommandA (frontera audio/cine)" },
    ],
  },
  {
    id: "S4",
    name: "HYPOTHESIS: cinemáticas y bucle de película",
    apiFamilies: ["GLIDE2X", "USER32"],
    rationale:
      "Cadena verificada WinMain -> 0x00538017 -> 0x00537ED1 -> 0x0053A5A2 -> 0x0053A5FB; " +
      "el bucle pinta con LFB, hace swap, bombea mensajes y avanza por tick.",
    members: [
      { addr: "0x00537BB0", confidence: "MEDIA", note: "wrapper de cine (ancla E-3): GetTickCount" },
      { addr: "0x00537BBB", confidence: "ALTA", note: "bucle de mensajes de cine: BeginPaint/EndPaint/PeekMessageA" },
      { addr: "0x00537D70", confidence: "ALTA", note: "volcado a framebuffer lineal (grLfbWriteRegion)" },
      { addr: "0x00537E69", confidence: "ALTA", note: "swap(1) del wrapper de cine (ancla en +5)" },
      { addr: "0x00537E7A", confidence: "MEDIA", note: "LFB + swap encadenados; sin import directo" },
      { addr: "0x00537EA6", confidence: "ALTA", note: "grSstWinOpen 640x480@60Hz para cine (ancla)" },
      { addr: "0x00537ED1", confidence: "ALTA", note: "entrada de cine: GetTickCount + grGlideGetState" },
      { addr: "0x00538017", confidence: "MEDIA", note: "envoltorio de 41 B llamado desde WinMain" },
      { addr: "0x0053A5A2", confidence: "MEDIA", note: "prepara el bucle: audio, estado Glide y 0x0053A5FB" },
      { addr: "0x0053A5FB", confidence: "MEDIA", note: "bucle de película: dibujo, swap, mensajes, tick" },
      { addr: "0x0053A757", confidence: "MEDIA", note: "paso auxiliar del bucle; vuelve a 0x00537BB0" },
      {
        addr: "0x0053A43A",
        confidence: "MEDIA",
        evidence: ["R"],
        note: "función recursiva invocada desde el bucle de película; sin API alcanzable",
        raw: "el bucle 0x0053A5FB la llama de forma directa y ella se llama a sí misma (arista de 0x0053A43A a 0x0053A43A)",
      },
    ],
  },
  {
    id: "S5",
    name: "HYPOTHESIS: entrada (teclado y joystick)",
    apiFamilies: ["USER32", "WINMM"],
    rationale:
      "El frame step llama a 0x0043F80D antes del update; de ahí cuelgan el sondeo de joystick y " +
      "las conversiones de tecla con VkKeyScanA.",
    members: [
      { addr: "0x0043F80D", confidence: "MEDIA", note: "paso de entrada del frame; sin import directo" },
      { addr: "0x0043EA70", confidence: "BAJA", note: "1019 B llamada por el paso de entrada; sin API ni ancla" },
      { addr: "0x0043F51D", confidence: "ALTA", note: "joyGetPosEx (sondeo de joystick)" },
      { addr: "0x0043EE6B", confidence: "ALTA", note: "joyGetDevCapsA + joyGetPosEx (capacidades)" },
      { addr: "0x004403D6", confidence: "MEDIA", note: "VkKeyScanA; sin camino directo desde WinMain (callback/indirecta)" },
      { addr: "0x0043FD44", confidence: "ALTA", note: "VkKeyScanA" },
      { addr: "0x0043FB8D", confidence: "ALTA", note: "VkKeyScanA" },
      { addr: "0x00440CD1", confidence: "MEDIA", note: "VkKeyScanA; sin camino directo desde WinMain" },
      { addr: "0x00440D79", confidence: "MEDIA", note: "VkKeyScanA; sin camino directo desde WinMain" },
    ],
  },
  {
    id: "S6",
    name: "HYPOTHESIS: shell Win32 (ventana, mensajes y arranque)",
    apiFamilies: ["USER32", "GDI32", "GLIDE2X", "KERNEL32"],
    rationale:
      "WinMain y sus vecinos directos: registro de clase, creación de ventana, bombeo de mensajes, " +
      "diálogos y apagado; mezcla USER32/GDI32 con la init y el cierre de Glide.",
    members: [
      { addr: "0x00463278", confidence: "ALTA", note: "WinMain: init Glide, ventana, bucle y registro" },
      { addr: "0x0046324D", confidence: "ALTA", note: "grSstWinOpen 512x384@75Hz (ancla)" },
      { addr: "0x00463011", confidence: "ALTA", note: "apagado: DestroyWindow + MessageBoxA + grGlideShutdown" },
      { addr: "0x00463091", confidence: "ALTA", note: "registro de clase y CreateWindowExA" },
      { addr: "0x0046317F", confidence: "BAJA", note: "candidato por prólogo: Sleep/DispatchMessageA/PeekMessageA (0 llamadores)" },
      { addr: "0x00463C4E", confidence: "BAJA", note: "candidato por prólogo: DialogBoxParamA/DefWindowProcA (0 llamadores)" },
      { addr: "0x00463DE5", confidence: "BAJA", note: "candidato por prólogo: EndDialog (0 llamadores)" },
      { addr: "0x00463205", confidence: "ALTA", note: "WaitMessage + ruta de MCI/ventana" },
      { addr: "0x00463BE5", confidence: "MEDIA", note: "15 B entre el registro de clase y WinMain; rol sin determinar" },
      { addr: "0x00463BF4", confidence: "MEDIA", note: "CloseWindow + ShowCursor + grSstControl; solo alcanzable desde el diálogo" },
      { addr: "0x00463C21", confidence: "MEDIA", note: "BringWindowToTop + ShowCursor + grSstControl; solo desde el diálogo" },
    ],
  },
  {
    id: "S7",
    name: "HYPOTHESIS: configuración en el registro",
    apiFamilies: ["ADVAPI32"],
    rationale:
      "Los seis únicos usos de ADVAPI32: crear/cerrar clave y leer/escribir valores; la lectura de " +
      "InstallDir y CDDriveName está anclada y la llama WinMain.",
    members: [
      { addr: "0x00442EEC", confidence: "ALTA", note: "RegCreateKeyA" },
      { addr: "0x00442F1D", confidence: "ALTA", note: "RegCloseKey" },
      { addr: "0x00442F41", confidence: "ALTA", note: "RegQueryValueExA: InstallDir + CDDriveName (ancla)" },
      { addr: "0x00442F95", confidence: "BAJA", note: "candidato por prólogo: RegSetValueExA (0 llamadores)" },
      { addr: "0x00442FCD", confidence: "BAJA", note: "candidato por prólogo: RegQueryValueExA (0 llamadores)" },
      { addr: "0x00443034", confidence: "BAJA", note: "candidato por prólogo: RegSetValueExA (0 llamadores)" },
    ],
  },
];

const parseAddress = (value) => {
  const n = Number.parseInt(String(value).replace(/^0x/i, ""), 16);
  if (!Number.isFinite(n)) throw new Error(`bad address: ${value}`);
  return n;
};
export const hex = (value) => `0x${value.toString(16).padStart(8, "0")}`;

function readRows(file) {
  if (!existsSync(file)) throw new Error(`missing ${file}; run "gexmap map --exe <GEX3D.EXE>" first`);
  return readFileSync(file, "utf8")
    .split(/\r?\n/)
    .filter((line) => line.length && !line.startsWith("#"))
    .map((line) => line.split("\t"));
}

export function loadArtifacts(dir = "analysis/EU") {
  const base = resolve(dir);
  const functions = readRows(join(base, "functions.tsv")).map((row) => ({
    start: Number.parseInt(row[0], 16),
    size: Number.parseInt(row[1], 10),
    kind: row[2],
    basis: row[3],
    symbol: row[4],
    callers: Number.parseInt(row[5], 10),
    imports: row[7] ? row[7].split(";") : [],
  }));
  const edges = readRows(join(base, "calls.tsv")).map((row) => ({
    from: Number.parseInt(row[0], 16),
    to: Number.parseInt(row[1], 16),
    kind: row[2],
    count: Number.parseInt(row[3], 10),
  }));
  const anchorsPath = join(base, "anchors.json");
  if (!existsSync(anchorsPath)) throw new Error(`missing ${anchorsPath}`);
  const anchors = JSON.parse(readFileSync(anchorsPath, "utf8")).anchors ?? [];
  return { dir: base, functions, edges, anchors };
}

const familyOf = (name) => name.split(".")[0].toUpperCase();

export function buildSegments(model, definitions = SEGMENTS) {
  const { functions, edges, anchors } = model;
  const byStart = new Map(functions.map((fn) => [fn.start, fn]));
  const out = new Map();
  const inn = new Map();
  for (const edge of edges) {
    if (!out.has(edge.from)) out.set(edge.from, []);
    out.get(edge.from).push(edge);
    if (!inn.has(edge.to)) inn.set(edge.to, []);
    inn.get(edge.to).push(edge);
  }
  const thunkFamily = new Map();
  for (const fn of functions) {
    if (fn.kind === "thunk" && fn.imports.length === 1) thunkFamily.set(fn.start, familyOf(fn.imports[0]));
  }
  const anchorStarts = new Set(
    anchors.map((anchor) => (anchor.function_start ? parseAddress(anchor.function_start) : null)).filter(Boolean),
  );
  const anchorsAt = new Map();
  for (const anchor of anchors) {
    if (!anchor.function_start) continue;
    const start = parseAddress(anchor.function_start);
    if (!anchorsAt.has(start)) anchorsAt.set(start, []);
    anchorsAt.get(start).push(anchor);
  }

  const reachable = new Set([WINMAIN]);
  let frontier = [WINMAIN];
  while (frontier.length) {
    const next = [];
    for (const node of frontier) {
      for (const edge of out.get(node) ?? []) {
        if (!reachable.has(edge.to)) {
          reachable.add(edge.to);
          next.push(edge.to);
        }
      }
    }
    frontier = next;
  }

  const directFamilies = (start) => {
    const names = new Set(byStart.get(start)?.imports ?? []);
    for (const edge of out.get(start) ?? []) {
      const family = thunkFamily.get(edge.to);
      if (family) names.add(family);
    }
    return new Set([...names].map(familyOf));
  };
  const familiesByStart = new Map(functions.map((fn) => [fn.start, directFamilies(fn.start)]));

  const problems = [];
  const memberOwner = new Map();
  for (const def of definitions) {
    for (const declared of def.members) {
      const start = parseAddress(declared.addr);
      if (memberOwner.has(start)) problems.push(`${def.id}: ${declared.addr} already belongs to ${memberOwner.get(start)}`);
      else memberOwner.set(start, def.id);
    }
  }
  const reports = [];

  for (const def of definitions) {
    const families = new Set(def.apiFamilies);
    const members = [];
    for (const declared of def.members) {
      const start = parseAddress(declared.addr);
      const fn = byStart.get(start);
      if (!fn) {
        problems.push(`${def.id}: ${declared.addr} is not a mapped start`);
        continue;
      }
      if (fn.kind !== "func") {
        problems.push(`${def.id}: ${declared.addr} is a ${fn.kind}, not game code`);
        continue;
      }
      if (declared.confidence && !["ALTA", "MEDIA", "BAJA"].includes(declared.confidence)) {
        problems.push(`${def.id}: ${declared.addr} has unknown confidence ${declared.confidence}`);
      }

      const direct = familiesByStart.get(start) ?? new Set();
      const hasI = [...direct].some((family) => families.has(family));
      const hasT = (out.get(start) ?? []).some((edge) => {
        const family = thunkFamily.get(edge.to);
        return family && families.has(family);
      });
      const hasA = anchorStarts.has(start);
      const hasIT = hasI || hasT;

      let overlap = 0;
      let overlapWith = null;
      if (!hasIT) {
        const callees = new Set((out.get(start) ?? []).map((edge) => edge.to));
        for (const anchored of anchorStarts) {
          if (anchored === start) continue;
          const shared = (out.get(anchored) ?? []).filter((edge) => callees.has(edge.to)).length;
          if (shared > overlap) {
            overlap = shared;
            overlapWith = anchored;
          }
        }
      }
      const hasO = overlap >= MIN_OVERLAP;

      const neighbors = new Set([
        ...(out.get(start) ?? []).map((edge) => edge.to),
        ...(inn.get(start) ?? []).map((edge) => edge.from),
      ]);
      neighbors.delete(start);
      const isSeed = (node) => {
        if (anchorStarts.has(node)) return true;
        const nodeFamilies = familiesByStart.get(node);
        if (!nodeFamilies || nodeFamilies.size === 0) return false;
        return [...nodeFamilies].some((family) => families.has(family));
      };
      let gRadius = 0;
      if ([...neighbors].some((node) => isSeed(node))) gRadius = 1;
      else {
        for (const node of neighbors) {
          if (memberOwner.get(node) !== def.id) continue;
          const second = new Set([
            ...(out.get(node) ?? []).map((edge) => edge.to),
            ...(inn.get(node) ?? []).map((edge) => edge.from),
          ]);
          if ([...second].some((other) => other !== start && isSeed(other))) {
            gRadius = 2;
            break;
          }
        }
      }

      const hasS = Boolean(declared.string);
      const hasR = Boolean(declared.raw);
      if (declared.string && (!declared.string.text || !declared.string.va)) {
        problems.push(`${def.id}: ${declared.addr} declares S without {text, va}`);
      }
      if (declared.raw && declared.raw.trim().length < 20) {
        problems.push(`${def.id}: ${declared.addr} declares R without a substantive note`);
      }

      const computed = new Set();
      if (hasI) computed.add("I");
      if (hasT) computed.add("T");
      if (hasA) computed.add("A");
      if (gRadius) computed.add("G");
      if (hasO) computed.add("O");
      if (hasS) computed.add("S");
      if (hasR) computed.add("R");

      for (const code of declared.evidence ?? []) {
        if (!(code in EVIDENCE)) problems.push(`${def.id}: ${declared.addr} declares unknown evidence ${code}`);
        else if (!computed.has(code)) {
          problems.push(`${def.id}: ${declared.addr} declares ${code} but the artifacts do not support it`);
        }
      }
      for (const code of MANUAL) {
        if (computed.has(code) && !(declared.evidence ?? []).includes(code)) {
          problems.push(`${def.id}: ${declared.addr} has manual evidence ${code} but does not declare it`);
        }
      }

      const prologue = fn.basis === "prologue";
      const hasOther = hasA || hasS || hasO || hasR;
      const expected = prologue
        ? "BAJA"
        : hasIT && reachable.has(start)
          ? "ALTA"
          : hasOther || (hasIT && !reachable.has(start)) || gRadius === 1
            ? "MEDIA"
            : "BAJA";
      if (declared.confidence && declared.confidence !== expected) {
        problems.push(
          `${def.id}: ${declared.addr} declares ${declared.confidence}, the evidence supports ${expected}`,
        );
      }

      members.push({
        addr: hex(start),
        start,
        size: fn.size,
        basis: fn.basis,
        callers: fn.callers,
        reachableFromWinMain: reachable.has(start),
        evidence: [...computed].sort().join(""),
        families: [...direct].filter((family) => families.has(family)).sort(),
        anchor: (anchorsAt.get(start) ?? []).map((a) => a.label),
        overlap: hasO ? { shared: overlap, with: hex(overlapWith) } : null,
        graphRadius: gRadius,
        string: declared.string ?? null,
        raw: declared.raw ?? null,
        confidence: expected,
        note: declared.note ?? "",
      });
    }

    const directFamiliesInSegment = new Set();
    for (const member of members) for (const family of member.families) directFamiliesInSegment.add(family);
    for (const family of families) {
      if (!directFamiliesInSegment.has(family)) {
        problems.push(`${def.id}: declares family ${family} but no member uses it directly`);
      }
    }
    if (!members.length) problems.push(`${def.id}: no members`);
    if (!members.some((member) => member.confidence === "ALTA")) problems.push(`${def.id}: no ALTA member`);

    reports.push({
      ...def,
      count: members.length,
      confidence: {
        ALTA: members.filter((m) => m.confidence === "ALTA").length,
        MEDIA: members.filter((m) => m.confidence === "MEDIA").length,
        BAJA: members.filter((m) => m.confidence === "BAJA").length,
      },
      members,
    });
  }

  const crossEdges = [];
  for (const edge of edges) {
    const from = memberOwner.get(edge.from);
    const to = memberOwner.get(edge.to);
    if (from && to && from !== to) crossEdges.push({ from: hex(edge.from), to: hex(edge.to), segments: `${from}->${to}` });
  }

  const allMembers = reports.flatMap((report) => report.members);
  const summary = {
    segments: reports.length,
    members: allMembers.length,
    mappedStarts: functions.length,
    gameStarts: functions.filter((fn) => fn.kind === "func").length,
    byConfidence: {
      ALTA: allMembers.filter((m) => m.confidence === "ALTA").length,
      MEDIA: allMembers.filter((m) => m.confidence === "MEDIA").length,
      BAJA: allMembers.filter((m) => m.confidence === "BAJA").length,
    },
    reachableFromWinMain: reachable.size,
    crossSegmentEdges: crossEdges.length,
  };

  if (problems.length) return { segments: reports, summary, crossEdges, problems };
  return { segments: reports, summary, crossEdges, problems: [] };
}
