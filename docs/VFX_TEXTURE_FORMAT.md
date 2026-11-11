# VFX_TEXTURE_FORMAT — el contenedor de texturas `.VFX` de Gex PC (Módulos 4A y 4B)

**Fecha:** 2026-11-11 · **Estado:** contenedor, **dimensiones**, **formatos de
píxel** y **decodificadores CPU** **DEMOSTRADOS** (esta revisión corrige el
Módulo 4B publicado en `b71122a`).

La fuente de verdad de este documento es **el ejecutable original**
(`C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE`, PE32 x86, sin modificar), **los
ficheros originales** (`C:\GEX_REZ\E2_A_ORIGINAL\LEVEL\*.VFX`, solo lectura) y
**el SDK público de Glide 2.x** (el paquete `glide 2002.04.10ds1`, que es el
código de la biblioteca de 3dfx; se usa como *referencia documental* del
formato: **no se ha copiado código suyo al port**, los decodificadores están
escritos desde cero a partir de las reglas documentadas).

Las herramientas externas (`gex2-tools`, `Gex3DViewer`) se usan como
**referencia técnica**, nunca como fuente primaria y **sin copiar código**
(ninguna de las dos tiene licencia identificada).

Herramientas usadas para el análisis, todas ya presentes en el entorno:

| Herramienta | Uso |
|---|---|
| `llvm-objdump` (toolchain llvm-mingw) | desensamblado del ejecutable |
| `tools/gexmap/pe.mjs` (repo) | lector PE32 e IAT: nombres de los imports |
| Node.js (repo, `tools/gexmap`) | scripts de análisis de ficheros y tablas |
| SDK de Glide 2.x (paquete Debian `glide`) | enumeraciones, estructuras y tablas de tamaños |
| `port/` (repo) | módulo portable `gex/vfx.h` + `src/asset/vfx.c` + `src/asset/vfx_decode.c` |

No se instaló nada (Ghidra/REA no están disponibles y su instalación requiere
autorización expresa: `tools/setup-re-env.ps1` es un borrador sin probar).

---

## 1. Resumen

| Cuestión | Estado | Evidencia |
|---|---|---|
| Estructura del contenedor | **DEMOSTRADO** | §2 |
| Tabla de entradas de textura | **DEMOSTRADO** | §2, §3 |
| Correspondencia índice de 14 bits (DFX) → entrada (VFX) | **DEMOSTRADO** | §5 |
| El registro es un `GrTexInfo` de Glide 2.x | **DEMOSTRADO** | §4.1, §6.3 |
| Los identificadores 1/11/12 son los `GR_TEXFMT` de Glide 2.x | **DEMOSTRADO** | §4.2 |
| El bloque de 112 B de `+0x14` es una `GuNccTable` (no una paleta) | **DEMOSTRADO** | §4.3, §6.4 |
| Dimensiones de cada textura | **DEMOSTRADO** (LOD + aspecto) | §4.5, §6.5 |
| Tamaño esperado del bloque de datos | **DEMOSTRADO** (15 251/15 251 registros) | §6.5 |
| Formato de píxel de 1, 11 y 12 | **DEMOSTRADO** | §4.4, §6.6 |
| Decodificadores CPU | **DEMOSTRADO** (con pruebas) | §7 |
| Compresión | **NO hay descompresor en el juego** (los bytes van tal cual a Glide) | §4.3 |
| Orden de bytes de los formatos de 16 bits | **little-endian** (demostrado con el original) | §4.4 |
| Escala de los vectores de croma I/Q | **1:1** (demostrado con el original) | §4.4 |

---

## 2. Estructura del contenedor (DEMOSTRADO)

### 2.1 La cadena de carga en el original

`0x00461C32` construye la tabla global de texturas `0x006488F4` a partir del
`.VFX` del nivel. Desensamblado (llvm-objdump, x86 AT&T):

```
0x461c32: pushl %ebp ; movl %esp,%ebp ; subl $0x10,%esp
0x461c38: movl 0x8(%ebp),%eax          ; el búfer del fichero
0x461c3c: calll 0x461b90                ; 0x461B90 = carga el fichero entero en memoria
0x461c44: movl %eax,-0x8(%ebp)
0x461c47: movl -0x8(%ebp),%ecx
0x461c4a: movl (%ecx),%edx              ; count = dword[0]
0x461c4c: movl %edx,-0x4(%ebp)
0x461c4f: movl -0x4(%ebp),%eax
0x461c52: shll $0x2,%eax                ; count * 4
0x461c56: calll 0x464fb3                ; malloc
0x461c5e: movl %eax,0x6488f4            ; 0x6488F4 = tabla (array de punteros)
0x461c63: movl -0x8(%ebp),%ecx
0x461c66: addl $0x4,%ecx                 ; el primer registro está en fichero+4
...
0x461c8a: (bucle i = 0 .. count-1)
0x461c93: movl -0x8(%ebp),%eax          ; registro actual
0x461c96: movl %eax,(%edx,%ecx,4)       ; tabla[i] = registro
0x461ca5: addl $0x8c,%eax               ; registro + 0x8C
0x461cb6: movl %eax,0x10(%ecx)          ; registro->[0x10] = puntero a los datos
0x461cc4: addl $0x8c,%ecx               ; registro + 0x8C
0x461cdb: addl 0x84(%ecx),%edx          ; + dword[registro+0x84]
0x461ce1: movl %edx,-0x8(%ebp)          ; siguiente registro
```

Es decir, **el juego recorre exactamente**:

```
count      = dword[0]
registro_i = fichero + 4 + Σ (0x8C + dword[registro_j + 0x84])   (j < i)
datos_i    = registro_i + 0x8C
tamaño_i   = dword[registro_i + 0x84]
```

`0x461B90` es "cargar el fichero entero": `fopen` (`0x5515CC` = `"rb"`),
`fseek(..., SEEK_END)`, `ftell`, `malloc`, `fseek(..., SEEK_SET)`,
`fread`, `fclose`.

El cargador **no copia ni transforma los datos**: `registro+0x10` apunta
directamente al bloque dentro del fichero cargado en memoria.

### 2.2 Comprobación contra los ficheros originales

