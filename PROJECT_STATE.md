# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE started (Etapa A documented in Phase 1; bounded static E-3/M-13 + Línea A/B executed read-only 2026-10-10 under explicit authorization - no Ghidra/decomp; static function map + call graph + per-function imports on the EU exe 2026-10-25, `docs/FUNCTION_MAP.md`; first evidence-based subsystem segmentation 2026-10-30, `docs/SUBSYSTEM_SEGMENTS.md`; dynamic frame-step trace with hardware breakpoints 2026-10-30, `docs/FRAME_STEP_TRACE.md`; static DirectSound init routes 2026-10-31, `docs/AUDIO_INIT_PATHS.md`)
**Implementation:** Started 2026-10-31 — native port skeleton in `port/` (C11, no dependencies): reader for the `SLAG` audio container (`0x0053899C` + loader `0x005388AC`), 29 test cases + fuzz, 69 305 checks, `docs/SAG_CONTAINER.md`; **audio route** implemented the same day (`0x00538285` IMA ADPCM decoder + `0x005380B3` streaming pump + `0x00538731` player) with an injectable backend, 60 052 + 144 checks, `docs/AUDIO_MIXER_RE.md`; **audible audio** added 2026-11-01 (I-2b): real DirectSound backend (`dsound.h`/`-ldsound`, no new dependency) and the frame-loop scheduler, 215 + 50 checks including the device suite, `docs/AUDIO_MIXER_RE.md` §16; **native frame loop** added 2026-11-01 (I-3): the M-13 accumulator moved into `port/src/core/loop_core.c` (single implementation, the prototype harness now compiles it), `port/src/core/frame.c` reproduces the measured order pre-frame → update × N → audio scheduler → present, 410 checks (incluida la regresión T6b del error de `pre_frame`) + `framedemo`, `docs/FRAME_LOOP_PORT.md`; **first native process** added 2026-11-02 (I-4): real Win32 window, `WM_KEYDOWN`/`WM_KEYUP` routing, focus loss, `QueryPerformanceCounter` clock and the I-3 frame loop behind a null renderer (`gexloop.exe`), 826 + 117 + 1009 new checks and a real-window run (Escape and `WM_CLOSE`, 0 errors), `docs/WINDOW_INPUT_PORT.md`; **renderer research & design** added 2026-11-02 (I-5): original Glide surface fully censused (38 imports), present sequence confirmed, alternatives evaluated (D3D 11 recommended over OpenGL/Glide-layer/software, conditioned on device-creation test R-1), `gex_renderer` contract proposed with the null renderer kept for tests, validation plan R-1…R-8; **no rendering implemented**, `docs/RENDERER_ARCHITECTURE_RESEARCH.md`; **D3D11 device** added 2026-11-02 (R-1): `gex_d3d11_device` creates/destroys the device in hardware and WARP modes (both reached feature level 11_1 on the maintainer machine), 205 new checks with Clang 22.1.8 and GCC 16.1.0, leak check by manual-reference + final Release to 0 (no D3D11 debug layer installed: `DXGI_ERROR_SDK_COMPONENT_MISSING` documented), no swapchain/window/drawing, `docs/D3D11_DEVICE.md`; **DXGI swapchain** added 2026-11-03 (R-2): bitblt `DXGI_SWAP_EFFECT_DISCARD`, 2 buffers, `R8G8B8A8_UNORM`, windowed on the existing I-4 `HWND`, backbuffer exposed for R-3, `ResizeBuffers` (0×0 refused without touching DXGI) and backbuffer→swapchain→device teardown; measured refs device 2→3→2, swapchain 2 (one held by its backbuffer), backbuffer 1, all to 0 when released in order; 429 new checks with Clang 22.1.8 and GCC 16.1.0, hardware and WARP, no `Present`/clear/drawing, `docs/D3D11_SWAPCHAIN.md`. **No playable build**: Hito P / MAIN unchanged; these are components, not the port
**Testing:** Hito 1 + 5ª–9ª sesión (2026-10-09, PC mantenedor; nGlide 2.10, ≈25 FPS Steam, wrapper = nGlide 2.10 CONFIRMADO por payload-hash, imports solo-sistema/backend dinámico; A2 46%, global 20%; resto pendiente) + smoke in-game H4a (2026-10-11, PC mantenedor: arranque real hasta menú; registro restaurado)
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-11-02

