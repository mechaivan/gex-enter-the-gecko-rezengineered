# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE started (Etapa A documented in Phase 1; bounded static E-3/M-13 + Línea A/B executed read-only 2026-10-10 under explicit authorization - no Ghidra/decomp; static function map + call graph + per-function imports on the EU exe 2026-10-25, `docs/FUNCTION_MAP.md`; first evidence-based subsystem segmentation 2026-10-30, `docs/SUBSYSTEM_SEGMENTS.md`; dynamic frame-step trace with hardware breakpoints 2026-10-30, `docs/FRAME_STEP_TRACE.md`; static DirectSound init routes 2026-10-31, `docs/AUDIO_INIT_PATHS.md`; U-1f completada 2026-11-05 con corrección de U-1a…U-1e, `U1F_RESULTADOS.md`; U-1g completada 2026-11-06, `U1G_RESULTADOS.md`)
**Implementation:** Started 2026-10-31 — native port skeleton in `port/` (C11, no dependencies): reader for the `SLAG` audio container (`0x0053899C` + loader `0x005388AC`), 29 test cases + fuzz, 69 305 checks, `docs/SAG_CONTAINER.md`; **audio route** implemented the same day (`0x00538285` IMA ADPCM decoder + `0x005380B3` streaming pump + `0x00538731` player) with an injectable backend, 60 052 + 144 checks, `docs/AUDIO_MIXER_RE.md`; **audible audio** added 2026-11-01 (I-2b): real DirectSound backend (`dsound.h`/`-ldsound`, no new dependency) and the frame-loop scheduler, 215 + 50 checks including the device suite, `docs/AUDIO_MIXER_RE.md` §16; **native frame loop** added 2026-11-01 (I-3): the M-13 accumulator moved into `port/src/core/loop_core.c` (single implementation, the prototype harness now compiles it), `port/src/core/frame.c` reproduces the measured order pre-frame → update × N → audio scheduler → present, 410 checks (incluida la regresión T6b del error de `pre_frame`) + `framedemo`, `docs/FRAME_LOOP_PORT.md`; **first native process** added 2026-11-02 (I-4): real Win32 window, `WM_KEYDOWN`/`WM_KEYUP` routing, focus loss, `QueryPerformanceCounter` clock and the I-3 frame loop behind a null renderer (`gexloop.exe`), 826 + 117 + 1009 new checks and a real-window run (Escape and `WM_CLOSE`, 0 errors), `docs/WINDOW_INPUT_PORT.md`; **renderer research & design** added 2026-11-02 (I-5): original Glide surface fully censused (38 imports), present sequence confirmed, alternatives evaluated (D3D 11 recommended over OpenGL/Glide-layer/software, conditioned on device-creation test R-1), `gex_renderer` contract proposed with the null renderer kept for tests, validation plan R-1…R-8; `docs/RENDERER_ARCHITECTURE_RESEARCH.md`; **D3D11 device** added 2026-11-02 (R-1): `gex_d3d11_device` creates/destroys the device in hardware and WARP modes (both reached feature level 11_1 on the maintainer machine), 205 new checks with Clang 22.1.8 and GCC 16.1.0, leak check by manual-refere... (line truncated to 2000 chars)
**Testing:** Hito 1 + 5ª–9ª sesión (2026-10-09, PC mantenedor; nGlide 2.10, ≈25 FPS Steam, wrapper = nGlide 2.10 CONFIRMADO por payload-hash, imports solo-sistema/backend dinámico; A2 46%, global 20%; resto pendiente) + smoke in-game H4a (2026-10-11, PC mantenedor: arranque real hasta menú; registro restaurado)
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-11-19 (S-10: **la visibilidad BSP de MAP7 usa la cámara
del juego en todo el vuelo**, sin predicados; se elimina `scene_visible_all` y
se documenta que el culling por esfera ya estaba cableado desde S-3)

### S-10: la visibilidad BSP de MAP7 — confirmada y cableada sin predicados (2026-11-19)

Detalle completo en `docs/D3D11_FIRST_SCENE_PLAN.md` § 26, `S2_BSP_TRAVERSAL.md`
§ 10 y `S2_CAMERA_VISIBILITY.md` § 15.8.

**Hallazgo.** La premisa «`scene_visible_all` dibuja todas las hojas» **no**
describía el código: desde S-3 los modos con cámara pasan `cam_ctx` a
`gex_bsp_walk`, que aplica la prueba de esfera original de los seis semiplanos.
El predicado era redundante. El control lo demuestra: con `cam=NULL` la pose
final vuelve a dar **1483** hojas / **1818** triángulos; con cámara da **249 /
1722**.

**Correcciones.**

- Eliminado `scene_visible_all`; los dos recorridos usan `visible = NULL` y la
  visibilidad la decide solo `cam_ctx`. La vista diagnóstica sigue recorriendo
  las 1483 hojas, ahora declarada explícitamente como «sin cámara».
- `scene_emit_with_camera` reconstruye la cadena de orientación (`gex_cam_update`)
  sobre una copia local antes del recorrido y de la emisión, como hace el
  original antes de `0x401C00`.
- La aplicación ya no imprime las estadísticas del fotograma inicial como si
  fueran las de la pose congelada (`frozen pose stats` + `(initial frame)`).

**Verificación.**

- Nueva `test_map7_visibility_flight`: 18 fotogramas medidos reproducidos
  (conteo y lista), conteo variable, subconjunto estricto de las 1483 hojas,
  pose final 249/1722, cadena reconstruida con A/B vacíos y determinismo.
- Suite completa **0 fallos**; sanitizers O0/O2/ASan/UBSan **TODO OK**.
- Capturas reales GPU hardware (`--camera-path-freeze 1/36/69/101/133/166/225/
  278` y menú): 307/1123, 402/1676, 455/1730, 122/414, 353/1834, 166/787,
  127/414, 119/437 y 249/1722 (1742 con menú). Control todas-las-hojas vs.
  cámara: PSNR medio 24,45 dB (36), 26,03 (69), 29,37 (278) y 37,86 (menú).
- Sin regresión: capturas byte-idénticas a S-9 (PSNR infinito); menú, llama y
  231 texturas del vuelo intactos.

**Pendiente.** Comparación píxel a píxel con el framebuffer original en poses
intermedias (sin captura de referencia), el rectángulo 2D de fondo `0x64FB40`,
la pantalla inferior del televisor y el intercambio R/B. Ninguno es de
visibilidad BSP.

### S-9: oscuridad general de MAP7 — la tabla de color de Glide (2026-11-19)

Detalle completo en `docs/D3D11_FIRST_SCENE_PLAN.md` § 25 (y cierre de § 19.11).

**Síntoma.** El vuelo de MAP7 se veía más oscuro que el original, sobre todo en
las sombras: hierba, madera y suelo casi negros. § 19.11 había dejado abierta la
«escala del color» al ver flotantes como 40.0 o 180.0 en el vértice de Glide.

**Causa raíz demostrada.** El motor no entrega el byte de color del comando: lo
convierte con una **tabla de 256 flotantes en `0x555158`**, leída del ejecutable,
con `tabla[i] = min(2·i, 255)` (0→0,0; 1→2,0; 20→40,0; 127→254,0; 128→255,0;
255→255,0). La tabla se aplica al **construir el vértice** (`0x467d85`–`0x467dbf`,
justo antes de `grDrawTriangle` en `0x467e5b`), **después** de la niebla
(`0x46B727`, llamada desde `0x426d41`–`0x426dc5`), que mezcla sobre el byte
crudo. nGlide usa ese flotante como **0..255 sin normalizar** y lo trunca
(medido con un arnés de 32 bits y el `glide2x.dll` real: 0,5→0; 1,0→0;
127,5→127; 255→255; 510→255). Por tanto el color efectivo del original es
`min(2·byte, 255) / 255`. El port hacía `byte / 255`: mitad de brillo hasta 127
y más oscuro aún por encima, porque el original satura en blanco desde 128.

