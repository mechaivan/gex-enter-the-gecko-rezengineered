# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE started (Etapa A documented in Phase 1; bounded static E-3/M-13 + Línea A/B executed read-only 2026-10-10 under explicit authorization - no Ghidra/decomp; static function map + call graph + per-function imports on the EU exe 2026-10-25, `docs/FUNCTION_MAP.md`; first evidence-based subsystem segmentation 2026-10-30, `docs/SUBSYSTEM_SEGMENTS.md`; dynamic frame-step trace with hardware breakpoints 2026-10-30, `docs/FRAME_STEP_TRACE.md`; static DirectSound init routes 2026-10-31, `docs/AUDIO_INIT_PATHS.md`; U-1f completada 2026-11-05 con corrección de U-1a…U-1e, `U1F_RESULTADOS.md`; U-1g completada 2026-11-06, `U1G_RESULTADOS.md`)
**Implementation:** Started 2026-10-31 — native port skeleton in `port/` (C11, no dependencies): reader for the `SLAG` audio container (`0x0053899C` + loader `0x005388AC`), 29 test cases + fuzz, 69 305 checks, `docs/SAG_CONTAINER.md`; **audio route** implemented the same day (`0x00538285` IMA ADPCM decoder + `0x005380B3` streaming pump + `0x00538731` player) with an injectable backend, 60 052 + 144 checks, `docs/AUDIO_MIXER_RE.md`; **audible audio** added 2026-11-01 (I-2b): real DirectSound backend (`dsound.h`/`-ldsound`, no new dependency) and the frame-loop scheduler, 215 + 50 checks including the device suite, `docs/AUDIO_MIXER_RE.md` §16; **native frame loop** added 2026-11-01 (I-3): the M-13 accumulator moved into `port/src/core/loop_core.c` (single implementation, the prototype harness now compiles it), `port/src/core/frame.c` reproduces the measured order pre-frame → update × N → audio scheduler → present, 410 checks (incluida la regresión T6b del error de `pre_frame`) + `framedemo`, `docs/FRAME_LOOP_PORT.md`; **first native process** added 2026-11-02 (I-4): real Win32 window, `WM_KEYDOWN`/`WM_KEYUP` routing, focus loss, `QueryPerformanceCounter` clock and the I-3 frame loop behind a null renderer (`gexloop.exe`), 826 + 117 + 1009 new checks and a real-window run (Escape and `WM_CLOSE`, 0 errors), `docs/WINDOW_INPUT_PORT.md`; **renderer research & design** added 2026-11-02 (I-5): original Glide surface fully censused (38 imports), present sequence confirmed, alternatives evaluated (D3D 11 recommended over OpenGL/Glide-layer/software, conditioned on device-creation test R-1), `gex_renderer` contract proposed with the null renderer kept for tests, validation plan R-1…R-8; `docs/RENDERER_ARCHITECTURE_RESEARCH.md`; **D3D11 device** added 2026-11-02 (R-1): `gex_d3d11_device` creates/destroys the device in hardware and WARP modes (both reached feature level 11_1 on the maintainer machine), 205 new checks with Clang 22.1.8 and GCC 16.1.0, leak check by manual-refere... (line truncated to 2000 chars)
**Testing:** Hito 1 + 5ª–9ª sesión (2026-10-09, PC mantenedor; nGlide 2.10, ≈25 FPS Steam, wrapper = nGlide 2.10 CONFIRMADO por payload-hash, imports solo-sistema/backend dinámico; A2 46%, global 20%; resto pendiente) + smoke in-game H4a (2026-10-11, PC mantenedor: arranque real hasta menú; registro restaurado)
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-11-11 (Módulo 5A)

### Auditoría: carga y relocalización `.DFX` (2026-11-08)

