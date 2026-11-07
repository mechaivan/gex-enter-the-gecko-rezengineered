# RESULTADOS DE U-1g — RELACIÓN ENTRE LA RUTA 3D Y LOS RECURSOS DFX/VFX (2026-11-06)

**Binario analizado:** `C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE`
(PE32 i386, 1 557 504 B, EU v1.00.000; `imageBase = 0x00400000`).

**Assets contrastados (solo lectura):** `GEX.DFX` (146 008 B) y `GEX.VFX`
(1 588 B) de `C:\GEX_REZ\E2_A_ORIGINAL`.

**Modo:** análisis estático read-only. No se ha ejecutado el juego, no se ha
abierto ventana alguna, no se ha modificado el ejecutable ni ningún fichero de
`C:\GEX_REZ`, y no se ha instalado ni configurado ninguna herramienta.

> **⚠ NOTA DE CORRECCIÓN (U-1h, 2026-11-06; revisada 2026-11-07).** La tabla
> opcode→handler de §2.5 estaba **desplazada una fila**; la correcta está en
> §2.5 y en [U1H_RESULTADOS.md §8.1](U1H_RESULTADOS.md). `0x0041A203` es
> **colisión** segmento–BSP: U-1g §7.3 **no** lo clasificó como dibujo, solo lo
> dejó como hipótesis de recorrido sin algoritmo. `0x00425010` es un **emisor de
> modelo/objeto** (no del nivel). Las cuestiones §11.1–§11.7 quedaron resueltas
> estáticamente en U-1h. Este documento conserva el texto publicado, con las
> correcciones marcadas en línea y el detalle en `U1H_RESULTADOS.md` (§18,
> auditoría final del 2026-11-07).

> **Consolidación final (2026-11-07).** El estado consolidado de U-1, la
> separación de ámbitos y los pendientes están en
> [`U1_CONSOLIDACION.md`](U1_CONSOLIDACION.md).

---

## 0. Resumen ejecutivo

U-1g tenía dos objetivos: descifrar el **flujo de comandos** que consume
`0x0046558A` y establecer la **relación real** entre ese flujo, el `.DFX` y el
`.VFX`. Ambos quedan resueltos en lo esencial:

1. **`0x0046558A` no recorre una lista**: dibuja **un solo comando** por llamada.
   El recorrido lo hace `0x00469836`, que sigue una **lista enlazada** de
   registros.
2. La lista se construye **cada frame** en un pool de dos buffers de `0x6000` B
   (`0x64FF00 + 0x6000 * level[+0x14]`), inicializado por `0x00465530`. El head
   es `buf + 0x5FF8`.
3. Cada registro tiene **cabecera de 8 bytes**: `[0]` = longitud en bytes o flag
   de dibujo (0 = omitir), `[4]` = puntero al siguiente (`-1` = fin). El
   **opcode** está en `byte[+0xB]`; el byte `0xE1` es un **prefijo** que ocupa
   4 bytes y fija `[0x57B8EC]` antes del comando real.
4. Los vértices van **inline** (sin buffer aparte). El campo de **14 bits**
   `word[+0x1A] & 0x3FFF` es un **índice a la tabla `0x006488F4`**, construida
   por `0x00461C32` a partir del **`.VFX`**; el registro apuntado es un
   `GrTexInfo` extendido que se pasa a `grTexDownloadTable`, `grTexSource` y
   `grTexDownloadMipMap`.
5. El **`.DFX`** no contiene comandos ni texturas: es un **blob relocalizable**
   con cabecera `{count, offsets[count]}` y una imagen con punteros absolutos.
   `0x004735C0` suma la base a cada dword apuntado por `offsets[i]`; después
   `0x00461E0B` compacta la imagen sobre el hueco de la tabla y devuelve el
   buffer, que `0x0043B11D` guarda en `level[+4]`.
6. **La relación DFX↔VFX no está en los ficheros**: es una relación de runtime.
   El `.VFX` se carga **primero** (asigna direcciones de textura y llena
   `0x6488F4`); el `.DFX` se carga **después** (grafo de escena y geometría).
   El puente es el emisor de comandos (`0x00425010`, `0x004FF06F`), que traduce
   caras del `.DFX` a registros con índices de textura del `.VFX`.

Los dos formatos se validaron contra los assets reales: el `.DFX` encaja
exactamente (322 offsets, imagen de 143 960 B, raíz en `0x2E0`, 90 punteros en
los primeros 512 B) y el `.VFX` termina exactamente en `0x634` con 4 registros
de `0x8C + 256` B.

**Resultado negativo principal:** no existe ninguna referencia del `.DFX` al
`.VFX` ni viceversa; no hay tabla de texturas dentro del `.DFX`, ni geometría
dentro del `.VFX`, ni un identificador de recurso por comando distinto del
índice de 14 bits.

---

## 1. Metodología y herramientas

| Elemento | Detalle |
|---|---|
| Desensamblador | `llvm-objdump` (Intel syntax), `--start-address/--stop-address` |
| Lector PE | `tools/gexmap/pe.mjs` para VA↔RVA↔offset y tabla de imports |
| Referencias cruzadas | `analysis/EU/functions.tsv`, `analysis/EU/calls.tsv` |
| REA | **no viable**: `rea doctor` → `healthy: false`, `environment_healthy: false`, sin Hopper ni Ghidra (`missing_analysis_engine`). No se instaló nada. |
| Validación de assets | lectura binaria directa de `GEX.DFX` / `GEX.VFX` (solo lectura) |
| Ejecución | ninguna. Sin breakpoints, sin ventanas, sin emulador |