## Ampliación de objetivos (2026-10-08, solo documentación)

- Se ha ampliado el conjunto de objetivos futuros: FA-01…FA-15 (fundamentos),
  M-01…M-26 (modernización), X-01 (deferred) y X-02 (long-term research).
  Ver MODERNIZATION_GOALS.md.
- **Ninguna feature está implementada.** Todas están PROPOSED / TO INVESTIGATE
  (X-01/X-02 DEFERRED).
- La prioridad actual es la Fase 1 / Research (Fase 0 cerrada 2026-10-08).
- No se debe saltar a implementación: la documentación precede a cualquier
  modificación técnica.
- Orden: ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL.
- 2026-10-08: +M-24 localización (PROPOSED, P3, sin fase) y +X-02 posible
  rewrite Rust/Bevy (DEFERRED, long-term research, fuera de implementación).
- 2026-10-09: +M-25 voice pack selection UK/USA (PROPOSED, P3, sin fase).
- 2026-10-09: +M-26 skippable intro cinematic (PROPOSED, P3, sin fase).

## Current objective

> Understand the original PC version of Gex: Enter the Gecko and existing
> community fixes before implementing any modifications.
> Ruta corta (2026-10-10): Hito R → Hito K → Hito P (= MAIN 100%) →
> Olas 1–3. Ver ROADMAP.
>
> Cambio de fase (2026-10-31): se abre una **vía de implementación nativa** en
> paralelo a la investigación. El objetivo ya no es solo entender el binario,
> sino tener componentes reales, probados sin el juego, que se puedan integrar
> después. El primer componente es el lector del contenedor de audio `SLAG`
> (`docs/SAG_CONTAINER.md`, código en `port/`), seguido el mismo día por la ruta
> de audio completa —decodificador IMA ADPCM, bomba de streaming y reproductor—
> (`docs/AUDIO_MIXER_RE.md`). Esta vía **no** promociona Hito P ni MAIN: el panel
> sigue midiendo la ruta de mods/parches.
>
> 2026-11-01 (I-2b): la ruta de audio ya es **audible** en el port. Un *backend*
> DirectSound real (la API del original, sin dependencias nuevas) sustituye al
> WAV como salida y `gex_audio_sched` reproduce el contrato de los llamantes de
> `0x005380B3` en el frame loop. Verificado en el dispositivo de esta máquina
> (cursor a 44 100 B/s, contenido leído por `Lock` idéntico al decodificado);
> la audición humana no la puede comprobar el agente.
>
> 2026-11-01 (I-3): existe un **frame loop nativo** ejecutable. El acumulador
> M-13 se movió al port (una sola implementación) y `gex_frame_run()` reproduce
> el orden medido `pre-frame → update × N → planificador de audio → present`,
> con reloj inyectado, catch-up acotado, descartes contabilizados, pausas/reanudación y política de errores explícita (incluida corrección: los errores de pre-frame limpian acumulador y reanclan reloj). Las tres fases siguen siendo callbacks: no hay ventana,
> entrada ni renderer. 410 checks + demo `framedemo`; `docs/FRAME_LOOP_PORT.md`.
> La regresión T6b fija que un error de `pre_frame` deja `acc_us = 0` y
> reancla `t_prev_us` en `now_us`, y que el frame siguiente no recupera el
> tiempo anterior (mutaciones de ambas líneas detectadas).
>
> 2026-11-02 (I-4): existe el **primer proceso nativo ejecutable**. `gexloop.exe`
> abre una ventana Win32 real (clase `GexNativeLoopWindow`, 960×540), drena
> mensajes, mantiene estado de teclado con flancos y liberación al perder foco,
> sale con Escape o al cerrar la ventana, y conduce el frame loop de I-3 con
> `QueryPerformanceCounter` en microsegundos. Paso lógico 60 Hz (16 667 µs) y
> presentación objetivo 120 Hz (8 333 µs) con espera en
> `MsgWaitForMultipleObjectsEx`; `timeBeginPeriod(1)` se usa y se restaura para
> que la cadencia de presentación no quede limitada por los ~15,6 ms del
> temporizador por defecto. `pre_frame` y `update` son stubs y `present` es un
> **renderer nulo** que no dibuja. 826 + 117 + 1009 checks nuevos y prueba real
> de ventana (Escape, `WM_CLOSE`, pérdida de foco, ~105–108 presents/s,
> 59,5–60,0 pasos/s, 0,85–1,93 % de CPU, 0 errores). Doc:
> `docs/WINDOW_INPUT_PORT.md`.
>
> 2026-11-02/03 (I-5, R-1, R-2): el renderer nativo deja de ser solo diseño. I-5
> censa la superficie Glide del original y elige **Direct3D 11** con un contrato
> agnóstico; R-1 crea y destruye el **dispositivo D3D11** real en hardware y
> WARP (ambos FL 11_1 en esta máquina); R-2 crea la **swapchain DXGI** bitblt de
> 2 buffers sobre el `HWND` de I-4, expone el backbuffer para R-3, redimensiona
> con `ResizeBuffers` y cierra en orden backbuffer → swapchain → dispositivo.
> Todavía **no se presenta ningún frame, no hay clear y no se dibuja**; `gexloop`
> sigue con el renderer nulo. Docs: `docs/RENDERER_ARCHITECTURE_RESEARCH.md`,
> `docs/D3D11_DEVICE.md`, `docs/D3D11_SWAPCHAIN.md`.

