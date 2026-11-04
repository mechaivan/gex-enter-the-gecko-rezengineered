# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE started (Etapa A documented in Phase 1; bounded static E-3/M-13 + Línea A/B executed read-only 2026-10-10 under explicit authorization - no Ghidra/decomp; static function map + call graph + per-function imports on the EU exe 2026-10-25, `docs/FUNCTION_MAP.md`; first evidence-based subsystem segmentation 2026-10-30, `docs/SUBSYSTEM_SEGMENTS.md`; dynamic frame-step trace with hardware breakpoints 2026-10-30, `docs/FRAME_STEP_TRACE.md`; static DirectSound init routes 2026-10-31, `docs/AUDIO_INIT_PATHS.md`; U-1 subfase completada hasta U-1e, pendiente U-1f)
**Implementation:** Started 2026-10-31 — native port skeleton in `port/` (C11, no dependencies): reader for the `SLAG` audio container (`0x0053899C` + loader `0x005388AC`), 29 test cases + fuzz, 69 305 checks, `docs/SAG_CONTAINER.md`; **audio route** implemented the same day (`0x00538285` IMA ADPCM decoder + `0x005380B3` streaming pump + `0x00538731` player) with an injectable backend, 60 052 + 144 checks, `docs/AUDIO_MIXER_RE.md`; **audible audio** added 2026-11-01 (I-2b): real DirectSound backend (`dsound.h`/`-ldsound`, no new dependency) and the frame-loop scheduler, 215 + 50 checks including the device suite, `docs/AUDIO_MIXER_RE.md` §16; **native frame loop** added 2026-11-01 (I-3): the M-13 accumulator moved into `port/src/core/loop_core.c` (single implementation, the prototype harness now compiles it), `port/src/core/frame.c` reproduces the measured order pre-frame → update × N → audio scheduler → present, 410 checks (incluida la regresión T6b del error de `pre_frame`) + `framedemo`, `docs/FRAME_LOOP_PORT.md`; **first native process** added 2026-11-02 (I-4): real Win32 window, `WM_KEYDOWN`/`WM_KEYUP` routing, focus loss, `QueryPerformanceCounter` clock and the I-3 frame loop behind a null renderer (`gexloop.exe`), 826 + 117 + 1009 new checks and a real-window run (Escape and `WM_CLOSE`, 0 errors), `docs/WINDOW_INPUT_PORT.md`; **renderer research & design** added 2026-11-02 (I-5): original Glide surface fully censused (38 imports), present sequence confirmed, alternatives evaluated (D3D 11 recommended over OpenGL/Glide-layer/software, conditioned on device-creation test R-1), `gex_renderer` contract proposed with the null renderer kept for tests, validation plan R-1…R-8; `docs/RENDERER_ARCHITECTURE_RESEARCH.md`; **D3D11 device** added 2026-11-02 (R-1): `gex_d3d11_device` creates/destroys the device in hardware and WARP modes (both reached feature level 11_1 on the maintainer machine), 205 new checks with Clang 22.1.8 and GCC 16.1.0, leak check by manual-refere... (line truncated to 2000 chars)
**Testing:** Hito 1 + 5ª–9ª sesión (2026-10-09, PC mantenedor; nGlide 2.10, ≈25 FPS Steam, wrapper = nGlide 2.10 CONFIRMADO por payload-hash, imports solo-sistema/backend dinámico; A2 46%, global 20%; resto pendiente) + smoke in-game H4a (2026-10-11, PC mantenedor: arranque real hasta menú; registro restaurado)
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-11-04

### Avances recientes (U-1 - Geometría Original)

- **U-1a (stride y campos):** Identificado stride constante (12 bytes) y campos ([ebp + 0x8], [ebp + 0xc], [ebp + 0x10])
- **U-1b (cálculo de w):** Confirmado w = 1.0f (2D) y w = valor + 8.0f (3D)
- **U-1c (geometría completa):** Investigada la construcción de los cuatro vértices, verificado el stride de 12 bytes
- **U-1d (recuperación de índices):** Seguido el origen de los tres argumentos, identificado operaciones de multiplicación por stride
- **U-1e (relación geometría-índices):** Analizado cómo se relacionan los vértices con los índices, determinado que los vértices se generan directamente a partir de los argumentos

### Pendientes

- **U-1f (relación índices-recursos):** Analizar cómo se relacionan los índices con los recursos (texturas, etc.)
- **U-1g (relación con DFX):** Investigar la relación con los archivos DFX
- **U-1h (interpretación final):** Determinar la interpretación exacta de los tres campos (podrían ser coordenadas, índices, offsets o referencias a estructuras)

## Estado vigente del port gráfico (R-1 a R-4)

R-3 está **COMPLETADO**: el renderer D3D11 crea una RTV del backbuffer, limpia el target y llama a `Present(0, 0)` (`SyncInterval=0`, `Flags=0`). La salida visible es únicamente el color de limpieza; no hay geometría, shaders, texturas, cámara ni lógica del juego. Device/context de R-1 y swapchain/backbuffer de R-2 son prestados; el renderer posee su RTV.

Pruebas asociadas a R-3: `test_d3d11_renderer.exe` = 28 checks y `test_app.exe` = 117 checks de aplicación (145 combinados). Batería global Clang: 133.015 checks, 0 fallos en 12 suites. GCC no tiene validación completa porque `test_sag.exe` no compila en el entorno documentado.

**Limitación de resize:** `test_d3d11_renderer.c` registra un fallo de `gex_d3d11_swapchain_resize` como INFO y continúa con el tamaño original; no demuestra resize exitoso en todos los entornos.

R-4 está **INICIADO como investigación**, no como implementación. VSync configurable, tearing, Debug Layer configurable y recuperación de device loss siguen pendientes. Ver `docs/R4_DIAGNOSTIC.md`.