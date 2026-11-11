# VFX_TEXTURE_FORMAT — el contenedor de texturas `.VFX` de Gex PC (Módulo 4A)

**Fecha:** 2026-11-11 · **Estado:** contenedor **DEMOSTRADO**; decodificación de
píxel **NO demostrada** (se documenta por qué y qué falta).

La fuente de verdad de este documento es **el ejecutable original**
(`C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE`, PE32 x86, sin modificar) y **los
ficheros originales** (`C:\GEX_REZ\E2_A_ORIGINAL\LEVEL\*.VFX`, solo lectura).
Las herramientas externas (`gex2-tools`, `Gex3DViewer`) se usan como
**referencia técnica**, nunca como fuente primaria y **sin copiar código**
(ninguna de las dos tiene licencia identificada).

Herramientas usadas para el análisis, todas ya presentes en el entorno:

| Herramienta | Uso |
|---|---|
| `llvm-objdump` (toolchain llvm-mingw) | desensamblado del ejecutable |
| `tools/gexmap/pe.mjs` (repo) | lector PE32 e IAT: nombres de los imports |
| Node.js (repo, `tools/gexmap`) | scripts de análisis de ficheros y tablas |
| `port/` (repo) | módulo portable `gex/vfx.h` + `src/asset/vfx.c` |

No se instaló nada (Ghidra/REA no están disponibles y su instalación requiere
autorización expresa: `tools/setup-re-env.ps1` es un borrador sin probar).

---

## 1. Resumen

| Cuestión | Estado | Evidencia |
|---|---|---|
| Estructura del contenedor | **DEMOSTRADO** | §2 |
| Tabla de entradas de textura | **DEMOSTRADO** | §2, §3 |
| Correspondencia índice de 14 bits (DFX) → entrada (VFX) | **DEMOSTRADO** | §5 |
| Formatos presentes en los ficheros | **DEMOSTRADO (los valores)** | §4.1 |
| Formato de píxel de cada formato | **NO demostrado** | §4.3 |
| Dimensiones de cada textura | **NO demostrado** | §4.3 |
| Paleta del formato 1 | **PARCIAL** (existe y la sube el juego; contenido no fijado) | §4.2 |
| Compresión | **NO hay descompresor en el juego** (los bytes van tal cual a Glide) | §4.3 |

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

### 2.2 Comprobación contra los ficheros originales

El recorrido anterior, aplicado byte a byte a los ficheros reales, **termina
exactamente en el EOF**:

| Fichero | Tamaño | `count` | Registros | ¿El recorrido acaba en el EOF? |
|---|---:|---:|---:|---|
| `LEVEL/AZTEC16.VFX` | 1 140 276 | 260 | 260 | **sí** |
| `GEX.VFX` | 1 588 | 4 | 4 | **sí** |

Los 44 `.VFX` del inventario tienen la misma forma (el `count` del primer
dword coincide con el número de registros que se pueden recorrer).

### 2.3 Formato

```
+0x00  dword   count                 número de texturas
+0x04  registro[0]
       ...
       registro[count-1]
```

Cada registro:

```
+0x00  dword   campo A              2, 3, 4 o 6 en los ficheros originales
+0x04  dword   campo B              igual que A en todos los ficheros originales
+0x08  dword   campo C              2..6
+0x0C  dword   FORMATO              1, 11 o 12 en los ficheros originales
+0x10  dword   puntero a los datos  lo escribe el juego (en el fichero hay un
                                      valor de relleno: 0x0064FE04 en AZTEC16.VFX
                                      y en GEX.VFX: hay que IGNORARLO)
+0x14  ...     datos del formato     tabla de paleta (solo formato 1, §4.2)
+0x84  dword   tamaño del bloque de datos
+0x88  dword   cursor del pool       lo escribe el juego (en el fichero copia
                                      del tamaño)
+0x8C  bytes   bloque de datos       tamaño = dword[+0x84]
```

El registro siguiente está en `registro + 0x8C + dword[registro+0x84]`.

---

## 3. El puntero de +0x10: NO es un puntero válido

En el fichero, `dword[+0x10]` vale `0x0064FE04` en **todos** los registros de
`AZTEC16.VFX` y de `GEX.VFX`: es una dirección del segmento de datos del
ejecutable, no un puntero a los datos de la textura. El juego lo **sobrescribe**
con `registro + 0x8C` (el inicio del bloque de datos) antes de handing the
record a Glide. Un parser que copie ese valor al revés leería datos de otro
fichero o basura; hay que **ignorarlo** y usar `registro + 0x8C`.

Esto es el mismo patrón de "puntero que hay que relocalizar" que el `.DFX`
(`0x004735C0`), pero aquí lo resuelve el propio cargador: no hace falta
relocalizar, basta con no usar el valor.

---

## 4. Formatos