El recorrido anterior, aplicado byte a byte a los ficheros reales, **termina
exactamente en el EOF**:

| Fichero | Tamaño | `count` | Registros | ¿El recorrido acaba en el EOF? |
|---|---:|---:|---:|---|
| `LEVEL/AZTEC16.VFX` | 1 140 276 | 260 | 260 | **sí** |
| `GEX.VFX` | 1 588 | 4 | 4 | **sí** |

Los 37 `.VFX` de `E2_A_ORIGINAL` (36 niveles + `GEX.VFX`) tienen la misma
forma: el `count` del primer dword coincide con el número de registros que se
pueden recorrer **y el recorrido termina exactamente en el EOF** (medido byte
a byte en los 37 ficheros: ninguno sobra ni falta). Por eso el parser exige
que el último bloque termine en el EOF: si sobran bytes, el contenedor es
incoherente y se rechaza (ver §7.1).

### 2.3 Formato

```
+0x00  dword   count                 número de texturas
+0x04  registro[0]
       ...
       registro[count-1]
```

Cada registro es un **`GrTexInfo` de Glide 2.x**:

```
+0x00  dword   smallLod             GrLOD_t: 0 = 256 px ... 8 = 1 px
+0x04  dword   largeLod             GrLOD_t (igual que smallLod en los
                                    ficheros originales: un solo nivel)
+0x08  dword   aspectRatio          GrAspectRatio_t: 0 = 8x1 ... 6 = 1x8
+0x0C  dword   format               GrTextureFormat_t de Glide 2.x
+0x10  dword   puntero a los datos  lo escribe el juego (en el fichero hay un
                                    valor de relleno: 0x0064FE04 en AZTEC16.VFX
                                    y en GEX.VFX: hay que IGNORARLO)
+0x14  ...     tabla NCC            GuNccTable de 112 B, SOLO en el formato 1
+0x84  dword   tamaño del bloque de datos
+0x88  dword   cursor del pool      lo escribe el juego (en el fichero copia
                                    del tamaño)
+0x8C  bytes   bloque de datos      tamaño = dword[+0x84]
```

El registro siguiente está en `registro + 0x8C + dword[registro+0x84]`.

---

## 3. El puntero de +0x10: NO es un puntero válido

En el fichero, `dword[+0x10]` vale `0x0064FE04` en **todos** los registros de
`AZTEC16.VFX` y de `GEX.VFX`: es una dirección del segmento de datos del
ejecutable, no un puntero a los datos de la textura. El juego lo **sobrescribe**
con `registro + 0x8C` (el inicio del bloque de datos) antes de entregar el
registro a Glide. Un parser que copie ese valor al revés leería datos de otro
fichero o basura; hay que **ignorarlo** y usar `registro + 0x8C`.

Esto es el mismo patrón de "puntero que hay que relocalizar" que el `.DFX`
(`0x004735C0`), pero aquí lo resuelve el propio cargador: no hace falta
relocalizar, basta con no usar el valor.

---

## 4. Formatos

### 4.1 El registro es un `GrTexInfo` de Glide 2.x (DEMOSTRADO)

El ejecutable construye un `GrTexInfo` para la fuente (`FONT.3DF`) en
`0x0065BFA0` y lo rellena así (`0x00432AB7..0x00432AE2`):

```
0x432AB7: movl %eax,0x65bfa0          ; +0x00  smallLod   (del info de 3df)
0x432AC2: movl %ecx,0x65bfa4          ; +0x04  largeLod   (del info de 3df)
0x432ACE: movl %edx,0x65bfa8          ; +0x08  aspectRatio(del info de 3df)
0x432ADA: movl %eax,0x65bfac          ; +0x0C  format     (del info de 3df)
0x432AE2: movl %ecx,0x65bfb0          ; +0x10  puntero a los datos
```

El SDK de Glide 2.x define esa estructura exactamente así
(`glide2x/h3/glide/src/glide.h`):

```c
typedef struct {
    GrLOD_t           smallLod;
    GrLOD_t           largeLod;
    GrAspectRatio_t   aspectRatio;
    GrTextureFormat_t format;
    void              *data;
} GrTexInfo;
```

Coincide campo a campo con el registro `.VFX`, y el cargador (`0x00461C32`)
escribe `registro+0x10 = registro+0x8C` y entrega el registro tal cual a
`grTexDownloadMipMap`/`grTexSource`. **El registro VFX es un `GrTexInfo`.**

Esto corrige una hipótesis que U-1f dejó abierta
(`U1F_RESULTADOS.md` §8.2): el orden de los campos es
**smallLod, largeLod, aspectRatio, format, data** (no `largeLod` primero).

### 4.2 Los identificadores de formato son los `GR_TEXFMT` de Glide 2.x (DEMOSTRADO)

El juego no usa una enumeración propia: el campo `format` del `GrTexInfo`
**es** un `GrTextureFormat_t` de Glide 2.x, y sus valores son los del SDK
(`glide2x/h3/glide/src/glide.h`):

```c
typedef FxI32 GrTextureFormat_t;
#define GR_TEXFMT_8BIT                  0x0
#define GR_TEXFMT_RGB_332 GR_TEXFMT_8BIT
#define GR_TEXFMT_YIQ_422               0x1
#define GR_TEXFMT_ALPHA_8               0x2
#define GR_TEXFMT_INTENSITY_8           0x3
#define GR_TEXFMT_ALPHA_INTENSITY_44    0x4
#define GR_TEXFMT_P_8                   0x5
#define GR_TEXFMT_RSVD0                 0x6
#define GR_TEXFMT_RSVD1                 0x7
#define GR_TEXFMT_16BIT                 0x8
#define GR_TEXFMT_ARGB_8332 GR_TEXFMT_16BIT
#define GR_TEXFMT_AYIQ_8422             0x9
#define GR_TEXFMT_RGB_565               0xa
#define GR_TEXFMT_ARGB_1555             0xb   /* 11 */
#define GR_TEXFMT_ARGB_4444             0xc   /* 12 */
#define GR_TEXFMT_ALPHA_INTENSITY_88    0xd
#define GR_TEXFMT_AP_88                 0xe
#define GR_TEXFMT_RSVD2                 0xf
```

