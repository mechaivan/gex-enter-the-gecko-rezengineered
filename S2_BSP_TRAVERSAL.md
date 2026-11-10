# S-2 — Recorrido BSP de AZTEC16

**Objeto:** recorrido estructural del BSP de niveles (S-2, primer escenario original), con visibilidad original de cámara implementada y validada estáticamente, sin emisión de polígonos, sin buffers D3D11, shaders ni texturas.

**Referencias:** `S1_DFX_PARSER.md`, `U1_CONSOLIDACION.md`, `U1H_RESULTADOS.md`, `S2_CAMERA_VISIBILITY.md`, `docs/D3D11_FIRST_SCENE_PLAN.md`, `U1_DFX_RELOCATION_AUDIT.md`.

**Estado actual:** recorrido estructural y visibilidad de cámara implementados y
validados en `port/`. La visibilidad se documenta en
[`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md). La **ruta de emisión de
polígonos** está reconstruida con evidencia estática en §5, pero **no
implementada**. No se declara S-2 completo: falta la emisión en código y la
validación dinámica contra el ejecutable original.

---

## 1. Estado actual (vigente)

- Módulo `port/`: `include/gex/bsp.h`, `src/bsp/bsp.c`, `tests/test_bsp.c`.
- API: `gex_bsp_walk(image, image_size, root_offset, visible, ctx, cam_ctx, out)`,
  `gex_bsp_walk_free`, `gex_bsp_node_type`, `gex_bsp_node_is_leaf`.
- Algoritmo: DFS iterativo con marcos enter/exit, protección de ciclos (coloreado
  de 3 estados), referencias compartidas procesadas una vez, tope de 0x800 hojas.
- Lecturas seguras por bytes (`READ_U16`/`READ_U32`), `node_in_bounds` con resta
  anti-overflow, capacidades independientes, liberación en todos los caminos.
- Instrumentación de fallos exclusiva de tests (`GEX_BSP_TEST_FAULT_INJECT`),
  declarada solo en `test_bsp.c`, no en la API pública.
- Pruebas: **37 comprobaciones de BSP, 0 fallos** (más 889 de cámara en
  `test_camera`). ASan+UBSan sin errores. AZTEC16.DFX = **exactamente 815
  hojas** (predicado NULL).
- Compilación normal (sin macro): sin símbolos de inyección.
- Compilación de tests (integrada en `build.ps1 test` y en `make test`):
  `clang -DGEX_BSP_TEST_FAULT_INJECT -std=c11 -O2 -Wall -Wextra -Wpedantic -I include tests/test_bsp.c src/bsp/bsp.c src/camera/camera.c src/asset/dfx.c src/asset/sag.c -lm -o test_bsp`
- `test_camera` enlaza `src/bsp/bsp.c`, porque incluye la prueba de orden de
  hijos con `gex_bsp_walk`.

## 2. Nodo BSP (0x20 B)

| Offset | Tipo | Campo |
|---|---|---|
| +0x00 | int16 | centro X |
| +0x02 | int16 | centro Y |
| +0x04 | int16 | centro Z |
| +0x06 | int16 | radio (leído como **unsigned** en la visibilidad) |
| +0x0C/+0x0E/+0x10 | int16 ×3 | normal del plano, escala 4096 |
| +0x12 | uint16 | tipo: 1 = interno, 2 = hoja |
| +0x14 | int32 | distancia del plano |
| +0x18 | uint32 | hijo 1 (offset relativo a la imagen; 0 = ausente) |
| +0x1C | uint32 | hijo 2 (offset relativo; 0 = ausente) |

La clasificación del original es `(word[+0x12] & 1) == 0` → hoja. Con los tipos
reales 1/2 equivale a `& 2 != 0`, pero no son equivalentes para otros valores de
tipo. `port/` usa el predicado original `(tipo & 1) == 0`.

## 3. Evidencia que lo respalda

- `U1H_RESULTADOS.md` §6.1: firma `void walk(descriptor*, camera*, list*)`,
  pila explícita, arranque con `desc[0]` (raíz = `descriptor.root_bsp`),
  anexa hojas con tope `0x800` (`out[4 + 4*out[0]] = leaf`).
- Nodo interno: prueba de plano
  `d = ((camPos · N) >> 12) − node+0x14`. Si `d >= 0` apila primero `+0x1C` y
  luego `+0x18` (se visita `+0x18` antes); si `d < 0`, al revés.
- El descriptor se obtiene del contenedor vía `gex_dfx_load_level`
  (offset = `image[0]`); `root_bsp` está en `+0x00` del descriptor.

## 4. Visibilidad original (implementada en `port/`)

La visibilidad de `0x00401C00` **no** usa planos en `cam+0xC8/+0xD4/+0xD8`.
Usa dos matrices `int16` de 3×3, `cam+0x88` (**A**, 9 palabras) y `cam+0xA8`
(**B**, 9 palabras), construidas por `0x00405C41` a partir de `M = *[cam+0xE8]`
y aplicadas al centro del nodo con `0x0046999C` (`(M·v) sar 12`). `M` es el
resultado de la cadena de orientación
`0x46DD40` → `0x46E643` (documentada en
[`S2_CAMERA_VISIBILITY.md` §6](S2_CAMERA_VISIBILITY.md)): rotaciones Euler
**Z → Y → X** con ángulos crudos sobre `cam+0x30/0x32/0x34` (o
`cam+0x128/0x12A/0x12C` en la rama alternativa), cambio de ejes `M2`, y
`M = transpose(O) = R^T · M2^T`. La prueba es una esfera de radio `r` contra
**seis semiplanos** (near, far y cuatro laterales) con tolerancia `-r`:

```
(T_A.x − A_cam.x) > −r   &&  (T_A.x − A_cam.x) < cam+0x5C + r
(T_A.y − A_cam.y) > −r
(T_A.z − A_cam.z) > −r
(T_B.x − B_cam.x) > −r
(T_B.y − B_cam.y) > −r
```

La fila 2 de B (`cam+0xB4..0xB8`) nunca se compara. A y B se construyen en
`0x00405C41` a partir de la orientación `M = *[cam+0xE8]` y de los vectores
locales de `0x00402DC0`; el radio se lee **sin signo**. Todos los detalles,
fórmulas, pseudocódigo y discrepancias están en
[`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md).