### 4.1 Los valores que aparecen (DEMOSTRADO)

En `AZTEC16.VFX` (260 registros) y en el resto de niveles:

| Valor | Registros en AZTEC16.VFX | Qué hace el juego con él |
|---:|---:|---|
| 1 | 42 | sube una **tabla de paleta** desde `registro+0x14` (`grTexDownloadTable`, §4.2) |
| 11 | 73 | — |
| 12 | 145 | activa el **blending alfa** (`grAlphaCombine` con modo 3 en `0x0046568C`) |

El valor está en `+0x0C` y el juego lo compara explícitamente:

```
0x465667: cmpl $0xc,0xc(%ecx)     ; ¿formato == 12?
0x465679: cmpl $0x0,0xc(%edx)     ; ¿formato == 0?
```

### 4.2 La paleta del formato 1 (PARCIAL)

`0x0046612C` (consumidor de la tabla) hace, **solo para el formato 1**:

```
0x466136: movl -0x30(%ebp),%ecx
0x466139: cmpl $0x1,0xc(%ecx)      ; if (formato == 1)
0x466142: addl $0x14,%edx           ; registro + 0x14
0x466145: pushl %edx
0x466146: pushl $0x0
0x466148: pushl $0x0
0x46614a: calll 0x5405c2            ; grTexDownloadTable(0, 0, registro+0x14)
```

Es decir: **el formato 1 es el único que lleva una tabla de paleta**, y la
tabla está en `+0x14`. En `GEX.VFX` (cuatro texturas de formato 1) ese bloque
empieza con `04 18 26 33 44 5d 78 9d 9e a6 b4 c3 bd d5 de f2`, que parece una
rampa de 4 colores RGBA… pero **los bytes de datos usan valores de 0x00 a
0xFF**, así que esa rampa de 4 entradas **no** puede ser la paleta completa.
Queda por determinar qué es exactamente ese bloque (posiblemente un descriptor
`GrTable_t` o una paleta comprimida) y cuál es la paleta de 256 entradas.

### 4.3 Lo que NO está demostrado (y por tanto no se implementa)

1. **El formato de píxel.** Los bytes del bloque se entregan **tal cual** a
   Glide (`grTexDownloadMipMap`, nivel 3) → no hay ningún descompresor ni
   conversor en el juego. El juego solo dice "formato = X" a Glide. Por tanto
   el significado de los bytes es el de los **formatos de textura de Glide**
   para ese identificador, y la equivalencia exacta entre estos identificadores
   y la enumeración de Glide (`GR_RGBA4`/`GR_RGB8_32`/`GR_COLOR_INDEX_8`/…)
   **no está demostrada con el ejecutable**: el juego nunca compara el campo
   con los valores de la enumeración de Glide, solo con `0x00` y `0x0C`.
2. **Las dimensiones.** Los campos A, B y C (`+0x00`, `+0x04`, `+0x08`) no
   encajan con ninguna fórmula simple de dimensiones: por ejemplo, para el
   registro 0 de
   `AZTEC16.VFX` (A=2, B=2, C=2, formato 12) el bloque son 4 096 bytes, que a
   2 B/píxel serían 2 048 píxeles, mientras que `2^2 × 2^2` serían 16 píxeles.
   La única relación clara es que **el tamaño del bloque es siempre una
   potencia de dos** en los ficheros originales (32, 256, 512, 1 024, 2 048,
   4 096, 8 192, 16 384, 32 768) y que `C` (el campo de `+0x08`) escala el
   tamaño en factor 2 (a igualdad de A y B). No se ha encontrado la fórmula
   que relacione tamaño y dimensiones.
3. **La codificación concreta de los bytes.** Se probaron varias lecturas
   (byte directo, palabras de 16 bits en ambos sentidos, nibbles) midiendo la
   coherencia espacial (desviación media entre píxeles vecinos horizontales):
   el formato 11 muestra un byte constante y el formato 12 una estructura muy
   regular, lo que apunta a formatos empaquetados de Glide, pero **no se ha
   cerrado** cuál.
4. **Si hay compresión.** No hay ningún descompresor en el juego, así que a
   priori **no la hay**: los bytes del fichero son los que consume Glide. Es la
   hipótesis vigente, no una certeza.

