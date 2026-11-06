# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE started (Etapa A documented in Phase 1; bounded static E-3/M-13 + Línea A/B executed read-only 2026-10-10 under explicit authorization - no Ghidra/decomp; static function map + call graph + per-function imports on the EU exe 2026-10-25, `docs/FUNCTION_MAP.md`; first evidence-based subsystem segmentation 2026-10-30, `docs/SUBSYSTEM_SEGMENTS.md`; dynamic frame-step trace with hardware breakpoints 2026-10-30, `docs/FRAME_STEP_TRACE.md`; static DirectSound init routes 2026-10-31, `docs/AUDIO_INIT_PATHS.md`; U-1f completada 2026-11-05 con corrección de U-1a…U-1e, `U1F_RESULTADOS.md`; U-1g completada 2026-11-06, `U1G_RESULTADOS.md`)
**Implementation:** Started 2026-10-31 — native port skeleton in `port/` (C11, no dependencies): reader for the `SLAG` audio container (`0x0053899C` + loader `0x005388AC`), 29 test cases + fuzz, 69 305 checks, `docs/SAG_CONTAINER.md`; **audio route** implemented the same day (`0x00538285` IMA ADPCM decoder + `0x005380B3` streaming pump + `0x00538731` player) with an injectable backend, 60 052 + 144 checks, `docs/AUDIO_MIXER_RE.md`; **audible audio** added 2026-11-01 (I-2b): real DirectSound backend (`dsound.h`/`-ldsound`, no new dependency) and the frame-loop scheduler, 215 + 50 checks including the device suite, `docs/AUDIO_MIXER_RE.md` §16; **native frame loop** added 2026-11-01 (I-3): the M-13 accumulator moved into `port/src/core/loop_core.c` (single implementation, the prototype harness now compiles it), `port/src/core/frame.c` reproduces the measured order pre-frame → update × N → audio scheduler → present, 410 checks (incluida la regresión T6b del error de `pre_frame`) + `framedemo`, `docs/FRAME_LOOP_PORT.md`; **first native process** added 2026-11-02 (I-4): real Win32 window, `WM_KEYDOWN`/`WM_KEYUP` routing, focus loss, `QueryPerformanceCounter` clock and the I-3 frame loop behind a null renderer (`gexloop.exe`), 826 + 117 + 1009 new checks and a real-window run (Escape and `WM_CLOSE`, 0 errors), `docs/WINDOW_INPUT_PORT.md`; **renderer research & design** added 2026-11-02 (I-5): original Glide surface fully censused (38 imports), present sequence confirmed, alternatives evaluated (D3D 11 recommended over OpenGL/Glide-layer/software, conditioned on device-creation test R-1), `gex_renderer` contract proposed with the null renderer kept for tests, validation plan R-1…R-8; `docs/RENDERER_ARCHITECTURE_RESEARCH.md`; **D3D11 device** added 2026-11-02 (R-1): `gex_d3d11_device` creates/destroys the device in hardware and WARP modes (both reached feature level 11_1 on the maintainer machine), 205 new checks with Clang 22.1.8 and GCC 16.1.0, leak check by manual-refere... (line truncated to 2000 chars)
**Testing:** Hito 1 + 5ª–9ª sesión (2026-10-09, PC mantenedor; nGlide 2.10, ≈25 FPS Steam, wrapper = nGlide 2.10 CONFIRMADO por payload-hash, imports solo-sistema/backend dinámico; A2 46%, global 20%; resto pendiente) + smoke in-game H4a (2026-10-11, PC mantenedor: arranque real hasta menú; registro restaurado)
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-11-06

### Avances recientes (U-1 - Geometría Original)