**Implementación en `port/`:** `gex_bsp_walk()` acepta un `cam_ctx` opcional.
Cuando se proporciona, aplica la prueba de esfera contra 6 semiplanos y el
orden de apilado basado en `d = ((camPos·N) >> 12) − distance`. El predicado de
hoja es `(tipo & 1) == 0` (coincide con `& 2` para tipos 1/2). Cuando `cam_ctx`
es NULL se conserva el comportamiento anterior (predicado de devolución de
llamada y orden de apilado fijo).

La distancia `+0x14` se lee como dword sin signo y se **reinterpreta** con una
copia de bits (`bits_i32`), no con un cast de punteros: el original la usa como
entero con signo y el wrap de 32 bits de `(camPos·N) >> 12 − distance` se
reproduce en `gex_cam_plane_distance()` con acumulador `uint32_t`. Centros y
normales se reinterpretan igual (`bits_i16`), sin conversiones de
implementación definida.

`0x004269C2` (emisión de hojas visibles) y los emisores `0x00426256`/
`0x00426588` no se implementan en S-2 (fuera de alcance: buffers D3D11). Su ruta
está reconstruida con evidencia en §5.

## 5. Ruta de emisión de polígonos — reconstruida (2026-11-10)

**Alcance:** reconstrucción estática (solo lectura del desensamblado) de cómo las
hojas visibles se convierten en registros de comando y llegan al consumidor
Glide. **No se implementa nada**: `port/` sigue sin emisión, sin buffers D3D11,
sin shaders y sin texturas. Esta sección es la especificación de entrada para el
generador de buffers (S-3).

Marcas: **[A]** demostrado en el desensamblado; **[B]** parcialmente demostrado
(estructura sí, semántica no); **[C]** hipótesis.

### 5.1 Cadena de llamada y contrato

`0x0043DC3A` instala los emisores en el struct de nivel (70261-70267): **[A]**