Los tres valores que aparecen en los `.VFX` son exactamente:

| Valor del `.VFX` | `GR_TEXFMT` de Glide 2.x | Bytes por texel | Alfa |
|---:|---|---:|---|
| 0 | `GR_TEXFMT_RGB_332` | 1 | no (el juego lo distingue con otro modo de alfa) |
| 1 | `GR_TEXFMT_YIQ_422` | 1 | no |
| 11 | `GR_TEXFMT_ARGB_1555` | 2 | 1 bit |
| 12 | `GR_TEXFMT_ARGB_4444` | 2 | 4 bits |

Distribución medida en los 37 `.VFX` (15 251 registros): formato 1 → 7 723,
formato 11 → 4 698, formato 12 → 2 830, formato 0 → 0.

### 4.3 El bloque de 112 B es una tabla NCC, no una paleta (DEMOSTRADO)

**Corrección importante.** La primera versión de este documento (y el Módulo
4B publicado en `b71122a`) interpretaba el bloque de `+0x14` como una "tabla de
paleta". Es incorrecto.

* La **paleta** de Glide 2.x (`GuTexPalette`) son 256 `FxU32` = **1 024 bytes**,
  no 112.
* La **tabla NCC** (`GuNccTable`, "Narrow Channel Compression") ocupa
  **exactamente 112 bytes**:

```c
typedef struct {
  FxU8  yRGB[16];      /* 16 B */
  FxI16 iRGB[4][3];    /* 24 B */
  FxI16 qRGB[4][3];    /* 24 B */
  FxU32 packed_data[12]; /* 48 B */
} GuNccTable;           /* 112 B */
```

* El `type` que el juego pasa a `grTexDownloadTable` es **0**, y en Glide 2.x
  `0` es la **tabla NCC 0**, no la paleta:

```c
typedef FxU32 GrTexTable_t;
#define GR_TEXTABLE_NCC0    0x0
#define GR_TEXTABLE_NCC1    0x1
#define GR_TEXTABLE_PALETTE 0x2
```

* El manual de referencia de Glide 2.2 lo dice literalmente: *"data: Table
  data, either of type GuNccTable or GuTexPalette"*, y *"The color palette is
  referenced when rendering texture formats `GR_TEXFMT_P_8` or
  `GR_TEXFMT_AP_88`. One of two NCC tables is used when decompressing texture
  formats `GR_TEXFMT_YIQ_422` or `GR_TEXFMT_AYIQ_8422`"*.

* Y el **formato 1 es `YIQ_422`**, precisamente el formato que necesita la
  tabla NCC. La coherencia es total.

La llamada en el original (solo para el formato 1):

```
0x466136: movl -0x30(%ebp),%ecx
0x466139: cmpl $0x1,0xc(%ecx)      ; if (formato == 1)
0x466142: addl $0x14,%edx           ; registro + 0x14
0x466145: pushl %edx
0x466146: pushl $0x0                ; type = GR_TEXTABLE_NCC0
0x466148: pushl $0x0                ; tmu = 0
0x46614a: calll 0x5405c2            ; grTexDownloadTable(0, 0, registro+0x14)
```

(y la equivalente en `0x004670FC`).

**Prueba directa sobre los originales:** el `packed_data` que trae el fichero
coincide **byte a byte** con el que el propio SDK calcula a partir de los
campos crudos (`yRGB` en 4 palabras; los 3 valores de 9 bits de cada vector de
croma en una palabra). Por ejemplo, en `GEX.VFX`, registro 0:

```
iRGB[0] = (482, 488, 75)  ->  packed = (482<<18)|(488<<9)|75 = 0x078BD04B
qRGB[0] = (500, 501, 50)  ->  packed = (500<<18)|(501<<9)|50 = 0x07D3EA32
```

y ambos valores aparecen literalmente en el bloque del fichero. La prueba
automática (`test_vfx.c`) recalcula el empaquetado de **las 7 723 tablas** de
los 37 `.VFX` y compara: **coinciden las 12 palabras de todas**.

El juego **comparte** la tabla entre dos texturas copiando el bloque entero
(`memcpy` de `0x70` B en `0x004732FB`): en `AZTEC16.VFX` hay un par de
entradas con tabla idéntica (las entradas 25 y 83).

### 4.4 Los formatos de píxel (DEMOSTRADO)

Las tres disposiciones de bit, del SDK de Glide 2.x (`swlibs/texus/lib/quantize.c`
y `dequant.c`), con la palabra de 16 bits en **little-endian** (Voodoo es
little-endian y el juego entrega los bytes tal cual):

| Formato | Bit 15..12 | R | G | B | Alfa |
|---|---|---|---|---|---|
| `ARGB_1555` | A (1 bit) | 5 bits (14..10) | 5 bits (9..5) | 5 bits (4..0) | 0 → 0, 1 → 255 |
| `ARGB_4444` | A (4 bits) | 4 bits (11..8) | 4 bits (7..4) | 4 bits (3..0) | 0..15 → 0..255 |

Expansión a 8 bits: 5 → 8 por replicación de bits (`v<<3 | v>>2`), 4 → 8
multiplicando por 17 (`v*0x11`). Es exactamente lo que hacen las tablas
`P5_8`/`P4_8` del SDK.

`YIQ_422` son **8 bits por texel** y el byte es un **índice de una paleta de
256 colores** que se deriva de la tabla NCC: 4 bits de luminancia, 2 bits de
croma I y 2 bits de croma Q.

```
iy = (byte >> 4) & 0x0F     ->  luminancia  yRGB[iy]
ia = (byte >> 2) & 0x03     ->  croma I     iRGB[ia]
ib = (byte     ) & 0x03     ->  croma Q     qRGB[ib]

R = saturar(y + iRGB[ia][0] + qRGB[ib][0])
G = saturar(y + iRGB[ia][1] + qRGB[ib][1])
B = saturar(y + iRGB[ia][2] + qRGB[ib][2])
```

Esto es lo que hacen el decodificador software del propio SDK
(`swlibs/texus/lib/dequant.c`, `_txImgDequantizeYIQ422`, que indexa una paleta
de 256 construida con `txYABtoPal256`) y el constructor de la red neuronal que
genera esas tablas (`nccnnet.c`: `iy = (i>>4)&0x0f; ia = (i>>2)&0x03;
ib = i&0x03;` y `color = Y[iy] + A[ia] + B[ib]`).