> Por instrucción explícita del enunciado ("no inventar formatos ni
> interpretar bytes desconocidos como texturas válidas"), **el módulo 4A no
> implementa decodificadores de píxel**: se entrega el contenedor, la tabla, la
> validación estricta y los bloques de datos crudos, y se documenta lo que
> falta.

### 4.4 Comparación con las herramientas externas (referencia, sin copiar código)

`gex2-tools` (SK83RJOSH, sin licencia) describe el VFX como
`File{texture_count, textures[]}` y nombra los formatos `RGB8A1 = 1`,
`R7G6B5A1 = 11` y `ARGB4 = 12`, con "dimensiones desde tamaño + aspecto" y un
"workaround de bug encoder" en el formato 1. Su lectura coincide con este
documento **en el contenedor** (un `count` y una tabla de texturas) y en los
**valores** de formato (1, 11, 12), pero:

* sus **nombres** de formato no son los de Glide y no están respaldados por el
  ejecutable: el juego nunca usa esos nombres, solo los valores;
* su obtención de las dimensiones ("tamaño + aspecto") no coincide con nada
  demostrado aquí (§4.3.2);
* su código **no se ha copiado**: el módulo `port/src/asset/vfx.c` está escrito
  desde cero contra el desensamblado y contra los ficheros.

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

## 6. Lo implementado en `port/`

| Fichero | Contenido |
|---|---|
| `port/include/gex/vfx.h` | API pública: `gex_vfx_file`, `gex_vfx_record`, `gex_vfx_status`, `gex_vfx_open/parse/close`, `gex_vfx_find_index`, `gex_vfx_format_has_palette` |
| `port/src/asset/vfx.c` | parser portable: recorre la tabla como el juego, valida offsets/tamaños/conteos y multiplicaciones, libera en error |
| `port/tests/test_vfx.c` | 1 122 comprobaciones |

La API es coherente con el resto del port (C11 portable, `gex_*_status` con
`_name()`, `free`/`close` idempotente, cero en error). El módulo **no** crea
recursos D3D11 ni se integra todavía en el renderer (eso es el Módulo 4B).

### 6.1 Validaciones implementadas

* ruta nula o vacía → `GEX_VFX_ERR_ARG`;
* fichero de menos de 4 bytes → `GEX_VFX_ERR_FORMAT`;
* cabecera de registro que no cabe → `GEX_VFX_ERR_RANGE`;
* bloque de datos que no cabe → `GEX_VFX_ERR_RANGE`;
* `count * sizeof(registro)` que desborda `size_t` → `GEX_VFX_ERR_OVERFLOW`;
* `count` enorme → reserva fallida → `GEX_VFX_ERR_NOMEM`;
* en cualquier error, la estructura queda limpia y liberable.

### 6.2 Pruebas (reales)

`port/tests/test_vfx.c`: **1 122 comprobaciones, 0 fallos** en Clang `-O0`, `-O2`
y ASan/UBSan (`-fsanitize=address,undefined -fno-sanitize-recover=all`).

* nombres de estado y argumentos inválidos;
* contenedor sintético: campos, puntero de datos, tamaño, paleta, offsets;
* entradas truncadas, tamaño desbordado y límite exacto (el bloque que termina
  en el EOF es válido);
* `AZTEC16.VFX`: 260 registros, el recorrido acaba exactamente en el EOF, los
  formatos son {1, 11, 12} (42/73/145) y todos los tamaños son potencias de
  dos;
* `GEX.VFX`: 4 registros de 256 bytes, formato 1;
* correspondencia DFX → VFX: los 9 609 polígonos del nivel usan índices 0..3 y
  todos existen en el `.VFX`.

---

## 7. Limitaciones y siguiente paso

1. **No hay decodificador de píxel**: falta cerrar §4.3 (formato de píxel,
   dimensiones y paleta). El camino más corto es localizar la enumeración de
   Glide que el juego da por buena (los campos que `grTexDownloadMipMap`
   consume) y contrastar el resultado decodificando una textura conocida.
2. **No hay integración en el renderer**: crear `ID3D11Texture2D` y
   `ID3D11ShaderResourceView` y atender el índice de 14 bits es el Módulo 4B,
   y exige resolver antes el punto 1 (o usar una textura de color sólido, que
   sería inventar datos).
3. **No se ha validado la cámara ni el BSP contra el ejecutable** (S-2 sigue
   abierto).
4. Los ficheros originales se han usado **solo en lectura**; no se ha
   copiado ninguna textura ni imagen extraída al repositorio.

---

## 8. Referencias

* `docs/RENDERER_ARCHITECTURE_RESEARCH.md` — el renderer y el uso de Glide.
* `U1H_RESULTADOS.md`, `U1G_RESULTADOS.md` — la cadena de 14 bits y la tabla
  `0x006488F4` (U-1).
* `S2_BSP_TRAVERSAL.md` §5 — el flujo de la emisión y el registro UV.
* `RESEARCH.md` S-24 / `PATCH_ANALYSIS.md` H-01, H-02 — las herramientas
  externas y sus descripciones.
* `tools/glide-shim/README.md` — los 38 imports de `glide2x.dll` que usa el
  ejecutable (aquí: `grTexDownloadTable`, `grTexNCCTable`,
  `grTexDownloadMipMap`, `grTexTextureMemRequired`, `grAlphaCombine`).
