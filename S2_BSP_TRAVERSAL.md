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

### 5.5 Proyección exacta (`0x0046A1D0`) y clasificación

`0x004269C2` copia los tres vértices a `0x646820`, `0x646828` y `0x646830`
(6 B cada uno: `int16` x/y/z) y llama `0x0046A1D0` sin argumentos
(42420-42447). Antes copia la **matriz de emisión** `*[cam+0xE0]` a los
globales (42267-42313):

| Global | Origen | Tipo |
|---|---|---|
| `0x646BE0..0x646BF0` (9 words) | `[+0x00..+0x10]` | `int16` Q12, row-major |
| `0x646BB4/B8/BC` | `[+0x14/+0x18/+0x1C]` | `int32`, traslación |

`0x46A1D0` (120410-120712) aplica a **cada** vértice la misma aritmética de
32 bits (120412-120560):

```
; r = 0,1,2 (fila);  v = (x,y,z) del vértice
cam[r] = (M[r][0]*x + M[r][1]*y + M[r][2]*z) sar 12
x' = cam[0] + tx        ; +0x646BB4
y' = cam[1] + ty        ; +0x646BB8
z' = cam[2] + tz        ; +0x646BBC
```

- Nueve `imul` de 32 bits con operandos `movsx` de `int16`; suma de 32 bits con
  wrap modular, sin comprobación de desbordamiento.
- `sar 12` aritmético; acumulador de **32 bits**, nunca 64.
- `z'` **no** se reescala: es la profundidad usada para bucket, niebla y
  subdivisión.

Descarte (120520-120560): si **algún** vértice cumple `z' < [0x6474E8]`
(`0xA0` = 160) **o** `z' > 0xFFFF`, escribe `[0x647578] |= 0x40000` y retorna
sin proyectar: los tres se validan antes de dividir. **[A]**

Proyección (120560-120640), por vértice, con `z' != 0`:

```
xs = (x' * F) idiv z' + cx     ; F  = [0x646850] = 0x140 (320)
ys = (y' * F) idiv z' + cy     ; cx = [0x646814] = 0x100 (256)
                               ; cy = [0x646818] = 0xC0  (192)
```

- `imul` de 32 bits + `idiv` de 32 bits: **truncamiento hacia cero**, no `sar`.
  El dividendo puede desbordar y no se comprueba.
- Si `z' == 0` el original no divide (conserva el valor previo de los globales);
  el caso solo es alcanzable si `[0x6474E8] <= 0`, porque `z' < 0xA0` ya descarta.
- El recorte `[-0x400, 0x3FF]` se aplica **después** de la división y **antes**
  del empaquetado (120640-120685): dos `cmp`/`mov` por eje, no un clamp
  vectorial. Son píxeles enteros de 10 bits con signo.

Empaquetado: `(y<<16) | (x & 0xFFFF)` en `0x647550` (v0), `0x647558` (v1),
`0x647554` (v2). Los 16 bits altos son `y` con signo; los bajos, `x` de 16 bits.

Al final (120680-120712):

```
avgz       = (z0 + z1 + z2) idiv 3        -> [0x646C00]
[0x6474A4] = avgz >> 2                    -> índice de bucket base
[0x646854] = clamp(([0x647564] idiv avgz) + [0x647570], 0, 0x1000)
```

`[0x647564]`/`[0x647570]` son constantes del nivel (§5.15) y `[0x646854]` es el
factor de niebla **analítico**, que `0x4269C2` sustituye por la LUT (§5.15).

Clasificación (42483-42543):

```
maxz = max(z0, z1, z2)
if (maxz < 0x6A4)                  -> subdividir (§5.6)
else if (maxz >= [0x65BF80])       -> color por vértice con niebla (§5.7)
else                               -> color crudo = dword[vtx+8]
emisor = (maxz >= [0x65BF04] && !(poly+0x06 & 4)) ? 0x426588 : 0x426256
```

`0x65BF80` y `0x65BF04` son umbrales de profundidad que fija `0x461895`
(§5.15), no `0x4611ED`. `0x6A4` (1700) es el umbral de subdivisión **[A]** como
constante, sin unidad establecida **[C]**.

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

`0x0046B727` (121657-121720) mezcla cada byte de `[0x6474C4/C5/C6]` hacia el
color de niebla:

```
para c en {R,G,B}:
    desde = byte_c(0x6474C4)                       ; lo carga el llamador
    hacia = [0x6468A0] / [0x647574] / [0x647560]   ; fog+0x40/+0x41/+0x42
    out[c] = desde + (((hacia - desde) * factor) sar 12)   ; clamp 0..255
```

