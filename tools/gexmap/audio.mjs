/**
 * audio.mjs - the two DirectSound initialisation routes of GEX3D.EXE (EU) as
 * declarative, checkable data.
 *
 * Scope. This module records *what the code does*, not why. It answers three
 * questions that the published map alone cannot:
 *
 *   1. Who creates the DirectSound device, and when is the second creation site
 *      reachable? 0x00538CB0 has exactly one caller (0x00538FD5) and 0x00538FD5
 *      has no caller at all; 0x00538551 has two callers (0x00538731, 0x0053B10B)
 *      and both are reachable from the entry point by direct calls.
 *   2. What does each route build? 0x00538551 creates the device and the primary
 *      buffer; 0x00538731 creates the single streaming buffer and starts it;
 *      0x0053B10B wraps 0x00538551 with the mixer state.
 *   3. How do the two chains differ? They share the DirectSoundCreate thunk and
 *      the global at 0x0057C330, and nothing else.
 *
 * Evidence layers, kept apart on purpose:
 *
 *   CONFIRMED  a mechanical statement that the disassembly shows directly: the
 *              instruction, the vtable slot, the global written, the branch.
 *   HYPOTHESIS a reading of purpose (what the buffer/volume/path is *for*), or a
 *              claim that depends on data this module cannot see (the bytes of
 *              the WAVEFORMATEX at 0x005582E0, the text of the format strings).
 *
 * Dynamic status. No route below was instrumented: the reference image
 * (1 557 504 B, MD5 692b12825003417cc4a5a9d4db13eebe) was no longer on the
 * analysis machine when this file was written, and the only available build is a
 * different binary (1 559 040 B, MD5 3198350eb398db9a64771842d7781b4b, entry
 * 0x00541FA0 instead of 0x00541B20). A trace of that build would not be evidence
 * about the reference image. Every route here is therefore STATIC existence plus
 * a predicted dynamic behaviour that has NOT been observed (see LIMITS).
 */

export const ENTRY = 0x00541b20;
export const WINMAIN = 0x00463278;
export const DS_DEVICE_INIT = 0x00538551;

/**
 * DirectSound / mixer globals touched by these routes. `writer` is the function
 * that stores into the global (the only writer found in the disassembly).
 */
export const DSOUND_GLOBALS = [
  {
    address: 0x0057c384,
    name: "g_pDS",
    type: "IDirectSound*",
    writer: 0x00538551,
    note: "dispositivo vivo; 0 = no inicializado. Guarda de reentrada de 0x00538551",
  },
  {
    address: 0x0057c378,
    name: "g_pPrimary",
    type: "IDirectSoundBuffer*",
    writer: 0x00538551,
    note: "buffer primario; solo se toca en la creacion, el SetFormat y la liberacion",
  },
  {
    address: 0x0057c374,
    name: "g_pStream",
    type: "IDirectSoundBuffer*",
    writer: 0x00538731,
    note: "unico buffer secundario de reproduccion; se libera y recrea en cada 0x00538731",
  },
  {
    address: 0x0057c330,
    name: "g_dsbd",
    type: "DSBUFFERDESC (0x14 B)",
    writer: 0x00538551,
    note: "misma estructura reutilizada por las dos funciones: +0 dwSize, +4 dwFlags, +8 dwBufferBytes, +0xC dwReserved, +0x10 lpwfxFormat",
  },
  {
    address: 0x0057c348,
    name: "g_wfxStream",
    type: "WAVEFORMATEX (0x12 B)",
    writer: 0x0053899c,
    note: "formato leido del fichero de audio; es el lpwfxFormat del buffer secundario",
  },
  {
    address: 0x005582e0,
    name: "g_wfxPrimary",
    type: "WAVEFORMATEX (0x12 B, solo lectura en el codigo)",
    writer: null,
    note: "formato del buffer primario; sus 18 bytes no se pueden leer sin la imagen original",
  },
  {
    address: 0x005582f4,
    name: "g_musicVolume",
    type: "int",
    writer: 0x00538724,
    note: "escala de volumen; 0x00538731 aplica [0x5582F4]*0x1E-0xBB8 a SetVolume (0x3C)",
  },
  {
    address: 0x005ac5c8,
    name: "g_pDSInit",
    type: "IDirectSound*",
    writer: 0x0053b10b,
    note: "copia del dispositivo que 0x0053B10B usa como bandera de 'dispositivo ya creado'",
  },
  {
    address: 0x005ac5f0,
    name: "g_soundReady",
    type: "int",
    writer: 0x0053b10b,
    note: "1 = sistema de sonido inicializado; 0x0053B10B lo pone a 0 al reentrar",
  },
  {
    address: 0x005847e4,
    name: "g_pDSAlt",
    type: "IDirectSound*",
    writer: 0x00538cb0,
    note: "dispositivo de la ruta muerta; ningun otro codigo lo lee",
  },
  {
    address: 0x0057c39c,
    name: "g_pDSAltAlias",
    type: "IDirectSound*",
    writer: 0x00538cb0,
    note: "alias de 0x005847E4 usado por 0x00538D1B y 0x00538FF8",
  },
  {
    address: 0x005847e0,
    name: "g_altReady",
    type: "byte",
    writer: 0x00538cb0,
    note: "bandera de la ruta muerta; se pone a 1 al final",
  },
  {
    address: 0x0065d7c8,
    name: "g_hwndCached",
    type: "HWND",
    writer: 0x00401031,
    note: "copia de 0x006488F8 hecha una sola vez en 0x401042; es el HWND que recibe 0x00538551 desde 0x0053B10B",
  },
  {
    address: 0x006488f8,
    name: "g_hwndMain",
    type: "HWND",
    writer: 0x00463278,
    note: "handle que WinMain guarda en 0x463153; 0x00537E9C lo devuelve y es el HWND de la ruta 0x00538731",
  },
];

