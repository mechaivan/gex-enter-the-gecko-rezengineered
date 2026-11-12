# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE started (Etapa A documented in Phase 1; bounded static E-3/M-13 + Línea A/B executed read-only 2026-10-10 under explicit authorization - no Ghidra/decomp; static function map + call graph + per-function imports on the EU exe 2026-10-25, `docs/FUNCTION_MAP.md`; first evidence-based subsystem segmentation 2026-10-30, `docs/SUBSYSTEM_SEGMENTS.md`; dynamic frame-step trace with hardware breakpoints 2026-10-30, `docs/FRAME_STEP_TRACE.md`; static DirectSound init routes 2026-10-31, `docs/AUDIO_INIT_PATHS.md`; U-1f completada 2026-11-05 con corrección de U-1a…U-1e, `U1F_RESULTADOS.md`; U-1g completada 2026-11-06, `U1G_RESULTADOS.md`)
**Implementation:** Started 2026-10-31 — native port skeleton in `port/` (C11, no dependencies): reader for the `SLAG` audio container (`0x0053899C` + loader `0x005388AC`), 29 test cases + fuzz, 69 305 checks, `docs/SAG_CONTAINER.md`; **audio route** implemented the same day (`0x00538285` IMA ADPCM decoder + `0x005380B3` streaming pump + `0x00538731` player) with an injectable backend, 60 052 + 144 checks, `docs/AUDIO_MIXER_RE.md`; **audible audio** added 2026-11-01 (I-2b): real DirectSound backend (`dsound.h`/`-ldsound`, no new dependency) and the frame-loop scheduler, 215 + 50 checks including the device suite, `docs/AUDIO_MIXER_RE.md` §16; **native frame loop** added 2026-11-01 (I-3): the M-13 accumulator moved into `port/src/core/loop_core.c` (single implementation, the prototype harness now compiles it), `port/src/core/frame.c` reproduces the measured order pre-frame → update × N → audio scheduler → present, 410 checks (incluida la regresión T6b del error de `pre_frame`) + `framedemo`, `docs/FRAME_LOOP_PORT.md`; **first native process** added 2026-11-02 (I-4): real Win32 window, `WM_KEYDOWN`/`WM_KEYUP` routing, focus loss, `QueryPerformanceCounter` clock and the I-3 frame loop behind a null renderer (`gexloop.exe`), 826 + 117 + 1009 new checks and a real-window run (Escape and `WM_CLOSE`, 0 errors), `docs/WINDOW_INPUT_PORT.md`; **renderer research & design** added 2026-11-02 (I-5): original Glide surface fully censused (38 imports), present sequence confirmed, alternatives evaluated (D3D 11 recommended over OpenGL/Glide-layer/software, conditioned on device-creation test R-1), `gex_renderer` contract proposed with the null renderer kept for tests, validation plan R-1…R-8; `docs/RENDERER_ARCHITECTURE_RESEARCH.md`; **D3D11 device** added 2026-11-02 (R-1): `gex_d3d11_device` creates/destroys the device in hardware and WARP modes (both reached feature level 11_1 on the maintainer machine), 205 new checks with Clang 22.1.8 and GCC 16.1.0, leak check by manual-refere... (line truncated to 2000 chars)
**Testing:** Hito 1 + 5ª–9ª sesión (2026-10-09, PC mantenedor; nGlide 2.10, ≈25 FPS Steam, wrapper = nGlide 2.10 CONFIRMADO por payload-hash, imports solo-sistema/backend dinámico; A2 46%, global 20%; resto pendiente) + smoke in-game H4a (2026-10-11, PC mantenedor: arranque real hasta menú; registro restaurado)
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-11-13 (secuencia de cámara hasta el menú principal: capturada y documentada en `S3_MENU_CAMERA_SEQUENCE.md`)

### Validación dinámica de S-2 contra el ejecutable original (2026-11-11)