Los valores de croma I/Q se guardan en 9 bits con signo (complemento a dos),
que es el rango que usa el hardware: en los ficheros originales los valores
crudos van de 0 a 511 y se reparten en torno a 256 (p. ej. 482 → −30,
500 → −12), lo que confirma la interpretación con signo.

**Contraste empírico con los originales** (no hay imagen de referencia, así que
se contrastaron hipótesis):

* Leer los campos de 16 bits en big-endian produce un campo plano casi
  saturado; en little-endian sale una **textura coherente** con estructura
  visible. Es la lectura correcta.
* Interpretar I/Q como sin signo produce un **66 % de píxeles saturados**
  (basura); con signo, un **0,2 %**, y la imagen sale un degradado suave. La
  escala 1:1 (suma directa) es la que reproduce el degradado; con 1:2 o 1:4 el
  contraste se aplana.

Aspecto practice: `GEX.VFX` registro 0 decodificado con el código del port
(16×16, YIQ_422):

```
      .:-==++++=
     .:=*####*++
     .-*%%%%%##*
     .=#%%%%%%##
     .-*%%%%%###
     .-+#%%%%###
     .:-****####
      .:--=*###*
       ...-*##*#
          =###*#
         :*###*#
....... .+##*#*#
----:. .+*#*#*##
.:.   :+*#*###*#
....-=**********
+++********+++++
```

### 4.5 Dimensiones: LOD, aspecto y tamaño (DEMOSTRADO)

Glide 2.x (`glide2x/h3/glide/src/glide.h`):

```c
typedef FxI32 GrLOD_t;
#define GR_LOD_256 0x0 ... #define GR_LOD_1 0x8

typedef FxI32 GrAspectRatio_t;
#define GR_ASPECT_8x1 0x0
#define GR_ASPECT_4x1 0x1
#define GR_ASPECT_2x1 0x2
#define GR_ASPECT_1x1 0x3
#define GR_ASPECT_1x2 0x4
#define GR_ASPECT_1x4 0x5
#define GR_ASPECT_1x8 0x6
```

* El **LOD fija el lado mayor**: `lado = 256 >> lod` (lod 0 → 256 px, lod 8 →
  1 px).
* El **aspecto lo divide**, sin bajar de un texel: 8x1 → `lado × lado/8`,
  4x1 → `lado × lado/4`, 2x1 → `lado × lado/2`, 1x1 → `lado × lado`,
  1x2 → `lado/2 × lado`, 1x4 → `lado/4 × lado`, 1x8 → `lado/8 × lado`.
* La **cadena de mipmap** va de `large_lod` (el nivel más detallado) a
  `small_lod`, con el tamaño de cada nivel = `texels × bytes_por_texel`
  (`glide2x/h3/glide/src/g3df.c`:
  `for(lod = large_lod; lod <= small_lod; lod++) mem += hostSize[aspect][lod] << (format >= GR_TEXFMT_16BIT);`).

Esto reproduce exactamente las tablas del SDK (`_grMipMapHostWH[7][9][2]` y
`_grMipMapHostSize[4][9]` de `glide2x/sst1/glide/src/ditex.c`), por ejemplo:

| lod | 8x1 | 4x1 | 2x1 | 1x1 | 1x2 | 1x4 | 1x8 |
|---:|---|---|---|---|---|---|---|
| 0 | 256×32 | 256×64 | 256×128 | 256×256 | 128×256 | 64×256 | 32×256 |
| 2 | 64×8 | 64×16 | 64×32 | 64×64 | 32×64 | 16×64 | 8×64 |
| 4 | 16×2 | 16×4 | 16×8 | 16×16 | 8×16 | 4×16 | 2×16 |
| 6 | 4×1 | 4×1 | 4×2 | 4×4 | 2×4 | 1×4 | 1×4 |
| 8 | 1×1 | 1×1 | 1×1 | 1×1 | 1×1 | 1×1 | 1×1 |

En los **37 `.VFX` originales los 15 251 registros tienen `smallLod ==
largeLod`**: cada textura es un único nivel, y el tamaño del bloque de datos
coincide **en los 15 251 registros** con el tamaño que predice la fórmula
(§6.5).

### 4.6 Comparación con las herramientas externas (referencia, sin copiar código)

`gex2-tools` (SK83RJOSH, sin licencia) describe el VFX como
`File{texture_count, textures[]}` y nombra los formatos `RGB8A1 = 1`,
`R7G6B5A1 = 11` y `ARGB4 = 12`, con "dimensiones desde tamaño + aspecto" y un
"workaround de bug encoder" en el formato 1. Su lectura coincide con este
documento **en el contenedor** y en los **valores** de formato, pero:

* sus **nombres** de formato no son los de Glide: los valores coinciden
  (1 = YIQ de 8 bits, 11 = ARGB de 16 bits con alfa de 1 bit, 12 = ARGB de 16
  bits con alfa de 4 bits) pero la nomenclatura es propia;
* su obtención de las dimensiones ("tamaño + aspecto") no es la fórmula de
  Glide 2.x que aquí se demuestra (§4.5);
* su código **no se ha copiado**: el módulo `port/src/asset/vfx*.c` está
  escrito desde cero contra el SDK, contra el desensamblado y contra los
  ficheros.

`Gex3DViewer` (MatBourgon, sin licencia) abre `.dfx` + `.vfx` y exporta
"sheets" de textura; sirve como referencia de que el par DFX/VFX por nivel es la
unidad correcta, que es justo lo que usa el port.

---

## 5. Correspondencia DFX → VFX

La geometría del nivel lleva el **índice de textura de 14 bits** en el registro
UV (`word[uv+0x02] & 0x3FFF`, tal y como lo lee `gex_emit_collect`), y el
juego lo usa así:

```
0x465648: movw 0x1e(%eax),%cx          ; word del registro de comando
0x46564c: andl $0x3fff,%ecx            ; índice de 14 bits
0x465652: movl 0x6488f4,%edx           ; la tabla de texturas
0x465658: movl (%edx,%ecx,4),%eax      ; tabla[indice]
```