**Corrección.**

- `port/src/geometry/geometry.c`: `vertex_from_triangle` aplica la tabla
  (`color_to_vertex`) a R/G/B; el alfa no se toca. Se hace en la construcción
  del vértice y **no en `emit`**, para que el registro de comando siga siendo
  byte-exacto frente al original.
- El panel sintético del menú se entrega como su mitad exacta (`0xFF08080C`)
  para que la pantalla conserve `0xFF101018`; el panel está fuera de encuadre.
- No se tocan niebla, R/B, escala de texturas, atlas, cámara, orden, scissor ni
  mezcla.

**Verificación.**

- Nueva `test_color_table` con los valores medidos; actualizados
  `test_geometry`, `test_d3d11_mesh`, `test_d3d11_pipeline`, `test_menu` y
  `test_scene_level`. `test_geometry` añadido a la matriz de sanitizers.
- Suite completa sin fallos; sanitizers O0/O2/ASan/UBSan **TODO OK**.
- Capturas reales 960×540 en poses congeladas (`36/69/101/133/166/225/278`),
  antes/después: la luminancia media sube de 34,3→45,0 (36), 27,0→37,7 (69),
  33,9→45,1 (101), 32,3→45,9 (133), 38,5→53,5 (166), 43,5→57,4 (225) y
  48,2→65,3 (278); el centro sube entre el +62 % y el +100 %. El blanco puro
  sigue en el 0,00 % (máximo 0,38 % en los brillos de la llama): no se pierde
  detalle.
- 231 texturas en el vuelo y 239 con el menú (cobertura 100 %); menú, logo y
  llama sin regresión (`test_menu` 223, `test_actor` 80, 0 fallos).

**Pendiente que sigue abierto:** pantalla inferior del televisor (§ 22.6),
intercambio R/B (§ 23.5) y `poly#116`.

### S-8: anomalías visuales de MAP7 — atlas incompleto y ancho del atlas (2026-11-18)

Detalle completo en `docs/D3D11_FIRST_SCENE_PLAN.md` § 24 y
`docs/VFX_TEXTURE_FORMAT.md` § 9.8.

**Síntoma.** Durante el vuelo de cámara aparecían zonas extensas lisas,
gris/azuladas, que ocupaban muros, rampas y suelo en vez de la geometría
texturizada; en algunos encuadres parecían bloques planos desproporcionados.

**Causa raíz (dos defectos del port, no de la geometría).**

1. **El atlas tenía un máximo de 128 entradas y el vuelo usa 231.** La
   aplicación pedía solo las 128 texturas más usadas del recorrido; las
   restantes no están en la tabla `gex_tex_entries`, y el pixel shader, cuando
   no encuentra el índice, devuelve el **color por vértice** (ya con niebla) y
   alfa 1: superficie plana. Medido píxel a píxel sobre el triángulo ganador en
   el espacio lógico 512×384: `samples=36` 7 723 px, `69` 29 771, `101`
   28 302 (17,0 % de los píxeles cubiertos), `133` 3 408, `166` 9 782,
   `225` 3 742, `278` 0.
2. **`atlas_layout` devolvía el ancho de la última fila, no el máximo** de
   todas ellas. Con 128 texturas la fila 0 medía 4 070 px y la última 1 278,
   de modo que el atlas se creaba de 1 278 px: `blit_with_border` escribía con
   ese paso y las texturas de la fila ancha invadían las filas siguientes;
   además `atlas_uv = origen / atlas_w` superaba 1 y el WRAP las leía de otra
   zona. Era una **corrupción global del atlas, latente desde el Módulo 4C**.
   Al pedir 231 texturas la última fila pasaba a medir 1 616 y las anteriores
   4 064/4 080, y la corrupción empeoraba.

**Correcciones.**

- `GEX_D3D11_TEXTURE_MAX_ENTRIES` 128 → **256**; `gex_tex_entries[512]` en el
  HLSL (dos `float4` por entrada) y constant buffer de 8 208 B, muy por debajo
  de los 64 KiB de D3D11. La aplicación pide las **231** del vuelo (cobertura
  **100 %**) y 239 con el menú.
- `atlas_layout` devuelve el **ancho máximo** de fila.
- `--camera-path-freeze N`: avanza exactamente N muestras y congela la cámara.
  Antes, el número de muestras por present dependía del reloj y dos capturas
  del «mismo» fotograma podían caer en poses distintas.

**Verificación.**

- Suite completa de `port` sin fallos; sanitizers O0/O2/ASan/UBSan **TODO OK**.
- `test_d3d11_pipeline` **504 comprobaciones, 0 fallos**, con la nueva
  `test_texture_set_multiatlas`: 130 texturas de 32×32 fuerzan dos filas (la
  última más corta) y comprueban ancho 4 080, alto 68 y texeles concretos de la
  fila 0 (texturas 0 y 100) y de la fila 1 (129); además cruza el antiguo límite
  de 128.
- Atlas real: `1278×260` (128 entradas con el bug) → **`4080×326`** (231
  entradas corregidas); el menú carga 239 y el atlas es `4066×326`.
- Capturas reales 960×540 con GPU hardware, mismo comando y **pose congelada**
  en `samples=36/69/101/133/166/225/278`, antes (128 + bug de ancho) contra
  después (231 + corrección): cambian entre **36,4 % y 47,0 %** de los píxeles.
  En `samples=101` el muro de los grafitis pasa de bloques lisos grises a la
  textura legible. El menú y la llama no muestran regresión (captura y
  `test_menu` 223 / `test_actor` 80, 0 fallos).

**Descartado y pendiente.**

- **No hay defecto de triangulación, índices, vértices, UV, materiales ni orden
  de dibujo:** la emisión ya estaba validada 1123/1123 con el mismo orden, y la
  UV de cada textura no se recorre más de una vez (S-5).
- **El patrón de bloques en la hierba** está presente en los propios datos de
  las texturas de 64×64 YIQ_422 (volcadas a PPM y comparadas); no es corrupción
  del atlas.
- **La oscuridad general** era la incógnita de § 19.11: el original entrega a
  Glide `min(2·byte, 255)` (tabla `0x555158`) y el port normalizaba el byte.
  **Corregido en S-9** (§ 25).

### S-7: mezcla de renderizado por comando (2026-11-18)

Detalle completo en `docs/D3D11_FIRST_SCENE_PLAN.md` § 19.12 y
`docs/VFX_TEXTURE_FORMAT.md` § 9.7.

**Qué se implementó.** La selección de mezcla que el original hace en
`0x46558A` antes de cada comando, transportada hasta el renderer D3D11. No es un
caso especial de la textura 370: es la regla completa. La cámara, la
profundidad, el orden de dibujo, el scissor, el menú y la niebla **no se han
tocado**.

**La regla** (`gex_emit_blend_for_command`), con el opcode del comando y el modo
de textura (bits 14-15 del word de textura):