| Campo | Valor | Papel |
|---|---|---|
| `level+0x4C98` | `0x004258BD` | caras de modelo (rango) |
| `level+0x4C9C` | `0x00425010` | todas las caras de modelo |
| `level+0x4CA0` | `0x004269C2` | emisión de hojas BSP |
| `level+0x4CA4` | `0x0040172B` | sin trazar en esta fase **[C]** |

Cada frame, `0x0043DED0`:

1. Llama `0x00401C00` → deja la lista de hojas visibles en `[0x64FEA4]` con
   formato `{u32 count; void *leaf[count]}` (70421). **[A]**
2. Si `dword[level+0x4C94] & 0x200 == 0`, llama `[level+0x4CA0]` con **cinco**
   argumentos (70432-70444). **[A]**

| # | Argumento | Papel |
|---|---|---|
| 1 | `[0x64FEA4]` | lista de hojas visibles |
| 2 | `*(level+4)` | descriptor de nivel: `+0x24` = tabla de vértices de 12 B |
| 3 | `*(level+8)` | cámara: `+0xE0` = matriz 3×3 Q12 + traslación `+0x14/+0x18/+0x1C` |
| 4 | `*(level+0x24)` | descriptor del buffer de comandos `{0, cursor, límite}` |
| 5 | `0x64FF00 + 0x6000*level[+0x14]` | array de 0xC00 buckets de 8 B |

3. Escribe el valor devuelto en `[level+0x24]+4` (70446-70448). **[A]**

**Contrato de retorno:** `0x004269C2` devuelve el **cursor final** del buffer de
comandos (43305-43306), no 0/1; por eso el llamador reescribe el mismo campo que
el emisor ya había actualizado. `0x00426256` y `0x00426588` devuelven 0/1
(emitido / descartado).

`0x0043E1A4` (70602), `0x00424C70` y `0x004622C2` llaman después
`0x00469836(bucket_array + 0x5FF8)` para dibujar la lista.

### 5.2 Dos estructuras distintas: comandos y buckets

`0x00424B0F..0x00424C0E` inicializa el doble buffer: **[A]**

| Global / campo | Valor | Papel |
|---|---|---|
| `level+0x4C10` | `[0x551644]+0x110+0x2580*level[+0x14]` | región de trabajo del nivel |
| `level+0x4C14` | `[0x551644]+0x2670+0x2580*level[+0x14]` | segunda mitad |
| `level+0x24` | `[0x64FEE0 + 4*level[+0x14]]` | descriptor de comandos (doble buffer: `0x64FEE0` / `0x64FEE4`) |
| `level+0x6F7C` | `0x64FF00 + 0x6000*level[+0x14]` | bucket array activo |
| `level+0x6F80` | `0x64FF00 + 0x6000*(1-level[+0x14])` | bucket array alterno |

El buffer de comandos se reserva en `0x0043B083`/`0x0043B0C0` (asignador
`0x00464FB3`, tamaño alineado a 4) y se inicializa así:

```
desc[+0x00] = 0                          ; contador; ningún emisor lo incrementa **[C]**
desc[+0x04] = desc + 0x0C                ; cursor
desc[+0x08] = desc + tamaño - 0x28       ; límite
desc[+0x0C..]                            ; comandos, contiguos
```

Los comandos se escriben **secuencialmente** desde `desc+0x0C`; el bucket array
solo guarda punteros encadenados a esos comandos. Son dos estructuras
independientes: el pool **no** es el buffer de comandos. **[A]**

`0x00465530(pool, 0xC00)` inicializa el bucket array (115595-115624): para cada
slot `i`, `slot[i][+4] = slot[i-1]`, `slot[0][+4] = -1` y `slot[i][0] = 0`.
El campo `[0]` queda a cero y actúa de separador; el encadenado real empieza en
`[+4]`. **[A]**

### 5.3 Hoja → polígonos

```
npolys = (int16)word[leaf+0x10];                            // 42361
polys  = leaf+0x0C;                                         // 42371
if (desc->cursor + npolys*0x34 >= desc->limit) skip_leaf;    // 42362-42368
for (i = 0; i < npolys; i++, poly += 0x14) { ... }
```

