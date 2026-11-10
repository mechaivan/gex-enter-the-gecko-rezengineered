# CHANGELOG

Formato: `YYYY-MM-DD - versión - descripción`.

## 2026-11-10 - 0.0.0 - Decimoquinta correccion (aritmetica de 32 bits con wrap x86 en BSP)

- Hallazgo: `gex_cam_plane_distance()` (`port/src/camera/camera.c`) sumaba los
  tres productos en `int32_t`. Con coordenadas grandes la suma supera
  `INT32_MAX` (o baja de `INT32_MIN`) y el desbordamiento con signo es
  comportamiento indefinido en C, aunque el original x86 envuelve el registro
  de 32 bits. El `>> 12` sobre un valor negativo tambien era de implementacion
  definida, y la distancia `+0x14` se leia con un cast `int32_t* -> uint32_t*`
  (violacion de aliasing estricto).
- Correccion en `camera.c`:
  - `gex_cam_plane_distance()` acumula en `uint32_t` (modulo 2^32), aplica
    `sar12()` sobre los bits del registro y resta `distance` con `sub_wrap()`.
  - `sar12()` se implementa sobre `uint32_t` con mascara `0xFFF00000`, sin
    depender de `>>` sobre negativos.
  - `gex_cam_sphere_visible()` usa `sub_wrap()`/`add_wrap()` para
    `T_A/B[i] - a_cam/b_cam[i]`, `-r` y `far + r`.
  - Se anaden `bits_i32()`, `bits_i16()`, `low_i16()` y `neg_i16()` (copias de
    bits con `memcpy` / truncado a 16 bits) y se usan en todas las conversiones
    que antes dependian de la implementacion: productos de matrices, `neg` de
    16 bits en las rotaciones y en `M2`, truncado de A/B, normalizacion y
    marco local. `norm_angle()` calcula el signo con una comparacion en lugar
    de `>> 31`.
- Correccion en `bsp.c`: la distancia `+0x14` se lee en un `uint32_t` local y
  se reinterpreta con `bits_i32()`; centros y normales usan `bits_i16()`. Sin
  casts de punteros ni conversiones de implementacion definida.
- Pruebas nuevas:
  - `test_camera`: modelo de referencia x86 independiente (`ref_sar12`,
    `ref_add32`, `ref_sub32`, `ref_plane_distance`, `ref_sphere_visible`) y
    casos deterministas: suma superior a `INT32_MAX` (3 x 32767^2 ->
    `-262192`), suma inferior a `INT32_MIN` (3 x -32768 x 32767 -> `262168`),
    resultado negativo antes de `sar 12` (-100), `0 - INT32_MIN`,
    `5 - INT32_MAX`, `-5 - INT32_MAX` (wrap a `2147483644`), 512
    configuraciones aleatorias de distancia de plano y 256 de visibilidad
    comparadas contra el modelo. Total: **889 comprobaciones, 0 fallos**.
  - `test_bsp`: distancia cruda `0x80000000` (`INT32_MIN`) y orden de hijos
    resultante, mas el caso `distance = 0`. Total: **37 comprobaciones,
    0 fallos**; AZTEC16 sigue en 815 hojas.
- Verificacion: `.\build.ps1 test` completo pasa; ambas baterias tambien
  pasan con `clang -fsanitize=address,undefined` y con `gcc`.
- Documentacion: `S2_CAMERA_VISIBILITY.md` (nueva subseccion de auditoria
  aritmetica y cifras), `S2_BSP_TRAVERSAL.md` (lectura de la distancia,
  historial 14.ª y cifras), `PROJECT_STATE.md`, `ROADMAP.md`,
  `docs/D3D11_FIRST_SCENE_PLAN.md` y contrato de `port/include/gex/camera.h`.
- Sin cambios de `PROJECT_STATUS.svg` (MAIN 40 %). S-2 **no** se declara
  completo: la emision de poligonos, la integracion D3D11 y la validacion
  dinamica contra el ejecutable siguen pendientes.

## 2026-11-10 - 0.0.0 - Decimocuarta correccion (auditoria de orientacion y visibilidad de camara)

- Auditoria estatica del commit `a3f433e` contra el desensamblado de
  `GEX3D.EXE`; cinco defectos corregidos en `port/src/camera/camera.c`:
  - `gex_cam_build_visibility_matrices()` calculaba `a_cam`/`b_cam` con `M` en
    lugar de con `A`/`B`. Ahora `a_cam = transform3(A, pos)` y
    `b_cam = transform3(B, pos)` (B completo, incluida su fila 2).
  - `gex_cam_sphere_visible()` calculaba `T_A`/`T_B` con `M` y volvia a
    transformar `pos`. Ahora usa `A`/`B` y lee `cam->a_cam`/`cam->b_cam`.
  - `gex_cam_build_local_frame()` truncaba los productos vectoriales a `int16`
    antes de `sar 12`; ahora el producto es `int32`, luego `sar 12` y despues
    truncado a `int16`.
  - La normalizacion aproximada ordenaba los valores con signo y permutaba las
    componentes; ahora ordena solo magnitudes y cada componente conserva su
    posicion.
  - `tab_sin`/`tab_cos` negaban el angulo y el builder volvia a negar los
    angulos de Euler. Ahora las identidades de cuadrante son correctas
    (`tab_cos` = `S[0x400-a]`, `-S[a-0x400]`, `-S[0xC00-a]`, `S[a-0xC00]`) y el
    builder pasa los angulos crudos, en orden Z -> Y -> X.
- `gex_cam_mat_mul` y el transformador de vectores usan acumulador `uint32`
  para reproducir el wrap de 32 bits de x86 sin comportamiento indefinido.
- B fila 2 (`cam+0xB4..0xB8`) se preserva desde el llamador: el original la lee
  para `b_cam` pero no se localizo su escritor. Se documenta como decision del
  port y nunca se compara en la visibilidad.
- `port/tests/test_camera.c` reescrito: **103 comprobaciones, 0 fallos**.
  Cubre tablas exactas, rotaciones exactas por eje (positivas, negativas y con
  vuelta), composicion Z->Y->X, rama alternativa y base, marco local exacto,
  cableado M/A/B con matrices no triviales, `a_cam`/`b_cam`, preservacion de B
  fila 2, limites de los seis semiplanos, radio unsigned, orden de hijos y
  predicado de hoja. La prueba BSP sintetica se desacopla del frustum (centros
  en el origen).
- `port/build.ps1` y `port/Makefile`: `test_camera` enlaza `src/bsp/bsp.c`;
  nuevo objetivo `test_bsp` con `-DGEX_BSP_TEST_FAULT_INJECT`, `bsp.c`,
  `camera.c`, `dfx.c`, `sag.c` y `-lm`. `.\build.ps1 test` completo pasa
  (camera 103, BSP 29, resto de suites OK); ASan/UBSan y `gcc` limpios.
- Documentacion actualizada: `S2_CAMERA_VISIBILITY.md` (angulos crudos,
  formulas de rotacion, valores normalizados, §11 y correcciones de auditoria),
  `S2_BSP_TRAVERSAL.md` (estado, comando de test, predicado `& 1`, historial
  13.ª), `PROJECT_STATE.md`, `ROADMAP.md` y
  `docs/D3D11_FIRST_SCENE_PLAN.md` (Modulo 2, Hito 2, siguiente hito).
- Limitaciones declaradas: las tablas sin/cos se generan como
  `round(4096*sin/cos(2*pi*i/4096))` y **no** se ha demostrado equivalencia bit
  a bit con las tablas retail; sigue sin haber validacion dinamica contra el
  ejecutable. S-2 no se declara completo: la emision de poligonos y la
  integracion D3D11 siguen pendientes. Sin cambios de `PROJECT_STATUS.svg`
  (MAIN 40 %).

## 2026-11-09 - 0.0.0 - Decimotercera correccion (implementacion de orientacion y visibilidad de camara en port/)

- Implementada la cadena de orientacion de camara en `port/`:
  `port/include/gex/camera.h`, `port/src/camera/camera.c`,
  `port/tests/test_camera.c` (39 pruebas deterministas, 0 fallos).
  Funciones: `gex_cam_init_tables()`, `gex_cam_build_orientation()` (0x46DD40),
  `gex_cam_orientation_finalize()` (0x46E643), `gex_cam_build_local_frame()`
  (0x402DC0), `gex_cam_build_visibility_matrices()` (0x405C41),
  `gex_cam_update()`, `gex_cam_sphere_visible()`, `gex_cam_plane_distance()`.
  Aritmetica Q12 exacta (int16 con signo, sar 12, idiv truncado, sin saturacion,
  sin punto flotante). Tablas sin/cos generadas matematicamente (BSS original
  no se localizo estaticamente).
- Implementada la prueba de visibilidad original (0x00401C00) en
  `port/src/bsp/bsp.c`: `gex_bsp_walk()` acepta `cam_ctx` opcional; cuando se
  proporciona aplica prueba de esfera contra 6 semiplanos y orden de apilado
  basado en `d = ((camPos·N) >> 12) − distance`; predicado de hoja corregido a
  `(tipo & 1) == 0`.
- `port/tests/test_bsp.c` actualizado a la nueva firma de `gex_bsp_walk`;
  29 pruebas, 0 fallos; AZTEC16 = 815 hojas. Todas las pruebas pasan con
  ASan/UBSan.
- Actualizados `S2_CAMERA_VISIBILITY.md` (secciones 11-12),
  `S2_BSP_TRAVERSAL.md` (§4, auditoria 12.ª), `PROJECT_STATE.md` (fecha
  2026-11-09, estado de hitos), `ROADMAP.md` (S-2),
  `docs/D3D11_FIRST_SCENE_PLAN.md` (Módulo 2, Hito 2).
- `port/build.ps1` y `port/Makefile`: agregado `test_camera` al target `test`.
- Sin cambios de `PROJECT_STATUS.svg` (MAIN 40 %). S-2 no se declara completo
  mientras falte integracion con emisión y validacion dinamica.
- **Nota (2026-11-10):** la auditoria de la entrada del 2026-11-10 (decimocuarta
  correccion) encontro cinco defectos en la implementacion descrita aqui:
  `a_cam`/`b_cam` y `T_A`/`T_B` se calculaban con `M` en lugar de `A`/`B`, el
  `sar 12` del marco local llegaba tarde, la normalizacion permutaba
  componentes y la trigonometria negaba el angulo dos veces. Las cifras de esta
  entrada (39 pruebas) y la lectura de «angulos negados» quedan superadas por
  la decimocuarta correccion.

## 2026-11-09 - 0.0.0 - Undecima correccion (visibilidad original de camara documentada)

- Nuevo `S2_CAMERA_VISIBILITY.md`: ingenieria inversa estatica y de solo
  lectura de `0x00401C00` (firma cdecl, pila, 6 semiplanos, radio sin signo),
  de `0x0046999C` (transformacion 3x3 int16 con `sar 12`, sin saturacion), de
  `0x00405C41` (matrices A/B, `cam+0xC8`, `cam+0xD4`), de `0x00402DC0`
  (vectores locales y normalizacion aproximada) y de la cadena
  `0x46DD40` -> `0x46E643`.
- Correcciones documentales: `cam+0x88` son 9 int16 (no 7) y existe B en
  `cam+0xA8`; `cam+0xC8/0xD4/0xD8` no son planos y `0x401C00` no los lee; el
  predicado de hoja es `(tipo & 1) == 0`; el orden de apilado depende del
  plano. Notas de correccion en `U1H_RESULTADOS.md` §6.1 y
  `U1_CONSOLIDACION.md` §4.1.
- Actualizados `S2_BSP_TRAVERSAL.md`, `PROJECT_STATE.md`, `ROADMAP.md` y
  `docs/D3D11_FIRST_SCENE_PLAN.md`. Sin cambios de codigo, pruebas ni
  PROJECT_STATUS.svg (MAIN 40 %).

## 2026-11-09 - 0.0.0 - Duodecima correccion (orientacion de camara y matriz M documentadas)

- Reconstruccion de la cadena de orientacion de camara (`0x46DD40` ->
  `0x46E643` -> `0x405C41`) documentada en `S2_CAMERA_VISIBILITY.md` §6:
  rotaciones Euler Z -> Y -> X (pre-multiplicacion, angulos negados) sobre
  `cam+0x30/0x32/0x34` (rama normal) o `cam+0x128/0x12A/0x12C` (rama
  alternativa si `[0x551644+0xa0] & 0x8`); matriz base `*[cam+0xF0]` o
  identidad si `cam+0x43C != 0`; cambio de ejes `M2 =
  4096*[[1,0,0],[0,0,-1],[0,1,0]]`; `O = M2 · R`; `M = *[cam+0xE8] =
  transpose(O) = R^T · M2^T`; `0x405C41` construye A (`cam+0x88..0x98`) y B
  (`cam+0xA8..0xB2`) aplicando M a los vectores locales de `0x402DC0`, y
  escribe `cam+0xC8 = A·camPos` y `cam+0xD4 = B·camPos`.
- Ejes verificados instruccion a instruccion: `0x46CD38` = ejes Z (fila
  2 identidad), `0x46C3A4` = ejes Y (fila 1 identidad), `0x46BA10` = ejes X
  (fila 0 identidad). `0x4698B0` rota un vector por la matriz global con
  `sar 12`.
- Incógnitas restantes: `cam+0xB4/0xB6/0xB8` (fila 2 de B) leida pero nunca
  escrita por la cámara; escalado por columna `cam+0x490/0x492/0x494`;
  consumidor de `cam+0x14/0x18/0x1C` y de `cam+0x43C`.
- Actualizados `S2_BSP_TRAVERSAL.md` (§4 e historial de auditorías, 11.ª
  pasada), `U1_CONSOLIDACION.md` (§4.1), `PROJECT_STATE.md` (línea S-2 e
  hitos), `ROADMAP.md` (S-2). Sin cambios de codigo, pruebas ni
  PROJECT_STATUS.svg (MAIN 40 %).

## 2026-11-08 - 0.0.0 - Decima correccion (descripciones de la Subfase E en ROADMAP)

- ROADMAP.md: S-1 y S-2 describen la implementacion existente (parser DFX y
  recorrido estructural validados) en lugar de ordenar su implementacion.
  Se separan las partes pendientes: integracion del escenario, visibilidad
  original de camara y emision de poligonos. Subfase E sigue EN CURSO y
  S-1/S-2 no se declaran completos.
- Sin cambios de codigo, pruebas ni PROJECT_STATUS.svg (MAIN 40 %).

## 2026-11-08 - 0.0.0 - S-2: novena correccion (sincronizacion del estado del proyecto)

- Eliminadas contradicciones residuales en PROJECT_STATE.md y ROADMAP.md:
  S-1 figura como implementado, la Subfase E como EN CURSO y el recorrido BSP
  como estructuralmente implementado con visibilidad/emision pendientes.
- D3D11_FIRST_SCENE_PLAN.md: modulos 1 y 2 alineados con el codigo real
  (rutas y API), Hito 2 marcado parcial, siguiente paso corregido.
- Sin cambios de codigo. 29 comprobaciones, 0 fallos. ASan+UBSan limpios.
  AZTEC16 = 815 hojas. PROJECT_STATUS.svg sin cambios (MAIN 40 %).

## 2026-11-08 - 0.0.0 - S-2: octava correccion BSP (consolidacion documental)

- Consolidacion de S2_BSP_TRAVERSAL.md, correcciones en D3D11_FIRST_SCENE_PLAN.md,
  ROADMAP.md, PROJECT_STATE.md. Sin cambios de codigo.
- 29 comprobaciones, 0 fallos. ASan+UBSan limpios. AZTEC16 = 815 hojas.

## 2026-11-08 - 0.0.0 - S-2: septima auditoria BSP (compilacion verificable y cierre documental)

- Compilacion normal sin inyeccion verificada (sin simbolos). Compilacion de
  tests con -DGEX_BSP_TEST_FAULT_INJECT verificada. 29 comprobaciones, 0 fallos.
  ASan+UBSan limpios. AZTEC16 = 815 hojas.

## 2026-11-08 - 0.0.0 - S-2: sexta auditoria BSP (aislamiento definitivo)

- Declaraciones de hooks de test eliminadas de bsp.h; declaradas solo en
  test_bsp.c. Compilacion normal sin estado global.
- 29 comprobaciones, 0 fallos. ASan+UBSan limpios. AZTEC16 = 815 hojas.

## 2026-11-08 - 0.0.0 - S-2: quinta auditoria BSP (aislamiento de fallos)

- Mecanismo de inyeccion de fallos movido a compilacion condicional; eliminado
  de la API publica. Pruebas deterministas de fallo de primera/segunda reserva.
- 72 comprobaciones, 0 fallos. ASan+UBSan limpios. AZTEC16 = 815 hojas.

## 2026-11-08 - 0.0.0 - S-2: cuarta auditoria BSP (memoria y pruebas de fallos)

- grow_visited recibe nvis y copia solo elementos validos; node_in_bounds usa
  resta anti-overflow; inyeccion de fallos de asignacion en tests.
- ASan+UBSan ejecutados sin errores. 44 comprobaciones, 0 fallos.
- AZTEC16 = exactamente 815 hojas.

## 2026-11-08 - 0.0.0 - S-2: recorrido BSP estructural de AZTEC16

- **Alcance:** módulo BSP en `port/` (`gex_bsp_walk`, predicado de visibilidad
  opcional), sin buffers, shaders ni integración con renderer.
- **Documento nuevo:** `S2_BSP_TRAVERSAL.md`.
- **Hechos:** AZTEC16 produce 815 hojas con predicado NULL; visibilidad original
  (cámara) documentada como dependencia no resuelta.
- **Pruebas:** 7 comprobaciones, 0 fallos.
- **`PROJECT_STATUS.svg`:** sin cambios.

## 2026-11-08 - 0.0.0 - S-1: parser del contenedor DFX (primer hito)

- **Alcance:** implementación del parser del contenedor `.DFX` en `port/`
  (`gex_dfx_parse`, `gex_dfx_load`, `gex_dfx_relocate`, `gex_dfx_load_level`),
  sin recorrido BSP, sin buffers D3D11, sin shaders ni texturas.
- **Documento nuevo:** `S1_DFX_PARSER.md` (arquitectura, formato, API, memoria,
  validaciones, pruebas, limitaciones).
- **Hechos:**
  - Formato implementado: `count` + offsets + imagen en `4*aligned`,
    `aligned = (count+0x200)&~0x1FF`; imagen lógica = `size - 4*aligned`.
  - Descriptor leído desde `image + image[0]`.
  - Relocalizador independiente con `delta` explícito (`gex_dfx_relocate_raw`).
  - `0x00461ECA` no implementado (sin llamadores; copia truncada).
- **Pruebas:** 76 comprobaciones, 0 fallos; validación con AZTEC16.DFX
  (16 082 entradas, 3 882 vértices) y GEX.DFX (322 entradas).
- **`PROJECT_STATUS.svg`:** sin cambios (MAIN sigue en 40 %; S-1 no completo).

## 2026-11-08 - 0.0.0 - Investigación de la variante alternativa del cargador DFX (0x00461ECA)

- **Alcance:** análisis estático instrucción a instrucción de `0x00461ECA`
  (variante del cargador `.DFX`), contrastado con `0x00461E0B`, el relocalizador
  `0x004735C0` y los bytes de `AZTEC16.DFX`/`GEX.DFX`. Sin ejecutar el juego,
  sin modificar `C:\GEX_REZ`, el port ni los assets, sin instalar herramientas y
  sin crear commits durante la investigación.
- **Documento ampliado:** `U1_DFX_RELOCATION_AUDIT.md` con la sección `H`
  (firma, pseudocódigo, tabla comparativa, cuantificación de la copia truncada,
  búsqueda de llamadores, implicaciones para S-1) y actualización de §F.
- **Hechos fijados:**
  - Firma real: `void *dfx_load_at(const char *path, void *dest)` (`cdecl`,
    2 argumentos, retorno en `EAX`).
  - Comparte lectura (`0x461B90`), cabecera, alineación, localización de la
    imagen y relocalizador; difiere en delta (`dest`), destino externo, copia
    2 048 B más corta, ámbito de arena (`0x4650B0`/`0x4650FD`) y retorno
    `dest`.
  - Si `dest == -1`: `dest = 0x801FC000 − roundUp4(tamaño)` (aritmética
    confirmada; el buffer completo termina en `0x801FC000`).
  - `0x00461ECA` **no tiene llamadores estáticos** en este build (sin
    `call`/`jmp`, sin VA literal, sin `.reloc`, sin exports).
  - Aplicada a `AZTEC16.DFX`: copia 477 004 B (2 046 B menos que la imagen
    lógica) y deja 22 punteros colgantes + 99 posiciones sin copiar; a
    `GEX.DFX` no pierde punteros.
- **Consolidación:** actualizados `U1_CONSOLIDACION.md`, `U1G_RESULTADOS.md`,
  `U1H_RESULTADOS.md`, `docs/U1_GEOMETRY_RECOVERY_PLAN.md`,
  `docs/D3D11_FIRST_SCENE_PLAN.md`, `ROADMAP.md`, `PROJECT_STATE.md`.
- **S-1:** no implementado ni iniciado. La variante no requiere una segunda
  ruta de carga (no se usa y su copia truncada rompe `AZTEC16`).
- **`PROJECT_STATUS.svg`:** sin cambios (MAIN sigue en 40 %, 14/35 puntos y
  6/12 hitos).

## 2026-11-08 - 0.0.0 - Auditoría definitiva de la carga y relocalización DFX

- **Alcance:** verificación estática instrucción a instrucción del cargador `.DFX` (`0x00461E0B`), del relocalizador (`0x004735C0`), del asignador/liberador de arena (`0x00464FB3`, `0x0046506B`) y del almacenamiento en `level[+4]` (`0x0043B2A9`), contrastada con los bytes reales de `AZTEC16.DFX` y `GEX.DFX`. Sin ejecutar el juego, sin modificar `C:\GEX_REZ`, el port ni los assets, y sin instalar herramientas.
- **Documento nuevo:** `U1_DFX_RELOCATION_AUDIT.md` (secciones A–G), referencia técnica principal de la mecánica del contenedor.
- **Corrección de transcripción resuelta:** `0x00461E62` recarga `ECX` con `aligned`; `imageBase = B + 4*aligned` es correcto y no dependía de `count`.
- **Hechos fijados:**
  - `aligned = (count + 0x200) & ~0x1FF` en **DWORDs**; imagen en `4*aligned`.
  - Relocalizador: `*(u32*)(imageBase + off) += delta`, con `imageBase = B + 4*aligned` y **`delta = B`**.
  - Compactación de `4*(roundUp4(tamaño)/4 − aligned)` bytes desde `B + 4*aligned` hasta **`B` (offset 0)**; después libera `4*aligned` y devuelve `B`.
  - `AZTEC16.DFX`: 544 586 B, `count` 16 082, `aligned` 16 384 dwords, imagen en `0x10000`, 479 050 B lógicos, copia de 479 052 B (2 B de padding), relleno de 1 204 B a cero.
  - `GEX.DFX`: 146 008 B, `count` 322, `aligned` 512 dwords, imagen en `0x800`, 143 960 B, relleno de 756 B a cero; `image[0]` no se relocaliza.
- **Cifras históricas refutadas:** `count` 131, imagen en 528, imagen GEX en 1 292, `aligned` 2 048 B para ambos, imagen GEX de 143 360 B, `delta = 0` y compactación a `B+4`. Causa común: confundir `4 + 4*count` (fin de tabla) con `4*aligned` (inicio de imagen).
- **Pendientes expresamente abiertos:** identidad exacta de los thunks de E/S,
  generalización de `[0x647FAC]`, llamadas calculadas (variante `0x00461ECA`
  analizada el mismo día, §H) y direcciones absolutas en ejecución.
- **S-1:** no implementado ni iniciado. `PROJECT_STATUS.svg` no cambia (MAIN sigue en 40 %, 14/35 puntos y 6/12 hitos).

## 2026-11-07 - 0.0.0 - Planificación: Primer Escenario Original en Direct3D 11

- **Alcance:** planificación técnica para transformar geometría BSP de niveles en datos renderizables por el port D3D11. Revisión completa de documentación U-1, arquitectura renderer existente y componentes D3D implementados. Sin implementar código, modificar assets ni ejecutar juego.
- **Nivel de prueba seleccionado:** AZTEC16.DFX (el más completo y mejor documentado con 544,586 B, 3,882 vértices, 9,609 polígonos, 3,462 normales).
- **Arquitectura propuesta:** 5 módulos (Parser DFX, Recorrido BSP, Generador de Buffers, Cargador de Texturas, Integración Renderer) con flujo completo desde DFX hasta renderizado D3D11.
- **Formatos demostrados:** contenedor .DFX, descriptor de nivel (10/15 campos demostrados), nodo BSP, polígono, vértice, normal, auxiliar UV, y 8 opcodes demostrados.
- **Hitos definidos:**
  1. Parser DFX Operativo - leer AZTEC16.DFX y extraer descriptor
  2. Recorrido BSP Funcional - implementar algoritmo de `0x401C00`
  3. Buffers D3D11 Generados - convertir vértices DFX (12B) a formato D3D11
  4. Texturas Cargadas - cargar .VFX y crear texturas D3D11
  5. Primer Renderizado Completo - renderizar AZTEC16 con D3D11
- **Bloqueos reales:** Shaders (no documentados en U-1), formato NCC (conversión a D3D11), sistema de coordenadas.
- **Siguiente hito concreto:** Implementar Módulo 1 (Parser DFX) con `gex_dfx_parse` y `gex_dfx_load_level`.
- **Evidencia clave:** `U1_CONSOLIDACION.md` (geometría BSP demostrada), código R-1 a R-4 (renderer base), AZTEC16.DFX (caso de prueba completo).
- **Documentación:** `docs/D3D11_FIRST_SCENE_PLAN.md` (plan técnico detallado), actualizados `ROADMAP.md`, `PROJECT_STATE.md`.

## 2026-11-07 - 0.0.0 - U-1: Consolidación documental final

## 2026-11-07 - 0.0.0 - U-1h: Auditoría final y corrección de afirmaciones