## Vía de implementación nativa (abierta 2026-10-31)

Decisión del mantenedor: empezar a implementar sin esperar al cierre de la
Fase 3, eligiendo piezas cuyo comportamiento ya está demostrado por
instrucción. Condiciones que debe cumplir cada pieza:

1. semántica completa del original (sin bucles internos sin leer);
2. dependencias de plataforma nulas o mínimas;
3. prueba posible sin el juego completo ni assets originales;
4. consumidor identificado en el motor.

Estado:

| Pieza | Original | Código | Prueba |
| --- | --- | --- | --- |
| Lector `SLAG` (formato + payload `data`) | `0x0053899C`, `0x005388AC`, `0x00538B01`, `0x00538409`, `0x00538A99`, `0x00538ACA` | `port/src/asset/sag.c` | `port/tests/test_sag.c` (69 305 checks, 0 fallos) |
| Decodificador IMA ADPCM de 4 bits | `0x00538285`, tablas `0x00558138`/`0x00558178` | `port/src/audio/adpcm.c` | `port/tests/test_adpcm.c` (60 052 checks, 0 fallos) |
| Bomba de streaming + reproductor | `0x005380B3`, `0x00538731`, `0x00538409`, `0x005384A3`, `0x005384ED`, `0x00538040/68/75/A6` | `port/src/audio/stream.c` | `port/tests/test_audio.c` (215 checks, 0 fallos) |
| *Backend* de salida inyectable | `IDirectSoundBuffer` (Lock/Unlock/Play/Stop/GetCurrentPosition) | `gex_audio_backend` | captura en memoria + WAV (`port/tools/sagplay.c`) |
| *Backend* de dispositivo DirectSound | `0x00538551` (dispositivo), `CreateSoundBuffer(0x8000, 0xE0)`, `GetCurrentPosition`, `SetVolume` `0x0053886c` | `port/src/audio/backend_dsound.c` | `port/tests/test_device.c` (50 checks, 0 fallos, se omite sin tarjeta) |
| Programación del bombeo (frame loop) | llamantes `0x00463205`, `0x0053A5FB`, `0x0053FCB7`; prefill `0x00538842` | `port/src/audio/sched.c` | `port/tests/test_audio.c` (planificador) |
| Núcleo temporal (acumulador M-13) | `0x0043EA36` (frame step) + `16.666` de `0x0054C060` | `port/src/core/loop_core.c` | `tools/m13-core` (120 checks) + `port/tests/test_frame.c` |
| Frame loop nativo | orden de `0x0043EA36`/`0x0043F80D`/`0x0043E1BA`/`0x005380B3` | `port/src/core/frame.c` | `port/tests/test_frame.c` (410 checks, incluida T6b) + `port/tools/framedemo.c` |
| Estado de teclado (nivel, flancos, foco) | decisión del port: `WM_KEYDOWN`/`WM_KEYUP` (la ruta S5 del original es HYPOTHESIS) | `port/src/input/key_state.c` | `port/tests/test_input.c` (826 checks) |
| Reloj monotónico real | `QueryPerformanceCounter`/`QueryPerformanceFrequency` | `port/src/platform/win32_time.c` | `port/tests/test_win32_time.c` (1009 checks) |
| Renderer nulo | no aplica: I-4 declara que no dibuja | `port/src/render/null_renderer.c` | `port/tests/test_app.c` (present contabilizado) |
| Núcleo de aplicación | orden del frame de I-3 + Escape/foco | `port/src/app/i4_app.c` | `port/tests/test_app.c` (117 checks) |
| Ventana Win32 + mensajes | shell S6 (`0x00463091`/`0x00463278`), adaptado | `port/src/platform/win32_window.c` | prueba real de ventana (Escape, `WM_CLOSE`) |
| Ejecutable I-4 | — | `port/src/app/i4_main_win32.c` (`gexloop.exe`) | prueba real sostenida + CPU |
| Dispositivo Direct3D 11 | no aplica: API del sistema (decisión I-5) | `port/src/render/d3d11_device.c` | `port/tests/test_d3d11_device.c` (314 checks, hardware y WARP) |
| Swapchain DXGI sobre el HWND | `grSstWinOpen` `0x0046324D` (2 buffers de color), adaptado | `port/src/render/d3d11_swapchain.c` | `port/tests/test_d3d11_swapchain.c` (572 checks, hardware y WARP) |