- **Documento:** [`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md), referencia técnica principal de la mecánica del contenedor.
- **Método:** lectura estática del desensamblado + verificación byte a byte de `AZTEC16.DFX` y `GEX.DFX`. Sin ejecución, sin modificar `C:\GEX_REZ` ni el port.
- **Hechos fijados:**
  - `aligned = (count + 0x200) & ~0x1FF` en DWORDs; imagen en `4*aligned`; relleno intermedio a cero.
  - `0x004735C0(container*, imageBase, delta)` hace `*(u32*)(imageBase + off) += delta`.
  - La llamada real usa `imageBase = B + 4*aligned` y **`delta = B`**.
  - Compactación de `4*(roundUp4(tamaño)/4 − aligned)` bytes desde `B + 4*aligned` hasta **`B`**; libera `4*aligned` y devuelve `B`.
- **Cifras verificadas:** AZTEC16 `count` 16 082, imagen en `0x10000`, 479 050 B (copia 479 052 B); GEX `count` 322, imagen en `0x800`, 143 960 B.
- **Corrección de transcripción:** `0x00461E62` recarga `ECX = aligned`; el cálculo de `imageBase` nunca dependió de `count`.
- **Pendientes:** thunks de E/S, generalización de `[0x647FAC]`, llamadas
  calculadas (variante `0x00461ECA` ya analizada: `dfx_load_at(path, dest)`,
  sin llamadores estáticos) y direcciones absolutas en ejecución.
- **S-1 (parser DFX):** implementado y documentado en `port/` ([`S1_DFX_PARSER.md`](S1_DFX_PARSER.md)); validado con AZTEC16 y GEX.DFX. No se declara S-1 completo mientras no se cierre la integración del primer escenario.
- **S-2 (recorrido BSP + visibilidad de cámara):** implementado y documentado en `port/` ([`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md)); auditado (7.ª–18.ª pasada; la 11.ª solo documental, la 13.ª corrige el código de la 12.ª, la 14.ª la aritmética de 32 bits y la 18.ª la emisión): **37 comprobaciones de BSP, 889 de cámara y 38 809 de emisión, 0 fallos**, ASan/UBSan limpios, AZTEC16 = exactamente 815 hojas. La cadena de orientación (`0x46DD40` → `0x46E643` → `0x405C41`, rotaciones Euler Z→Y→X con ángulos crudos, `M = transpose(O)`) y la prueba de esfera contra seis semiplanos de `0x401C00` están implementadas en `port/src/camera/camera.c` ([`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md)) e integradas en `gex_bsp_walk` mediante `cam_ctx` opcional. Toda la aritmética de 32 bits reproduce el wrap de x86 sin desbordamiento con signo ni conversiones de implementación definida (acumuladores `uint32_t`, `sar 12` sobre los bits del registro, copias de bits para los campos crudos). `test_camera` y `test_bsp` están integrados en `build.ps1 test` y en `make test`. Limitaciones declaradas: tablas trigonométricas aproximadas (equivalencia bit a bit no demostrada), B fila 2 (`cam+0xB4..0xB8`) preservada por decisión del port y **sin validación dinámica** contra el ejecutable. La **emisión de polígonos está implementada en código** (2026-11-10/11, 17.ª–18.ª pasadas): módulo puro `port/src/emit/emit.c` + [`port/include/gex/emit.h`](port/include/gex/emit.h) con `gex_level_emit` y sus fases separadas (`collect`, `project`, `classify`, `subdivide`, `sort`), reproduciendo la proyección exacta de `0x46A1D0`, la niebla `0x64FEA8`/`0x461895`, los umbrales `0x65BF80`/`0x65BF04`, el determinante de `0x46B6B5`, el reparto de color entre `0x426256` y `0x426588` (segunda pasada de niebla), la prioridad de la subdivisión (cuatro principales + hasta tres *slivers*, sin niebla), el gate completo de los emisores y la ordenación por `(maxz >> 2) + 4*layer` con desempate LIFO. La 18.ª pasada corrige los seis defectos detectados (x/y sin inicializar, aritmética con wrap x86, offsets comprobados, mezcla de color real, subdivisión/emisores/orden verificados y AZTEC16 estructural) y deja `test_level_emit` en **38 809 comprobaciones**, 0 fallos, limpio en Clang `-O0`/`-O2` y ASan/UBSan. S-2 **no** se declara completo: falta la validación dinámica contra el ejecutable original y la integración con el renderizador (Módulo 3). El contrato revisado está en [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §11.
- **Módulo 3 (2026-11-11): implementado en dos mitades.** La conversión
  portable `gex_emit_output → gex_geometry` vive en
  [`port/include/gex/geometry.h`](port/include/gex/geometry.h) +
  [`port/src/geometry/geometry.c`](port/src/geometry/geometry.c) y no depende de
  Windows ni de D3D11; la creación de `ID3D11Buffer` de vértices e índices vive
  en [`port/include/gex/render/d3d11_mesh.h`](port/include/gex/render/d3d11_mesh.h)
  + [`port/src/render/d3d11_mesh.c`](port/src/render/d3d11_mesh.c) y es
  Windows-only. Vértice de 28 B con posición, profundidad, color por vértice,
  UV, textura, capa y flags; deduplicación exacta y determinista; triángulos
  clipeados por profundidad descartados y contados; índices `R16_UINT` o
  `R32_UINT` según el conteo de vértices. Pruebas: `test_geometry` **6 963
  comprobaciones, 0 fallos** (Clang `-O0`/`-O2` y ASan/UBSan) y
  `test_d3d11_mesh` **108 comprobaciones, 0 fallos** con WARP y con hardware
  (feature level 11_1), incluida lectura de vuelta de ambos buffers y
  AZTEC16 real (2 242 triángulos, 3 918 vértices, 6 726 índices, 109 704 B +
  13 452 B). No hay shaders, ni `ID3D11InputLayout`, ni texturas, ni
  `DrawIndexed`: **los buffers existen y son verificables, pero no hay
  renderizado visible**. Detalle en
  [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §12 y §13.
- **Módulo 5A (2026-11-11): pipeline de dibujo D3D11.** Implementado en
  [`port/include/gex/render/d3d11_pipeline.h`](port/include/gex/render/d3d11_pipeline.h)
  + [`port/src/render/d3d11_pipeline.c`](port/src/render/d3d11_pipeline.c)
  (Windows-only): vertex shader y pixel shader mínimos de color por vértice
  compilados **en ejecución** con `d3dcompiler_47.dll` (componente de Windows;
  no hay `fxc.exe` en el entorno y no se instala nada), `ID3D11InputLayout` que
  refleja los 28 B y los offsets publicados de `gex_vertex`, estados de
  rasterizado/blending/profundidad, viewport, constant buffer y `DrawIndexed`.
  El renderer R-3 acepta una geometría opcional
  (`gex_d3d11_renderer_set_geometry`, punteros prestados) y la dibuja entre el
  `ClearRenderTargetView` y el `Present`; sin geometría su comportamiento es
  el de siempre. La geometría de prueba es un cubo 3D
  ([`port/include/gex/geometry_shapes.h`](port/include/gex/geometry_shapes.h),
  portable) que la aplicación (`gexloop`) muestra en la ventana. Pruebas:
  `test_d3d11_pipeline` **137 comprobaciones, 0 fallos** (offscreen con WARP,
  sin ventanas) y `test_geometry_shapes` **355 comprobaciones, 0 fallos**
  (portable, con ASan/UBSan). Dos hallazgos documentados: el enlazador D3D11
  exige que el pixel shader reciba la **misma estructura** que el vertex shader
  declara como salida (con un parámetro suelto recibía SV_POSITION), y
  `D3D11_INPUT_ELEMENT_DESC` no tiene campo `Offset` (el byte offset va en
  `AlignedByteOffset`). Detalle en
  [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §14.
- **Pendiente de integración:** cargador de texturas VFX (Módulo 4), materiales,
  *depth buffer*, blending del original y la carga del nivel real en el bucle
  de frames. El primer escenario original todavía no está renderizado. Sigue
  pendiente la validación dinámica de S-2 contra el ejecutable original.
  `PROJECT_STATUS.svg` no cambia (MAIN 40 %).

### Planificación: Primer Escenario Original en Direct3D 11 (2026-11-07)

- **Objetivo:** Determinar el camino mínimo para transformar geometría BSP de niveles en datos renderizables por el port D3D11.
- **Nivel de prueba:** AZTEC16.DFX (el más completo y mejor documentado).
- **Arquitectura propuesta:** 5 módulos (Parser DFX, Recorrido BSP, Generador de Buffers, Cargador de Texturas, Integración Renderer).
- **Hitos definidos:**
  1. Parser DFX Operativo - leer AZTEC16.DFX y extraer descriptor
  2. Recorrido BSP Funcional - implementar algoritmo de `0x401C00`
  3. Buffers D3D11 Generados - convertir geometría a buffers D3D11
  4. Texturas Cargadas - cargar .VFX y crear texturas D3D11
  5. Primer Renderizado Completo - renderizar AZTEC16 con D3D11
- **Bloqueos reales:** Shaders (no documentados en U-1), formato NCC (conversión a D3D11), sistema de coordenadas.
- **Estado de los hitos (2026-11-11):** Hito 1 (parser DFX) implementado y validado; Hito 2 (recorrido BSP) con recorrido estructural, visibilidad original de cámara y **emisión de polígonos** implementados y auditados (889 comprobaciones de cámara + 37 de BSP + 38 809 de emisión, 0 fallos); Hito 3 (buffers D3D11) **implementado**: conversión portable (`gex_geometry`) y creación real de vertex/index buffers (`gex_d3d11_mesh`) con WARP y hardware, 6 963 + 108 comprobaciones, 0 fallos; Hito 5 **parcial**: el Módulo 5A añade el pipeline de dibujo real (shaders, input layout, estados, `DrawIndexed`) y una geometría 3D de prueba visible en la ventana, 137 + 355 comprobaciones, 0 fallos. Quedan la validación dinámica de S-2 contra el ejecutable original, el Hito 4 (texturas) y el resto del Hito 5 (materiales, profundidad, carga del nivel). La mecánica de carga está documentada en `U1_DFX_RELOCATION_AUDIT.md`.
- **Evidencia clave:** `U1_DFX_RELOCATION_AUDIT.md` (carga/relocalización verificada), `U1_CONSOLIDACION.md` (geometría BSP demostrada), código R-1 a R-4 (renderer base), AZTEC16.DFX (caso de prueba completo).

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

- **U-1h (interpretación final):** COMPLETADO y AUDITADO (2026-11-06;
  auditoría final 2026-11-07) — contenedor `.DFX`
  (`count` + offsets + imagen en `4*((count+0x200)&~0x1FF)`) y descriptor de
  nivel con encadenado exacto de regiones; nodo BSP `0x20` B (unión interno/hoja,
  tipo `+0x12` = 1/2); polígono `0x14` B, vértice 12 B, normal 6 B y auxiliar UV
  12 B (índice de textura en `word[+6]`); recorrido de dibujo `0x401C00` →
  `[0x64FEA4]` → `0x4269C2` → `0x426256`/`0x426588` → `0x469836` → `0x46558A`;
  colisión segmento–BSP `0x41A203`; tabla de despacho corregida (11 handlers);
  anomalías de U-1g resueltas y `0x64FB40` identificado como rectángulo de fondo
  `0xF0`. La consulta `0x4024B9` + `0x5015A9` marca polígonos `0x20`; su uso como
  *pickups*/triggers es **hipótesis** (no hay polígonos `0x20` en los tres
  niveles). `GEX.DFX` es un modelo/personaje, no un nivel. Evidencia completa:
  [`U1H_RESULTADOS.md`](U1H_RESULTADOS.md) (§18: auditoría final).

- **Consolidación documental (2026-11-07):** `U1_CONSOLIDACION.md` fija el
  estado campo a campo del descriptor (10 demostrados, 1 hipótesis,
  3 sin identificar, 2 parciales), separa geometría BSP de nivel /
  geometría de modelos / formatos de comandos y registra los pendientes
  expresos: `GEX.DFX` sin descodificar, emisor del opcode `0x2C`, layout de
  `grDrawPlanarPolygonVertexList` para `0x28` y propósito del segundo
  registro de `0x426588`.

- **Ruta de emisión de polígonos (2026-11-10, solo documentación):**
  `S2_BSP_TRAVERSAL.md` §5 reconstruye con evidencia estática el tramo
  `0x43DC3A` → `0x43DED0` → `0x4269C2` → `0x426256`/`0x426588` → buckets →
  `0x469836` → `0x46558A`: cinco argumentos de llamada, contrato de retorno
  (cursor final), buffer de comandos frente a bucket array, formatos de
  polígono/vértice/UV/comando, proyección a píxeles de 10 bits, subdivisión
  en cuatro triángulos, niebla, *culling* 2D por determinante y orden de
  buckets `(z>>2) + 4*capa`. Se corrige que la capacidad se reserva por
  polígono (`0x34` B), no por comando. **Estado posterior (2026-11-11):** esa
  ruta ya está implementada en `gex_level_emit` y consumida por el Módulo 3; lo
  que sigue faltando es la validación dinámica contra el ejecutable. Detalle en
  `U1H_RESULTADOS.md` §20 y en `docs/D3D11_FIRST_SCENE_PLAN.md` §11–§13.

### Pendientes

- **Sin bloques de U-1:** Subfase C cerrada documentalmente. Quedan campos
  y ramas marcados como abiertos en U-1h §13.3/§14 y en
  `U1_CONSOLIDACION.md` §6: consumidores de `desc+0x10/+0x14`, `desc+0x30`,
  `desc+0x34` y `desc+0x3C`; estado de `desc+0x38` (hipótesis); bits de
  flags sin consumidor; emisor del opcode `0x2C`; layout de `0x28`;
  propósito del segundo registro de `0x426588` y formato del modelo
  `GEX.DFX`. Su cierre, si se autoriza ejecución, exige instrumentación
  dinámica.
- **Semántica de juego de la consulta esférica:** `0x4024B9` + `0x5015A9` está
  demostrada, pero no hay polígonos `0x20` en `AZTEC16`, `CIRCUIT0` ni `REZOP2`;
  confirmar si son *pickups*/triggers exige un nivel con esos polígonos o una
  traza en ejecución.
- **Abiertas de U-1f:** `0x004338D7` (rama `estilo & 0x80`), layout del vértice de
  rectángulo 2D y layout del vértice de `grDrawPlanarPolygonVertexList`
  (resuelto el consumidor, no el layout completo).
- **Carga `.DFX` (auditoría 2026-11-08):** la variante `0x00461ECA` ya está
  analizada (firma `dfx_load_at(path, dest)`, sin llamadores estáticos en este
  build, copia truncada). Quedan abiertas la identidad exacta de los thunks de
  E/S, la generalización de `[0x647FAC]` fuera de esta ruta, el propósito de la
  variante y la constante `0x801FC000`, y las direcciones absolutas reales en
  ejecución. Detalle en
  [`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md) §F y §H.

## Estado vigente del port gráfico (R-1 a R-5)

R-3, R-4 y R-5 están **COMPLETADOS**: implementación básica del renderer D3D11 con device, swapchain, Present, clear y render target view. Pruebas verificadas en modo WARP con 104 checks, 0 fallos. **Geometría (Módulo 3, 2026-11-11):** existen ya vertex/index buffers reales —`gex_geometry` + `gex_d3d11_mesh`— con vértices de 28 B (posición, profundidad, color, UV, textura, capa, flags), creados con WARP y con hardware y verificados por lectura de vuelta. **Dibujo (Módulo 5A, 2026-11-11):** el renderer dibuja esa geometría con un vertex/pixel shader propios, `ID3D11InputLayout`, estados y `DrawIndexed`; la aplicación muestra un cubo 3D de prueba. Lo que sigue sin existir es el material de producción: texturas, *depth buffer*, blending del original y la carga del nivel real, así que **la geometría visible es de prueba, no una escena del juego**.