- **Alcance:** relectura estática de todas las conclusiones de U-1h contra `GEX3D.EXE`, `gex_full.asm` y los assets reales; sin ejecución, sin ventanas, sin modificar binarios ni assets. Nuevo §18 en `U1H_RESULTADOS.md` con método, correcciones y evidencias.
- **Correcciones de precisión:**
  - `0x4024B9` usa el radio de la **esfera de consulta** + `320`, no el radio del nodo; el radio `+0x06` solo lo usa `0x401C00`.
  - `0x24`/`0x2C` leen el índice de textura en `rec+0x1A`; `0x34`/`0xB4` en `rec+0x1E`; `0x24` lee UV2 en `+0x20/+0x21`, no `word[aux+2]`.
  - Rango no-op real: `0x38..0x3C`, `0x44`, `0x50..0xB0` y `0xB8..0xEC`.
  - Handler `0x30`: solo `guColorCombineFunction` + `grDrawTriangle`, **sin** `grConstantColorValue4`; tabla Glide por handler corregida y thunks verificados contra los imports de `glide2x.dll`.
  - Handler `0x2C`: es un **cuadrilátero** de 4 vértices (stride 8) que dibuja **2 triángulos**, no un triángulo; longitud `0x2C` confirmada. No se localizó un emisor que escriba el opcode `0x2C` de forma literal.
  - `0x64FB40` (no `0x0054FB40`): reencadenado `0x462240..0x4622AB`; `+0x12 = 0x180` o `0xE0` según `[level+0x6FB0]`, `+0x10 = 0x200`, `+0x00 = 0x14`; opcode `0xF0` escrito en la inicialización `0x4635BD`.
  - La descarga de tabla ocurre en `0x24`, `0x2C` y `0xB4` (no en `0x461C32`).
- **Hipótesis degradada:** la consulta esférica `0x4024B9` + `0x5015A9` demuestra el marcado `0x8000 | 0x40` de polígonos `0x20`, pero **no hay ningún polígono `0x20`** en `AZTEC16`, `CIRCUIT0` ni `REZOP2`; su uso como *pickups*/triggers no está confirmado.
- **Cita corregida:** U-1g §7.3 no afirmó «recorrido de dibujo»; dejó el recorrido como hipótesis sin algoritmo. U-1h demuestra que `0x41A203` es colisión segmento–BSP.
- **Documentación:** `U1H_RESULTADOS.md` §18, `U1G_RESULTADOS.md`, `ROADMAP.md`, `PROJECT_STATE.md` y `docs/U1_GEOMETRY_RECOVERY_PLAN.md`.

## 2026-11-06 - 0.0.0 - U-1h: Interpretación final del flujo 3D y del grafo `.DFX` completada

- **Semántica del flujo 3D y estructura del `.DFX` cerrada de extremo a extremo; Subfase C (U-1) terminada.**
  Contenedor validado en `AZTEC16.DFX`, `CIRCUIT0.DFX` y `REZOP2.DFX`: `count` + tabla de `count` offsets + relleno hasta `aligned = (count + 0x200) & ~0x1FF` dwords; imagen en `4*aligned`, offsets relativos a la imagen.
- **Hechos demostrados:**
  - Descriptor de nivel (`image[0]`): raíz BSP `+0x00`, nº/tabla de registros de 12 B `+0x10/+0x14`, vértices `+0x18`, polígonos `+0x1C`, normales `+0x20`, tabla de vértices `+0x24` (12 B), inicio de polígonos `+0x28` (0x14 B), tabla de normales `+0x2C` (6 B), objetos `+0x30`, tabla no identificada `+0x34`. Encadenado exacto en los tres niveles: `[+0x28]+nPolys*0x14 == [+0x24]` → `+nVerts*12 == [+0x2C]` → `+nNormals*6 == [+0x00]`.
  - Nodo BSP `0x20` B, unión interno/hoja: centro `+0/+2/+4`, radio `+0x06`, normal unitaria (escala 4096) `+0x0C..+0x11`, distancia `+0x14`, hijos `+0x18/+0x1C`; hoja con puntero y contador de polígonos y AABB. `+0x12` es tipo exacto: `1` interno, `2` hoja (2 807 nodos, sin otros valores). Hijo A = lado positivo de la normal.
  - Polígono `0x14` B: índices `+0/+2/+4`, flags `+0x06`, 4 índices con signo a normales, puntero a registro UV de 12 B en `+0x10`. Flags con consumidor: `0x8000` oculto, `0x4000` fuera de colisión, `0x0040` especial, `0x0020` marca de la consulta esférica (hipótesis: trigger).
  - Registro auxiliar UV 12 B: `byte[+0]=U0`, `byte[+1]=V0`, `byte[+4]=U1`, `byte[+5]=V1`, `byte[+8]=U2`, `byte[+9]=V2`; `word[+6]` = índice de textura de 14 bits, y el emisor añade `0x4000`.
  - Ruta de dibujo: `0x401C00` recorre el BSP y anexa hojas visibles a `[0x64FEA4]` (máx. 0x800); `0x4269C2` las emite; `0x426256`/`0x426588` escriben los comandos; `0x469836` recorre la lista y `0x46558A` despacha a Glide.
  - `0x41A203` = colisión segmento–BSP; `0x4024B9` + `0x5015A9` = consulta esférica que marca polígonos `0x20` (hipótesis: *pickups*/triggers; no hay polígonos `0x20` en los tres niveles).
  - Tabla de despacho corregida (mapa `0x469765`, handlers `0x469735`): 11 comandos activos (`0x20`, `0x24`, `0x28`, `0x2C`, `0x30`, `0x34`, `0x40`, `0x48`, `0x4C`, `0xB4`, `0xF0`); el resto son no-op.
  - `0x64FB40` = rectángulo 2D de fondo opcode `0xF0`, `0x14` B, `0x200×0x180`, negro; se reencadena como último comando del frame.
  - `.VFX`: `record[+0x0C]` = `GrTexInfo.format`; `record[+0x14]` = datos NCC *inline* para `grTexDownloadTable`; `0x461C32` no descarga la tabla.
  - `image[+0x8C]` = puntero relocalizado al nombre de nivel de 12 caracteres en `image+0xE0`.
- **Anomalías de U-1g resueltas estáticamente:** el opcode se escribe siempre **después** del color; el handler de `0x30` es `0x4673E9` y consume exactamente `0x20` B. La tabla opcode→handler de U-1g estaba desplazada una fila.
- **`GEX.DFX`:** mismo contenedor, pero **no es un nivel**; `image[0]` no está relocalizado, no hay BSP ni descriptor de nivel. Es un modelo/personaje con tabla de frames y nombre `"gex______"`. No se generaliza el formato de nivel.
- **Resultados negativos:** sin consumidor localizado para `desc+0x10/+0x14`, `desc+0x30`, `desc+0x34`, `desc+0x38/+0x3C`, bits de flags sin marcar, segundo registro de `0x426588` ni formato del modelo `GEX.DFX`.
- **Pruebas:** análisis estático con `llvm-objdump` y `tools/gexmap/pe.mjs`; validación binaria *read-only* de tres niveles y `GEX.DFX`; sin ejecución, sin ventanas, sin instalación de herramientas.
- **Documentación:** `U1H_RESULTADOS.md` (18 secciones + 2 anexos; §18 = auditoría final), `U1G_RESULTADOS.md` (nota de corrección y tabla de opcodes), `ROADMAP.md`, `PROJECT_STATE.md`, `docs/U1_GEOMETRY_RECOVERY_PLAN.md`.

## 2026-11-06 - 0.0.0 - U-1g: Relación de la ruta 3D con DFX/VFX completada

- **Formato del flujo de comandos de `0x0046558A` y de los contenedores `.DFX`/`.VFX` establecido y validado con assets reales.**
  `0x0046558A` dibuja **un** comando por llamada; el recorrido lo hace `0x00469836` sobre una lista enlazada de registros con cabecera de 8 B (`[0]` = longitud/flag de dibujo, `[4]` = siguiente; `-1` = fin). El opcode está en `byte[+0xB]`, el prefijo `0xE1` ocupa 4 B y fija `[0x57B8EC]`, y los vértices van *inline*.
- **Hechos demostrados:**
  - Pool de comandos `0x64FF00 + 0x6000*level[+0x14]`, doble buffer `0x64FEE0`/`0x64FEE4`, head `buf+0x5FF8`, inicialización `0x00465530(buf, 0xC00)`.
  - Dispatcher: `cmd = opcode & ~3`, índice `cmd-0x20` en el mapa `0x00469765`, 12 handlers en `0x00469735` (`0x20`/`0x24`, `0x30`/`0x34`, `0x40`, `0x48`, `0x4C`, `0x50`, `0x60`, `0x70`, `0x80`, `0xB4`, `0xF0`).
  - Recurso: `word[+0x1A] & 0x3FFF` → tabla `0x006488F4` → registro `.VFX`; `+0x14` = nivel (`>>14`); `record[+0x0C]==1` activa `grTexDownloadTable`, y se llama a `grTexSource`/`grTexDownloadMipMap`.
  - `.VFX`: `count` + registros de `0x8C + [+0x84]` B; `GEX.VFX` (1 588 B, 4 registros) termina exactamente en `0x634`.
  - `.DFX`: `{count, offsets[]}` + imagen relocalizable; `0x004735C0` suma la base a cada dword apuntado; `0x00461E0B` compacta la imagen; `0x0043B11D` guarda el resultado en `level[+4]` y carga **primero** el `.VFX`.
  - `GEX.DFX` (146 008 B): `count=322`, `aligned=0x200` dwords, imagen de 143 960 B, raíz en `0x2E0`, 90 punteros relocalizados en los primeros 512 B.
- **Resultados negativos:** no hay referencias cruzadas `.DFX`↔`.VFX`, ni tabla de texturas en el `.DFX`, ni identificador de recurso por comando distinto del índice de 14 bits, ni buffer de vértices separado.
- **Cuestiones abiertas:** solapamiento `+0x8`/`+0xB` en los triángulos, longitud `0x20` del opcode `0x30` frente a la lectura de `+0x20/+0x21`, estructura interna del grafo `.DFX`, semántica de `record[+0x0C]` y de `+0x14` del `.VFX`.
- **Pruebas:** análisis estático con `llvm-objdump`, `tools/gexmap/pe.mjs` y `analysis/EU`; validación binaria de `GEX.DFX`/`GEX.VFX` en solo lectura. REA no viable (`missing_analysis_engine`), sin instalar herramientas.
- **Documentación:** `U1G_RESULTADOS.md` (metodología, encuadre, opcodes, recurso de 14 bits, emisores, cadena de carga, formatos, hechos/hipótesis/negativos), `ROADMAP.md`, `PROJECT_STATE.md`.

## 2026-11-05 - 0.0.0 - U-1f: Relación índices-recursos completada con corrección documental

- **Última fase de recuperación de geometría original completada con correcciones fundamentales.**
  U-1f analiza cómo se relacionan los índices con los recursos (texturas, etc.) y corrigió errores en U-1a...U-1e: `0x00432BBD` es un glifo 2D (no triángulos 3D) y `0x00432DD1` es un rectángulo 2D sólido (no geometría 3D). La geometría 3D real está en `0x0046558A` con flujo de comandos hacia la tabla de texturas `0x006488F4`.
- **Hechos demostrados:** 
  - `0x00432BBD`: emisor de glifos 2D, índice de glifo → celda 16×16 de `FONT.3DF`
  - `0x00432DD1`: emisor de rectángulos sólidos 2D, sin textura
  - Geometría 3D real en `0x0046558A`: opcode en `byte[+0xB]`, índice de 14 bits hacia `0x006488F4`
  - `FONT.3DF` cargado por `0x004329EF`, `.VFX` construye tabla de texturas, `.DFX` alimenta flujo 3D
- **Hipótesis retiradas:** H1 (stride 12 bytes), H6 (cálculo de w, `0x00432DD1` = 3D)
- **Corrección documental:** actualizadas descripciones en `tools/gexmap/segments.mjs` y `docs/SUBSYSTEM_SEGMENTS.md` para reflejar hallazgos de U-1f.
- **Pruebas:** análisis estático con `llvm-objdump`, `tools/gexmap/pe.mjs` y `analysis/EU`; validación mediante desensamblado completo.
- **Documentación:** `U1F_RESULTADOS.md` (metodología, thunks, fórmulas, listas, ruta 3D, DFX/VFX, resultados negativos), `docs/U1_GEOMETRY_RECOVERY_PLAN.md` actualizado.

## 2026-11-03 - 0.0.0 - R-3: Direct3D 11 renderer con primera salida gráfica visible

- **Tercera y última pieza del renderer real, con primera salida gráfica visible.**
  `gex_d3d11_renderer` (`port/src/render/d3d11_renderer.c` + `gex/render/d3d11_renderer.h`, Windows-only) implementa el renderer real con `Present` call, clear del backbuffer y render target view. Primera presentación gráfica visible con Direct3D 11. Contrato: liberar RTV antes de `resize`; bitblt escala el backbuffer al cliente. Implementa el contrato `gex_renderer` definido en I-5.
- **Resultados:** `test_d3d11_renderer.exe` aporta 28 checks y `test_app.exe` 117 checks de aplicación (145 asociados a R-3), 0 fallos con Clang 22.1.8; batería completa 133 015 checks, 0 fallos en las 12 suites Clang.
- **Alcance demostrado:** primera salida gráfica visible mediante clear + `Present(0, 0)`. No se ha demostrado ausencia general de device loss ni estabilidad de sincronización a largo plazo.
- **Limitación de resize:** la prueba registra como INFO un fallo de `gex_d3d11_swapchain_resize` y continúa con el tamaño original; no garantiza resize exitoso en todos los entornos.
- **Decisión:** se confirma Direct3D 11 como API de renderizado principal para el proyecto.

## 2026-11-03 - 0.0.0 - R-4: Implementación de VSync, Debug Layer, diagnóstico y medición

- **Implementación completa de VSync configurable, Debug Layer, diagnóstico de dispositivo y medición del tiempo entre frames.**
  - VSync configurable: conecta vsync_retraces con SyncInterval de IDXGISwapChain::Present, manteniendo Flags=0.
  - Debug Layer: implementación de enable_debug_layer para solicitar la capa de depuración, gestionando DXGI_ERROR_SDK_COMPONENT_MISSING sin romper la inicialización normal.
  - Diagnóstico de dispositivo: detección de errores de Present relacionados con pérdida del dispositivo y consulta de GetDeviceRemovedReason.
  - Medición: añadida medición del tiempo entre frames utilizando el reloj monotónico existente.
- **Resultados:** 145 checks nuevos, 0 fallos con Clang 22.1.8; batería completa 133 160 checks, 0 fallos.
- **Características:** VSync activado/desactivado, configuración inválida detectada, Debug Layer disponible/no disponible, diagnóstico de errores de presentación.
- **Pruebas:** cubren todas las nuevas características con hardware y WARP cuando corresponda, sin abrir ventanas ni demos interactivas durante las pruebas. 17e0a0ca49dfbdc4fd45b0948129b7baebfd940d

## 2026-11-03 - 0.0.0 - R-2: swapchain DXGI sobre la ventana Win32 (sin presentar)

- **Segunda pieza del renderer real, sin `Present` ni dibujo.**
  `gex_d3d11_swapchain` (`port/src/render/d3d11_swapchain.c` +
  `gex/render/d3d11_swapchain.h`, Windows-only) obtiene la factoría desde el
  dispositivo de R-1 (`IDXGIDevice → IDXGIAdapter → IDXGIFactory`), crea una
  swapchain **bitblt `DXGI_SWAP_EFFECT_DISCARD`** de 2 buffers
  `R8G8B8A8_UNORM` en modo ventana sobre el `HWND` existente de I-4, conserva
  el backbuffer para R-3 y lo vuelve a obtener tras cada `ResizeBuffers`.
  Las interfaces de paso se liberan antes de volver; el módulo conserva solo
  swapchain + backbuffer y **nunca duplica el dispositivo**.
- **Decisiones:** bitblt como base medible (la comparación con flip-model
  queda para R-4, con `Present` real); 2 buffers por los 2 buffers de color de
  `grSstWinOpen` (`0x0046324D`); sin MSAA, sin cambio de modo, sin
  `ALLOW_MODE_SWITCH`; `vsync_retraces = 3` almacenado para el futuro
  `Present`, sin usar todavía.
- **Resize:** `ResizeBuffers(0, w, h, UNKNOWN, 0)` conserva buffers y formato;
  el backbuffer se libera antes y se readquiere después, también si el resize
  falla. `0×0` se rechaza con `BAD_SIZE` **sin llamar a DXGI** (ventana
  minimizada). Probados resize válido, mismo tamaño, repetido y el camino real
  de ventana (`SetWindowPos` → `GetClientRect` → `ResizeBuffers`). Contrato
  para R-3: liberar la futura RTV antes de redimensionar.
- **Hallazgos COM medidos:** el dispositivo pasa de **2 a 3** referencias al
  crear la swapchain y vuelve a **2** al destruirla; la swapchain arranca con
  **2** (una la retiene su propio backbuffer) y el backbuffer con **1**;
  liberar en orden backbuffer → swapchain devuelve **0** en ambos. El
  backbuffer referencia a la swapchain, así que liberar la swapchain primero
  **no** la destruye (`Release` → 1): el orden exigido queda demostrado.
- **Errores medidos y limpios:** `BufferCount = 1000` → DXGI lo rechaza con
  `DXGI_ERROR_INVALID_CALL` (`0x887A0001`) y el struct queda cerrado; `HWND`
  falso o destruido → `BAD_ARGUMENT`; tras destruir la ventana,
  `ResizeBuffers` devuelve `S_OK` (DXGI no valida el `HWND`: el fallo real
  aparecerá en `Present`, R-3) y el cierre sigue limpio.
- **Pruebas: `test_d3d11_swapchain` **572 checks, 0 fallos, 0 saltos** —
  idéntico con Clang 22.1.8 y GCC 16.1.0, sin avisos con
  `-std=c11 -O2 -Wall -Wextra -Wpedantic`. WARP y hardware real (FL 11_1).
  Sonda nueva `swapprobe.exe` (ventana real, WARP y hardware). Enlace
  `-ld3d11 -luuid` (los IID de DXGI viven en `libuuid.a`; no se necesita
  `-ldxgi` porque no se llama a `CreateDXGIFactory`). Batería completa:
  **132 870 checks, 0 fallos**; `m13-core` 120 y `gexmap` 22/22 sin cambios.
- **Errores encontrados y resueltos (todos de la prueba, no del módulo):**
  suponer 1 referencia en la swapchain (son 2 por el backbuffer), re-iniciar
  un dispositivo vivo en el bloque de dos ventanas (`BAD_STATE` correcto de
  R-1) y una aserción invertida al cerrar con ventana muerta; además faltaban
  `win32_window.c`/`key_state.c` y `-luuid` en el enlace de la prueba, y
  `ID3D11Texture2D_GetDesc` devuelve `void` en los headers WIDL.
- **Sin `Present`, sin clear, sin render target, sin geometría, sin texturas**
  (R-3 y siguientes). `gexloop` sigue con el renderer nulo, sin cambios en
  `loop_core.c`, `frame.c`, audio ni en la ventana. Doc:
  `docs/D3D11_SWAPCHAIN.md`.

## 2026-11-02 - 0.0.0 - R-1: dispositivo Direct3D 11 real (hardware y WARP)

- **Primera pieza del renderer real, sin swapchain ni dibujo.**
  `gex_d3d11_device` (`port/src/render/d3d11_device.c` + `gex/render/
  d3d11_device.h`, Windows-only con el patrón de `backend_dsound`) crea el
  dispositivo con `D3D11CreateDevice` en modo **HARDWARE** o **WARP**
  (selección explícita), pide la lista de feature levels 11_1→9_1 y **registra
  el nivel realmente obtenido**, entrega el contexto inmediato y libera todo
  COM en un `shutdown` idempotente que permite re-`init`. Fallo = struct a
  cero; doble `init` sobre un dispositivo vivo se rechaza sin tocarlo.
- **Resultado real en el PC mantenedor (Windows 11):** hardware y WARP
  creados ambos a **feature level 11_1**, flags 0x0, `S_OK`. La decisión
  D3D11 de I-5 deja de estar condicionada y queda **confirmada**.
- **Fallos medidos y limpios:** bits reservados de flags (`0xFFFFFFFF`) →
  `E_INVALIDARG` (0x80070057) con struct totalmente cerrado; capa debug sin
  SDK layers instaladas → `DXGI_ERROR_SDK_COMPONENT_MISSING` (0x887A002D),
  el código exacto que Microsoft documenta — `D3D11SDKLayers.dll` no está
  instalada en esta máquina y **no se descarga nada** para conseguirla.
- **Hallazgo COM:** el contexto inmediato arranca con 1 referencia, pero el
  device lleva referencias internas del runtime (el contexto mantiene vivo a
  su device), así que «device refcount == 1» no es un invariante válido. La
  verificación de fugas es conductual: referencia manual retenida, `shutdown`
  que libera las suyas, el objeto sobrevive solo con la manual y el
  `Release` final devuelve **0** (destrucción completa). Se ejecuta en WARP,
  hardware, ciclo de 3 re-inits, dos dispositivos simultáneos y rama debug.
- **Pruebas: `test_d3d11_device` **314 checks, 0 fallos, 0 saltos** — idéntico
  con Clang 22.1.8 (llvm-mingw) y con GCC de GNU 16.1.0 (MSYS2), sin avisos
  con `-std=c11 -O2 -Wall -Wextra -Wpedantic`. `d3d11.h` verificado idéntico
  por MD5 en ambos toolchains; enlace con `-ld3d11` (sin descargas).
  `build.ps1` integra la prueba en `test` y añade el target `probe` con la
  sonda `d3d11probe.exe`. Batería completa: **132 298 checks, 0 fallos**.
  `m13-core` 120 checks y `gexmap` 22/22 sin cambios (núcleo intacto).
- **Errores encontrados y resueltos:** las macros COM C de los headers WIDL
  requieren `COBJMACROS` antes de incluir `d3d11.h` (la cabecera lo define);
  el invariante inicial de refcount del device era falso y se sustituyó por
  la verificación conductual; `-Wswitch` por los niveles 12.x en el
  `switch` de nombres.
- **Alcance respetado:** sin swapchain, sin ventana gráfica nueva, sin
  geometría, shaders ni texturas; sin dependencias nuevas; `Makefile` POSIX
  sin compilar D3D11 (solo comentario); renderer nulo e I-4 intactos;
  `docs/PROJECT_STATUS.svg` sin tocar. Documentación oficial consultada
  (Microsoft Learn): `D3D11CreateDevice` y guía WARP.
- **Documentación:** `docs/D3D11_DEVICE.md` (nuevo),
  `docs/RENDERER_ARCHITECTURE_RESEARCH.md` (R-1 ejecutado, decisión
  confirmada), `PROJECT_STATE.md`, `ROADMAP.md` (R-1 cerrado), `BUILD.md`,
  `port/README.md`, `port/build.ps1`, `port/Makefile`.

## 2026-11-02 - 0.0.0 - I-5: investigación y diseño del renderer nativo

- **Hito de investigación; nada implementado.** No se dibuja nada, no se
  compila `d3d11`, no se tocan `loop_core.c`/`frame.c`/el planificador de
  audio, no cambian I-4 ni el build, no hay dependencias nuevas. `gexloop`
  sigue con el renderer nulo. `docs/PROJECT_STATUS.svg` sin tocar (mide MAIN).
- **Evidencia consolidada del render original (`docs/RENDERER_ARCHITECTURE_RESEARCH.md`
  §2).** El exe EU importa **exclusivamente Glide** (38 funciones de
  `glide2x.dll`, cero DirectDraw/Direct3D; censadas por familia). Inicialización
  confirmada: `grSstWinOpen` con 512×384 @ 75 Hz y **2 buffers de color, 0
  auxiliares** para juego (`0x0046324D`) y 640×480 @ 60 Hz para cine
  (`0x00537EA6`), tras una sonda `grSstQueryHardware` (`0x00424D82`). Presentación
  confirmada por traza: render **condicional** `0x0043DC3A` (instala callbacks,
  envía 2 listas), drenaje **doble** por frame (`grBufferNumPending` dentro del
  render y en el frame step) y `grBufferSwap(3)` que bloquea ~23 ms de los 40.
  El swap **encola y retorna** (semántica G-01): el freno es el spin-a-cero del
  frame siguiente — el original es *present-bound* por su bucle, no por el swap.
  Geometría: triángulos/líneas en espacio de pantalla con `w` (Glide no tiene
  matrices; el juego transforma en CPU, INFERIDO por ausencia de imports de
  transformación). Texturas: carga `.3df` (`gu3dfLoad`, `0x004329EF`),
  mipmaps y descarga parcial (`0x00461C32`/`0x00473238`), paletas NCC
  (`grTexNCCTable`, `0x00469836`); `FONT.3DF` es `.3df` `rgb332` 128×128.
  Cine vía `grLfbWriteRegion` (`0x00537D70`).
- **Comparativa de alternativas (§5).** Direct3D 11 (sistema, COM ya probado
  en el repo con DirectSound, **cabeceras y librerías verificadas hoy en ambos
  toolchains**, WARP determinista para pruebas sin GPU) vs OpenGL (compat
  profile deprecado, sin rasterizador software de sistema) vs capa Glide
  (nGlide/dgVoodoo: freeware cerrado con descarga; OpenGLide: LGPL-2.0 Beta
  sin actividad desde 2020; solo tendría sentido para reusar el binario, que
  este port excluye por diseño) vs rasterizador software propio C11 (plan B
  determinista). **Recomendación: Direct3D 11**, condicionada a R-1 (crear
  dispositivo): ningún programa D3D11/OpenGL se ha compilado ni ejecutado en
  el proyecto todavía.
- **Arquitectura propuesta (§6, sin código de producción).** Contrato
  `gex_renderer` agnóstico de la API (init/present/shutdown espejo del nulo)
  con vtable interna `begin_frame`/`present`/`on_focus_lost`;
  `gex_null_renderer` **se conserva** como implementación de prueba (los 117
  checks de `test_app` no cambian); backend `backend_d3d11.c` futuro,
  Windows-only y excluido del Makefile POSIX como `backend_dsound.c`; el
  `HWND` de I-4 entra por el config; la cadencia sigue siendo del bucle
  (`MsgWaitForMultipleObjectsEx`), no del Present; el drenaje doble del
  original **no se replica** deliberadamente (artefacto de cola de Voodoo).
