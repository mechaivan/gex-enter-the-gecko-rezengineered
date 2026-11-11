# ROADMAP

Progresión del proyecto, ordenada por complejidad y dependencias:

```text
ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL
```

Los objetivos de modernización viven en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md)
(M-01…M-26, todos PROPOSED; X-01/X-02 DEFERRED). Este roadmap puede cambiar
si la investigación demuestra que es necesario.

> Ruta corta (2026-10-10): **Hitos R → K → P + Olas 1–3**.
> Las antiguas Fases 4–14 se fusionan abajo; los IDs M-xx/FA-xx NO cambian.
> Hito P = MAIN 100% del panel (sin crear otra definición).

## Fase 0 — Planning / Documentation ✅ (cerrada 2026-10-08)

- [x] Definir objetivos y principios del proyecto.
- [x] Recopilar recursos públicos iniciales.
- [x] Preparar estructura del repositorio y documentación base.
- [x] Fijar jerarquía de referencias y taxonomía (ORIGINAL/F/T/R).
- [x] Definir objetivos futuros M-01…M-24 + FA-01…FA-15 (PROPOSED) + X-01/X-02.
- [x] Cerrar la recopilación de material público pendiente (S-01…S-19).
- Entorno RE y máquina de testing → movidos a Fase 1 (pendientes).

## Fase 1 — Research & Original PC Documentation 🟡 (en curso)

Completado: inventario EU, Hito 1, catálogos F/S (Lotes 1–2, S-14),
E-1/E-1.1, matrices FA/testing. **Catálogos cerrados: no ampliar
F-xx/S-xx sin necesidad de una implementación concreta.**

- [ ] Specs Hito 1 (build/GPU/driver/refresco) — 1 registro.
- [x] Método A/B decidido: E-2.3 (conmutación InstallDir por copia;
  I-23 causa identificada 2026-10-10).
- [ ] E-2 (Tests 1–2; Test 3 condicional) — LISTO (E-2.3),
  pendiente ejecución PC mantenedor.
- [ ] Probar `tools/setup-re-env.sh` en máquina sin restricciones.
- [ ] Cierre Fase 1 → autorización Fase 2.

### Subfase C — Geometría Original (U-1) ✅ (2026-11-04 a 2026-11-06)

- [x] U-1a: Stride y campos (2026-11-04)
- [x] U-1b: Cálculo de w (2026-11-04)
- [x] U-1c: Geometría completa (2026-11-04)
- [x] U-1d: Recuperación de índices (2026-11-04)
- [x] U-1e: Relación geometría-índices (2026-11-04)
- [x] U-1f: Relación índices-recursos (2026-11-05) — corregidas U-1a…U-1e: `0x00432BBD` = glifo 2D, `0x00432DD1` = rectángulo 2D, geometría 3D real en `0x0046558A`
- [x] U-1g: Relación con DFX (2026-11-06) — flujo de comandos, índice de 14 bits → `0x006488F4` (`.VFX`), `.DFX` relocalizable (`0x004735C0`), cadena `0x0043B11D` → `.VFX` + `.DFX`; formatos validados con `GEX.DFX`/`GEX.VFX`
- [x] U-1h: Interpretación final (2026-11-06; **auditada 2026-11-07**) — contenedor `.DFX` (`count` + offsets + imagen en `4*((count+0x200)&~0x1FF)`), descriptor de nivel y encadenado de regiones exacto, nodo BSP `0x20` B (unión interno/hoja con tipo `+0x12` = 1/2), polígono `0x14` B, vértice 12 B, normal 6 B, auxiliar UV 12 B (índice de textura en `word[+6]`), recorrido de dibujo `0x401C00` → `0x4269C2` → emisores, colisión segmento–BSP `0x41A203`, tabla de despacho corregida (11 handlers), anomalías de U-1g resueltas y `0x64FB40` identificado como rectángulo de fondo `0xF0`. Auditoría final en `U1H_RESULTADOS.md` §18: precisados el radio de `0x4024B9`, los índices UV por handler, el rango no-op `0x50..0xB0`, las llamadas Glide de `0x30`, el reencadenado `0x462240..0x4622AB` y la descarga de tabla en `0x2C`; la consulta esférica queda como **hipótesis** de *pickups* (no hay polígonos `0x20` en los tres niveles)