Los artefactos de `analysis/EU` se usaron solo como índice; **todas las
afirmaciones de instrucción se verificaron sobre el desensamblado real**.

Ficheros temporales (fuera del repo, no versionados):

* `%TEMP%\opencode\gex_full.asm` — desensamblado completo
* `%TEMP%\opencode\u1g_46558a.txt` — `0x0046558A` completo (195 510 B)
* `%TEMP%\opencode\u1g_461b90.txt`, `u1g_43b11d.txt`, `u1g_4735c0.txt`
* `%TEMP%\opencode\u1g_425010.txt`, `u1g_4ff06f.txt`, `u1g_461fbd.txt`
* `%TEMP%\opencode\u1g_assets.mjs` — validador de `GEX.DFX` / `GEX.VFX`

---

## 2. Encuadre del flujo de comandos

### 2.1 Pool, doble buffer y cabecera de lista

`0x00461FBD` (constructor del frame) hace:

| Dirección | Acción |
|---|---|
| `0x462024..0x462032` | `level[+0x14] = 1 - level[+0x14]` (alterna buffer) |
| `0x462069` | `buf = [0x64FEE0 + 4*level[+0x14]]` |
| `0x4620f4` | `0x00465530(buf, 0xC00)` — inicializa la free-list |
| `0x4622b8..0x4622c2` | `0x00469836(buf + 0x5FF8)` — dibuja la lista |

Los dos punteros de buffer son `[0x64FEE0]` y `[0x64FEE4]`; el pool físico es
`0x64FF00 + 0x6000 * level[+0x14]` (confirmado de forma independiente en
`0x004FF0B1..0x004FF0C5`).

`0x00465530(buf, count)` recorre `count = 0xC00` nodos de 8 B de mayor a menor:

```
para node = buf + 8*count - 8 hasta buf + 8:
    node[4] = node - 8
    node[0] = 0
buf[4] = -1
buf[0] = 0
```

Es decir, una **pila de nodos libres** de `0xC00` entradas. El head que se
dibuja es el nodo más alto, `buf + 8*0xBFF = buf + 0x5FF8`.

Antes de llamar al consumidor, `0x00461FBD` inserta un **registro estático de
cierre** en `0x64FB40`:

```
0x462296  dword[0x64FB40] = 0x14        ; longitud
0x46228c  dword[0x64FB44] = next        ; siguiente (antiguo [buf+0x5FFC])
0x46227a  word [0x64FB4E] = level*0x180 ; +0x0E
0x462262  word [0x64FB52] = 0xE0        ; +0x12
0x4622ab  dword[buf + 0x5FFC] = 0x64FB40
```

Su opcode (`0x64FB4B`) **no se escribe** y vale 0 en la imagen; como el
dispatcher descarta cualquier `cmd - 0x20 > 0xD0` (§2.4), el registro es un
**no-op** con dos campos de 16 bits. Su función exacta (marcador de fin,
sincronización o relleno) **no está establecida**.

> **Corregido en U-1h §10 (auditoría del 2026-11-07):** el opcode `0xF0` **sí se
> escribe**, en la inicialización `0x4635BD`; la longitud inicial es `4` y el
> reencadenado la fija en `0x14`. El registro **no es un no-op**: es el
> rectángulo 2D de fondo. Además, el reencadenado real es
> `0x462240..0x4622AB`; `+0x12` vale `0x180` (con `+0x10 = 0x200`) si
> `[0x551644][+0x6FB0] > 0`, y `0xE0` en caso contrario.

### 2.2 Formato de un registro

```
offset  contenido
+0x00   u32 longitud en bytes / flag de dibujo (0 = no dibujar)
+0x04   u32 puntero al siguiente registro (-1 = fin)
+0x08   payload
...     opcode en byte[+0x0B] (salvo prefijo 0xE1)
+0x0C   vértices inline (según opcode)
```

Hechos verificados:

* `[0]` es a la vez **longitud** (el emisor avanza `p += longitud`) y **flag de
  dibujo** (`0x00469836` solo llama si `[0] != 0`). Es una reutilización del
  mismo campo, no dos campos distintos.
* `[4]` es el enlace; `-1` termina.
* `byte[+0xB]` es el opcode en todos los caminos verificados
  (`0x4655AD`, `0x4655E0`, `0x469887`).

### 2.3 Consumidor `0x00469836`

```
0x469839  [0x64757C] = 0
0x469843  [0x64767C] = 0
0x46984D  [0x6475A0] = [0x6475E0] = [0x647620] = [0x647660] = 1.0f (0x3D800000)
0x469875  grTexNCCTable(0, 0)
bucle:
  if (p == -1) fin
  if (p[0] != 0) 0x0046558A(p)
  p = p[4]
```

No hay caché, ordenación ni agrupación: se dibuja en el orden de la lista.

### 2.4 Dispatcher `0x0046558A`

Tamaño exacto: **17 068 B** (`0x46558A..0x469836`). Es el único consumidor de
comandos 3D.