- **Plan de validación incremental (§7):** R-1 crear/destruir dispositivo
  (HW+WARP), R-2 swapchain sobre el HWND, R-3 clear de color verificable por
  hash bajo WARP, R-4 presentación estable sin fugas (debug layer), R-5
  medición de `SyncInterval` 0/1/3 frente a los CSV del glide-shim (8 318
  swaps), R-6 errores/device-lost/cierre, R-7 geometría y textura sintética
  `.3df`+NCC, R-8 A/B visual contra el original (PC mantenedor). R-1…R-6 no
  requieren assets del juego; R-8 sí.
- **Incógnitas registradas (§8, sin inventar):** campos de `GrVertex` que
  rellena el juego (U-1), modos de combine/alpha/chromakey/fog (U-2),
  formatos de textura (U-3), estructura de las «2 listas» de `0x0043DC3A`
  (U-4), papel de `grSstControl` (U-5), `GEX.VFX` (U-6), tasa real de
  refresco en runtime (U-7). Decisiones pendientes D-1 (flip vs bitblt),
  D-2 (política de device-lost), D-3 (drenaje, decidido no replicar).
- **Documentación:** `docs/RENDERER_ARCHITECTURE_RESEARCH.md` (nuevo),
  `PROJECT_STATE.md`, `ROADMAP.md` (I-5 cerrado como investigación), `port/
  README.md`. Fuentes externas citadas con fecha de consulta (OpenGLide
  2026-11-02); sezero/glide sigue siendo semántica de referencia sin copiar
  código (GPL de 3dfx incompatible con el repo). REA no operativo; no se usó.

## 2026-11-02 - 0.0.0 - I-4: ventana Win32, entrada de teclado y reloj real

- **Primer proceso nativo ejecutable (`port/src/app/i4_main_win32.c` →
  `port/build/gexloop.exe`).** Abre una ventana Win32 real (clase
  `GexNativeLoopWindow`, 960×540), mantiene un bucle continuo, drena la cola de
  mensajes y conduce `gex_frame_run()` de I-3 con microsegundos de
  `QueryPerformanceCounter`. No hay SDL/GLFW/raylib ni dependencias nuevas:
  solo `user32`, `gdi32` y `winmm` del sistema.
- **`0x0043E719` deja de ser candidato de entrada (CONFIRMADO por la traza).**
  `docs/FRAME_STEP_TRACE.md` lo sitúa dentro del update (sitio `0x0043E2F5`,
  599/599 frames) como máquina de estados de acción sobre `WORD [obj+0x4E3E]`;
  no es entrada ni audio. La ruta de entrada del original es S5
  (`0x0043F80D`, `joyGetPosEx`/`joyGetDevCapsA`, `VkKeyScanA`), **HYPOTHESIS**:
  la fuente exacta de teclado no está confirmada. El port usa
  `WM_KEYDOWN`/`WM_KEYUP` como decisión explícita, no como reproducción.
- **Entrada (`port/src/input/key_state.c`).** Tablas de 256 códigos virtuales:
  nivel (`down`), flanco de bajada (`pressed`), flanco de subida (`released`),
  auto-repeat coalescido, `WM_KILLFOCUS` que libera todas las teclas y Escape
  interpretado después del frame. `WM_SYSKEYDOWN`/`WM_SYSKEYUP` se enrutan igual
  y se dejan pasar a `DefWindowProcW` para no romper Alt+F4.
- **Reloj (`port/src/platform/win32_time.c` + `clock.c`).** Frecuencia y
  contador de QPC; conversión portable ticks → µs con cociente y resto; guarda
  de retroceso (0 y `GEX_FRAME_WARN_CLOCK`, no error). En esta máquina QPC
  devolvió 10 000 000 Hz.
- **Cadencia y CPU.** Paso lógico 60 Hz (16 667 µs, preset desacoplado de I-3);
  presentación objetivo 120 Hz (8 333 µs), independiente del paso. Espera en
  `MsgWaitForMultipleObjectsEx(QS_ALLINPUT, MWMO_INPUTAVAILABLE)`. **Hallazgo
  medido:** sin tocar la resolución del temporizador, Windows (~15,6 ms) capaba
  la presentación a ~65/s; se añadió `timeBeginPeriod(1)` (con
  `timeEndPeriod(1)` al salir) y se midieron 104,9–107,7 presents/s y
  59,5–60,0 pasos/s. La diferencia hasta 120 es el redondeo a milisegundos de
  la espera; se documenta el valor efectivo, no se presenta 120 como medido.
- **Renderer nulo (`port/src/render/null_renderer.c`).** `present` se conecta al
  frame loop, devuelve 0 y contabiliza `presents`/`last_now_us`/`last_steps`,
  pero no dibuja: no hay dispositivo, superficie ni swap. `WM_ERASEBKGND`
  devuelve 1 y `WM_PAINT` solo valida la región. `pre_frame` y `update` son
  stubs que cuentan llamadas; no hay lógica de juego ni audio adjunto.
- **Pruebas nuevas:** `test_input.c` (826 checks), `test_app.c` (117) y
  `test_win32_time.c` (1 009). `build.ps1 all` pasa con 69 305 + 60 052 + 215 +
  410 + 826 + 117 + 1 009 + 50 = **131 984 checks, 0 fallos**. `tools/m13-core`
  sigue en **120 checks, 0 fallos**; `gexmap` 22/22 y `check`/`segments` sin
  problemas. Flags: `-std=c11 -O2 -Wall -Wextra -Wpedantic`, sin avisos.
- **Prueba real de ventana (Windows 11):** ventana localizada con
  `FindWindowEx`; W↓/W↑, A↓ + `WM_KILLFOCUS`, Escape y `WM_CLOSE`. Tres
  ejecuciones con código de salida 0, `exit: clean`, `frame errors=0`,
  `clock warnings=0`, `discards=0`, `aborted=0`, `backwards=0`; 2,20 s/107,7
  presents/s/60,0 pasos/s/1,38 % CPU, 1,56 s/104,9/59,5/1,93 % y 3,65 s/98,6/
  60,0/0,85 %. Los mensajes se entregaron con `PostMessage` (misma cola y
  `WndProc`), **no** con teclado físico.
- **Límites:** no hay renderizado, lógica de juego, cámara, colisiones ni
  assets; `gexloop` no adjunta audio. La ruta de entrada del original sigue sin
  confirmar. Sin `make` en este Windows (el `Makefile` se actualizó pero no se
  ejecutó). El `gcc.exe` del `PATH` es un driver de Clang 22.1.8 y no cuenta
  como segunda validación, pero se usó el GCC de GNU 16.1.0 de MSYS2 (fuera del
  `PATH`): misma batería, 131 984 checks y 0 fallos, sin avisos; `gexloop`
  compilado con GCC también arrancó limpio. Se corrigió el orden de las
  librerías en `build.ps1` (GNU ld no las resolvía antes de las fuentes).
  Integridad de los cuatro archivos personales no verificada; no se tocaron ni
  publicaron.
- **Documentación:** `docs/WINDOW_INPUT_PORT.md` (nuevo), `PROJECT_STATE.md`,
  `ROADMAP.md` (I-4 cerrado), `TESTING.md`, `BUILD.md`, `port/README.md`,
  `port/Makefile` y `port/build.ps1`. `docs/PROJECT_STATUS.svg` **no se toca**:
  mide MAIN, no componentes.

## 2026-11-01 - 0.0.0 - I-3: núcleo temporal y frame loop nativo integrado

- **El acumulador M-13 se mueve al port, sin copia divergente.** `m13_core.c`
  pasa a `port/src/core/loop_core.c` (misma semántica, prefijo `gex_loop_core_`)
  y `tools/m13-core/m13_core.h` queda como shim que traduce `m13_*` a
  `gex_loop_core_*`. `test_m13_core.c` **no se toca**: sus 120 checks ahora
  compilan y prueban la fuente del port. Se añade `gex_loop_core_rebase()` para
  reanclar el reloj tras una pausa.
- **Frame loop nativo (`port/src/core/frame.c`, `gex/core/frame.h`).**
  `gex_frame_run(now_us)` reproduce el orden medido en
  `docs/FRAME_STEP_TRACE.md`: `pre_frame` (`0x0043F80D`) → `update` × N
  (`0x0043E1BA`) → `gex_audio_sched_tick()` (`0x005380B3`) → `present`
  (`0x0043DC3A` + drenaje `0x0054053E` + swap `0x005404F6`). El tick de audio
  va **dentro** de la fase de presentación: una pasada con 3 updates de
  recuperación hace un solo tick, como el original.
- **Tiempo:** preset Original 1:1 (`step_us = 40000`, `original_mode = 1`,
  un update por present, sin acarreo) y preset desacoplado (`step_us = 16667`
  = 60 Hz, la cuantía del sello `16.666` que el update ya usa). No se elige
  40 ms como paso lógico: 40 ms es el periodo de **presentación** (swap
  bloqueante). El valor definitivo de `STEP` sigue pendiente de E-2.
- **Política explícita:** clamp de 250 ms antes del acumulador, catch-up
  máximo de 5 pasos con descarte contabilizado, reloj invertido ignorado y
  presentado una vez (`GEX_FRAME_WARN_CLOCK`), pausa que congela todas las
  fases y reanuda reanclando el reloj, y errores de callback/audio que abortan
  el frame sin ejecutar fases posteriores, vaciando el acumulador para no
  repetir el paso fallido. El frame loop no abre, cierra ni reinicia recursos
  que no posee.
- **Código nuevo:** `port/include/gex/core/loop_core.h`,
  `port/src/core/loop_core.c`, `port/include/gex/core/frame.h`,
  `port/src/core/frame.c`, `port/tests/test_frame.c` (410 checks),
  `port/tools/framedemo.c` (demo con reloj guionizado). `port/build.ps1` y
  `port/Makefile` ganan `test_frame` y `framedemo`; el `Makefile` de
  `tools/m13-core` compila el núcleo del port.
- **Resultados medidos (Windows 11, clang 22.1.8, `-Wall -Wextra -Wpedantic`,
  sin avisos):** `build.ps1 all` → 69 305 + 60 052 + 215 + **410** + 50 checks,
  0 fallos; `tools/m13-core` → 120 checks, 0 fallos; `node --test
  tools/gexmap/test.mjs` 22/22; `cli.mjs check` y `segments` OK. `framedemo`
  original: 4 frames → 4 pasos, 4 presents; desacoplado con `--dt 60000`:
  3 presents → 10 pasos (3, 4, 3), 3 ticks de audio.
- **Qué es y qué no es:** el reloj, el acumulador, el orden de fases, el
  planificador de audio, el catch-up y el ciclo de vida son reales.
  `pre_frame`, `update` y `present` son callbacks de prueba: **no** hay lógica
  del juego, ventana, entrada, renderer ni lectura de reloj real. No se afirma
  que el juego original se ejecute.
- **Regresión de `pre_frame` (2026-11-02):** T6b fija que el error deja
  `core.acc_us = 0` y `core.t_prev_us = now_us`, y que el frame siguiente no
  recupera el tiempo anterior: 15 ms acumulados → fallo en 20 000 µs → 0
  updates a 32 000 µs (12 ms desde el nuevo ancla) → 1 update a 48 667 µs.
  Verificado por mutación: quitar cualquiera de las dos líneas del fix produce
  11 fallos cada una. En I-3 no se usó un GCC de GNU (`gcc.exe` era un driver
  de Clang 22.1.8 con el mismo target; actualización 2026-11-02: el GCC de GNU
  16.1.0 de MSYS2, fuera del `PATH`, validó después la batería de I-4); el
  harness `tools/m13-core` se compiló a mano con los flags de su Makefile: 120
  checks, 0 fallos.
- **Discrepancias F1–F6** documentadas en `docs/FRAME_LOOP_PORT.md` §11: el
  drenaje en bucle y el swap bloqueante no se modelan; `0x0046300C` no se
  modela por falta de semántica; el sello temporal del update no se implementa;
  el modo desacoplado diverge a propósito del original *present-bound*; el
  manejo de errores de audio y la pausa son política del port.
- **Sin cambios** en `docs/PROJECT_STATUS.svg`: mide MAIN, no componentes.
- **Documentación:** `docs/FRAME_LOOP_PORT.md` (nuevo), `docs/FRAME_STEP_TRACE.md`
  (§7), `docs/M13_DECOUPLE_PROPOSAL.md` (§4), `PROJECT_STATE.md`, `ROADMAP.md`
  (I-3 cerrado), `TESTING.md`, `BUILD.md`, `README.md`, `port/README.md` y
  `tools/m13-core/README.md`.

## 2026-11-01 - 0.0.0 - I-2b: audio audible (backend DirectSound) y planificador

- **Backend de dispositivo: DirectSound, no SDL/WASAPI.** El original habla
  DirectSound (`0x00538551` crea dispositivo y primario, `0x005387f1` el
  secundario de `0x8000`, `0x005380B3` decide con `GetCurrentPosition`). Usar la
  misma API conserva 1:1 el cursor, el *wrap* de `DSBPLAY_LOOPING` y la regla de
  no sobrescribir la mitad que suena. No añade dependencias: `dsound.h` viene con
  el toolchain y `dsound.dll` con Windows (`-ldsound`). SDL2/SDL3 **no está
  instalado** en esta máquina (sin `sdl2-config`/`sdl3-config`, sin `SDL.h`, sin
  `pkg-config`), así que adoptarlo habría supuesto vendorizar código de terceros
  y una dependencia obligatoria para un solo *sink*. Se descarta y se documenta.
- **Código nuevo:**
  - `port/include/gex/audio/backend_dsound.h`, `port/src/audio/backend_dsound.c`
    — `gex_audio_dsound`: `DirectSoundCreate` + `SetCooperativeLevel` +
    `CreateSoundBuffer(0x8000)`; `submit` (Lock/Unlock de la mitad, `Restore` tras
    `DSERR_BUFFERLOST`, `Play(looping)` en el primer envío), `stop`, `cursor`
    (`GetCurrentPosition`), `pause`/`resume` y volumen `level*30 - 3000`
    (`0x0053886c`).
  - `port/include/gex/audio/sched.h`, `port/src/audio/sched.c` — planificador:
    `gex_audio_sched_init/due/tick/pause/resume`, el contrato de los llamantes de
    `0x005380B3` (`0x00463205`, `0x0053A5FB`, `0x0053FCB7`) y del prefill
    `0x00538842`. El tick lee el cursor del propio *sink*, la misma fuente que la
    bomba: una sola verdad (fue un fallo real detectado por una prueba roja).
  - `port/include/gex/audio/stream.h` — `gex_audio_backend` gana `pause`/`resume`
    opcionales; `stop` no es lo mismo que `pause`.
  - `port/tests/test_device.c` — 50 checks contra el dispositivo real; se
    auto-omite (`skipped`, salida 0) si no hay tarjeta.
  - `port/tests/test_audio.c` — 5 suites nuevas de planificador; 144 → **215**
    checks.
  - `port/tools/sagplay.c` — `--device`, `--help`, parseo posicional
    `[in.sag] [out.wav]`, uso del planificador y diagnóstico
    (`device:`/`submits:`/`cursor:`/`note:`).
  - `port/build.ps1` / `port/Makefile` — `test_device.exe` con `-ldsound`;
    `sagplay.exe` con `-lm -ldsound`; `sched.c` en las fuentes de audio.
- **Desviaciones D12–D14** documentadas en `docs/AUDIO_MIXER_RE.md` §11:
  `DSSCL_NORMAL` + ventana de escritorio en vez de `DSSCL_PRIORITY` + ventana del
  juego; `DSBCAPS_GETCURRENTPOSITION2` y `DSBCAPS_GLOBALFOCUS` añadidos a los
  `0xE0` originales; `stop` resetea la posición (`SetCurrentPosition(0)`) en vez
  de recrear el buffer secundario.
- **Evidencia en hardware (esta máquina, Windows 11, DirectSound emulado):**
  `CreateSoundBuffer` con los flags de D13 → `DS_OK`; `Lock` por mitades →
  `b1 = 16384`, `b2 = 0`; cursor a ≈44 100 B/s (22 050 Hz × 2 B);
  `Stop` congela el cursor y `Play` reanuda desde ahí (13 296 → 23 000 en
  250 ms); `test_device` 50/50; `sagplay --device` entrega 3 mitades y el *gate*
  difiere 56 veces en una ejecución real.
- **Qué es y qué no es:** el PCM llega a un dispositivo real y su cursor avanza a
  la frecuencia correcta, y la mitad escrita se lee por `Lock` y coincide
  muestra a muestra con `gex_adpcm_decode`. **No** se ha verificado la audición
  humana: el agente no puede oír; `sagplay --device` queda para el mantenedor.
  Sin `.sag` reales: todo con fixtures sintéticos.
- **Sin cambios** en `docs/PROJECT_STATUS.svg`: mide MAIN, no componentes.
- **Documentación:** `docs/AUDIO_MIXER_RE.md` (§10.1, §10.4, §11 D12–D14, §12,
  §13, §14.4 y nuevo **§16**), `docs/AUDIO_INIT_PATHS.md` (§10),
  `PROJECT_STATE.md`, `ROADMAP.md` (I-2b cerrado, punto de integración I-3/I-4),
  `TESTING.md`, `BUILD.md`, `README.md` y `port/README.md`.

## 2026-10-31 - 0.0.0 - Ruta de audio nativa: IMA ADPCM + bomba de streaming

- **Corrección de nomenclatura (CONFIRMADO):** `0x005380B3` **no es un
  mezclador**. Es una bomba de *streaming* de un único buffer; `0x00538285`,
  el bucle que quedaba sin leer, es un **decodificador IMA/DVI ADPCM de 4
  bits** (2 muestras por byte, nibble alto primero, estado continuo). No hay
  mezcla multi-voz en esta ruta.
- **Funciones reconstruidas instrucción a instrucción:**
  - `0x00538285` (388 B): decodificador ADPCM; orden exacto (paso del índice
    anterior, ajuste antes del delta, clamp `[-32768, 32767]`, índice `[0, 88]`).
  - `0x005380B3` (466 B): `Lock(0x4000)` → *gate* del cursor de reproducción →
    `read(0x1000)` → `decode` → `memset` de la cola corta → `Unlock`; media
    mitad de silencio y `Stop()` posterior.
  - `0x00538731` (379 B): carga del `.sag`, `CreateSoundBuffer(0x8000, 0xE0)`,
    reseteo de estado, prefill, `Play(looping)` y `SetVolume`.
  - `0x00538409`, `0x005384A3`, `0x005384ED`, `0x00538040/68/75/A6` y el setter
    de volumen `0x00538724`.
- **Tablas ADPCM recuperadas:** `adjust[16]` (`0x00558138`) y `step[89]`
  (`0x00558178`) son las tablas canónicas de IMA ADPCM, leídas de la única
  build presente (mod, `.data` a `+0x170`, verificado en tres anclas y en tres
  copias de la tabla). **HIPÓTESIS** para la imagen de referencia: falta leer su
  `.data`. El decodificador no las lleva incrustadas: se inyectan.
- **Código nuevo:**
  - `port/include/gex/audio/adpcm.h`, `port/src/audio/adpcm.c` — decodificador.
  - `port/include/gex/audio/stream.h`, `port/src/audio/stream.c` — buffer
    `0x8000`/`0x4000`, bomba, reproductor y *backend* inyectable
    (`submit`/`stop`/`cursor`).
  - `port/tests/test_adpcm.c` — vector dorado `07 F0` → `{0, 11, −19, −15}`,
    clamp, continuidad, fuente corta, tablas alternativas, 20 000 flujos
    aleatorios; **60 052 checks, 0 fallos**.
  - `port/tests/test_audio.c` — ciclo del buffer, formatos, payload corto,
    múltiplo exacto, truncado, equivalencia con decodificación de una pasada,
    *gate* del cursor, cola silenciosa, errores; **144 checks, 0 fallos**.
  - `port/tools/sagplay.c` — demo: `SLAG` sintético (tono 440 Hz) → PCM →
    `build/tone.wav`.
  - `port/build.ps1` / `port/Makefile`: targets `audio` y `test` ampliado.
- **Bug latente del original documentado y no replicado:** con un payload
  múltiplo exacto de `0x1000`, `read` devuelve `-1`, el original decodifica
  `0xFFFFFFFE` muestras y hace `memset(ptr1 - 4, 0, 0x4004)`. El port lo trata
  como fin de fuente (D8).
- **Desviaciones D7–D11** documentadas en `docs/AUDIO_MIXER_RE.md` §11.
- **Negativos / límites:** no hay `.sag` reales; no hay backend de dispositivo
  (la salida se verifica escribiendo un WAV y capturando muestras); no hay
  bucle de bombeo conectado al frame; la mezcla multi-voz, si existe, no está
  identificada.
- **Sin cambios** en `docs/PROJECT_STATUS.svg`: mide MAIN, no componentes.
- **Documentación:** nuevo `docs/AUDIO_MIXER_RE.md`; actualizados
  `docs/AUDIO_INIT_PATHS.md` (§4.2, §4.5, §6, §10), `docs/SAG_CONTAINER.md`
  (§1, §3, §9, §11), `PROJECT_STATE.md`, `ROADMAP.md` (I-2/I-2b), `TESTING.md`,
  `BUILD.md`, `README.md` y `port/README.md`.

## 2026-10-31 - 0.0.0 - Primer componente del port nativo: lector `SLAG`

- **Cambio de fase:** se abre la vía de implementación nativa en `port/`
  (C11, sin dependencias). No promociona Hito P ni MAIN: el panel sigue
  midiendo la ruta de mods/parches, y **no hay build jugable**.
- **Componente implementado:** lector del contenedor de audio `SLAG`.
  Transcripción instrucción a instrucción del parser `0x0053899C` y del
  cargador de fichero completo `0x005388AC`, más los auxiliares que usan
  (`0x00538B01` `read4`, `0x00538409` `read`, `0x00538A99` `tell`,
  `0x00538ACA` `seek`).
  - `port/include/gex/asset/sag.h` — API pura: `gex_sag_parse`,
    `gex_sag_load`, `gex_sag_release`, `gex_sag_status_name`.
  - `port/src/asset/sag.c` — parser sobre buffer en memoria (sin I/O, sin
    DirectSound, sin memoria global) + cargador que reproduce `fopen`/`fseek`/
    `ftell`/`malloc`/`fread`/`fclose`.
  - `port/tools/sagdump.c` — CLI que imprime lo que el lector entendió.
  - `port/tests/test_sag.c` — 29 casos + 2 fuzz; **69 305 checks, 0 fallos**.
  - `port/tests/make_demo_sag.mjs` — fixture sintético de 54 bytes.
  - `port/build.ps1` (Windows, no hay `make` aquí) y `port/Makefile` (POSIX).
- **Formato documentado:** `SLAG` + dword != `0xFFFFFFFF` + `WAVE`; chunks
  `tag`+`size`; `fmt ` → `memset(dest,0,18)` + `min(size,18)` + `seek(pos+size)`;
  `data` → devuelve el **tamaño declarado** con el cursor al payload. Nuevo
  `docs/SAG_CONTAINER.md` con mapa de funciones, algoritmo con direcciones,
  desviaciones y límites.
- **Desviaciones deliberadas (documentadas, §6):** separación
  `TRUNCATED`/`SENTINEL` (el `read4` original confunde lectura corta con el
  dword `0xFFFFFFFF`), comprobaciones de límites con `size_t`, flag
  `data_complete` para payload truncado, `NO_DATA` en lugar de `-1` para
  `fmt ` de tamaño 0 al final, y aritmética de 64 bits.
- **Negativos / límites:** no hay ningún fichero `.sag` real en el repositorio
  ni en disco (el árbol de la build mod solo contiene `GEX3D.EXE`), así que la
  validación es contra fixtures sintéticos; no hay imagen de referencia para
  comparar dinámicamente contra el parser original; no se reproduce audio.
- **Sin cambios** en `docs/PROJECT_STATUS.svg`: mide MAIN, no componentes.
- **Documentación sincronizada:** `PROJECT_STATE.md` (vía de implementación),
  `ROADMAP.md` (I-1…I-4), `BUILD.md`, `TESTING.md`, `README.md`,
  `REVERSE_ENGINEERING.md`, `docs/AUDIO_INIT_PATHS.md` §10, `.gitignore`
  (`*.sag`).

## 2026-10-31 - 0.0.0 - Rutas de inicialización de DirectSound (EU, estático)

- **Pregunta cerrada (en estático):** ¿es alcanzable el segundo
  `DirectSoundCreate` de `0x00538CB0`? **No por llamada directa.** Su único
  llamante es `0x00538FD5` (35 B, `basis=prologue`), que tiene **0 aristas
  entrantes** en `analysis/EU/calls.tsv`. No se afirma que sea código muerto:
  un `call` indirecto o una tabla de punteros serían invisibles al grafo.
- **Ruta viva:** `0x00538551` (467 B) es el **único** creador del dispositivo
  y tiene exactamente **dos** llamantes, ambos conectados a la entrada:
  - **A (arranque):** `0x00541B20` -> `0x00463278` (WinMain, sitio
    `0x00463A02`) -> `0x0053D822` -> `0x0053B10B` (`0x0053D82F`) ->
    `0x00538551` (`0x0053B120`).
  - **B (reproducción por nombre):** WinMain -> `0x00538017`
    (`0x004637F0`/`0x00463808`) -> `0x00537ED1` -> `0x0053A5A2` ->
    `0x00538731` (`0x0053A5D1`) -> `0x00538551` (`0x00538739`).
  - **C (voz, parcial):** `0x0053FF0D` -> `0x00538731` (`0x0054002A`) ->
    `0x00538551`.
- **Qué construye cada una (CONFIRMED por instrucción):** `0x00538551` crea el
  dispositivo, fija `SetCooperativeLevel(+0x18, hwnd, 3)`, crea el buffer
  primario (`DSBCAPS_PRIMARYBUFFER`, `dwBufferBytes=0`) y le aplica
  `SetFormat(+0x38, 0x005582E0)`; `0x00538731` crea el **único** buffer
  secundario (`dwFlags=0xE0`, `0x8000` B) desde un fichero cargado entero y su
  cabecera `SLAG`/`WAVE`/`fmt `/`data`, y lo arranca con
  `Play(+0x30, 0, 0, 1)`; `0x0053B10B` envuelve la creación una sola vez
  (`[0x5AC5C8]`) y deja `[0x5AC5F0]=1`.