/**
 * Mechanical reading of each function in the two chains. `role` is a one-line
 * label (HYPOTHESIS); `claim` is what the instructions do (CONFIRMED unless the
 * level says otherwise); `evidence` lines always carry an address.
 */
export const AUDIO_SEMANTICS = [
  {
    address: 0x00538551,
    size: 467,
    role: "creacion del dispositivo DirectSound y del buffer primario",
    claim:
      "si [0x57C384] != 0 retorna 0 (guarda de reentrada); si no, DirectSoundCreate(NULL,&[0x57C384],NULL), SetCooperativeLevel(vtable+0x18, hwnd, 3), rellena DSBUFFERDESC en 0x57C330 (dwSize=0x14, dwFlags=1=PRIMARYBUFFER, dwBufferBytes=0, lpwfxFormat=0), CreateSoundBuffer(vtable+0xC)->[0x57C378] y SetFormat(vtable+0x38) con 0x5582E0; cualquier error libera lo creado, pone los punteros a 0 y retorna -1",
    level: "CONFIRMED",
    evidence: [
      "0x538557-0x53855E: cmp dword [0x57C384],0 ; jne 0x53871E (retorno 0)",
      "0x538564-0x538579: push 0; push 0x57C384; push 0; call 0x5404F0 (DSOUND.dll!DirectSoundCreate); if eax!=0 -> -1",
      "0x53857E-0x538597: push 3; push [ebp+8]; call [edx+0x18] (SetCooperativeLevel)",
      "0x5385BC-0x5385DA: [0x57C330]=0x14; [0x57C334]=1; [0x57C338]=0; [0x57C340]=0",
      "0x5385E4-0x538608: push 0; push 0x57C378; push 0x57C330; call [edx+0xC] (CreateSoundBuffer); if eax!=0 -> error",
      "0x538614-0x5386A6: switch de 7 HRESULT (0x80040110,0x80004001,0x80070057,0x8007000E,0x8878000A,0x88780064,0x887800AA) con todos los cuerpos vacios",
      "0x5386C9-0x5386E1: push 0x5582E0; call [edx+0x38] (SetFormat del buffer primario)",
      "0x5386E3-0x53871C: ante error: Release(+0x8) del primario y del dispositivo, punteros a 0, retorno -1",
    ],
  },
  {
    address: 0x00538731,
    size: 379,
    role: "creacion y arranque del unico buffer secundario de streaming",
    claim:
      "llama 0x538551(hwnd) y aborta con -1 si falla; si existe [0x57C374] lo para (vtable+0x48) y lo libera (+0x8); limpia el fichero cargado (0x5384ED/0x5384A3); 0x5388AC(arg2,arg3) carga el fichero entero en memoria y 0x53899C(0x57C348,0x12) rellena el WAVEFORMATEX; rellena DSBUFFERDESC (dwFlags=0xE0, dwBufferBytes=0x8000, lpwfxFormat=0x57C348) y CreateSoundBuffer(+0xC)->[0x57C374]; Play(+0x30,0,0,1) y SetVolume(+0x3C, [0x5582F4]*0x1E-0xBB8); retorna 0",
    level: "CONFIRMED",
    evidence: [
      "0x538735-0x538744: push [ebp+8]; call 0x538551; if eax==-1 -> retorno -1",
      "0x53874E-0x538779: if [0x57C374]!=0: call [edx+0x48] (Stop); call [edx+0x8] (Release); [0x57C374]=0",
      "0x538783-0x53878C: call 0x5384ED; if !=0 call 0x5384A3",
      "0x538791-0x5387A3: push [ebp+0x10]; push [ebp+0xC]; call 0x5388AC; if 0 -> -1",
      "0x5387AD-0x5387BC: push 0x12; push 0x57C348; call 0x53899C (parser de cabecera); if -1 -> -1",
      "0x5387C9-0x5387E7: [0x57C340]=0x57C348; [0x57C330]=0x14; [0x57C334]=0xE0; [0x57C338]=0x8000",
      "0x5387F1-0x538815: push 0; push 0x57C374; push 0x57C330; call [ecx+0xC] (CreateSoundBuffer); if !=0 -> -1",
      "0x53881A-0x538838: [0x57C360]=0; [0x57C35C]=0; [0x57C37C]=0; [0x57C380]=0",
      "0x538842-0x538847: call 0x5380B3 (mezclador); call 0x538040 (Restore si hace falta)",
      "0x53884C-0x538865: push 1; push 0; push 0; call [ecx+0x30] (Play); if !=0 -> -1",
      "0x53886C-0x5388A6: eax=[0x5582F4]*0x1E-0xBB8; clamp muerto (-10000..0); call [ecx+0x3C] (SetVolume); retorno 0",
    ],
  },
  {
    address: 0x0053b10b,
    size: 400,
    role: "envoltorio de inicializacion del sistema de sonido",
    claim:
      "si [0x5AC5C8]==0 llama 0x538551([0x65D7C8]) y copia el dispositivo a [0x5AC5C8]; si [0x5AC5F0]!=0 libera los buffers de los arrays 0x5AC5CC/0x5AC5D0 y llama 0x53A81D; memset(0x5867C8,0,0x25E00) y memset(0x585810,0,0xFA0); 0x53B3CC(arg), 0x53B29B(0x20,..) y 0x53A8A2(arg,0,0,..); pone [0x5AC5F0]=1 y retorna 1 (0 si cualquier paso falla)",
    level: "CONFIRMED",
    evidence: [
      "0x53B111-0x53B13A: if [0x5AC5C8]==0: push [0x65D7C8]; call 0x538551; if -1 -> 0; [0x5AC5C8]=[0x57C384]",
      "0x53B140-0x53B14D: if [0x5AC5F0]!=0: call 0x53A81D",
      "0x53B15B-0x53B1AA: recorre [0x5AC5CC] con [0x5AC5D8] entradas y libera cada puntero no nulo (+0x8)",
      "0x53B1BD-0x53B20B: recorre [0x5AC5D0] con [0x5AC5D4] entradas y libera cada puntero no nulo (+0x8)",
      "0x53B215-0x53B23A: memset(0x5867C8,0,0x25E00); memset(0x585810,0,0xFA0)",
      "0x53B23D-0x53B25C: 0x53B3CC(arg); if 0 -> 0; 0x53B29B(0x20,resultado); if 0 -> 0",
      "0x53B26C-0x53B286: 0x53A8A2(arg,0,0,resultado); if 0 -> 0",
      "0x53B288-0x53B292: [0x5AC5F0]=1; retorno 1",
    ],
  },
  {
    address: 0x0053d822,
    size: 320,
    role: "frontera audio/cine: inicializa sonido y ajusta el volumen auxiliar/MCI",
    claim:
      "llama 0x53B10B(arg) y escribe [obj+0x4E97]=0xFF si falla o 0 si funciona; despues usa auxGetVolume y mciSendCommandA sobre el objeto global [0x551644]",
    level: "CONFIRMED",
    evidence: [
      "0x53D82B-0x53D839: push [ebp+8]; call 0x53B10B; test eax",
      "0x53D83B-0x53D85C: [obj+0x4E97]=0xFF (fallo) o 0 (exito); [obj+0x4E94]=0",
      "0x53D863-0x53D8AE: ramas sobre [obj+0x4E95], [obj+0x4EA0] y [obj+0x4E98]",
    ],
  },
  {
    address: 0x005380b3,
    size: 466,
    role: "mezclador: rellena el buffer secundario con Lock/Unlock",
    claim:
      "consulta GetCurrentPosition(vtable+0x10), bloquea 0x4000 B con Lock(+0x2C), mezcla en la region bloqueada y la libera con Unlock(+0x4C); mantiene el cursor en [0x57C37C] y la bandera [0x57C380]",
    level: "CONFIRMED",
    evidence: [
      "0x5380DC-0x538101: call 0x538040; call 0x5380A6; call [eax+0x10] (GetCurrentPosition)",
      "0x538141-0x53815A: push 0x4000; push [0x57C37C]; push [0x57C374]; call [edx+0x2C] (Lock)",
      "0x538166-0x538189: call 0x5384ED; avanza [0x57C37C]",
      "0x53819D-0x5381C5: bloque de 0x1000 B -> 0x538409 y 0x538285",
      "0x538207-0x53821A: memset de 0x4000 B; [0x57C380]=1",
      "0x538251-0x53827E: call [eax+0x48] (Stop) y call [edx+0x4C] (Unlock)",
    ],
  },
  {
    address: 0x00538040,
    size: 40,
    role: "restaura el buffer si el estado lo pide",
    claim: "si [0x57C374]!=0 y 0x538068()!=0 llama Restore(vtable+0x50)",
    level: "CONFIRMED",
    evidence: [
      "0x538043-0x53804A: cmp dword [0x57C374],0; je fin",
      "0x53804C-0x538053: call 0x538068; if 0 -> fin",
      "0x538055-0x538063: push [0x57C374]; call [ecx+0x50] (Restore)",
    ],
  },
  {
    address: 0x00538068,
    size: 13,
    role: "consulta de estado del buffer (bit 0x2 = DSBSTATUS_LOOPING)",
    claim: "devuelve 0x538075() & 0x2",
    level: "CONFIRMED",
    evidence: [
      "0x53806B-0x538070: call 0x538075; and eax,2; ret",
      "0x538094-0x53809B: dentro de 0x538075, call [edx+0x24] (GetStatus) y devuelve el estado",
    ],
  },
  {
    address: 0x005380a6,
    size: 13,
    role: "consulta de estado del buffer (bit 0x1 = DSBSTATUS_PLAYING)",
    claim: "devuelve 0x538075() & 0x1",
    level: "CONFIRMED",
    evidence: [
      "0x5380A9-0x5380AE: call 0x538075; and eax,1; ret",
      "0x538094-0x538099: 0x538075 devuelve 0 si [0x57C374]==0 o si GetStatus falla",
    ],
  },
  {
    address: 0x0053851a,
    size: 55,
    role: "parada del buffer secundario",
    claim: "si [0x57C384]!=0, [0x57C374]!=0 y 0x5380A6()!=0 llama Stop(vtable+0x48)",
    level: "CONFIRMED",
    evidence: [
      "0x53851D-0x53852F: comprueba [0x57C384] y [0x57C374]",
      "0x538533-0x53853A: call 0x5380A6 (GetStatus & 1)",
      "0x53853E-0x53854C: push [0x57C374]; call [ecx+0x48] (Stop)",
    ],
  },
  {
    address: 0x00538724,
    size: 13,
    role: "fija el volumen de musica",
    claim: "escribe el argumento en [0x5582F4]",
    level: "CONFIRMED",
    evidence: [
      "0x538727-0x53872A: mov eax,[ebp+8]; mov [0x5582F4],eax; ret",
      "0x54000B-0x540010: 0x53FF0D llama 0x538724(valor/0x7F) antes de reproducir la voz",
    ],
  },
  {
    address: 0x005388ac,
    size: 240,
    role: "carga un fichero entero en memoria (secuencia CRT)",
    claim:
      "abre el fichero con 0x541060(ruta,0x5582F8), mide con 0x540F90/0x540DE0, reserva con 0x5416E0, lee con 0x540CA0 y cierra con 0x540C20; guarda fichero/tamano/cursor en [0x57C364]/[0x57C36C]/[0x57C370]; retorna 1 si todo fue bien",
    level: "HYPOTHESIS",
    evidence: [
      "0x5388B4-0x5388C5: push 0x5582F8; push [ebp+8]; call 0x541060; [0x57C364]=eax",
      "0x5388E4-0x538906: 0x540F90(fichero,0,2); 0x540DE0(fichero); [0x57C36C]=eax",
      "0x53890B-0x53892C: 0x540F90(fichero,0,0); 0x5416E0(tamano); [0x57C368]=eax",
      "0x538943-0x538961: 0x540CA0(buffer,1,tamano,fichero); compara con el tamano",
      "0x538972-0x53898B: 0x540C20(fichero); [0x57C370]=0; [0x57C364]=0",
    ],
  },
  {
    address: 0x0053899c,
    size: 253,
    role: "parser de cabecera de audio (tags SLAG/WAVE/fmt /data)",
    claim:
      "lee dwords con 0x538B01: exige 0x47414C53 (\"SLAG\"), luego -1 distinto, luego 0x45564157 (\"WAVE\"); recorre chunks, al tag 0x20746D66 (\"fmt \") copia min(tamano,arg2) bytes al destino tras memset, y al tag 0x61746164 (\"data\") devuelve el desplazamiento",
    level: "CONFIRMED",
    evidence: [
      "0x5389A2-0x5389D2: cmp eax,0x47414C53 (\"SLAG\"); cmp eax,0x45564157 (\"WAVE\")",
      "0x538A1D-0x538A24: cmp dword [ebp-0x4],0x20746D66 (\"fmt \")",
      "0x538A2A-0x538A35: memset(destino,0,arg2); copia min(tamano,arg2) con 0x538409",
      "0x538A70-0x538A7C: cmp dword [ebp-0x4],0x61746164 (\"data\"); devuelve [ebp-0xC]",
      "0x538A7E-0x538A8D: avanza al siguiente chunk con 0x538ACA y repite",
    ],
  },
  {
    address: 0x005384a3,
    size: 74,
    role: "cierra el fichero cargado o libera el buffer",
    claim: "si [0x57C364]!=0 llama 0x540C20 y lo pone a 0; si no y [0x57C368]!=0 llama 0x541950 (HeapFree) y lo pone a 0",
    level: "CONFIRMED",
    evidence: [
      "0x5384A6-0x5384BD: if [0x57C364]!=0: 0x540C20([0x57C364]); [0x57C364]=0",
      "0x5384C9-0x5384E1: si no, if [0x57C368]!=0: 0x541950([0x57C368]); [0x57C368]=0",
    ],
  },
  {
    address: 0x005384ed,
    size: 45,
    role: "consulta si hay un fichero cargado",
    claim: "retorna 1 si [0x57C364]!=0 o [0x57C368]!=0, 0 en caso contrario",
    level: "CONFIRMED",
    evidence: [
      "0x5384F1-0x53850A: cmp [0x57C364],0; cmp [0x57C368],0; [ebp-4]=0 o 1",
      "0x538513-0x538519: retorna [ebp-4]",
    ],
  },
  {
    address: 0x00538017,
    size: 41,
    role: "trampolin de reproduccion (reenvia los tres argumentos)",
    claim: "push [ebp+0x10]; push [ebp+0xC]; push [ebp+8]; call 0x537ED1; retorna 0",
    level: "CONFIRMED",
    evidence: [
      "0x53801A-0x53802B: push [ebp+0x10]; push [ebp+0xC]; push [ebp+8]; call 0x537ED1; add esp,0xC",
      "0x53802E-0x538031: xor eax,eax; ret",
    ],
  },
  {
    address: 0x00537ed1,
    size: 326,
    role: "reproduce un sonido por nombre y opcionalmente espera",
    claim:
      "sale si [0x558118]==0; construye dos rutas con 0x5406B0 (formatos 0x55811C y 0x558124), comprueba la segunda con 0x541060/0x540C20, llama 0x53A5A2(ruta1,ruta2,arg2) y, si arg3!=0, espera con GetTickCount hasta consumir arg3",
    level: "CONFIRMED",
    evidence: [
      "0x537EDA-0x537EE3: cmp dword [0x558118],0; je fin",
      "0x537EE8-0x537F10: 0x5406B0(buf,0x55811C,arg8); 0x5406B0(buf2,0x558124,arg8)",
      "0x537F18-0x537F32: 0x541060(buf2,0x55812C); if 0 -> fin",
      "0x537F4F-0x537F83: 0x541060(buf1,0x558130); if 0 -> fin",
      "0x537FA5-0x537FB7: push [ebp+0xC]; push buf1; push buf2; call 0x53A5A2",
      "0x537FCB-0x538011: bucle con call [0x65F32C] (GetTickCount) hasta superar [ebp+0x10]",
    ],
  },
  {
    address: 0x0053a5a2,
    size: 89,
    role: "comprueba dos ficheros y arranca la reproduccion",
    claim:
      "si 0x53A7D8(a) y 0x53A7D8(b) son distintos de 0 (existencia via 0x541060/0x540C20): 0x538731(0x537E9C(1), b, 1), 0x53A5FB(a,0xF,arg3) y 0x53851A(); retorna 0; si falta alguno retorna 1",
    level: "CONFIRMED",
    evidence: [
      "0x53A5A5-0x53A5C3: 0x53A7D8(arg8); 0x53A7D8(argC); si alguno es 0 -> retorno 1",
      "0x53A5C5-0x53A5D6: push 1; push [ebp+0xC]; call 0x537E9C; push eax; call 0x538731",
      "0x53A5D9-0x53A5E8: push [ebp+0x10]; push 0xF; push [ebp+8]; call 0x53A5FB",
      "0x53A5EB-0x53A5F2: call 0x53851A; xor eax,eax",
    ],
  },
  {
    address: 0x00538cb0,
    size: 107,
    role: "segunda creacion de DirectSound (ruta sin llamantes)",
    claim:
      "DirectSoundCreate(NULL,&[0x5847E4],NULL); si falla retorna; si no llama 0x537E9C(1) y SetCooperativeLevel(vtable+0x18) con un solo argumento de pila, copia el dispositivo a [0x57C39C], memset(0x5846F0,0,0x2C) y memset(0x5846C8,0,0x21) y pone [0x5847E0]=1; no crea buffer primario ni comprueba el resultado de SetCooperativeLevel",
    level: "CONFIRMED",
    evidence: [
      "0x538CB3-0x538CC5: push 0; push 0x5847E4; push 0; call 0x5404F0; if eax!=0 -> ret",
      "0x538CC7-0x538CDD: push 1; call 0x537E9C; push eax; push [0x5847E4]; call [ecx+0x18]",
      "0x538CE6-0x538CEB: [0x57C39C]=[0x5847E4]",
      "0x538CF0-0x538D0A: memset(0x5846F0,0,0x2C); memset(0x5846C8,0,0x21)",
      "0x538D12: mov byte [0x5847E0],1",
    ],
  },
  {
    address: 0x00538fd5,
    size: 35,
    role: "prologo de la ruta muerta (cero llamantes)",
    claim:
      "llama 0x538CB0; si [ebp+8]!=0 llama 0x538D1B(arg), si no llama 0x538FF8(); ret",
    level: "CONFIRMED",
    evidence: [
      "0x538FD8: call 0x538CB0",
      "0x538FDD-0x538FEF: cmp dword [ebp+8],0; je 0x538FF1; push [ebp+8]; call 0x538D1B",
      "0x538FF1-0x538FF7: call 0x538FF8; ret",
    ],
  },
];