- Prueba de hoja: `word[leaf+0x12] & 2` (42355-42358).
- `npolys` se lee **con signo**: un valor negativo no itera.
- La reserva es **0x34 B por polígono**. Un polígono subdividido puede emitir
  hasta 4 registros de 0x34 (0xD0 B) y **no se ha localizado comprobación de
  límite dentro de los emisores** (`0x426256` lee `desc+0x04` una vez y nunca
  `desc+0x08`). La cota es, por tanto, aproximada: con muchos polígonos muy
  cercanos a cámara el original podría desbordar el buffer de comandos.
  **[A]** para la ausencia de comprobación; el desbordamiento real no se ha
  observado en ejecución **[C]**.

### 5.4 Polígono y vértice (repaso operativo)

| Polígono `0x14` | Tipo | Uso en la emisión |
|---|---|---|
| `+0x00/+0x02/+0x04` | `u16` | índices de vértice 0/1/2 en `desc+0x24`, stride **12** |
| `+0x06` | `u16` | flags (ver abajo) |
| `+0x10` | `ptr` | registro UV de 12 B: tres grupos `{u8, v8, word textura}` |

| Vértice `12` | Tipo | Uso |
|---|---|---|
| `+0x00/+0x02/+0x04` | `int16` | x/y/z locales |
| `+0x08` | `u32` | atributo empaquetado: color RGB + byte sin identificar |

Flags consumidos **en esta ruta** (el resto sigue en U1H §5.2):

| Bit | Efecto |
|---|---|
| `0x8000` | oculta el polígono salvo que `[0x551644]+0xA0 & 1` (42389-42398) **[A]** |
| `0x0004` | fuerza el emisor `0x426256` aunque la profundidad pida `0x426588` (42539-42543) **[A]** |
| `0x0001`/`0x0002` | sin consumidor localizado en la emisión **[C]** |

### 5.5 Proyección y clasificación por profundidad

`0x004269C2` copia los tres vértices a `0x646820`, `0x646828` y `0x646830`
(6 B cada uno: x/y/z) y llama `0x0046A1D0` sin argumentos (42420-42447). Ese
procedimiento usa la matriz de `cam+0xE0` (copiada a `0x646BE0..0x646BF0` y
`0x646BB4/B8/BC`, 42267-42313) y deja: **[A]**

| Global | Contenido |
|---|---|
| `0x647430`, `0x64742C`, `0x6474A0` | profundidades `z0`, `z1`, `z2` (16 bits útiles; se usan como divisor) |
| `0x647550` | `(y0<<16) | (x0 & 0xFFFF)` |
| `0x647558` | `(y1<<16) | (x1 & 0xFFFF)` |
| `0x647554` | `(y2<<16) | (x2 & 0xFFFF)` |

Las coordenadas se recortan a `[-0x400, 0x3FF]` antes de empaquetar
(120640-120685): son **píxeles enteros de 10 bits con signo**, no coma flotante.
El mapeo `0x647558 → vértice 1` y `0x647554 → vértice 2` se deduce del
emparejamiento con colores y UV en el registro (§5.8); el interior de
`0x46A1D0` no está trazado línea a línea **[B]**.

Clasificación (42483-42543):

```
maxz = max(z0, z1, z2)
if (maxz < 0x6A4)                  -> subdividir (§5.6)
else if (maxz >= [0x65BF80])       -> color por vértice con niebla (§5.7)
else                               -> color crudo = dword[vtx+8]
emisor = (maxz < [0x65BF04] || (poly+0x06 & 4)) ? 0x426256 : 0x426588
```

`0x65BF80` y `0x65BF04` son umbrales de distancia que reescribe cada frame
`0x004611ED` a partir del registro de niebla y de `cam+0x4C/0x4E/0x50`
(111191-111259); la fórmula exacta queda **[B]**. `0x6A4` (1700) es el umbral de
subdivisión **[A]** como constante, sin unidad establecida **[C]**.

### 5.6 Subdivisión en cuatro triángulos

Si `maxz < 0x6A4` (42483-42489) y el flag de subdivisión vale 1 (42594-42595):