- **Diferencias con la ruta sin llamantes:** `0x00538CB0` no crea buffer
  primario, no aplica `SetFormat`, **no comprueba** el `HRESULT` de
  `SetCooperativeLevel` y le pasa **un solo argumento de pila** (falta
  `dwLevel`). Viva y muerta solo comparten el thunk `0x005404F0` y la
  estructura `0x0057C330`.
- **Sin evidencia dinámica, y por qué:** la imagen de referencia
  (1 557 504 B, MD5 `692b12825003417cc4a5a9d4db13eebe`) ya no está en la
  máquina; la única build presente es un mod (1 559 040 B, MD5
  `3198350eb398db9a64771842d7781b4b`, entrada `0x00541FA0`) cuyo patrón de
  bytes no contiene `0x00538551`. Una traza suya no sería evidencia sobre el
  binario de referencia. Las tres rutas quedan como **existencia estática +
  ejecución no observada**.
- **Nuevo `docs/AUDIO_INIT_PATHS.md`** - método, política de evidencia, las
  cuatro cadenas con sitios, lectura de `0x00538551`/`0x00538731`/`0x0053B10B`/
  `0x00538CB0`/`0x005380B3`, comparación de las dos creaciones, límites,
  reproducción y siguiente paso.
- **Nuevo `tools/gexmap/audio.mjs`** - 14 globales, 19 fichas de semántica
  mecánica, 20 llamadas por vtable, 3 rutas vivas + 1 sin llamantes y
  `validateAudio()`. La ruta C se marca `partial` y el validador **exige**
  que no empiece en la entrada; si alguien la «arregla» sin evidencia, falla.
- **Pruebas:** `node --test tools/gexmap/test.mjs` -> **22/22** (6 nuevas de
  audio); `segments` -> 0 problemas; `check` -> ok.
- **Documentación sincronizada:** `docs/SUBSYSTEM_SEGMENTS.md` (S3, §5, §6.3,
  §8), `docs/FRAME_STEP_TRACE.md` (§11, cierre del objetivo anterior),
  `PROJECT_STATE.md`, `ROADMAP.md`, `README.md`, `tools/README.md`,
  `tools/gexmap/README.md`, `TESTING.md`. `docs/PROJECT_STATUS.svg` no se toca
  (mide MAIN, no RE).

## 2026-10-30 - 0.0.0 - Traza dinámica del frame step (EU, medida en ejecución)

- **Medido en ejecución, no inferido:** se instrumentó `GEX3D.EXE` EU (MD5
  `692b12825003417cc4a5a9d4db13eebe`) con un depurador propio y **breakpoints de
  hardware** (`Dr0`–`Dr3`, sin parchear un byte) en 9 sesiones con el update
  `0x0043E1BA` como **ancla de frame**, más 2 sesiones largas previas. Total
  observado: 2 326 frames.
- **Recorrido del frame confirmado:** `0x0043EA36` → entrada `0x0043F80D` →
  update `0x0043E1BA` (con render `0x0043DC3A` condicional a
  `WORD [obj+0x4E38]==0`) → drenaje de cola `0x0054053E` (bucle) → `swap(3)`
  `0x005404F6` → hoja `0x0046300C`. Orden idéntico en 1 723/1 724 frames.
  Periodo **p50 = 40 ms (25 FPS)**; swap ≈23 ms, trabajo de juego ≈14 ms.
- **Los 25 sitios del update, medidos:** 24 destinos distintos, **23 ejecutan
  exactamente 1 vez/frame**; `0x0045E5C5` es el único con dos sitios
  (`0x0043E3E6`, `0x0043E4F0`). **Negativo:** el *forwarder* `0x0047344B`
  (sitio `0x0043E31C`) dio **0/606**; `0x0043E63D`, `0x00423AFE` y `0x00423A6A`
  dieron **0/611**.
- **Semántica mecánica nueva (CONFIRMED, con la instrucción que la sostiene):**
  `0x0053F610` (acumulador 24.8, +1,0/frame con arg `0x100`), `0x0053F631`
  (registros de 0x2C B, índice de frame por módulo), `0x0045E5C5` (dos pasadas
  sobre la lista), `0x00460BF7` (banderas + copia de 3 WORD), `0x0041F714`
  (recorrido por pares de cadenas), `0x00436F6F` (tres recorridos de listas),
  `0x0046FACE` (pool de 0x20 B, 8 WORD a 0x1000, distancia
  `(30*max+12*min)>>5`), `0x00421E5A` (selección de bloque + despacho por
  `WORD +0x4E3E`), `0x0046E643`, `0x00540650`, `0x00540950`, `0x00465530`,
  `0x00421668`, `0x0043E719`, `0x0043E63D`, `0x0047344B`, `0x00473471`.
  Total **19 de 24 destinos con semántica (17 CONFIRMED + 2 HYPOTHESIS)**; los 5
  restantes se declaran sin asignar.
- **Hechos del update:** `memset(obj+0x78,0,0x28)` al inicio; al final
  `+0x10C = (GetTickCount()−[0x57B5FC])/16,666` (doble verificado en
  `0x0054C060`), `+0x104++` y `+0xA0 &= 0xF7FFFFFF`.
- **Nuevo `docs/FRAME_STEP_TRACE.md`** — método, entorno, tabla de los 25 sitios,
  semántica, negativos, límites y reproducción.
- **Nuevo `tools/gexmap/frame.mjs`** — datos declarativos (frame step, 25 sitios,
  render, bucle de cine, 19 semánticas) con `validateFrame()`; **0 problemas**.
- **Nuevo `tools/frame-trace/`** — `tracer.cs` (depurador con breakpoints de
  hardware, compila y corre), `summarize.mjs` (estadística por frame),
  `README.md` y `.gitignore`. Los binarios y los logs no se publican.
- **Pruebas:** `node --test tools/gexmap/test.mjs` → **16/16** (4 nuevas de
  frame: validación estructural, 25 sitios/24 destinos, tasas plausibles y
  evidencia de cada afirmación); `segments` → 0 problemas; `check` → ok.
- **Panel/SVG sin cambios:** `docs/PROJECT_STATUS.svg` mide MAIN (versión
  jugable), no hitos de RE; v3.6 intacto.

## 2026-10-30 — 0.0.0 — Primera segmentación por subsistemas (EU, HYPOTHESIS)

- **Nuevo `tools/gexmap/segments.mjs` + comando `segments`** — agrupa **65
  inicios** (2,58 % de los 2 523 de juego) en **siete subsistemas HYPOTHESIS**:
  paso de frame y tiempo (8), render Glide (10), audio/mezcla/voz (9),
  cinemáticas (12), entrada (9), shell Win32 (11) y registro (6). Cada miembro
  declara dirección, tamaño, base, llamadores, evidencia y confianza; el comando
  **recalcula** la evidencia (`I`/`T`/`A`/`G`/`O`) desde `analysis/EU/` y falla
  si una afirmación no se sostiene. Las capas manuales `S` (string) y `R`
  (desensamblado crudo) se registran con su detalle.
- **Nuevo `docs/SUBSYSTEM_SEGMENTS.md`** — método, reglas de confianza, tablas
  por segmento, recorridos crudos del frame/cine/audio/entrada, resultados
  negativos y siguientes pasos.
- **Medido (CONFIRMADO contra los artefactos):** 35 ALTA / 22 MEDIA / 8 BAJA;
  20 de las 24 anclas caen dentro de un miembro; 20 aristas directas entre
  grupos; 840 filas del mapa alcanzables desde `WinMain` solo por llamadas
  directas. El frame step real es `0x0043EA36` (entrada → update → espera de
  cola → swap(3) → `0x0046300C` → `ret`); el ancla `0x0043EA51` solo describía
  la espera y el swap.
- **Resultados negativos publicados:** de los 24 callees directos del update
  `0x0043E1BA`, solo 2 alcanzan una API a profundidad ≤3 (`0x0043DC3A` Glide,
  `0x0043E719` WinMM) y **18 no alcanzan ninguna ni a profundidad 12**; el
  segundo `DirectSoundCreate` (`0x00538CB0`) solo se alcanza vía `0x00538FD5`
  (prólogo, 0 llamadores); `0x0053FCB7` no alcanza APIs; el string
  `SOFTWARE\Crystal Dynamics\Gex2\1.00` tiene una única referencia en `.rdata`,
  no en `.text`; `W=no` no prueba código muerto (callbacks/indirectas).
- **Pruebas:** `segments` → **0 problemas**; `node --test tools/gexmap/test.mjs`
  → **12/12** (4 sintéticos + 5 invariantes del mapa + 3 de segmentación,
  incluido el rechazo de una confianza inflada y de una evidencia inventada);
  `check` → ok (2 655 inicios, 5 097 aristas, 24 anclas). Los artefactos de
  `analysis/EU/` **no se modifican** y **ByteMatch sigue en 0,00 %**.
- **Panel/SVG sin cambios:** no hay un hito nuevo de MAIN verificado; v3.6
  intacto. Ficheros modificados esta ronda: `tools/gexmap/{segments.mjs,cli.mjs,test.mjs,README.md}`,
  `docs/SUBSYSTEM_SEGMENTS.md`, `docs/FUNCTION_MAP.md`, `REVERSE_ENGINEERING.md`,
  `TESTING.md`, `PROJECT_STATE.md`, `ROADMAP.md`, `README.md`, `tools/README.md`,
  `CHANGELOG.md`.

## 2026-10-25 — 0.0.0 — Mapa estático de funciones + grafo de llamadas (EU)

- **Nuevo `tools/gexmap/`** — `map` (requiere el `GEX3D.EXE` EU original),
  `check` (valida sin binario, apto para CI), `query` (consulta una función) y
  `test.mjs` (tests offline). Lector PE32 propio, desensamblado con
  `llvm-objdump`, reparación de las 24 regiones donde el barrido lineal se
  desincroniza, grafo de llamadas directas e imports por función. README
  propio.
- **Nuevo `docs/FUNCTION_MAP.md`** — método, cifras, reconciliación con
  ByteMatch, ejemplo trabajado del update `0x43E1BA` y límites.
- **Artefactos derivados en `analysis/EU/`** — `functions.tsv` (2 655 filas,
  sha256 `530a648e…f675d`), `calls.tsv` (5 097 aristas, sha256
  `66def313…467e2`), `anchors.json` (24 anclas, sha256 `f750a720…9c389`) y
  `summary.json` (hashes, recuentos, reparaciones, límites). El binario no se
  copia al repo.
- **Medido (CONFIRMADO):** 1 448 inicios por `call` directa + 1 por entrada PE
  + 1 por primer byte de `.text` + 92 stubs = **1 542 inicios confirmados**;
  **132** stubs `jmp [IAT]`; **5 097** aristas (5 095 `call` + 2 `tail`); 0
  llamadas directas sin resolver; 24 anclas documentales (13 en inicio exacto,
  11 dentro, 0 fuera). Los **1 113** candidatos por prólogo son **HYPOTHESIS** y
  se publican aparte.
- **Corrección documental (`docs/BYTEMATCH.md`):** la afirmación «cada entrada
  listada es llamada de verdad, el método no puede inventar una» era falsa.
  De las 1 647 entradas del informe, **199 no son inicios confirmados** (95 son
  límite de instrucción pero no inicio; **104 ni siquiera son límite de
  instrucción**) y **94 inicios confirmados no están en el informe**. También se
  corrigen los «122 funciones que llaman a una API, todas thunks» (medido: 218
  funciones referencian imports — 86 `func` + 132 `thunk`) y los «40 thunks»
  del informe (el binario tiene 132 stubs; 40 son destinos de `call` directa).
- **Defecto corregido antes de publicar:** los stubs `jmp dword ptr [IAT]` se
  absorbían en la función anterior y le atribuían sus imports. Caso medido:
  `0x0054038E` (función real de 54 B, `ret` en `0x005403C3`) aparecía con 354 B
  de extensión y **50 imports**; ahora tiene **54 B, 0 imports** y los stubs son
  filas `kind=thunk` desde `0x005403C4`. La comparación se midió ejecutando el
  mapa con y sin la pasada de stubs.
- **Validación cruzada:** la entrada PE `0x00541B20` llama a `0x00463278`, la
  dirección que la documentación identifica como `WinMain`; la arista sale del
  desensamblado, no de la documentación. Además, las 40 funciones de la unidad
  `game/imports` del informe coinciden una a una con los 40 stubs del mapa
  marcados `kind=thunk` con `basis=call`.
- **Pruebas:** `check` → ok (2 655 inicios, 5 097 aristas, 1 354 462 B de
  `.text`, 24 anclas, hashes coincidentes); `node --test` → **9/9** (4 casos
  sintéticos + 5 invariantes); dos ejecuciones completas → **hashes idénticos**
  de los cuatro artefactos.
- **Límites / resultados negativos:** los 1 113 prólogos no se promueven; los
  tamaños son cotas superiores; el 100 % de cobertura es del union de
  extensiones; no hay evidencia dinámica; no se usó Ghidra ni REA; el mapa no
  sustituye al splitter ni al `symbols.txt` y **el porcentaje de ByteMatch sigue
  en 0,00 %**.
- **Panel/SVG sin cambios:** no hay un hito nuevo de MAIN verificado; v3.6
  intacto. Ficheros modificados esta ronda: `tools/gexmap/*`,
  `analysis/EU/*`, `docs/FUNCTION_MAP.md`, `docs/BYTEMATCH.md`,
  `REVERSE_ENGINEERING.md`, `TESTING.md`, `PROJECT_STATE.md`, `CHANGELOG.md`,
  `ROADMAP.md`, `tools/README.md`, `.github/workflows/byte-match.yml`.

## 2026-10-25 — 0.0.0 — S-27 verificada: referencia externa `Hydroloxed/gexdecomp` (Gex 1996 para Windows)

- **Nueva referencia secundaria:** `RESEARCH.md` S-27 — decompilación
  *matching* de **Gex (1996) para Windows** (`Hydroloxed/gexdecomp`, `main` @
  `8ba80780`, 2026-09-26). Se mantiene separada de Gex64Decomp (S-11,
  N64/MIPS): otro juego, otro motor y otra plataforma de análisis. Sin
  licencia declarada: **no se copia código**.
- **Verificado en origen (2026-10-25):** README (Ghidra → `unassemblize` →
  decomp.me, MSVC 4.0), estado temprano (1178 funciones marcadas, 32 con
  scratch, 28 al 100 %, 1136 `UNIMPLEMENTED`) y las cuatro funciones de
  temporización de `src/game/winmain.c` (`TIME_GetTicks` `0x00402fa0`,
  `RunTimer` `0x00404ab0`, `GameThread` `0x004050e0`, `WaitForNextFrame`
  `0x00405120`) **sin implementar**: no demuestran comportamiento. Los
  porcentajes son declarados por la fuente, no re-verificados aquí.
- **Límites:** direcciones y firmas de Gex 1996, **no trasladables** a
  `GEX3D.EXE` (Enter the Gecko 1998), que sigue siendo la fuente de verdad.
  Sin binarios, sin ejecución y sin cambios de código.
- **Desambiguación `S-26`:** nota en `RESEARCH.md` §0 — dos secciones
  históricas comparten identificador (A: E-1; B: Línea A); no se renombran
  para no romper las citas existentes y el siguiente ID libre queda fijado en
  `S-28`.
- **Panel/SVG sin cambios:** una referencia documental no es un hito
  verificado del panel (regla §6); v3.6 intacto. Ficheros modificados esta
  ronda: `RESEARCH.md`, `CHANGELOG.md`, `PROJECT_STATE.md`.

## 2026-11-05 — 0.0.0 — U-1f: relación índices-recursos y corrección de U-1a…U-1e

- **Análisis de U-1f:** completado el estudio estático de la relación entre los
  argumentos que construyen vértices y los recursos del juego. Resultado
  principal: **no hay relación entre las rutas 2D y la geometría 3D**; cada una
  tiene su propio modelo de datos. Documento:
  [`U1F_RESULTADOS.md`](U1F_RESULTADOS.md).
- **`0x00432BBD` = glifo de texto 2D (no geometría 3D):** el primer argumento es
  un **índice de glifo**; se descompone en `col = glyph & 0xF` y
  `fila = glyph >> 4` y se convierte en coordenadas de textura sobre una rejilla
  de 16×16 celdas de 16×16 texeles con borde de 1 texel. Quad de 8×8 px,
  cuatro vértices con **stride 0x3C**, `1.0f` en `+0x20` (campo no verificado
  contra los headers de Glide), u/v en `+0x24`/`+0x28`, recurso único
  `grTexSource(0, [0x0057B5EC], 3, &0x0065BFA0)` (`FONT.3DF`).
- **`0x00432DD1` = rectángulo 2D sólido:** `x = arg1 & 0xFF`,
  `y = (arg1 >> 8) + 3`, `w = (arg2 & 0xFF) << 4`, `h = (arg2 >> 8) * 12`;
  el tercer argumento se ignora y **no se llama a `grTexSource`**. Único
  llamador `0x00434F18`.
- **Lista de visualización 2D:** base `0x0065BFC0`, registros de 6 B
  `{word, word, byte estilo, byte glifo}`, contador `0x0065CBE0` (máx. `0x1FF`),
  emisor `0x00432852`. Consumidores: `0x00434F18` y `0x00434FE9`; un rectángulo
  (`estilo == 0x40`) consume **dos ranuras**.
- **Carga de `FONT.3DF`:** `0x004329EF` (`strcpy`/`strcat` de `"\font.3df"` +
  `gu3dfGetInfo`/`gu3dfLoad` + `grTexTextureMemRequired` +
  `grTexDownloadMipMap`), único llamador `0x00463705`.
- **Geometría 3D real en `0x0046558A` (17 068 B):** lee un **flujo de
  comandos** (opcode en `byte[+0xB]`, extensión `0xE1`), un campo de **14 bits
  (`& 0x3FFF`)** indexa la **tabla de texturas `0x006488F4`**, y los vértices
  van *inline* en el flujo (no hay vertex/index buffer). Usa 9 `grDrawTriangle`,
  6 `grDrawLine`, 4 `grTexDownloadTable`, 2 `grChromakeyMode` y 1
  `grDrawPlanarPolygonVertexList` (`0x466B34`, 4 vértices).
- **Recursos de nivel:** `0x00461C32` construye `0x006488F4` desde el **`.VFX`**
  (`grTexDownloadMipMap`); `0x00461E0B` carga el **`.DFX`** con el lector
  `0x00461B90` (`fopen "rb"`) y lo relocaliza con `0x004735C0`; orquestador
  `0x0043B11D(dfx, vfx, nivel)`.
- **Correcciones retiradas de U-1a…U-1e:** «stride de 12 bytes» (el
  `imul …,0xC` de `0x00432E1B` es altura en píxeles), «cálculo de w» (se escribe
  una constante `1.0f` en `+0x20`; el `+8` es tamaño de quad), «`0x00432DD1` = ruta 3D» y «ambas generan
  geometría del mundo». Hipótesis H1 y H6 **retiradas**. Se anotan con nota de
  corrección en cabecera en `U1E_RESULTADOS.md` (historial conservado) y se
  reescribe `docs/U1_GEOMETRY_RECOVERY_PLAN.md`.
- **Resultados negativos:** ninguna referencia a `.DFX`/`.VFX`, `0x6488F4` ni
  buffers de vértices en las rutas 2D; ningún camino de datos 2D → 3D; los
  conjuntos de llamadores son disjuntos.
- **Alcance y límites:** análisis **estático read-only** con `llvm-objdump` y
  `tools/gexmap/pe.mjs`; REA no viable (`rea doctor` → `healthy: false`, sin
  Hopper/Ghidra). Sin ejecución, sin ventanas, sin instalación, sin modificar
  `GEX3D.EXE` ni `C:\GEX_REZ`. Quedan abiertos: `0x004338D7`,
  `[0x0057B600]`, layout del vértice de rectángulo 2D, layout de
  `grDrawPlanarPolygonVertexList`, significado de `registro[+0x0C]` y formato
  de cabecera `.DFX`/`.VFX`.
- **Panel/SVG sin cambios:** investigación estática en `dev/gex`, sin evolución
  de MAIN ni hito verificado; `PROJECT_STATUS.svg` (v3.6) intacto. Ficheros
  modificados esta ronda: `U1F_RESULTADOS.md` (nuevo), `U1E_RESULTADOS.md`,
  `docs/U1_GEOMETRY_RECOVERY_PLAN.md`, `docs/SUBSYSTEM_SEGMENTS.md`,
  `docs/RENDERER_ARCHITECTURE_RESEARCH.md`, `ROADMAP.md`, `PROJECT_STATE.md`,
  `CHANGELOG.md`. No se avanza a U-1g.

## 2026-11-04 — 0.0.0 — U-1e: relación geometría-índices

- **Análisis de U-1e (relación geometría-índices):** Se ha completado el análisis de la relación entre vértices e índices.
  - **Relación vértices-índices:** Los vértices se generan directamente a partir de los argumentos, sin secuencia de índices intermediaria.
  - **Tipo de datos de los argumentos:** Hay evidencia mixta que apunta a diferentes interpretaciones - la multiplicación por stride sugiere índices u offsets, mientras que el procesamiento a flotante sugiere coordenadas.
  - **Relación datos de entrada-vértices finales:** Los vértices finales se generan directamente a partir de los argumentos, sin evidencia de dependencia de datos externos durante la construcción.
  - **Comparación de rutas 2D y 3D:** Las rutas comparten la misma estructura básica, con diferencias en el procesamiento de los argumentos.
  - **Hipótesis confirmadas:** H1 y H6 han sido verificadas mediante evidencia directa del código.
  - **Cuestiones pendientes:** Queda por resolver U-1f.
- **Documentación actualizada:** Se ha creado `docs/U1_GEOMETRY_RECOVERY_PLAN.md` y se ha actualizado `ROADMAP.md`.
- **Sin cambios de código.** No se toca el renderer R-5, no se ejecuta el juego, no se instala nada y no se hace commit ni push. `PROJECT_STATUS.svg` no cambia (MAIN sin evolución).

## 2026-11-04 — 0.0.0 — R-4: estabilidad y diagnóstico Direct3D 11/DXGI

- **Implementación de R-4 (estabilidad y diagnóstico Direct3D 11/DXGI):** Funcionalidades específicas implementadas y probadas.
  - **Debug Layer:** IMPLEMENTADO Y PROBADO con fallback real cuando el SDK no está disponible.
  - **VSync:** IMPLEMENTADO Y PROBADO con conexión entre vsync_retraces y SyncInterval.
  - **Medición temporal:** IMPLEMENTADO Y PROBADO con cálculo de frame_time_us usando reloj monotónico.
  - **Tearing:** NO IMPLEMENTADO, solo investigado teóricamente.
  - **Diagnóstico de dispositivo:** PARCIALMENTE IMPLEMENTADO (solo detección, no recuperación).
  - **Pruebas:** Tests unitarios específicos para cada funcionalidad implementada.
  - **Documentación:** Implementación detallada en `port/src/render/d3d11_device.c` y `port/src/render/d3d11_renderer.c`.

## 2026-11-04 — 0.0.0 — R-5: renderer básico D3D11

- **Implementación de R-5 (renderer básico D3D11):** Pipeline de renderizado básico implementado.
  - **Estado:** Implementación completada del renderer básico (R-3), 104 checks, 0 fallos.
  - **Componentes implementados:** Present, clear del backbuffer, render target view.
  - **Pruebas:** Verificación en modo WARP (104 checks, 0 fallos). Las pruebas que crean HWND solo se compilaron, no se ejecutaron.
  - **Cobertura:** Renderer básico independiente de geometría sintética.
  - **Nota:** Geometría sintética específica con shaders y buffer de vértices aún no implementada.

## 2026-10-21 — 0.0.0 — ByteMatch: informe base verificado + corrección de la configuración inventada

- **Nuevo:** `tools/bytematch-report.mjs` — genera `report/EU/report.json` (esquema de
  informe objdiff v2) leyendo el `GEX3D.EXE` original y verificando tamaño, MD5 y SHA-256.
  Registra lo demostrable: `total_code` = **1 354 413 B** (suma de las 1 647 funciones
  identificadas), `total_data` = **196 096 B** de `.rdata`+`.data`+`.idata` en el fichero,
  `total_functions` = **1 647** puntos de entrada con llamada directa (`E8 rel32`) dentro de
  `.text` (cota inferior medida; el .EXE está stripped y no hay PDB) y 0 bytes emparejados.
  `--check` valida el fichero comprometido sin el binario (lo que corre el CI).
- **Nuevo:** el informe se divide en **2 unidades** medidas sobre el binario:
  `game/code` (1 607 funciones, 1 354 117 B) y `game/imports` (40 *thunks* `jmp [IAT]`
  nombrados con su importación real: `_grBufferSwap@4`, `_gu3dfLoad@8`, `RtlUnwind`…).
- **Nuevo:** `docs/BYTEMATCH.md` — cómo lee ByteMatch, cómo se construye su mapa de
  unidades, por qué el informe marca 0,00 % (verdadero), el método de recuento de funciones,
  por qué el binario **no** se puede trocear en subsistemas automáticamente (grafo de
  llamadas en un solo bloque, sin separadores de `.obj` recuperables) y qué falta para un
  informe real de objdiff.
- **Nuevo:** badge de progreso de ByteMatch en `README.md`.
- **Corregido:** `.github/workflows/byte-match.yml` — se elimina la descarga del binario
  `objdiff` y la invocación inválida `report generate --config --units --version`
  (objdiff 3.8.3 usa `[-p] [-o] [-f] [-c]`); ese era el motivo de las ~20 ejecuciones
  fallidas. Ahora valida el informe y lo sube como artefacto `EU_report`.
- **Eliminado:** `objdiff.yml`, `config/target.toml`, `config/match-units.toml` y
  `config/README.md` — esquema TOML inexistente en objdiff, punto de entrada erróneo
  (`0x00463278` frente al real `0x00541B20`), sección `.reloc` inexistente, reubicaciones
  y unidad `0x0043E1BA` nunca verificadas, y `pip install objdiff-cli` (no publicado).
- **Corregido:** `TESTING.md` §E-2.3 y `CHANGELOG` — los CSV del A/B **no** están en
  Drive `REZengineered/analysis/`; solo en el PC del mantenedor, con su sha-256. La copia
  a Drive sigue pendiente.
- **Corregido:** contabilidad de `total_code`/`total_data` — se usaba el tamaño **virtual**
  de las secciones. En `.data` eso son 1 120 528 B, de los que solo **189 952 B** están en el
  fichero: los ~930 KB restantes son memoria sin inicializar (buffer de runtime) que nunca
  existió en disco. Ahora se cuentan los bytes presentes en el fichero (`raw`), con lo que
  el reparto del .EXE queda en 87 % código / 12,6 % datos / 0,4 % resto.