/**
 * Indirect calls through a COM vtable. `offset` is the slot byte offset inside
 * the interface, and `api` is the DirectSound method at that slot (the standard
 * IDirectSound / IDirectSoundBuffer layout).
 */
export const VTABLE_CALLS = [
  { site: 0x00538592, owner: 0x00538551, iface: "IDirectSound", offset: 0x18, api: "SetCooperativeLevel", args: "hwnd, 3 (DSSCL_PRIORITY)" },
  { site: 0x005385a7, owner: 0x00538551, iface: "IDirectSound", offset: 0x08, api: "Release", args: "-" },
  { site: 0x005385fe, owner: 0x00538551, iface: "IDirectSound", offset: 0x0c, api: "CreateSoundBuffer", args: "&g_dsbd(primary), &g_pPrimary, NULL" },
  { site: 0x005386b7, owner: 0x00538551, iface: "IDirectSound", offset: 0x08, api: "Release", args: "-" },
  { site: 0x005386dc, owner: 0x00538551, iface: "IDirectSoundBuffer", offset: 0x38, api: "SetFormat", args: "0x5582E0" },
  { site: 0x005386f1, owner: 0x00538551, iface: "IDirectSoundBuffer", offset: 0x08, api: "Release", args: "-" },
  { site: 0x0053870c, owner: 0x00538551, iface: "IDirectSound", offset: 0x08, api: "Release", args: "-" },
  { site: 0x00538765, owner: 0x00538731, iface: "IDirectSoundBuffer", offset: 0x48, api: "Stop", args: "-" },
  { site: 0x00538776, owner: 0x00538731, iface: "IDirectSoundBuffer", offset: 0x08, api: "Release", args: "-" },
  { site: 0x0053880b, owner: 0x00538731, iface: "IDirectSound", offset: 0x0c, api: "CreateSoundBuffer", args: "&g_dsbd(stream), &g_pStream, NULL" },
  { site: 0x00538860, owner: 0x00538731, iface: "IDirectSoundBuffer", offset: 0x30, api: "Play", args: "0, 0, 1 (DSBPLAY_LOOPING)" },
  { site: 0x005388a3, owner: 0x00538731, iface: "IDirectSoundBuffer", offset: 0x3c, api: "SetVolume", args: "g_musicVolume*0x1E-0xBB8" },
  { site: 0x00538063, owner: 0x00538040, iface: "IDirectSoundBuffer", offset: 0x50, api: "Restore", args: "-" },
  { site: 0x00538094, owner: 0x00538075, iface: "IDirectSoundBuffer", offset: 0x24, api: "GetStatus", args: "&status" },
  { site: 0x00538101, owner: 0x005380b3, iface: "IDirectSoundBuffer", offset: 0x10, api: "GetCurrentPosition", args: "&play, &write" },
  { site: 0x0053815a, owner: 0x005380b3, iface: "IDirectSoundBuffer", offset: 0x2c, api: "Lock", args: "0, 0x4000, &p1, &n1, &p2, &n2, 0" },
  { site: 0x00538251, owner: 0x005380b3, iface: "IDirectSoundBuffer", offset: 0x48, api: "Stop", args: "-" },
  { site: 0x0053827e, owner: 0x005380b3, iface: "IDirectSoundBuffer", offset: 0x4c, api: "Unlock", args: "p1, n1, p2, n2" },
  { site: 0x0053854c, owner: 0x0053851a, iface: "IDirectSoundBuffer", offset: 0x48, api: "Stop", args: "-" },
  { site: 0x00538cdd, owner: 0x00538cb0, iface: "IDirectSound", offset: 0x18, api: "SetCooperativeLevel", args: "hwnd (un solo argumento de pila: falta dwLevel)" },
];

