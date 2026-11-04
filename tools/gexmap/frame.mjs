/**
 * frame.mjs - frame step trace (EU) as declarative, checkable data.
 *
 * Two kinds of statements live here, and they are deliberately kept apart:
 *
 *  1. STRUCTURE (checked by `validateFrame` against analysis/EU): the call
 *     sites inside 0x0043EA36 and 0x0043E1BA, their targets, and the fact that
 *     every site lies inside the caller's mapped range with a matching edge in
 *     calls.tsv. This is CI-safe: it needs no binary and no execution.
 *
 *  2. MEASUREMENTS (2026-10-30, nine instrumented sessions): how many times each
 *     call site ran, and in how many frames. They are recorded, not recomputed;
 *     the method and the raw logs are described in docs/FRAME_STEP_TRACE.md and
 *     can be reproduced with tools/frame-trace.
 *
 * `site` is the address of the `call` instruction (static, read from the
 * disassembly); `ret` is the return address the CPU pushed, which is what the
 * tracer observed on the stack. They differ by the 5 bytes of `call rel32`.
 *
 * Semantic claims are NOT made here as purpose. `SEMANTICS` records what a
 * function mechanically does (code shape) with the instruction evidence that
 * supports it, and marks the reading CONFIRMED (mechanics) or HYPOTHESIS
 * (purpose). A CONFIRMED claim never asserts what the data *means* in the game.
 */

export const FRAME_STEP = {
  address: 0x0043ea36,
  size: 58,
  label: "frame step (llamado desde WinMain 0x00463AAF)",
  calls: [
    { site: 0x0043ea3d, ret: 0x0043ea42, target: 0x0043f80d, label: "entrada del frame" },
    { site: 0x0043ea49, ret: 0x0043ea4e, target: 0x0043e1ba, label: "update por frame" },
    { site: 0x0043ea51, ret: 0x0043ea56, target: 0x0054053e, label: "espera de cola (bucle, _grBufferNumPending@0)" },
    { site: 0x0043ea5e, ret: 0x0043ea63, target: 0x005404f6, label: "swap(3) (_grBufferSwap@4)" },
    { site: 0x0043ea63, ret: 0x0043ea68, target: 0x0046300c, label: "hoja final de 5 B" },
  ],
};

/**
 * Direct calls of the update 0x0043E1BA (25 sites, 24 distinct targets), in the
 * order the disassembly lays them out.
 *
 * `calls` / `frames` are measured hits and frames-with-a-hit; `frameSteps` is the
 * number of frame steps that session captured. m1..m7 end with a truncated tail
 * frame (the capture stops right after the 0x0043E1BA anchor), which is why some
 * rows read n-1/n; m8's capture ends after the last site, so it reads n/n.
 */