```
0x4655AD  opcode = byte[record + 0x0B]
0x4655B3  if (opcode == 0xE1):
0x4655BF      [0x57B8EC] = ((dword[record+8] & 0x9FF) >> 5) & 3
0x4655D5      record += 4
0x4655E0      opcode = byte[record + 0x0B]
0x4655E9  cmd   = opcode & 0xFFFFFFFC
0x4655F2  flag2 = opcode & 0x4
0x4655FB  flag1 = opcode & 0x2
0x465604  flag0 = opcode & 0x1
0x46560A  if (cmd == 0x4C) flag2 = 0
0x465617  if (cmd & 0x20) { máquina de estado de alpha/blend/color }
0x465A4C  idx = cmd - 0x20
0x465A64  if (idx > 0xD0) -> 0x469711 (ret)
0x465A7C  h = byte[0x469765 + idx]
0x465A82  jmp [4*h + 0x469735]
```

Detalles:

* Los **bits 0 y 1** del opcode son flags de estado; el **bit 2** distingue la
  variante (p. ej. `0x20` sólido frente a `0x24` texturizado) y también se pasa
  como flag.
* El prefijo `0xE1` **no es un comando**: desplaza el puntero 4 bytes y el
  comando real empieza ahí; su `byte[+0xB]` cae en `record+0xF`.
* La máquina de estado (`0x465617..0x465A4C`) llama a `grAlphaBlendFunction`,
  `grAlphaCombine`, `grChromakeyMode`, `grConstantColorValue4` y
  `guColorCombineFunction` según `cmd & 0x20`, `cmd & 0x10`, `cmd & 0x4` y el
  nivel de transparencia `[0x57B8EC]`.

### 2.5 Tabla de despacho

Dos tablas contiguas:

* `0x00469765`: **mapa de bytes** indexado por `cmd - 0x20` (rango
  `0x00..0xD0`), que devuelve un índice `0..11`.
* `0x00469735`: **tabla de 12 punteros** de handler.

| `cmd` | Handler | Llamadas Glide observadas |
|---|---|---|
| `0x20` | `0x00465A89` | `grConstantColorValue4`, `guColorCombineFunction`, `grDrawTriangle` |
| `0x24` | `0x00465E5A` | `grTexDownloadTable`, `grTexSource`, `grConstantColorValue4`, `guColorCombineFunction`, `grDrawTriangle` |
| `0x28` | `0x0046636C` | `grConstantColorValue4`, `guColorCombineFunction`, `grDrawPlanarPolygonVertexList` |
| `0x2C` | `0x00466B3E` | `grTexDownloadTable`, `grTexSource`, `grConstantColorValue4`, `guColorCombineFunction`, 2×`grDrawTriangle` (cuadrilátero) |
| `0x30` | `0x004673E9` | `guColorCombineFunction`, `grDrawTriangle` (sin `grConstantColorValue4`) |
| `0x34` | `0x0046789D` | `grTexSource`, `guColorCombineFunction`, `grDrawTriangle` |
| `0x38`, `0x3C`, `0x44`, `0x50`–`0xB0`, `0xB8`–`0xEC` | `0x00469711` | `ret` (no-op) |
| `0x40` | `0x00468512` | `grConstantColorValue4`, `guColorCombineFunction`, `grDrawLine` |
| `0x48` | `0x0046875B` | `grConstantColorValue4`, `guColorCombineFunction`, `grDrawLine` |
| `0x4C` | `0x00468B54` | `grConstantColorValue4`, `guColorCombineFunction`, `grDrawLine` (3 llamadas) |
| `0xB4` | `0x00467E65` | `grTexDownloadTable`, `grTexSource`, `guColorCombineFunction`, `grDrawTriangle` |
| `0xF0` | `0x00469268` | `grConstantColorValue4`, `guColorCombineFunction`, 2×`grDrawTriangle` |

> **Corregido en U-1h (§8.1):** la tabla publicada originalmente aquí estaba
> desplazada una fila (asignaba `0x20`/`0x24` a `0x465A89`, `0x30`/`0x34` a
> `0x465E5A`, etc.). La tabla de arriba ya es la correcta: 11 handlers activos,
> verificados leyendo el mapa `0x469765` y los 12 punteros de `0x469735`.

`0x00469715` y `0x00469725` son otras dos tablas de salto usadas por la máquina
de estado de alpha, **no** por el dispatcher de opcodes.

---

## 3. Referencia a recursos: el índice de 14 bits

### 3.1 Código exacto

Dentro del handler `0x24` (`0x00465E5A`):

> **Corregido en U-1h:** el texto original decía "handler `0x30`", herencia de
> la tabla desplazada de §2.5. El código de abajo pertenece al handler del
> comando `0x24` (triángulo texturizado); el handler real del `0x30` es
> `0x004673E9` y no accede a texturas.

```
0x466110  eax = record
0x466115  cx  = word[record + 0x1A]
0x466119  ecx = cx & 0x3FFF          ; índice de textura (14 bits)
0x46611F  edx = [0x6488F4]           ; tabla de punteros
0x466125  eax = [edx + 4*ecx]        ; registro de textura
0x466128  [ebp-0x30] = eax
0x46612B  if (eax == 0) -> ret
0x466136  if (record[+0x0C] == 1)
0x46613F      grTexDownloadTable(0, 0, record + 0x14)
0x46614F  grTexSource(0, record[+0x88], 3, record)
```