Límites de esta vía, explícitos: no hay ficheros `.sag` reales disponibles y la
equivalencia con el original es estática (transcripción instrucción a
instrucción). La salida a hardware **sí** está implementada y medida desde
I-2b (2026-11-01): `test_device` y `sagplay --device` entregan PCM a un
dispositivo DirectSound real y comprueban que el cursor avanza a la frecuencia
del flujo. Lo que **no** está verificado es la audición humana: el agente no
tiene oídos, así que la confirmación por oído queda para el mantenedor con
`sagplay --device`. Ver `docs/SAG_CONTAINER.md` §6/§9 y
`docs/AUDIO_MIXER_RE.md` §11/§14/§16.

I-3 (2026-11-01) añade el frame loop: el acumulador de paso fijo, el orden de
fases medido y el tick del planificador de audio. Lo que **no** hace es jugar:
`pre_frame`, `update` y `present` son callbacks de prueba; no hay ventana,
entrada, renderer ni lectura de reloj real. La política de errores se verificó
con T6b: un fallo de `pre_frame` vacía el acumulador y reancla el reloj, de modo
que el frame siguiente no repite el tiempo fallido. Discrepancias con el
original (F1–F6) y límites en `docs/FRAME_LOOP_PORT.md` §11–§12.

I-5 (2026-11-02) **investiga y diseña** el renderer real sin implementarlo:
la superficie Glide del original queda censada (38 imports, §2.1 del doc), la
secuencia de presentación confirmada (render condicional `0x0043DC3A` +
drenaje doble + `grBufferSwap(3)` bloqueante ~23 ms) y la alternativa elegida
es **Direct3D 11** detrás de un contrato C11 agnóstico (`gex_renderer`), con
`gex_null_renderer` conservado para las pruebas y OpenGL/rasterizador propio
como alternativas. La elección quedó condicionada a R-1 (crear dispositivo),
primer paso del hito de implementación. Incógnitas U-1…U-7 (campos del
vértice, modos de combine, formatos de textura, las «2 listas» de
`0x0043DC3A`) quedan registradas y sin inventar. Detalle en
`docs/RENDERER_ARCHITECTURE_RESEARCH.md`.