- [x] Consolidación documental de U-1 (2026-11-07) — `U1_CONSOLIDACION.md`:
  estado campo a campo del descriptor (10 demostrados, 1 hipótesis,
  3 sin identificar, 2 parciales), separación de geometría BSP de nivel /
  geometría de modelos / formatos de comandos, corrección del stride de
  modelos (`0x425010` 16 B; `0x4258BD` 12 B) y pendientes expresos
  (`GEX.DFX`, `0x2C`, `0x28` y segundo registro de `0x426588`).

- [x] Auditoría de carga y relocalización `.DFX` (2026-11-08) —
  `U1_DFX_RELOCATION_AUDIT.md`: formato y validación de `AZTEC16.DFX` y
  `GEX.DFX`, funciones `0x00461B90`, `0x00461E0B`, `0x004735C0`, `0x00464FB3`,
  `0x0046506B` y `0x0043B2A9`, algoritmo exacto (`imageBase = B + 4*aligned`,
  `delta = B`, compactación a `B`) y correcciones históricas. La variante
  `0x00461ECA` se analizó el mismo día (§H: `dfx_load_at(path, dest)`, sin
  llamadores estáticos). En ese momento S-1 seguía sin implementar; hoy el
  parser está implementado en `port/` (ver Subfase E).

### Subfase D — Renderer D3D11 (R-1 a R-5) ✅ (completado 2026-11-04)

- [x] R-1: Dispositivo D3D11 (hardware y WARP) (2026-11-02)
- [x] R-2: Swapchain DXGI sobre HWND (2026-11-03)
- [x] R-3: Renderer básico con Present y clear (2026-11-03)
- [x] R-4: Estabilidad y diagnóstico Direct3D 11/DXGI (2026-11-03)
- [x] R-5: Renderer básico con pipeline D3D11 (2026-11-04)

### Subfase E — Primer Escenario Original (S-1) 🔄 EN CURSO

