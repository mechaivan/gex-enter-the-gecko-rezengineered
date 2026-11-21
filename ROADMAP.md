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

### Subfase E — Primer Escenario Original (S-1)
  - **Validación dinámica de S-2 (2026-11-11):** la cadena de cámara del
    port se validó contra el proceso original y quedó **equivalente bit a
    bit** (M, A, B, a_cam, b_cam); se corrigieron tres defectos reales (negación
    de ángulos, aplicación de `M2` por columnas, truncado de la tabla
    trigonométrica).
  - **Segunda tanda (2026-11-11):** el nivel que carga el juego es
    `map7.dfx`, no `AZTEC16.DFX`; con el nivel correcto el recorrido reproduce
    **18/18 frames** con la lista de hojas visible idéntica al original.
    **S-2 no se cierra.** Detalle en
    [`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md) § 14 y
    [`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md) § 9.
  - **Tercera tanda (2026-11-11, emisión):** capturado el contexto real de
    emisión (`0x65CD60`, proyección, niebla) y reconstruida la rama de
    subdivisión del original. El port reproduce ahora la emisión de nivel
    **triángulo a triángulo**: mismo conjunto, **mismo orden de dibujo** y
    mismas fases de subdivisión (1123/1123 en el frame de referencia).
  - **Sexta tanda (2026-11-11, materiales de MAP7):** reconstruido el
    estado de material por polígono (índice de textura, modo de mezcla,
    color y niebla) y comparado con el original en ejecución. Corregido un
    defecto real: el índice de textura se leía del primer dword del registro
    UV en lugar del segundo, de modo que se cargaban y muestreaban las
    texturas equivocadas. El modo de mezcla pasa a ser **por polígono** (solo
    las texturas ARGB_4444 se mezclan con alfa) sin tocar el orden de dibujo
     ni introducir un *depth buffer*. Los 1123 colores por vertice coinciden
     con los del original.
  - **Septima tanda (2026-11-11, auditoria correctiva):** la hipotesis del
    «tinte de niebla por poligono» del modulo anterior **resulto ser
    incorrecta**: la variacion entre poligonos la produce el factor de
    niebla (funcion de la profundidad), no el color destino. Medido sobre
    las 3476 mezclas del frame 0 de MAP7, el color destino es constante
    `(B=0x14, G=0x14, R=0x5A)` y las mezclas con color negro tienen factor
    0 (sin efecto visible). La prueba pasa a exigir la coincidencia
    **exacta** de los 1123 colores. Se resuelve ademas la hipotesis que
    U-1f dejo abierta sobre el campo `+0x0C` del registro de textura: es
    el **formato** (`0x0C` = ARGB_4444, el unico que conserva el canal
    alfa). Se documenta el papel de la tabla NCC (descompresion YIQ_422 en
    la TMU, tipo NCC0, confirmado en ejecucion) y que la ruta de
    emision de nivel **no** lleva iluminacion (la de modelos si).
  - **Octava tanda (2026-11-11, cierre de auditoría de materiales):** la
    semántica exacta de `grAlphaCombine` queda **pendiente** y así se
    documenta: no hay encabezados del SDK de Glide 2.x en el repo ni en el
    entorno, pero la DLL real disponible
    (`C:\GEX_REZ\SMOKE_H4A\glide2x_gex_real.dll`) es una implementación
    software que genera el programa de la unidad de mezcla del Voodoo
    (plantillas por valor de parámetro en `0x1013D2xx`): es el material
    para derivarla, pero la derivación no está hecha. Lo **demostrado**
    es que el original mezcla **por polígono** según el formato y
    cuáles son los argumentos exactos; lo **hipotético** es que
    `(3,4,1,0,0)` equivalga a `SRC_ALPHA`/`INV_SRC_ALPHA`. El caso opaco se
    reproduce con independencia de esa aritmética (alfa de salida 1) y el
    impacto posible se limita a los 32 triángulos con ARGB_4444 del frame 0
    (2,8 %). La auditoría de YIQ_422 confirma que la decodificación en
    CPU del port coincide con la **referencia software del SDK** (bits,
    coeficientes `Y+A+B`, croma de 9 bits con signo, saturación, alfa 255,
    un byte por texel) y que el empaquetado de las 7 723 tablas NCC de los 37
    `.VFX` originales coincide byte a byte; **no** se afirma equivalencia
    píxel a píxel con el silicio del Voodoo, que exigiría capturar el
    framebuffer del juego. La prueba de las texturas con alfa se amplía:
    obtiene los índices de textura realmente usados por la geometría de MAP7
    (62) y exige que el subconjunto ARGB_4444 sea exactamente el de la
    referencia, en los dos sentidos.
  - **Undécima tanda (2026-11-12, secuencia de cámara hasta el menú):** con
    el mismo trazador de breakpoints de hardware y una copia de trabajo del
    exe (md5 verificado, sin parcheos) se ha reconstruido la secuencia de
    entrada al menú: sin teclas el juego se queda en el bucle de cine S4 (tres
    películas) y no entra en la ruta 3D; al entrar, la cámara **vuela sola**
    428 frames (~17,1 s a ~25 fps) desde el encuadre por defecto del nivel
    `(2341,4071,3074)` hasta el del menú `(2243,487,196)`, con aceleración
    inicial, crucero y frenada final. Se ha **descartado** que sea una
    cinemática de vídeo (el S4 va antes y en su propio bucle) y que sea una
    polinomio de grado bajo como descripción de la trayectoria (el ajuste
    polinómico de grado 3 deja residuos del orden del recorrido); esto no
    demuestra por sí solo una spline ni identifica el interpolador o el número
    de puntos de control, que siguen pendientes de decodificar. El bloque `0x43B56B..0x43B5CC` copia posición y orientación para la
    **colocación inicial**; no es la rutina que actualiza la cámara durante los
    428 frames. Quedaban pendientes el instante exacto de aparición de los
    elementos del menú y el mecanismo de la trayectoria.
    Documento nuevo: `S3_MENU_CAMERA_SEQUENCE.md`.
  - **Duodécima tanda (2026-11-13, controlador de cámara del menú):** el
    fichero de datos es **`level\map7.dfx`**, abierto por `0x461B90` en el
    mismo instante en que la cámara empieza a volar, y `0x461E0B` devuelve su
    **imagen compactada**, que es la región que el parser del port ya extrae
    (no hace falta E/S nuevo). El encuadre final del menú `(2243, 487, 196)`
    es un **literal de esa imagen** (offset `0xC356`) y la posición del nodo
    está en `+0x28`. El avance es **por frame** (contador int16 en `+0x46` del
    estado global `0x006466D8`): la unidad son los **428 frames**, no los
    17,1 s, de modo que a 60 fps la secuencia duraría ≈ 7,1 s. Se **corrige**
    lo publicado el 2026-11-12: `0x200` es la alineación de la imagen del
    `.DFX`, no un tamaño de registro. Quedaban pendientes la representación de
    la trayectoria y la rutina por frame (cerradas en las tandas 13.ª y 14.ª);
    siguen abiertos el papel de `+0x10/+0x12/+0x14` y el estado de menú.
  - **Decimotercera tanda (2026-11-13, el escritor de la cámara del menú):**
    con **watchpoints de escritura** (nueva opción `watch:<addr>` del trazador
    del repo) se identifica la instrucción que escribe cada campo de la
    cámara: la posición en `0x411BBB`/`0x411BCC`/`0x411BDD` (427 escrituras =
    una por frame del frame 1 al 428, y **paran** al final del vuelo) y la
    orientación en `0x4122E8`/`0x4122F6`/`0x412304` (563 = una por frame
    siempre). La cámara **copia** tres `int16` desde un puntero que produce la
    cadena `0x53E98C` → `0x53EF00` → `0x53DEB0` (buffer global `0x5AC608`):
    `0x53EF00` recorre un **array de registros de la imagen del `.DFX`** y
    **interpola entre el registro actual y el siguiente** con una fracción de
    15 bits guardada en el nodo. Quedaban pendientes quién avanza el nodo y la
    curva exacta; ambas se cerraron en la tanda siguiente (y el contador de
    `+0x46` no gobierna la selección).
  - **Decimocuarta tanda (2026-11-13, el algoritmo de la trayectoria, cerrado):**
    los tres pendientes anteriores quedan resueltos con medida. (a) **Quién
    avanza:** `0x53E98C` llama a `0x53F252` en `0x53E997` y a `0x53EF00` en
    `0x53E9B0`; se ejecuta **una vez por frame** desde `0x411B94` (607 llamadas,
    607 desde ahí; **428** llegan a evaluar = los 428 frames). (b) **Cuánto:**
    `+1` en la fracción de 15 bits por llamada; al llegar a `D` (el `int16` de
    `array[idx*32]`, duración en frames) hace `idx++` y `frac = 0`. Las 25
    duraciones suman **exactamente 428**. (c) **La curva:** el registro es de
    **32 B** (corrección: no 20 B) con punto y dos tangentes; `t = (frac15<<15)/D`
    y **Hermite cúbica** con aritmética entera (`>>15`, truncado a `int16`).
    Reimplementada en Python y comparada con las 428 muestras publicadas:
    **428/428 exactas**. Además, el contador `0x0064711E` **no** gobierna el nodo
    (coincide con la fracción en 4 de 607 escrituras). Trazador ampliado de
    forma aditiva (`watchdyn:`, `probe:`, `dumpindraw:`). Detalle en
    `S3_MENU_CAMERA_SEQUENCE.md` § 4.9-4.14.
  - **Decimoquinta tanda (2026-11-13, la trayectoria del menú en el port):**
    las tres rutinas se reproducen con **aritmética entera** en un módulo
    portable (`port/include/gex/camera_path.h`,
    `port/src/camera/camera_path.c`): avance `+1` de la fracción de 15 bits,
    cambio de segmento al llegar a `D`, centinela, evaluación Hermite Q15 y
    truncado a `int16`, sin coma flotante. El origen de los **26 registros** se
    reconstruye siguiendo la **cadena real de punteros** del `.DFX` (header
    `0x5FC` → actor `0x95EC` → `+0x28` → entidad `0xC00C` → objeto `0xC02C` →
    array `0xC034`); los 832 B coinciden con el volcado runtime. La referencia
    se genera con `analysis/gen_menu_camera_path_ref.py`. **1437
    comprobaciones, 0 fallos**, con las **428/428** posiciones exactas, la
    primera `(2341, 4071, 3074)`, la última `(2243, 487, 196)` y ninguna
    escritura tras el centinela; sanitizers O0/O2/asan/ubsan 24/24. La variante
    de rotación (10 B) se **rechaza**, no se aproxima: esa variante no es la
    del menú (ver la decimoséptima tanda). No se declara la cámara completa:
    faltan la integración de la orientación, el estado de menú y la
    conexión al renderer D3D11.
  - **Decimosexta tanda (2026-11-13, la trayectoria conectada al renderer
    D3D11):** cierra el circuito. (a) **Lectura de los registros reales**:
    `gex_dfx_load_camera_path()` recorre la cadena dentro de la imagen
    compactada del `.DFX` y **valida** (variante 0, cantidad, límites,
    centinela); los niveles sin esa estructura (AZTEC16, CIRCUIT0) se
    **rechazan** con `GEX_DFX_ERR_PATH`, no se inventa una trayectoria.
    (b) **Escena**: `gex_scene_level_load_camera_path()` deja el nodo en el
    estado original y `gex_scene_level_camera_path_step()` hace por frame el
    paso, escribe **solo la posición** y **re-emite** el nivel con la cámara
    nueva (mismo BSP, misma emisión, misma geometría; 0,50 ms por frame
    medido). (c) **Renderer**: `gex_d3d11_mesh_update()` sustituye el
    contenido de los buffers, y la app engancha el paso al **update lógico**
    (`--camera-path`) sin tocar pipeline, shaders, atlas, blending, niebla ni
    orden de dibujo. **Validación:** tres suites nuevas y exactas —
    `test_camera_path_scene` (2567: cadena, 26 registros, 428 posiciones,
    centinela, niveles ajenos), `test_camera_path_tsv` (1728: las 428
    posiciones contra el **TSV leído en tiempo de ejecución**) y
    `test_camera_path_render` (25: el buffer de vértices leído de vuelta de
    la **GPU** cambia al mover la cámara); `build.ps1 test` sin fallos y
    `run_sanitizers.ps1` **32/32**. **Ejecución real** (ventana Win32, GPU
    hardware): la cámara recorre la escena durante los 428 updates (los
    triángulos visibles cambian de 1123 a 41 y a 362 según el punto de vista) y
    **termina en `(2243, 487, 196)`**. **Límites declarados:** ~~la
    orientación todavía no se interpola~~ **(superado en la decimoctava
    tanda: el *look-at* ya está integrado y la matriz de emisión gira)** y el
    atlas de texturas (64 entradas) cubre el
    **91,3 %** de los vértices del vuelo, que tocan 166 texturas distintas
    **(cifras de aquella tanda, medidas con la orientación fija; con el
    *look-at* integrado son 231 texturas y 82,2 %, ver la decimoctava
    tanda)**.
  - **Decimoséptima tanda (2026-11-13, la orientación del menú):** cierra la
    pregunta de la **segunda trayectoria**. (a) **Medición**: los 428 ángulos
    del menú **no** salen de un cuaternio sino de un *look-at* (`0x406515` →
    `0x40AE51`) entre la **posición** de la cámara y el **punto de mira**,
    que son dos trayectorias **de 32 B** del `.DFX`: `0xC034` (posición) y
    `0xC39C` (mira, el actor hermano del slot 1). (b) **Aritmética
    exacta**: `len = (4*isqrt((d0²+d1²)<<4) + 8) >> 4` (`0x40534F`, con raíz
    **entera** `0x4647EB`) y `atan2` (`0x464181`) de la CRT en doble
    precisión convertido a `int16` en **Q12 de media vuelta** (`2048 = π`) y
    **truncado**; `euler = (-atan2(d.z, len), 0, atan2(d.y, d.x) + 0x400)`.
    (c) **Verificación**: **428/428** contra la serie publicada y **425/425**
    contra la captura directa del ejecutable original; referencias
    `analysis/menu_camera_rotation_map7.tsv`,
    `analysis/gen_menu_camera_rotation_ref.py` y
    `analysis/capture_menu_camera_rotation.py`. (d) **Resultado negativo**:
    la trayectoria de **10 B** (variante 1, `0xD118`/`0xD120`, 18 registros,
    `D` + cuaternión Q12, SLERP `0x43158D`, `0x431430`, `0x431036`) **sí
    existe** pero **no se evalúa** en el menú (16 289 impactos medidos): es
    de otro actor del nivel. (e) **Pendiente en aquella tanda** (superado en
    la decimoctava): integrar la orientación en el port e identificar las
    "matrices" `ctrl+0x4C`/`ctrl+0x26C` de `0x40A6F2`, que resultaron ser
    **puntos** de un muelle, no matrices.
  - **Décima tanda (2026-11-12, modos de mezcla por comando y byte alto del
    color de vértice):** los dos puntos que la novena dejó abiertos se
    **cierran con captura dinámica** del juego en ejecución (trazador con
    breakpoints de hardware sobre una copia de trabajo del exe; 160 086
    llamadas capturadas). (1) **Modos de mezcla**: MAP7 usa solo tres modos
    —`(1,5,4,0)` en la geometría del nivel, `(1,4,4,0)` en 6 triángulos (la
    textura 370) y `(4,0,0,0)` en las 128 primitivas 2D— y **nunca** los
    aditivos ni el de borrado; el estado global del port es correcto salvo en
    esos 6 triángulos, para los que queda **diseñada** la corrección (modo por
    triángulo y tramos de índices, sin tocar cámara, profundidad, orden de
    dibujo ni niebla) sin aplicarla en esta ronda. (2) **Byte alto del color
    de vértice**: el vértice real del juego (6 000 volcados) lleva el color en
    tres flotantes y **sin alfa**, y el byte alto del color del comando es el
    opcode; el alfa de la unidad de mezcla es 1.0. El port multiplicaba el
    alfa del texel por ese byte: **corregido**, con prueba de regresión que
    falla con el comportamiento anterior y validación con MAP7 en GPU
    hardware (1 123 triángulos, 20 frames, 0 errores). Queda **pendiente**
    medir si el driver compensa la escala RGB 0..2 de Glide, mientras que el
    port normaliza a 0..1; tampoco se afirma equivalencia con el SST-1 ni
    comparación píxel a píxel.
  - **Decimoctava tanda (2026-11-13, la orientación integrada en el port):**
    cierra el circuito de la cámara del menú. (a) **Refutación con
    evidencia**: `0x40A6F2` **no** es una rutina de matrices sino un
    **muelle/limitador sobre puntos** (3 × int16) con cinco modos (tabla de
    saltos en `0x40AD98`: `0x40A740`, `0x40A834`, `0x40AD6A` no-op,
    `0x40A989`, `0x40AA7D`); `ctrl+0x4C` y `ctrl+0x26C` son **puntos**, no
    matrices, y en la rama del vuelo (`ctrl+0x484 != 0`, retorno `0x411D0C`)
    el *look-at* usa los **samples crudos** de las dos splines, sin
    transformación. (b) **Constantes leídas del ejecutable**: `0x54C178` es el
    double `0x1.921fb54524550p+2` = **6.283185308** (no el `2π` de la libm) y
    `0x54C180` = **4096.0**; `0x540950` trunca hacia cero (`fistpll`), luego
    `atan = trunc(atan2(y,x) / 6.283185308 * 4096.0)` y el caso `atan2(1,1)`
    da **511**, no 512. `0x40534F` se transcribe exacto:
    `len = (isqrt_frac((d0²+d1²)<<4, 4) + 8) >> 4`, con `0x4647EB` completo
    (bit más alto, `sh >>= 1`, `cl = sh + frac/2`, refinado), sin `sqrt()`.
    (c) **Port**: `gex_cam_length_2d`, `gex_cam_atan2_q12` y
    `gex_cam_look_at` en `gex/camera.h`; `gex_dfx_load_camera_path_slot()` con
    `GEX_DFX_SLOT_CAMERA_LOOKAT` lee la mira (actor `0x961C` → `0xC374` →
    `0xC394` → `0xC39C`, 26 registros, suma 428); la escena avanza **dos
    nodos independientes** y en cada update calcula el *look-at* y
    **reconstruye las matrices derivadas** antes de re-emitir. (d)
    **Validación**: `test_camera` (995) cubre la matemática con valores
    conocidos, `test_camera_path_scene` (2701) la extracción de las dos
    trayectorias y el primer/último Euler, `test_camera_path_tsv` (5735)
    compara **428/428 posiciones, 428/428 miras y 428/428 Euler** contra los
    dos TSV leídos en ejecución, y `test_camera_path_render` (26) comprueba
    que la matriz de emisión y el buffer de la GPU cambian; suite completa
    **0 fallos** y sanitizers **32/32**. (e) **Ejecución real** (ventana
    Win32, GPU hardware): 428 muestras, `finished=1`, final
    `pos=(2243, 487, 196)`, `euler=(-29, 0, 2842)`, 1722 triángulos, salida
    limpia. (f) **Texturas medidas de nuevo** con la orientación real: 231
    distintas, las 64 más usadas cubren el 82,2 % de los vértices.
  - **Vigésima tanda (2026-11-15, el menú del televisor en el port):** el port
    dibuja el menú principal de MAP7. (a) **Ruta medida**: el original dibuja
    los actores con `0x471552(actor, matriz de cámara, out, modo)` →
    `0x4698B0` (transforma las tres columnas del registro) → traslación de
    `+0x14/+0x18/+0x1C` → `0x469B02` (focal 320, centro 256,192, near 160) →
    renderer virtual; `arg3 == 0` es la rama de **panel** `0x4718CF` y
    `arg3 != 0` la **texturizada** `0x470F96` (billboard `0x470C7B`).
    (b) **Formato del registro de animación (0x20 B, medido con el trazador)**:
    `+0x00` 9 × int16 base ortonormal Q12 **por columnas**, `+0x12` relleno,
    `+0x14/+0x18/+0x1C` int32 posición de mundo. Registros medidos: panel en
    `(2900,-84,-528)` con base `(-3977,976,0)/(-976,-3977,0)/(0,0,4096)` (se
    proyecta **fuera** del encuadre) y rótulos en `(2758,412,198/170/143/115)`
    = START/LOAD/OPTIONS/QUIT con base común
    `(663,-4041,0)/(4041,663,0)/(0,0,4096)`. (c) **`.DFX`**: `tvmenu__` clase
    `0x0DFE68` (1 modelo, `field_0x10 = 0`, caja de 8 vértices/12 polígonos,
    radio 552, sin textura: `uv_ptr = 0x20800080`); `tvbutn__` clase
    `0x0DC538` (20 modelos; el 2 y el 3 son el mismo quad de 4 vértices con
    textura `600` y `604`). (d) **Texturas `600..607`** decodificadas
    (128×32, ARGB_1555, LOD 1): `600` START, `601` QUIT, `602` OPTIONS,
    `603` LOAD en lavanda y `604..607` las mismas en rojo; la regla `+4` está
    medida en el par START. (e) **Port**: módulos `gex/actor` y `gex/menu`,
    `gex_emit_output_push`, `gex_scene_level_add_menu`, opción `--menu` y
    prioridad de las ocho texturas en la selección del atlas (sin ampliarlo);
    `test_actor` con **80 comprobaciones** contra MAP7.DFX y `test_menu` con
    **193** que fijan las constantes medidas. (f) **Validación**:
    suite completa 0 fallos, sanitizers **40/40** (O0/O2/ASan/UBSan) y captura
    real con GPU hardware en la que los cuatro rótulos caen a **1 px** de los
    del original, con las mismas palabras y colores. El fondo sigue mostrando
    la limitación conocida de visibilidad del BSP (no es regresión de esta
    tanda).  - **Decimonovena tanda (2026-11-14, la aparición del menú y la captura del
    encuadre):** separa el menú de pausa del menú principal y mide cuándo
    aparece. (a) **Medida dinámica** (breakpoints de hardware sobre una copia
    de trabajo del EU, MD5 `692B12825003417CC4A5A9D4DB13EEBE`): `0x52754F`
    (update de `tvmenu__`) se ejecuta **2413 veces**, una por frame;
    `0x527432` (mostrar/ocultar) **una sola vez**, en la carga
    (`t=28433 ms`), por la rama de **ocultar**. `[entity+0xF4]` pasa de `0` a
    **`2`** en `t=28586 ms` (~150 ms tras la carga) y se queda en `2` durante
    el vuelo; `count=4`, `idx=1`, `b13=0`. El menú **no** aparece al terminar
    los 428 frames: cámara y menú son independientes. (b) **Dos menús**: la
    tabla de `.rdata` con `MAIN MENU...`/`LEVEL SELECT MENU...`/`QUIT GAME`
    (`0x54E1E0`, acciones `0x423A8D`, `0x4238E2`, `0x423930`) es la del
    **menú de pausa/debug in-game**; el principal es el actor `tvmenu__` del
    nivel MAP7 (`0x463340` carga `\gex.dfx`, `map7.dfx`, `map7.vfx`,
    `map7.tad`). La tabla de actores `0x553204` asocia por **nombre**
    (`('tvmenu__','map_____') -> 0x527432, 0x52754F`); el `.DFX` no lleva
    punteros de código. (c) **Captura real**: el port gana
    `--screenshot PATH`/`--screenshot-frame N` (PPM P6 del backbuffer ya
    dibujado, antes del `Present`); con GPU hardware el vuelo termina en
    `pos=(2243, 487, 196)`, `euler=(-29, 0, 2842)` y la imagen muestra la
    estatua de Gex y un **muro BSP de cuatro pantallas**. (d) **Atlas**: la
    primera captura salía sin textura porque la pantalla solo aparece al
    final y quedaba fuera de las 64 más usadas del vuelo; el fotograma final
    usa **65** índices y `--textures-final`
    (`gex_scene_camera_path_final_texture_usage`) pide los 64 más usados de
    ese fotograma (descarta el 99, 3 vértices), sin ampliar el atlas ni
    tocar el shader; el muro sale texturizado. (e) **Límite declarado**: las
    opciones del menú **no** son BSP; las dibuja el actor `tvmenu__`
    (estado 2, 4 entradas), así que verlas exige render de actores, que es
    el siguiente paso y no se adelanta aquí.
  - **Novena tanda (2026-11-12, ecuación de la mezcla alfa sobre el driver
    real):** registro histórico de la investigación que identificó nGlide
    2.10 y obtuvo dinámicamente el programa de
    `grAlphaCombine(3,4,1,0,0)`. La ecuación genérica del programa es
    `r9.a = texel.a × v0.a`, emparejada con
    `(1,5,4,0)` → `SRC_ALPHA`/`INV_SRC_ALPHA`. La captura posterior de
    6 000 vértices de MAP7 precisó que el vértice no aporta canal alfa y que
    el alfa efectivo es constante 1.0; por tanto, en MAP7 la salida correcta
    es `texel.a`, no una multiplicación por un alfa variable de vértice.
    Esa corrección ya está aplicada y probada en la décima tanda. La novena
    sigue siendo la evidencia de la ecuación del driver, no el estado final
    del renderer. La limitación respecto al SST-1 y la comparación píxel a
    píxel con el Voodoo permanece.
  - **Quinta tanda (2026-11-11, cámara real en el renderer):** integrado el
    estado de cámara original en la ruta de escena y en el renderer D3D11:
    la matriz de emisión `cam+0xE0` se reconstruye desde la cámara
    (bit a bit contra el original), la visibilidad del BSP usa la prueba de
    esfera de la cámara y desaparece la dependencia de la matriz sintética
    y del encuadre automático en el modo de cámara. `map7.dfx` se dibuja con
    la cámara del juego y reproduce sus 307 hojas, su matriz de emisión y sus
    1123 triángulos en el mismo orden de dibujo. El modo diagnóstico se
    conserva (`--diagnostic-view`).
  - **Cuarta tanda (2026-11-11, corrección de la tercera):** la profundidad de
    referencia del original es la **máxima** de las tres, no la mínima
    (`0x426CC1..0x426D06`); la pasada anterior lo había leído al revés y
    había introducido una regresión en el orden de dibujo (14/1123 en lugar de
    1123/1123). Además, en la subdivisión **solo `det2d >= 0` aborta** (y
    solo en los tres primeros subtriángulos). La aparente diferencia de
    "1252 frente a 1123" era un artefacto de medición: `0x46558A` cuenta
    comandos de todo tipo (128 de tipo `0x1C` y 1 de tipo `0x14` no son
    triángulos de nivel). Detalle en
    [`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md) § 9.5.
 🔄 EN CURSO

- [ ] S-1: Parser DFX (2026-11-07) — parser del contenedor `.DFX` **implementado y validado** en `port/` ([`S1_DFX_PARSER.md`](S1_DFX_PARSER.md)): contenedor, relocalización y descriptor de nivel sobre AZTEC16.DFX y GEX.DFX. La integración del primer escenario ya tiene geometría, texturas y renderer D3D11; S-1 permanece abierto por el alcance restante del parser y su validación global.
- [ ] S-2: Recorrido BSP (2026-11-07) — **recorrido estructural y visibilidad de cámara implementados y validados** en `port/` ([`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md)): DFS con detección de ciclos, referencias compartidas, límite de hojas y 815 hojas en AZTEC16; la **visibilidad original de cámara y su cadena de orientación (`0x46DD40` → `0x46E643` → `0x405C41`) están implementadas** ([`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md)) y auditadas (889 comprobaciones de cámara + 37 de BSP, 0 fallos; `build.ps1 test` y `make test`), con la aritmética de 32 bits reproduciendo el wrap de x86. La **ruta de emisión de polígonos está implementada en código** (17.ª–18.ª pasadas): módulo puro `port/src/emit/emit.c` + `port/include/gex/emit.h` con `gex_level_emit` y sus fases (`collect`, `project`, `classify`, `subdivide`, `sort`), reproduciendo la proyección exacta de `0x46A1D0`, la niebla `0x64FEA8`/`0x461895`, los umbrales `0x65BF80`/`0x65BF04`, el determinante de `0x46B6B5`, el reparto de color entre `0x426256` y `0x426588` (segunda pasada de niebla), la prioridad de la subdivisión (4 principales + hasta 3 *slivers*, sin niebla), el gate completo de los emisores y la ordenación por `(maxz >> 2) + 4*layer` con desempate LIFO. La 18.ª pasada audita y corrige seis defectos (x/y sin inicializar, aritmética con wrap x86, offsets comprobados, mezcla de color real, subdivisión/emisores/orden verificados y AZTEC16 estructural) y deja `port/tests/test_level_emit.c` en **38 855 comprobaciones**, 0 fallos, limpio en Clang `-O0`/`-O2` y ASan/UBSan. Pendiente: completar la validación dinámica integral contra el ejecutable original; las tablas trigonométricas ya están verificadas bit a bit. S-2 no se declara completo.
- [x] S-3: Generador de Buffers D3D11 (2026-11-07, completado 2026-11-11) — **implementado en dos mitades**: conversión portable `gex_emit_output → gex_geometry` (`port/include/gex/geometry.h`, `port/src/geometry/geometry.c`) y creación/liberación de vertex/index buffers D3D11 (`port/include/gex/render/d3d11_mesh.h`, `port/src/render/d3d11_mesh.c`, Windows-only). Vértice de 28 B con posición, profundidad, color por vértice, UV, textura, capa y flags; deduplicación exacta y determinista; índices correctos para todos los triángulos, subdivisiones y pasadas de niebla; validación de punteros, conteos, multiplicaciones y desbordamientos antes de reservar o crear; `R16_UINT`/`R32_UINT` automático. Pruebas: `test_geometry` **6 963 comprobaciones, 0 fallos** (Clang `-O0`/`-O2` y ASan/UBSan) y `test_d3d11_mesh` **108 comprobaciones, 0 fallos** (WARP + hardware, lectura de vuelta, AZTEC16 real). **No incluye** shaders, `ID3D11InputLayout`, texturas ni `DrawIndexed` en este módulo aislado; esos componentes se integran en S-5. Detalle en [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §12–§13.
- [ ] S-4: Cargador de Texturas (2026-11-07) - cargar .VFX, construir la tabla de texturas (índice de 14 bits) y crear texturas D3D11. **4A (2026-11-11): el contenedor `.VFX` está demostrado e implementado** (`port/include/gex/vfx.h`, `port/src/asset/vfx.c`, portable): tabla de entradas, offsets y tamaños validados estrictamente (con validación estricta del EOF: la tabla tiene que cubrir el fichero entero), tabla NCC solo para el formato `YIQ_422`, correspondencia DFX→VFX verificada (los 9 609 polígonos de AZTEC16 usan los índices 0..3 y el `.VFX` tiene 260 entradas). **Módulo 4B (2026-11-11) y auditoría correctiva:** el registro VFX es un `GrTexInfo` de Glide 2.x; los identificadores 1, 11 y 12 son **`GR_TEXFMT_YIQ_422`, `GR_TEXFMT_ARGB_1555` y `GR_TEXFMT_ARGB_4444`**; el bloque de 112 B de `+0x14` es una **`GuNccTable`**, no una paleta; y las dimensiones se derivan del LOD (`256 >> lod`) y del aspecto (`8x1`…`1x8`), con el tamaño del bloque coincidiendo en **15 251 de 15 251 registros** de los 37 `.VFX` originales. **Módulo 4C (2026-11-11):** las texturas del `.VFX` ya se suben a D3D11 (atlas + `ID3D11ShaderResourceView` + sampler lineal/WRAP + muestreo en el shader con el índice de 14 bits) y el nivel de AZTEC16 sale **texturizado** en ejecución real con GPU hardware (3 010 + 324 comprobaciones, 0 fallos; suite completa 16/16). **Lo que falta: completar la validación dinámica de S-2, el *depth buffer* y el blending por polígono del original. La trayectoria de posición de MAP7 ya está conectada al renderer. Detalle en `docs/VFX_TEXTURE_FORMAT.md` §9 y `docs/D3D11_FIRST_SCENE_PLAN.md` §18.
- [ ] S-5: Integración con Renderer D3D11 (2026-11-07) — integrar módulos anteriores con renderer existente para renderizar AZTEC16 (incluye los shaders y el `ID3D11InputLayout` que el Módulo 3 dejó fuera a propósito). **Parcial (5A, 2026-11-11):** el pipeline de dibujo ya existe (`gex_d3d11_pipeline`: vertex/pixel shader de color por vértice compilados en ejecución con `d3dcompiler_47.dll`, `ID3D11InputLayout` de los 28 B del vértice, estados, viewport y `DrawIndexed`), el renderer R-3 lo usa antes del `Present` y la aplicación muestra un cubo 3D de prueba (`gex_shape_cube`). **Auditoría y primera ejecución real (2026-11-11):** corregida la liberación parcial (la limpieza ya no depende de una bandera), conservado el diagnóstico de error que se perdía al limpiar, añadidos los errores de dibujo al log, corregida la normal sin normalizar del sombreado, añadida la inyección de fallos por etapa del pipeline (287 comprobaciones offscreen con WARP, 0 fallos) y **ejecutada la aplicación real con ventana Win32 y swapchain en GPU real**: 30 frames presentados sin errores y **evidencia visual confirmada por captura de la ventana** (el cubo aparece con sus tres caras). Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` §14–§15. **Falta:** materiales NCC con iluminación, *depth buffer*, blending por polígono del original, la niebla aplicada después de la textura y `WM_SIZE`. La trayectoria de posición de MAP7 ya está conectada al renderer; la **rotación ya está integrada** (*look-at* del original, decimoctava tanda) y el **estado del menú** sigue pendiente. Las **texturas (S-4) ya están** (Módulo 4C) y la **carga del nivel real en el bucle de frames** también (Módulo 5B, §16).
- [ ] S-5b: Primera geometria real de un nivel (2026-11-11, HECHO) - nuevo modulo portable `gex_scene_level` (`port/include/gex/scene_level.h`, `port/src/scene/scene_level.c`) que encadena `gex_dfx_load` -> `gex_dfx_load_level` -> `gex_bsp_walk` -> `gex_level_emit` -> `gex_geometry_build` sin duplicar implementaciones, con estadisticas y una **vista diagnostica** (matriz sintetica + niebla 0x461895 + encuadre automatico) que **no es la camara original**. La aplicacion acepta `--level <ruta>`; sin el sigue el cubo de prueba. AZTEC16: 815 hojas, 2854 triangulos (1427 de niebla), 4914 vertices, 8562 indices. **Ejecutado con ventana Win32 y GPU real y verificado visualmente** (captura local, no publicada): se ven las estructuras del nivel. Pruebas: `test_scene_level` 8627 comprobaciones, 0 fallos. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` section 16. **Falta:** depth buffer, blending por poligono y la niebla despues de la textura. La trayectoria de posicion de la camara de MAP7 ya esta integrada; la **rotacion ya esta integrada tambien** (look-at del original, decimoctava tanda) y el **estado del menu** sigue pendiente. Las **texturas ya estan** (Modulo 4C, section 18).
- [x] S-5c: Fidelidad de las texturas de MAP7 (2026-11-16) — **causa demostrada: la escala UV, no el atlas.** El original escribe `s = byte` (`0x4661A6..0x4661B8`: `byte * 1.0f / 16.0f` en `sow`; `0x4684EF`: `1/16` en `oow`; el hardware divide `sow/oow`) y el espacio de coordenadas de Glide es **256 unidades = lado MAYOR de la textura, con escala uniforme en ambos ejes** (SDK: `255.0f * oow` recorre la textura entera; ruta de fuentes `0x432BE2`: `*256/16` con `oow = 1.0` sobre 16 columnas × 4 filas de 8×8 en 128×32). Medición de MAP7 (`uvprobe`, 16 292 polígonos, 288 texturas): cada textura usa 0..255 en el lado mayor y 0..(256·menor/mayor) en el menor; **ninguna recorre su UV más de una vez**. El port usaba `texels = uv * 256`, que repetía cada textura `256/lado` veces (32→8×, 64→4×, 16→16×): origen de los mosaicos y de la pantalla del televisor en cuatro partes. **Corrección:** `texels = uv * max(w, h)` y se retira el `/2` de los rótulos; equivalencia exacta para los rótulos (`byte/256*128 = byte/2`), demostrada. **Atlas:** el fotograma final usa 65 índices y con el menú hacen falta 73; `MAX_ENTRIES` 64→128 (constant buffer 4 KiB), cobertura del 97,6 % al **100,0 %** (atlas 4026×130). **Validación:** suite 0 fallos, `test_d3d11_pipeline` 343 comprobaciones (nueva `test_texture_uv_scale` con 64×32), regresión que falla con la fórmula antigua, sanitizers 40/40, captura real en GPU hardware con la cámara intacta (`samples=428`, `pos=(2243, 487, 196)`, `euler=(-29, 0, 2842)`). **Pendiente independiente (S-6):** la pantalla inferior del televisor muestra el cristal `311` en vez del logo `368` porque son quads coplanares y la regla LIFO dibuja el cristal después; no es UV y no se toca el orden de emisión sin demostrar qué modelo posee cada quad. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 22 y `docs/VFX_TEXTURE_FORMAT.md` § 9.2.

- [x] S-6: Auditoría visual de MAP7 (2026-11-17) — **defectos medidos, no supuestos**, comparando original y port en el **mismo encuadre** 512×384 (texto del menú a ±1 px, `samples=428`, `pos=(2243, 487, 196)`, `euler=(-29, 0, 2842)`). **(1) El parche plano `(16,16,24)` es el panel del televisor del menú**, no el suelo: `MENU_PANEL_COLOR 0xFF101018`, 12 triángulos sin UV que se proyectan en `(310,305)-(741,848)` y cubren 14 956 px; refutada la hipótesis del "suelo tex 265 plano" (tex 265 es gris y el suelo se dibuja con la matemática esperada). **Causa:** el menú se añadía **después** del ordenado; el original ordena el actor con la misma clave `(maxz>>2)+4·layer` (panel 146..291, suelo 193..242, rótulos 134..135) y el suelo lo tapa. **Corregido:** `GEX_MENU_PART_PANEL` se emite antes de `gex_emit_sort` y `GEX_MENU_PART_LABELS` después (las dos rutas medidas `0x4718CF` / `0x470F96`); captura con `--menu`: panel idéntico a la captura sin menú y rótulos idénticos a S-5 (0 px de 17 550). **(2) Las esquinas azules `(51,51,204)` no son geometría ausente**: ni con 249 hojas (2 820 polígonos) ni con 1 483 (16 292) hay polígono en `(20,20)`; es el clear `{0.2,0.2,0.8}` de `i4_main_win32.c:528`, y el original pinta un rectángulo 2D `0x64FB40` (handler `0x00469268`) con degradado. **(3) El fuego rectangular es la mezcla de la textura `370`** (aditiva `(1,4,4,0)` en el opcode `0xB6` del original frente al `(1,5,4,0)` global). **(4) Límites laterales, corregido:** el port dibujaba fuera de la zona lógica (29 941 px en la banda izquierda y 35 680 en la derecha de una ventana 960×540); ahora `ScissorEnable` + `RSSetScissorRects` con `gex_d3d11_pipeline_logical_rect` (la matemática del vertex shader): 0 px de fuga y 0 px distintos dentro. **(5) R/B sin demostrar:** el suelo (tex 265 gris) pide quitar el intercambio del shader y la pared lo contrario; no se toca. **(6)** El `depth-clip` del original (`0x46A1D0`, `0x4015e3`, máscara `0x7E07E000`) ya se reproduce y la diferencia de ~1 517 polígonos/frame era de AZTEC16, no de MAP7. **Validación:** suite 0 fallos, `test_d3d11_pipeline` 343→**409**, `test_scene_level` 12 335→**14 106**, `test_menu` 193→**223**, sanitizers 40/40. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 23.

- [x] S-7: Mezcla de renderizado por comando (2026-11-18) — **la selección de mezcla del original, transportada al renderer D3D11.** La regla de `0x46558A` se reproduce en `gex_emit_blend_for_command`: los comandos de nivel (`opcode & 0x04`) sin el bit `0x02` usan `ALPHA`; con él, el modo de textura decide — 0/1 con textura ARGB_4444 → `ALPHA`, 0/1 sin alfa → `ADDITIVE`, 2 → `ERASE (0,6,0,0)`, 3 → `ADDITIVE_ALPHA_OFF (1,4,0,0)`, >3 → `ADDITIVE`. El menú y la pasada de niebla conservan `ALPHA`. El triángulo lleva `opcode`, `texture_mode` y `blend`; `gex_emit_assign_blend` los reasigna en cada re-emisión y `gex_geometry_build` agrupa los índices en **tramos contiguos** (sin cambiar el orden, saltando clipeados) que el pipeline dibuja con un `DrawIndexed` y un estado por tramo. Cinco estados D3D11 (`ALPHA`, `ADDITIVE`, `OPAQUE`, `ERASE`, `ADDITIVE_ALPHA_OFF`); `pipeline->blend` sigue siendo el alias de `ALPHA` y una malla sin tramos recibe uno completo. La lista de texturas con alfa sale del **propio `.VFX`** (todos los registros con `format == 12`; 103 en MAP7), no de índices fijos. **Resultado:** MAP7 queda en **1 117 `ALPHA` + 6 `ADDITIVE`** (textura 370, opcode `0xB6`, modo 1, `ARGB_1555`) y **11 tramos**. **Validación:** suite completa sin fallos, sanitizers O0/O2/ASan/UBSan TODO OK; `test_level_emit` (`test_blend_rule` + `test_map7_blend`), `test_geometry`, `test_d3d11_mesh`, `test_d3d11_pipeline` (`test_blend_runs` con píxeles medidos y dos `DrawIndexed`) y `test_camera_path_scene` (6 aditivos en el fotograma 0, 428 muestras e idempotencia). Capturas reales 960×540 con GPU hardware contra el baseline `9ecb09d`: sin menú **56 997 px** distintos, todos dentro del bbox de la llama (`x 279..839`, `y 38..495`); con menú, de los 8 861 px del menú solo cambian **170**, todos dentro de ese bbox; fuera de la llama, **0 px**. No se tocaron cámara, profundidad, orden, scissor, menú, niebla, UV ni R/B. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 19.12 y `docs/VFX_TEXTURE_FORMAT.md` § 9.7.

- [x] S-8: Anomalías visuales de MAP7 — atlas completo y ancho del atlas (2026-11-18) — **dos defectos de empaquetado, no de geometría.** **(1)** El atlas admitía 128 entradas pero el vuelo de MAP7 usa **231** índices distintos; las que quedaban fuera del `gex_tex_entries` se dibujaban con el color por vértice (con niebla), es decir, **superficies planas**. Medido sobre el triángulo ganador en 512×384: 28 302 px (17,0 % de los cubiertos) en `samples=101`, 29 771 en `69`, 9 782 en `166`. **(2)** `atlas_layout` devolvía el ancho de la **última** fila en vez del máximo: con 128 texturas la fila 0 medía 4 070 px y la última 1 278, de modo que el atlas se creaba de 1 278 px y las texturas de la fila ancha invadían las filas siguientes y se muestreaban con WRAP fuera de su sitio. Corrupción latente desde el Módulo 4C. **Correcciones:** `GEX_D3D11_TEXTURE_MAX_ENTRIES` 128→**256**, array HLSL `gex_tex_entries[512]`, constant buffer 8 208 B (< 64 KiB); `atlas_layout` devuelve el ancho máximo de fila; nueva opción `--camera-path-freeze N` para capturas reproducibles. La aplicación pide las 231 del vuelo (**cobertura 100 %**; 239 con menú). **Validación:** suite completa sin fallos; sanitizers TODO OK; `test_d3d11_pipeline` **504** comprobaciones con la nueva `test_texture_set_multiatlas` (130 texturas 32×32, dos filas, ancho 4 080/alto 68 y texeles verificados en ambas filas); atlas real `1278×260`→**`4080×326`**; capturas reales 960×540 en poses congeladas (`samples=36/69/101/133/166/225/278`) antes/después con cambios del **36,4–47,0 %** y el muro de los grafitis legible; menú y llama sin regresión. **Pendiente:** la oscuridad general (escala RGB 0..2 de Glide, § 19.11) y el patrón de bloques de la hierba (propio de las texturas 64×64 YIQ_422). Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 24 y `docs/VFX_TEXTURE_FORMAT.md` § 9.8.

- [x] S-9: Oscuridad general de MAP7 — la tabla de color de Glide (2026-11-19) — **el original no entrega a Glide el byte de color normalizado, sino `min(2·byte, 255)`.** El motor convierte el byte con una **tabla de 256 flotantes en `0x555158`** (leída del ejecutable: 0→0,0; 1→2,0; 20→40,0; 127→254,0; 128→**255,0**; 255→255,0) al **construir el vértice** (`0x467d85`–`0x467dbf`, justo antes de `grDrawTriangle` en `0x467e5b`), **después** de la niebla (`0x46B727` desde `0x426d41`–`0x426dc5`), que mezcla sobre el byte crudo. nGlide usa ese flotante como **0..255 sin normalizar** y lo trunca (medido con un arnés de 32 bits y el `glide2x.dll` real: 0,5→0; 1,0→0; 127,5→127; 255→255; 510→255), así que el color efectivo es `min(2·byte,255)/255`. El port hacía `byte/255`: mitad de brillo hasta 127 y más oscuro aún por encima, porque el original satura en blanco desde 128. **Corrección:** `vertex_from_triangle` (`port/src/geometry/geometry.c`) aplica la tabla a R/G/B con el alfa intacto, en la construcción del vértice y **no en `emit`**, para que el registro de comando siga byte-exacto; el panel sintético del menú se entrega como su mitad exacta (`0xFF08080C`) y la pantalla conserva `0xFF101018`. No se tocan niebla, R/B, escala de texturas, atlas, cámara, orden, scissor ni mezcla. **Validación:** nueva `test_color_table` con los valores medidos; actualizados `test_geometry`, `test_d3d11_mesh`, `test_d3d11_pipeline`, `test_menu` y `test_scene_level`; `test_geometry` añadido a la matriz de sanitizers; suite completa sin fallos y sanitizers O0/O2/ASan/UBSan TODO OK; capturas reales 960×540 en poses congeladas (`36/69/101/133/166/225/278`) con luminancia media 34,3→45,0, 27,0→37,7, 33,9→45,1, 32,3→45,9, 38,5→53,5, 43,5→57,4 y 48,2→65,3 (centro +62 % a +100 %), blanco puro ≤ 0,38 % y 231 texturas en el vuelo / 239 con menú (cobertura 100 %), menú y llama sin regresión. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 25 y § 19.11.

- [x] S-10: Visibilidad BSP de MAP7 — cableada sin predicados (2026-11-19) — **la premisa «`scene_visible_all` dibuja todas las hojas» no describía el código**: desde S-3 los modos con cámara pasan `cam_ctx` a `gex_bsp_walk` y el recorrido aplica la esfera de los seis semiplanos original; el predicado era redundante. Control: con `cam=NULL` la pose final da **1483 hojas / 1818 triángulos** (vista diagnóstica); con cámara da **249 / 1722**. **Correcciones:** eliminado `scene_visible_all` (los dos recorridos usan `visible = NULL`; la visibilidad la decide solo `cam_ctx`, como en el original, que no tiene predicado adicional); `scene_emit_with_camera` reconstruye la cadena de orientación (`gex_cam_update`) sobre una copia local antes del recorrido y de la emisión; corregido el comentario obsoleto de `scene_level.h`; la aplicación ya no imprime las estadísticas del fotograma inicial como si fueran las de la pose congelada (`frozen pose stats`). **Validación:** nueva `test_map7_visibility_flight` (18 fotogramas medidos reproducidos en conteo y lista, subconjunto estricto de 1483, pose final 249/1722, cadena reconstruida con A/B vacíos, determinismo); suite 0 fallos y sanitizers TODO OK; capturas reales GPU hardware en `--camera-path-freeze 1/36/69/101/133/166/225/278` y menú con 307/1123, 402/1676, 455/1730, 122/414, 353/1834, 166/787, 127/414, 119/437 y 249/1722 (1742 con menú); control todas-las-hojas vs. cámara con PSNR medio 24,45 dB (36), 26,03 (69), 29,37 (278) y 37,86 (menú), la diferencia es la roca/edificios lejanos; capturas byte-idénticas a S-9 (PSNR infinito). **Pendiente:** comparación píxel a píxel con el framebuffer original en poses intermedias (sin captura de referencia), el rectángulo 2D de fondo `0x64FB40`, la pantalla inferior del televisor y el intercambio R/B. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 26, `S2_BSP_TRAVERSAL.md` § 10 y `S2_CAMERA_VISIBILITY.md` § 15.8.

- [x] S-11: Fondo de MAP7 — medido en el original y corregido (2026-11-20) — **el port limpiaba a azul de marca `(51,51,204)`; el original limpia a `(8,9,40)`.** Referencia real: grabación externa del ejecutable (`gdigrab`, 640×480, 38 s; vuelo ≈6,3–15,9 s y menú estático 16,2–30 s; 60 fotogramas a 5 fps, 48 a 30 fps y 10 fps sobre la parada). Comparación por hitos visuales: **no hay sincronía exacta pose↔fotograma** (fps variable). El fondo del vuelo es **plano `(8,9,40)`** en esquinas y bandas de cielo de los 60 fotogramas; en el menú el original monta un fondo 2D de interfaz con degradado `(15,10,51)`@`y=40` → `(46,10,118)`@`y=220` y estrellas. **Hallazgo:** `MAP7.VFX` tiene **652 texturas**; **325..365** son ese atlas de fondo (tiles 16×16/32×32) y sus medias reproducen el degradado; la tabla del nivel (**16 292 polígonos**, 9606 vértices) usa 320..324 y 366..370 y **nunca 325..365**, así que el fondo de la interfaz **no se puede emitir con el BSP**. El tile 325 es plano y vale `(8,8,41)`, coherente con el vuelo. **Corrección:** `gex_scene_background_rgba()` + `GEX_SCENE_BACKGROUND_R/G/B` (8/9/40) en `port/include/gex/scene_level.h` y `port/src/scene/scene_level.c`; `i4_main_win32.c` limpia con la función e imprime `clear color: RGB(8, 9, 40)`. Sin tocar cámara, proyección, BSP, culling, atlas, UV, mezcla, orden, niebla ni R/B. **Verificación:** nueva `test_background_color` (9 comprobaciones), `test_scene_level` **14 857** comprobaciones 0 fallos; suite completa 0 fallos; sanitizers O0/O2/ASan/UBSan **44/44 TODO OK**; capturas reales 960×540 GPU hardware (`--camera-path-freeze 1/36/69/101/133/166/225/278` y menú). Cobertura del fondo: pose 225 28,19 %→**46,14 %**; menú 9,59 %→**32,20 %**. Menú vs. original: MAE 44,72→**39,79**, PSNR 12,25→**13,36 dB**. **Limitación:** el encuadre del menú sigue difiriendo; la cámara de reposo está medida (fila `# estable` del TSV, muestra 427) pero no la del menú renderizado, y se descartó que sea solo focal (×1,405 desplaza y recorta la estatua) — no se toca cámara ni proyección sin medición nueva. Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` §27.

- [x] S-12: Superficies blancas de MAP7 — texturas fuera del atlas (2026-11-21) — **en el vuelo congelado aparecían superficies lisas y claras (blanco, lavanda, rosa) que parecían «volúmenes inflados»; no eran geometría ni D3D11, eran texturas que no estaban en el atlas.** Método: rasterizador software (512×384) que replica *vertex shader* y *pixel shader* del port; coincide con la captura GPU casi píxel a píxel, así que el defecto está en los datos entregados. **Causa raíz demostrada:** `--textures-final` construía el atlas con las texturas del **fotograma final** (65 índices en MAP7) mientras el port dibuja **todo el vuelo**; la muestra 133 usa **97** índices y **64 quedaban fuera**, afectando a **1 030 de 1 834 triángulos**. Cuando el índice no está en la tabla de búsqueda el *pixel shader* no falla: dibuja el **color por vértice sin textura**, que es el bloque liso claro. **Corrección:** `port/src/app/i4_main_win32.c` calcula siempre el conjunto del vuelo y solo cae al fotograma final si el vuelo **no cabe** en el atlas (con aviso por `stderr`); MAP7 son **231** índices y **239 ≤ 256** con las ocho texturas del menú. Sin tocar shader, atlas, UV, mezcla, orden, cámara, proyección, BSP, niebla ni color. **Verificación:** dos invariantes nuevos en `test_texture_usage` (`test_camera_path_scene` **2 877→2 879**), suite completa 0 fallos, sanitizers O0/O2/ASan/UBSan **44/44 TODO OK**, capturas reales 960×540 GPU hardware antes/después en las ocho poses y menú (MAD 41,22/45,29/39,05/28,14/6,73/7,93/0,23 → **0**; atlas 2 986×130→**4 080×326**, menú 4 026×130→**4 066×326**), y **sin regresión** en la ruta por defecto (poses 36/166/278 sin `--textures-final` byte-idénticas a S-11). **Hipótesis descartadas:** el cian de la papelera es auténtico (texturas 86/87/88 de mimbre gris + color de vértice `0x0E8FFF` = RGB(14,143,255) con la tabla `0x555158`); el borde negro dentado de la colina en la pose 101 también está en el original; el sangrado del atlas queda descartado por el borde replicado; en la pose del menú no falta ninguna textura (el `tex=0` es el panel sintético). **Limitación:** REA no está instalado en este host y no hay sincronía exacta pose↔fotograma (fps variable). Detalle en `docs/D3D11_FIRST_SCENE_PLAN.md` § 28.

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
  `test_d3d11_device` **332 checks, 0 fallos** con **Clang 22.1.8 y GCC 16.1.0**;
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