/**
 * Routes. Each node is a mapped function; each step is the `call` site that
 * carries the edge. `status` is deliberately explicit: the chain EXISTS in the
 * code (static) and is reachable from the entry point by direct calls, but it was
 * not observed running (no instrumentation of the reference image).
 */
export const INIT_ROUTES = [
  {
    id: "A",
    label: "arranque: WinMain -> frontera audio/cine -> dispositivo + buffer primario",
    status: "CONFIRMED (estatico); ejecucion NO observada",
    nodes: [0x00541b20, 0x00463278, 0x0053d822, 0x0053b10b, 0x00538551],
    steps: [
      { from: 0x00541b20, to: 0x00463278, site: 0x00541c5a, note: "CRT startup -> WinMain (4 argumentos)" },
      { from: 0x00463278, to: 0x0053d822, site: 0x00463a02, note: "WinMain -> frontera audio/cine" },
      { from: 0x0053d822, to: 0x0053b10b, site: 0x0053d82f, note: "inicializacion del sistema de sonido" },
      { from: 0x0053b10b, to: 0x00538551, site: 0x0053b120, note: "creacion del dispositivo (una sola vez)" },
    ],
  },
  {
    id: "B",
    label: "reproduccion por nombre: WinMain -> trampolin -> sonido -> buffer secundario",
    status: "CONFIRMED (estatico); ejecucion NO observada",
    nodes: [0x00541b20, 0x00463278, 0x00538017, 0x00537ed1, 0x0053a5a2, 0x00538731, 0x00538551],
    steps: [
      { from: 0x00541b20, to: 0x00463278, site: 0x00541c5a, note: "CRT startup -> WinMain" },
      { from: 0x00463278, to: 0x00538017, site: 0x004637f0, note: "WinMain -> trampolin (tambien en 0x463808)" },
      { from: 0x00538017, to: 0x00537ed1, site: 0x00538026, note: "trampolin -> reproducir por nombre" },
      { from: 0x00537ed1, to: 0x0053a5a2, site: 0x00537fb7, note: "construye rutas y comprueba existencia" },
      { from: 0x0053a5a2, to: 0x00538731, site: 0x0053a5d1, note: "crea el buffer secundario" },
      { from: 0x00538731, to: 0x00538551, site: 0x00538739, note: "asegura el dispositivo (retorno inmediato si ya existe)" },
    ],
  },
  {
    id: "C",
    label: "voz: ruta de voz -> buffer secundario",
    status: "CONFIRMED (estatico); ejecucion NO observada",
    partial: true,
    nodes: [0x0053ff0d, 0x00538731, 0x00538551],
    steps: [
      { from: 0x0053ff0d, to: 0x00538731, site: 0x0054002a, note: "si el buffer no esta sonando, reproduce la ruta de voz" },
      { from: 0x00538731, to: 0x00538551, site: 0x00538739, note: "asegura el dispositivo" },
    ],
    prefix: "0x0053FF0D se alcanza desde 0x0053FE80 <- 0x00540036 / 0x00540301 / 0x00540088 / 0x005402C8 / 0x0054032D (docs/SUBSYSTEM_SEGMENTS.md, S3)",
  },
];