- [ ] S-1: Parser DFX (2026-11-07) — parser del contenedor `.DFX` **implementado y validado** en `port/` ([`S1_DFX_PARSER.md`](S1_DFX_PARSER.md)): contenedor, relocalización y descriptor de nivel sobre AZTEC16.DFX y GEX.DFX. La integración del primer escenario (buffers, texturas y renderer) sigue pendiente; S-1 no se declara completo.
- [ ] S-2: Recorrido BSP (2026-11-07) — **recorrido estructural y visibilidad de cámara implementados y validados** en `port/` ([`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md)): DFS con detección de ciclos, referencias compartidas, límite de hojas y 815 hojas en AZTEC16; la **visibilidad original de cámara y su cadena de orientación (`0x46DD40` → `0x46E643` → `0x405C41`) están implementadas** ([`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md)) y auditadas (889 comprobaciones de cámara + 37 de BSP, 0 fallos; `build.ps1 test` y `make test`), con la aritmética de 32 bits reproduciendo el wrap de x86. La **ruta de emisión de polígonos está implementada en código** (17.ª–18.ª pasadas): módulo puro `port/src/emit/emit.c` + `port/include/gex/emit.h` con `gex_level_emit` y sus fases (`collect`, `project`, `classify`, `subdivide`, `sort`), reproduciendo la proyección exacta de `0x46A1D0`, la niebla `0x64FEA8`/`0x461895`, los umbrales `0x65BF80`/`0x65BF04`, el determinante de `0x46B6B5`, el reparto de color entre `0x426256` y `0x426588` (segunda pasada de niebla), la prioridad de la subdivisión (4 principales + hasta 3 *slivers*, sin niebla), el gate completo de los emisores y la ordenación por `(maxz >> 2) + 4*layer` con desempate LIFO. La 18.ª pasada audita y corrige seis defectos (x/y sin inicializar, aritmética con wrap x86, offsets comprobados, mezcla de color real, subdivisión/emisores/orden verificados y AZTEC16 estructural) y deja `port/tests/test_level_emit.c` en **38 809 comprobaciones**, 0 fallos, limpio en Clang `-O0`/`-O2` y ASan/UBSan. Pendientes: validación dinámica contra el ejecutable y sustituir la aproximación de tablas trigonométricas por valores verificados; S-2 no se declara completo.
- [x] S-3: Generador de Buffers D3D11 (2026-11-07, completado 2026-11-11) — **implementado en dos mitades**: conversión portable `gex_emit_output → gex_geometry` (`port/include/gex/geometry.h`, `port/src/geometry/geometry.c`) y creación/liberación de vertex/index buffers D3D11 (`port/include/gex/render/d3d11_mesh.h`, `port/src/render/d3d11_mesh.c`, Windows-only). Vértice de 28 B con posición, profundidad, color por vértice, UV, textura, capa y flags; deduplicación exacta y determinista; índices correctos para todos los triángulos, subdivisiones y pasadas de niebla; validación de punteros, conteos, multiplicaciones y desbordamientos antes de reservar o crear; `R16_UINT`/`R32_UINT` automático. Pruebas: `test_geometry` **6 963 comprobaciones, 0 fallos** (Clang `-O0`/`-O2` y ASan/UBSan) y `test_d3d11_mesh` **108 comprobaciones, 0 fallos** (WARP + hardware, lectura de vuelta, AZTEC16 real). **No incluye** shaders, `ID3D11InputLayout`, texturas ni `DrawIndexed`: no hay renderizado visible. Detalle en [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §12–§13.
- [ ] S-4: Cargador de Texturas (2026-11-07) - cargar .VFX, construir la tabla de texturas (índice de 14 bits) y crear texturas D3D11. **4A (2026-11-11): el contenedor `.VFX` está demostrado e implementado** (`port/include/gex/vfx.h`, `port/src/asset/vfx.c`, portable): tabla de entradas, offsets y tamaños validados estrictamente (con validación estricta del EOF: la tabla tiene que cubrir el fichero entero), tabla NCC solo para el formato `YIQ_422`, correspondencia DFX→VFX verificada (los 9 609 polígonos de AZTEC16 usan los índices 0..3 y el `.VFX` tiene 260 entradas). **Módulo 4B (2026-11-11) y auditoría correctiva:** el registro VFX es un `GrTexInfo` de Glide 2.x; los identificadores 1, 11 y 12 son **`GR_TEXFMT_YIQ_422`, `GR_TEXFMT_ARGB_1555` y `GR_TEXFMT_ARGB_4444`**; el bloque de 112 B de `+0x14` es una **`GuNccTable`**, no una paleta; y las dimensiones se derivan del LOD (`256 >> lod`) y del aspecto (`8x1`…`1x8`), con el tamaño del bloque coincidiendo en **15 251 de 15 251 registros** de los 37 `.VFX` originales. **Módulo 4C (2026-11-11):** las texturas del `.VFX` ya se suben a D3D11 (atlas + `ID3D11ShaderResourceView` + sampler lineal/WRAP + muestreo en el shader con el índice de 14 bits) y el nivel de AZTEC16 sale **texturizado** en ejecución real con GPU hardware (2 968 + 324 comprobaciones, 0 fallos; suite completa 16/16). **Lo que falta: la cámara real del juego (S-2)**, el *depth buffer* y el blending por polígono del original. Detalle en `docs/VFX_TEXTURE_FORMAT.md` §9 y `docs/D3D11_FIRST_SCENE_PLAN.md` §18.
- [ ] S-5: Integración con Renderer D3D11 (2026-11-07) — integrar módulos anteriores con renderer existente para renderizar AZTEC16 (incluye los shaders y el `ID3D11InputLayout` que el Módulo 3 dejó fuera a propósito). **Parcial (5A, 2026-11-11):** el pipeline de dibujo ya existe (`gex_d3d11_pipeline`: vertex/pixel shader de color por vértice compilados en ejecución con `d3dcompiler_47.dll`, `ID3D11InputLayout` de los 28 B del vértice, estados, viewport y `DrawIndexed`), el renderer R-3 lo usa antes del `Present` y la aplicación muestra un cubo 3D de prueba (`gex_shape_cube`). **Auditoría y primera ejecución real (2026-11-11):** corregida la liberación parcial (la limpieza ya no depende de una bandera), conservado el diagnóstico de error que se perdía al limpiar, añadidos los errores de dibujo al log, corregida la normal sin normalizar del sombreado, añadida la inyección de fallos por etapa del pipeline (287 comprobaciones offscreen con WARP, 0 fallos) y **ejecutada la aplicación real con ventana Win32 y swapchain en GPU real**: 30 frames presentados sin errores y **evidencia visual confirmada por captura de la ventana** (el cubo aparece con sus tres caras). Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` §14–§15. **Falta:** texturas (S-4), materiales NCC, *depth buffer*, blending del original, carga del nivel real en el bucle de frames y `WM_SIZE`.
- [ ] S-5b: Primera geometria real de un nivel (2026-11-11, HECHO) - nuevo modulo portable `gex_scene_level` (`port/include/gex/scene_level.h`, `port/src/scene/scene_level.c`) que encadena `gex_dfx_load` -> `gex_dfx_load_level` -> `gex_bsp_walk` -> `gex_level_emit` -> `gex_geometry_build` sin duplicar implementaciones, con estadisticas y una **vista diagnostica** (matriz sintetica + niebla 0x461895 + encuadre automatico) que **no es la camara original**. La aplicacion acepta `--level <ruta>`; sin el sigue el cubo de prueba. AZTEC16: 815 hojas, 2854 triangulos (1427 de niebla), 4914 vertices, 8562 indices. **Ejecutado con ventana Win32 y GPU real y verificado visualmente** (captura local, no publicada): se ven las estructuras del nivel. Pruebas: `test_scene_level` 8627 comprobaciones, 0 fallos. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` section 16. **Falta:** texturas, depth buffer, blending y la camara real del juego.

## Fase 2 — Reverse Engineering mínimo ⬜ (no iniciada)

Solo Hito K (orden: REVERSE_ENGINEERING.md §3):
- [x] Estático adelantado en Fase 1 (inventario §4.1 + E-1 + E-3/M-13 timing).
- Proyecto Ghidra + naming (`WinMain`, game-loop). Mapa estático
  2026-10-25: `tools/gexmap` identifica inicios confirmados, grafo de
  llamadas e imports, y `WinMain` por la arista desde la entrada PE;
  Ghidra, decompilación y nombres originales siguen pendientes
  (`docs/FUNCTION_MAP.md`). Primera segmentación por subsistemas
  2026-10-30: 65 inicios, 7 grupos HYPOTHESIS, evidencia y confianza
  validadas (`docs/SUBSYSTEM_SEGMENTS.md`); no asigna roles. Traza
  dinámica del frame step 2026-10-30 (breakpoints de hardware, 2 326
  frames): orden del frame y 25 sitios del update medidos, 19 destinos
  con semántica mecánica (`docs/FRAME_STEP_TRACE.md`). Rutas de inicialización de DirectSound
  2026-10-31 (estático, sin traza): `0x00538551` es el único creador vivo
  (2 llamantes conectados a la entrada) y `0x00538FD5` -> `0x00538CB0`
  sigue sin llamantes (`docs/AUDIO_INIT_PATHS.md`). Ruta de audio
  reconstruida el mismo día: `0x00538285` es un decodificador IMA ADPCM
  de 4 bits, `0x005380B3` una bomba de streaming y `0x00538731` el
  reproductor (`docs/AUDIO_MIXER_RE.md`); ya no queda ningún bucle de
  audio sin leer.
- C mínima: timing FA-04/05, ruta/registro FA-11 (I-23 causa
  identificada estática 2026-10-10; dinámica pendiente), init FA-03.
- D mínima: diffs F-02/F-03 vs original (pistas timing).
- Resto de C/D POSPUESTO hasta que una implementación lo pida.

## Fase 3 — Technical Planning ⬜ (puerta del prototipo)

- Soluciones candidatas (solo Hito K) + método (loader/DLL/parche —
  sin presuponer; se decide aquí con evidencia).
- Insumo disponible: `docs/M13_DECOUPLE_PROPOSAL.md` (propuesta M-13;
  no sustituye la decisión de método ni promociona estados).
- Arquitectura mínima para Hito P; resto pospuesto.
- Promover a PLANNED solo los FA/M-xx implicados en Hito P.

## Hitos R → K → P + Olas 1–3 (2026-10-10; sustituyen a Fases 4–14)

### Hito R — Base de ejecución reproducible
- [x] Hito 1 (arranca + jugable Win11+nGlide).
- [ ] Specs + E-2 (método E-2.3 listo; ejecución pendiente).
- Criterio: cualquiera repite arranque y A/B con la hoja TESTING.

### Hito K — Conocimiento mínimo (Fase 2 mínima)
- Criterio: sabemos DÓNDE intervenir (timing, ruta, init) + Fase 3
  decide el método. Nada más se investiga sin pedirlo una
  implementación.

### Hito P — Primer prototipo (= MAIN 100% del panel)
- Una intervención mínima, reversible y documentada sobre copia:
  arranca, cambio observable, resto idéntico, sin regresión.
- Criterio: MAIN-1…MAIN-10 completados (docs/PROJECT_STATUS.md §4.9).

### Ola 1 — Desacoplamiento + base (tras Hito P)
M-13 (+ M-14 solo tras verificar M-13) + enablers mínimos
(M-01/M-02/M-03, M-21 como referencia viva). M-13 va primero porque
sus FA (FA-04/05) salen del camino crítico de Hito K.

### Ola 2 — Display e input
M-04/M-05, M-15…M-17, M-06…M-12. Requieren FA-06/07/13/14; orden por
dependencia real, no por número.

### Ola 3 — Integración y resto
M-18/M-19/M-20, M-22…M-26. X-01/X-02 siguen DEFERRED.

## Vía de implementación nativa (abierta 2026-10-31)

Ruta paralela a Hitos R/K/P, decidida por el mantenedor: implementar ya las
piezas cuyo comportamiento está demostrado por instrucción, sin esperar a la
Fase 3. El port vive en `port/` y cada pieza trae su prueba ejecutable sin el
juego. **No** altera el panel: Hito P y MAIN siguen midiendo la ruta de
mods/parches.

- [x] **I-1 — Lector del contenedor de audio `SLAG`.** Parser `0x0053899C` +
  carga `0x005388AC` transcritos a C11 (`port/src/asset/sag.c`), 29 casos y
  fuzz, 69 305 checks. Doc: `docs/SAG_CONTAINER.md`.
- [x] **I-2 — Ruta de audio (`SLAG` → PCM).** El «mezclador» `0x005380B3`
  resultó ser una **bomba de streaming** y `0x00538285` un **decodificador IMA
  ADPCM de 4 bits**; no hay mezcla de voces en esta ruta. Implementados
  `port/src/audio/adpcm.c` (`0x00538285` + tablas `0x00558138`/`0x00558178`) y
  `port/src/audio/stream.c` (buffer `0x8000`/`0x4000`, bomba `0x005380B3`,
  reproductor `0x00538731`) con *backend* inyectable; 60 052 + 144 checks y demo
  `port/tools/sagplay.c` (tono sintético → `build/tone.wav`). Doc:
  `docs/AUDIO_MIXER_RE.md`. Backend de dispositivo y bucle de bombeo → I-2b.
- [x] **I-2b — Audio audible en el port.** *Backend* de dispositivo con
  DirectSound (la API del original: `dsound.h`/`-ldsound`, sin dependencias
  nuevas; SDL2/SDL3 no está instalado y habría supuesto vendorizar código),
  `port/src/audio/backend_dsound.c` + `port/include/gex/audio/backend_dsound.h`,
  y programación del bombeo con `port/src/audio/sched.c` +
  `port/include/gex/audio/sched.h` (el contrato de los llamantes de
  `0x005380B3`: una llamada por frame tras el update). Volumen de `0x0053886c`
  incluido. Pruebas: `test_audio` 215 checks y `test_device` 50 checks (esta
  última se auto-omite sin tarjeta), con el cursor avanzando a 44 100 B/s y el
  contenido de la mitad leído por `Lock` idéntico al decodificado. Doc:
  `docs/AUDIO_MIXER_RE.md` §16. **Límite:** la audición humana no la verifica
  el agente; `sagplay --device` queda para el mantenedor. El *gate* del cursor
  se respeta (56 diferimientos medidos en una ejecución real).
- [x] **I-3 — Núcleo de tiempo / frame step.** El acumulador de paso fijo se
  movió de `tools/m13-core` al port (`port/src/core/loop_core.c`, una sola
  implementación; el harness del prototipo compila esa fuente y conserva sus
  120 checks) y `port/src/core/frame.c` implementa `gex_frame_run()` con el
  orden medido `pre_frame → update × N → gex_audio_sched_tick → present`, reloj
  inyectado, catch-up acotado (5 pasos, clamp 250 ms), descartes
  contabilizados, reloj invertido recuperado, pausas/reanudación y política de
  errores explícita (incluida corrección: los errores de pre_frame limpian
  acumulador y reanclan reloj, fijada por la regresión T6b). Preset Original
  1:1 (40000 µs, un update por present) y preset desacoplado (16667 µs = 60 Hz,
  la cuantía del sello `16.666` del update). Pruebas: `test_frame` 410 checks
  + `framedemo`; sin ventana, entrada ni render (los tres callbacks son stubs).
  Doc: `docs/FRAME_LOOP_PORT.md`.
- [x] **I-4 — Ventana + entrada + renderer nulo.** Primer proceso nativo:
  `gexloop.exe` abre una ventana Win32 real (clase `GexNativeLoopWindow`,
  960×540), drena mensajes (`WM_KEYDOWN`/`WM_KEYUP`/`WM_KILLFOCUS`/`WM_CLOSE`),
  mantiene estado de teclado con flancos y libera todo al perder foco, sale con
  Escape o al cerrar, y conduce `gex_frame_run()` con
  `QueryPerformanceCounter`. Paso lógico 60 Hz (16 667 µs) y presentación
  objetivo 120 Hz (8 333 µs) con `MsgWaitForMultipleObjectsEx` y
  `timeBeginPeriod(1)` restaurado al salir; medido ~105–108 presents/s,
  59,5–60,0 pasos/s, 0,85–1,93 % de CPU y 0 errores. `present` es un renderer
  nulo que no dibuja; `pre_frame`/`update` son stubs. La evidencia retira
  `0x0043E719` como candidato de entrada (es una máquina de estados de acción
  dentro del update); la ruta de entrada del original es S5 (HYPOTHESIS) y el
  port usa mensajes Win32 como decisión explícita. Pruebas: 826 + 117 + 1 009
  checks nuevos y prueba real de ventana (Escape y `WM_CLOSE`). Doc:
  `docs/WINDOW_INPUT_PORT.md`.
- [x] **I-5 — Investigación y diseño del renderer nativo (sin implementar).**
  Hito de investigación: consolida la evidencia del render original (Glide
  exclusivo, 38 imports censados; `grSstWinOpen` 512×384@75 Hz con 2 buffers
  de color; render condicional `0x0043DC3A` + drenaje doble + `grBufferSwap(3)`
  que bloquea ~23 ms; triángulos en pantalla con `w`, texturas `.3df`/NCC) y
  evalúa D3D 11 vs OpenGL vs capa Glide vs rasterizador software contra siete
  criterios del proyecto. **Recomendación: Direct3D 11** (cabeceras y librerías
  verificadas en ambos toolchains hoy, WARP para pruebas sin GPU, cero
  descargas), condicionada a R-1 (crear dispositivo) como primer paso del hito
  de implementación; OpenGL y el rasterizador C11 quedan como alternativas con
  el mismo contrato. Se define el contrato `gex_renderer` (vtable interna,
  `gex_null_renderer` sigue vivo para las pruebas) y el plan R-1…R-8. **No se
  implementó nada**: sin `d3d11` compilado, sin geometría, sin cambios en
  `frame.c`/`loop_core.c`/audio, sin dependencias ni build. Doc:
  `docs/RENDERER_ARCHITECTURE_RESEARCH.md`.
- [x] **R-1 — Dispositivo Direct3D 11 (hardware y WARP).** Primera pieza del
  renderer real, sin swapchain ni dibujo: `gex_d3d11_device`
  (`port/src/render/d3d11_device.c`, Windows-only) crea el dispositivo con
  `D3D11CreateDevice` en modo `HARDWARE` o `WARP`, registra el feature level
  obtenido (lista explícita 11_1→9_1), entrega el contexto inmediato y libera
  todo COM en `shutdown` idempotente; fallo = struct a cero. **Medido en esta
  máquina: hardware y WARP creados a FL 11_1**; bits reservados →
  `E_INVALIDARG` (0x80070057) con cierre limpio; capa debug ausente →
  `DXGI_ERROR_SDK_COMPONENT_MISSING` (0x887A002D), hallazgo documentado.
  Verificación de fugas conductual (referencia manual + `Release` final a 0)
  porque el runtime mantiene referencias internas al device. Prueba:
  `test_d3d11_device` **314 checks, 0 fallos** con **Clang 22.1.8 y GCC 16.1.0**;
  sonda `d3d11probe.exe`; batería completa **132 298 checks, 0 fallos**. La
  decisión D3D11 de I-5 queda confirmada. Doc: `docs/D3D11_DEVICE.md`.
- [x] **R-2 — Swapchain DXGI sobre la ventana de I-4 (sin presentar).**
  `gex_d3d11_swapchain` (`port/src/render/d3d11_swapchain.c`, Windows-only)
  obtiene la factoría desde el dispositivo de R-1 (`IDXGIDevice →
  IDXGIAdapter → IDXGIFactory`), crea una swapchain **bitblt
  `DXGI_SWAP_EFFECT_DISCARD`** de 2 buffers `R8G8B8A8_UNORM` en modo ventana
  sobre el `HWND` existente, conserva el backbuffer para R-3 y lo renueva en
  cada `ResizeBuffers`. Rechaza tamaños 0 sin tocar DXGI (ventana minimizada),
  admite resize repetido y cierra en orden backbuffer → swapchain → dispositivo.
  **Medido:** dispositivo 2→3→2 referencias al crear/destruir la swapchain;
  swapchain 2 (una la retiene su backbuffer), backbuffer 1, todas a 0;
  `BufferCount = 1000` → `DXGI_ERROR_INVALID_CALL` con struct cerrado;
  `ResizeBuffers` no valida el `HWND` (devuelve `S_OK` tras destruirlo). Prueba:
  `test_d3d11_swapchain` **572 checks, 0 fallos, 0 saltos**, hardware y WARP,
  **Clang 22.1.8 y GCC 16.1.0**; sonda `swapprobe.exe`; batería completa
  **132 870 checks en el cierre histórico de R-2**. En ese momento no había
  `Present`, clear ni dibujo; R-3 añadió posteriormente la primera salida
  gráfica visible. Este dato histórico no describe el estado actual.
- [x] **R-3 — Direct3D 11 renderer con primera salida gráfica visible.**
  Implementa el renderer real con `Present` call, clear del backbuffer y render
  target view. Primera presentación gráfica visible con Direct3D 11. **145 checks,
  0 fallos** con Clang 22.1.8; batería completa **133 160 checks**. Doc:
  `docs/D3D11_RENDERER.md`.
- [x] **R-4 — Investigación de VSync, DXGI, Debug Layer y pérdida del dispositivo.**
  Implementa VSync configurable, Debug Layer, diagnóstico de dispositivo y medición
  del tiempo entre frames. **145 checks** con Clang 22.1.8; batería completa
  **133 160 checks**. Doc: `docs/R4_DIAGNOSTIC.md`. 17e0a0ca49dfbdc4fd45b0948129b7baebfd940d

Orden justificado: I-2 cerró el subsistema ya empezado; I-2b lo hace audible
(backend DirectSound + planificador, 2026-11-01); I-3 conecta ese planificador
al primer bucle temporal nativo (2026-11-01); I-4 cierra el primer proceso
ejecutable con ventana, teclado y reloj real, sin renderizado (2026-11-02); I-5
investiga y diseña el renderer real sin implementarlo (2026-11-02); R-1 verifica
la decisión con el dispositivo D3D11 real, hardware y WARP, sin swapchain ni
dibujo (2026-11-02); R-2 conecta ese dispositivo con la ventana de I-4 mediante
una swapchain DXGI (R-2) y añadió después el renderer con primera salida visible (R-3, 2026-11-03). La
llamada `gex_audio_sched_tick(&sched)` ya está dentro del frame loop, una vez
por frame presentado y tras el update (`port/src/core/frame.c`); I-4 rellena
`pre_frame`/`update`/`present` con stubs y un renderer nulo, y lee el reloj real.
El renderizado y la lógica de juego quedaban para hitos posteriores; I-4 no los
adelanta. (Estado actual: R-3/R-4/R-5 completaron el pipeline D3D11 básico; el
Módulo 3 añadió después la geometría del escenario —conversión portable y
vertex/index buffers reales—, y el Módulo 5A el pipeline de dibujo con shaders
propios, con un cubo 3D de prueba visible en la ventana; siguen pendientes las
texturas, los materiales, el *depth buffer* y la carga del nivel real; la
lógica de juego no ha empezado.)

## Publicación pública (hitos transversales ⬜ — no iniciados)

> Secuencia de publicación futura, en orden de prioridad actual. Nada
> completado; no publicar hasta cerrar la revisión de distribución.

- [ ] Documentación y diagnóstico: registro de compatibilidad + pruebas.
- [ ] Diseño técnico: método de modernización documentado (Fase 3+), sin
  distribuir materiales protegidos.
- [ ] Prototipo interno: comprobar con copia legítima, originales intactos
  cuando el diseño lo permita.
- [ ] Pruebas y recuperación: instalación, funcionamiento, copias,
  restauración, desinstalación.
- [ ] Revisión de distribución: repo, historial, dependencias, licencias y
  método de modificación (ver LICENSE.md §5–§6).
- [ ] Preparación pública: avisos, instrucciones y docs antes de anunciar
  en GitHub, foros o Discord.

---

Leyenda: ✅ completada · 🟡 en curso · ⬜ pendiente