- `factor` = `[0x646854]`, que el llamador carga con
  `(int16)tabla[0x64FEA8][z >> 2]` (LUT, §5.15).
- `(hacia - desde) * factor` es `imul` de 32 bits; `sar 12` aritmético; el clamp
  `0..255` se aplica **después** del desplazamiento.
- El resultado se escribe de vuelta en `0x6474C4..C6`.

El color base **no es el mismo** en todas las rutas:

| Ruta | Color base | Efecto |
|---|---|---|
| `0x4269C2`, rama con niebla (42493-42525) | `dword[vtx+8]` por vértice | color del vértice mezclado con niebla |
| `0x4269C2`, rama sin niebla (42527-42535) | `dword[vtx+8]` copiado | color del vértice intacto |
| `0x426588`, registro principal de 0x34 (426889-4268a4) | argumentos del llamador | color **sin** niebla |
| `0x426588`, segundo registro de 0x24 (42695a-42696f) | `0x00FFFFFF` (blanco) | niebla pura como color por vértice |

Esto **corrige** la conclusión anterior («`0x426588` parte de blanco y su color
final es la niebla pura»): eso vale solo para su **segundo** registro. El
registro texturado conserva los colores del llamador y el segundo registro de
0x24 B con prefijo `0xE1` es la **pasada de niebla** del triángulo, no un
duplicado sin propósito (§5.8, `U1_CONSOLIDACION.md` §6.4).

Consecuencia de orden: `0x426588` es el emisor de polígonos **lejanos**
(`maxz >= [0x65BF04]`, con niebla) y `0x426256` el de polígonos **cercanos o
forzados** (`poly+0x06 & 4`), que recibe los colores ya calculados por el
llamador.

`0x46AA14` (120848-121018) es la rutina **equivalente para modelos**: transforma
la normal por `0x646860`/`0x646880`, modula `0x6474C4..C6` por la intensidad
(`imul` + `sar 12`, clamp a `0x1000`) y aplica niebla con `[0x646854]` usando
`and 0xFF` en lugar de clamp. **No proyecta** y no interviene en la ruta BSP.

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

Ese segundo registro es la **pasada de niebla** del triángulo (§5.7): sus
vértices `+0x10/+0x18/+0x20` son los mismos xy empaquetados y sus colores
`+0x0C/+0x14/+0x1C` son `0x46B727` aplicado a base **blanca**, es decir el
color de niebla puro por vértice. El registro principal (0x34) conserva los
colores del llamador sin mezclar. Esto **resuelve** el pendiente
`U1_CONSOLIDACION.md` §6.4. **[A]**

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
| Culling | determinante 2D en `0x46B6B5` | sin `0x46B6B5`; color por `0x46AA14` |

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

1. **Transformación**: la matriz de emisión `*[cam+0xE0]` (3×3 Q12 row-major
   + traslación `int32`, §5.14) y la proyección entera de §5.5. Para D3D11
   conviene conservar la matriz y proyectar en coma flotante; el recorte a 10
   bits es una pérdida del original, no un requisito.
2. **Culling**: el original descarta por determinante 2D `>= 0`. Un pipeline
   D3D11 puede usar *winding order* + `CullMode`, pero el signo debe fijarse
   contra el original: el determinante es `(x1-x0)(y2-y0) - (x2-x0)(y1-y0)`,
   el convencional, y se descarta cuando es `>= 0`.
3. **Subdivisión**: si se quiere equivalencia visual exacta, replicar el umbral
   `0x6A4` y los cuatro subtriángulos; si se usa interpolado por perspectiva, se
   puede omitir, pero entonces la topología y el color por vértice difieren.
4. **Color**: `dword[vtx+8]` es el color base; la niebla mezcla hacia
   `fog+0x40..0x42` con factor `tabla[z>>2]` (LUT de §5.15). `0x426588` añade
   una segunda pasada de niebla con base blanca; `0x426256` no aplica niebla.
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
- `0x46A1D0`, `0x46B6B5`, `0x46B727`, `0x461895` y `0x46DD40` están
  reconstruidos línea a línea; `0x46AA14` está caracterizado por su efecto
  (iluminación + niebla de modelos) sin trazar las nueve multiplicaciones.
- Queda **[B]** la semántica exacta de `+0x44..+0x4A` del registro de niebla
  (segundo color / paleta Glide) y del override `+0x50`.
