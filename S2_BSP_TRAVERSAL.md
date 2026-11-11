# S-2 — Recorrido BSP de AZTEC16

**Objeto:** recorrido estructural del BSP de niveles (S-2, primer escenario original), con visibilidad original de cámara y emisión de polígonos implementadas, y **validadas dinámicamente contra el ejecutable original** (2026-11-11), sin buffers D3D11, shaders ni texturas.

**Referencias:** `S1_DFX_PARSER.md`, `U1_CONSOLIDACION.md`, `U1H_RESULTADOS.md`, `S2_CAMERA_VISIBILITY.md`, `docs/D3D11_FIRST_SCENE_PLAN.md`, `U1_DFX_RELOCATION_AUDIT.md`.

**Estado actual:** recorrido estructural, visibilidad de cámara y **emisión de
polígonos** implementados en `port/` y validados **dinámicamente contra el
ejecutable original** el 2026-11-11: la cadena de cámara reproduce bit a bit M,
A, B, `a_cam` y `b_cam` del juego (§14 de
[`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md)) y el recorrido reproduce el
**conteo y la lista completa de hojas visibles** en 18 frames consecutivos del
nivel `map7.dfx` (§9). La emisión se documenta en §5.

Lo que **queda abierto** es la comparación cuantitativa de la geometría emitida
(polígonos y triángulos por frame) con el ejecutable: el original dibuja ~1517
polígonos por frame y el port produce un recuento distinto porque su contexto de
emisión (`gex_emit_matrix`) es una reconstrucción estática del de original, no
una captura. Por eso **S-2 no se declara completo**.

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
- **Emisión (2026-11-11):** implementada en `include/gex/emit.h` +
  `src/emit/emit.c` (`gex_level_emit`, §5), con **38 855 comprobaciones** en
  `tests/test_level_emit.c` y consumida por el Módulo 3
  (`include/gex/geometry.h` + `src/geometry/geometry.c`,
  `tests/test_geometry.c`, 6 963 comprobaciones) y por los buffers D3D11
  (`include/gex/render/d3d11_mesh.h`, `tests/test_d3d11_mesh.c`, 108
  comprobaciones). Sigue **pendiente la validación dinámica contra el ejecutable
  original**: S-2 no se declara completo.
- **Validación dinámica (2026-11-11, §9):** la cadena de cámara del port se
  validó contra el proceso original y reproduce **bit a bit** M, A, B, a_cam y
  b_cam (§14 de `S2_CAMERA_VISIBILITY.md`). El recorrido BSP con visibilidad
  reproduce **el conteo y la lista completa de hojas visibles** del original en
  **18/18 frames** consecutivos. La discrepancia que se había medido (307
  frente a 0) tenía su origen en que se comparaban **niveles distintos**: el
  port recorría `AZTEC16.DFX` y el juego `map7.dfx` (§9.1). **S-2 sigue
  abierta** solo por la comparación de la emisión (§9.5).

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
`0x00426588` están implementados como módulo puro en `port/src/emit/`
(2026-11-10, 17.ª–18.ª pasadas): sin D3D11, sin Win32 y sin Glide. Su ruta está
reconstruida con evidencia en §5 y validada con pruebas estáticas. La
**validación dinámica contra el ejecutable** (recorrido y visibilidad) se
describe en §9; la de la emisión (conteos de geometría) **sigue pendiente**.

## 5. Ruta de emisión de polígonos — reconstruida e implementada (2026-11-10)

**Alcance:** reconstrucción estática (solo lectura del desensamblado) de cómo las
hojas visibles se convierten en registros de comando y llegan al consumidor
Glide, y su implementación como módulo puro `gex_level_emit` en
`port/src/emit/` (C11, sin D3D11, sin Win32, sin Glide). Esta sección documenta
el comportamiento original y el contrato del módulo; el detalle de la API está
en `docs/D3D11_FIRST_SCENE_PLAN.md` §11.

Marcas: **[A]** demostrado en el desensamblado; **[B]** parcialmente demostrado
(estructura sí, semántica no); **[C]** hipótesis o decisión de seguridad del
port.

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
3. Proyecta y emite **cuatro** subtriángulos principales con `0x426256` (nunca
   con `0x426588`) y **sin niebla**, en el orden exacto
   `(v0,m01,m20)`, `(m20,m12,v2)`, `(m01,m12,m20)`, `(m01,v1,m12)`. Si alguno
   devuelve 0 (gate del emisor) se **aborta la subdivisión** conservando los ya
   emitidos. **[A]**
4. Si `(color0 >> 24) & 2 == 0`, intenta además tres *slivers* degenerados en el
   orden `(v0,v1,m01)`, `(m12,v1,v2)`, `(v0,m20,v2)`; sus retornos se ignoran y
   no abortan la subdivisión. En aritmética exacta son colineales (det = 0), pero
   el clamp de x/y a `[-0x400,0x3FF]` y el truncamiento entero pueden romper la
   colinealidad y hacer que alguno pase el gate. **[A]** para el orden y la
   condición; **[B]** para la frecuencia con que pasan.

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
   `D = (x1-x0)(y2-y0) - (x2-x0)(y1-y0)` sobre las coordenadas empaquetadas
   `(y<<16)|x` y lo guarda en `0x6468E0` (121628-121656). **Si `D >= 0`,
   descartan el triángulo**: el *back-face culling* es 2D, posterior a la
   proyección. (La 18.ª pasada corrige el orden de factores que figuraba aquí:
   el desensamblado resta `(x2-x0)(y1-y0)` a `(x1-x0)(y2-y0)`.)
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

Requisitos derivados de la evidencia anterior. **Estado (2026-11-11):** los ocho
puntos están cubiertos por `gex_level_emit` (`port/src/emit/emit.c`) y su
consumidor `gex_geometry_build` (`port/src/geometry/geometry.c`), salvo el
*depth buffer* y el `CullMode`, que pertenecen al Módulo 5. Se conserva el
enunciado porque fija qué debe transportar la emisión y por qué.

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

**Qué hace el consumidor con estos requisitos (Módulo 3, 2026-11-11):** la
transformación, el *culling* 2D, la subdivisión, el color por vértice, la niebla
y el orden ya los resuelve `gex_level_emit`; el generador de geometría no
recalcula nada de eso. `gex_geometry_build` transporta **tal cual** `x/y/z`,
`color`, `u/v`, `texture`, `layer` y `flags` de cada esquina a un `gex_vertex`
de 28 B, deduplica por igualdad exacta de esos 28 B y emite un índice por
esquina en el orden de emisión (que es el orden de dibujo decidido en §5.9). Los
triángulos marcados `GEX_EMIT_F_DEPTH_CLIPPED` —cuyas `x`/`y` valen 0 por
contrato— se descartan y se cuentan, en vez de dibujarse en el origen de la
pantalla. `gex_d3d11_mesh_create` convierte esa geometría en dos
`ID3D11Buffer` inmutables y elige `R16_UINT` o `R32_UINT` según el conteo de
vértices.

Lo que la emisión **no** entrega y el Módulo 3 tampoco inventa: el
`D3D11_INPUT_ELEMENT_DESC`/`ID3D11InputLayout` (necesita la firma del shader), la
matriz de proyección a espacio de clip para el *depth buffer* y el modo de
mezcla. Esos tres son Módulo 5.

Consecuencia práctica medida en AZTEC16 con la cámara sintética de prueba:
2 242 triángulos emitidos (1 121 de ellos con pasada de niebla) → 6 726 índices
y 3 918 vértices tras deduplicar. La deduplicación no fusiona nada que difiera
en color, UV, textura, capa o flags, así que una pasada de niebla nunca comparte
vértices con el registro principal del mismo triángulo.

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

**Implementación en `port/`:** el módulo `port/src/emit/emit.c` (cabecera
`port/include/gex/emit.h`) implementa `gex_level_emit` y sus fases separadas
(`gex_emit_collect`, `gex_emit_project`, `gex_emit_classify`,
`gex_emit_subdivide`, `gex_emit_sort`) reproduciendo esta ruta estáticamente
(18.ª pasada, 2026-11-11). Puntos verificados en el desensamblado:
- La **subdivisión tiene prioridad absoluta**: `0x426D09` compara `maxz` con
  `0x6A4` y salta a `0x426E71` **antes** de `det2d`, niebla, emisor y gate de
  pantalla. Por eso `gex_emit_classify` marca `subdivide` sin consultar el gate.
- El gate de `0x426256`/`0x426588` completo: `[0x647578] & 0x60000`, vértice
  clipeado por profundidad (`0x40000`), `det2d >= 0`, bucket fuera de
  `[0, 0xC00)`, `poly+0x06 & 0x8000` y descarte si **todos** los x/y caen al
  mismo lado de `0`, `0x200` o `0x180`.
- La subdivisión emite cuatro principales en el orden del original más hasta
  tres *slivers* si `(color0 >> 24) & 2 == 0`; **no aplica niebla** y usa
  siempre el emisor cercano. Un principal rechazado aborta la subdivisión.
- `0x426588` emite el registro principal de 0x34 B con los colores del llamador
  y después el registro de niebla de 0x24 B (base `0x00FFFFFF`, opcode `0x32`,
  sin UV); la inserción LIFO deja la niebla dibujada primero.
- `layer` es el byte `uv+0x0B` con signo (`movsx`), y la clave es
  `(maxz sar 2) + 4*layer`, descendente, con desempate **solo** por secuencia
  de inserción (LIFO). El `avgz` de `0x646C00`/`0x6474A4` no lo usan los
  emisores de nivel.
- Toda la aritmética de 32 bits reproduce el wrap de x86 sin desbordamiento con
  signo ni desplazamientos de implementación definida: helpers `add_wrap`,
  `sub_wrap`, `mul_wrap`, `neg_wrap`, `sar32` e `idiv32` (protege `b == 0` y
  `b == -1`), y copias de bits para `int32 <-> uint32`.

Diferencias de seguridad/port respecto al original:
- El índice de la LUT de niebla `z >> 2` se acota a `[0, 3463]` (el original no
  lo acota y puede leer fuera de tabla para `z >= 0x3620`) **[C]**.
- La capacidad del buffer de salida se reserva dinámicamente por triángulo
  emitido; el desbordamiento silencioso del original se sustituye por
  `GEX_EMIT_ERR_OVERFLOW` **[C]**.
- Los vértices se conservan como `int32` de profundidad y `int16` de píxeles
  recortados a `[-0x400,0x3FF]` como el original (sin el empaquetado de 10 bits
  del consumidor) **[C]**.
- Si `z' == 0` el original conserva los globales del polígono anterior; el port
  fija x/y a 0 y marca `GEX_EMIT_F_DEPTH_CLIPPED` (con `near_z > 0` el caso ya
  está clipeado) **[C]**.
- Todas las lecturas de la imagen DFX se comprueban contra `image_size` con
  sumas en `size_t` (un offset `0xFFFFFFFF` nunca envuelve a un valor pequeño)
  **[C]**.
- No se implementa la ruta de modelos (`0x4013A0`, traslación `sar 4`); queda
  fuera de este módulo.

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

- 17.ª (2026-11-10): implementación de la emisión en `port/` (`emit.h`,
  `emit.c`, `test_level_emit.c`, `Makefile`/`build.ps1`). Primer contrato de la
  API y pruebas iniciales; el `test_level_emit` de esta pasada tenía una
  expectativa de subdivisión incorrecta (asumía niebla y no cubría el orden
  real). Cifra inicial: 627 comprobaciones.

- 18.ª (2026-11-11): **auditoría y corrección de `emit.c`**. Se corrigen seis
  defectos: (1) `xs`/`ys` sin inicializar al descartar por profundidad; (2)
  aritmética de 32 bits con posible desbordamiento con signo y desplazamientos
  de implementación definida; (3) offsets y sumas que podían envolver en
  `gex_emit_collect`; (4) `test_mix_color` no probaba la mezcla real; (5)
  subdivisión, interpolación, emisores, niebla y orden LIFO asumidos sin
  verificar; (6) AZTEC16 se usaba como validación visual con matriz identidad.
  Se reconstruyen la prioridad de la subdivisión, los cuatro principales y los
  tres *slivers*, el gate completo, los dos registros de `0x426588`, la capa
  `int8` y la clave `(maxz >> 2) + 4*layer`. `test_level_emit` pasa a **38 855
  comprobaciones**, 0 fallos, limpio en Clang `-O0`/`-O2` y
  ASan/UBSan; AZTEC16 se valida estructuralmente (815 hojas, 9609 polígonos,
  1121 triángulos cercanos y 2242 lejanos —1121 con pasada de niebla— con una
  cámara sintética, y determinismo bit a bit). **S-2 no se declara completo**:
  falta la validación dinámica contra el ejecutable.

Cifras históricas (7, 18, 23, 44 comprobaciones) corresponden a estados
intermedios y no son el resultado vigente. El resultado vigente es **37
comprobaciones de BSP + 889 de cámara + 38 855 de emisión**, 0 fallos.

## 7. Limitaciones

- Visibilidad y emisión reconstruidas e implementadas, pero **sin validación
  dinámica** contra el ejecutable original: no hay traza de cámaras concretas ni
  comparación de hojas visibles o de registros emitidos. Las tablas
  trigonométricas son una aproximación matemática (equivalencia bit a bit no
  demostrada).
- La emisión está implementada como módulo puro (§5) con pruebas estáticas; no
  se ha comparado contra una captura de registros de comando del ejecutable.
  AZTEC16 se valida de forma **estructural y de determinismo**, no visual.
- La matriz de emisión `cam+0xE0` está modelada en `gex_emit_matrix`
  (`port/include/gex/emit.h`), **separada** de `gex_camera.m` (`cam+0xE8`,
  visibilidad). No debe usarse `gex_camera.m` como sustituto (son matrices
  distintas, §5.14).
- **Geometría y buffers (Módulo 3, 2026-11-11):** la conversión a vértices e
  índices y la creación de vertex/index buffers D3D11 están implementadas y
  probadas, pero **no hay renderizado visible**: faltan shaders,
  `ID3D11InputLayout`, texturas y `DrawIndexed` (Módulos 4 y 5).
- `PROJECT_STATUS.svg` sin cambios (MAIN sigue en 40 %; S-2 no completo).

---

## 8. Validación dinámica contra el ejecutable original (2026-11-11)

### 8.1 Qué se pudo observar en el proceso vivo

Con el original en ejecución (copia de trabajo aislada, AZTEC16 forzado como
`LEVEL\MAP7.DFX`, guion de teclas hasta la ruta 3D) y puntos de
interrupción de **hardware**:

| Dirección | Qué ocurre | Frecuencia observada |
|---|---|---|
| `0x401C00` | recorrido BSP (`desc`, `cam`, `out`) | 1 por frame (145 en 145 frames) |
| `0x4269C2` | emisión (`hojas`, `desc`, `cam`) | 1 por frame |
| `0x46558A` | dibujado de polígono | 219 998 en 145 frames → **~1517 por frame** |

El buffer de salida del recorrido (`out[0]` = contador de hojas visibles, dword)
da **307**, **306** y **303** hojas visibles en los tres primeros frames
medidos.

El nodo que el original somete a la prueba de esfera tiene, en su primer
frame: centro `(−5818, −2134, 2124)` en `+0x00`, radio `1100` en `+0x06`,
normal `(−2034, −3034, 536)` en `+0x0C` y tipo `1` en `+0x12` — es decir, la
**misma disposición** que documenta el §2 de este archivo y que usa el port.

### 8.2 Resultado de la comparación

| Magnitud | Original | Port | Estado |
|---|---|---|---|
| Cadena de cámara (M, A, B, a_cam, b_cam) | valores capturados | **idénticos bit a bit** | ✅ validado |
| Fórmula de prueba de hoja | `d0 <= −r` / `d0 >= far + r` | igual | ✅ validado |
| Estructura del nodo BSP | centro/radio/normal/tipo | igual | ✅ validado |
| Total de hojas del nivel | — | 815 (predecesor NULL) | ⚠️ sin contraparte en el original |
| **Hojas visibles con la cámara capturada** | **307** | **0** | ❌ **diferencia real** |
| Polígonos dibujados por frame | ~1517 | — | ⚠️ no comparable aún |

### 8.3 Diagnóstico de la diferencia

Como la cadena de cámara ya es **equivalente bit a bit** (§14 de
`S2_CAMERA_VISIBILITY.md`) y la fórmula de visibilidad coincide con el
desensamblado, la diferencia está en el **espacio de datos del nivel**: la
secuencia de bytes del nodo raíz que el original prueba
(`26 EE 8A F7 8A F7 4C 08 04 4C 04 61 40 16 92 16`, es decir centro
`(−5818,−2134,2124)` y radio `1100`) **no aparece en ninguna** de las
imágenes decodificadas por el port:

- `AZTEC16.DFX` decodificado por `port/src/asset/dfx.c` (479 050 bytes),
- `MAP7.DFX` = copia de `AZTEC16.DFX` (misma imagen),
- `MAP7.DFX` de la copia `SMOKE_H4A` (926 400 bytes).

En cambio, el port sí recorre **815** hojas con centros plausibles
(`y ≈ 16 000`), mientras que la cámara capturada está en `y = 4071`: el
original y el port **no están mirando el mismo espacio de coordenadas**.

### 8.4 Montaje reproducible (todo fuera de `C:\GEX_REZ`)

```powershell
# 1) Copia de trabajo aislada del juego (ejecutable original + glide2x real)
$dst = "$env:TEMP\gexrun"
Copy-Item "C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE" $dst
Copy-Item "C:\GEX_TEST\F03\glide2x_gex_real.dll" "$dst\glide2x.dll"
Copy-Item "C:\GEX_REZ\E2_A_ORIGINAL\LEVEL" $dst -Recurse

# 2) AZTEC16 como nivel 3D: el original carga <base>\level\map7.dfx
#    (cadena en 0x551900, sprintf en 0x463403). InstallDir del registro:
#    HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00
Copy-Item "C:\GEX_REZ\E2_A_ORIGINAL\LEVEL\AZTEC16.DFX" "C:\GEX_TEST\F03\LEVEL\MAP7.DFX" -Force
Copy-Item "C:\GEX_REZ\E2_A_ORIGINAL\LEVEL\AZTEC16.VFX" "C:\GEX_TEST\F03\LEVEL\MAP7.VFX" -Force

# 3) Trazador: breakpoints de hardware, volcado de memoria, guion de teclas
#    hasta la ruta 3D (el original arranca por menus; sin CD no entra solo)
```

Los dos `MAP7.*` son **copias de trabajo** y se pueden borrar cuando ya no
se necesite repetir la medición; `C:\GEX_REZ` nunca se escribe.

### 8.5 Consecuencia para el estado del proyecto

La emisión del port (`gex_level_emit`) opera sobre el espacio de datos del
port; como ese espacio **no** está demostrado equivalente al del original, el
contrato de emisión de §5 y el §11 de
`docs/D3D11_FIRST_SCENE_PLAN.md` **no se dan por validados**. S-2 permanece
abierta. La vía de cierre es determinar qué transformación aplica el original
al contenedor `.DFX` al cargarlo (reubicación de punteros, origen del nivel u
otro chunk) y reproducirla en `port/src/asset/dfx.c`.

## 9. Validación dinámica contra el ejecutable original (2026-11-11) — el nivel real

### 9.1 El nivel que carga el juego: `map7.dfx`

La primera observación decisiva: **el juego no carga `AZTEC16.DFX`**. El
original construye la ruta del nivel con un `sprintf` en `0x00463403` sobre la
cadena de `0x00551900`, y la ruta resultante se capturó en vivo con el
trazador (`dumpstr` sobre el argumento de ruta del cargador `0x00461E0B`):

```text
STR  base=0x001AFA9C   C:\GEX_REZ\GEX2\gex.dfx
STR  base=0x0064D5A8   C:\GEX_REZ\GEX2\level\map7.dfx
```

Es decir: el juego usa **`C:\GEX_REZ\GEX2\level\map7.dfx`**, que pertenece a
la instalación original y **no** al `InstallDir` del registro. Cualquier
comparación，debe usar ese fichero. La primera medición «307 hojas frente a 0»
era, por tanto, una comparación entre **niveles distintos** (`AZTEC16.DFX` en el
port frente a `map7.dfx` en el juego), no un defecto del recorrido.

### 9.2 El cargador y el buffer `B`

`0x00461E0B` (argumento: la ruta) lee el fichero entero, calcula
`aligned = ((count + 0x200) >> 9) << 9`, llama al relocalizador
`0x004735C0` (que suma `B` a los DWORD enumerados en la tabla de offsets) y
compacta la imagen desde `B + 4*aligned` hasta `B`. Devuelve `B` en `EAX`
(observable en `0x00461EC9`).

Medido en la sesión de referencia:

| Dato | Valor observado |
|---|---|
| `B` (base del buffer del nivel) | `0x1940CF4C` |
| descriptor del nivel (`desc`) | `B + 0xDDE8` |
| `desc[0]` (raíz del BSP) | `B + 0x9CE94` |
| `root_bsp` del port (`gex_dfx_load_level`) | `0x9CE94` ✔ **coincide** |

El puntero de nodo que el original entrega al recorrido es, por tanto, el
mismo offset de imagen que usa el port: el parser del contenedor y la
extracción del descriptor del port son correctos.

### 9.3 El nodo raíz, leído de una vez

48 bytes contiguos leídos del nodo raíz real (`dumpindraw`), decodificados:

```text
+0x00  centro   (-5818, -2134, 2124)   (int16 x3)
+0x06  radio    19460                   (uint16)
+0x0C  normal   (-2034, -3034, 536)    (int16 x3, Q12)
+0x12  tipo     1                       (int16)
+0x18  punteros a hijos, ya RELOCALIZADOS (punteros absolutos)
```

Los punteros a hijos son la diferencia de representación entre el original y
el port: el original relocaliza los DWORD del `.DFX` en memoria (`v += B`), de
modo que sus hojas son **punteros absolutos**; el port trabaja con **offsets**
dentro de la imagen, que es el mismo espacio. La estructura del nodo
(centro, radio, normal, tipo) coincide con la documentada en §2.

### 9.4 La lista de hojas: coincidencia frame a frame

El original deja en su buffer de salida el contador en `out[0]` y los punteros
a las hojas visibles a partir de `out[4]`. Se capturaron **18 frames
consecutivos** de la ruta 3D (cámara leída del objeto de cámara `0x0065CD80` en
la entrada del recorrido, lista leída en la entrada del frame siguiente).

Con esos datos como referencia, el port reproduce:

* el **conteo** de hojas visibles (307, 306, 303, 301, 303, 302, 302, 307, 310,
  305, 292, 291, 305, 308, 310, 309, 297, 295), y
* la **lista completa de hojas, en el mismo orden**, elemento a elemento
  (18/18 frames).

La regresión vive en `port/tests/test_camera.c` (cámara) y
`port/tests/test_bsp.c` + `port/tests/ref_map7_frames.h` (recorrido).

### 9.5 Lo que sigue abierto

* **La emisión de geometría no se ha comparado cuantitativamente.** El original
  dibuja ~1517 polígonos por frame (breakpoint en `0x0046558A`: 219 998 draws
  en 145 frames). El port emite sobre su propio contexto
  (`gex_emit_matrix`), que es una reconstrucción **estática** del del original
  (§5 y §11 del plan D3D11); para comparar conteos haría falta capturar el
  contexto de emisión real del juego (`0x0065CD60` y la matriz escalada de
  `0x46A1D0`) y construirlo desde ahí. Es el siguiente paso natural.
* La rama de **ángulos alternativos** (`[0x551644+0xa0] & 8`) y la de
  **base forzada a identidad** (`cam+0x43C != 0`) no se han observado en
  ejecución: en el juego capturado el flag valía 0 y la base era la identidad.
* La **tabla de rutas del VFX** y la mezcla de color por vértice siguen
  pendiente de la comparación numérica con el original.


---

**No se declara S-2 completo**: la visibilidad original, el recorrido y la
emisión están implementados y documentados, y el recorrido y la visibilidad
**están validados dinámicamente** contra el ejecutable (§9: 18/18 frames con
la lista de hojas idéntica). Queda pendiente la **comparación cuantitativa de
la emisión** (§9.5). La integración con el renderizador ya tiene su primera
mitad (Módulo 3: geometría y buffers); la segunda (shaders, texturas y dibujo)
sigue pendiente.