export const UPDATE_CALLS = [
  { site: 0x0043e288, ret: 0x0043e28d, target: 0x00540650, calls: 603, frames: 603, frameSteps: 603, session: "m8" },
  { site: 0x0043e2e9, ret: 0x0043e2ee, target: 0x00465530, calls: 599, frames: 599, frameSteps: 600, session: "m1" },
  { site: 0x0043e2f5, ret: 0x0043e2fa, target: 0x0043e719, calls: 599, frames: 599, frameSteps: 600, session: "m1" },
  { site: 0x0043e31c, ret: 0x0043e321, target: 0x0047344b, calls: 0, frames: 0, frameSteps: 606, session: "m5", note: "rama no tomada: 0 ejecuciones en 606 frames" },
  { site: 0x0043e328, ret: 0x0043e32d, target: 0x00413d73, calls: 604, frames: 604, frameSteps: 605, session: "m2" },
  { site: 0x0043e36f, ret: 0x0043e374, target: 0x00436f6f, calls: 604, frames: 604, frameSteps: 605, session: "m2" },
  { site: 0x0043e396, ret: 0x0043e39b, target: 0x0053f610, calls: 605, frames: 605, frameSteps: 606, session: "m5" },
  { site: 0x0043e3ac, ret: 0x0043e3b1, target: 0x0053f631, calls: 606, frames: 606, frameSteps: 607, session: "m6" },
  { site: 0x0043e3d5, ret: 0x0043e3da, target: 0x0045eac4, calls: 606, frames: 606, frameSteps: 607, session: "m6" },
  { site: 0x0043e3e6, ret: 0x0043e3eb, target: 0x0045e5c5, calls: 604, frames: 604, frameSteps: 605, session: "m2" },
  { site: 0x0043e3f2, ret: 0x0043e3f7, target: 0x005401cd, calls: 606, frames: 606, frameSteps: 607, session: "m3" },
  { site: 0x0043e418, ret: 0x0043e41d, target: 0x0046face, calls: 606, frames: 606, frameSteps: 607, session: "m4" },
  { site: 0x0043e46b, ret: 0x0043e470, target: 0x00537862, calls: 602, frames: 602, frameSteps: 603, session: "m7" },
  { site: 0x0043e486, ret: 0x0043e48b, target: 0x0041f714, calls: 606, frames: 606, frameSteps: 607, session: "m4" },
  { site: 0x0043e49e, ret: 0x0043e4a3, target: 0x00421668, calls: 599, frames: 599, frameSteps: 600, session: "m1" },
  { site: 0x0043e4aa, ret: 0x0043e4af, target: 0x00460bf7, calls: 606, frames: 606, frameSteps: 607, session: "m3" },
  { site: 0x0043e4c5, ret: 0x0043e4ca, target: 0x00473471, calls: 605, frames: 605, frameSteps: 606, session: "m5" },
  { site: 0x0043e4f0, ret: 0x0043e4f5, target: 0x0045e5c5, calls: 604, frames: 604, frameSteps: 605, session: "m2", note: "segunda llamada en el mismo frame" },
  { site: 0x0043e516, ret: 0x0043e51b, target: 0x00412f8b, calls: 606, frames: 606, frameSteps: 607, session: "m4" },
  { site: 0x0043e525, ret: 0x0043e52a, target: 0x0046dd40, calls: 602, frames: 602, frameSteps: 603, session: "m7" },
  { site: 0x0043e559, ret: 0x0043e55e, target: 0x0046e643, calls: 603, frames: 603, frameSteps: 603, session: "m8" },
  { site: 0x0043e568, ret: 0x0043e56d, target: 0x00405c41, calls: 606, frames: 606, frameSteps: 607, session: "m6" },
  { site: 0x0043e581, ret: 0x0043e586, target: 0x00421e5a, calls: 602, frames: 602, frameSteps: 603, session: "m7" },
  { site: 0x0043e5c2, ret: 0x0043e5c7, target: 0x0043dc3a, calls: 603, frames: 603, frameSteps: 603, session: "m8" },
  { site: 0x0043e5ff, ret: 0x0043e604, target: 0x00540950, calls: 606, frames: 606, frameSteps: 607, session: "m3" },
];

/** Calls inside the render function 0x0043DC3A observed in the same sessions. */
export const RENDER_CALLS = [
  { site: 0x0043de9d, ret: 0x0043dea2, target: 0x0045e5c5, calls: 604, frames: 604, frameSteps: 605, session: "m2" },
  { site: 0x0043e0c0, ret: 0x0043e0c5, target: 0x0054053e, calls: 1104, frames: 1104, frameSteps: 1104, session: "traceH" },
];

/**
 * The movie loop calls the same input function with a different return address:
 * 0x00537C8D inside 0x00537BBB. It ran 242 times before the first gameplay
 * frame in the long session, and 0 times once the level was running.
 */
export const MOVIE_LOOP_CALL = {
  site: 0x00537c88,
  ret: 0x00537c8d,
  target: 0x0043f80d,
  calls: 242,
  note: "bucle de cine 0x00537BBB; no aparece durante el gameplay",
};

/**
 * Mechanical semantics with instruction-level evidence.
 * level: CONFIRMED = the instruction sequence proves the mechanics;
 *        HYPOTHESIS = the mechanics are described but the game purpose is not proven.
 */