R-1 (2026-11-02) **confirma la decisión con el dispositivo real**: el módulo
`gex_d3d11_device` crea y destruye el dispositivo en modo hardware y en modo
WARP — ambos a feature level 11_1 en esta máquina —, registra el nivel
obtenido, entrega el contexto inmediato y cierra sin referencias COM
pendientes (verificación conductual: referencia manual + `Release` final a 0,
porque el runtime mantiene referencias internas al device). Fallos medidos y
limpios: bits reservados → `E_INVALIDARG`; capa debug no instalada →
`DXGI_ERROR_SDK_COMPONENT_MISSING`. **314 checks nuevos**, 0 fallos, con Clang 22.1.8; GCC 16.1.0 no compila
test_sag.exe, por lo que la validación completa solo se ha realizado con Clang.
**batería completa 132 298 checks**. **No hay swapchain, ventana ni dibujo**
(R-2 y siguientes). Detalle en `docs/D3D11_DEVICE.md`.

R-2 (2026-11-03) **conecta el dispositivo con la ventana de I-4**: el módulo
`gex_d3d11_swapchain` crea una swapchain DXGI real sobre el `HWND` existente
(bitblt `DXGI_SWAP_EFFECT_DISCARD`, 2 buffers, `R8G8B8A8_UNORM`, ventana, sin
MSAA), expone el backbuffer para R-3, redimensiona con `ResizeBuffers`
(rechazando 0×0 sin tocar DXGI) y cierra en el orden backbuffer → swapchain →
dispositivo. Referencias medidas: dispositivo 2→3→2 al crear/destruir la
swapchain; swapchain 2 (una la retiene su backbuffer) y backbuffer 1; todas
llegan a 0 al liberar en orden. **No hay `Present`, ni clear, ni dibujo** (R-3
y siguientes), y `gexloop` sigue con el renderer nulo. **572 checks nuevos**, 0
fallos, hardware y WARP, Clang 22.1.8; GCC 16.1.0 no compila test_sag.exe,
por lo que la validación completa solo se ha realizado con Clang.
**batería completa 132 870 checks**. Detalle en `docs/D3D11_SWAPCHAIN.md`.

I-4 (2026-11-02) conecta la ventana, la entrada y el reloj real, pero **sigue
sin jugar**: `pre_frame` y `update` solo cuentan llamadas y `present` usa un
renderer nulo. La ventana, el reloj QPC, los mensajes, el estado de teclado, la
liberación por pérdida de foco, Escape y el cierre son reales y se ejecutaron.
La ruta de entrada del original (S5) sigue siendo HYPOTHESIS y la fuente exacta
de teclado no está confirmada; el port usa mensajes Win32 como decisión
explícita, no como reproducción. Diferencias W1–W8, pruebas y límites en
`docs/WINDOW_INPUT_PORT.md` §12–§13. `0x0043E719` queda descartado como entrada:
la traza lo sitúa en el update como máquina de estados de acción.

## Checklist de la Fase 0 ✅ (cerrada 2026-10-08)

- [x] Definir objetivos y principios (README, ROADMAP).
- [x] Crear estructura inicial del repositorio.
- [x] Recopilación inicial de fuentes públicas (PCGamingWiki, foros, proyectos).
- [x] Mapa inicial de problemas conocidos (sin verificar — ver KNOWN_ISSUES.md).
- [x] Catálogo inicial de fixes comunitarios (sin analizar técnicamente).
- [x] Plan de reverse engineering (ver REVERSE_ENGINEERING.md).
- [x] Inventario de herramientas disponibles en este entorno.
- [x] Recibir y revisar los enlaces/recursos pendientes del mantenedor
  (PCGamingWiki, REA, Gex64Decomp-secundaria, speedrun/setup — jerarquía
  fijada en RESEARCH.md §0).