| Condición | Modo |
|---|---|
| `opcode & 0x04 == 0` u `opcode & 0x02 == 0` | `ALPHA` |
| modo 0/1 y la textura es ARGB_4444 | `ALPHA` |
| modo 0/1 y la textura no tiene alfa | `ADDITIVE` |
| modo 2 | `ERASE` (`0,6,0,0`) |
| modo 3 | `ADDITIVE_ALPHA_OFF` (`1,4,0,0`) |
| modo > 3 | `ADDITIVE` |

El menú (flags 0) y la pasada de niebla conservan `ALPHA`. `has_alpha` no se
adivina: la aplicación pasa a `gex_scene_level_set_alpha_textures` **todos** los
registros del `.VFX` con `format == 12` (ARGB_4444). En MAP7 son **103**.

**Cómo viaja.** Cada triángulo lleva `opcode`, `texture_mode` y `blend`
(`gex_emit_assign_blend` los reasigna en cada re-emisión, incluidas las 428
muestras de la trayectoria). `gex_geometry_build` agrupa los índices en **tramos
contiguos** con el mismo modo, saltando los clipeados y sin cambiar el orden; el
pipeline hace un `OMSetBlendState` + `DrawIndexed` por tramo. Una malla sin
tramos recibe un tramo `ALPHA` completo (compatibilidad).

**Los cinco estados D3D11:** `ALPHA` (`SRC_ALPHA`/`INV_SRC_ALPHA`, alfa
`ONE`/`ZERO`), `ADDITIVE` (`SRC_ALPHA`/`ONE`, alfa `ONE`/`ZERO`), `OPAQUE`
(`ONE`/`ZERO`), `ERASE` (`ZERO`/`INV_DEST_COLOR`, alfa `ZERO`/`ZERO`) y
`ADDITIVE_ALPHA_OFF` (`SRC_ALPHA`/`ONE`, alfa `ZERO`/`ZERO`). `pipeline->blend`
sigue siendo el alias de `blends[ALPHA]`.

**Resultado en MAP7.** 1 123 comandos → **1 117 `ALPHA` + 6 `ADDITIVE`** (los 6
de la textura 370, opcode `0xB6`, modo 1, `ARGB_1555`), **11 tramos**. Los 12
comandos `0xB6` con texturas 185/234/241 quedan en alfa porque su formato es
ARGB_4444.

**Pruebas y evidencias.**

- Suite completa de `port` sin fallos; sanitizers O0/O2/ASan/UBSan **TODO OK**.
- `test_level_emit`: `test_blend_rule` (las siete filas de la regla, menú y
  niebla) y `test_map7_blend` (1117 + 6 sobre el `.DFX` real).
- `test_geometry`: tramos, clipeado que no los parte, fallback y fallo de
  reserva. `test_d3d11_mesh`: copia, validación y rechazo de tramos
  incoherentes. `test_d3d11_pipeline`: `test_blend_runs` mide píxeles (negro
  aditivo ≥253 sobre blanco; el mismo texel en alfa 116..122; rojo aditivo
  250/128; en alfa 193..199/57..63) y cuenta dos `DrawIndexed` para dos tramos.
  `test_camera_path_scene`: 6 aditivos en el fotograma 0, consistencia tras 428
  muestras e idempotencia.
- Capturas reales 960×540 con GPU hardware, mismo comando y fotograma 1000
  contra el baseline `9ecb09d`: sin menú **56 997 px** distintos, todos dentro
  del bbox de la llama (`x 279..839`, `y 38..495`); con menú, de los 8 861 px
  del menú solo cambian **170**, y los 170 caen dentro de ese bbox. Fuera de la
  llama, **0 px** de diferencia.

**Límites.** Regla medida sobre nGlide 2.10, no sobre el SST-1. La escala RGB
0..2 (§ 19.11) y el intercambio R/B (§ 23.5) siguen abiertos y no se han
tocado. `ERASE`, `OPAQUE` y `ADDITIVE_ALPHA_OFF` están implementados y probados
de forma sintética, pero MAP7 no los ejercita.

### S-6: auditoría visual de MAP7 en el port D3D11 (2026-11-17)

Detalle completo en `docs/D3D11_FIRST_SCENE_PLAN.md` § 23.

**Método.** Se compara el **mismo encuadre** en los dos ejecutables, a la
resolución de referencia del original (512×384), con la cámara validada
(`samples=428`, `pos=(2243,487,196)`, `euler=(-29,0,2842)`). El encuadre es
correcto: el texto del menú cae a **±1 px** entre original y port (START
original `(104,142)-(186,160)` / port `(104,143)-(187,160)`; LOAD original
`(110,173)-(198,199)` / port `(111,172)-(199,199)`). Toda la auditoría se hace
por **medición** (`probe_pixel`, `polyprobe`, `leafprobe`, `emitprobe`,
`menuprobe`) y no por inspección visual.

**1. El parche plano `(16,16,24)` de abajo-derecha es el panel del televisor
del menú, no el suelo.**

- `MENU_PANEL_COLOR 0xFF101018` en `port/src/scene/menu.c`: 12 triángulos sin
  textura (el `.DFX` no le da UV: `uv_ptr = 0x20800080`).
- `menuprobe` lo proyecta en `(310,305)-(741,848)` del espacio lógico y cubre
  **14 956 px** de los 196 608 de la pantalla, en la esquina inferior derecha.
  Es exactamente la región del defecto.