1. Copia los tres vértices a `0x54F7C0/C4/C8` (posiciones) y calcula los puntos
   medios `0x54F7CC/D0/D4` con `(a+b)/2` truncado hacia cero
   (`cdq; sub; sar 1`). **[A]**
2. Calcula los atributos de los puntos medios:
   - color `0x54F7D8/DC/E0`: media por byte de los `dword[vtx+8]`;
   - UV `0x54F7E4/E8/EC`: media de `u` y de `v` por separado y **copia el word
     de textura de `+0x06`** del registro UV (42710-42800).
   El word de textura debe ser idéntico en los tres vértices; el original no lo
   comprueba. **[A]**
3. Proyecta y emite **cuatro** subtriángulos con `0x426256` (nunca con
   `0x426588`), abortando el polígono si alguno devuelve 0. **[A]**

La subdivisión es la solución del original al *warping* afín de texturas en
triángulos muy cercanos; para D3D11 equivale a un interpolado correcto por
perspectiva, pero **cambia la topología** (un triángulo pasa a cuatro).

### 5.7 Color por vértice y niebla

`0x0046B727` (121657-121720) mezcla cada byte del color del vértice hacia el
color de niebla:

```
salida[c] = desde[c] + ((hacia[c] - desde[c]) * factor) sar 12     ; clamp 0..255
```

- `desde` = bytes de `0x6474C4/C5/C6`, cargados con `dword[vtx+8]`.
- `hacia` = `0x6468A0`, `0x647574`, `0x647560`.
- `factor` = `[0x646854] = (int16)tabla[0x64FEA8][z >> 2]` (**[A]** para el
  cálculo; contenido y longitud de `0x64FEA8` **[C]**).
- El resultado se escribe de vuelta en `0x6474C4..C6`. **[A]**

Los tres destinos se cargan desde el registro de niebla
(`fog+0x40/+0x41/+0x42`) en `0x0043DE45..0x0043DE69`; `+0x44..+0x4A` y
`+0x4C/+0x4E` alimentan las tablas y umbrales de `0x0043B6C7` (67470-67489).
La semántica de esos campos (color, rango, factor) es **[B]**.

`0x426588` parte de **blanco** (`[0x6474C4] = 0x00FFFFFF`, 42109-42111) y aplica
la misma mezcla con `tabla[z>>2]` por vértice, de modo que su color final es la
niebla pura, no el color almacenado en el vértice. **[A]**

### 5.8 Emisores y formato de comando

Firma común (cdecl, 10 argumentos) de `0x00426256` y `0x00426588`: **[A]**

```
int emit(poly *p, u32 *uv0, u32 *uv1, u32 *uv2,
         u32 color0, u32 color1, u32 color2,
         desc *out, u8 *bucket_array, u32 debug_color)
```

Ambos: **[A]**

1. Salen si `[0x551644]+0xA0 & 1` (modo wireframe) y usan `debug_color` y
   `0x55555555` para el registro de depuración.
2. Llaman `0x46B6B5`, que calcula el determinante en pantalla
   `(x2-x0)(y1-y0) - (x1-x0)(y2-y0)` y lo guarda en `0x6468E0`
   (121628-121656). **Si el determinante es `>= 0`, descartan el triángulo**:
   el *back-face culling* es 2D, posterior a la proyección.
3. Calculan `idx = (maxz >> 2) + 4*byte[uv0+0x0B]` y salen si `idx` cae fuera de
   `[0, 0xC00)`. El byte `+0x0B` del registro UV es, por tanto, un **desplazamiento
   de bucket** (capa de orden de dibujo), no parte de la textura. **[A]**
4. Insertan el comando al frente del bucket:
   `cmd[+4] = bucket[8*idx+4]; bucket[8*idx+4] = cmd`. **[A]**

`0x00426256` escribe un registro de **0x34 B** con el opcode en `+0x0B`:

| Offset | Tipo | Contenido |
|---|---|---|
| `+0x00` | `u32` | longitud (`0x34`) |
| `+0x04` | `ptr` | siguiente comando o slot anterior del bucket |
| `+0x08` | `u32` | color0 (AARRGGBB) |
| `+0x0C` | `u32` | xy0 empaquetado |
| `+0x10` | `u32` | uv0 (u8, v8, word textura) |
| `+0x14/+0x18/+0x1C` | — | color1, xy1, uv1 |
| `+0x20/+0x24/+0x28` | — | color2, xy2, uv2 |
| `+0x2C/+0x2E/+0x30` | `u16` | `z0`, `z1`, `z2` (palabra baja) |
| `+0x0B` | `u8` | `0xB4 \| ((color0 >> 24) & 3)` |

Variantes: si `[0x551644]+0xA0 & 1`, escribe un registro de **0x20 B** con
opcode `0x4C` (wireframe) y `0x55555555`; si `& 0x1000`, un registro de **0x18 B**
con opcode `0x20` (sólido plano, posiciones en `+0x0C/+0x10/+0x14`). **[A]**

`0x00426588` escribe el mismo registro de 0x34 con opcode **`0xB6`**
(`0x4000` en `word[+0x1E]`, es decir, modo de textura 1) **más** un segundo
registro de 0x24 B con prefijo `0xE1` (`dword[+0x08] = 0xE1000040`) y opcode
`0x32` en `+0x0F`. El prefijo lo consume `0x46558A`: extrae el modo de
transparencia de `dword[+0x08]`, avanza el puntero 4 B y relee el opcode. **[A]**
El propósito del segundo registro sigue sin establecer (`U1_CONSOLIDACION.md`
§6.4).

### 5.9 Orden de dibujo y consumidor

`0x00469836(start)` recorre `while (start != -1) { if (dword[start]) 0x46558A(start); start = dword[start+4]; }`
(119918-119943). Como los slots vacíos valen 0, el recorrido salta el separador
y entra en la cadena de comandos del bucket. **[A]**

Con `start = bucket + 0x5FF8` (= slot 0xBFF) y `idx` creciente con `z`:

- orden de buckets: **0xBFF → 0**, es decir, de más lejano a más cercano;
- dentro de cada bucket, **LIFO** (el último emitido se dibuja primero);
- el resultado es un *painter's algorithm* por profundidad cuantizada
  (`z >> 2`) y capa (`4*byte[uv+0x0B]`). **[A]**

`0x0046558A` consume un comando por llamada: opcode en `byte[+0x0B]` (con
prefijo `0xE1`), `op & ~3` comparado con `0x4C`, `0xB4`, `0x34`, `0x24` y
`0x2C`, modo de textura = `word[+0x1E]` o `word[+0x1A]` desplazado 14 bits,
índice de 14 bits (`& 0x3FFF`) hacia `[0x6488F4]` y despacho `(op-0x20)` con el
mapa `0x469765`. Detalle completo en `U1H_RESULTADOS.md` §8 y
`U1F_RESULTADOS.md`. **[A]**

### 5.10 Diferencia con los modelos

`0x00425010` **no** comparte ruta con el BSP de nivel: **[A]** en lo estructural,
**[B]** en la semántica.

| Aspecto | BSP de nivel (`0x4269C2`) | Modelos (`0x425010`) |
|---|---|---|
| Fuente | lista de hojas visibles | `faceSet` `{+4 vértices, +0x10 count, +0x14 caras}` |
| Vértice | 12 B | **16 B** (`idx << 4`; lee dwords `+0/+4/+8`) |
| Cara | — | 12 B, con `+6` (índice a tabla `0x558800`, stride 8) y `+7` (flags) |
| Bucket | pool de nivel `0x64FF00 + 0x6000*k` | array global `0x558800` |
| Índice de bucket | `(maxz>>2) + 4*byte[uv+0xB]` | suma de profundidades y flags de cara |
| Salida | 0x18/0x20/0x34, opcodes 0x20/0x4C/0xB4/0xB6 | 0x18/0x20/0x24/0x2C, opcodes 0x20/0x24/0x30/0x34 |
| Culling | determinante 2D en `0x46B6B5` | ruta propia (`0x46AA14`) |

El emparejamiento exacto longitud↔opcode por rama en el emisor de modelos y la
semántica de `+6`/`+7` quedan **[B]**; no afectan a S-2.