- [x] Crear estructura de carpetas en Drive (originales, backups, análisis…).
- [x] Recibir archivos originales del mantenedor → inventariados en Fase 1
  (ver `docs/ORIGINAL_ARTIFACT_INVENTORY.md`).

## Checklist de la Fase 1 🟡 (en curso)

- [x] Inventariar archivos originales: edición EU v1.00.000 completa
  (1585 ficheros, hashes, TOC, instalador + ejecutable identificados).
  Variantes US/demo/parcheadas: pendientes.
- [~] Documentar la versión PC (ejecutables, dependencias, registro,
  CD-audio): estático hecho; dinámico pendiente.
- [~] Problemas conocidos con evidencia: estática propia añadida;
  primeras observaciones propias (Hito 1: I-15/I-16 parciales,
  I-11 no reproducido, I-21/I-22/I-23 nuevos).
- [~] Analizar fixes existentes: Lote 1 (2026-10-09) con descripciones
  F-01…F-12 verificadas en fuente + revisión cruzada + S-14 (2026-10-09);
  diffs binarios pendientes (Fase 2); Lote 2 (2026-10-09): F-13 oficial
  + F-01/F-03/F-10 profundizados (S-23; binarios pendientes).
- [x] Cubrir FA-01…FA-15 a nivel documental: matriz de cobertura
  (2026-10-09); cobertura exhaustiva CERRADA 2026-10-10 (resto por
  implementación, ROADMAP R→K→P).
- [x] Inventariar S-14 (setup package speedrun): metadatos + hashes +
  inventario nominal (22 entradas) + README leído (2026-10-09, carpeta
  `S-14_extracted`, solo lectura). Análisis binario → Fase 2.
- [~] Preparar/validar entorno de RE (Ghidra o alternativa + proyecto).
  Alternativa estática operativa desde 2026-10-25: `tools/gexmap` +
  `docs/FUNCTION_MAP.md` (2 655 inicios, 5 097 aristas, 132 stubs de import,
  1 113 candidatos por prólogo = HYPOTHESIS). Primera segmentación por
  subsistemas 2026-10-30: 65 inicios en 7 grupos HYPOTHESIS, con evidencia y
  confianza validadas (`docs/SUBSYSTEM_SEGMENTS.md`; 35 ALTA / 22 MEDIA /
  8 BAJA). Traza dinámica del frame step 2026-10-30: 2 326 frames
  instrumentados con breakpoints de hardware, 25 sitios del update medidos
  (23 de 24 callees 1 vez/frame), 19 destinos con semántica mecánica
  (`docs/FRAME_STEP_TRACE.md`, `tools/gexmap/frame.mjs`, `tools/frame-trace/`).
  Rutas de inicialización de DirectSound 2026-10-31 (estático): tres cadenas
  alcanzables desde la entrada (`0x00538551` con dos llamantes) y una cuarta
  sin llamantes (`0x00538FD5` -> `0x00538CB0`); datos en `tools/gexmap/audio.mjs`,
  análisis en `docs/AUDIO_INIT_PATHS.md`. No hay traza: la imagen de referencia
  ya no está en la máquina de análisis.
  Ghidra y decompilación siguen pendientes; el mapa es prerrequisito
  del splitter, no un sustituto.