- **Refutada** la hipótesis anterior ("parche de suelo con la textura 265
  plana"): la textura 265 decodifica en grises (56–83) y el suelo se dibuja con
  la matemática esperada — `emitprobe` da el triángulo ganador `poly#136`
  (tex 265, color interpolado `(44,77,152)`, texel ≈63 → `(11,19,38)`) y la
  captura sin `--menu` muestra `(11,19,38)` en ese píxel. El parche solo
  aparece con `--menu`, y la diferencia entre las dos capturas del port es de
  43 944 px con la forma exacta del panel y de los rótulos.
- **Causa demostrada del exceso:** el port **añadía la escena del menú después
  del ordenado del nivel**, así que el panel se dibujaba el último y tapaba el
  suelo. El original dibuja los actores por el **mismo** renderer ordenado por
  buckets, con la clave `(maxz>>2) + 4·layer`: el panel tiene 146..291 (en la
  zona visible, 277..291), el suelo 193..242 y los rótulos 134..135. Con el
  orden descendente del original (mayor = más lejos = antes), el panel queda
  **detrás** del suelo —el original muestra suelo en toda su proyección— y los
  rótulos **delante** (`poly#116`, clave 25..39, queda debajo de ellos).
- **Corregido:** el menú se emite en dos partes según las dos rutas medidas
  (`GEX_MENU_PART_PANEL` antes de `gex_emit_sort`, `GEX_MENU_PART_LABELS`
  después). Captura con `--menu`: los 9 puntos del panel coinciden
  **exactamente** con la captura sin menú y la región de los rótulos es
  **idéntica** a la de S-5 (0 px distintos de 17 550). Pruebas en
  `test_scene_level` (+1 771) y `test_menu` (+30). Detalle en § 23.1.

**2. Las esquinas azules `(51,51,204)` no son geometría ausente.**

- Ni con las **249 hojas** que ve la cámara (2 820 polígonos) ni con las
  **1 483 hojas** del nivel entero (16 292 polígonos) hay un solo polígono que
  cubra `(20,20)`: `polyprobe` y `leafprobe` dan cobertura vacía en ambos
  casos.
- `(51,51,204)` = `{0.2f, 0.2f, 0.8f}` = el **clear de marcador** de
  `port/src/app/i4_main_win32.c:528`. En el original ese azul no existe: el
  original pinta un **rectángulo 2D de fondo** de 512×384 (`0x64FB40`, handler
  `0x00469268`) con un degradado morado/oscuro medido en `(15,8,46)`,
  `(17,8,64)` y `(20,8,71)`.

**3. El fuego rectangular con fondo negro es la mezcla de la textura `370`.**

- La llama usa la textura **370** (32×32, `ARGB_1555`): alfa 255 en los 1 024
  texeles y fondo **negro**. El original la dibuja con
  `grAlphaBlendFunction (1,4,4,0)` — **aditiva** — en los 6 triángulos del
  opcode `0xB6`; el port usa el estado global `(1,5,4,0)`, así que el negro
  del fondo se vuelve opaco y tapa lo que hay detrás. **Corregido en S-7**
  (§ 19.12): la mezcla se elige por comando.

**4. Límites laterales: el port no recortaba a la zona lógica. (Corregido.)**

- El original dibuja en una superficie de **512×384**: fuera de ella no hay
  framebuffer y nada puede aparecer. El port escala esa zona a la ventana
  (960×540 → factor 1,40625 y bandas de 120 px), pero dibujaba **fuera** del
  rectángulo: en la captura de referencia, **29 941 px** de la banda izquierda
  y **35 680 px** de la derecha (de 64 800 cada una) tenían geometría que el
  original recortaría.
- **Corrección implementada:** `ScissorEnable = TRUE` en el rasterizador y
  `RSSetScissorRects` en `gex_d3d11_pipeline_set_target`, con el rectángulo
  calculado por `gex_d3d11_pipeline_logical_rect` — **la misma matemática del
  vertex shader** (`scale = min(vw/rw, vh/rh)`, centrado, bordes hacia fuera).
- **Prueba de regresión:** `test_logical_rect` (+66 comprobaciones, 409 en
  `test_d3d11_pipeline`): 960×540 → `(120,0,720,540)`; 4:3 exacto → todo el
  destino; más ancho → bandas laterales; más alto → bandas arriba/abajo;
  argumentos nulos → rect a cero; y la propiedad de que el rectángulo cabe en
  el destino y contiene el centro.
- **Verificación en ejecución:** captura 960×540 tras el cambio → **0 px** de
  geometría en las dos bandas y **0 px distintos** dentro de la zona lógica
  respecto a la captura anterior. El encuadre de referencia (512×384) no
  cambia por construcción (el rectángulo es todo el destino).

**5. Color por vértice: intercambio R/B. Queda como incógnita, sin cambiar.**

- Media global (R−B) sin menú: original `+22,12` frente a port `−25,97`; con
  intercambio, `+25,97`. La correlación por píxel es solo `+0,254` y las
  pruebas por regiones se contradicen.
- **Medición limpia (suelo, textura 265 gris):** en `(341,341)` el port da
  `(11,19,38)` con el intercambio actual del vertex shader y `(38,19,11)` si se
  quita; el original muestra `(55,36,21)`. Es decir, el original conserva el
  orden R>G>B del color de vértice (y el port queda ~35 % más oscuro por otro
  motivo), mientras que el intercambio lo invierte. La región del suelo lo
  confirma (`(330,350)-(440,375)`: original R−B `+19,1`, port actual `−5,3`,
  sin intercambio `+5,3`).
- **Pero** la pared prefiere lo contrario (región `(400,200)-(470,280)`:
  original R−B `+12,2`, port actual `+5,7`, sin intercambio `−5,7`). No hay una
  regla única demostrada; el ganador del original en esos píxeles no está
  identificado, así que el signo no se puede atribuir al orden de canales.
  **No se toca el shader**: haría falta un fotograma de control con un polígono
  y una textura conocidos (§ 23.6).

**6. Recuento y descarte de polígonos: sin regresión.**

- Con la cámara: **249 hojas / 2 820 polígonos** (`draw=764`, `subdivide=466`,
  `backface=983`, `depth-clip=62`, `offscreen=545`). Todas las hojas: 1 483 /
  16 292. El `depth-clip` del original está verificado (`0x46A1D0` compara con
  `[0x6474E8]` y `0xFFFF`, marca `0x40000` y el llamador `0x4015e3` descarta
  con máscara `0x7E07E000`) y el port hace lo mismo.
- La diferencia de ~1 517 polígonos/frame citada en la documentación anterior
  **no es comparable**: se midió en AZTEC16 (`S2_BSP_TRAVERSAL.md` § 8), no en
  MAP7.

**Validación.**

- Suite completa (`port\build.ps1 test`): **0 fallos**; `test_d3d11_pipeline`
  pasa de 343 a **409** comprobaciones, `test_scene_level` de 12 335 a
  **14 106** y `test_menu` de 193 a **223**.
- `run_sanitizers.ps1`: **40/40 OK** (O0/O2/ASan/UBSan) con los cambios de
  `menu.c` y `scene_level.c`.
- Captura real con GPU hardware a 960×540: `samples=428 finished=1
  pos=(2243, 487, 196) euler=(-29, 0, 2842) triangles=1742` con `--menu` y
  `1722` sin él; bandas limpias y panel oculto tras el nivel.
- Captura 512×384 de referencia: el panel ya no tapa el suelo y los rótulos
  siguen a ±1 px del original, con su color.

### S-5: fidelidad de las texturas de MAP7 (2026-11-16)

Detalle completo en `docs/D3D11_FIRST_SCENE_PLAN.md` § 22 y
`docs/VFX_TEXTURE_FORMAT.md` § 9.2.

**Causa principal demostrada: la escala de la UV, no el atlas ni el filtrado.**

- **El original escribe `s = byte`.** La ruta BSP `0x4661A6..0x4661B8` hace
  `fildl (float)byte` → `fmuls 1.0f` (`0x54c188`) → `fdivs 16.0f`
  (`0x54c18c`) → `fstps 0x6475a4` (`sow`), y `0x4684EF` escribe
  `0x3D800000 = 1/16` en `base+0x20` (`oow`). El hardware de Glide divide
  `sow/oow`, así que la coordenada efectiva es **`s = byte`** (0..255).
- **El espacio de coordenadas es `256 = lado MAYOR` de la textura, con escala
  uniforme en ambos ejes.** Confirmado en el SDK de Glide: `test17.c` y
  `qatest00.c` usan `255.0f * oow` para recorrer la textura entera, y
  `gsplash.c` documenta "texture coords in [0..255.0f]". La ruta de fuentes
  `0x432BE2` usa `*256/16` con `oow = 1.0` sobre una rejilla de 16 columnas ×
  4 filas de celdas de 8×8 texels en una textura de 128×32: 256 unidades = 128
  texels (lado mayor), 1 unidad = 0,5 texel.
- **Medición de MAP7** (`uvprobe`, 16 292 polígonos, 288 texturas, 652
  registros VFX): el rango UV de cada textura es **siempre 0..255 en el lado
  mayor y 0..(256·menor/mayor) en el menor**. Ejemplos medidos:
  `64×32 → u 0..255 / v 0..127`, `16×32 → 0..127 / 0..255`,
  `4×4 → 0..255 / 0..255`, `128×32 → 0..255 / 0..63`,
  `8×64 → 0..31 / 0..255`. Ninguna textura recorre su rango UV más de una vez.
- **El port usaba `texels = uv * 256`**, que repetía cada textura
  `256/lado` veces (32→8×, 64→4×, 16→16×): era el origen de los **mosaicos**
  (paredes, tronco, columna del fondo) y de la **pantalla del televisor en
  cuatro partes**. No era un problema del atlas, del sampler WRAP ni del
  filtrado lineal.
- **Corrección:** el shader en modo texel pasa a
  `texels = uv * max(size.x, size.y)` (escala uniforme = lado mayor) y se
  retira el `/2` que `menu.c` aplicaba a los rótulos. Equivalencia exacta para
  los rótulos: antes `(byte/2)/256*256 = byte/2`; ahora
  `byte/256*128 = byte/2`, con divisiones por potencias de dos y sin error de
  redondeo → **los rótulos no cambian, demostrado, no solo observado**.

**Atlas: ampliado con medición, no a ciegas.**

- El fotograma final del vuelo usa **65 índices distintos** y el menú añade los
  ocho `600..607`, es decir hacen falta **73**. Con el límite anterior (64) se
  quedaba fuera el **2,4 % de los vértices** del encuadre del menú (97,6 % de
  cobertura); sin menú, el 99,87 % (se perdía la textura 99, de 16×8).
- `GEX_D3D11_TEXTURE_MAX_ENTRIES` pasa de **64 a 128**; el constant buffer
  resultante mide **4 KiB** (HLSL `float4 gex_tex_entries[256]`, dos `float4`
  por entrada). Tras el cambio: **73 texturas decodificadas, 100,0 % de
  cobertura**, atlas 4026×130.

**Defecto independiente, no UV: el logo de la pantalla inferior.**

- La pantalla inferior del televisor tiene **dos juegos de quads coplanares**:
  el logo GEX (textura `368`, 64×64, opaca, con su viñeta) y el "cristal"
  (textura `311`, 64×64, negra con viñeta, también opaca). Comparten `z`
  (574/572/570/540/538/536) y sus `seq` son 137–145 (`368`) frente a 115–122
  (`311`). La regla LIFO del bucket dibuja primero el `seq` mayor, así que el
  **cristal 311 tapa el logo 368**. El original muestra el logo.
- **Clasificación:** visibilidad/orden de emisión y estado de material, **no**
  interpretación UV. No se toca el orden de emisión sin demostrar antes qué
  modelo/actor posee cada quad y si el cristal pertenece a un estado que el
  original no dibuja en este fotograma. Queda como pendiente real (S-6).

**Hipótesis descartadas en esta tanda.**

- "Dividir la UV entre 2 en el pipeline": solo valía para los rótulos; era una
  compensación del `uv*256` erróneo. Refutada como regla general.
- "La repetición es intencionada en el original": refutada; ninguna textura
  recorre su UV más de una vez y el WRAP solo existe como valor por defecto del
  sampler.
- "Normalizar por las dimensiones reales de cada eje": refutada; el eje menor
  nunca llega a 255 y la escala es uniforme sobre el lado mayor.
- "El atlas pequeño causa los mosaicos": refutada; los mosaicos aparecían con
  las 64 texturas cargadas; eran escala UV.

**Validación.**

- Suite completa (`port\build.ps1 test`): **0 fallos**; `test_d3d11_pipeline`
  **343 comprobaciones**, incluida la nueva `test_texture_uv_scale` con una
  textura 64×32 (byte `(128,64)` → texel `(32,16)`; `(255,127)` → `(63,31)`).
- Prueba de regresión **real**: con la fórmula antigua (`uv*256`) la prueba
  falla; restaurada la corrección, pasa (verificado revirtiendo y restaurando).
- `run_sanitizers.ps1`: **40/40 OK** (O0/O2/ASan/UBSan).
- Aplicación compilada (`build.ps1 loop`) y **captura real** en ventana Win32
  con GPU hardware: 960×540, `textures: 65 distinctas ... se piden 73 ...
  cubren el 100.0%`, atlas 4026×130, `samples=428 finished=1
  pos=(2243, 487, 196) euler=(-29, 0, 2842) triangles=1742`.
- Comparación visual con el original: mosaicos eliminados (paredes, tronco,
  columna del fondo con una sola copia), pantalla del televisor ya no está en
  cuatro partes, rótulos START/LOAD/OPTIONS/QUIT correctos en orden y posición.

### Menú del televisor en el port: actores medidos (2026-11-15)

Detalle completo en `S3_MENU_CAMERA_SEQUENCE.md` § 11.

- **El menú es geometría de actor, no overlay.** Ruta medida en el binario:
  `0x471552(actor, matriz de cámara, out, modo)` transforma las columnas del
  registro con `0x4698B0`, traslada con `+0x14/+0x18/+0x1C`, proyecta con
  `0x469B02` (focal 320, centro 256,192, near 160) y llama al renderer
  virtual. `arg3 == 0` → rama de **panel** `0x4718CF`; `arg3 != 0` → rama
  **texturizada** `0x470F96` (billboard `0x470C7B`).
- **Formato del registro de animación (medido, 0x20 B):** `+0x00` 9 × int16
  base ortonormal Q12 **por columnas**, `+0x12` relleno, `+0x14/0x18/0x1C`
  int32 posición de mundo. Verificado con los cinco registros del menú.
- **Registros del menú:** panel `tvmenu__` base `(-3977,976,0)/(-976,-3977,0)/(0,0,4096)`
  en `(2900,-84,-528)`; rótulos en `(2758,412,198/170/143/115)` =
  START/LOAD/OPTIONS/QUIT con base común `(663,-4041,0)/(4041,663,0)/(0,0,4096)`.
  **Corrección de S-6 (2026-11-17):** con la cámara validada
  (`pos=(2243,487,196)`, `euler=(-29,0,2842)`) el panel **sí** entra en el
  encuadre: se proyecta en `(310,305)-(741,848)` del espacio lógico, es decir
  ocupa el tercio inferior derecho de la pantalla (14 612 px de los 196 608 de
  la zona lógica). La afirmación anterior —"se proyecta **fuera** del
  encuadre, arriba-derecha"— era **falsa**; ver `S3_MENU_CAMERA_SEQUENCE.md`
  § 11.2 y § 11.7.
- **En el `.DFX`:** `tvmenu__` clase `0x0DFE68`, 1 modelo, `field_0x10 = 0`,
  caja de 8 vértices/12 polígonos, radio 552, **sin textura**
  (`uv_ptr = 0x20800080`); `tvbutn__` clase `0x0DC538`, 20 modelos, y los
  modelos 2 y 3 son el mismo quad de 4 vértices/2 triángulos con textura
  `600` y `604`.
- **Texturas `600..607` (128×32, ARGB_1555, LOD 1) decodificadas:**
  `600` START, `601` QUIT, `602` OPTIONS, `603` LOAD (lavanda) y `604..607` las
  mismas en **rojo** (resaltado, regla `+4` medida en el par START).
- **Implementado:** módulos `gex/actor` (lectura de clase/modelo/vértice/
  polígono/subparte/UV) y `gex/menu` (escena medida), `gex_emit_output_push`,
  `gex_scene_level_add_menu` y `--menu` en `gexloop.exe`; `test_actor` con
  **80 comprobaciones** contra MAP7.DFX y `test_menu` con **193** que fijan
  las constantes medidas de la escena.
- **Validación:** suite completa en verde, sanitizers O0/O2/ASan/UBSan 40/40,
  y captura real con GPU hardware: los cuatro rótulos caen a **1 px** de los
  del original, con las mismas palabras y colores. El fondo sigue siendo la
  limitación conocida de visibilidad del BSP (no es regresión de esta tanda).

### Aparición del menú: estado medido y captura real (2026-11-14)

Detalle completo en `S3_MENU_CAMERA_SEQUENCE.md` § 10.

- **El menú no aparece al terminar los 428 frames.** Traza con breakpoints de
  hardware sobre `0x52754F` (update de `tvmenu__`) y `0x527432`
  (mostrar/ocultar): `0x52754F` corre **2413 veces** (una por frame) y
  `0x527432` **una sola vez**, en la carga, tomando la rama de **ocultar**.
  `[entity+0xF4]` pasa de `0` a **`2`** en `t=28586 ms` (~150 ms tras la
  carga) y se queda en `2` durante el vuelo y después; con `count=4`,
  `idx=1`, `b13=0`. Cámara y menú son independientes.
- **Dos "menús" distintos.** La tabla de `.rdata` con `MAIN MENU...` /
  `LEVEL SELECT MENU...` / `QUIT GAME` (`0x54E1E0`, acciones `0x423A8D`,
  `0x4238E2`, `0x423930`) es el **menú de pausa/debug in-game**. El menú
  principal real es el actor `tvmenu__` del nivel MAP7: `0x463340` carga
  `\gex.dfx`, `map7.dfx`, `map7.vfx` y `map7.tad`; la tabla de actores
  `0x553204` asocia `('tvmenu__','map_____') -> 0x527432, 0x52754F` por
  **nombre** (el `.DFX` no lleva punteros de código).
- **Captura de fotograma en el port.** `--screenshot PATH` /
  `--screenshot-frame N`: el renderer copia el backbuffer ya dibujado a un
  PPM (P6) antes del `Present`. Ejecución real con GPU hardware: el vuelo
  termina en `pos=(2243, 487, 196)`, `euler=(-29, 0, 2842)` y la captura
  muestra la estatua de Gex y un **muro BSP de cuatro pantallas**.
- **La pantalla salía sin textura por la selección del atlas**, no por un
  material roto: el port pide las 64 texturas más usadas del vuelo y la del
  fotograma final queda fuera. Medido: el fotograma final usa **65** índices;
  `--textures-final` pide los 64 más usados de ese fotograma (descarta el
  índice 99, 3 vértices) y el muro sale texturizado, sin ampliar el atlas.
- **Lo que falta para ver el menú:** las opciones no son BSP; las dibuja el
  actor `tvmenu__` (estado 2, 4 entradas). El port solo dibuja BSP, así que el
  siguiente paso real es el render de actores, no otra corrección de
  materiales.

### Secuencia de cámara del menú: la orientación integrada (2026-11-13)

Detalle completo en `S3_MENU_CAMERA_SEQUENCE.md` § 6.4 y § 8.

- **Refutada la premisa de matrices.** `0x40A6F2` **no** es una rutina de
  matrices sino un **muelle/limitador sobre puntos** (3 × int16) con cinco
  modos (tabla de saltos en `0x40AD98`: `0x40A740`, `0x40A834`, `0x40AD6A`
  no-op, `0x40A989`, `0x40AA7D`). Firma real:
  `0x40A6F2(mode, pA, pB, prev, dst, limit)`; modo 0:
  `d = pB - pA; dst = (d >> 2) - prev; prev += dst; pA += prev`.
  `ctrl+0x4C` y `ctrl+0x26C` son **puntos**, no matrices: la mira suavizada
  del modo *attract* y la de la segunda cadena.
- **En el vuelo no hay transformación:** con `ctrl+0x484 != 0` (retorno
  `0x411D0C`) el *look-at* usa el **sample crudo** de la spline de mira y
  `cam+0x00` es el sample crudo de posición. Por eso la referencia se genera
  desde el `.DFX`, sin valores capturados ya transformados.
- **Constantes leídas del ejecutable:** `0x54C178` = double
  `0x1.921fb54524550p+2` = **6.283185308** (no el `2π` de la libm) y
  `0x54C180` = **4096.0**; `0x540950` trunca hacia cero (`fistpll`). Luego
  `atan = trunc(atan2(y,x) / 6.283185308 * 4096.0)` y `atan2(1,1) = 511`
  (con `2π` daría 512). `0x40534F` se transcribe exacto:
  `len = (isqrt_frac((d0²+d1²)<<4, 4) + 8) >> 4`, con `0x4647EB` completo
  (bit más alto, `sh >>= 1`, `cl = sh + frac/2`, refinado), sin `sqrt()`.
- **Port:** `gex_cam_length_2d`, `gex_cam_atan2_q12` y `gex_cam_look_at`
  (`gex/camera.h`); `gex_dfx_load_camera_path_slot()` +
  `GEX_DFX_SLOT_CAMERA_LOOKAT` leen la mira (`0x961C` → `0xC374` → `0xC394` →
  `0xC39C`, 26 registros, suma 428); la escena avanza **dos nodos
  independientes** y en cada update calcula el *look-at*, reconstruye las
  matrices derivadas con el pipeline existente y re-emite.
- **Validación:** `test_camera` 995 (matemática del *look-at* con valores
  conocidos y el caso `atan2(1,1)=511`), `test_camera_path_scene` 2701
  (extracción de las dos trayectorias, primer/último Euler, mira y matriz de
  emisión), `test_camera_path_tsv` 5735 (**428/428 posiciones, 428/428 miras
  y 428/428 Euler** contra los dos TSV leídos en ejecución, sin cambios tras
  el centinela) y `test_camera_path_render` 26 (la matriz de emisión y el
  buffer de la GPU cambian). Suite completa **0 fallos**; sanitizers
  **32/32**.
- **Ejecución real** (ventana Win32, GPU hardware): 428 muestras,
  `finished=1`, `pos=(2243, 487, 196)`, `euler=(-29, 0, 2842)`, 1722
  triángulos, salida limpia sin errores de frame.
- **Texturas medidas de nuevo** con la orientación real: **231** distintas,
  las 64 más usadas cubren el **82,2 %** de los vértices.

### Comparativa PS1 PAL vs PC europeo (2026-11-12)

- Observaciones manuales registradas en `docs/PS1_PC_BEHAVIOR_COMPARISON.md`: attract/demo mode por inactividad solo observado en el menú principal PS1; loop musical continuo en PS1 frente a pistas que terminan en PC; PS1 omite la cinemática de Ubisoft que sí aparece en PC.
- Son diferencias observadas, no causas técnicas demostradas. La lista exacta de demos y el origen de la diferencia de cinemáticas siguen pendientes.
- Investigación de loop pendiente antes de implementar. S-02 (`_inmm.dll`) y F-10 (handler musical externo) son pistas comunitarias/documentales, no fixes verificados para PC EU.
- PS1 continúa como referencia secundaria. No cambiar el baseline PC silenciosamente; el loop estilo PS1 debe evaluarse como opción/mejora explícita.


### Secuencia de cámara del menú: el algoritmo de posición, cerrado (2026-11-13)

- **Quién avanza el nodo (medido):** `0x53E98C` llama a `0x53F252` en
  `0x53E997` (**avanza**) y a `0x53EF00` en `0x53E9B0` (**evalúa** en
  `0x005AC608`). `0x53E98C` se ejecuta **una vez por frame** desde
  `0x411B94` (bloque `0x411B8F` del update de cámara): 607 llamadas, 607 desde
  `0x411B94`; **428** llegan a evaluar = los 428 frames del vuelo.
- **El avance:** `frac = ((nodo[2]>>1)&0x7FFF) + 1` por llamada; si
  `frac >= D`, `nodo[0]++` y `frac = 0`. `D` es el `int16` de
  `array[idx*32 + 0]`: la **duración en frames** del segmento. Duraciones
  medidas: `20,13,10,10,15,10,15,14,25,39,24,26,23,17,15,18,14,13,14,15,15,
  15,15,15,18` → **suma exacta 428**.
- **El registro es de 32 B** (corrección: la interpretación anterior decía
  20 B): `+0` `D`; `+2/+4/+6` punto (int16); `+8/+0xC/+0x10` tangente de salida
  (int32); `+0x14/+0x18/+0x1C` tangente de entrada (int32). El array está
  **incrustado en el objeto** (`obj[+0]` → `obj+8`). `obj[+6] == 1` selecciona
  la otra variante, de 10 B (rotación, cuaternios).
- **La fórmula (verificada):** `t = (frac15 << 15) / D` (Q15) y
  **Hermite cúbica** con aritmética entera: `h00 = 2t³-3t²+32768`,
  `h01 = 3t²-2t³`, `h10 = t³-2t²+t`, `h11 = t³-t²` (en Q15), y
  `out[k] = (int16)((P_a·h00 + P_b·h01 + T_a·h10 + T_b·h11) >> 15)`.
  `S = [0x0054C1B0] = 15`, medido en runtime.
- **Verificación:** reimplementada en Python con enteros exactos y comparada
  con las 428 muestras de `analysis/menu_camera_map7.tsv`: **428/428
  coincidencias exactas** en los tres ejes. También coincide la primera
  muestra viva de `0x005AC608` (`(2341, 4071, 3074)` = frame 0).
- **Resultado negativo medido:** el contador `0x0064711E`
  (`0x006466D8+0x46`) **no** gobierna el nodo: coincide con la fracción en
  **4 de 607** escrituras, no coincide nunca con el índice, llega a 50 y se
  reinicia de forma irregular.
- **Trazador (aditivo):** nuevas opciones `watchdyn:` (watchpoint en un
  puntero del heap resuelto en el primer impacto), `probe:` (dword por
  impacto, para correlacionar) y `dumpindraw:` (bloque tras desreferenciar un
  argumento). Corregido el ruido de `SIN REGISTRO LIBRE` (una sola línea por
  dirección). Los breakpoints y opciones existentes no cambian.
- **Sigue abierto:** el criterio con que se calcularon las tangentes del
  registro y el estado de menú. **Superado:** la frase "la ruta de rotación
  (10 B → cuaternio → Euler)" describía la pregunta equivocada; la orientación
  real es un *look-at* entre dos trayectorias de 32 B y ya está integrada
  (sección anterior). No se ha tocado el pipeline D3D11.

### Secuencia de cámara del menú: la orientación, cerrada (2026-11-13)

Detalle completo en `S3_MENU_CAMERA_SEQUENCE.md` § 8.

- **Los 428 ángulos NO salen de un cuaternio.** El controlador de cámara del
  menú (`0x65CD80`) tiene **dos** trayectorias de **32 B** en el `.DFX`:
  `0xC034` (posición) y `0xC39C` (mira, el actor "hermano" del slot 1), con
  las mismas duraciones (suma 428). La orientación sale de un **look-at**
  (`0x406515` → `0x40AE51`): `d = cam - mira`, `euler_x = -atan2(d.z, len)`,
  `euler_y = 0`, `euler_z = atan2(d.y, d.x) + 0x400`.
- **Aritmética exacta:** `len = (isqrt_frac((d0²+d1²)<<4, 4) + 8) >> 4`
  (`0x40534F`, con `0x4647EB` = raíz **entera**, no la de la CRT; `isqrt_frac(n,4) = floor(√n·4)`);
  `atan2` (`0x464181`) es el de la CRT en doble precisión, convertido a
  `int16` en **Q12 de media vuelta** (`2048 = π`) y **truncado**
  (`0x540950`, `fistpll` con redondeo hacia cero). Constantes medidas:
  `0x54C178` = **6.283185308** (double `0x1.921fb54524550p+2`, no el `2π` de
  la libm), `0x54C180` = **4096.0**. El `+0x400` son 90°.
- **Verificación:** la fórmula reproduce los 428 ángulos publicados
  **428/428** con enteros, desde el `.DFX`
  (`analysis/gen_menu_camera_rotation_ref.py`), y **425/425** contra la
  captura directa del ejecutable original
  (`analysis/capture_menu_camera_rotation.py`, breakpoint `0x406515`).
  Referencia: `analysis/menu_camera_rotation_map7.tsv`.
- **La trayectoria de 10 B existe pero no es la del menú (resultado
  negativo).** La variante 1 (`obj[+6] == 1`) está en `0xD118` / `0xD120`:
  18 registros de 10 B = `D` (int16) + cuaternión (4 × int16, Q12), con el
  centinela en el registro 17 y 305 frames de segmento. Se evalúa con
  `0x53EF00` → **`0x43158D`** (SLERP entero), con `0x431430` (producto escalar
  de 4 componentes, `sar 11`) y `0x431036` (decodificador de cuaternión
  comprimido). En 16 289 impactos medidos sobre el menú **su array nunca se
  evalúa**: pertenece a otro actor del nivel.
- **Pendiente en aquella tanda (superado):** integrar la orientación en el
  port e identificar las "matrices" `ctrl+0x4C` / `ctrl+0x26C` de `0x40A6F2`.
  Hoy está integrada (sección anterior) y se sabe que esos campos son
  **puntos** de un muelle, no matrices. Sigue pendiente el criterio de las
  tangentes del exportador.

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
    llamadas). **Implementado en S-7 (2026-11-18):** el modo se elige por
    comando y se aplica por tramos contiguos de índices, con cinco estados
    D3D11; la regla completa está en `docs/D3D11_FIRST_SCENE_PLAN.md` § 19.12.
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
- **Secuencia de cámara hasta el menú (2026-11-12):** investigada con
  captura dinámica del original (copia de trabajo, exe intacto, breakpoints de
  hardware y volcado de solo lectura). Resultado: sin teclas el juego **no
  llega nunca** a la ruta 3D (se queda en el bucle de cine S4, tres
  películas); al entrar en la ruta 3D la cámara **vuela sola** durante **428
  frames** (~17,1 s a ~25 fps) desde el encuadre por defecto del nivel
  `(2341, 4071, 3074)` hasta el encuadre del menú `(2243, 487, 196)`, en un
  arco largo (sube a `Y=6503`, baja a `Y=-8346`) y frena hasta el alto
  total. **No** es una cinemática de vídeo (el S4 corre antes, en su propio
  bucle bloqueante) ni una interpolación de dos puntos (el ajuste polinómico
  de grado 3 descarta un polinomio de bajo grado como descripción; la
  representación real se cerró después: **26 registros de control de 32 B** y
  **Hermite en Q15**, ver la entrada del 2026-11-13). El bloque `0x43B56B..0x43B5CC` realiza la **colocación inicial** copiando
  posición y orientación desde el nodo; no es la rutina de actualización de
  los 428 frames. El fichero y la representación de los datos se precisan en
  el apartado siguiente. En ese momento seguían pendientes el instante exacto
  de aparición de los elementos del menú y la temporización. Detalle en
  [`S3_MENU_CAMERA_SEQUENCE.md`](S3_MENU_CAMERA_SEQUENCE.md).