export const SEMANTICS = [
  {
    address: 0x00540650,
    size: 96,
    claim: "memset(dst, byte, count): rellena memoria con un byte y devuelve el valor de relleno",
    level: "CONFIRMED",
    evidence: [
      "0x540650: mov edx,[esp+0xC]; mov ecx,[esp+0x4]; test edx,edx; je 0x5406A3",
      "0x54065E: mov al,[esp+0x8]  (valor de relleno)",
      "0x54067B-0x540685: shl/add para replicar el byte en 4 bytes",
      "0x540691: rep stosd; 0x540697: bucle de bytes restantes",
      "0x54069D: mov eax,[esp+0x8]; ret  (devuelve el valor, no dst)",
    ],
    callSite: 0x0043e288,
    perFrame: "1,00/frame",
  },
  {
    address: 0x00540950,
    size: 48,
    claim: "__ftol: convierte un double de la pila FPU a entero de 64 bits (edx:eax) con truncamiento",
    level: "CONFIRMED",
    evidence: [
      "0x540956: wait; fnstcw [ebp-0x2]",
      "0x54095B-0x540966: or ah,0xC; fldcw [ebp-0x4]  (truncamiento)",
      "0x540969: fistp qword ptr [ebp-0xC]",
      "0x54096C: fldcw [ebp-0x2]  (restaura CW)",
      "0x54096F: mov eax,[ebp-0xC]; mov edx,[ebp-0x8]; ret",
    ],
    callSite: 0x0043e5ff,
    perFrame: "1,00/frame desde el update (606/607); 10 623 llamadas en 607 frames sumando todos los llamantes (17,5/frame, sesión m3)",
  },
  {
    address: 0x00465530,
    size: 90,
    claim: "inicializa una lista enlazada sobre un array de nodos de 8 B: +4 = nodo anterior (-1 en el primero), +0 = 0",
    level: "CONFIRMED",
    evidence: [
      "0x46553C: lea edx,[ecx+8*eax-0x8]  (ultimo nodo = base + 8*(count-1))",
      "0x46555C-0x465571: [nodo+4] = nodo-8; [nodo] = 0; nodo -= 8",
      "0x465573-0x465580: [base+4] = -1; [base] = 0",
    ],
    callSite: 0x0043e2e9,
    perFrame: "1,00/frame",
  },
  {
    address: 0x00421668,
    size: 136,
    claim: "recorre 16 cadenas (indices 1,3,...,31 con paso 8 desde +0x10) y llama 0x421283(entrada, arg2) por cada nodo",
    level: "CONFIRMED",
    evidence: [
      "0x4216A1: i = 1; 0x4216AD: i += 2; 0x4216B3: cmp i,0x1F; jg fin",
      "0x4216BF: mov edx,[ecx+8*eax+0x10]  (cadena del indice i)",
      "0x4216CC-0x4216D4: push arg2; push nodo; call 0x421283",
      "0x4216DF: nodo = [nodo+4]  (siguiente de la cadena)",
    ],
    callSite: 0x0043e49e,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0043e719,
    size: 339,
    claim: "maquina de estados por frame sobre el objeto global: estado WORD +0x4E3E (0->4; 6 no hace nada; 7 -> 0x423A6A y vuelve a 0; otro -> 0x43E63D), bits de entrada +0xA0/+0x6FC0/+0x28+4, y +0x4000 de [0x551644]+0x54 con [0x5555DC]+0xF4 en {2,3,4} -> 0x423AFE; fija el bit 0x100 en [obj+0xC]+0x10",
    level: "CONFIRMED",
    evidence: [
      "0x43E728-0x43E72E: edx = [obj+0x8]; guarda en [ebp-0x4]",
      "0x43E734: and ecx,0x40000  ([obj+0xA0])",
      "0x43E747-0x43E752: and eax,0x2  ([obj+0x6FC0])",
      "0x43E757-0x43E762: and edx,0x8000  ([[obj+0x28]+4])",
      "0x43E767-0x43E77E: WORD [obj+0x4E3E] == 0 -> 4",
      "0x43E78A-0x43E7A7: == 7 -> call 0x423A6A (5 B: push ebp/mov ebp,esp/pop ebp/ret) y WORD = 0",
      "0x43E7B3: == 6 -> salta sin llamar",
      "0x43E7BC: otro -> call 0x43E63D",
      "0x43E7C9-0x43E7DA: [0x551644]+0x54 & 0x4000",
      "0x43E7F5-0x43E825: [0x5555DC]+0xF4 en {2,3,4} -> no llama",
      "0x43E839: call 0x423AFE  (621 B; llama 0x53DD93 -> mciSendCommandA y 0x53FCB7)",
      "0x43E85C: or ch,0x1  -> [obj+0xC]+0x10 |= 0x100",
    ],
    callSite: 0x0043e2f5,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0043e63d,
    size: 58,
    claim: "cancelacion del estado de accion: si el estado WORD +0x4E3E es 4 llama 0x422140; despues estado = 0 y limpia el bit 0x100 de [obj+0xC]+0x10",
    level: "CONFIRMED",
    evidence: [
      "0x43E643-0x43E64D: movsx ecx,[obj+0x4E3E]; cmp ecx,4; jne 0x43E654",
      "0x43E64F: call 0x422140",
      "0x43E657-0x43E660: WORD [obj+0x4E3E] = 0",
      "0x43E669: and dh,-0x2  (limpia 0x100)",
    ],
    callSite: null,
    perFrame: "0/frame: no ejecutada en 611 frames de gameplay (m9)",
  },
  {
    address: 0x0047344b,
    size: 17,
    claim: "forwarder de 17 B: push [ebp+8]; call 0x452368; ret",
    level: "CONFIRMED",
    evidence: [
      "0x47344E-0x473452: mov eax,[ebp+0x8]; push eax; call 0x452368",
      "0x47345A-0x47345B: pop ebp; ret  (cuerpo completo, 17 B)",
    ],
    callSite: 0x0043e31c,
    perFrame: "0/frame: el sitio 0x43E31C no se ejecuto en 606 frames (m5)",
  },
  {
    address: 0x00473471,
    size: 17,
    claim: "forwarder de 17 B: push [ebp+8]; call 0x455F1A; ret",
    level: "CONFIRMED",
    evidence: [
      "0x473474-0x473478: mov eax,[ebp+0x8]; push eax; call 0x455F1A",
      "0x473480-0x473481: pop ebp; ret  (cuerpo completo, 17 B)",
    ],
    callSite: 0x0043e4c5,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0053f610,
    size: 33,
    claim: "acumulador en coma fija 24.8: [0x5AC610] += arg; [0x5AC614] = [0x5AC610] >> 8 (parte entera). Llamado cada frame con arg = 0x100, es decir +1,0 por frame",
    level: "CONFIRMED",
    evidence: [
      "0x53F613-0x53F61B: eax = [0x5AC610]; eax += [ebp+0x8]; [0x5AC610] = eax",
      "0x53F620-0x53F629: ecx = [0x5AC610]; sar ecx,8; [0x5AC614] = ecx",
      "0x43E391-0x43E396: push 0x100; call 0x53F610  (constante 256 = 1,0 en 8 bits fraccionarios)",
    ],
    callSite: 0x0043e396,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0053f631,
    size: 1375,
    claim: "recorre un array de registros de 0x2C B desde [arg+0xC] hasta [arg+0xC]+0x2C*[arg+8]; salta los que tienen [rec]&2; incrementa [rec+4]; llama 0x40223B(rec+8, arg2); si devuelve != 0 pone [rec]|=1; y calcula un indice de frame como ((WORD [rec+0xA] + [0x5AC614]) % [[arg+0xC]+0x14]) * 6",
    level: "CONFIRMED",
    evidence: [
      "0x53F644-0x53F652: fin = [arg+0xC] + 0x2C*[arg+8]; puntero = [arg+0xC]",
      "0x53F65A-0x53F666: avanza de 0x2C en 0x2C mientras < fin",
      "0x53F66F-0x53F676: if ([rec] & 2) -> siguiente",
      "0x53F67F-0x53F688: [rec+4] += 1",
      "0x53F68B-0x53F6A0: call 0x40223B(rec+8, arg2); if 0 -> siguiente",
      "0x53F6A9-0x53F6B1: [rec] |= 1",
      "0x53F8B9-0x53F8C7: eax = movsx WORD [rec+0xA]; eax += [0x5AC614]; idiv [[arg+0xC]+0x14]; edx = resto",
      "0x53F8D7: imul ecx,edx,6  (6 B por frame de animacion)",
    ],
    callSite: 0x0043e3ac,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0045e5c5,
    size: 186,
    claim: "dos pasadas sobre la misma lista enlazada ([arg+4], siguiente en [nodo+8]): en la primera, si [nodo+0x10]&0x400 limpia el bit 0x20, y si no lo tiene y [nodo+0x10]&0x20 llama 0x45E480(arg, nodo, arg2); en la segunda llama 0x45E265(arg, nodo, arg2) por cada nodo con [nodo+0x10]&0x20",
    level: "CONFIRMED",
    evidence: [
      "0x45E5CE-0x45E5D4: nodo = [arg+4]; if 0 -> segunda pasada",
      "0x45E5E3-0x45E5F1: [nodo+0x10] & 0x400 -> 0x45E616",
      "0x45E5F3-0x45E60C: si no, [nodo+0x10]&0x20 -> call 0x45E480(arg, nodo, arg2)",
      "0x45E619-0x45E62F: si 0x400 y 0x20 -> [nodo+0x10] &= ~0x20",
      "0x45E632-0x45E638: nodo = [nodo+8]; bucle",
      "0x45E63A-0x45E66B: segunda pasada desde [arg+4]; si [nodo+0x10]&0x20 -> call 0x45E265(arg, nodo, arg2)",
    ],
    callSite: 0x0043e3e6,
    perFrame: "2,00/frame (dos sitios: 0x43E3E6 y 0x43E4F0) + 1 desde el render (0x43DE9D)",
  },
  {
    address: 0x00460bf7,
    size: 665,
    claim: "prueba de banderas por nodo de la lista [arg+0x18]+4 (0x40, 0x200, 0x8000000, 0x10000000, +0xFC&0x1000, comparacion con [0x5555DC] y [+0xF4]==5) y copia tres WORD del nodo: desde [nodo+0x2C]+0x34/0x38/0x3C si tiene 0x10000000, o desde [nodo+0x48] en caso contrario",
    level: "CONFIRMED",
    evidence: [
      "0x460C00-0x460C13: nodo = [[arg+0x18]+4]; if 0 -> fin",
      "0x460C19-0x460C53: [nodo+0x14]&0x40, [nodo+0x10]&0x200, 0x8000000, 0x10000000",
      "0x460C58-0x460C87: nodo != [0x5555DC] o [+0xF4] != 5 o [nodo+0xFC]&0x1000 -> 0x460E6D",
      "0x460C9D-0x460CDB: [nodo+0x10]&0x10000000 -> tres WORD de [nodo+0x2C]+0x34/0x38/0x3C",
      "0x460CE1-0x460CFF: si no -> tres WORD de [nodo+0x48]",
    ],
    callSite: 0x0043e4aa,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0041f714,
    size: 456,
    claim: "recorrido por pares de cadenas de nodos: (a) si WORD [[0x551644]+0x6F36] != 1, recorre las 16 cadenas de indices 0x10..0x1F; (b) por cada i de 0..7 toma idx = tabla[0x54D4E0+4*i] y empareja su cadena con las de tabla[0x54D4E0+4*(i+1)..]; (c) repite el emparejamiento con las tablas 0x54D520 y 0x54D500. Cada par se pasa a 0x41F5C1",
    level: "CONFIRMED",
    evidence: [
      "0x41F72B-0x41F735: movsx eax,WORD [[0x551644]+0x6F36]; cmp 1; je 0x41F77F",
      "0x41F737-0x41F74D: i = 0x10; i <= 0x1F; i++",
      "0x41F755: nodo = [arg + 8*i + 0x10]; 0x41F76A: call 0x41F5C1(nodo, ctx); 0x41F775: nodo = [nodo+4]",
      "0x41F79E: idx = [4*i + 0x54D4E0]; 0x41F7A8: cadena = [arg + 8*idx + 0x10]",
      "0x41F7E9-0x41F7FB: segundo indice j = i+1..7 sobre la misma tabla 0x54D4E0",
      "0x41F800-0x41F81F: por cada par de nodos de ambas cadenas -> call 0x41F5C1",
      "0x41F862: tabla 0x54D520; 0x41F894: tabla 0x54D500 (tercera pasada de pares)",
    ],
    callSite: 0x0043e486,
    perFrame: "1,00/frame",
  },
  {
    address: 0x00436f6f,
    size: 317,
    claim: "tres recorridos de listas del objeto: (1) sobre [arg+0x2C04] propaga el bit 0x4 de [nodo+8] al bit 0x1 y limpia el 0x2; (2) sobre [arg+0x10C14] pone [[nodo+0x14]+8] |= 1 y llama al puntero [nodo+0x10] con (nodo, arg); (3) sobre [arg+0x2C04] llama 0x461A36(nodo) y 0x461A00(arg+0x2C08, nodo) por cada nodo sin el bit 0x1",
    level: "CONFIRMED",
    evidence: [
      "0x436F8D-0x436FEA: nodo = [arg+0x2C04]; [nodo+8]&4 -> |=1, si no &=~1; despues [nodo+8] &= ~2; nodo = [nodo+4]",
      "0x436FEF-0x437045: nodo = [arg+0x10C14]; [[nodo+0x14]+8] |= 1; if [nodo+0x10] call [nodo+0x10](nodo, arg); nodo = [nodo+4]",
      "0x43704A-0x43709F: nodo = [arg+0x2C04]; if ([nodo+8]&1)==0 -> call 0x461A36(nodo); call 0x461A00(arg+0x2C08, nodo)",
    ],
    callSite: 0x0043e36f,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0046face,
    size: 840,
    claim: "asigna entradas de 0x20 B de un pool ([arg+0x4C10] cursor, [arg+0x4C14] limite), rellena 8 WORD con 0x1000 (1,0 en 12 bits), calcula un angulo con dos llamadas a 0x463E40 (tabla de seno/coseno: +0x400 y +0xC00), aproxima una distancia con (30*max + 12*min) >> 5 y enlaza la entrada en [arg+0x2C]",
    level: "CONFIRMED",
    evidence: [
      "0x46FAD4-0x46FAE2: punteros globales 0x647680, 0x647688, 0x647698",
      "0x46FB04-0x46FB28: copia tres WORD de [arg+8] a 0x647680/0x647682/0x647684",
      "0x46FB55-0x46FB72: eax = [nodo+0x18] << 5; if ([arg+0x4C10] + eax) > [arg+0x4C14] -> [nodo+0x2C] = 0",
      "0x46FBA2-0x46FBB1: entrada = [arg+0x4C10]; [arg+0x4C10] += 0x20",
      "0x46FCC4-0x46FD06: ocho WORD a 0x1000/0x0 (matriz 4x2)",
      "0x46FC46-0x46FC5E: intercambio max/min por XOR",
      "0x46FC61-0x46FC72: (min*0x1E + max*0xC) >> 5  (aproximacion de distancia)",
      "0x46FC82/0x46FCA5: call 0x463E40(..., ...) + 0x400 / + 0xC00  (dos componentes de angulo)",
      "0x46FD63-0x46FD75: [nodo+0x2C] = entrada; entrada->[0x30] = anterior",
    ],
    callSite: 0x0043e418,
    perFrame: "1,00/frame",
  },
  {
    address: 0x00421e5a,
    size: 448,
    claim: "elige un bloque de transformacion (arg+0x28, arg+0x50 o el global 0x64FE80) segun [0x551644]+0x4C94&0x10 y [0x551644]+0xA0&0x40000, y despacha por el estado WORD +0x4E3E a 0x42276C, 0x422740, 0x42216E, 0x424191 y 0x423A6F",
    level: "CONFIRMED",
    evidence: [
      "0x421E6C-0x421E7D: [0x551644]+0x4C94 & 0x10",
      "0x421E8A-0x421EA5: [0x551644]+0xA0 & 0x40000 -> arg+0x50; si no -> global 0x64FE80; si no habia 0x10 -> arg+0x28",
      "0x421EB4-0x421EBD: movsx WORD [arg+0x4E3E]; cmp 0",
      "0x421EC2-0x421ED3: [arg+0xA0]&8 -> call 0x42276C(arg)",
      "0x421EDE-0x421EEE: WORD [arg+0x6F36]==1 -> call 0x422740(arg)",
      "0x421F12: call 0x42216E; 0x421FB7: call 0x424191; 0x421FD4: call 0x423A6F",
    ],
    callSite: 0x0043e581,
    perFrame: "1,00/frame",
  },
  {
    address: 0x0046e643,
    size: 492,
    claim: "copia WORD de un registro temporal: si WORD [arg2+0x12]==1 llama 0x46E898(&tmp, arg2) y escribe tmp[0]->[arg1], tmp[2]->[arg1+6]; otra rama llama 0x46999C",
    level: "CONFIRMED",
    evidence: [
      "0x46E64C-0x46E653: movsx ecx,WORD [arg2+0x12]; cmp 1; jne 0x46E6C9",
      "0x46E65D: call 0x46E898(&tmp, arg2)",
      "0x46E668-0x46E676: [arg1] = tmp[0]; [arg1+6] = tmp[2]",
      "0x46E803: call 0x46999C",
    ],
    callSite: 0x0043e559,
    perFrame: "1,00/frame",
  },
  {
    address: 0x00413d73,
    size: 181,
    claim: "si [arg+0x50] tiene 0x4000 llama 0x537B14; si tiene 0xF0 llama 0x537B65 y despues 0x53E98C(handle, bloque+0x10) para los dos handles [obj+0x480]/[obj+0x484]",
    level: "HYPOTHESIS",
    evidence: [
      "0x413D80-0x413D9A: [obj+0x438] != 0 y [obj+0x14C] == 5, si no ret",
      "0x413DA6-0x413DB0: [arg+0x50] & 0x4000 -> call 0x537B14",
      "0x413DBB-0x413DC5: [arg+0x50] & 0xF0 -> call 0x537B65",
      "0x413DD6-0x413DEF: call 0x53E98C([obj+0x480], [obj+0x480]+0x10)",
      "0x413E03-0x413E1C: call 0x53E98C([obj+0x484], [obj+0x484]+0x10)",
    ],
    callSite: 0x0043e328,
    perFrame: "1,00/frame",
  },
  {
    address: 0x005401cd,
    size: 251,
    claim: "dispara un id de la tabla WORD 0x55A340 indexada por el byte +0x6F71 (si < 0x1E y != -1) y otro por cada nodo de la lista [arg+0x18]+4 con [nodo+0x10]&0x200; llama 0x540036(id) -> 0x53FE80 (ruta de voz)",
    level: "HYPOTHESIS",
    evidence: [
      "0x5401E5-0x5401F1: WORD [arg+0x4E3E] != 0 -> ret",
      "0x5401F7-0x540203: WORD [arg+0x4E3A] != 0 -> ret",
      "0x540209: call 0x54038E; si 0 -> ret",
      "0x54021B-0x540239: b = [arg+0x6F71]; si b >= 0x1E ret; id = movsx WORD [2*b+0x55A340]; si id == -1 ret",
      "0x540251: call 0x540036(id)",
      "0x540260-0x5402B1: recorre nodos ([nodo+8]) y llama 0x540036(WORD [[nodo+0x18]+6]) si [nodo+0x10]&0x200",
      "0x540036 -> 0x53FE80 (141 B, ruta %s\\audio\\voiceuk\\%s.sag) y 0x54038E",
    ],
    callSite: 0x0043e3f2,
    perFrame: "1,00/frame",
  },
];