Es decir: **el índice de 14 bits indexa directamente la tabla del `.VFX`**, sin
transformación intermedia.

Comprobación sobre los originales (`port/tests/test_vfx.c`):

* `AZTEC16.DFX` tiene 9 609 polígonos; sus índices de textura son **0, 1, 2 y
  3** (514, 12, 6 689 y 2 394 polígonos respectivamente) y ninguno es ≥ 16;
* `AZTEC16.VFX` tiene 260 entradas → **todos los índices del nivel tienen
  entrada** en el `.VFX` del mismo nivel.

Los 256 huecos restantes del `.VFX` (índices 4..259) los usan las texturas de
modelos, sprites o de interfaz, que la geometría del nivel no recorre.

---

## 6. Auditoría correctiva del Módulo 4B (2026-11-11)

### 6.1 Qué se corrigió

El Módulo 4B publicado en `b71122a` concluyó que el bloque de 112 B era una
"paleta" y que el formato de píxel y las dimensiones seguían **sin
demostrarse**. Con la documentación de Glide 2.x a mano y el SDK de Glide 2.x
(paquete `glide 2002.04.10ds1`) se corrigieron tres cosas:

| Afirmación del 4B | Estado real | Evidencia |
|---|---|---|
| "el bloque de 112 B es una tabla de paleta" | **FALSO**: es una `GuNccTable` | §4.3, §6.4 |
| "los identificadores 1/11/12 no son los de Glide" | **FALSO**: son `GR_TEXFMT_YIQ_422`, `GR_TEXFMT_ARGB_1555` y `GR_TEXFMT_ARGB_4444` | §4.2, §6.2 |
| "las dimensiones no encajan con ninguna fórmula" | **FALSO**: encajan exactamente con las reglas de LOD y aspecto de Glide 2.x | §4.5, §6.5 |
| "el registro es un `GrTexInfo`" | **CORRECTO**, pero el orden de los campos es smallLod, largeLod, aspectRatio | §4.1, §6.3 |

### 6.2 Las enumeraciones reales de Glide 2.x

El paquete `glide 2002.04.10ds1` (Debian) incluye el código de la biblioteca de
3dfx con los encabezados originales (`glide2x/{sst1,h3,cvg}/glide/src/glide.h`).
La variante `h3` (Voodoo2) y la `sst1` (Voodoo1) coinciden en lo que importa:

* `GrTextureFormat_t`: 0 = RGB_332, 1 = YIQ_422, …, **11 = ARGB_1555,
  12 = ARGB_4444**;
* `GrTexTable_t`: 0 = NCC0, 1 = NCC1, **2 = PALETTE**;
* `GrTexInfo`: `{smallLod, largeLod, aspectRatio, format, data}`;
* `GuNccTable`: 112 B; `GuTexPalette`: 1 024 B.

El ejecutable usa `glide2x.dll` (Voodoo1/Voodoo2), así que la numeración
aplicable es la de `glide2x`.

### 6.3 El `GrTexInfo` que el juego construye para la fuente

Además de la fuente `.3DF`, el juego rellena un `GrTexInfo` en `0x0065BFA0`
con la misma disposición (§4.1). Los campos salen del `Gu3dfHeader` de la
fuente, que ya trae `small_lod`, `large_lod`, `aspect_ratio` y `format` en el
mismo orden: el ejecutable y el SDK coinciden sin ambigüedad.

### 6.4 La prueba byte a byte de la `GuNccTable`

El `packed_data` de la tabla es la prueba definitiva, porque el SDK define una
forma empaquetada **bit a bit**. Tomando `GEX.VFX`, registro 0:

| Campo crudo | Valor | Empaquetado que calcula el SDK | En el fichero |
|---|---|---|---|
| `iRGB[0]` | (482, 488, 75) | `(482<<18)\|(488<<9)\|75` = `0x078BD04B` | `0x078BD04B` ✓ |
| `qRGB[0]` | (500, 501, 50) | `(500<<18)\|(501<<9)\|50` = `0x07D3EA32` | `0x07D3EA32` ✓ |
| `yRGB[0..3]` | `04 18 26 33` | `0x33261804` | `0x33261804` ✓ |

Una paleta de 256 colores no tiene ninguna forma empaquetada de este tipo: la
coincidencia de las 12 palabras en las 7 723 tablas de los 37 `.VFX` es una
prueba directa de que el bloque es una `GuNccTable`.

### 6.5 Validación del modelo de dimensiones sobre TODOS los originales

Script de contraste (no forma parte del port): para cada registro de los 37
`.VFX` se calcula el tamaño esperado con las reglas de §4.5 y se compara con
`dword[+0x84]`.

```
ficheros: 37
registros: 15251
formatos: 1:7723 11:4698 12:2830
coincidencias: 15251/15251
registros con +0x00 != +0x04: 0
discrepancias: 0
```

**El 100 % de los registros encaja.** Ejemplos:

| Fichero | Registro | lod/aspecto/formato | Tamaño esperado | Tamaño en el fichero |
|---|---:|---|---:|---:|
| `GEX.VFX` | 0 | 4/4/3, YIQ_422 | 16×16×1 = 256 | 256 ✓ |
| `GEX.VFX` | 0 | 4/4/3, YIQ_422 | 16×16×1 = 256 | 256 ✓ |
| `AZTEC16.VFX` | 0 | 2/2/2, ARGB_4444 | 64×32×2 = 4 096 | 4 096 ✓ |
| `AZTEC16.VFX` | 1 | 2/2/3, ARGB_4444 | 64×64×2 = 8 192 | 8 192 ✓ |
| `AZTEC16.VFX` | 25 | 0/0/5, YIQ_422 | 64×256×1 = 16 384 | 16 384 ✓ |

La excepción que se había medido antes ((4,4,3) con formato 12) era un error
de aritmética del fitting, no un registro anómalo: con la regla de Glide 2.x
(64×64×2 = 8 192 B) **todos** los registros cuadran.

### 6.6 Contraste de los decodificadores