- **Controlador de cámara del menú (2026-11-13):** el fichero de datos es
  **`level\map7.dfx`**, abierto por `0x461B90` (fopen/fseek/ftell/malloc/fread/
  fclose) en el **mismo instante** en que la cámara empieza a volar
  (25,766 s); `0x461E0B` devuelve su **imagen compactada** —la región que
  sigue a la tabla de relocalización, con `aligned = (count + 0x200) & ~0x1FF`—
  que es exactamente la región que **el parser del port ya extrae** (`gex_
  dfx_container.image`): no hace falta E/S nuevo para llegar a ella. El
  **encuadre final del menú `(2243, 487, 196)` es un literal de esa imagen**
  (offset `0xC356`, una sola ocurrencia) y la posición del nodo está en `+0x28`
  (int16 ×3). El **avance es por frame**: un contador int16 en `+0x46` del
  estado global `0x006466D8` suma 1 en cada frame ⟹ en el port la unidad son
  los **428 frames**, no los 17,1 s (a 60 fps serían ≈ 7,1 s). Se **corrige**
  lo publicado el 2026-11-12: `0x200` es la **alineación** de la imagen del
  `.DFX`, no un tamaño de registro de 0x200 B. Quedaban pendientes entonces la
  representación de la trayectoria y la rutina por frame que escribe la
  posición; ambas se cerraron en las dos entradas siguientes. Siguen pendientes
  el papel de `+0x10/+0x12/+0x14` y el estado de menú. Detalle en
  [`S3_MENU_CAMERA_SEQUENCE.md`](S3_MENU_CAMERA_SEQUENCE.md).