const hex = (value) => `0x${value.toString(16).padStart(8, "0")}`;

/**
 * Re-checks every structural claim in this module against the committed map.
 * Returns a list of problems; empty means consistent.
 */
export function validateFrame(model) {
  const problems = [];
  const byStart = new Map(model.functions.map((fn) => [fn.start, fn]));
  const edges = new Set(model.edges.filter((e) => e.kind === "call").map((e) => `${e.from}:${e.to}`));

  const inside = (site, fn) => site >= fn.start && site < fn.start + fn.size;

  const checkCalls = (owner, calls, label) => {
    const fn = byStart.get(owner);
    if (!fn) { problems.push(`${label}: ${hex(owner)} no existe en el mapa`); return; }
    if (fn.kind !== "func") problems.push(`${label}: ${hex(owner)} no es una funcion (${fn.kind})`);
    for (const call of calls) {
      if (!inside(call.site, fn)) {
        problems.push(`${label}: el sitio ${hex(call.site)} cae fuera de ${hex(owner)}+${fn.size}`);
      }
      if (!byStart.has(call.target)) { problems.push(`${label}: destino ${hex(call.target)} no existe`); continue; }
      if (!edges.has(`${owner}:${call.target}`)) {
        problems.push(`${label}: falta la arista ${hex(owner)} -> ${hex(call.target)}`);
      }
      if (call.ret != null && call.site + 5 !== call.ret) {
        problems.push(`${label}: ret ${hex(call.ret)} no es site+5 (${hex(call.site + 5)})`);
      }
      if (call.frames != null && call.frames > call.frameSteps) {
        problems.push(`${label}: ${hex(call.site)} tiene mas frames (${call.frames}) que la sesion (${call.frameSteps})`);
      }
      if (call.calls != null && call.frames != null && call.calls > 0 && call.frames === 0) {
        problems.push(`${label}: ${hex(call.site)} tiene llamadas sin frames`);
      }
    }
  };

  if (byStart.get(FRAME_STEP.address)?.size !== FRAME_STEP.size) {
    problems.push(`frame step: tamano ${byStart.get(FRAME_STEP.address)?.size} != ${FRAME_STEP.size}`);
  }
  checkCalls(FRAME_STEP.address, FRAME_STEP.calls, "frame step");
  checkCalls(0x0043e1ba, UPDATE_CALLS, "update");
  checkCalls(0x0043dc3a, RENDER_CALLS, "render");
  checkCalls(0x00537bbb, [MOVIE_LOOP_CALL], "bucle de cine");

  const seen = new Set();
  for (const s of SEMANTICS) {
    if (seen.has(s.address)) problems.push(`semantica duplicada: ${hex(s.address)}`);
    seen.add(s.address);
    const fn = byStart.get(s.address);
    if (!fn) { problems.push(`semantica: ${hex(s.address)} no existe`); continue; }
    if (fn.size !== s.size) problems.push(`semantica: ${hex(s.address)} tamano ${fn.size} != ${s.size}`);
    if (!s.claim || s.claim.length < 10) problems.push(`semantica: ${hex(s.address)} sin afirmacion`);
    if (!Array.isArray(s.evidence) || s.evidence.length === 0) {
      problems.push(`semantica: ${hex(s.address)} sin evidencia`);
    }
    if (s.level !== "CONFIRMED" && s.level !== "HYPOTHESIS") {
      problems.push(`semantica: ${hex(s.address)} nivel invalido ${s.level}`);
    }
    if (s.level === "CONFIRMED" && s.evidence.length < 2) {
      problems.push(`semantica: ${hex(s.address)} CONFIRMED con evidencia insuficiente`);
    }
    if (s.callSite != null) {
      const owner = byStart.get(0x0043e1ba);
      if (!owner || !inside(s.callSite, owner)) {
        problems.push(`semantica: el sitio ${hex(s.callSite)} no cae dentro del update`);
      }
      if (!edges.has(`${0x0043e1ba}:${s.address}`) && s.address !== 0x0043e63d) {
        problems.push(`semantica: falta la arista 0x0043e1ba -> ${hex(s.address)}`);
      }
    }
  }
  return problems;
}