- **Método:** copia de trabajo aislada del juego (el `GEX3D.EXE` original +
  `glide2x` real + `LEVEL`/`AUDIO`); `C:\GEX_REZ` intacto y solo lectura.
  Trazador propio con puntos de interrupción de **hardware** y volcado de
  memoria de solo lectura (`tools/frame-trace`); sin parchear el ejecutable.
  AZTEC16 forzado como `LEVEL\MAP7.DFX` (copia fuera de `C:\GEX_REZ`) y guion
  de teclas hasta la ruta 3D.
- **Lo observado en el proceso vivo:** 1 recorrido BSP (`0x401C00`), 1 emisión
  (`0x4269C2`) y ~1517 polígonos dibujados por frame; **307** hojas visibles
  en el primer frame medido; cámara completa en `0x0065CD80` (posición,
  Euler, marco local, matrices A/B/M, a_cam/b_cam, base).
- **Tres defectos reales del port, corregidos y con regresión:**
  (a) los ángulos Euler se **niegan** (`0x46DE51/0x46DE67/0x46DE7D`);
  (b) `M2` se aplica a las **columnas** de la matriz de rotación, no a las
  filas (`0x46E038..0x46E11D`); (c) la tabla trigonométrica del original
  **trunca**, no redondea (`0x00646C20`: 1024/1024 coinciden con el truncado,
  502 difieren del redondeado).