- **Escritor de la cámara del menú (2026-11-13):** con **breakpoints de
  escritura** (watchpoints de 4 B, nueva opción `watch:<addr>` del trazador del
  repo) sobre `cam+0x00/+0x04/+0x30/+0x34` se identifica **qué instrucción
  escribe** cada campo durante el vuelo: la **posición** la escriben
  `0x411BBB`/`0x411BCC`/`0x411BDD` (427 escrituras = **una por frame**, del
  frame 1 al 428, y **dejan de escribirse** al terminar el vuelo) y la
  **orientación** `0x4122E8`/`0x4122F6`/`0x412304` (563 = una por frame
  siempre). El bloque de la posición **copia tres `int16`** desde un puntero que
  devuelve `0x53E98C` → `0x53EF00` → `0x53DEB0`, que rellena el buffer global
  `0x5AC608`. `0x53EF00` navega un **array de registros** (dentro de la imagen
  del `.DFX`) indexado por un **nodo** con el índice (`int16`) y la fracción de
  interpolación (15 bits), e interpola entre el registro actual y el
  siguiente. Quedaba pendiente quién avanza el nodo y la curva exacta; se
  cerraron en la entrada siguiente, y el contador de `+0x46` **no** gobierna la
  selección. Detalle en
  [`S3_MENU_CAMERA_SEQUENCE.md`](S3_MENU_CAMERA_SEQUENCE.md) § 4.8-4.10.