Los dos bits altos del mismo word (`>> 14`) son el **nivel LOD/mipmap**
seleccionado (coherente con los campos `smallLod`/`largeLod` de `GrTexInfo`).

Hecho adicional: el handler **descarta** el comando si el índice apunta a
`NULL` (`0x46612B..0x466131`), sin dibujar.

### 3.2 Registro de textura (`.VFX`) — campos usados

| Offset | Uso en el binario | Valor observado en `GEX.VFX` |
|---|---|---|
| `+0x00` | `log2` ancho (`GrTexInfo.largeLod`) | `4` |
| `+0x04` | `log2` alto (`smallLod`) | `4` |
| `+0x08` | aspect / flags (`aspectRatio`) | `3` |
| `+0x0C` | formato (`format`) | `1` en los 4 registros |
| `+0x10` | puntero a datos; `0x461C32` lo sobrescribe con `reg+0x8C` | `0x64FE04` en fichero |
| `+0x14` | tabla NCC/paleta (`grTexDownloadTable`) | `0x33261804`, etc. |
| `+0x84` | tamaño de píxeles en bytes | `256` |
| `+0x88` | dirección Glide; `0x461C32` la sobrescribe | `256` en fichero |
| `+0x8C` | inicio de los píxeles | primer byte de datos |

`record[+0x0C] == 1` es la condición para llamar a `grTexDownloadTable(0, 0,
record+0x14)`; el valor `1` es, por tanto, un **formato con tabla** (hipótesis:
`GR_TEXFMT_AP_88` o paleta, pendiente de los headers de Glide 2.x).

### 3.3 Construcción de la tabla `0x006488F4` (`0x00461C32`)

```
0x461C3C  p = 0x00461B90(path)          ; lee el fichero completo
0x461C4A  count = dword[p]
0x461C52  table = 0x00464FB3(count*4)
0x461C5E  [0x6488F4] = table
0x461C63  p += 4
para i = 0..count-1:
    table[i] = p
    record[+0x10] = record + 0x8C
    p = record + 0x8C + record[+0x84]
    need = grTexTextureMemRequired(3, record)
    ... control de hueco en [0x6487C4]/[0x6487CC]
    if (record[0] < 7 && record[+0x0C] == 1):
        byte[+0x8D] = byte[+0x8C]
        byte[+0x8E] = byte[+0x8C]
    record[+0x88] = [0x6487C4]
    grTexDownloadMipMap(0, [0x6487C4], 3, record)
    [0x6487C4] += need
```

Consecuencias demostradas:

* La tabla es un **array de punteros** de `4*count` B, asignado en el arena.
* El tamaño de cada registro se deduce del fichero: `0x8C + record[+0x84]`.
* La descarga a la tarjeta es **secuencial** y con alineación de 2 MB
  (`and 0xFFE00000`), igual que en la ruta de fuente de U-1f.

---

## 4. Emisores de comandos

### 4.1 `0x00425010` — caras de **modelo/objeto** a comandos

> **Corregido en U-1h (§7.3):** `0x00425010` y `0x004258BD` son emisores de
> **modelo/objeto** (caras de 12 B, vértices de 16 B), instalados como
> *function pointers* del struct de nivel (`level+0x4C98`/`+0x4C9C`). El
> **nivel BSP** usa los emisores `0x00426256`/`0x00426588`. El texto de abajo
> sigue siendo válido para los modelos.

Firma observada: `void emit(faceSet *fs /*[+8]*/, int polyIndex /*[+0xC]*/,
int *counter /*[+0x10]*/, pool *p /*[+0x14]*/)`.

El registro de cara es de **12 B**:

```
+0x00  u16 idx0
+0x02  u16 idx1
+0x04  u16 idx2
+0x06  u8  campo6
+0x08  u32 param (color o puntero a bloque UV)
+0x0C  ...
```

La tabla de vértices es de **12 B por vértice** (`0x42503B`:
`[fs+0x10] * 0xC`), con posición en `+0x0`/`+0x2`/`+0x4`.

Selección de opcode según `face[+0x07]`:

| Bits `face[+7]` | Opcode | Longitud | Payload escrito |
|---|---|---|---|
| `& 1 == 0`, `& 2 == 0` | `0x20` | `0x18` | `+8` color; `+C/+10/+14` posiciones (4 B por vértice) |
| `& 2` | `0x24` | `0x24` | `+8` dword; `+C` xy; `+10` uv; `+14` xy; `+18` uv; `+1C` xy; `+20` uv |
| `& 1`, `& 2` | `0x30` | `0x20` | `+8` dword; `+C` xy; `+10` uv; `+14` xy; `+18` uv; `+1C` xy |
| `& 1` | `0x34` | `0x2C` | color por vértice (12 B por vértice) |
| `& 8` (dentro de cada rama) | — | — | color plano copiado a los tres vértices |

La proyección se hace con `0x0046AA14` (sólido) o `0x0046ACD0` (texturizado).
Ambas escriben el resultado en los globales `0x646820..0x64686E` y el color
corregido en `0x6474C0..0x6474C6`, y **no tocan** `0x6474C7`.

### 4.2 `0x004FF06F` — rectángulo `0x40` + prefijo `0xE1`