- **Medido (2026-10-21):** estructura interna del binario, registrada en `docs/BYTEMATCH.md`:
  132 importaciones de 7 DLL; 1 643 de 1 647 funciones conectadas entre sí (un solo bloque);
  los 122 únicos puntos de contacto con el sistema son *thunks* de importación (el juego no
  tiene capa de envoltura propia); los huecos de relleno entre funciones (8–15 B) no
  permiten recuperar los `.obj` originales. Por tanto el troceado en subsistemas es trabajo
  de RE, no una medida, y el mapa de unidades real llegará con las unidades reimplementadas.
- Verificado con el parser real de ByteMatch (`src/report/parse.ts`): el informe parsea,
  se codifica a protobuf y vuelve idéntico. `matched_functions_percent` se fuerza a 0 para
  no heredar la convención de objdiff «total 0 = 100 %».
- Sin ejecutar Gex; sin modificar binarios; `docs/PROJECT_STATUS.svg` intacto.

## 2026-10-22 — 0.0.0 — VC6 toolchain: packaging script + CI workflow + docs

- **Nuevo:** `tools/package-msvc6.ps1` — empaqueta VC++ 6.0 SP6 + Platform SDK Feb 1998
  como `msvc6-toolchain.zip` (binarios `cl.exe`/`link.exe`/`lib.exe`, headers, libs)
  listo para subir como release asset `msvc6-toolchain`.
- **Nuevo:** `.github/workflows/msvc6-build.yml` — CI que descarga el asset, setea
  `PATH`/`INCLUDE`/`LIB`, compila test `.obj` con flags matching (`/Od /Ob0 /Oi- /Oy- /Gd /MT /Gy /Z7`)
  y prueba objdiff smoke test en Linux.
- **Nuevo:** `docs/MSVC6_TOOLCHAIN.md` — guía completa: requisitos, ejecución del script,
  publicación en `gh release create msvc6-toolchain`, uso en CI, y checklist para el
  matching real (splitter → symbols.txt → C → objdiff → bytematch).
- Este toolchain es el **bloqueante raíz** para objdiff matching: el binario original usa
  linker 5.00 (MSVC 5.0/6.0); sin toolchain idéntico, objdiff no puede producir `.obj`
  comparables byte a byte.
- Eliminado workflow diagnóstico `modern-msvc-test.yml` (cumplió su propósito).

## 2026-10-21 — 0.0.0 — Análisis motor timing/game loop con Ghidra (REA+WSL) + WSL/Linux flow

- **Nuevo:** `docs/TIMING_GAME_LOOP_ANALYSIS.md` — consolidación de todos los
  hallazgos RE sobre el motor de timing/game loop (exe EU original):
  flujo temporal reconstruido, tabla de direcciones clave (`0x43B36D`,
  `0x43E5DD`, `0x43E1BA`, `0x43EA51`, `0x43F80D`, `0x43DC3A`, `0x46324D`),
  análisis completo de `0x43E1BA` (update por frame: 25 callees, 1 caller),
  sello temporal `elapsed/16.666` verificado, PDB `gex3d.pdb`, anomalía
  34 ms vs 42–44 FPS (ABIERTA), hipótesis abiertas y gaps pendientes.
  **Primer análisis profundo con Ghidra sobre binario Gex.**
- **Actualizado:** `docs/REA_WINDOWS_ENABLEMENT.md` §7 — flujo WSL/Linux/macOS
  con Ghidra (recomendado); `tools/setup-re-env.ps1` hash SHA-256 real;
  `tools/README.md` nota sobre restricción x86 en Windows.
- Sin ejecutar Gex; sin modificar binarios; `docs/PROJECT_STATUS.svg` intacto.

## 2026-10-21 — 0.0.0 — E-2.3 A/B test ejecutado + Análisis motor timing/game loop con Ghidra (REA+WSL) + WSL/Linux flow

- **E-2.3 A/B test EJECUTADO (2026-10-21, PC mantenedor):** Original EU vs F-03 EU con glide-shim (QPC 10MHz). nGlide 2.10, VSync OFF. **Resultados:** Original EU = 21.2 FPS promedio (p50=40ms, alta varianza); F-03 EU = 43.2 FPS (p50=13ms, frame pacing consistente). **Conclusión:** F-03 ~2× más rápido, frame pacing 2x más consistente. Original EU ~21 FPS real (no 25), F-03 ~43 FPS (confirmado 42–44). Anomalía 34ms vs 42-44 FPS: límite 34ms en F-03 no es cuello de botella. Artefactos: `glide-shim_original.csv` (sha256 `f15b9b14…4a3f8`), `glide-shim_F03.csv` (sha256 `45f2e612…87845`) — **solo en el PC del mantenedor**; la copia en Drive `REZengineered/analysis/` sigue pendiente (no verificada).
- **Nuevo:** `docs/TIMING_GAME_LOOP_ANALYSIS.md` — consolidación completa hallazgos RE timing/game loop (exe EU): flujo temporal, direcciones clave, análisis `0x43E1BA` (25 callees, 1 caller), sello temporal, PDB `gex3d.pdb`, anomalía 34ms, hipótesis, gaps.
- **Actualizado:** `docs/REA_WINDOWS_ENABLEMENT.md` §7 — flujo WSL/Linux/macOS con Ghidra (recomendado); `tools/setup-re-env.ps1` hash SHA-256 real; `tools/README.md` nota restricción x86.
- Sin ejecutar Gex (salvo test A/B); sin modificar binarios; `docs/PROJECT_STATUS.svg` intacto.

## 2026-10-21 — 0.0.0 — WSL/Linux/macOS con Ghidra (alternativa recomendada) + hash real — nueva §7 con flujo completo
  para WSL/Linux/macOS con Ghidra 12.1.4 (activación WSL, JDK 21, descarga +
  verificación SHA-256 `ddac49f…`, instalación REA, variables de sesión,
  verificación y trabajo acotado). Esta es la **vía recomendada** para analizar
  `GEX3D.EXE` (PE32 x86), ya que las releases publicadas de REA en Windows
  solo admiten x86-64.
- **Actualizado:** `tools/setup-re-env.ps1` — hash SHA-256 real de Ghidra 12.1.4
  completado (antes era placeholder). `tools/README.md` actualizado con nota
  sobre restricción x86 en Windows.
- Sin ejecutar ni instalar nada; sin modificar `main` ni `PROJECT_STATUS.svg`.

## 2026-10-21 — 0.0.0 — Comparativa F-02 (2013 NTSC-D3D) vs original EU (documental, sin ejecutar)

- **Nuevo:** `docs/F02_VS_ORIGINAL.md` — consolidación de toda la evidencia
  documental sobre F-02 (exe 2013 NTSC-D3D, S-14): identidad, relación con
  otros fixes, inventario S-14, diferencias conocidas con EU (renderer, región,
  CD-check, música), aplicabilidad a EU (hipótesis: no aplicable), gaps
  pendientes. Sin ejecutar nada ni modificar binarios.
- Referencias cruzadas añadidas en `PATCH_ANALYSIS.md` (F-02), `KNOWN_ISSUES.md`
  (I-01, I-03, I-11) y `REVERSE_ENGINEERING.md` (§3).

## 2026-10-21 — 0.0.0 — Comparativa F-03 vs original EU (documental, sin ejecutar)

- **Nuevo:** `docs/F03_VS_ORIGINAL.md` — consolidación de toda la evidencia
  estática y observacional sobre F-03 (build 1998-06-29, no parche quirúrgico)
  vs el original EU: identidad de binarios, diff estático (82.9% bytes ≠),
  comportamiento observado (42–44 FPS vs ~25), análisis de timing (direcciones
  documentadas), mecanismo inferido, anomalía 34 ms vs 42–44 FPS (ABIERTA),
  hipótesis abiertas y gaps pendientes. Sin ejecutar nada ni modificar binarios.
- Referencias cruzadas añadidas en `KNOWN_ISSUES.md` (I-01), `PATCH_ANALYSIS.md`
  (F-03) y `REVERSE_ENGINEERING.md` (§3).

## 2026-10-16 — 0.0.0 — REA: diagnóstico de habilitación en Windows (documentado, sin ejecutar)

- **Nuevo:** `docs/REA_WINDOWS_ENABLEMENT.md` — REA como capa de
  orquestación/evidencia que necesita proveedor externo (Hopper/Ghidra/IDA);
  adoptada en el proyecto pero **sin demostración sobre ningún binario de
  GEX**; estado verificado del host Windows; `tools/setup-re-env.sh` pendiente
  y Linux-only; binarios GEX identificados (`GEX3D.EXE` = PE32 x86); límites
  reales del carril Windows (Ghidra **P0 experimental, solo lectura**, NTFS
  local) y aviso de versión (**x86 32-bit** solo en `main`; las releases
  publicadas, incl. 6.7.0, admiten solo x86-64):
  oportunidad de usar Ghidra+REA para el xrefs/call-graph del path temporal
  documentado (`0x43E1BA`/`0x43E5DD`/`0x43EA51`); y plan de habilitación
  (dependencias, descargas, integridad, ubicaciones, reversión) **pendiente de
  autorización**.
- **Revisión posterior (2026-10-16, auditoría del contenido publicado):**
  corregidas referencias cruzadas frágiles (se eliminaron números de línea del
  `CHANGELOG.md` que nuestra propia inserción desplazaba) y retirada la
  dirección `0x43EB76` (no documentada en el repo; provenía del análisis F-03 de
  la sesión) en favor de las direcciones ya documentadas del exe EU; añadidos
  los límites reales del proveedor Windows P0 y el aviso de versión x86.
- **Sin cambios de fase ni de ejecutable:** no se descargó, instaló, ejecutó ni
  modificó nada del juego; `docs/PROJECT_STATUS.svg` intacto (sin avance real).
- Actualizados por coherencia: `docs/TOOLKIT.md` §3/§5, `RESEARCH.md` §S-18,
  `PROJECT_STATE.md`, `REVERSE_ENGINEERING.md` §4.

## 2026-10-11 — 0.0.0 — Investigación documental: referencia externa `soul-re` (motor «Gex 2» en Soul Reaver PS1)

- **Nuevo informe:** `docs/research/SOUL_RE_ENGINE_REFERENCE.md` — análisis
  estático de `fmil95/soul-re` (MIT; commit
  `59d1ee87749da95b6ced413a9f60b30a6fb91d45`, verificado como el que mide el
  85,05 % publicado en decomp.dev; ejecutable principal 90,04 %; Code
  751,84 kB / Data 156,62 kB; 232 funciones aún en `INCLUDE_ASM`, mayoría en
  overlays).
- **Hallazgos clave (como REFERENCIA, no como hecho sobre Gex PC):** modelo de
  tiempo del motor (`timeMult` en punto fijo 4.12 con 4096 = frame de 33 ms,
  `frameRateLock` 30/20 fps, modo 24 fps, acumulador de frames lógicos) sobre
  `GAMELOOP_DoTimeProcess`; integración física `PhysicsMove` escalada por
  `timeMult`; orden de actualización por frame documentado en `GAMELOOP_Process`;
  modelo de objetos `Instance` con mensajería/consultas y desactivación por
  distancia en 1/8 por frame; streaming (cola de 40 entradas + `MEMPACK` con
  compactación) y overlays con tabla de reubicación + tabla de funciones
  versionada (`cinemax`); cámara por modos con amortiguación crítica; animación
  G2 con controladores procedurales y descompresión ADPCM de canales.
- **Límites explícitos:** sin ejecutar Soul Reaver ni Gex; sin modificar
  ejecutable/DLL/shim/Registro/instalación; ninguna afirmación se ha verificado
  contra `GEX3D.EXE` (la etiqueta «Gex 2 engine + game code» es del repositorio
  externo); hipótesis sobre los 25 FPS de Gex PC marcadas como tales, con método
  de verificación pendiente.
- **Panel/SVG sin cambios:** investigación documental no es un hito verificado
  del panel (regla §6); porcentajes intactos. Ficheros modificados esta ronda:
  el informe nuevo + `CHANGELOG.md` + `PROJECT_STATE.md`.

## 2026-10-11 — 0.0.0 — Smoke test in-game H4a OK (arranque real a menú, PC); panel v3.6 (solo estado verificado)

- **Smoke in-game H4a (PC mantenedor, Windows 11, reportado
  2026-10-11): OK.** Copia aislada `C:\GEX_REZ\SMOKE_H4A` con el
  ejecutable EU original (SHA-256 `d0932549…` = fila `GEX3D.EXE` de
  `docs/ORIGINAL_ARTIFACT_INVENTORY.md`, md5 `692b1282…`), shim H4a
  como `glide2x.dll` (SHA-256 `d0ea3151…`, el binario del run V-1
  H4a) y `glide2x_gex_real.dll` = DLL real V-0 (SHA-256
  `7cbd0958…299aab`); CD `D:` montado. El juego llegó al menú
  principal; proceso con código de salida 0 (cierre del usuario con
  Alt+F4). `InstallDir` (vista de 32 bits) apuntado temporalmente a
  la copia y RESTAURADO/verificado después como `C:\GEX_REZ\GEX2`.
- Primer intento: falló por CD no montado (entorno; NO fallo del
  shim). Log local del 2º intento
  (`preflight\smoke-h4a-launch-2.txt`) NO archivado en Drive: subir
  manualmente a `REZengineered/Reports/H4a/` si se quiere conservar
  allí. El modo del smoke (NOLOG/instrumentado) no consta.
- Alcance: ARRANQUE real — distinto de las estáticas V-0/V-1/V-1b
  (tablas de exports) y del arnés V-2 (carga en proceso contra una
  fake staged). NO demostrado aún: hooks durante partida, estabilidad
  prolongada, renderizado, compatibilidad plena ni tanda instrumentada
  (footer `# end` + `overflow=0`). Cautela permanente: no repetir
  cambios de Registro sin backup de la vista de 32 bits
  (`reg export … /reg:32`) y reversión documentada y verificada.
- Panel **v3.6**: SOLO dato de estado verificado → micro-línea
  «Shim OK V-0→V-2+P-W5 + smoke a menú» + fecha/ref (`3d342e8`).
  **Cifras sin cambios** (ningún hito/área cambia). Mismo diseño.
- Sync: TESTING (nueva sección Smoke in-game H4a), README shim
  (estado in-game + cautela Registro), PROJECT_STATE (blurb +
  Testing), PROJECT_STATUS/SVG (v3.6). Sin cambios de código ni
  binarios; el smoke se ejecutó en el PC del mantenedor, no desde
  Arena.

## 2026-10-11 — 0.0.0 — H4a: `exports_shim.txt` archivado en `Reports/H4a/` — archivo de evidencias V-1 completo

- El mantenedor subió el `exports_shim.txt` del run H4a a
  `REZengineered/Reports/H4a/` (creado 2026-10-11T02:40:13Z;
  mtime de origen `2026-10-10T23:26:16.281Z`, el segundo del run
  H4a): 36485 B, MD5 `0e3e7b06724be1add54e9f8b94d88c54`,
  SHA-256 recomputado en el sandbox
  `9b9170a6eb2817ea353c754c32ce0f15374b174f43dfd698406209ab746ca228`.
- V-1b intacto: `Reports/V1b/exports_shim.txt` sigue con 36483 B
  y SHA-256 `9a6f2bfe…53b6071` (idéntico al verificado la ronda
  anterior; MD5 de Drive == local en ambos).
- Los dos dumps difieren exactamente en 7 campos (14 líneas), y
  la tabla de 38 exports es idéntica: grafía inicial
  (`./glide2x.dll:` H4a vs `glide2x.dll:` V-1b), `Time/Date`
  (`Sun Oct 11 01:26:15` vs `Sat Oct 10 23:39:13` local),
  `ImageBase` (`6c040000` vs `6ca40000`; auto-image-base de ld =
  hash del dllname, única diferencia del comando: `./`),
  `CheckSum`, `Time/Date stamp` (`6acac997` = 23:26:15Z vs
  `6acab081` = 21:39:13Z) y
  las 2 direcciones derivadas (import/export). Confirma la
  identificación previa (predicción `./glide2x.dll:` + 01:26
  local) y descarta cualquier mezcla entre runs.
- Verificación cruzada de tiempos: stamp H4a 23:26:15Z = 1 s
  antes de `v1-h4a.log` (mtime 23:26:16.375Z) y del propio dump
  (23:26:16.281Z); stamp V-1b 21:39:13Z = 1 s antes de
  `v1b.log` (21:39:14Z).
- Sync: TESTING (V-1 H4a: archivo completo), README shim
  (reconciliación V-1b), PROJECT_STATE (pendiente → completo).
  PROJECT_STATUS sin cambios (no citaba el dump). **SVG/panel
  intactos** (documental). Sin cambios de código; sin mover ni
  alterar nada en Drive.

## 2026-10-11 — 0.0.0 — Evidencias V-1 H4a/V-1b/Matrix2/V-2 organizadas en `Reports/` (Drive): subcarpetas + hashes recomputados

- Organización (solo mover; sin duplicar ni borrar) en
  `REZengineered/Reports/`: `H4a/` (`v1-h4a.log`, `v2-h4a.log`),
  `V1b/` (`v1b.log`, `exports_real.txt`, `exports_shim.txt`),
  `Matrix2/` (`matrix2.log`), `V2/` (`v2_cleanupfix.log`); los 5
  artefactos P-W5 siguen en la raíz de `Reports/` (md5 intactos).
- Hashes recomputados 2026-10-11 en el sandbox sobre copias
  descargadas (MD5 de Drive == MD5 local en los 7 ⇒ descarga
  íntegra). SHA-256: `v1-h4a.log` `2fe53c6c…`, `v2-h4a.log`
  `da4319be…`, `v1b.log` `da8e375d…`, `exports_real.txt`
  `e44d0c76…`, `exports_shim.txt` `9a6f2bfe…`, `matrix2.log`
  `cc26ff90…`, y `v2_cleanupfix.log`
  `3ff4672a5602337fa02f03696df91469b31595dd923726e2e9aaafc4c3d824ea`
  (== SHA-256 documentado de V-2 ⇒ recomputado y coincidente).
- El único `exports_shim.txt` subido (había dos runs distintos,
  NUNCA mezclados) queda identificado como el de **V-1b** por dos
  señales independientes: su PE `Time/Date` `Sat Oct 10
  23:39:13 2026` (objdump imprime esa marca en hora local;
  calibrado con un PE sintético: 21:39:13Z ⇒ `23:39:13` en
  Madrid; `v1b.log` se escribió a 21:39:14Z) y la grafía de ruta
  sin `./` (V-1b) frente a `./` (`build-win32.sh`/H4a). El
  volcado de H4a NO está en Drive ⇒ pendiente de re-subida (se
  reconocería por `./glide2x.dll:` inicial y `Time/Date`
  ~`Sun Oct 11 01:26` local).
- Verificación cruzada: `p-w5-forwarder-probe.log` (no movido)
  recomputado
  `c630759a0782f5e174675202b65c0df42c66c63ee641eb7750139edec43e8b47`
  == SHA aportado por el mantenedor; `v1-h4a.log` lleva el
  SHA-256 del shim H4a `d0ea3151…`; `v1b.log` lleva el de V-1b
  `1f234c0a…ccb98217` (== TESTING).
- `matrix2.log` archivado cierra el pendiente H4b: build rc=1
  (ld no exporta los nombres decorados) ⇒ H4a es la forma buena.
- Sync: TESTING (V-1 H4a; MATRIZ-2/H4b), README shim (F-3;
  reconciliación V-1b), PROJECT_STATE (blurb + pendientes),
  PROJECT_STATUS (corrección puntual fila v3.5: SHA recomputado).
  **SVG/panel intactos** (organizar evidencias no es hito). Sin
  cambios de código: `gshim.c`/`.def`/arnés congelados.

## 2026-10-11 — 0.0.0 — P-W5: cierre documental — log SHA-256 + evidencias archivadas en Drive (`Reports/`)

- Evidencias P-W5 archivadas (subida manual del mantenedor) en
  `REZengineered/Reports/`; presencia, tamaños y contenido
  cotejados vía API de Drive desde el sandbox: log íntegro
  `p-w5-forwarder-probe.log` (2286 B; SHA-256 *aportado*
  `c630759a0782f5e174675202b65c0df42c66c63ee641eb7750139edec43e8b47`;
  contiene las 38 líneas `PASS:` + `PROBE SUMMARY: hooks=3/3
  forwarders=35/35 failed=0` + `PROBE: ALL PASS` +
  `PROBE_EXIT_CODE=0`), `forwarder_probe.c` (3719 B; mecanismo
  cotejado: solo resolución — ningún export invocado),
  `forwarder_probe.exe` (268240 B), `expected_iat.txt` (1019 B,
  idéntico por contenido al del repo) + informe P-W5.
- Alcance sin cambios: la prueba acredita resolución y propiedad
  de los 38 exports; **no** valida comportamiento en llamada ni
  arranque/funcionamiento de Gex. El SHA-256 del log es el
  aportado por el mantenedor (no recomputado desde el sandbox).
- Panel: sin cambio visible → SVG intacto (v3.5, métricas
  iguales); corrección puntual en la fila v3.5 de PROJECT_STATUS
  §5 («pendientes de archivar» → archivadas, log SHA-256);
  TESTING/README/PROJECT_STATE sincronizados.

## 2026-10-11 — 0.0.0 — P-W5 SUPERADO (resolución dinámica de exports, probe PC); registro en Reports + panel v3.5 (solo dato de estado)

- **P-W5 (resolución de los 38 exports con el cargador de Windows)
  SUPERADO en PC** (reportado 2026-10-11): `forwarder_probe.exe`
  en `/tmp/gex-pw5-e1-probe` → `PROBE SUMMARY: hooks=3/3
  forwarders=35/35 failed=0` + `PROBE: ALL PASS`, exit code 0.
  Los 3 hooks resuelven dentro del shim `glide2x.dll`; los 35
  forwarders dentro de `glide2x_gex_real.dll` (copia real
  renombrada). Sin llamadas a funciones exportadas y sin Gex.
- Alcance: cubre resolución + pertenencia de módulo. NO cubre
  conducta en llamada de los 35 (fuera del shim; la ejerce
  funcionalmente la fase in-game), in-game, rendimiento, ni el
  camino del IAT del exe (AC4 del plan; cobertura extra no
  exigida). Inferencias etiquetadas (no medidas): la copia real
  cargó bajo el nombre renombrado en ese entorno; los 35 no
  re-dirigen a un segundo salto en esa ruta (coincide con V-1b).
- Procedencia: resultado **aportado por el mantenedor**; binario,
  fuente y log del probe NO están en el repo (no auditados por
  Arena) → pendientes de archivar en `REZengineered/Reports/`.
  El commit exacto del árbol del probe no consta (rama en
  `551fae1`). Registro subido a Drive `REZengineered/Reports/`
  (informe con ficha tamaño/SHA-256/MD5; validación = reportado
  en PC; artefactos crudos pendientes).
- Panel **v3.5**: SOLO dato de estado verificado (micro-línea
  «Shim validado (V-0→V-2+P-W5, PC)») + fecha/ref. **Cifras
  intactas** (A1 32, A2 46, A3 0, A4 0, A5 33; global 20%; MAIN
  40%): P-W5 no completa ningún hito del panel.
- Producción intacta (`gshim.c`/`.def`/arnés); sin ejecutar Gex;
  DLL original intacta. Hitos abiertos sin cambios: procedimiento
  in-game, P-W1…P-W4, E-2, y los MAIN/A-x sin evidencia.
- Sync: TESTING (bloque P-W5 + alcance), README shim (estado
  V-0→V-2+P-W5, riesgos re-anclados), PROJECT_STATE (blurb +
  pendientes de archivo), PROJECT_STATUS/SVG (v3.5).

## 2026-10-11 — 0.0.0 — V-2 SUPERADO en PC (arnés 14/14 + SUITE ALL PASS); V-1b reconciliado; dashboard v3.4 sin cambios de cifras

- **V-2 SUPERADO** (PC mantenedor, reportado 2026-10-11, sobre
  `0c22556`): `W32HARNESS: 14 passed, 0 failed` + `W32HARNESS:
  ALL PASS` (W00–W11 reales, sin SKIP de entorno) + `SUITE: ALL
  PASS` + `V2_RC=0`; log `/tmp/v2_cleanupfix.log` SHA-256
  `3ff4672a5602337fa02f03696df91469b31595dd923726e2e9aaafc4c3d824ea`;
  `SKIP behaviour` en MINGW32 = esperado (etapa Linux-host); V-1
  PASS en la misma corrida. Cierra P-W6.
- Alcance: valida la DLL en el arnés (carga real en proceso,
  reenvío, resolución/fail-fast, orden, degradación, overflow,
  doble shutdown, DETACH/prebound) contra fake staged, sin Gex ni
  DLL real. Pendiente: in-game (smoke NOLOG + tanda real) y P-W5
  (35 forwarders vs DLL real). Efecto colateral: W01 PASS en PC ⇒
  `%llu`/`%lld` sin mangle en esa toolchain (F-1, contingencia).
- **V-1b RECONCILIADO: PASS 2026-10-10 se MANTIENE** (criterios
  documentados superados: arquitectura + presencia exacta +
  forwarders; registro recuperado coincide: rc=0, real 38/38).
  Falta de archivo concreta, sin reconstruir nada: `v1b.log`,
  `exports_shim.txt` y `exports_real.txt` nunca estuvieron en git
  (`.gitignore` excluye `*.log`) ni constan archivados → pendientes
  en `REZengineered/Reports/` (Drive), junto al log+SHA de V-1
  H4a y el resto de `matrix2.log`.
- **V-0 sin cambios:** identidad + SHA-256 `7cbd…9aab` (y MD5)
  documentados; DLL original no modificada ni ejecutada.
- **Dashboard v3.4** (PROJECT_STATUS.md + SVG): SOLO dato de
  estado verificado (instrumentación Glide validada V-0→V-2,
  2026-10-11) + fecha/ref `0c22556`; **cifras intactas** (A1 32,
  A2 46, A3 0, A4 0, A5 33; global 20%; MAIN 40%): ningún
  hito/área del panel cambió → sin cambios de pesos ni de estado
  de áreas.
- Producción intacta: `gshim.c`/`gshim.def`/`tests/win32/harness.c`
  y los 14 escenarios sin tocar.