- **Trayectoria del menú implementada y validada en el port (2026-11-13):**
  las tres rutinas medidas (`0x53F252` avance, `0x53EF00` evaluación, `0x53DEB0`
  Hermite Q15) están reproducidas con **aritmética entera** en
  `port/include/gex/camera_path.h` + `port/src/camera/camera_path.c`, sin coma
  flotante en ninguna parte. El módulo recibe los **26 registros ya validados**
  (32 B: `D`, punto y tangentes) y reproduce el avance `+1` de la fracción de
  15 bits, el cambio de segmento al llegar a `D`, el centinela y el truncado a
  `int16`. El origen de los registros se reconstruyó siguiendo la **cadena real
  de punteros** del `.DFX` (header `0x5FC` → actor `0x95EC` → `+0x28` →
  entidad `0xC00C` → objeto `0xC02C` → array `0xC034`), no por escaneo de
  bytes; los 832 B coinciden con el volcado runtime en `0x193E6F80`.
  **Validación exacta:** `port/tests/test_camera_path.c` → **1437
  comprobaciones, 0 fallos**, con las **428/428** posiciones medidas, la
  primera `(2341, 4071, 3074)`, la última `(2243, 487, 196)` y ninguna
  escritura posterior al centinela; `run_sanitizers.ps1` → O0/O2/asan/ubsan
  24/24. **No** se declara la cámara completa: faltan la rotación (registros de
  10 B / cuaternio / Euler) y el estado de menú. La conexión de la trayectoria
  de posición al renderer se cerró en la tanda siguiente. Detalle en
  [`S3_MENU_CAMERA_SEQUENCE.md`](S3_MENU_CAMERA_SEQUENCE.md) § 4.5, § 6 y § 8.