```
0x4FF075  color = {byte[+8], byte[+0xC], byte[+0x10], 0}
0x4FF08B  pool = [0x551644]
0x4FF0C8  word[rec+0x0C] = x0
0x4FF0D6  word[rec+0x0E] = y0
0x4FF0E1  word[rec+0x10] = x1
0x4FF0EC  word[rec+0x12] = y1
0x4FF0FA  dword[rec+0x08] = color
0x4FF100  dword[rec+0x00] = 4        ; longitud provisional
0x4FF109  byte [rec+0x0B] = 0x40     ; opcode (después del color)
0x4FF11C  dword[rec+0x00] = 0x14     ; longitud final
```

Y, si el argumento `[+0x24]` es distinto de 0:

```
0x4FF148  dword[rec2+0x00] = 2
0x4FF154  dword[rec2+0x08] = 0xE1000068
0x4FF16A  dword[rec2+0x00] = 0x0C
```

Ese segundo registro es el **prefijo** `0xE1`: su `dword[+8]` codifica en los
bits 5-6 el nivel de transparencia (`[0x57B8EC]`), y el consumidor lo procesa
sumando 4 al puntero. El rectángulo mide `0x14` B exactos (8 de cabecera +
4 de color + 4 palabras de 16 bits).

**Observación relevante:** en el rectángulo el opcode se escribe **después** del
color (por eso sobrevive). En los triángulos `0x20/0x24/0x30` el emisor escribe
`byte[+0xB] = opcode` **antes** de copiar un dword a `+0x8`; ese dword
(`face[+8]` o `[0x6474C4]`) ocupa `+0x8..+0xB` y por tanto **solapa el opcode**.
El byte alto de ese dword valdría 0 en el binario estático, lo que dejaría el
opcode a 0. Que el juego funcione implica que en runtime ese byte alto lleva el
opcode (dword de cara pre-etiquetado) o que el camino no se usa. **No se puede
decidir estáticamente**; queda como cuestión abierta (§11.1).

---

## 5. Cadena de carga

### 5.1 `0x0043B11D(dfxPath /*[+8]*/, vfxPath /*[+0xC]*/, level /*[+0x10]*/)`

| Dirección | Acción |
|---|---|
| `0x43B287` | `0x00461C32(vfxPath)` → **`.VFX`**; llena `0x6488F4` |
| `0x43B28F` | `0x0045D720()` — inicialización posterior a texturas |
| `0x43B298` | `0x00461E0B(dfxPath)` → **`.DFX`** |
| `0x43B2A9` | `level[+4] = dfxImage` |
| `0x43B2D7..0x43B2E0` | `level[+8] = [0x54D050]` |

**El orden es VFX primero, DFX después.** Es el hecho que explica la relación:
las direcciones de textura se asignan antes de construir la geometría.

`level[+0x14]` es el selector de buffer (`0..1`) y `level[+0x24]` el puntero al
buffer actual. `level[+4]` se consume en muchos puntos; por ejemplo
`0x0040B475` pasa `[[0x551644]+4][0]` (es decir, `dfx[0]`) como raíz a
`0x0041A1E0`, que recorre un árbol (§7.3, hipótesis).

### 5.2 Lector `0x00461B90(path)`

```
[0x5AC784] = 0x8000
f = fopen(path, "rb")           ; 0x00541060
if (!f) error "..."             ; 0x00463011 (MessageBox)
fseek(f, 0, SEEK_END)           ; 0x00540F90
size = ftell(f)                 ; 0x00540DE0
buf = malloc(size)              ; 0x00464FB3
fseek(f, 0, SEEK_SET)
fread(buf, 1, size, f)          ; 0x00540CA0
fclose(f)                       ; 0x00540C20
return buf
```

Devuelve el fichero **completo** en un buffer del arena. `0x00464FB3` redondea
el tamaño a múltiplo de 4 y anota la última reserva en `[0x647FAC]`; `0x0046506B`
es el *rollback* del arena.

### 5.3 Carga del `.DFX` (`0x00461E0B`)

```
p = 0x00461B90(path)
count = dword[p]
reloc = { count, entries = p+4 }
aligned = ((count + 0x200) & ~0x1FF)        ; dwords
0x004735C0(&reloc, p + 4*aligned, p)        ; delta = p
sizeDwords = [0x647FAC] / 4
end = p + 4*(sizeDwords - aligned)
while (p < end) { *p = p[aligned]; p += 4 } ; compacta sobre la tabla
0x0046506B(4*aligned)                       ; libera la tabla
return base
```

La compactación **mueve la imagen al principio del buffer**, eliminando el
hueco de la tabla de offsets. El puntero devuelto es el mismo `base`.

### 5.4 Relocalizador `0x004735C0(reloc*, end, delta)`

```
q = reloc->entries; end = reloc->entries + 4*reloc->count
for (; q < end; q += 4) {
    idx = *q >> 2
    *(u32*)(base + 4*idx) += delta
}
```

`base` es el tercer argumento. Cada entrada es un **offset de imagen** (siempre
múltiplo de 4) que apunta a un dword; se le suma la base para convertir el
offset relativo en un puntero absoluto. **No es una tabla de objetos**: es una
lista de **posiciones de punteros**.

---

## 6. Formato `.DFX` (validado con `GEX.DFX`)

### 6.1 Cabecera