* **Orden de bytes**: little-endian (big-endian aplana la imagen).
* **Signo de I/Q**: con signo (sin signo → 66 % de saturación).
* **Escala de I/Q**: 1:1 (1:2 y 1:4 aplanan el contraste).
* **Alfa**: el formato 12 tiene alfa de 4 bits (en las texturas reales hay
  píxeles opacos y transparentes); el 11, de 1 bit; el YIQ_422 no tiene alfa.
* **Prueba cruzada**: el mismo texture decodificado con el script de análisis y
  con el código del port da **exactamente la misma imagen**.

### 6.7 Resultado negativo: la prueba de discontinuidad anterior era inválida

La conclusión del 4B de que "los bloques no son un raster plano" **no se
sostiene**: la prueba (diferencia entre la discontinuidad horizontal y la
vertical para anchos candidatos) mezclaba en la misma comparación **texturas
de tamaños distintos y niveles mipmap distintos**, y el ancho "ganador" no
tenía por qué ser el correcto. Con el tamaño de la textura ya demostrado
(§4.5), la lectura del raster es inmediata y el resultado es una imagen
coherente. Se **descarta** la prueba de discontinuidad como método.

### 6.8 Lo que sigue sin estar demostrado

| Cuestión | Estado |
|---|---|
| Formato del `FONT.3DF` (que el juego carga con `gu3dfLoad`) | **DESCONOCIDO** (no es un `.VFX`; el inventario lo da como RGB332) |
| Texturas con formato 0 (`RGB_332`) | **DESCONOCIDO**: no aparece en ningún `.VFX` original, así que no hay decodificador |
| Formatos 6, 7, 8, 9, 10, 13, 14, 15 de Glide | **DESCONOCIDO**: no aparecen |
| Si el hardware aplica un filtrado (bilineal) entre texels | fuera de alcance: es comportamiento de la GPU, no del formato |

---

## 7. Lo implementado en `port/`

| Fichero | Contenido |
|---|---|
| `port/include/gex/vfx.h` | API pública: `gex_vfx_file`, `gex_vfx_record`, `gex_vfx_status`, `gex_vfx_open/parse/close`, `gex_vfx_find_index`, `gex_vfx_format_has_ncc_table`, `gex_vfx_format_info_get`, `gex_vfx_ncc_table_twin`, `gex_vfx_ncc_table_parse/pack`, `gex_vfx_texture_info_get`, `gex_vfx_texture_level_size/offset`, `gex_vfx_decode_argb1555/argb4444/yiq422`, `gex_vfx_decode_level` |
| `port/src/asset/vfx.c` | parser portable: recorre la tabla como el juego, valida offsets/tamaños/conteos y multiplicaciones, dimensiones según las reglas de Glide 2.x, libera en error |
| `port/src/asset/vfx_decode.c` | decodificadores CPU: ARGB_1555, ARGB_4444 y YIQ_422 con tabla NCC; salida RGBA8 con alfa recto |
| `port/tests/test_vfx.c` | 2 968 comprobaciones |

La API es coherente con el resto del port (C11 portable, `gex_*_status` con
`_name()`, `free`/`close` idempotente, cero en error). El módulo **no** crea
recursos D3D11 ni se integra todavía en el renderer (eso es el Módulo 4C).

### 7.1 Validaciones implementadas

* ruta nula o vacía → `GEX_VFX_ERR_ARG`;
* fichero de menos de 4 bytes → `GEX_VFX_ERR_FORMAT`;
* cabecera de registro que no cabe → `GEX_VFX_ERR_RANGE`;
* bloque de datos que no cabe → `GEX_VFX_ERR_RANGE`;
* **el recorrido de los `count` registros no termina en el EOF (sobran bytes)
  → `GEX_VFX_ERR_FORMAT`**: la tabla no cubre el fichero y el contenedor es
  incoherente;
* `count * sizeof(registro)` que desborda `size_t` → `GEX_VFX_ERR_OVERFLOW`;
* `count` enorme → reserva fallida → `GEX_VFX_ERR_NOMEM`;
* en cualquier error, la estructura queda limpia y liberable.

### 7.2 Semántica de la tabla NCC

`gex_vfx_record.ncc_table`/`ncc_table_size` describen una **`GuNccTable` de
112 B únicamente para el formato `YIQ_422`**, que es el único para el que el
juego llama a `grTexDownloadTable` con `GR_TEXTABLE_NCC0`. Para los formatos 11
y 12 el parser deja `ncc_table = NULL` y `ncc_table_size = 0`: el bloque `+0x14`
está a cero en los ficheros originales. `gex_vfx_format_has_ncc_table(format)`
deja el criterio explícito.

### 7.3 Dimensiones

`gex_vfx_texture_info_get()` devuelve el tamaño del nivel más detallado, el
número de niveles mipmap, los bytes por texel y el **tamaño total esperado del
bloque de datos**, con las reglas de §4.5. `gex_vfx_texture_level_size()` y
`gex_vfx_texture_level_offset()` dan el tamaño y la posición de cada nivel de
la cadena.

### 7.4 Decodificadores

* Salida **RGBA8** (R, G, B, A), alfa **recto** (no premultiplicado).
* `gex_vfx_decode_argb1555()` y `gex_vfx_decode_argb4444()`: lectura
  little-endian de la palabra de 16 bits y expansión de bits según §4.4.
* `gex_vfx_decode_yiq422()`: índice 4/2/2 bits y suma de luminancia y croma
  saturada, con la tabla NCC como entrada.
* `gex_vfx_decode_level()`: despacha por el formato del registro y usa la
  tabla NCC del propio registro.
* Validación de entradas: punteros nulos, dimensiones nulas o mayores de 256,
  datos truncados (`GEX_VFX_DEC_ERR_RANGE`), destino insuficiente
  (`GEX_VFX_DEC_ERR_RANGE`) y multiplicaciones desbordadas
  (`GEX_VFX_DEC_ERR_OVERFLOW`). Un formato sin decodificador devuelve
  `GEX_VFX_DEC_ERR_FORMAT`.

### 7.5 Pruebas (reales)

`port/tests/test_vfx.c`: **2 968 comprobaciones, 0 fallos** en Clang `-O0`,
`-O2` y ASan/UBSan (`-fsanitize=address,undefined -fno-sanitize-recover=all`).