### 5.11 Primitiva 2D: `0x004FF06F`

`0x004FF06F(r, g, b, x0, y0, x1, y1, flag)` **no** emite geometría 3D: escribe
una **línea 2D** de 0x14 B (opcode `0x40`) en el bucket 0 del pool
(`[0x551644]+0x24`), con color RGB en `+0x08` y cuatro `int16` en
`+0x0C/+0x0E/+0x10/+0x12`; opcionalmente añade un registro de 0x0C con prefijo
`0xE1` (`dword[+0x08] = 0xE1000068`). **[A]** No interviene en la ruta BSP.

### 5.12 Qué necesita un generador de buffers D3D11

Requisitos derivados de la evidencia anterior (no implementados):

1. **Transformación**: `M` de `cam+0xE0` (3×3 Q12 + traslación) y proyección a
   píxeles enteros recortados a `[-0x400, 0x3FF]`. Para D3D11 conviene conservar
   la matriz y proyectar en coma flotante; el recorte a 10 bits es una pérdida
   del original, no un requisito.
2. **Culling**: el original descarta por determinante 2D `>= 0`. Un pipeline
   D3D11 puede usar *winding order* + `CullMode`, pero el signo debe fijarse
   contra el original (el determinante es el negativo del convencional).
3. **Subdivisión**: si se quiere equivalencia visual exacta, replicar el umbral
   `0x6A4` y los cuatro subtriángulos; si se usa interpolado por perspectiva, se
   puede omitir, pero entonces la topología y el color por vértice difieren.
4. **Color**: `dword[vtx+8]` es el color base; la niebla mezcla hacia
   `fog+0x40..0x42` con factor `tabla[z>>2]`. `0x426588` usa blanco como base.
5. **UV**: `u8`/`v8` por vértice + word de textura compartido. Índice de 14 bits
   hacia `0x6488F4`; los 2 bits altos del word son el **modo de textura**, no
   parte del índice.
6. **Orden**: painter por `(z>>2) + 4*capa`; en D3D11 se sustituye por
   *depth buffer*, pero la capa `byte[uv+0xB]` sigue siendo necesaria para el
   orden relativo de superficies coplanares/decaladas.
7. **Capacidad**: reservar por comando, no por polígono (el original reserva
   0x34 por polígono y puede desbordar al subdividir).
8. **Culling por flags**: `0x8000` (oculto) y `0x0004` (forzar sólido) deben
   respetarse; el resto de bits no tiene consumidor en esta ruta.

### 5.13 Límites de esta reconstrucción

- Todo es **estático**: no hay traza de una frame real, ni comparación de
  registros emitidos contra el original.
- `0x46A1D0` (proyección) y `0x46AA14`/`0x46B6B5` se han caracterizado por sus
  entradas y salidas, no línea a línea.
- No se ha determinado el significado de `0x64FEA8` (tabla de factor de niebla),
  `0x65BF80`/`0x65BF04` (umbrales) ni `byte[uv+0x0B]` (capa) más allá de su uso.
- `0x0040172B` (`level+0x4CA4`) no se ha trazado.

## 6. Historial de auditorías (no es estado actual)

- 1.ª–4.ª auditorías: detección de ciclos, límite de hojas, lecturas
  desalineadas, validación de argumentos, aritmética de límites, memoria.
- 5.ª: aislamiento de instrumentación, pruebas deterministas.
- 6.ª: aislamiento definitivo, consolidación.
- 7.ª: compilación verificable, cierre documental.
- 8.ª: consolidación documental definitiva.
- 9.ª: sincronización del estado del proyecto en `PROJECT_STATE.md` y `ROADMAP.md`
  (solo documentación; sin cambios de código).