```
+0x00  u32 count                     ; número de entradas de relocalización
+0x04  u32 offsets[count]            ; offsets de imagen (múltiplos de 4)
+...   relleno hasta 512 dwords      ; aligned = ((count+0x200)&~0x1FF)
+0x...  imagen relocalizable
```

### 6.2 Medidas reales

| Dato | Valor |
|---|---|
| Tamaño de fichero | 146 008 B (`0x23A58`) |
| `count` | 322 (`0x142`) |
| `aligned` | 512 dwords = `0x800` B |
| Inicio de imagen | `0x800` |
| Tamaño de imagen | 143 960 B (`0x23258`) |
| Entradas desalineadas | 0 |
| Offset máximo | `0x20808` (133 128) — dentro de la imagen |
| `image[0]` | `0x2E0` — raíz (offset válido y alineado) |
| Punteros en los primeros `0x200` B de imagen | 90 |
| `image[0x8C]` | `0x10138`, **es** objetivo de relocalización |

La cabecera de la imagen (primeros dwords, antes de relocalizar) es:

```
image[0x00] = 0x000002E0    ; raíz / nodo principal
image[0x04] = 0x00000000
image[0x08] = 0x00350001    ; versión o tamaño (hipótesis)
image[0x0C] = 0x0000004C    ; puntero (relocalizado)
image[0x10] = 0x00000050    ; puntero
image[0x14] = 0x27100FA0
image[0x18] = 0x2EE01770
image[0x1C] = 0x000001F8    ; puntero
image[0x20] = 0x00000040    ; puntero
image[0x24] = 0x00000034    ; puntero
```

El nodo raíz (`0x2E0`) tiene los primeros `0x20` B a cero salvo
`+0x8C`, que es otro puntero relocalizado (`0x10138`). Ese `image[+0x8C]` se usa
en `0x43B2BB` como **origen de un `strcpy`** a `0x54FA90` cuando
`level[+0x4E38] == 2`; el contenido observado en `GEX.DFX` empieza por `0x28`,
así que **no es un nombre de nivel legible** y su semántica queda abierta.

### 6.3 Interpretación

* El `.DFX` es un **blob con punteros absolutos** (estilo "relocatable image"),
  no un contenedor con tabla de recursos.
* La tabla de offsets existe **solo** para la relocalización y se destruye al
  cargar.
* La raíz en `image[0]` y los punteros de la cabecera describen un **grafo de
  escena** (nodos, listas de caras, materiales). La estructura interna concreta
  (árbol/BSP, nodos de `0x8C` B) es **hipótesis** (§7.3).

---

## 7. Formato `.VFX` (validado con `GEX.VFX`)

### 7.1 Cabecera y registros

```
+0x00  u32 count
+0x04  registro[0]   (0x8C B de cabecera + pixels)
...    registro[1..count-1] consecutivos
```

Cada registro mide `0x8C + record[+0x84]` B (el puntero de píxeles es
`record + 0x8C`).

### 7.2 Medidas reales

| Dato | Valor |
|---|---|
| Tamaño de fichero | 1 588 B (`0x634`) |
| `count` | 4 |
| Registro 0 | `@0x004`, `+0x84=256`, `+0x8C=@0x090` |
| Registro 1 | `@0x190`, `+0x84=256`, `+0x8C=@0x21C` |
| Registro 2 | `@0x31C`, `+0x84=256`, `+0x8C=@0x3A8` |
| Registro 3 | `@0x4A8`, `+0x84=256`, `+0x8C=@0x534` |
| Fin calculado | `0x634` — **exacto** |
| `+0x00/+0x04` | `4/4` (16×16) en los 4 |
| `+0x08` | `3` en los 4 |
| `+0x0C` | `1` en los 4 |
| `+0x14` | `0x33261804`, `0x3C2B1100`, `0x33221100`, `0x351F150B` |

El ajuste exacto del final confirma que el tamaño de registro es
`0x8C + [+0x84]` y que no hay tabla de offsets ni alineación adicional.

### 7.3 Relación con el árbol del `.DFX` (hipótesis)

> **Corregido en U-1h (§4, §6):** la hipótesis quedó **demostrada y ampliada**.
> `dfx[0]` es la raíz de un **BSP** cuyos nodos miden `0x20` B (unión
> interno/hoja con tipo `+0x12` = 1/2). Hay dos recorridos distintos:
> `0x00401C00` (visibilidad → `0x4269C2` emisión → dibujo) y `0x0041A203`
> (**colisión segmento-BSP**). U-1g **no** clasificó `0x0041A203` como dibujo:
> en §7.3 solo constató un recorrido de árbol sin algoritmo. Además,
> `0x004024B9` + callback `0x005015A9` recorre el árbol y marca los polígonos
> con `0x20`; su uso como *pickups*/triggers es **hipótesis** (no hay polígonos
> `0x20` en los tres niveles auditados). Detalle completo en
> `U1H_RESULTADOS.md`.

`0x0040B475` hace `0x0041A1E0(dfx[0], arg2, arg3, 0)`; `0x0041A1E0` es un
envoltorio de `0x0041A203`, que trabaja con una pila de contextos
(`0x6476C0`, `0x6476E0`, `0x6476F4`, …) y recorre nodos. U-1g dejó como
**hipótesis razonable pero no demostrada** que `dfx[0]` fuese la raíz de un
**árbol/BSP**; U-1h lo demostró en §3–§6: nodo de `0x20` B, tipo `+0x12` y
algoritmo de los tres recorridos (visibilidad, colisión y consulta esférica).