- **Resultado:** tras la corrección el port reproduce **bit a bit** el marco
  local, M, A, B, a_cam y b_cam del original para esa cámara.
  Regresión en `port/tests/test_camera.c` (`test_dynamic_reference_camera`,
  `test_table_truncation`). Detalle en
  [`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md) § 14.
- **El nivel real (2026-11-11, 2.ª tanda):** el juego carga
  `C:\GEX_REZ\GEX2\level\map7.dfx` (ruta capturada en vivo en el cargador
  `0x461E0B`), **no** `AZTEC16.DFX`. La discrepancia «307 hojas frente a 0» era
  una comparación entre niveles distintos.
- **Resultado:** con el nivel correcto, el port reproduce **18/18 frames**
  consecutivos con el **conteo y la lista completa de hojas visibles**
  idénticos al original, y el `root_bsp` del port (`0x9CE94`) coincide
  exactamente con el nodo raíz que el original entrega al recorrido
  (`desc[0] = B + 0x9CE94`). Regresión en `port/tests/test_bsp.c` +
  `port/tests/ref_map7_frames.h`.
- **Emisión (2026-11-11, 3.ª y 4.ª tanda):** capturado el contexto real de
  emisión del original (`0x65CD60` = la `O` escalada por 1,6, traslación en
  `+0x14`, focal/cx/cy/near_z, LUT y umbrales de niebla) y reconstruida la
  rama de subdivisión. El port reproduce la emisión de nivel **triángulo a
  triángulo**: mismas 1123 capturas con idénticas profundidades (3429/3429),
  **mismo orden de dibujo** (1123/1123) y mismas fases de subdivisión (27
  polígonos, 25 abortos, 39 subtriángulos intentados).
- **Corrección de la 3.ª tanda:** la profundidad de referencia del original es
  la **máxima** de las tres (`0x426CC1..0x426D06`), no la mínima; la pasada
  anterior lo leyó al revés e introdujo una regresión en el orden de dibujo.
  Y en la subdivisión **solo `det2d >= 0` aborta**, y solo en los tres
  primeros subtriángulos.
- **Lo que sigue abierto:** el emisor lejano (`0x426588`), que en MAP7 no se
  usa nunca (`[0x65BF04] = 0xFFFF`) y cuya ruta (doble registro `0x34` +
  `0x24`) está implementada en el port pero **sin validación dinámica**; la
  comparación numérica de los colores con niebla; las ramas alternativas
  (`[0x551644+0xa0] & 1` y `& 0x1000`), que no se pueden forzar de forma no
  invasiva; y el detalle interno de la tabla de buckets (0xC00 entradas).
- **Materiales de MAP7 (2026-11-11):** el estado de material por
  polígono queda reconstruido y verificado contra el original en
  ejecución: el índice de textura (la mitad alta del **segundo** dword
  del registro UV, que el port leía del primero y por tanto
  seleccionaba **texturas equivocadas** — 4 en lugar de 62), el modo de
  mezcla por polígono (solo las texturas ARGB_4444 se mezclan con alfa;
  las demás son opacas), el color de niebla (el general del frame:
  B=0x14, G=0x14, R=0x5A) y la niebla antes de la textura. Con ello el
  port reproduce los **1123 colores por vértice** del original de
  forma **exacta** (la prueba exige la coincidencia exacta, no una
  coincidencia parcial).
- **Cierre de auditoría de materiales (2026-11-11, registro histórico superado
  el 2026-11-12):** la conclusión de que la ecuación seguía pendiente era
  correcta en esa fecha, pero quedó sustituida por la medición dinámica de
  nGlide 2.10 descrita a continuación. No debe leerse como estado actual.
- **Ecuación de mezcla alfa en nGlide 2.10 (2026-11-12):** el *harness* i686
  obtiene dinámicamente el programa de `grAlphaCombine(3,4,1,0,0)`:
  `r9.a = texel.a × v0.a`. La captura posterior de 6 000 vértices de MAP7
  demuestra que el vértice entregado a Glide lleva solo tres flotantes B/G/R,
  sin canal alfa; el alfa efectivo de la unidad de mezcla es constante 1.0.
  Por tanto, en MAP7 la salida es **`texel.a × 1.0 = texel.a`**. El juego
  empareja ARGB_4444 con `(1,5,4,0)` → **`SRC_ALPHA`/`INV_SRC_ALPHA`,
  alfa `ONE`/`ZERO`**. El renderer fue corregido para devolver el alfa del
  texel, sin multiplicarlo por el byte alto del opcode; la regresión añadida
  falla con el comportamiento anterior. No se afirma que nGlide sea el
  hardware SST-1 ni equivalencia píxel a píxel. Detalle en
  [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) § 19.8
  y § 19.11.
- **Modos de mezcla por comando y byte alto del color de vértice (2026-11-12):**
  dos puntos que quedaban abiertos de la auditoría de materiales, **cerrados
  con captura dinámica** del juego original en ejecución (trazador del repo con
  breakpoints de hardware, sobre una **copia de trabajo** del exe, md5
  verificado, sin tocar un byte):
  - **Modos de mezcla**: MAP7 usa **solo tres** de las seis cuádruplas
    posibles: `(1,5,4,0)` para la geometría del nivel, `(1,4,4,0)` para **6**
    triángulos (los de la textura 370) y `(4,0,0,0)` para las 128 primitivas
    2D. Los modos **aditivo** y **de borrado no aparecen nunca** (0 de 160 086
    llamadas). El estado global del port coincide en todo salvo esos 6
    triángulos; el diseño de la corrección (modo por triángulo, tramos de
    índices) queda cerrado y **no se aplica en esta ronda** (0,5 % de la
    escena). Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 19.10.
  - **Byte alto del color de vértice**: el vértice que el juego entrega a Glide
    **no tiene canal alfa** (el color viaja en tres flotantes B, G, R;
    6 000 vértices volcados en ejecución). El byte alto del color del comando
    (`0x34`/`0xB4`/`0x36`/`0xB6`) es el **opcode**, no un alfa. El alfa de la
    unidad de mezcla es una **constante 1.0**, y el port multiplicaba el
    alfa del texel por ese byte (0xB4 → 0,706): **corregido** (el alfa de
    salida es el del texel), con prueba de regresión que falla con el
    comportamiento anterior. Detalle en § 19.11.
  Queda **pendiente** la **escala del color**: los tres flotantes del
  vertice estan en el espacio 0..2 de Glide (2 x el byte) y el port
  normaliza a 0..1; no se ha medido si el driver compensa esa escala
  al final (docs/D3D11_FIRST_SCENE_PLAN.md 19.11).
- **Secuencia de cámara hasta el menú (2026-11-13):** investigada con
  captura dinámica del original (copia de trabajo, exe intacto, breakpoints de
  hardware y volcado de solo lectura). Resultado: sin teclas el juego **no
  llega nunca** a la ruta 3D (se queda en el bucle de cine S4, tres
  películas); al entrar en la ruta 3D la cámara **vuela sola** durante **428
  frames** (~17,1 s a ~25 fps) desde el encuadre por defecto del nivel
  `(2341, 4071, 3074)` hasta el encuadre del menú `(2243, 487, 196)`, en un
  arco largo (sube a `Y=6503`, baja a `Y=-8346`) y frena hasta el alto
  total. **No** es una cinemática de vídeo (el S4 corre antes, en su propio
  bucle bloqueante) ni una interpolación de dos puntos (el ajuste polinomico
  de grado 3 deja residuos del orden del recorrido: es una spline de varios
  puntos de control). El código muestra que la cámara no interpola: **copia**
  posición y orientación de un nodo de control (`0x43B56B..0x43B5CC`) que el
  mundo carga desde un fichero (`0x461E0B` → `0x461B90`, array de registros
  de 0x200 B). Lo pendiente: el instante exacto de aparición de los elementos
  del menú y si el avance es por frame o por tiempo. Detalle en
  [`S3_MENU_CAMERA_SEQUENCE.md`](S3_MENU_CAMERA_SEQUENCE.md).
- **Auditoría YIQ_422 (2026-11-11):** la decodificación en CPU del port
  coincide con la **referencia software del SDK** ( disposición de bits,
  coeficientes `Y+A+B`, croma de 9 bits con signo, saturación, alfa 255, un
  byte por texel) y el empaquetado de las 7 723 tablas NCC de los 37 `.VFX`
  originales coincide byte a byte. **No se afirma equivalencia píxel a
  píxel** con el silicio del Voodoo: eso exigiría capturar el framebuffer
  del juego, que no es accesible con las herramientas disponibles.
- **Auditoría correctiva (2026-11-11):** la hipótesis del
  «tinte de niebla por polígono» del módulo anterior **era
  incorrecta**: la variación de color entre polígonos la produce el
  **factor** de niebla (función de la profundidad), no el color destino.
  Medido sobre las 3476 mezclas del frame 0 de MAP7, el color destino es
  constante y las mezclas con color negro tienen factor 0 (sin efecto
  visible). Queda abierto solo el caso de un nivel con colores de
  niebla distintos por objeto, que MAP7 no tiene.
  el proceso vivo), recorre el BSP con la prueba de esfera original y usa
  la proyección del juego (focal 320, centro 256/192, plano cercano 160).
  Con `map7.dfx` el port reproduce las 307 hojas visibles (mismo orden), la
  matriz de emisión bit a bit y los 1123 triángulos en el mismo orden de
  dibujo. Ejecutado con GPU hardware y ventana Win32 real. El modo
  diagnóstico (matriz sintética + encuadre automático) se conserva con
  `--diagnostic-view`.
- **Estado de S-2:** la ruta de emisión de nivel **ejercitada por MAP7**
  (recorrido BSP, visibilidad y emisión con el emisor cercano) queda
  **demostrada equivalente al original fase a fase**, no solo por el recuento:
  profundidades bit a bit en los 3429 polígonos, mismo conjunto de 1123
  triángulos, **mismo orden de dibujo** y mismas fases de subdivisión. La
  ruta del emisor lejano sigue abierta. Detalle en
  [`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md) § 9.5.

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
- **S-2 (recorrido BSP + visibilidad de cámara):** implementado y documentado en `port/` ([`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md)); auditado (7.ª–18.ª pasada; la 11.ª solo documental, la 13.ª corrige el código de la 12.ª, la 14.ª la aritmética de 32 bits y la 18.ª la emisión): **37 comprobaciones de BSP, 889 de cámara y 38 855 de emisión, 0 fallos**, ASan/UBSan limpios, AZTEC16 = exactamente 815 hojas. La cadena de orientación (`0x46DD40` → `0x46E643` → `0x405C41`, rotaciones Euler Z→Y→X con los ángulos **negados** (como en el original), `M = transpose(O)` con `M2` aplicado a las columnas) y la prueba de esfera contra seis semiplanos de `0x401C00` están implementadas en `port/src/camera/camera.c` ([`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md)) e integradas en `gex_bsp_walk` mediante `cam_ctx` opcional. Toda la aritmética de 32 bits reproduce el wrap de x86 sin desbordamiento con signo ni conversiones de implementación definida (acumuladores `uint32_t`, `sar 12` sobre los bits del registro, copias de bits para los campos crudos). `test_camera` y `test_bsp` están integrados en `build.ps1 test` y en `make test`. Limitaciones declaradas: la tabla trigonométrica retail ya está **verificada bit a bit** (truncado, 2026-11-11) y la cadena de cámara **validada dinámicamente**; siguen pendientes B fila 2 (`cam+0xB4..0xB8`), preservada por decisión del port, y el **espacio de datos del nivel**, que impide reproducir el recuento de hojas visibles del original (307 frente a 0). La **emisión de polígonos está implementada en código** (2026-11-10/11, 17.ª–18.ª pasadas): módulo puro `port/src/emit/emit.c` + [`port/include/gex/emit.h`](port/include/gex/emit.h) con `gex_level_emit` y sus fases separadas (`collect`, `project`, `classify`, `subdivide`, `sort`), reproduciendo la proyección exacta de `0x46A1D0`, la niebla `0x64FEA8`/`0x461895`, los umbrales `0x65BF80`/`0x65BF04`, el determinante de `0x46B6B5`, el reparto de color entre `0x426256` y `0x426588` (segunda pasada de niebla), la prioridad de la subdivisión (cuatro principales + hasta tres *slivers*, sin niebla), el gate completo de los emisores y la ordenación por `(maxz >> 2) + 4*layer` con desempate LIFO. La 18.ª pasada corrige los seis defectos detectados (x/y sin inicializar, aritmética con wrap x86, offsets comprobados, mezcla de color real, subdivisión/emisores/orden verificados y AZTEC16 estructural) y deja `test_level_emit` en **38 855 comprobaciones**, 0 fallos, limpio en Clang `-O0`/`-O2` y ASan/UBSan. S-2 **no** se declara completo: la integración con el renderizador ya está hecha (Módulo 3, 5A, 5B y 4C), pero **falta la validación dinámica contra el ejecutable original**. El contrato revisado está en [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §11.
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
- **Auditoría del Módulo 5A y primera ejecución real (2026-11-11):** se auditó
  el recorrido completo desde la inicialización hasta el cierre y **se corrigió
  la liberación parcial**: la limpieza estaba atada a una bandera
  (`geometry_ready`) y, si la malla fallaba después de crear el pipeline, se
  perdían los shaders, el input layout, los estados, el constant buffer, el
  handle de `d3dcompiler_47.dll` y los triángulos del cubo. Ahora cada recurso
  se libera según su propio estado y el cierre es completo aunque cualquier
  etapa intermedia falle. Además: el mensaje de error del pipeline **se perdía
  al limpiar** (el `memset` de la limpieza borraba el texto del compilador
  HLSL justo cuando el llamador iba a leerlo) — ahora se conserva; los
  errores de dibujo **se diagnostican** (`last_draw_status` y
  `last_draw_hresult` en el renderer, impresos por la aplicación); y el
  sombreado del cubo usaba una **normal sin normalizar**, que quemaba las
  caras a 255 (encontrado al mirar la ventana real, no en las pruebas
  offscreen) — corregido.
- **Evidencia, por categorías (separada):**
  - **Pipeline validado offscreen:** `test_d3d11_pipeline` 137 comprobaciones
    (287 con la inyección de fallos por etapa que se añadió en esta
    auditoría), 0 fallos, WARP y render target propio, sin ventanas.
  - **Aplicación compilada:** `gexloop.exe` enlaza device, swapchain, renderer,
    pipeline, malla y geometría; compila sin warnings propios.
  - **Ejecución real con ventana:** sí, en sesión de escritorio interactiva
    (sesión 1), con GPU real (dispositivo *hardware*, no WARP): 30 frames
    presentados, 0 errores de frame, salida limpia.
  - **Evidencia visual:** sí. La ventana se capturó con `PrintWindow` y se
    analizaron los píxeles: 105 959 de 470 000 (22,5 %) distintos del color de
    limpieza, con la caja del cubo centrada y los tres colores de sus caras
    visibles ((206,59,206), (91,26,26), (147,147,42)). La captura es una
    evidencia local y **no se publica en el repositorio**.
- **Módulo 5B (2026-11-11): primera geometría real de AZTEC16.DFX en la
  ventana.** Nuevo módulo portable
  [`port/include/gex/scene_level.h`](port/include/gex/scene_level.h) +
  [`port/src/scene/scene_level.c`](port/src/scene/scene_level.c) que encadena
  los módulos que ya existían (`gex_dfx_load` → `gex_dfx_load_level` →
  `gex_bsp_walk` → `gex_level_emit` → `gex_geometry_build`) sin duplicar
  ninguno. La aplicación acepta `--level <ruta>`: con ella dibuja la geometría
  real del nivel; sin ella, el cubo de prueba sigue disponible. Si la carga
  falla, se informa del error y **no se dibuja geometría inventada**: la
  aplicación sigue limpiando y presentando y cierra con normalidad.
  Estadísticas de AZTEC16: 16 082 entradas, 3 882 vértices y 9 609 polígonos en
  el descriptor, 815 hojas del BSP, **2 854 triángulos emitidos** (1 427
  pasadas de niebla), 0 descartados por profundidad, **4 914 vértices y 8 562
  índices** de geometría. La vista es **diagnóstica** (matriz de emisión
  sintética, niebla generada con 0x461895 y encuadre automático). Desde el
  2026-11-11 el módulo tiene además `gex_scene_level_load_camera()`, que
  carga el nivel con la **cámara original** (matriz de emisión `cam+0xE0`,
  prueba de esfera del BSP y la misma proyección) y reproduce las 307 hojas,
  la matriz de emisión bit a bit y los 1123 triángulos del original en el
  mismo orden de dibujo. Pruebas: `test_scene_level`
  **8 627 comprobaciones, 0 fallos** (portable, `-O0`/`-O2`/ASan+UBSan) y
  `test_level_emit` sube a **38 855**. Detalle en
  [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §16.
- **Módulo 4A (2026-11-11): el contenedor de texturas `.VFX`.** REA estática del
  ejecutable original (`0x00461C32` construye la tabla global `0x006488F4`
  desde el `.VFX`: `count` en `+0x00`, registros con cabecera de `0x8C` B,
  tamaño del bloque en `+0x84`, datos en `+0x8C`; el puntero de `+0x10` lo
  escribe el juego y en el fichero es un valor de relleno que hay que
  ignorar). El recorrido así descrito termina exactamente en el EOF de los 37
  `.VFX` de `E2_A_ORIGINAL` (36 niveles + `GEX.VFX`). El formato de la textura está en `+0x0C` (1, 11, 12 en
  los originales); solo el formato 1 sube una paleta desde `+0x14` y el
  formato 12 activa el blending alfa. Implementado en
  [`port/include/gex/vfx.h`](port/include/gex/vfx.h) +
  [`port/src/asset/vfx.c`](port/src/asset/vfx.c) (portable): parser con
  validación estricta de offsets, tamaños, conteos y desbordamientos, tabla de
  entradas y bloques de datos crudos. **Auditoría correctiva (2026-11-11):** el recorrido tiene que
  terminar **exactamente en el EOF** (si sobran bytes, `GEX_VFX_ERR_FORMAT`) y
  la tabla **NCC** (`GuNccTable`) **solo se publica para el formato `YIQ_422`** (para 11 y 12,
  `palette = NULL` y `palette_size = 0`). Pruebas: `test_vfx` **1 668
  comprobaciones, 0 fallos** (`-O0`, `-O2` y ASan/UBSan), incluida la
  correspondencia DFX→VFX. **No se implementan decodificadores de píxel**: el
  formato de píxel, las dimensiones y la paleta completa **no están
  demostrados** (el juego entrega los bytes tal cual a Glide) y no se inventa
  nada. Detalle en
  [`docs/VFX_TEXTURE_FORMAT.md`](docs/VFX_TEXTURE_FORMAT.md).
- **Módulo 4B (2026-11-11) y auditoría correctiva:** el **formato de píxel, las
  dimensiones y el contenido de la tabla quedan DEMOSTRADOS**. El registro VFX
  es un **`GrTexInfo` de Glide 2.x** (`smallLod`, `largeLod`, `aspectRatio`,
  `format`, `data`): el `GrTexInfo` de la fuente que el juego construye en
  `0x0065BFA0` (`0x00432AB7..0x00432AE2`) tiene esa misma disposición y el
  cargador de `.VFX` (`0x00461C32`) rellena `+0x10` con `registro+0x8C`. Los
  identificadores de `+0x0C` son los de la enumeración `GrTextureFormat_t` de
  Glide 2.x: **1 = `GR_TEXFMT_YIQ_422`, 11 = `GR_TEXFMT_ARGB_1555`,
  12 = `GR_TEXFMT_ARGB_4444`**. El bloque de 112 B de `+0x14` es una
  **`GuNccTable`** y **no** una paleta (la paleta de Glide son 1 024 B, y el
  `type = 0` de `grTexDownloadTable` en `0x0046614A` es `GR_TEXTABLE_NCC0`);
  el `packed_data` de todas las tablas de los originales coincide byte a byte
  con el que calcula el SDK. Las **dimensiones** salen del LOD (`256 >> lod`)
  y del aspecto (`8x1`…`1x8`): el tamaño del bloque coincide con el esperado
  en **15 251 de 15 251 registros** de los 37 `.VFX` originales, todos con un
  único nivel. API nueva en `gex/vfx.h`: `gex_vfx_format_info_get`,
  `gex_vfx_ncc_table_twin`, `gex_vfx_ncc_table_parse/pack`,
  `gex_vfx_texture_info_get` y los decodificadores
  `gex_vfx_decode_argb1555/argb4444/yiq422/level` (salida RGBA8, alfa recto)
  en `port/src/asset/vfx_decode.c`. `test_vfx`: **2 968 comprobaciones, 0
  fallos** (`-O0`, `-O2` y ASan/UBSan). Detalle en
  [`docs/VFX_TEXTURE_FORMAT.md`](docs/VFX_TEXTURE_FORMAT.md) §4 y §6.
- **Módulo 4C (2026-11-11): las texturas del `.VFX` en D3D11.** El nivel
  deja de verse como una maqueta de color plano: el `.VFX` del nivel se carga,
  sus texturas se decodifican a RGBA8 (portable, `gex/vfx_texture.h`) y se
  suben a la GPU en un **atlas** 2D con un texel de borde replicado por
  textura (`gex/render/d3d11_texture.h`, Windows-only), con un sampler
  **lineal + WRAP** (los valores por defecto de Glide 2.x, que el juego nunca
  cambia) y **sin mipmaps** (los 15 251 registros medidos tienen un solo
  nivel). El vertex shader transporta la UV y el índice de textura
  (`nointerpolation uint`, por polígono) y el pixel shader muestrea el atlas y
  modula el color por vértice con el texel. La **UV se normaliza dividiendo
  entre 256** (no 255): el byte 255 del `.DFX` es el último texel, y con 255
  el sampler WRAP devolvía el primer texel (costura en el borde de cada
  polígono). La coordenada del original es `byte / 16.0f` (`0x0046619D`), con
  un divisor constante: el byte está en un espacio independiente de la
  resolución, y el port lo interpreta como coordenada en **texels** (la textura
  se repite cada `w x h` texels), que es lo que mantiene la densidad de texels
  constante entre texturas de distinta resolución. **Verificado en ejecución
  real** (GPU hardware, ventana Win32, 960×540): el nivel de AZTEC16 sale
  texturizado (3 101 colores distintos en la captura de la ventana, frente a
  la maqueta plana anterior). `test_vfx`: **3 010 comprobaciones, 0 fallos**
  (`-O0`, `-O2` y ASan/UBSan); `test_d3d11_pipeline`: **324** (WARP); suite
   completa **16/16**. **Límites:** la niebla se aplica antes de la
   textura (el original la aplica después) y el blending alfa se aplica a todo
   el dibujo (el original, por polígono). La cámara real del juego ya está
   integrada en el renderer (§15 de `S2_CAMERA_VISIBILITY.md`); de esa ruta
  [`docs/VFX_TEXTURE_FORMAT.md`](docs/VFX_TEXTURE_FORMAT.md) §9 y
  [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §18.
- **Pendiente de integración:** los **materiales de producción** (el shader
  actual modula el color por vértice con el texel, pero no hay materiales
  NCC con iluminación), el ***depth buffer***, el **blending por polígono**
  del original, la **niebla aplicada después de la textura** (ahora se
  aplica antes, dentro del color por vértice), la **cámara real del juego**,
  el *resize* de la ventana y la animación. El primer escenario original ya
  se renderiza **con las texturas del `.VFX`**, pero **todavía no con la
  cámara real**: el encuadre sigue siendo el diagnóstico del port. Sigue
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
- **Estado de los hitos (2026-11-11):** Hito 1 (parser DFX) implementado y validado; Hito 2 (recorrido BSP) con recorrido estructural, visibilidad original de cámara y **emisión de polígonos** implementados y auditados (889 comprobaciones de cámara + 37 de BSP + 38 855 de emisión, 0 fallos); Hito 3 (buffers D3D11) **implementado**: conversión portable (`gex_geometry`) y creación real de vertex/index buffers (`gex_d3d11_mesh`) con WARP y hardware, 6 963 + 108 comprobaciones, 0 fallos; Hito 5 **parcial**: el Módulo 5A añade el pipeline de dibujo real (shaders, input layout, estados, `DrawIndexed`) y una geometría 3D de prueba visible en la ventana, 137 + 355 comprobaciones, 0 fallos. El **Hito 4 (texturas) está cerrado** (Módulo 4C: las texturas del `.VFX` se decodifican y se muestrean en D3D11, verificado en ejecución real con GPU hardware); quedan la validación dinámica de S-2 contra el ejecutable original y el resto del Hito 5 (materiales de producción, *depth buffer*, blending por polígono, niebla después de la textura y la cámara real). La mecánica de carga está documentada en `U1_DFX_RELOCATION_AUDIT.md`.
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

R-3, R-4 y R-5 están **COMPLETADOS**: implementación básica del renderer D3D11 con device, swapchain, Present, clear y render target view. Pruebas verificadas en modo WARP con 145 checks (15 en una sesión sin escritorio, donde la parte de swapchain se omite), 0 fallos. **Geometría (Módulo 3, 2026-11-11):** existen ya vertex/index buffers reales —`gex_geometry` + `gex_d3d11_mesh`— con vértices de 28 B (posición, profundidad, color, UV, textura, capa, flags), creados con WARP y con hardware y verificados por lectura de vuelta. **Dibujo (Módulo 5A, 2026-11-11):** el renderer dibuja esa geometría con un vertex/pixel shader propios, `ID3D11InputLayout`, estados y `DrawIndexed`; la aplicación muestra un cubo 3D de prueba. Lo que sigue sin existir es el material de producción: *depth buffer*, blending por polígono del original y materiales con iluminación; las **texturas ya están** (Módulo 4C) y la carga del nivel real también, así que **la geometría visible es la del juego, pero con el encuadre diagnóstico y sin materiales**.