> **Corrección de fondo (U-1f, 2026-11-05).** Las conclusiones de U-1a…U-1e sobre
> «stride de 12 bytes», «cálculo de w» y «`0x00432DD1` = ruta 3D» eran
> **incorrectas** y quedan retiradas. Hechos corregidos:
> `0x00432BBD` es un **glifo de texto 2D** (índice de glifo → celda 16×16 de
> `FONT.3DF`, quad 8×8, stride de vértice 0x3C); `0x00432DD1` es un **rectángulo
> 2D sólido** (`x=arg1&0xFF`, `y=(arg1>>8)+3`, `w=(arg2&0xFF)<<4`,
> `h=(arg2>>8)*12`, sin textura). La **geometría 3D real** está en `0x0046558A`
> (17 068 B): flujo de comandos con opcode en `byte[+0xB]`, índice de 14 bits
> (`& 0x3FFF`) hacia la tabla de texturas `0x006488F4` y vértices *inline*.
> Evidencia completa: [`U1F_RESULTADOS.md`](U1F_RESULTADOS.md).

- **U-1a (stride y campos):** CORREGIDO en U-1f — no hay struct de 12 B; los
  argumentos de `0x00432DD1` son bytes empaquetados `x|(y<<8)` y `w|(h<<8)`.
- **U-1b (cálculo de w):** CORREGIDO en U-1f — el `1.0f` se escribe en
  `vértice+0x20` (campo no verificado) y el `+8` es el tamaño del quad de glifo;
  no hay cálculo de `w`.
- **U-1c (geometría completa):** CORREGIDO en U-1f — se construyen quads 2D
  (glifo 8×8 y rectángulo), no geometría 3D.
- **U-1d (recuperación de índices):** CORREGIDO en U-1f — el primer argumento de
  `0x00432BBD` es un índice de glifo (`(char-0x20)&0x3F`); los registros vienen
  de la lista de 6 B en `0x0065BFC0` (contador `0x0065CBE0`, emisor
  `0x00432852`).
- **U-1e (relación geometría-índices):** CORREGIDO en U-1f — no hay secuencia de
  índices; las hipótesis H1 y H6 quedan retiradas.
- **U-1f (relación índices-recursos):** COMPLETADO (2026-11-05) — `FONT.3DF`
  cargado por `0x004329EF` y referenciado por `[0x0057B5EC]`; `.VFX` construye la
  tabla `0x006488F4` (`0x00461C32`); `.DFX` alimenta el flujo 3D (`0x00461E0B`,
  relocalización `0x004735C0`); orquestador `0x0043B11D`.
- **U-1g (relación con DFX/VFX):** COMPLETADO (2026-11-06) — flujo de comandos
  de `0x0046558A` (un comando por llamada; lista enlazada de registros de 8 B de
  cabecera; opcode en `byte[+0xB]`; prefijo `0xE1`; vértices *inline*), pool
  `0x64FF00 + 0x6000*level[+0x14]` con doble buffer, índice de 14 bits
  (`word[+0x1A] & 0x3FFF`) → `0x006488F4`, y formatos `.DFX` (`{count,
  offsets[]}` + imagen relocalizable) y `.VFX` (`count` + registros
  `0x8C+[+0x84]`) validados con `GEX.DFX`/`GEX.VFX`. Evidencia completa:
  [`U1G_RESULTADOS.md`](U1G_RESULTADOS.md).

### Pendientes

- **U-1h (interpretación final):** semántica de los campos del flujo 3D y del
  grafo del `.DFX`; incluye las cuestiones abiertas de U-1g (solapamiento
  `+0x8`/`+0xB` en triángulos, longitud `0x20` del opcode `0x30`, estructura
  interna del `.DFX`, `record[+0x0C]`, `+0x14` del `.VFX`).
- **Abiertas de U-1f/U-1g:** `0x004338D7` (rama `estilo & 0x80`), layout del
  vértice de rectángulo 2D, layout del vértice de
  `grDrawPlanarPolygonVertexList`, y el registro estático de cierre `0x64FB40`.

## Estado vigente del port gráfico (R-1 a R-5)

R-3, R-4 y R-5 están **COMPLETADOS**: implementación básica del renderer D3D11 con device, swapchain, Present, clear y render target view. Pruebas verificadas en modo WARP con 104 checks, 0 fallos. No hay geometría sintética específica con shaders y buffer de vértices.