/** The chain that exists but nothing calls. */
export const DEAD_ROUTE = {
  id: "D",
  label: "segunda creacion de DirectSound: 0x00538FD5 -> 0x00538CB0",
  status: "CONFIRMED (estatico): la cadena existe; NO tiene llamantes, ejecucion NO demostrada",
  nodes: [0x00538fd5, 0x00538cb0],
  steps: [{ from: 0x00538fd5, to: 0x00538cb0, site: 0x00538fd8, note: "unico llamante de 0x00538CB0" }],
  facts: [
    "0x00538FD5 tiene 0 aristas entrantes en calls.tsv y basis=prologue (nunca se vio un call rel32 hacia ella)",
    "0x00538CB0 tiene 1 arista entrante: 0x00538FD5 -> 0x00538CB0",
    "no hay ruta de datos conocida hacia 0x00538FD5: no aparece en ninguna tabla de punteros verificada",
    "la ausencia de llamantes directos NO prueba codigo muerto: callbacks y despacho por puntero son invisibles al grafo",
  ],
};

/**
 * Why no dynamic confirmation. These are facts about the analysis environment,
 * recorded so a future session does not have to rediscover them.
 */
export const LIMITS = [
  "La imagen de referencia (1557504 B, MD5 692b12825003417cc4a5a9d4db13eebe) ya no estaba en la maquina de analisis: solo queda una build modificada de 1559040 B (MD5 3198350eb398db9a64771842d7781b4b, entrada 0x00541FA0).",
  "La build modificada no sirve para instrumentar estas direcciones: la entrada y la seccion .data cambian de direccion, y el patron de bytes de 0x00538551 no aparece en ella.",
  "Por eso ninguna ruta de este modulo tiene hits medidos: la evidencia es estatica (desensamblado + grafo de llamadas), no dinamica.",
  "El contenido de 0x5582E0 (WAVEFORMATEX del buffer primario) y el texto de los formatos 0x55811C/0x558124 no se pueden leer sin la imagen original.",
  "La identificacion de 0x541060/0x540C20/0x540CA0/0x540DE0/0x540F90/0x5416E0/0x541950 como fopen/fclose/fread/ftell/fseek/malloc/HeapFree es una lectura de la forma de las llamadas (HYPOTHESIS), no un nombre verificado.",
];