- [~] Definir entorno de testing en Windows: protocolo + matriz FA +
  evaluación de alternativas + requisitos/checklist (TESTING.md,
  2026-10-09); Hito 1 ejecutado en PC del mantenedor (Win11 64-bit;
  build/GPU/driver pendientes). Protocolo P-F03 (A/B F-03) preparado
  (A1–A4); rama B autorizada y ejecutada parcial
  (42–44 FPS + speedup, observación 2026-10-09); E-1 ejecutado
  (E-1.1 identidad confirmada sha256); E-2 LISTO (método E-2.3
  reparado 2026-10-10, ejecución pendiente PC mantenedor);
  E-3 ejecutado (2026-10-10, estático solo-lectura, sin
  ejecutar/modificar) + M-13 flujo temporal estático (TESTING,
  REVERSE_ENGINEERING) + I-23 causa identificada + R-audio
  confirmado + `tools/m13-core` verificado (120 checks) +
  `tools/glide-shim` v6 setvbuf-comprobado (suites verdes 2026-10-10:
  14+3+74+3+3+17+11 + 71 conductual; V-0 superado PC; suite portable;
  V-1 superado PC 2026-10-10 (GCC 16.1.0, 38/38, F-2 evaluado benigno);
  F-3 H4a verificado PC (mismatch-alias + sin dllexport = 38);
  integrado en árbol, V-1 H4a superado PC (38/38; log y SHA
  archivados 2026-10-11 en `Reports/H4a/`); V-1b superado PC
  2026-10-10
  (cruce vs DLL real 38/38); V-2 SUPERADO PC 2026-10-11 (sobre
  `0c22556`: `W32HARNESS: 14 passed, 0 failed` + `ALL PASS` +
  `SUITE: ALL PASS` + `V2_RC=0`; log SHA-256 `3ff4672a…`);
  P-W5 SUPERADO PC 2026-10-11 (probe aislado de resolución de
  exports: 3/3 hooks → shim, 35/35 forwarders → `glide2x_gex_real`,
  exit 0; sin llamadas y sin Gex); evidencias P-W5 ya archivadas en
  `REZengineered/Reports/` (log SHA-256 `c630759a…`, recomputado
  2026-10-11 sobre la copia archivada: coincide; +
  binario/fuente/lista/informe). Evidencias V-1 H4a/V-1b,
  matrix2 y V-2 archivadas 2026-10-11 en
  `REZengineered/Reports/{H4a,V1b,Matrix2,V2}/` (completo:
  también el `exports_shim.txt` del run H4a, 2026-10-11 —
  SHA-256 `9b9170a6…` —, nunca mezclado con el de V-1b).
  Smoke in-game H4a SUPERADO PC 2026-10-11 (arranque real hasta
  menú, copia aislada, salida 0; registro restaurado). Pendiente:
  tanda instrumentada in-game (60 s; footer/overflow) y estabilidad
  en partida.)
- [x] Referencia externa documentada (2026-10-11): reconstrucción de
  Soul Reaver PS1 (`fmil95/soul-re`, MIT, commit `59d1ee8…`)
  analizada como referencia comparativa del motor (candidato de
  linaje «Gex 2»); informe en
  `docs/research/SOUL_RE_ENGINE_REFERENCE.md` (solo documentación;
  sin verificación en el exe PC; sin cambios de código ni binarios).
- [x] Referencia externa documentada (2026-10-25): decompilación de
  **Gex (1996) para Windows** (`Hydroloxed/gexdecomp`, sin licencia;
  `main` @ `8ba80780`) como referencia secundaria de la familia Gex
  (RESEARCH S-27); sin código copiado, sin binarios y sin trasladar
  direcciones a `GEX3D.EXE`.

## Limitaciones actuales

- **Originales inventariados; copia de trabajo verificada (fuera del
  repo):** `originals/` en Drive (volcado CloneCD + contenido del CD)
  catalogado en `docs/ORIGINAL_ARTIFACT_INVENTORY.md` con hashes;
  copia de trabajo del exe EU en scratch del sandbox (md5
  `692b1282…` verificado; NADA en el repo). Análisis binario
  profundo AUTORIZADO 2026-10-10 (pívot: modificar código,
  investigar dinámicamente incl. I-23, experimentos controlados;
  originales intactos + hashes + copias + restore points).
- **Windows de pruebas parcial:** este entorno (Linux) solo permite análisis
  estático; el PC del mantenedor (Win11 64-bit) ejecutó el Hito 1; build,
  GPU/driver y herramientas de captura siguen pendientes de registrar.