- Sync: TESTING (bloque V-2 SUPERADO + alcance + P-W6 cerrado +
  notas F-1/F-2 con fecha), README shim (cadenas de re-run, estado
  V-0→V-2, riesgos abiertos re-anclados a la tanda in-game, nota
  de nombres in-game vs gate, reconciliación V-1b),
  PROJECT_STATE (V-2 superado + pendientes), MODERNIZATION_GOALS
  (M-13: MSVCRT + V-2), REVERSE_ENGINEERING (primer paso
  reversible: V-2 superado), PROJECT_STATUS/SVG (v3.4).

## 2026-10-11 — 0.0.0 — V-2: limpieza del scratch de fakelogic portable Linux ↔ MSYS2 (EBUSY); fix en árbol

- PC MSYS2 MINGW32 (reportado 2026-10-11, sobre `2a8e6da`):
  harness 14/14 `ALL PASS`; suite roja con `OSError: [Errno 16]
  Device or resource busy: '/tmp/tmpcit66x25'` al salir
  `tempfile.TemporaryDirectory` en FAKELOGIC (tras un hijo
  `_Exit(99)` y el exe recién escrito).
- Causa: `TemporaryDirectory` no reintenta los errores que no son
  `PermissionError` (`tempfile._rmtree` los re-lanza), se borraban
  directorios en mitad del flujo (uno por caso) y había lecturas sin
  `close` explícito; `ignore_errors=True` lo silenciaría (prohibido).
  Reproducido: el mismo EBUSY inyectado en `tempfile._rmtree` mata
  al código viejo con el traceback exacto del PC.
- Fix portable: ciclo de vida explícito (`mkdtemp` + `scratch_rmtree`
  con reintentos acotados), el dir cwd del hijo se vacía entre casos
  en vez de borrarse, `close` explícito, y scratch imborrable →
  FAIL ruidoso con la ruta (nunca ignorado).
- Pines: EBUSY de un disparo inyectado en la limpieza real de
  FAKELOGIC (pre-fix: muere con EBUSY; ahora pasa) y
  SCRATCHSELFTEST (transitorio reintentado / persistente devuelto /
  EACCES real reportado).
- Verificación: `SUITE: ALL PASS` (295 checks; win32logic 26),
  BEHAVIOUR 93/93, sin restos en /tmp. Matriz por plataforma en
  TESTING. Panel intacto (V-2 aún NO superado). Re-run PC pendiente.

## 2026-10-11 — 0.0.0 — V-2: harness PC 14/14; suite bloqueada por fakelogic (vehículo POSIX + `__stdcall` redefinido); fix en árbol

- PC MSYS2 MINGW32 (reportado 2026-10-11, sobre `85d72d2`):
  `W32HARNESS: 14 passed, 0 failed` / `ALL PASS`; `SUITE:
  FAILURES`, único fallo `fakelogic: real fakereal.c compiles`.
- CAUSA 1 (error duro, oculto por el truncado a 300 chars del
  diagnóstico): el vehículo de FAKELOGIC enlazaba
  `tests/behavior/harness_impl.c` (Linux-only: `<dlfcn.h>`,
  `dlsym(RTLD_NEXT)`, `-ldl`) → en MinGW `fatal error: 'dlfcn.h'
  file not found` y `-ldl` inexistente. Reproducido en target
  real `x86-windows-gnu`.
- CAUSA 2 (aviso ruidoso + ABI cdecl silenciosa): el stub
  redefinía `__stdcall`, que los compiladores Win32 predefinen
  (GCC `gcc/config/i386/cygming.h:135`; clang además
  `__declspec`) → `-Wmacro-redefined`.
- Fix: FAKELOGIC compila el `fakereal.c` REAL + mini-impl
  portable generado (`getenv`/`_Exit`, 25 líneas) sin `-ldl`;
  ambos stubs guardan las grafías Win32 con `#ifndef` (gana la
  definición del target); diagnóstico completo volcado al log
  en fallo.
- Pines nuevos (STUBTARGET): grafías guardadas en ambos stubs;
  cero diagnósticos de redefinición (predefines reales en el PC,
  emuladas en Linux); la definición del target sobrevive
  (`__stdcall` → atributo, no vacío). Negativos vs `85d72d2`:
  4 FAIL (`__stdcall` → EMPTY/cdecl).
- Verificación: suite Linux `SUITE: ALL PASS` (291 checks,
  BEHAVIOUR 93/93); vehículo nuevo compila+enlaza limpio en
  `x86-windows-gnu` real con `-Wall -Wextra -Werror`. Panel
  intacto (V-2 aún NO superado). Re-run PC pendiente.

## 2026-10-11 — 0.0.0 — V-2 re-run 11/14: W03 off-by-one + W05/W10 strict-99 (fix en árbol)

- PC MSYS2 MINGW32 (reportado 2026-10-11, sobre `002fd6a`):
  11/14. W03 `want 1 trip marker, got 0`; W05/W10 código 99
  (`finalize-before-forward violated`). Build, 38 exportaciones
  e integridad repo OK.
- W03: aserción off-by-one (38 vs prefijo de 37; único
  `strncmp` erróneo auditado mecánicamente). Fix: longitud
  vía `sizeof`. La shim sí emite el marcador (B2).
- W05/W10: finalize SÍ corre con IO bloqueada (notas loud);
  el STRICT exigía el artefacto imposible → 99. Fix: el fake
  acepta la nota de finalize como prueba de orden (solo
  finalize la escribe, solo antes del forward); el 99 sigue
  vivo sin finalize. Espejo en el fake Linux (B16a/b).
- Regresiones: TRIPSCAN (scan REAL, 1 trip), FAKELOGIC
  (`fakereal.c` REAL, 6 estados incl. 2×99), B16a/b. Cero
  cambios en `gshim.c`/`.def`. Panel intacto (V-2 aún no
  pasa). Re-run PC pendiente.

## 2026-10-11 — 0.0.0 — V-2 re-run 6/14: causa bind perezoso vs check temprano (fix en árbol)

- PC MSYS2 MINGW32 (reportado 2026-10-11, sobre `a827647`):
  W00/W01/W02/W03/W04/W05/W09/W10 FAIL con «fake: shim did
  not bind glide2x_gex_real»; W06×3/W07/W08/W11 PASS. Build,
  38 exportaciones e integridad repo OK (`V2_RC=1`).
- Causa raíz: `gshim.c` enlaza el backend SOLO en la primera
  llamada interceptada (`ensure_init`; DllMain ATTACH-only por
  diseño); `child_check_fake` corría tras `child_load`, antes
  de ninguna llamada → NULL por construcción. W11 pasa por
  preload; W06/W07/W08 no chequean. Staging exonerado.
- Fix: check movido tras `child_actions` (9 sitios, mismas
  aserciones STRICT, ninguna eliminada). Regresiones: B15
  conductual (`gshim.c` real: ATTACH no enlaza, 1.ª llamada
  sí, orden handle→path) + pin de orden en `test_harness`
  (9 checks presentes). `gshim.c`/`.def` intactos. Panel
  intacto (V-2 aún no pasa). Re-run PC pendiente.

## 2026-10-11 — 0.0.0 — V-1 H4a superado PC (38/38); V-2 bloqueada por 2 fallos harness (fix en árbol)

- PC mantenedor (reportado 2026-10-11, GCC i686 16.1.0): V-1 H4a
  PASS — build OK, 38/38 exactas, 0 extras. Artefactos (log
  completo, SHA DLL) pendientes de archivar.