- No se ha localizado el **punto de reserva** del buffer de la LUT
  (`[desc+0x54]`): solo su escritura (`0x43B7DC`) y su relleno (`0x461895`).
- `byte[uv+0x0B]` (capa) se usa como índice pero no se ha encontrado su
  generador.
- `0x004013A0` (emisor de geometría de modelos) está identificado por su
  contrato, no trazado por completo.

### 5.14 La matriz de emisión `cam+0xE0`

`cam+0xE0` es un **puntero** a una estructura de 0x20 B, distinta de la matriz
de visibilidad:

| Offset | Tipo | Contenido |
|---|---|---|
| `+0x00..+0x10` | `int16[9]` Q12, **row-major** | fila 0 = x de pantalla, fila 1 = y de pantalla, fila 2 = profundidad |
| `+0x14` | `int32` | traslación x (en unidades de profundidad) |
| `+0x18` | `int32` | traslación y |
| `+0x1C` | `int32` | traslación z |

Construcción: `0x46DD40(cam)` (124412-124964) la reescribe **cada frame**:

1. `R = RotX·RotY·RotZ` sobre `cam+0xF0` (identidad si `cam+0x43C != 0`).
2. Cambio de ejes `M2 = [0x1000,0,0; 0,0,0xF000; 0,0x1000,0]` aplicado por
   columnas.
3. Escalado **por columna** con los words `cam+0x490` (col 0), `+0x492`
   (col 1) y `+0x494` (col 2).
4. Traslación: `0x46999C` multiplica por `-camPos` (y por `0x647520/524/528`)
   y deja el resultado en `+0x14/+0x18/+0x1C`.

Relación con las otras matrices de cámara:

| Campo | Tipo real | Papel |
|---|---|---|
| `cam+0xE0` | puntero a matriz de emisión (0x20 B) | **proyección**: mundo → pantalla |
| `cam+0xE4` | puntero a `O` (9 words) | rotación intermedia, por columnas |
| `cam+0xE8` | puntero a `M` (9 words) | visibilidad = `transpose(O)` |

`+0xE0` y `+0xE8` **no son la misma matriz ni un alias**: `0x46DD40` escribe
`+0xE0` y `+0xE4`; `0x46E643` deriva `+0xE8` de `+0xE4`. Comparten la rotación
de partida, pero `+0xE0` añade cambio de ejes, escalado por columna y
traslación; `+0xE8` no lleva traslación (la visibilidad usa `a_cam`/`b_cam`).

Consumidores de `*[cam+0xE0]`:

- `0x4269C2` (polígonos BSP): copia las 9 words y la traslación **tal cual** a
  `0x646BE0..`/`0x646BB4..` y llama `0x46A1D0` (§5.5).
- `0x4013A0` (geometría de modelos/objetos): copia las 9 words y la traslación
  **`sar 4`** a los mismos globales y llama `0x46A1D0`. Ese `>>4` es el único
  cambio de escala entre ambas rutas: los vértices de modelo están en Q4.

`0x40172B` **no** construye la matriz. `0x43DCA5` lo instala en `level+0x4CA4`
como *callback* y `0x43DDE9` lo invoca como `0x40172B(lista, cam, a, b)`.
Recorre registros de 0x18 B (`+0x20` cuenta, `+0x24` base), descarta
`word[rec] & 1`, prueba el registro contra `-radius` y llama
`0x4013A0(*[cam+0xE0], rec, a, b)`. Es un *flush* de registros de objeto, no
configuración de proyección. **[A]**

`0x4013A0` recorre registros de 0x0C B (tres índices `int16`), toma los
vértices de una tabla de 8 B (`int16 x/y/z + relleno`), proyecta con `0x46A1D0`
y descarta con la máscara `[0x647578] & 0x7E07E000` — **distinta** de la
`0x60000` de la ruta BSP — antes de escribir comandos planos de 0x20 B
(opcodes `0x4C`/`0x20`) en la lista de `[arg+0x5FFC]`. No aplica niebla.

### 5.15 LUT de niebla `0x64FEA8` y umbrales `0x65BF80`/`0x65BF04`

`0x64FEA8` **no es una tabla estática**: es un puntero global
(`dword[0x64FEA8]`) que el cargador de nivel fija a `[desc+0x54]` en
`0x43B7DC`. Apunta a una tabla de `int16` de **3464 entradas** (6928 B,
0x1B10) que rellena `0x461895`, llamada desde `0x43B7E6`.

Campos del registro de niebla del nivel:

| Campo | Uso |
|---|---|
| `+0x40/+0x41/+0x42` | color de niebla R/G/B → `0x6468A0`/`0x647574`/`0x647560` |
| `+0x44/+0x45/+0x46` | segundo color (paleta Glide `0x64FB48`/`0x65BF48`) |
| `+0x4C` | profundidad A (`int16`) |
| `+0x4E` | profundidad B (`int16`) |
| `+0x50` | override de umbral (`dword`) |
| `+0x54` | puntero a la tabla de niebla |

Generación (`0x461895`, 111177-111293) con `x = +0x4C`, `y = +0x4E`:

```
si x == y: no se rellena nada
B   = (x << 12) idiv (x - y)
A   = -((x * y) idiv (x - y))                 ; idiv con signo
lim = ([0x551644]->+0x8 ->+0x40) * 0x7FFF sar 8
A   = clamp(A, -lim, lim)
tabla[0] = 0
para i = 1; i < 0x3620; i += 4:
    f = B + (A << 12) idiv i
    tabla[i >> 2] = (int16)clamp(f, 0, 0x1000)
```

- Índice válido `0..3463`, es decir `z < 0x3620` (13856). La ruta de emisión
  **no comprueba** ese límite: con `z` entre `0x3620` y `0xFFFF` el original lee
  fuera de la tabla. El puerto debe acotar el índice, no reproducir el defecto.
- La función equivale a `factor(+0x4C) = 0x1000` y `factor(+0x4E) = 0`: niebla
  **lineal en 1/z** entre ambas profundidades. `+0x4C` es la profundidad de
  niebla total y `+0x4E` la de niebla nula.
- `[0x647564]` (§5.5) es `A` antes del `<<12` y `[0x647570]` es `B`: el cálculo
  analítico y la tabla son la misma función, evaluada por dos caminos.

Umbrales (111244-111259):

```
0x65BF80 = (-(A << 12)) idiv B          ; equivale a y = +0x4E
0x65BF04 = ((int16)+0x50 != -1) ? 0x65BF80 : dword[+0x50]
```

`0x65BF80` es la profundidad a la que la niebla deja de ser nula; por encima no
se consulta la tabla. `0x65BF04` separa el emisor con pasada de niebla
(`0x426588`) del pre-coloreado (`0x426256`).

> **Corrección:** la ronda anterior atribuía estos umbrales a `0x4611ED`.
> `0x4611ED` (110672-111176) es una rutina de **selección de LOD / dirección de
> portal**: compara dos puntos `+0x48..+0x4C` y `+0x18..+0x1C`, ordena las
> diferencias absolutas, calcula `(30*|a| + 12*|b| + 9*|c|) / 8`, normaliza el
> vector a 1/16 (Q14) y elige el subnodo por distancia al plano `+0x1E..+0x24`,
> escribiendo `+0xA0` (nodo), `+0xA4..+0xA8` (dirección) y `+0xAA` (factor).
> No toca la niebla ni la proyección.

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

- 16.ª (2026-11-10): reconstrucción **línea a línea** de la proyección y de la
  niebla. Se documentan la estructura y construcción de la matriz de emisión
  `cam+0xE0` (§5.14), la aritmética exacta de `0x46A1D0` (§5.5), el
  determinante 2D de `0x46B6B5`, la LUT de niebla `0x64FEA8` y su generador
  `0x461895` (§5.15), los umbrales `0x65BF80`/`0x65BF04` y el reparto de color
  entre `0x426256` y `0x426588` (§5.7). Se corrigen tres afirmaciones previas:
  (a) el determinante es el convencional, no su negación; (b) `0x426588` no usa
  blanco en su registro principal, solo en la pasada de niebla; (c) los
  umbrales los fija `0x461895`, no `0x4611ED` (que es selección de LOD). Se
  identifica el segundo registro de 0x24 B como pasada de niebla y se caracteriza
  `0x40172B`/`0x4013A0` como *flush* de geometría de modelos. Solo
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
- La matriz de emisión `cam+0xE0` está reconstruida pero **no existe en el
  puerto**: `port/include/gex/camera.h` solo modela `+0xE4`/`+0xE8`. No debe
  usarse `gex_camera.m` como sustituto (son matrices distintas, §5.14).
- Sin D3D11, shaders, texturas.
- `PROJECT_STATUS.svg` sin cambios (MAIN sigue en 40 %; S-2 no completo).

---

**No se declara S-2 completo**: la visibilidad original está implementada y
documentada y la emisión está reconstruida, pero falta la emisión en código y la
validación dinámica.