- **Sin Ghidra/JDK/Rizin en el sandbox:** imposible descargarlos aquí
  (egress restringido: sin apt, sin release-assets de GitHub, sin headers
  `-dev`). Documentado y verificado en `docs/TOOLKIT.md`. Ghidra se usará en
  una máquina sin restricciones (`tools/setup-re-env.sh` listo, sin probar).
  El mapa estático de `tools/gexmap` se construyó sin Ghidra (objdump); no hay
  decompilación ni tipos.
- **Mapa estático sin semántica:** los 1 113 inicios por prólogo son hipótesis;
  los tamaños son cotas superiores; el 100 % de cobertura es del union de
  extensiones. La segmentación por subsistemas (2026-10-30) es una primera
  rebanada **HYPOTHESIS** de 65 inicios: 8 son BAJA (7 prólogos), `W=no` no
  prueba código muerto y los nombres no son símbolos originales. No sustituye al
  splitter ni al `symbols.txt` y no cambia el 0,00 % de ByteMatch.
- **REA sin backend nativo:** REA es capa de orquestación/evidencia y necesita
  un proveedor externo (Ghidra/IDA/Hopper) para análisis nativo. En el sandbox
  el CLI se instaló pero `rea doctor` da `healthy:false`. **Ningún binario de
  GEX ha sido analizado con REA.** Estado del host Windows y plan de
  habilitación (pendiente de autorización): `docs/REA_WINDOWS_ENABLEMENT.md`.
- **Audio audible sin confirmación por oído:** el *backend* DirectSound entrega
  PCM al dispositivo y el cursor avanza a la frecuencia del flujo, pero el
  agente no puede escuchar la salida. La confirmación auditiva es del
  mantenedor (`port\build\sagplay.exe --device`). Además el backend es sólo
  Windows y no hay `.sag` reales: todo se valida con fixtures sintéticos.
- **I-5 + R-1 + R-2: infraestructura gráfica sin dibujo.** El renderer real
  todavía no dibuja nada. I-5 documenta evidencia, comparativa de APIs y
  contrato propuesto; R-1 implementa y verifica el **dispositivo D3D11**
  (hardware y WARP, ambos FL 11_1 en esta máquina) y R-2 la **swapchain DXGI**
  sobre el `HWND` de I-4 (bitblt, 2 buffers, 512×384 en la sonda, con
  `ResizeBuffers` y COM verificado). No hay `Present`, ni clear, ni geometría,
  ni shaders, ni texturas, y `gexloop` sigue con el renderer nulo. La capa
  debug D3D11 no está instalada en el PC mantenedor
  (`DXGI_ERROR_SDK_COMPONENT_MISSING`); la verificación de fugas es conductual
  y la prueba admite ambas ramas. Detalle en `docs/D3D11_DEVICE.md`,
  `docs/D3D11_SWAPCHAIN.md` y `docs/RENDERER_ARCHITECTURE_RESEARCH.md`.
- **I-4: ventana real, juego y renderer ausentes.** `gexloop.exe` crea una
  ventana Win32 real, recibe teclado y corre el frame loop con QPC, pero
  `pre_frame`/`update` son stubs y `present` es un renderer nulo que no dibuja.
  La prueba de ventana es programática (`FindWindowEx` + `PostMessage`), no con
  teclado físico ni comprobación visual. La ruta de entrada del original (S5)
  es HYPOTHESIS y la fuente exacta de teclado no está confirmada; el port usa
  mensajes Win32 como decisión explícita. Detalle en
  `docs/WINDOW_INPUT_PORT.md`.

## Criterio de salida de la Fase 0 (cumplido 2026-10-08, histórico)

1. [x] Los recursos pendientes del mantenedor están revisados.
2. [x] Existe al menos un ejecutable original inventariado (hash + versión):
   `GEX3D.EXE` EU v1.00.000.
3. [~] El entorno de análisis estático está operativo (sandbox; Ghidra
   pendiente en máquina sin restricciones).

Fase 1 iniciada por decisión del mantenedor (2026-10-08). El análisis
binario profundo (Ghidra/decompilación) sigue bloqueado hasta que el
mantenedor indique el cambio de fase.