---

## 8. Relación DFX ↔ VFX

| Aspecto | `.DFX` | `.VFX` |
|---|---|---|
| Contenido | grafo de escena + geometría + materiales | texturas y paletas |
| Cabecera | `{count, offsets[]}` + imagen | `count` + registros consecutivos |
| Punteros | absolutos, relocalizados por `0x4735C0` | solo offsets internos (`+0x10`, `+0x8C`) |
| Carga | `0x00461E0B` → `level[+4]` | `0x00461C32` → `0x6488F4` |
| Orden | **después** | **antes** |
| Referencia al otro | **ninguna** | **ninguna** |

**Hecho:** el puente es el **emisor de comandos**. `0x00425010` toma caras del
`.DFX` (`faceSet`), proyecta sus vértices y escribe registros cuyo campo de
14 bits indexa la tabla del `.VFX`. `0x004FF06F` hace lo mismo para rectángulos
sin textura. Después, `0x00469836` recorre la lista y `0x0046558A` resuelve el
índice contra `0x6488F4` y emite las llamadas Glide.

**Hipótesis no demostrada:** que el campo de 14 bits se copie directamente de
un campo del `.DFX` (probablemente del bloque UV de la cara) sin traducción
intermedia. Se ha verificado el destino (tabla) y el origen inmediato
(`word[record+0x1A]`), no el camino completo cara→UV→registro.

---

## 9. Integración en el frame

```
0x00461FBD  (frame)
  level[+0x14] ^= 1
  buf = [0x64FEE0 + 4*level[+0x14]]
  [0x551644]+0x24 = buf
  buf[4] = buf + 0xC ; buf[0] = 0
  0x00465530(buf, 0xC00)
  ... emisores 0x00425010 / 0x004FF06F ...
  0x00434FE9(...)
  buf[0x5FF8+4] = 0x64FB40
  0x00469836(buf + 0x5FF8)
  while (grBufferNumPending()) ;
  grBufferSwap(2)
```

El pool se **reinicia entero** cada frame (`0x465530`), de modo que los
registros no sobreviven entre frames. El doble buffer evita que el emisor
sobrescriba la lista que la GPU está consumiendo.

---

## 10. Hechos, hipótesis y resultados negativos

### 10.1 Hechos demostrados

1. `0x0046558A` dibuja **un** comando; el recorrido es de `0x00469836`.
2. La lista es enlazada por `[+4]`; `[0]` es longitud y flag de dibujo.
3. El opcode está en `byte[+0xB]`; `0xE1` es prefijo de 4 B.
4. `cmd = opcode & ~3`; el índice de despacho es `cmd - 0x20`; el mapa está en
   `0x00469765` y la tabla de handlers en `0x00469735`.
5. El índice de textura es `word[+0x1A] & 0x3FFF`; el nivel es `>> 14`.
6. La tabla `0x006488F4` la construye `0x00461C32` desde el `.VFX`.
7. El `.VFX` se carga antes que el `.DFX` en `0x0043B11D`.
8. El `.DFX` es un blob relocalizable `{count, offsets[]}`; `0x004735C0` suma la
   base y `0x00461E0B` compacta la imagen.
9. Los dos formatos encajan exactamente con los assets reales (§6, §7).
10. El rectángulo `0x40` mide `0x14` B y su prefijo `0xE1` mide `0xC` B.

### 10.2 Hipótesis (sin confirmar)

1. El nodo `dfx[0]` es la raíz de un **árbol/BSP** recorrido por `0x0041A203`.
2. `record[+0x0C] == 1` significa formato con tabla/paleta y activa
   `grTexDownloadTable`.
3. El byte alto del dword de cara contiene el opcode en runtime (§11.1).
4. `record[0] < 7` en `0x461C32` es una comprobación de formato con paleta.
5. El campo `+0x14` de los registros `.VFX` es la tabla NCC/paleta.
6. `image[+0x08] = 0x350001` es versión o tamaño del grafo.

### 10.3 Resultados negativos

1. **No existe** ninguna referencia del `.DFX` al `.VFX` (ni nombres, ni rutas,
   ni índices) en los datos analizados.
2. **No existe** una tabla de texturas dentro del `.DFX`; la única tabla de
   texturas es `0x006488F4`, construida desde el `.VFX`.
3. **No existe** un identificador de recurso por comando distinto del índice de
   14 bits: ni handles, ni nombres, ni direcciones de fichero.
4. **No hay** buffer de vértices separado: los vértices van inline en cada
   registro y el emisor los escribe directamente.
5. **No se ha demostrado** que el `.DFX` contenga comandos ya formados: el flujo
   de comandos se **genera en runtime** en el pool de `0x6000` B.

---

## 11. Cuestiones abiertas

> **Estado tras U-1h (2026-11-06):** las cuestiones **1 a 7** quedaron
> **resueltas estáticamente** (ver referencias). Permanecen abiertas la 8 y la 9,
> heredadas de U-1f.