- **Trayectoria del menú conectada al renderer D3D11 (2026-11-13):** la
  integración cierra el circuito completo. (a) **Lectura de los registros
  reales**: `gex_dfx_load_camera_path()` recorre la cadena de punteros dentro de
  la imagen compactada del `.DFX` (mismos offsets que la cadena documentada) y
  **valida** (variante, cantidad, límites, centinela); AZTEC16 y CIRCUIT0 se
  rechazan, no se falsea una trayectoria. (b) **Escena**:
  `gex_scene_level_load_camera_path()` deja el nodo en el estado original y
  `gex_scene_level_camera_path_step()` hace por frame el paso de trayectoria,
  escribe **solo la posición** y **re-emite** el nivel con la cámara nueva
  (mismo BSP, misma emisión, misma geometría: 0,50 ms por frame medido).
  (c) **Renderer**: `gex_d3d11_mesh_update()` sustituye el contenido de los
  buffers; la app engancha el paso al **update lógico** (`--camera-path`) sin
  tocar pipeline, shaders, atlas, *blending*, niebla ni orden de dibujo.
  **Validación:** tres suites nuevas y exactas — `test_camera_path_scene`
  (2567 comprobaciones: cadena, 26 registros, 428 posiciones, centinela,
  niveles ajenos), `test_camera_path_tsv` (1728: las 428 posiciones contra el
  **TSV leído en tiempo de ejecución**) y `test_camera_path_render` (25: el
  **buffer de vértices leído de vuelta de la GPU cambia** al mover la cámara) —
  `build.ps1 test` sin fallos y `run_sanitizers.ps1` **32/32**. **Ejecución
  real** (ventana Win32, GPU hardware): la cámara recorre la escena durante los
  428 updates (los triángulos visibles cambian de 1123 a 41 y a 362 según el
  punto de vista) y **termina en `(2243, 487, 196)`**, el encuadre del menú.
  **Límites declarados:** la **orientación no se interpola** (la ruta de
  rotación de 10 B sigue sin decodificar, así que la parte rotacional de la
  matriz no cambia: solo la traslación), y el atlas de texturas (64 entradas)
  cubre el **91,3 %** de los vértices del vuelo, que tocan 166 texturas
  distintas (con el atlas del frame 0 sería el 65,4 %). Detalle en
  [`S3_MENU_CAMERA_SEQUENCE.md`](S3_MENU_CAMERA_SEQUENCE.md) § 6.4.
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
  el *resize* de la ventana y la animación. MAP7 ya se renderiza con la
  trayectoria de posición real conectada al renderer D3D11: 428 updates,
  re-emisión de la escena y buffers GPU actualizados, con ejecución Win32
  verificada. La orientación todavía no se interpola y el estado del menú
  sigue sin aislarse. También sigue pendiente la validación dinámica completa
  de S-2 contra el ejecutable original. `PROJECT_STATUS.svg` no cambia
  (MAIN 40 %).

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