* nombres de estado y argumentos inválidos;
* contenedor sintético: campos, puntero de datos, tamaño, tabla NCC, offsets;
* entradas truncadas, tamaño desbordado y límite exacto (el bloque que termina
  en el EOF es válido);
* **bytes sobrantes al final**: con 1, 2, 4 y 8 bytes de cola, y con dos
  registros más 16 bytes de cola, el parser devuelve `GEX_VFX_ERR_FORMAT` y
  deja la estructura limpia; un `count` de 0 con 4 bytes de datos también es
  incoherente;
* **semántica de la tabla NCC** en los tres formatos observados: el formato
  `YIQ_422` publica `ncc_table` y `ncc_table_size = 0x70`; los formatos 11 y 12
  publican `ncc_table = NULL` y `ncc_table_size = 0` (tanto en el contenedor
  sintético como en los ficheros originales, registro a registro);
* **dimensiones**: la tabla completa de tamaños de Glide 2.x para LOD y aspecto,
  la cadena de mipmap (tamaño y offset de cada nivel) y los registros
  incoherentes (LOD o aspecto fuera de rango, cadena al revés, formato
  desconocido);
* **tabla NCC**: interpretación de los 112 bytes (luminancia, croma con signo de
  9 bits) y round trip exacto del empaquetado (`pack(parse(x)) == x`);
* **decodificadores**: valores conocidos a mano para ARGB_1555, ARGB_4444 y
  YIQ_422 (incluida la saturación), truncamientos, destinos pequeños,
  punteros nulos y dimensiones inválidas;
* **ficheros originales**: el tamaño del bloque de datos de **cada** registro
  de `AZTEC16.VFX` coincide con el que predice el modelo de dimensiones; el
  `packed_data` de **cada** tabla NCC de los originales coincide con el
  recalculado; la decodificación del primer registro de cada formato funciona
  y es **determinista**;
* `AZTEC16.VFX`: 260 registros, el recorrido acaba exactamente en el EOF, los
  formatos son {1, 11, 12} (42/73/145) y todos los tamaños son potencias de
  dos;
* `GEX.VFX`: 4 registros de 256 bytes, formato 1;
* correspondencia DFX → VFX: los 9 609 polígonos del nivel usan índices 0..3 y
  todos existen en el `.VFX`;
* **tabla NCC compartida**: en AZTEC16.VFX hay un par de entradas (25 y 83) con
  el bloque idéntico, que es el caso que resuelve el `memcpy` de 0x70 B del
  juego (`0x004732FB`); la relación se comprueba simétrica.

### 7.6 Auditoría correctiva del parser (2026-11-11, módulo 4A)

Dos defectos detectados en la auditoría directa del commit `a7d8f0f` y
corregidos:

1. **Validación estricta del EOF.** El parser recorría los `count` registros y
   aceptaba el fichero aunque sobraran bytes al final. Ahora, tras el
   recorrido, **el offset final tiene que coincidir exactamente con el tamaño
   del fichero**; si no, `GEX_VFX_ERR_FORMAT` y estructura limpia. La
   corrección no invalida ningún fichero real: los 37 `.VFX` de `E2_A_ORIGINAL`
   terminan exactamente en el EOF (medido).
2. **Semántica de la tabla.** `ncc_table`/`ncc_table_size` se publicaban para
   todos los formatos. Ahora **solo el formato `YIQ_422`** (el único con tabla
   NCC) los expone; para 11 y 12 valen `NULL` y `0`.

Con ello el comportamiento final del parser es: **acepta exactamente los
contenedores que el juego leería** (tabla que cubre el fichero entero) y
rechaza cualquier otro, sin dejar recursos vivos.

---

## 9. El Módulo 4C: de la coordenada de Glide al muestreo de D3D11

### 9.1 Qué calcula el original

El manejador de la ruta 3D (`0x00465E5A`, opcode `0x34`) escribe la
coordenada de textura del vértice así:

```
0x46619D: movb 0x10(%ecx),%dl        ; el byte de UV del comando
0x4661A6: fildl -0x208(%ebp)         ; (float)byte
0x4661AC: fmuls 0x54c188             ; * 1.0f
0x4661B2: fdivs 0x54c18c             ; / 16.0f
0x4661B8: fstps 0x6475a4             ; vertex->tmuvtx[0].sow
```

El divisor es una **constante** (16.0f): el espacio de coordenadas **no
depende del tamaño de la textura**, y por tanto el byte de UV del `.DFX` está
en un espacio independiente de la resolución de la textura.

La otra ruta del juego que usa coordenadas (la de las fuentes 2D, `0x432BE2`)
usa `* 256.0f / 16.0f`, y la fuente `FONT.3DF` (cabecera `3df v1.1 rgb332 lod
range: 128 128. aspect ratio: 4 1`, 4 096 bytes de datos) se recorre como una
rejilla de **16 columnas x 4 filas** de celdas de **8x8 texels**: 16 unidades
por celda sobre 128 texels ⇒ **1 unidad = 1/2 texel**.

### 9.2 Qué se ha implementado en el port

- El vértice transporta la UV normalizada **dividiendo entre 256** (no 255): el
  byte 255 es el último texel, no el borde derecho, y con `/255` el último
  vértice caía en 1.0 y el sampler con WRAP lo devolvía al primer texel
  (costura en el borde de cada polígono). El cambio es reversible
  (`round(u * 256)` devuelve el byte original).
- El **modo de interpretación** se elige al crear el conjunto de texturas:
  - `GEX_NCC_UV_MODE_TEXEL` (por defecto): el byte es la coordenada en
    **texels** y la textura se repite cada `w x h` texels.
  - `GEX_NCC_UV_MODE_NORMALIZED`: el byte es una fracción de la textura
    (256 bytes = la textura entera).
- **Por defecto el modo texel**: es el único que mantiene la **densidad de
  texels constante** entre texturas de distinta resolución (una de 64x64 y
  otra de 256x256 reparten el mismo número de texels por unidad de mundo), que
  es lo que necesita un nivel. La lectura normalizada haría que la densidad
  dependiera de la resolución de cada textura. Las dos lecturas producen
  imágenes distintas (el 69 % de los píxeles con geometría cambia entre ellas)
  y se han capturado las dos en ejecución real.