1. **Solapamiento `+0x8` / `+0xB` en triángulos.** El emisor escribe
   `byte[+0xB] = opcode` y después copia un dword a `+0x8`. Estáticamente el
   byte alto de ese dword es 0; dinámicamente debe llevar el opcode o el comando
   no se dibujaría. Requiere traza en runtime (fuera del alcance read-only de
   esta ronda).
   → **RESUELTA en U-1h §9.1:** el orden es el inverso; el opcode se escribe
   **después** del color en los cuatro puntos de escritura
   (`0x4263D2`, `0x426530`, `0x4268EC`, `0x426975`). No hay solapamiento ni
   dependencia de runtime.
2. **Longitud `0x20` del opcode `0x30`.** El handler lee `u`/`v` del tercer
   vértice en `+0x20/+0x21`, fuera de la longitud declarada. O el registro
   reutiliza los 4 B siguientes del pool, o el emisor y el handler no coinciden
   (posible inconsistencia del original). No se ha podido decidir sin ejecutar.
   → **RESUELTA en U-1h §9.2:** el handler del `0x30` es `0x004673E9`, que lee
   colores `+8/+0x10/+0x18` y posiciones `+0xC/+0x14/+0x1C`: exactamente `0x20`
   B, sin desbordamiento. Las lecturas de UV pertenecen al handler `0x24`:
   índice en `word[rec+0x1A]` y UV2 en `byte[rec+0x20]`/`byte[rec+0x21]`; `0x2C`
   lee el índice en `+0x1A` y `0x34`/`0xB4` en `+0x1E`.
3. **Estructura interna del `.DFX`.** Se conoce la cabecera, la relocalización y
   la raíz; no el tamaño ni el significado de los nodos, ni el algoritmo de
   recorrido.
   → **RESUELTA en U-1h §3–§6:** descriptor completo, nodo BSP de `0x20` B
   (unión interno/hoja), polígono `0x14` B, vértice 12 B, normal 6 B, auxiliar
   UV 12 B y tres recorridos trazados (visibilidad, colisión y consulta esférica).
4. **`image[+0x8C]` del `.DFX`**: puntero relocalizado usado como origen de
   `strcpy`; su contenido no es un nombre legible en `GEX.DFX`.
   → **RESUELTA en U-1h §3.6:** es el nombre de nivel de 12 caracteres en
   `image+0xE0` (`"aztec______"`, `"circuit____"`, `"rezop______"`); en
   `GEX.DFX` apunta a datos no textuales porque su cabecera no es la de nivel.
5. **Semántica de `record[+0x0C]`** (`1` observado) y de `record[0] < 7`.
   → **RESUELTA en U-1h §11:** `record[+0x0C]` es `GrTexInfo.format`
   (`0` sin tabla, `1` con tabla NCC, `0xC` otro).
6. **Significado de `+0x14` del `.VFX`** (tabla NCC/paleta, hipótesis).
   → **RESUELTA en U-1h §11:** son datos NCC **inline**: se pasa
   `record+0x14` directamente como `data` a `grTexDownloadTable`. Además se
   corrigió que `0x00461C32` **no** descarga la tabla: eso ocurre por comando,
   en los handlers `0x24`, `0x2C` y `0xB4` (no en `0x461C32`).
7. **Registro estático de cierre `0x64FB40`**: U-1g creyó que su opcode era 0
   y que `+0xE`/`+0x12` no tenían semántica establecida.
   → **RESUELTA en U-1h §10:** es el rectángulo 2D de fondo; el opcode `0xF0`
   se escribe en la inicialización `0x4635BD`, la longitud inicial `4` pasa a
   `0x14` en el reencadenado, y `+0xE`/`+0x12` son y/alto. El reencadenado real
   es `0x462240..0x4622AB`; `+0x12` vale `0x180` si
   `[0x551644][+0x6FB0] > 0` y `0xE0` en caso contrario, y `+0x10 = 0x200`.
8. **Layout del vértice de `grDrawPlanarPolygonVertexList`** (opcode `0x28`,
   4 vértices desde `[ebp-0x138]`), heredado de U-1f. **Sigue abierta.**
9. **`0x004338D7`** (rama `estilo & 0x80`), heredado de U-1f. **Sigue abierta.**

---

## 12. Relación con las fases siguientes

* **U-1g queda completada** en lo que respecta a: encuadre del flujo, opcodes,
  índice de recursos, cadena de carga y formatos `.DFX`/`.VFX`.
* **U-1h (interpretación final)** puede ya apoyarse en hechos firmes: sabe qué
  es comando, qué es vértice, qué es textura y qué es grafo de escena. Le queda
  la **semántica** de los campos (incluidas las cuestiones 1, 2, 3, 5 y 6).
* **Port gráfico:** el formato del flujo es reproducible sin el original; el
  port puede generar registros equivalentes o sustituirlos por su propio batch.
  Lo que **no** puede hacer todavía es interpretar el grafo del `.DFX` para
  extraer geometría (§11.3).

---

## 13. Documentación actualizada en esta ronda

* `U1G_RESULTADOS.md` (este documento).
* `ROADMAP.md` — U-1g marcado como completado.
* `CHANGELOG.md` — entrada U-1g.
* `PROJECT_STATE.md` — estado vigente de U-1 y pendientes reordenados.

`PROJECT_STATUS.svg` **no cambia**: no hay evolución de MAIN ni hito verificado
del panel; esto es investigación estática en `dev/gex`.