- V-2 NO superada: los 14 escenarios fallan en `child_load`
  («shim module is not the staged copy»: `join_path` doblaba el
  separador tras la `\` final de `GetTempPathA` mientras
  `GetModuleFileNameA` devuelve la grafía normalizada) y
  `run-harness.sh` exigía `cmp` (ausente en ese MSYS2).
- Fix: `canon_path` (canonización absoluta estricta por fichero,
  nunca basename; ambas grafías en el diagnóstico) +
  `join_path` sin doble separador; `files_identical` (sha256
  stdlib, sin binarios) + compare shell para git. Regresiones:
  `test_harness` (join extraído verbatim + gcc) + 4 casos
  herméticos en `test_env`. `gshim.c`/`.def` congelados.
  Panel intacto (sin hito: V-2 aún no pasa).

## 2026-10-11 — 0.0.0 — F-3 RESUELTO por H4a: mismatch-alias + sin dllexport = 38 exactas

- Matriz-2 PC (GCC 16.1.0, sobre `e3bbe01`): H4a (`.def`
  `"_X@N"=X@N` + C sin dllexport) VERIFICA 38/38, 0 extras,
  `CHECK_EXPORTS: PASS`. H1/H2/H3 no superan (detalle pendiente).
- Causa refinada: dllexport-stdcall emite `X@N` + dedup por
  nombre (el viejo dedupaba por símbolo: V-1 2026-10-10). La
  nota «REFUTADA» previa queda corregida: la interacción sí —
  sospecha del mantenedor CONFIRMADA.
- Integrado (mínimo vs H4a, export-lines idénticas): `.def`
  mismatch ×2, C/fakes sin dllexport, variantes por cirugía
  `.def`, test_def/test_api prohíben dllexport + exigen H4a.
- V-1/V-2 NO superados (H4a validó la FORMA en /tmp, falta el
  árbol). Panel intacto (sin hito).

## 2026-10-11 — 0.0.0 — F-3 causa aislada: matriz A/B/D agota `.def`; fix H1 (dllexport autoridad)

- Matriz PC: A/B (alias con `_`) link FAIL, D (sin `_`) 38
  erróneas; ninguna grafía `.def` da 38 exactas. Modelo: lookup
  antepone `_`, verbatim en exportación, twins de mismatch,
  dedup dllexport∪.def. «dllexport causa twins» REFUTADA.
- Fix H1 PROPUESTO (pendiente PC): `.def` 35-forwarders +
  dllexport×3; fakes dllexport + variantes `-DFAKE_NO_*`;
  test_def/test_api invertidos; matriz H1/H2/H3/H4a/H4b decide
  (H4b adopta la propuesta usuario si da 38).
- Docs: TESTING (matriz+causa+H1) + README + PROJECT_STATE.
  V-1/V-2 NO superados; panel intacto (sin hito).

## 2026-10-11 — 0.0.0 — F-3: V-1 reintento BUILD PASS + verificación FAIL (38+3 twins); fix propuesto

- PC 2026-10-11 (MSYS2 MINGW32): alias `"dec"=interna-sin-_`
  enlaza pero MinGW-32 exporta TAMBIÉN los internos
  (`grBufferSwap@4` + 2 twins): `check_exports.py` rc=1. V-1
  2026-10-10 PASS intacto (hecho histórico, otro .def).
- Fix PROPUESTO (pendiente PC): alias exact-match `"dec"="dec"`
  en `gshim.def` + `fakereal_full.def`; objdump auto-detectado
  (triplete→plano→PATH, test_env hermético); regresiones
  (test_def exact-match, test_exports twins 38+3).
- Aislado verificado: harness/build-behavior solo temp; V-1
  directo escribe salidas documentadas gitignored (diseño).
  Docs: TESTING (F-3) + README + PROJECT_STATE. Panel intacto.

## 2026-10-10 — 0.0.0 — F-2 evaluado: benigno y justificado (sin cambios de código)

- Dictamen `-Wcast-function-type` (`gshim.c:312`, cast
  `GetProcAddress` → `void (__stdcall *)(int32_t)` para
  `_grBufferSwap@4`): lint determinista sobre patrón API;
  firma corroborada (SDK `1 4`, `.def`, V-1b 38/38 real).
  Primarias: fuentes repo + V-1b; secundaria: `FARPROC` web.
- Decisión: cast SIN cambios; supresión local solo si `-Werror`
  futuro lo exige. Límite: estático; dinámica en V-2/P-W5.
  F-2 evaluado, NO corregido (sin defecto).
- Sync: TESTING (V-1 + nota Diagnóstico incl. V-1b superada),
  shim-README, PROJECT_STATE. Panel intacto.

## 2026-10-10 — 0.0.0 — V-1b superado en PC (cruce vs DLL real 38/38)

- V-1b EJECUTADO en PC mantenedor (MSYS2 MINGW32, copia temporal
  aislada): PASS, exit 0. `build-win32.sh` con copia real
  verificada (sin instalar, sin Gex): ambas PE32/i386; cruce
  38/38 (0 adicionales), 3 interceptadas + 35 forwarders OK;
  shim temporal SHA-256 `1f234c0a…ccb98217` (NO en repo).
  Limitaciones: cruce estático; no prueba carga ni Gex.
- F-2, arnés W00–W11 y V-2 siguen PENDIENTES (sin evidencia nueva).
- Sync: TESTING, PROJECT_STATE, shim-README, tools-README, M-13,
  REVERSE. Panel intacto (V-1b no mueve indicadores).

## 2026-10-10 — 0.0.0 — Coherencia V-1: actualizar Diagnóstico TESTING (V-1 PASS / V-1b PENDING / F-2)

- TESTING «Diagnóstico suite en PC Windows»: la cláusula
  «V-1/V-1b pendientes de evidencia Win32 real» quedó obsoleta
  tras d8abd27; actualizada a V-1 superada PC (MSYS2 MINGW32,
  GCC 16.1.0, 38/38), V-1b PENDING (falta cruce vs DLL real) y
  F-2 pendiente de evaluación. Sin cambios de estado: nada nuevo
  pasa a superado. Entradas históricas intactas.

## 2026-10-10 — 0.0.0 — V-1 superado en PC (build Win32 + 38/38); V-1b pendiente; hallazgo F-2

- V-1 EJECUTADO en PC mantenedor (MSYS2 MINGW32, copia temporal
  aislada): PASS, exit 0. GCC 16.1.0, target `i686-w64-mingw32`;
  `V-1 BUILD: PASS`; DLL PE32/i386 temporal (SHA-256
  `c62868115001ebfb0d303ad5df6bc49a557e8adf342f0b19df89080c7dea6df4`,
  NO incorporada al repo); exports 38/38 exactas (0 adicionales),
  3 de código = interceptadas, 35 forwarders a `glide2x_gex_real`
  mismo nombre (`check_exports.py` modo shim-only).
- V-1b PENDIENTE (no ejecutado): falta cruce vs copia real
  verificada (argumento a `build-win32.sh`; 38 incl. interceptores;
  procedimiento seguro: copia temporal, sin instalar, sin Gex).
- Hallazgo F-2 (PENDIENTE de evaluación, sin corregir): warning
  de conversión entre tipos de función en `gshim.c:312` (cast de
  `GetProcAddress` a puntero-función `__stdcall`); código intacto.
- Sync: TESTING (V-1 PASS/V-1b PENDING), PROJECT_STATE, shim-README,
  tools-README, M-13, REVERSE. Panel intacto (sin ítems ligados a V-1).

## 2026-10-10 — 0.0.0 — Nota: rutas locales redactadas pre-publicación

- Las rutas absolutas del PC del mantenedor citadas en entradas
  anteriores y en KNOWN_ISSUES.md, TESTING.md y PATCH_ANALYSIS.md se
  sustituyen por `<DIR_PRINCIPAL>` (carpeta principal de instalación;
  ruta exacta omitida). Evidencia intacta: hashes, mensajes de error
  (`Cannot load font …\font3.dff`), valores de registro y
  reproducibilidad 3/3. Motivo: no divulgar la estructura local de la
  máquina. Corrección documentada aquí, no silenciosa.

## 2026-10-10 — 0.0.0 — Auditoría de sincronización y documentación (sin cambios funcionales)

- Alcance: inventario git/worktree + revisión cruzada de 18 documentos +
  re-ejecución de suites. Sin commit/push, sin Gex, sin instalaciones,
  sin tocar producción (`gshim.c`/`.def`/`build-win32.sh` intactos) ni
  reescribir historial previo.
- Re-ejecutado (sandbox Linux, evidencia propia): suite glide-shim verde —
  7 etapas ALL PASS, 199 PASS/0 FAIL, BEHAVIOUR 71/71, SKIP win32
  ruidoso; sumas por fichero 14+3+74+3+3+17+11 exactas; B1a–B14 sin B9
  (script=log=docs); `tools/m13-core` 120 checks/0 fallos (binario
  retirado con `make clean`); `expected_iat.txt` 38 nombres; arnés:
  14 escenarios, timeout 180 s, `%TEMP%\gshim_w32_<pid>`,
  `cmp`+`git status` presentes; árbol sin binarios (2 `.pyc` ignorados);
  enlaces internos OK; hashes V-0 idénticos en TESTING/RESEARCH/shim.
- Correcciones mínimas: README raíz (estructura `tools/`), BUILD.md
  (builds de herramientas), mención arnés W00–W11 preparado/pendiente-PC
  (PROJECT_STATE, tools-README, M-13, REVERSE), PATCH F-03 (BLOQUEADO
  matizado como histórico), shim-README (cabecera multi-etapa + etapa 9),
  comentario cabecera `run_tests.sh`.
- Clasificación reciente verificada, sin promociones: gshim v6 =
  implementado + validado estructural Linux + V-0 ejecutado PC (PASS);
  V-1/V-1b preparados sin ejecutar; arnés W00–W11 = implementado +
  validado estructural (22/22, warning-clean vs stub), NO compilado
  MinGW real, NO ejecutado Windows (P-W1…P-W6, F-1, A-1 abiertos);
  `env.sh`+`test_env` 11/11 verdes en Linux, re-ejecución PC pendiente.
- Panel PROJECT_STATUS.md + SVG v3.3 INTACTOS (cifras idénticas; la
  auditoría no es progreso verificable; A5.5/LICENSE §6 = revisión
  pre-publicación, no esta sincronización). Observaciones sin cambio:
  rutas locales pre-existentes (I-23/E-1.1, evidencia; revisar al
  publicar), `!tools/**` en `.gitignore` (cubierto por procedimiento).

## 2026-10-10 — 0.0.0 — Shim Glide: arnés nativo Win32 W00–W11 (sin Gex, sin DLL real)

- Nuevo `tests/win32/`: `harness.c` (padre+hijo, 14 escenarios en
  procesos/dirs temporales) + `fakereal.c` (doble de la real con
  conteos en vivo) + `.def` + `build-harness.sh`/`run-harness.sh`.
  Carga la DLL de producción por ruta absoluta contra la fake
  staged al lado: loader, GetProcAddress y DllMain reales, CERO
  cambios en `gshim.c` (sin hooks: la costura beside-shim ya
  existía; documentado por qué no hay inyección).
- Cubre init, reenvío×3 + args/retornos, NOLOG, escritura/footer,
  doble shutdown, resolución×3 + load-fail (111), overflow 262144,
  log/marcador bloqueados, prebound y DETACH no-op; orden
  finalize-antes-de-reenviar (STRICT). Aislamiento: solo copias
  temporales, DLL repo comparada tras correr, `git status` intacto.
- Suite: etapa 9 con SKIP ruidoso (construye con i686 en
  cualquier SO; conducta solo en Windows); `test_env` 8→11.
  Validado aquí: suite verde, SKIP, C warning-clean vs stub,
  parsers ensayados 22/22. PENDIENTE PC: compilación MinGW-32
  real + 14 escenarios + P-W1..P-W6 (ver TESTING). Hallazgo F-1
  (`%llu` vs MSVCRT) sin corregir: lo dictamina W01 en el PC.

## 2026-10-10 — 0.0.0 — Shim Glide: test_docs UTF-8 explícito (fallo cp1252 PC Windows)

- `test_docs.py` leía README.md/`gshim.c` con la codificación del
  locale y reventaba en el PC (cp1252 no decodifica el byte 0x81
  UTF-8 del README): lectura explícita `encoding='utf-8'` en ambas
  lecturas. `gshim.c`/`.def` decodifican en cp1252 sin reventar,
  por eso el resto pasaba; no se toca producción ni documentación
  de contenido.
- Regresión hermética `test_docs_encoding` 3/3 (contrato UTF-8
  estricto + precondición del bug + `test_docs` verde con la
  decodificación implícita desactivada; verificado que FALLA con
  el `test_docs.py` previo). Suite 14+3+74+3+3+17+8 y 71 verdes.
- Sync: conteos + nota TESTING. V-1/V-1b pendientes de evidencia
  Win32 real; V-2 no iniciado.

## 2026-10-10 — 0.0.0 — Shim Glide: suite portable (diagnóstico PC Windows) + env.sh

- Fallos en Git Bash del PC: alias de Store ocultaba a python y
  gcc-64 disparaba el `#error` de `gshim.c` (correcto: la guarda
  solo actúa bajo `_WIN32` no-x86). Dictamen: entorno + hueco de
  robustez de la suite, cero regresiones del shim.
- Ajuste: `tests/env.sh` (python `python3`/`python`/`py`
  verificado por ejecución; host gated a Linux + gcc nativo con
  SKIP ruidoso; c-syntax clasifica guard=SKIP, resto=FAIL) +
  `test_env` 8/8 hermético (más extremo a extremo simulando
  Store+py+mingw64: suite verde con SKIPs). `build-win32.sh`
  revisado sin defectos.
- Producción intacta; suites 14+3+74+3+17+8 y 71 verdes. V-1/V-1b
  pendientes de evidencia Win32 real; V-2 no iniciado.
- Sync: README (conteos + nota Windows), TESTING, PROJECT_STATE,
  MODERNIZATION M-13, tools/README. Panel intacto.

## 2026-10-10 — 0.0.0 — Shim Glide: V-0 superado + V-1/V-1b preparados (sin build aún)

- V-0 EJECUTADO en PC mantenedor (salida real): PASS — 1630208 B,
  SHA-256/MD5 exactos, PE32/i386, copia idéntica. Metadatos 3dfx
  2.61.00.0658 = marca esperada del payload nGlide 2.10 (9ª/S-22).
- V-1/V-1b preparados: `build-win32.sh` (toolchain i686 + línea
  fijada + sha256 + listados objdump con guardas + shim-only
  automático) y `check_exports.py` (veredicto automático sobre
  los binarios: real con las 38, modo shim-only, resumen FAIL
  coherente). Revisión D1–D4 aplicada. Auto-probados:
  `test_exports` 17/17; parser validado contra `objdump -p` real.
- Bloqueo honesto: sandbox Linux sin MinGW (`ld` sin PE) ⇒ build
  y veredicto reales pendientes del PC (MSYS2 MINGW32). Gex no
  ejecutado; V-2 no iniciado. Suites 14+3+74+3+17 y 71 verdes.
- Sync: README (V-0 OK + procedimientos), TESTING, PROJECT_STATE,
  MODERNIZATION M-13, REVERSE_ENGINEERING, tools/README. Panel intacto.

## 2026-10-10 — 0.0.0 — Shim Glide v6: setvbuf comprobado (cota 2 KB condicional + evidencia separada)

- Hallazgo: el retorno de `setvbuf` NO se comprobaba — si el CRT
  rechazaba el búfer de 2 KB, la v5 afirmaba igual la cota: falso.
- Solución: `open_log` comprueba `setvbuf(...) == 0`
  (`buf_pinned`); rechazo ⇒ reenvío intacto + datos completos +
  tanda LOUDLY marcada (`buf=default-UNPINNED` + `gshim_error.txt`)
  y descartada por el criterio error-file existente (nunca 111).
  Cabecera gana campo `buf=2048|default-UNPINNED` (crash-safe).
- B14: doble de `setvbuf` en el harness (forzado por env,
  passthrough `RTLD_NEXT` si no): 6 checks (reenvío, error,
  cabecera, 501 filas, sin fail-fast). `buf=2048` afirmado en
  B1a/B6. Suites verdes 14+3+74+3 y 71 conductual.
- Docs honestos: cota 2 KB = propiedad ESPERADA pendiente de
  confirmación en Windows (no promesa portable); evidencia
  separada — (a) OBSERVADO ≤2048 B en Linux/glibc (B13),
  (b) MSVCRT esperado por contrato C89, V-2 lo confirma (footer
  `max` + A/B). Tripwires `test_docs` ampliados (banean
  absolutos portables).
- Sync: README v6 (Impacto condicional + fila de fallo + riesgo
  MSVCRT), MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (hitos, no conteos por auditoría).

## 2026-10-10 — 0.0.0 — Shim Glide v5: auditoría búfer CRT (fin del «cero syscalls»)

- Hallazgo: `fprintf` SÍ dispara escrituras implícitas al llenarse
  el búfer aunque no haya `fflush` (stdio estándar) — la v4 lo
  admitía como «residual» pero titulaba «cero syscalls»: falso y
  retirado de README + `gshim.c` (tripwire `test_docs`).
- Solución mínima: `setvbuf` 2 KB (C89 portable; MSVCRT y glibc
  lo respetan) ⇒ cada implícito ≤2 KB (~1–10 µs) con cadencia
  determinista (~1/100 filas) en ambos CRT; sin hilos ni
  bloqueos. Alternativas descartadas: búfer gigante (mata la
  forense de crash), sin búfer (syscall por fila), mmap/hilos
  (complejidad). Crash acotado a ~200 filas + cola de 64.
- `log_cost_us_max` SÍ captura los implícitos (ocurren dentro de
  la ventana medida) — documentadas 6 limitaciones (escalar, no
  transfiere entre máquinas, confla goteo+flush, sin finalize,
  sin NOLOG, page-cache/AV).
- Pruebas: B13 traza write() a nivel syscall (ptrace propio;
  LD_PRELOAD descartado: no ve escrituras intra-libc): máx
  exacto 2048 B con y sin finalize; 40 implícitos en tanda
  crash sin ningún `fflush` (prueba del punto 2). Suites verdes
  14+3+69+3 y 63 conductual. Sin Windows/Gex (V-x pendientes).
- Sync: README v5 (timing honesto + método), MODERNIZATION M-13,
  PROJECT_STATE, REVERSE_ENGINEERING, tools/README. Panel intacto.

## 2026-10-10 — 0.0.0 — Shim Glide v4: capacidad, I-O en path y marcador NOLOG

- Auditoría final pre-build (sin ejecutar Gex/compilar en Windows
  ni tocar instalaciones; dinámica NO declarada): (1) anillo a
  262144 stop-on-full (nunca reutiliza) + marcador `# overflow`
  + `fflush` una sola vez + nota error; `overflow=1` descarta la
  tanda siempre; análisis de capacidad (típico <5K, patológico
  <150K/60 s; footer `pending_calls` para re-dimensionar).
  (2) I/O en path: fuera el `fflush` cada 4096 (picos ~ms en el
  frame); goteo de 64 filas al búfer CRT sin syscalls por diseño;
  coste residual (~µs, 1/64 filas + flush CRT raros) medido por
  footer `max` + A/B NOLOG; crash acotado (~centenas de filas).
  Muestreo pending 1/1024→1/4096 (cambios siempre exactos).
  (3) Marcador NOLOG: fallos init/end audibles (error + tanda
  inválida para A/B); reenvío intacto.
- Nueva suite CONDUCTUAL (`tests/behavior`, 57 checks verdes):
  el `gshim.c` real compilado contra fakes Win32 (QPC/env/
  resolución programables): exactitud filas/footer/muestreo,
  tope + marcador, NOLOG, marcador roto, 111×4 + sin QPC,
  kill -9, doble shutdown, log bloqueado, DETACH no-op,
  fast-path. Estructurales 14+3+65 (10 pines v4 nuevos).
  Conductual ≠ validación Windows (V-0/V-1/V-1b/V-2 pendientes).
- Sync: README v4 (capacidad + timing + riesgos abiertos),
  MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (auditoría ≠ hito).

## 2026-10-10 — 0.0.0 — Shim Glide v3: revisión final de riesgos pre-build

- Revisión punto por punto (sin ejecutar Gex ni tocar
  instalaciones/registro; validación dinámica NO declarada):
  (1) `DllMain` pasa a solo-ATTACH (DETACH no-op: cero I/O bajo
  loader-lock; riesgo deadlock evaluado); volcado final en hook
  `grGlideShutdown` (hilo del juego, idempotente) + incremental
  cada 4096. (2) `GSHIM_NOLOG=1` aislado: `gshim_log.csv` con un
  único punto de apertura (`open_log`, call-sites guardados);
  señal = `gshim_nolog.marker` (init+end, contadores A/B).
  (3) Política fail-fast documentada: sin QPC o símbolo
  irresoluble → `gshim_error.txt` + exit 111, nunca retornos
  ficticios; log no abrible → reenvío intacto + gap visible.
  (4) Fuera la inferencia por transitividad: hipótesis explícita
  + V-1b (volcado DIRECTO de exports de la DLL real renombrada).
- `.def`: 35 reenvíos + 3 código (Shutdown se une a Swap/Pending).
  Suite verde 14+3+55 + sintaxis; `test_init` cubre las 4
  propiedades (estructural, no conductual). Footer con `by=` +
  fila `X`; fallback `grSstWinClose` pendiente explícito de V-2
  (footer tras salida normal).
- Sync: README v2→v3 (política fallos + V-1b + formato),
  MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (revisión ≠ hito).

## 2026-10-10 — 0.0.0 — Auditoría pre-ejecución del shim Glide (v2 + suite)

- Auditoría estática de `tools/glide-shim` (sin ejecutar Gex ni
  tocar instalaciones): 6 defectos encontrados y corregidos —
  D-01 exports de wrappers dependientes del auto-export (ahora
  `dllexport` + `.def` explícito), D-02 `LoadLibrary`/fopen en
  `DllMain` (ahora `DllMain` mínimo + init perezoso con guarda),
  D-03 `fflush` por swap (ahora anillo RAM 64K + volcado
  incremental), D-04 overflow `tick*1e6` (ahora ticks crudos +
  conversión offline), D-05 sin guarda de arquitectura (ahora
  `#error` si no-i386), D-06 carrera en init (Interlocked).
- Verificación nueva (más allá de IAT vs `.def`): exe PE32/i386
  re-confirmado, 38 imports Glide nombrados (0 ordinales, 0 sin
  decorar); aridades 38/38 contra SDK Glide 2.x
  (`sezero/glide`, `FX_CALL=__stdcall`; rama clásica en los 3
  prototipos duales). Sin MinGW/Wine/DLL real en sandbox
  (verificado ausente): build + dumpbin + dinámica quedan como
  validación V-1/V-2 en PC mantenedor (pins hash DLL Hito 1 en
  README). Compatibilidad plena NO confirmada.
- Nueva suite `./tests/run_tests.sh` (verde: 12+3+21 checks +
  `gcc -fsyntax-only`): `test_def` (38/38), `test_api`
  (4·nparams=@N), `test_init` (estructural, no conductual).
  README v2: construir + validar (V-0/V-1/V-2) + impacto timing
  (footer auto-coste + A/B `GSHIM_NOLOG`) + reversión.
- Sync: MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (auditoría ≠ hito).

## 2026-10-10 — 0.0.0 — Pívot: I-23 identificada, R-audio confirmado, núcleo M-13 verificado, shim Glide (fuente)

- Autorización expresa (pívot 2026-10-10): modificar código,
  investigar dinámicamente (incl. origen I-23) y experimentos
  controlados; originales intactos + hashes + copias + restore
  points. Análisis sobre copia trabajo (md5 `692b1282…`, fuera
  del repo); nada redistribuido.
- I-23 causa IDENTIFICADA (estática): WinMain `0x463281–95` lee
  `InstallDir`/`CDDriveName` vía `0x442F41`
  (`HKLM\…\Gex2\1.00`, REG_SZ); `0x4329EF` compone
  `<InstallDir>\font.3df`, fallo → fatal `0x463011`. E-2 método
  REPARADO: E-2.3 (conmutación InstallDir por copia, `/reg:32`,
  restore point + criterios) LISTO PARA EJECUTAR en PC
  mantenedor; confirmación dinámica pendiente. Micro-item:
  `font.3df` vs `font3.dff` (1 dir listing).
- R-audio CONFIRMADO: fmt `0x55A708` (`<InstallDir>\audio\
  voiceuk\<n>.sag`, `0x53FF0D`) + `VOICEUK/` 400 `.SAG`;
  `0x53FCB7` decode completo (pump + scheduler 375 + array
  voces); tabla `0x5ACBE4` filas por voz (R2 WORD = canal voz,
  hipótesis fuerte) → audio wall-clock obligatorio, valida
  propuesta §4. `0x43F80D`: 3 llamadores efectivos
  (frame/init/cine) + `0x4D5763` muerto. IAT Glide: 38 imports
  con slots (`0x65F45C–0x65F4F0`).
- Nuevo `tools/m13-core` (núcleo §4 C11 + harness): 120 checks,
  0 fallos (gcc Linux 2026-10-10); NO integrado, M-13 PROPOSED.
- Nuevo `tools/glide-shim` (proxy instrumentación Fase A):
  fuente C + `.def` 36/36 verificado contra IAT; sin compilar
  ni ejecutar (sin MinGW/Wine en sandbox; requiere PC Windows).
- Sync: TESTING E-2/E-2.3, KNOWN I-23/I-01, MODERN FA-04/08/11 +
  M-13, REVERSE Etapa C + avance, ROADMAP método/E-2/Hito R,
  STATE Fase 1 + limitaciones, PATCH, RESEARCH, propuesta
  (§4 estado, §10 puerta). Panel intacto (ningún hito
  completado: E-2 listo ≠ ejecutado; causa I-23 ≠ fix).

## 2026-10-10 — 0.0.0 — M-13: revisión crítica Rev.1 de la propuesta (sin implementar)

- `docs/M13_DECOUPLE_PROPOSAL.md` Rev.1: 8 hallazgos aplicados —
  fronteras por-campo (`0x43DC3A`/`0x53FCB7`), fases A/B (FPS-1
  original inejecutable con throttle intacto), A no acelera
  (drenaje en el exe), tolerancias como placeholders + procedimiento
  k×σ_baseline (Hito R/MAIN-8), Q pared-absoluta aclarada, R-catchup
  como divergencia declarada, ORG-1 + C′ + REV-1/NREG-1/PASO-1
  corregidos, typo Original Mode. S-01…S-06 clasificados (P/N);
  nada refutado. Veredicto: MANTENER con REVISIÓN.
  M-13 PROPOSED; FA/M/hitos intactos; E-2 bloqueado; panel intacto.

## 2026-10-10 — 0.0.0 — M-13: propuesta de prototipo reversible sim/render (diseño)

- Nuevo `docs/M13_DECOUPLE_PROPOSAL.md` (diseño, NO implementación):
  arquitectura SIM/PRESENT + acumulador de paso fijo, pseudocódigo,
  puntos de intervención A/B/C (método lo decide Fase 3), riesgos
  R-* + supuestos S-*, criterios REV/SIM/FPS/PASO/NREG (umbrales
  propuestos), puerta de autorización (E-2 + Hito K + Fase 3).
  M-13 sigue PROPOSED; FA/M intactos; causa del límite de FPS no
  resuelta. Panel intacto (documentar no completa hitos MAIN).

## 2026-10-10 — 0.0.0 — Línea A+B: refs Glide/desacoplado + lectores `+0x10C` y 75 Hz

- Línea B (estático solo-lectura, exe EU md5 `692b1282…`, copia
  eliminada; nada ejecutado/modificado): lectores del cociente
  `+0x10C` LOCALIZADOS — R1 `0x53FED4` (snapshot→`ds:0x5ACBD4`,
  1W/1R) y R2 `0x451A0C` (delta→tabla `0x5ACBE5`→WORD; flags modo
  `ds:0x5ACBC4`∈{0,3} + `ds:0x5ACBC3`); alias `ds:0x551640`=
  `ds:0x551644`→`0x648900`; snapshot/restore contador `ds:0x65CD30`;
  `grSstWinOpen` juego=512x384@75 Hz pedidos (`0x46324D`), cine=
  640x480@60 Hz (`0x537EA6`); 25 FPS reforzado (sigue hipótesis:
  tasa real runtime sin confirmar); boceto desacoplado (diseño
  solo, sin implementar). Detalle: REVERSE_ENGINEERING Etapa C.
- Línea A (solo lectura; clones eliminados): catálogo S-26 —
  sezero/glide (impl `grBufferSwap`/`grBufferNumPending` minada),
  soul-re (Gex-2-engine: `GAMELOOP_DoTimeProcess` + `decoupleGame`,
  PSX/VBL), TRX (patrón paso-fijo+interpolación, ref. diseño),
  KAIN2-PC (débil: D3D, sin Glide, 2023). Detalle: RESEARCH S-26.
- FA-04 sigue PARCIAL (semántica tabla + dinámica E-2 pendientes).
  Panel intacto (sin hito).

## 2026-10-10 — 0.0.0 — Atribución estática `0x418FF8`: fn2 «tvend», ajena a M-13

- Puntero-dato único en `.data` `0x553378` (slot fn2 del registro
  «tvend» `@0x55336C`, tabla 60 registros `@0x5531C8`–`@0x553678`
  {idA, idB, fn1, fn2, fn3}: pantallas/entidades × mundos).
  Barrido byte-exacto: 0 refs estáticas a la tabla; 0 llamadores
  directos (108/110 fns; 2 markey solo intra-familia lvltv→markey,
  raíz huérfana). `0x418FF8` escribe `[obj+0x10C]=0x10`
  (`0x4190D5`): campos propios, NO el cociente/contador timing.
  Impacto M-13: ninguno. Falta cobertura dinámica (E-2/I-23).
  REVERSE_ENGINEERING fila + FA-04 actualizados. Nada ejecutado/
  modificado; copia eliminada. Panel intacto (sin hito).

## 2026-10-10 — 0.0.0 — E-3 documentado + M-13 estático: flujo temporal gameplay

- TESTING E-3 (nuevo, ejecutado solo-lectura 2026-10-10): superficie
  timing EU (Sleep+GetTickCount, t0 `0x43B36D`, `elapsed/16.666`
  `0x43E5DD`); candidatos (1) `push 0xFA0` `0x4637E4`, (2) Sleep idle;
  contexto candidato 1 = hold splash `\movie\logo` (estado 6, solo
  sub==1; tabla 12 slots `0x551668` vs 9 pares en disco — pregunta
  abierta); veredicto APTO reversible sin beneficio M-13. Nada
  ejecutado/modificado; copia eliminada; E-2 sigue BLOQUEADO (I-23).
- REVERSE_ENGINEERING Etapa C timing [~]: flujo temporal gameplay
  (init estado 3 → pre/update/sello → swap(3) → housekeeping, serie
  sin separación sim/render); throttle = `swap_interval` Glide
  (spec pública; 75/3 = 25 FPS, consistente Hito 1 — inferencia);
  consumidores mapeados (flag 1er frame sin lectores; `0x473238`
  ignora contador; `+0x10C` sin lectores en ámbito; `0x418FF8` no
  atribuible); sabido vs no-demostrable + próximo paso mínimo.
  FA-04 → PARCIAL; I-01 + M-13 apuntan. Panel intacto (acoplamiento
  sin prueba dinámica; A3.3 sigue 0/21).

## 2026-10-10 — 0.0.0 — Simplificación estratégica: ruta R→K→P + Olas 1–3

- ROADMAP: Fases 4–14 fusionadas en Hitos R/K/P + Olas 1–3 (IDs
  M/FA intactos); Hito P = MAIN 100% (sin tercera definición);
  Fase 1 recortada a restante real; catálogos F/S cerrados salvo
  necesidad de implementación.
- Clasificación: RUTA = FA-01/04/05/11, I-01/I-23, F-05/F-03/F-02,
  M-13+enablers; resto POSPUESTO (sin IDs eliminados). RE
  priorizado (B→C mínima→D mínima); Etapa A variantes [~] (E-1).
- Sync: E-2 BLOQUEADO (I-23) en STATE/TESTING/RESEARCH; P-F03
  cabecera factual; COMPAT matriz futura comprimida; README ruta
  + objetivos 3–4 acotados. Panel intacto (reorganización, §6).

## 2026-10-10 — 0.0.0 — I-23: dependencia de ruta fija (método E-2 bloqueado)

- Observación reproducida 3/3 (principal + copias A/B): renombrar
  `<DIR_PRINCIPAL>\` impide arrancar (`Cannot load font
  <DIR_PRINCIPAL>\font3.dff`). Método sin registro invalidado;
  mediciones no iniciadas. Origen sin investigar (orden expresa).
  KNOWN I-23 (nuevo) + TESTING E-2.2 + mapeo FA-11. Panel intacto.

## 2026-10-10 — 0.0.0 — E-2.1: copias A/B verificadas (hashes match)

- `E2_A_ORIGINAL` = original (md5), `E2_B_F03EU` = F-03 EU (sha256);
  principal conservada. Lanzamiento sin registro (carpeta propia).
  Mediciones pendientes de orden de inicio. Panel intacto.

## 2026-10-10 — 0.0.0 — E-2.0: backup original verificado (falsa discrepancia MD5)

- Paste mantenedor = pin repo = metadato Drive
  (`692b1282…`, match exacto case-insensitive): la diferencia era
  solo mayúsculas de PowerShell; docs correctos, sin corrección.
  Fuente de copia A verificada. TESTING E-2.0. Panel intacto.

## 2026-10-09 — 0.0.0 — E-1.1: identidad exe probado CONFIRMADA (sha256 match)

- Paste mantenedor (`<DIR_PRINCIPAL>\GEX3D.EXE`) =
  `7a9b6851…a3e8254`, match exacto vs bytes Drive EU (re-descarga
  + hash local + metadato; md5 `3198350e…` coherente). P-F03/B
  atribuido al exe analizado (build 1998-06-29); E-1 completo.
  Pin sha256 completo registrado en TESTING E-1.1. Staging
  eliminado; repo sin binarios. E-2 pendiente. Panel intacto.

## 2026-10-09 — 0.0.0 — E-1: F03-EU es build junio-1998, sin nueva API timing

- Descargas Drive→Arena verificadas (md5+sha256 3/3); análisis
  solo-lectura local (pefile+binutils); copias eliminadas después.
- RESEARCH S-26 (nuevo): huellas PE (links 05-18/06-29/06-24, x86
  GUI, mismo CRT); EU = build distinta (82.9% bytes ≠, 1/381
  bloques =); imports sin nueva API timing (+mciGetErrorStringA;
  Sleep+GetTickCount únicos); path `_demo` EU/US; EU≠US (67.3% ≠).
- PATCH F-03 + TESTING E-1 (ejecutado; scripts superados pero
  conservados) + KNOWN I-01 (mecanismo interno/de build) + STATE.
  E-1.1 (hash exe probado) pendiente mantenedor; E-2 pendiente.
  Panel intacto (sin hito).

## 2026-10-09 — 0.0.0 — P-F03/B: binarios F-03 + rama B (42–44 + speedup)

- RESEARCH S-25 (nuevo): `F-03_FPS_Limiter/` en Drive (EU 1559040 B
  md5 3198350e… mtime 1998-07-08; US 1665536 B md5 5dde47d8… mtime
  1998-06-24; TXT 682 B = README OP salvo URL corta); original EU
  re-confirmado; bytes inaccesibles desde aquí (egress bloqueado).
- PATCH F-03: tabla de binarios + TXT + stamps de era; US ≠ exe 2013
  S-14 (mismo tamaño, distinto hash). P-F03/B parcial: 42–44 FPS
  menú+juego, cinemáticas ≈15, sim acelerada, audio normal (rama
  «FPS↑» inesperada → E-1.1 identidad).
- TESTING: sección P-F03/B + E-1 (captura estática solo-lectura con
  scripts) + E-2 (micro-test sim-vs-FPS) pendientes de autorización;
  matriz actualizada. KNOWN I-01 (OBSERVADO, mecanismo pendiente) +
  I-02 (dato 42–44), STATE, COMPAT Notas. Panel intacto (sin hito).

## 2026-10-09 — 0.0.0 — Lote tools: H-01/H-02 + R-03/R-04 + upscale (S-24)

- RESEARCH S-24 (nuevo, hub): gex2-tools (extractor .VFX→PNG PC, sin
  licencia), Gex3DViewer (visor/exportador PC WIP, sin licencia),
  GexPSXLZSS (LZSS BIGFILE PS1, sin licencia), unLoKable (suite
  audio CD, MIT; 0 correspondencias PC EU) + upscale Mega «Screen
  Titles» (nada verificable: HTTP 500). API GitHub, sin clonar/
  compilar/ejecutar/descargar.
- PATCH_ANALYSIS: nueva categoría H-xx (herramienta específica Gex
  PC) con H-01/H-02 + R-03/R-04 (referencias secundarias) + filas
  de relación + taxonomía. M-23: candidatos externos anotados.
- CREDITS: SK83RJOSH + SalsaGal; MatBourgon ampliado. Panel intacto
  (investigación, sin hitos).

## 2026-10-09 — 0.0.0 — P-F03: prueba A/B del limitador F-03 preparada

- Enlace exacto verificado: PCGW «3DFX FPS Limiter EU/US patch» →
  tgames t12190 (URL legacy + canónica www, `patch-patch` vivo);
  adjunto SOLO-miembros (sin URL directa pública; registro gratuito
  requerido para revelarlo).
- F-03: README del autor archivado verbatim; negativos: sin nº de
  versión, sin nombre/tamaño de archivo, sin hashes, sin FPS objetivo.
  Detector `voiceuk`=EU cruzado con inventario §§6/8 (match nominal);
  entorno Win11+nGlide fuera del alcance declarado (solo 3Dfx HW +
  Win98–XP); evidencia funcional publicada: nula.
- TESTING.md: protocolo P-F03 (A/B ABA en copias aisladas, métricas,
  criterios pre-declarados, reversión) PENDIENTE DE AUTORIZACIÓN
  (A1–A4). Sin descargas, sin ejecución, sin modificaciones.
  Panel intacto (plan, sin hitos).

## 2026-10-09 — 0.0.0 — Lote 2: parches oficiales/comunitarios, FPS, música, mandos

- RESEARCH S-23 (nuevo): parche oficial 3dfx + Generic Update (3dfxzone
  1.18 MB, readme v1.0 verbatim ©1999 Midway/US; añade `gex23Dfx.exe`
  a base `gex23d`; genérico sin describir; `GX2PATCH.ZIP` no localizado;
  sin changelog/FPS/EU). S-01/S-07/S-08 ampliados (PCGW re-verificado,
  MAW extras 609/570 KB, AF liens: espejo F-01 356 Ko + packs).
- PATCH_ANALYSIS: F-13 oficial (nuevo) + F-01 (espejo AF) + F-03 (FPS
  objetivo sin declarar; limitador≠desbloqueador) + F-10 (verbatims
  Xbox/música; cerrado, no reutilizable). Taxonomía F-xx ampliada.
- KNOWN I-01/I-02/I-19 + PROJECT_STATE actualizados. 25 FPS: ningún cap
  documentado coincide (30/24); PAL⇒25 no establecido; R-1 vigente.
  Panel intacto (investigación, sin hitos).

## 2026-10-09 — 0.0.0 — 9ª sesión: atribución nGlide 2.10 CONFIRMADA + panel v3.3

- TESTING 9ª (equipo real, solo lectura): imports PE x86 = 6 DLL sistema
  (KERNEL32/USER32/GDI32/ADVAPI32/WINMM/VERSION), sin D3D/DXGI/Vulkan
  estáticos ⇒ por sí solos compatibles con época Y wrapper dinámico
  (dicotomía 8ª corregida: era falsa).
- Atribución CONFIRMADA por cadena de hashes (sin ejecutar nada):
  instalador S-14 = oficial nGlide 2.10 (md5/SHA1 de Zeus, t=557) =
  muestra Falcon 3cfcd03a…; su drop `glide2x.dll` = SHA-256 7cbd… =
  DLL cargada ⇒ Gex usa nGlide 2.10 (backend dinámico). RESEARCH S-22:
  autenticidad + tabla de drops + resolución escenarios.
- Panel v3.3 (solo datos): A2.3 «Atribución origen (L)» ✓ → A2 37→46%
  (25/54), global 18→20% (57/285); MAIN 40% intacto; corrige cabecera
  §4.4 (stale 15/54). Fila v3.3→(este commit): registrar en siguiente.

## 2026-10-09 — 0.0.0 — 8ª sesión: registro sin fechas, payload pendiente, imports propuestos

- TESTING 8ª (equipo real + Drive, solo lectura): nGlide 2.10 en
  registro pero InstallDate/InstallLocation vacíos ⇒ vía temporal
  inconclusa (sin inferencias); configurador 2.10 triple-confirmado;
  DLL 7cbd reconfirmada (continuidad ×3).
- Instalador `nGlide210_setup.exe` fijado en `S-14_extracted` (3301587 B,
  md5/SHA-256; coherente con corpus) pero payload sin inspeccionar:
  bloqueo técnico (binario no transitable, sandbox sin salida a Drive),
  no de permiso; .exe ni ejecutado ni descargado. Sin referencia
  pública del payload (negativo documentado).
- Siguiente paso único: lista de imports PE de la DLL (script PowerShell
  incluido, algoritmo validado; sin instalar nada). Atribución a nGlide
  2.10: SIN DEMOSTRAR. Panel intacto.

## 2026-10-09 — 0.0.0 — 7ª sesión: hash idéntico, sin sustitución, vendedor indeterminado

- TESTING 7ª (equipo real, solo lectura): `SysWOW64\glide2x.dll` =
  mismo hash 7cbd… (1630208 B) ⇒ sin sustitución; relectura 2.61.00.0658
  (disputa 3ª cerrada: error transcripción); ProductName ES vs EN en
  igual hash (recurso multilingüe probable); `nglide_config.exe` existe
  ⇒ instalador ejecutado, uso por Gex sin demostrar.
- RESEARCH S-22: resolución + docs oficiales nGlide (backend D3D+Vulkan,
  Glide 2.60, sin política de sobrescritura documentada; foro: aparece
  en Agregar/quitar) + 3 escenarios de coexistencia. d3d9 sigue sin
  probar nada.
- Siguiente paso mínimo: registro desinstalación (versión/fecha) +
  versión y lectura del configurador. Sin cambios de hitos/%: panel
  intacto («¿nGlide?» sigue vigente).

## 2026-10-09 — 0.0.0 — Atribución Glide: nGlide sin demostrar + corpus S-22

- Análisis de atribución (sin nuevos datos del PC): demostrado = carga
  de `SysWOW64\glide2x.dll` + wrapper=SÍ; que la implementación sea
  nGlide 2.10 NO demostrado (metadatos 3dfx vs marca nGlide; 3 nulos;
  sustitución sin confirmar). Panel: «nGlide probable» → «¿nGlide?»
  (solo redacción, sin cifras; wrapper=SÍ intacto).
- RESEARCH S-22 (corpus público): hash antiguo 7cbd… circula (dllme,
  1.6 MB, MD5 f59d9780…); dllme lo etiqueta 2.61.00.0658 ⇒ cadena
  2.60.0.658 de 3ª sesión EN DISPUTA; nGlide v1.x declara ProductName
  propio. TESTING 3ª/5ª anotados.
- Siguiente paso mínimo (solo lectura, PC): hash+metadatos completos
  de la DLL + presencia/lectura del configurador. Sin slow-mo/FPS aún.

## 2026-10-09 — 0.0.0 — 6ª sesión: A/B VSync nulo + intros ≈15 (estimado)

- TESTING 6ª sesión (equipo real): A/B VSync On/Off ⇒ ≈25 FPS en ambos
  (método: Steam; cierra laguna de método); VSync solo DESCARTADO como
  causa; líder: límite propio del juego. Intros percibidas ~15 FPS
  (NO medido; formato/frecuencia sin confirmar). Parpadeo = 1 cambio
  de modo. Sin tocar originales ni registro.
- KNOWN (I-01/I-02/I-14/I-22), COMPATIBILITY, PROJECT_STATE actualizados;
  §4.10/Trazabilidad A2.2 al día; registro v3.2→6114f0b. RESEARCH sin
  cambios (revisado; sin fuentes nuevas).
- Sin cambios de hitos ni porcentajes (resultado nulo, completa ningún
  criterio): dashboard intacto (v3.2).

## 2026-10-09 — 0.0.0 — 5ª sesión: nGlide 2.10 + 25 FPS + wrapper=SÍ (A2.2)

- TESTING 5ª sesión (equipo real): nGlide 2.10 instalado; `glide2x`
  2.61.00.0658 (ficha nueva; hash/tamaño pendientes); ≈25 FPS estables
  (método pendiente); Aspect/Refresh nulos; un parpadeo; RTSS/Steam
  inservibles (limitación de tooling). Experimento A/B VSync definido,
  pendiente de autorización.
- Wrapper=SÍ (A2.2 L ✓): instalador nGlide→SysWOW64 + render en HW
  moderno sin 3dfx. A2 28→37%, global 16→18% (52/285). MAIN intacto
  (40%). API efectiva y procedencia siguen abiertas.
- Panel v3.2 (solo datos): SVG + MD sincronizados; siguiente hito =
  causa 25 FPS (FA-05, I-01/I-02). KNOWN (I-01/I-02/I-21/I-22),
  COMPATIBILITY, PROJECT_STATE y RESEARCH (S-02) actualizados.

## 2026-10-09 — 0.0.0 — Regla permanente: dashboard solo con avance real

- `docs/PROJECT_STATUS.md` §6: nueva regla permanente — el SVG es el
  dashboard oficial; solo se actualiza ante cambio real y verificable
  (hito con evidencia, % justificado, cambio de estado, nuevo hito,
  corrección); sin avance real, SVG y MD intactos, sin commits vacíos;
  diseño aprobado conservado; informe por sesión.
- `docs/REPO_SYNC_RULE.md`: referencia cruzada a la regla.
- SVG intacto (sigue v3.1); sin cambios de cifras, metodología o diseño.

## 2026-10-09 — 0.0.0 — Panel de progreso v3.1: ajustes visuales

- `docs/PROJECT_STATUS.svg` v3.1 (760×700, misma tarjeta): «40%» MAIN
  reducido de 124 a 68px; secundarias ampliadas (resumen 12px, áreas
  26/9.5/9px, stats 15px, pie 9.5px); banda ámbar 100% sustituida por
  la etiqueta `VERSIÓN JUGABLE` bajo la barra; espacios redistribuidos.
  Composición, colores y estructura intactos. Cifras idénticas.
- `docs/PROJECT_STATUS.md` v3.1: geometría y registro actualizados; el
  significado completo del 100% MAIN (leyenda + avisos) permanece en
  §4.9. Metodología, evidencias y alcance sin cambios (ref `e9e846c`).
- README sin cambios. Sin preview raster (entorno sin renderizador;
  auditoría estática).

## 2026-10-09 — 0.0.0 — Panel de progreso v3.0: rediseño visual tarjeta

- `docs/PROJECT_STATUS.svg` v3.0 (760×700): tarjeta negra/morada estilo
  HUD retro — «40%» gigante + barra MAIN ancha con ticks 25/50/75,
  6/12 hitos y 14/35 puntos; leyenda ámbar `100% MAIN = PRIMERA
  VERSIÓN JUGABLE PREPARADA PARA PRUEBAS` + aviso de no distribución;
  global 16% secundario; 5 mini-tarjetas de área; resumen compacto.
  Sin scripts ni remotos; decoración mínima.
- `docs/PROJECT_STATUS.md` v3.0: misma metodología, cifras, evidencias
  y alcance MAIN (ref `e9e846c`); solo cambian resumen literal,
  geometría documentada y registro. Cifras idénticas a v2.0.
- README sin cambios (enlace y descripción siguen vigentes).
  Sin preview raster (entorno sin renderizador; auditoría estática).

## 2026-10-09 — 0.0.0 — Panel de progreso v2.0: héroe MAIN + jerarquía mejorada

- `docs/PROJECT_STATUS.svg` v2.0 (760×960): héroe MAIN (40%, 14/35 PTS,
  6/12 hitos, EN CURSO) con leyenda `100% MAIN = PRIMERA VERSIÓN JUGABLE
  PREPARADA PARA PRUEBAS` y aviso de no distribución; global 16% (47/285)
  independiente; 5 áreas con nombres claros y estados texto+color; resumen
  CONSEGUIDO/EN INVESTIGACIÓN/SIGUIENTE HITO. Estilo HUD negro/morado.
- `docs/PROJECT_STATUS.md` v2.0: alcance MAIN verificable en 12 hitos
  (6 completados con evidencia: inventario/tests/Hito 1/Fase 1; 6
  pendientes incl. API gráfica y wrapper); distingue jugable vs
  distribuible vs completo; procedimiento conjunto SVG+MD.
- README: aclaración MAIN vs global. Cifras de áreas sin cambios.
  Sin preview raster (entorno sin renderizador; auditoría estática).

## 2026-10-09 — 0.0.0 — Panel SVG de progreso + metodología (PROJECT_STATUS v1.0)

- Nuevo `docs/PROJECT_STATUS.svg`: panel de diagnóstico retrofuturista
  (morado/negro, estático, sin scripts ni remotos) con 5 áreas + global.
- Nuevo `docs/PROJECT_STATUS.md`: fuente de verdad — metodología de hitos
  ponderados (XS1/S2/M3/L5/XL8), estados verificables, fórmulas, exclusiones
  N/A documentadas, evidencias y procedimiento de actualización Arena.
- Cifras iniciales v1.0: global 16% (47/285 PTS); A1 32%, A2 28%, A3 0%,
  A4 0%, A5 33%. Trazables a FA/M-xx, TESTING, KNOWN_ISSUES y BUILD/LICENSE.
- README: sección «Panel de progreso» + fila en la tabla de docs.
- Fase 1 en curso; RE no iniciada; exe intacto. Sin preview raster (entorno
  sin renderizador SVG; validado por auditoría estática).

## 2026-10-09 — 0.0.0 — Hito 1 (4ª sesión): fecha/firma Glide + plan de publicación

- TESTING.md: nota 4ª sesión — producto exacto `Glide® for Voodoo Banshee®`
  (precisión de transcripción); fecha mostrada en Propiedades 2019-09-15
  00:54:48 (campo sin identificar, no atribuir); sin pestaña/firma digital
  visible. Fecha + ausencia de firma NO determinan procedencia; hash
  identifica contenido, no autenticidad; carga = pista, no prueba de API
  ni descarte de wrapper. I-21/I-22 sin causa definitiva.
- Publicación futura (planificación, nada implementado): BUILD.md (requisitos
  de distribución BYO + validación local), LICENSE.md §5 (evaluación
  jurídica por método, 6 métodos, pendiente) y §6 (lista de revisión:
  licencias, avisos, dependencias, historial, privacidad, docs), ROADMAP
  (6 hitos transversales, todos pendientes).
- README: resumen inglés corregido (`*Gex: Enter the Gecko* — REZengineered`).
- RESEARCH/COMPAT/KNOWN/GOALS sincronizados (4ª sesión). Fase 1 en curso;
  RE no iniciada; exe intacto. Sin autorización legal declarada.

## 2026-10-09 — 0.0.0 — Hito 1 (3ª sesión): ficha Glide Banshee 2.60 + SHA-256

- TESTING.md: nota 3ª sesión — `SysWOW64\glide2x.dll` (1630208 B):
  `3Dfx Interactive, Inc. Glide DLL`, producto `Glide para Voodoo
  Banshee` (®), versión 2.60.0.658, SHA-256 registrado; `3dfxSpl2.dll`
  (`3dfx Splash Screen` 1.0.0.4). Ambas cargadas en `GEX3D.EXE`.
  Interpretación: compatible con Glide 3dfx, pero NO prueba
  originalidad ni descarta wrapper. I-21/I-22 sin causa atribuida.
- RESEARCH (S-21 corroborado + hipótesis de origen), COMPATIBILITY
  (ficha en nota Hito 1), KNOWN_ISSUES (I-21: ficha registrada),
  GOALS (FA-02/FA-03: ficha sí, origen/wrapper UNKNOWN).
- Siguiente paso único: fechas + firmas digitales del fichero (solo
  lectura). Fase 1 en curso, Fase 2 sin iniciar, RE no iniciado.

## 2026-10-09 — 0.0.0 — Hito 1 (2ª sesión): glide2x cargada + M-26 intro omisible

- TESTING.md: nota 2ª sesión — `glide2x.dll` CARGADA (SYSTEM32 = físico
  SysWOW64 por redirección WOW64) + `3dfxSpl2.dll` (splash Glide 2.x,
  S-21); ruta Glide = hipótesis muy sólida, identidad pendiente.
  `ddraw/d3d9/dxgi/atidx9loader32` presentes, rol UNKNOWN (no implican
  renderer); GPU AMD probable. Puntos 7–8 actualizados; parpadeos
  multimonitor en transiciones de vídeo registrados.
- KNOWN_ISSUES: I-21/I-22 observados 2 veces (sin nuevos IDs); mapeo
  ampliado (I-21→M-04, I-22→M-05/M-19).
- M-26 Skippable Intro (PROPOSED, P3, sin fase): GOALS (Nivel 11) +
  mapa, ROADMAP (M-01…M-26 + bullet Fase 1), PROJECT_STATE, RE (FMV),
  RESEARCH (S-21 splash), CREDITS (r/3dfx).
- Siguiente paso único: ficha del fichero Glide físico (Detalles +
  `certutil` SHA256, solo lectura). Fase 1 en curso, Fase 2 sin iniciar.

## 2026-10-09 — 0.0.0 — Hito 1: enumeración 64-bit vacía = artefacto WOW64 (nota + paso)

- TESTING.md: nota post-Hito 1 — `(Get-Process GEX3D).Modules` desde
  PowerShell 64-bit solo muestra la capa WOW64 (7 módulos: exe + `ntdll`
  + `wow64*`); el filtro gráfico vacío es un artefacto de medida y no
  dice nada sobre el renderer (ni a favor ni en contra de Glide).
- Siguiente paso único: repetir la consulta desde PowerShell de 32-bit
  (`SysWOW64...powershell.exe`), solo lectura, sin instalar nada.
- Sin cambios de fase ni de estados FA: FA-02/FA-03 siguen UNKNOWN.
  Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Hito 1: primera prueba funcional EU en Win11 (F-05 sin F-01)

- TESTING.md: Hito 1 ejecutado en PC del mantenedor (Win11 64-bit):
  instalación manual `<DIR_PRINCIPAL>` (532 ficheros, MD5 del exe
  verificado), `.reg` (`Version`=2, `InstallDir`, `CDDriveName`=`D`),
  imagen CloneCD montada como `D:`; arranca, entra en nivel, movimiento +
  música/SFX OK. Exe inalterado (sin parches); nGlide no instalado
  durante la prueba. Renderer UNKNOWN (splash 3DFX no probatorio;
  `glide*.dll` en SysWOW64 de procedencia desconocida, carga sin confirmar).
- KNOWN_ISSUES: I-15/I-16 parcialmente confirmados (1 config); I-11 no
  reproducido aquí; I-06 intacto + nuevos I-21 (bandas/HUD) e I-22
  (iconos multimonitor) como observaciones propias; cabecera actualizada.
- COMPATIBILITY (fila Win10/11 + nota Hito 1), PATCH_ANALYSIS (F-05
  ejecutado sin F-01), RESEARCH (S-01), MODERNIZATION_GOALS (nota runtime
  FA, estados sin cambiar), TOOLKIT (Windows parcial), README /
  PROJECT_STATE / ROADMAP (testing: Hito 1) sincronizados.
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Siguiente paso:
  identificar la DLL gráfica cargada con PowerShell (solo lectura).

## 2026-10-09 — 0.0.0 — Evaluación de entornos Windows (sin probar)

- TESTING.md: alternativas A (Win moderno+wrapper), B (VM parcial), C
  (HW época referencia) + checklist 14 requisitos (IMP/REC/ESP) + MVP
  (A) y referencia ideal (C). Ninguna config declarada compatible.
- Sync: TOOLKIT §5.2 [~], PROJECT_STATE/ROADMAP (máquina pendiente,
  faltan datos HW). Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Protocolo de pruebas + matriz FA (Windows, sin ejecutar)

- TESTING.md: protocolo reproducible (entorno, hashes, instalación,
  pasos/veredicto, artefactos, fallos, custodia, limitaciones
  moderno-vs-época) + matriz priorizada FA-04…FA-15 (14 pruebas, sin
  nuevos IDs). Base EU-Glide; US/D3D/parches solo comparadores.
- ROADMAP `M-01…M-24` revisado: correcto como está (describe la Fase 0
  cerrada 2026-10-08; M-25 vive en Fase 1). Sin cambios; M-25 PROPOSED.
- Sync: PROJECT_STATE/ROADMAP (testing [~], máquina pendiente).
- Fase 1 en curso, Fase 2 sin iniciar. Nada ejecutado.

## 2026-10-09 — 0.0.0 — Matriz documental FA-01…FA-15 (sin RE)

- MODERNIZATION_GOALS: nueva sección de cobertura documental (15 filas:
  conocido, evidencia+fuente, tipo, relevancia EU, falta, siguiente
  acción, estado). 4 BASE DOCUMENTADA (FA-01/02/03/09), 8 PARCIAL, 3
  PENDIENTE (FA-04/07/15). Tabla FA intacta (todo TO INVESTIGATE).
- RESEARCH: patrón de guardado `GEX2%d%d%c.GEX` (estático) aflorado a la
  lista verificada (ubicación UNKNOWN). Sin nuevos IDs, sin binarios.
- Sync: PROJECT_STATE/ROADMAP (FA documental [~], dinámica → Fase 2+).
- Fase 1 en curso (no cerrada), Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Revisión cruzada F-01…F-12 + S-14 (solo documental)

- PATCH_ANALYSIS: nueva sección de revisión cruzada (tabla de seguimiento
  F-01…F-12, duplicidades, análisis S-14 en 5 puntos, prioridades
  documentales). Sin fusiones, sin nuevos IDs, sin binarios.
- Correcciones mínimas: RESEARCH S-02 («exe capeado» → cita motivadora,
  veredicto F-04); PATCH F-01 (ambigüedad `voice`/`voiceuk` explicitada).
  F-05/F-06/F-07/F-12 ya cumplían (sin cambios); historial intacto.
- Sync: PROJECT_STATE/ROADMAP (revisión cruzada anotada en fixes [~]).
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Duplicidad
  confirmada: ninguna; probable: F-04↔F-02 (descriptiva, pendiente binario).

## 2026-10-09 — 0.0.0 — S-14: inventario nominal + README leído (carpeta extraída)

- `S-14_extracted` (subida por el mantenedor): 22 entradas inventariadas
  solo por nombre+metadatos (README, Game Files con exe 2013 + 15 WAV, 3
  instaladores); README.txt leído íntegro (montar Gex3DD3D.ccd, copiar
  exe+music, nGlide, _inmm DirectShow, sin disco). Nada ejecutado.
- Clasificación: inventario+README CONFIRMADOS a nivel documental;
  procedimiento NTSC-D3D DECLARADO (no verificado); binarios/WAV/EU
  aplicabilidad DESCONOCIDOS. Sin nuevos IDs.
- Sync: RESEARCH (S-14), PATCH_ANALYSIS (F-09), PROJECT_STATE/ROADMAP
  (S-14 [x], binarios→Fase 2), KNOWN_ISSUES (I-11 _inmm), COMPATIBILITY
  (fila D3D), REVERSE_ENGINEERING (Etapa D: exe 2013 candidato).
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Historial intacto.

## 2026-10-09 — 0.0.0 — S-14: inventario interno bloqueado (solo metadatos)

- Acceso + hashes re-verificados (2ª comprobación, Drive solo lectura):
  ambas copias intactas (MD5/SHA-256 coinciden, no trashed, sin modificar).
- Índice ZIP + README: BLOQUEADOS en este entorno con evidencia —
  `read_file_text` falla (25 MB cap vs 411301184 bytes) y `download_file`
  solo ofrece bytes completos o URL no consumible; copia completa al
  workspace prohibida. Sin descarga, extracción ni ejecución.
- Sync: RESEARCH (S-14: re-verificación + bloqueo), PATCH_ANALYSIS (F-09),
  PROJECT_STATE/ROADMAP (S-14 [~] con bloqueo). Contenido sigue
  DESCONOCIDO; Fase 1 en curso, Fase 2 sin iniciar.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Precisión F-12 + inspección documental S-14

- Bloque A: F-12 ya no presenta el menú debug como confirmado
  (PATCH_ANALYSIS: «Qué es (DECLARADO POR LA FUENTE)» + «Valoración» con
  atribución explícita; COMPATIBILITY y REVERSE_ENGINEERING suavizados
  igual). A2 verificado sin cambios: Last updated 2026-10-09, M-25
  PROPOSED, Fase 1 en curso, Fase 2 sin iniciar.
- Bloque B: S-14 con metadatos Drive autorizados (solo lectura): 411301184
  bytes, MD5/SHA-256 registrados, copia `research/` bit-idéntica;
  discrepancia 411/~393 resuelta con evidencia (unidades). Página releída:
  cita corregida («Includes…»), README no publicado por separado.
  Contenido sigue DESCONOCIDO; S-14 sigue investigación pendiente.
- Sync: RESEARCH (S-14 reestructurado + pendiente), PATCH_ANALYSIS (F-12,
  F-09), PROJECT_STATE/ROADMAP (S-14 [~]). Sin binarios, sin descargas al
  sandbox, sin ejecución, sin fase avanzada.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Nueva propuesta M-25: selección de voces UK/USA

- MODERNIZATION_GOALS: +M-25 Voice Pack Selection (UK/USA), PROPOSED, P3
  provisional, Nivel 10 (Audio/Voces), sin fase (post-11, TBD); refs
  cruzadas M-20 (Audio) y M-24 (textos, no confundir); mapa actualizado.
- RESEARCH: nueva S-20 (reparto Gould/Phillips: Wikipedia verificada +
  VGFacts/BTVA; general, no PC-específica) + UNKNOWNs base de M-25.
- Sync: ROADMAP (M-01…M-25, item Fase 1), PROJECT_STATE (ampliación +
  Last updated), REVERSE_ENGINEERING (Etapa C audio/voces), COMPATIBILITY
  (nota variante regional), PATCH_ANALYSIS (F-01/F-03 → M-25), CREDITS.
- Sin implementación, sin binarios tocados, sin fase avanzada.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Lote 1: investigación documental de fixes (F-01…F-12)

- PATCH_ANALYSIS: F-01…F-09 re-investigados en sus fuentes (hilos leídos
  íntegros, citas verbatim) + F-10 (music handler), F-11 (NO-CD D3D) y F-12
  (debug tool, confirma menú debug en build D3D) catalogados. F-04 queda
  como probable duplicado de F-02 (sin pieza separada); F-06 no validado
  (hilo VOGONS no resuelto); t12123 (repack full-game) fuera de alcance.
- RESEARCH: S-01/S-02/S-03/S-04/S-05/S-08/S-09/S-14 re-verificadas (S-02
  absorbe el hilo Zeus «No music in Gex 2»: winmm/MCI + `_inmm.dll`).
- KNOWN_ISSUES: notas Lote 1 en I-01/I-11/I-12/I-14/I-16 (I-14 debilitada a
  hipótesis débil; I-11/I-12 con nuevos reportes adyacentes).
- COMPATIBILITY (fila «versión D3D»), REVERSE_ENGINEERING (Etapa D:
  F-01…F-12), PROJECT_STATE/ROADMAP (Fase 1 [~] en fixes) sincronizados.
- Historial preservado: entradas anteriores intactas.

## 2026-10-08 — 0.0.0 — Regla permanente de sincronización + revisión global

- Nueva regla permanente `docs/REPO_SYNC_RULE.md`: cada cambio relevante
  exige revisión global (README/ROADMAP/PROJECT_STATE/CHANGELOG/objetivos/
  issues/RE/compatibilidad/testing), corrección de contradicciones y
  checklist final antes del commit. Sin avances de fase unilaterales.
- Primera aplicación: Fase 0 cerrada, Fase 1 en curso en README, ROADMAP
  y PROJECT_STATE (antes decían Fase 0); ROADMAP Fase 1 con inventario
  completado y entorno/S-14 pendientes; Fase 2 anota el estático
  adelantado sin darse por iniciada.
- Evidencia Fase 1 propagada sin convertir hipótesis en hechos:
  REVERSE_ENGINEERING (Etapa A ✅, B+ bloqueadas), RESEARCH (nueva
  sección «Verificado por el proyecto», originales recibidos),
  KNOWN_ISSUES (notas estáticas en I-03/I-05/I-11/I-15/I-16/I-19, ningún
  issue reproducido), COMPATIBILITY (fila EU confirmada), PATCH_ANALYSIS
  (T-01/T-04: D3D = US/F-01, no EU), MODERNIZATION_GOALS (M-07: sin
  DirectInput en EU; avance parcial FA), TOOLKIT (Etapa A ejecutada),
  TESTING/BUILD/CREDITS (punteros, fase, crédito Mysticore).
- Historial preservado: entradas anteriores intactas.

## 2026-10-08 — 0.0.0 — Fase 1: inventario de artefactos originales (solo docs)

- Nuevo `docs/ORIGINAL_ARTIFACT_INVENTORY.md`: inventario completo de
  `REZengineered/originals/` (1585 ficheros): `disc_image/` (volcado
  CloneCD: hashes, TOC 1+16 pistas, aritmética de sectores) y
  `original_install/` (raíz 19+2, `GEX2/`, `LEVEL/` 72, `AUDIO/` 37,
  `VOICEUK/` 400, `MOVIE/` 18, `DIRECTX/` 85 + `DRIVERS/` 5×189).
- Identificados con evidencia: instalador InstallShield 5.x (stub NE +
  CABs `ISc(` v4), `GEX3D.EXE` (PE32, Glide exclusivo, rama
  `3dfx\release_europe`), edición europea v1.00.000 (`SETUP.INI` +
  `DATA.TAG`), DirectX 4.05.01.1600, CD mixto masterizado ≥1998-05-19.
- Conciliación CD→instalación vía manifiesto de `DATA1.CAB` (100%):
  400/400 voces, 72/72 niveles, 37/37 TADs, 18/18 vídeos.
- Verificación: 30 descargas temporales con SHA-256 30/30, eliminadas
  tras el análisis; el repo no contiene bytes del juego original.
- PROJECT_STATE: checklist y limitaciones actualizados. STOP Fase 1.

## 2026-10-08 — 0.0.0 — Nuevas ideas M-24 (localización) y X-02 (Rust/Bevy)

- M-24 Localization (PROPOSED, P3): estudio futuro de localización de
  textos (English original + Español + otros); solo textos, sin doblaje;
  English = source of truth; Trilogy no reutilizable ni referencia.
- X-02 Posible rewrite Rust/Bevy (DEFERRED, long-term research, prioridad
  muy baja): solo registrar la posibilidad lejana; no es fase de
  implementación; no abandona el engine original.
- ROADMAP Fase 14 menciona X-02 como long-term (sin fase de implementación).
- Nada implementado; prioridades actuales intactas.

## 2026-10-08 — 0.0.0 — Objetivos M-01…M-23 y roadmap por fases (solo docs)

- Creado MODERNIZATION_GOALS.md: FA-01…FA-15, M-01…M-23, X-01 (DEFERRED);
  todo PROPOSED / TO INVESTIGATE; jerarquía de implementación.
- ROADMAP reordenado en fases 0–14 (entender → base → sencillo →
  dependiente → complejo → experimental).
- RESEARCH: áreas de investigación futura (reportado vs hipótesis vs futuro).
- COMPATIBILITY: dimensiones futuras + matriz hardware (vacía) + Linux/Wine
  como secundario (X-01 DEFERRED).
- TESTING: categorías futuras; KNOWN_ISSUES: mapeo a objetivos (sin nuevos
  issues); PATCH_ANALYSIS: mapeo fixes → objetivos; REVERSE_ENGINEERING:
  vínculo con FA/M; README: enlace a goals.
- Nada implementado; ninguna afirmación convertida en hecho.

## 2026-10-08 — 0.0.0 — Taxonomía estricta + Fase 0 estricta + wrappers ronda 2

- README: eliminada la explicación del juego de palabras del nombre (queda
  solo el nombre del proyecto). Logo IA pendiente (no crear gráficos).
- PATCH_ANALYSIS: taxonomía §0 (ORIGINAL / F-fix / T-wrapper / R-referencia)
  + regla explícita: nada catalogado se asume solución final.
- Nuevos T-04 (DDrawCompat), T-05 (DxWnd, upstream por confirmar), T-06
  (WineD3D for Windows); fuente S-19.
- S-14: catalogado como material de investigación; prohibido moverlo al
  sandbox o modificarlo. Originales y análisis binario pospuestos por
  decisión del mantenedor (PROJECT_STATE, RESEARCH).

## 2026-10-08 — 0.0.0 — Referencias del mantenedor + jerarquía + herramientas

- Jerarquía fijada: PC original = fuente de verdad; otras versiones solo
  secundarias; Gex Trilogy fuera de referencias (README, RESEARCH §0).
- RESEARCH: S-11/S-12/S-18 enriquecidos (Gex64Decomp, Trilogy-demoted, REA);
  nuevas S-14 (PC Version Setup Package speedrun), S-15 (guía PS1),
  S-16 (DxWrapper), S-17 (dgVoodoo2).
- PATCH_ANALYSIS: F-09 (setup package) + sección herramientas T-01..T-03.
- Copia privada de S-14 en `Drive → REZengineered/research/` (411 MB,
  pendiente de extraer README e inventario).

## 2026-10-08 — 0.0.0 — Entorno RE sandbox operativo

- `tools/setup-sandbox-re.sh`: venv (`pefile`, `capstone`, `yara-python`) +
  REA CLI 5.0.0 en `/opt/rea-toolkit`. Verificado con self-test.
- `tools/setup-re-env.sh`: script (sin probar) para entorno completo
  Ghidra 12.1.4 + JDK 21 en máquinas con internet completo.
- Verificado e imposible en sandbox: JDK/Ghidra/Rizin/radare2 (documentado
  el porqué en `docs/TOOLKIT.md`); `rea doctor` confirma falta de backend.
- Estructura Drive `REZengineered/{originals,backups,analysis,builds,research}`.
- Actualizados PROJECT_STATE, TOOLKIT, tools/README.

## 2026-10-08 — 0.0.0 — Inicialización Fase 0

- Creación de la estructura inicial del repositorio.
- Documentos base: README, PROJECT_STATE, ROADMAP, RESEARCH, KNOWN_ISSUES,
  PATCH_ANALYSIS, COMPATIBILITY, REVERSE_ENGINEERING, BUILD, TESTING,
  CREDITS, LICENSE, CHANGELOG.
- Guías: `docs/TOOLKIT.md`, `docs/ENGINEERING_LOG_TEMPLATE.md`.
- `.gitignore` orientado a no redistribuir material propietario.
- Recopilación inicial de fuentes públicas (PCGamingWiki, foros, proyectos).
- Mapa inicial de problemas conocidos (todos **sin verificar**).
- Catálogo inicial de fixes comunitarios (todos **sin analizar técnicamente**).
- Sin modificaciones del juego (correcto según la Fase 0).