- **Límite conocido:** el factor exacto de zoom (cuántos texels por byte de
  UV) **no se ha podido cerrar** con evidencia estática: depende de la escala
  de la cámara real del juego, que sigue sin validarse dinámicamente (S-2). El
  período de repetición (cada `w x h` texels) sí es el mismo en las dos
  lecturas.

### 9.3 Los modos de filtrado y direccionamiento del original

El ejecutable **no importa** `grTexFilterMode`, `grTexClampMode` ni
`grTexMipMapMode`: usa los valores por defecto de Glide 2.x, que son

- filtrado **lineal** (bilineal),
- direccionamiento **WRAP** (la textura se repite),
- **mipmapping desactivado** (solo el nivel `largeLod`), lo que encaja con que
  los 15 251 registros medidos tengan `smallLod == largeLod`.

El port usa un sampler lineal con WRAP y **no genera mipmaps** ✓.

### 9.4 La estrategia de recursos

El índice de textura es **por polígono** y el nivel usa varias texturas de
**dimensiones distintas**, así que hace falta un recurso único del que el
shader elija:

- **Descartado: varias llamadas de dibujo** (una por textura): el port dibuja
  en el orden de emisión (algoritmo del painter, sin *depth buffer*) y
  reordenar las primitivas cambiaría el resultado visual.
- **Descartado en la práctica: texture array**: es la opción más directa, pero
  en el equipo de desarrollo la creación de un array 2D con datos iniciales
  falla con `E_INVALIDARG` (hardware y WARP) y llega a colgar el driver.
- **Elegido: un atlas**: una sola textura 2D con todas las texturas del nivel,
  cada una con **un texel de borde replicado** para que el filtrado bilineal no
  mezcle una textura con la vecina. El shader calcula la posición del texel
  dentro de la textura y la traduce a la del atlas.

Las imágenes originales se conservan **intactas y con sus dimensiones** (en
`gex_vfx_texture`); el atlas es solo la empaquetación para la GPU. El índice de
14 bits del `.VFX` no es el número de textura del atlas: el shader lo busca en
una **tabla constante** (índice → tamaño y posición).

### 9.5 Composición y niebla

El color por vértice se conserva y se **modula** con el texel (`textura *
color`, alfa incluido), como el combinador de color de Glide. La **niebla**
sigue viajando dentro del color por vértice (así lo emite el módulo 2), pero el
original la aplica **después** de la textura: aquí se aplica antes. Es una
diferencia conocida y pendiente de corregir (habría que transportar el factor
de niebla por vértice).

### 9.6 Verificación

- `port/tests/test_vfx.c`: **2 968 comprobaciones, 0 fallos** (`-O0`, `-O2` y
  ASan/UBSan), incluido el banco de texturas portable.
- `port/tests/test_d3d11_pipeline.c`: **324 comprobaciones, 0 fallos** (WARP),
  incluida una prueba de muestreo con un `.VFX` sintético de 64x64 en
  `ARGB_4444`: la UV en el byte 32 debe pintar el texel 8 (R = 136, G = 136,
  B = 255).
- **Ejecución real** (GPU hardware, ventana Win32, 960x540): el nivel de
  AZTEC16 sale **texturizado**; la captura de la ventana tiene 3 101 colores
  distintos y el contenido de la textura se reconoce en el panel con marco de
  la pared.

## 8. Limitaciones y siguiente paso

1. **El Módulo 4C ya no está bloqueado por los datos**: hay decodificadores CPU
   verificados y dimensiones demostradas. Queda decidir el formato de los
   recursos D3D11 (`DXGI_FORMAT_R8G8B8A8_UNORM`), la cadena de mipmap y el modo
   de filtrado, y crear `ID3D11Texture2D` e `ID3D11ShaderResourceView`
   atendiendo al índice de 14 bits.
2. **El `FONT.3DF` sigue sin decodificarse**: es una ruta distinta
   (`gu3dfLoad`), no un `.VFX`.
3. **El formato 0 (`RGB_332`) no tiene decodificador**: no aparece en ningún
   `.VFX` original; si apareciera, haría falta el mismo tratamiento (1 byte por
   texel, `R3 G3 B2`).
4. **No se ha validado la cámara ni el BSP contra el ejecutable** (S-2 sigue
   abierto).
5. Los ficheros originales se han usado **solo en lectura**; no se ha copiado
   ninguna textura ni imagen extraída al repositorio.

---

## 9. Referencias

* `docs/RENDERER_ARCHITECTURE_RESEARCH.md` — el renderer y el uso de Glide.
* `U1H_RESULTADOS.md`, `U1G_RESULTADOS.md` — la cadena de 14 bits y la tabla
  `0x006488F4` (U-1).
* `S2_BSP_TRAVERSAL.md` §5 — el flujo de la emisión y el registro UV.
* `RESEARCH.md` S-24 / `PATCH_ANALYSIS.md` H-01, H-02 — las herramientas
  externas y sus descripciones.
* `tools/glide-shim/README.md` — los 38 imports de `glide2x.dll` que usa el
  ejecutable (aquí: `grTexDownloadTable`, `grTexNCCTable`,
  `grTexDownloadMipMap`, `grTexTextureMemRequired`, `grAlphaCombine`).
* *Glide 2.2 Reference Manual* (Document Release 015, 22 May 1997) —
  `grTexDownloadTable`, `grTexCalcMemRequired`, `grTexTextureMemRequired`,
  `grTexNCCTable`.
* SDK de Glide 2.x (paquete `glide 2002.04.10ds1`): `glide2x/h3/glide/src/glide.h`
  (enumeraciones, `GrTexInfo`, `GuNccTable`), `glide2x/sst1/glide/src/ditex.c`
  (`_grMipMapHostSize`, `_grMipMapHostWH`), `glide2x/h3/glide/src/g3df.c`
  (cadena de niveles y tamaño por nivel), `swlibs/texus/lib/{quantize,dequant,util,nccnnet}.c`
  (layouts de bit, decodificador YIQ de referencia y construcción de la paleta
  de 256).