- 10.ª: visibilidad original de cámara documentada en
  [`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md) (solo documentación; sin
  cambios de código). Corrige el modelo de `cam+0x88`/`cam+0xA8` y el predicado
  de hoja.
- 11.ª: cadena de orientación de cámara reconstruida en
  [`S2_CAMERA_VISIBILITY.md` §6](S2_CAMERA_VISIBILITY.md) (`0x46DD40` →
  `0x46E643` → `0x405C41`: rotaciones Euler Z→Y→X con ángulos crudos —la 13.ª
  retiró la lectura previa de «ángulos negados»—, matriz
  base o identidad, cambio de ejes `M2`, `M = transpose(O)`, ramas alternativa
  y base forzada) (solo documentación; sin cambios de código).
- 12.ª: implementación de orientación y visibilidad en `port/` (`camera.h`,
  `camera.c`, `test_camera.c`, cambios en `bsp.c`/`bsp.h`/`test_bsp.c`);
  39 pruebas de cámara + 29 de BSP, 0 fallos, ASan/UBSan limpios
  (solo código; sin cambios de PROJECT_STATUS.svg). Cifra superada por la 13.ª
  (103 comprobaciones de cámara).
- 13.ª (2026-11-10): auditoría del commit `a3f433e` contra el desensamblado.
  Se corrigen `a_cam`/`b_cam` (A/B en lugar de M), `T_A`/`T_B` (A/B en lugar
  de M), el `sar 12` del marco local, la permutación de la normalización y la
  doble negación trigonométrica; se documenta la aproximación de tablas y la
  preservación de B fila 2. `test_camera` pasa a 103 comprobaciones y
  `test_bsp` se integra en `build.ps1`/`Makefile` (29 comprobaciones).
  Resultado: `build.ps1 test` completo, ASan/UBSan y gcc limpios. Cifras
  superadas por la 14.ª (889 de cámara y 37 de BSP).
- 14.ª (2026-11-10): auditoría aritmética del recorrido. Se elimina el
  desbordamiento con signo de `gex_cam_plane_distance()` (acumulador
  `uint32_t`, `sar 12` sobre los bits del registro y resta con wrap), se
  sustituyen los casts de implementación definida por copias de bits
  (`bits_i32`/`bits_i16`) y se corrige la lectura de la distancia `+0x14`,
  que se hacía con un cast `int32_t* → uint32_t*` (aliasing estricto).
  `test_camera` pasa a 889 comprobaciones (modelo de referencia x86 para
  distancia de plano y visibilidad) y `test_bsp` a 37 (distancia cruda
  `0x80000000` y orden de hijos). `build.ps1 test`, ASan/UBSan y gcc limpios.
- 15.ª (2026-11-10): reconstrucción estática de la **ruta de emisión** (§5).
  Se documentan la cadena `0x43DC3A` → `0x43DED0` → `0x4269C2` → emisores →
  buckets → `0x469836` → `0x46558A`, el contrato de retorno, el doble buffer
  (descriptor de comandos y bucket array como estructuras distintas), los
  formatos de polígono/vértice/registro UV, la proyección a píxeles de 10 bits,
  la subdivisión en cuatro triángulos, la mezcla de niebla, los dos emisores de
  nivel, los buckets por `(z>>2) + 4*capa` y las diferencias con los modelos y
  con la primitiva 2D `0x4FF06F`. Se corrige la afirmación de que la capacidad
  se comprueba por comando: solo se reserva 0x34 por polígono. Solo
  documentación; **sin cambios de código** y sin alterar las cifras de prueba.

Cifras históricas (7, 18, 23, 44 comprobaciones) corresponden a estados
intermedios y no son el resultado vigente. El resultado vigente es **37
comprobaciones de BSP + 889 de cámara**, 0 fallos.

## 7. Limitaciones

- Visibilidad y emisión reconstruidas, pero **sin validación dinámica** contra el
  ejecutable original: no hay traza de cámaras concretas ni comparación de hojas
  visibles o de registros emitidos. Las tablas trigonométricas son una
  aproximación matemática (equivalencia bit a bit no demostrada).
- Emisión de polígonos **documentada (§5), no implementada**: no hay lista de
  polígonos visibles ni registros de comando en `port/`.
- Sin D3D11, shaders, texturas.
- `PROJECT_STATUS.svg` sin cambios (MAIN sigue en 40 %; S-2 no completo).

---

**No se declara S-2 completo**: la visibilidad original está implementada y
documentada y la emisión está reconstruida, pero falta la emisión en código y la
validación dinámica.