const hex = (value) => `0x${value.toString(16).toUpperCase()}`;

/**
 * Contrast the declarations above against `analysis/EU/` (functions.tsv +
 * calls.tsv). It needs no binary and no execution, so it runs in CI. It checks:
 *
 *   - every declared function exists with the declared size;
 *   - every route step is a real `call` edge and its site lies inside the caller;
 *   - every route starts at the entry point and ends at 0x00538551;
 *   - the caller set of 0x00538551 is exactly {0x00538731, 0x0053B10B};
 *   - 0x00538FD5 has no caller and 0x00538CB0 has exactly one;
 *   - every vtable site lies inside its owner;
 *   - every semantic claim carries evidence with an address, and CONFIRMED
 *     entries carry at least two lines.
 */
export function validateAudio(model) {
  const problems = [];
  const byStart = new Map(model.functions.map((fn) => [fn.start, fn]));
  const calls = model.edges.filter((edge) => edge.kind === "call");
  const edges = new Set(calls.map((edge) => `${edge.from}:${edge.to}`));
  const incoming = new Map();
  for (const edge of calls) {
    if (!incoming.has(edge.to)) incoming.set(edge.to, new Set());
    incoming.get(edge.to).add(edge.from);
  }
  const inside = (site, fn) => site >= fn.start && site < fn.start + fn.size;

  for (const entry of AUDIO_SEMANTICS) {
    const fn = byStart.get(entry.address);
    if (!fn) {
      problems.push(`semantica: ${hex(entry.address)} no existe en el mapa`);
      continue;
    }
    if (fn.kind !== "func") problems.push(`semantica: ${hex(entry.address)} no es una funcion (${fn.kind})`);
    if (fn.size !== entry.size) problems.push(`semantica: ${hex(entry.address)} tamano ${fn.size} != ${entry.size}`);
    if (!entry.claim || entry.claim.length < 10) problems.push(`semantica: ${hex(entry.address)} sin afirmacion`);
    if (!Array.isArray(entry.evidence) || entry.evidence.length === 0) {
      problems.push(`semantica: ${hex(entry.address)} sin evidencia`);
    }
    if (entry.level !== "CONFIRMED" && entry.level !== "HYPOTHESIS") {
      problems.push(`semantica: ${hex(entry.address)} nivel invalido ${entry.level}`);
    }
    if (entry.level === "CONFIRMED" && entry.evidence.length < 2) {
      problems.push(`semantica: ${hex(entry.address)} CONFIRMED con evidencia insuficiente`);
    }
    for (const line of entry.evidence ?? []) {
      if (!/0x[0-9A-Fa-f]{6}/.test(line)) problems.push(`semantica: ${hex(entry.address)} evidencia sin direccion: ${line}`);
    }
  }

  for (const route of INIT_ROUTES) {
    if (!route.partial && route.nodes[0] !== ENTRY) problems.push(`ruta ${route.id}: no empieza en la entrada`);
    if (route.partial && route.nodes[0] === ENTRY) problems.push(`ruta ${route.id}: es parcial pero empieza en la entrada`);
    if (route.nodes[route.nodes.length - 1] !== DS_DEVICE_INIT) {
      problems.push(`ruta ${route.id}: no termina en ${hex(DS_DEVICE_INIT)}`);
    }
    if (route.steps.length !== route.nodes.length - 1) {
      problems.push(`ruta ${route.id}: ${route.steps.length} pasos para ${route.nodes.length} nodos`);
    }
    for (let i = 0; i < route.steps.length; i++) {
      const step = route.steps[i];
      const from = route.nodes[i];
      const to = route.nodes[i + 1];
      if (step.from !== from || step.to !== to) {
        problems.push(`ruta ${route.id}: paso ${i} no encadena ${hex(from)} -> ${hex(to)}`);
      }
      const fn = byStart.get(from);
      if (!fn) {
        problems.push(`ruta ${route.id}: ${hex(from)} no existe`);
        continue;
      }
      if (!inside(step.site, fn)) {
        problems.push(`ruta ${route.id}: el sitio ${hex(step.site)} cae fuera de ${hex(from)}+${fn.size}`);
      }
      if (!edges.has(`${from}:${to}`)) {
        problems.push(`ruta ${route.id}: falta la arista ${hex(from)} -> ${hex(to)}`);
      }
    }
  }

  const callers = incoming.get(DS_DEVICE_INIT) ?? new Set();
  const expected = new Set([0x00538731, 0x0053b10b]);
  if (callers.size !== expected.size || [...expected].some((caller) => !callers.has(caller))) {
    problems.push(
      `${hex(DS_DEVICE_INIT)}: llamantes ${[...callers].map(hex).join(", ") || "ninguno"} != ${[...expected].map(hex).join(", ")}`,
    );
  }

  if ((incoming.get(0x00538fd5) ?? new Set()).size !== 0) {
    problems.push("0x538FD5 dejo de estar sin llamantes: la ruta muerta ya no lo es");
  }
  const altCallers = incoming.get(0x00538cb0) ?? new Set();
  if (altCallers.size !== 1 || !altCallers.has(0x00538fd5)) {
    problems.push(`0x538CB0: llamantes ${[...altCallers].map(hex).join(", ") || "ninguno"} != 0x538FD5`);
  }

  for (const route of [DEAD_ROUTE]) {
    for (let i = 0; i < route.steps.length; i++) {
      const step = route.steps[i];
      const from = route.nodes[i];
      const to = route.nodes[i + 1];
      const fn = byStart.get(from);
      if (!fn) {
        problems.push(`ruta ${route.id}: ${hex(from)} no existe`);
        continue;
      }
      if (!inside(step.site, fn)) problems.push(`ruta ${route.id}: sitio ${hex(step.site)} fuera de ${hex(from)}`);
      if (!edges.has(`${from}:${to}`)) problems.push(`ruta ${route.id}: falta la arista ${hex(from)} -> ${hex(to)}`);
    }
  }

  const live = new Set(INIT_ROUTES.flatMap((route) => route.nodes));
  for (const node of DEAD_ROUTE.nodes) {
    if (node !== DS_DEVICE_INIT && live.has(node)) problems.push(`ruta muerta: ${hex(node)} aparece en una ruta viva`);
  }

  const seenGlobals = new Set();
  for (const global of DSOUND_GLOBALS) {
    if (seenGlobals.has(global.address)) problems.push(`global duplicado: ${hex(global.address)}`);
    seenGlobals.add(global.address);
    if (global.writer != null && !byStart.has(global.writer)) {
      problems.push(`global ${hex(global.address)}: escritor ${hex(global.writer)} no existe`);
    }
  }

  for (const call of VTABLE_CALLS) {
    const owner = byStart.get(call.owner);
    if (!owner) {
      problems.push(`vtable: ${hex(call.owner)} no existe`);
      continue;
    }
    if (!inside(call.site, owner)) problems.push(`vtable: sitio ${hex(call.site)} fuera de ${hex(call.owner)}`);
    if (!Number.isInteger(call.offset) || call.offset < 0 || call.offset > 0x100 || call.offset % 4 !== 0) {
      problems.push(`vtable: offset invalido en ${hex(call.site)}`);
    }
    if (!call.api) problems.push(`vtable: sitio ${hex(call.site)} sin API`);
  }

  return problems;
}
