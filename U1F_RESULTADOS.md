# RESULTADOS DE U-1f — RELACIÓN ENTRE ARGUMENTOS/ÍNDICES Y RECURSOS (2026-11-05)

**Binario analizado:** `C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE`
(PE32 i386, 1 557 504 B, EU v1.00.000; `imageBase = 0x00400000`).

**Modo:** análisis estático read-only. No se ha ejecutado el juego, no se ha abierto
ventana alguna, no se ha modificado el ejecutable ni ningún fichero de `C:\GEX_REZ`,
y no se ha instalado ni configurado ninguna herramienta.

> **Consolidación final (2026-11-07).** Este documento conserva el texto
> publicado de U-1f. El estado consolidado de U-1, la separación de ámbitos
> y los pendientes están en [`U1_CONSOLIDACION.md`](U1_CONSOLIDACION.md).

---

## 0. Resumen ejecutivo

U-1f tenía como objetivo averiguar si los argumentos que construyen vértices
mantienen alguna relación demostrable con índices, offsets, tablas, buffers o
recursos externos. La respuesta es **sí en las dos rutas, pero de forma distinta y
sin ninguna relación entre ellas**:

* **Ruta 2D de texto (`0x00432BBD`)**: el primer argumento es un **índice de glifo**
  que se convierte en coordenadas de textura sobre **una única textura global
  (`FONT.3DF`)**. Es una relación índice → recurso directa y cerrada:
  no hay tabla intermedia, ni buffer de vértices, ni índice de dibujo.
* **Ruta 2D de rectángulo (`0x00432DD1`)**: no hay recurso. Los argumentos son
  coordenadas y tamaños empaquetados en bytes; el rectángulo se dibuja sin textura.
* **Ruta 3D (`0x0046558A`)**: es la única que dibuja geometría 3D y es
  completamente independiente de las anteriores. Lee un **flujo de comandos**
  (display list) y usa **un índice de 14 bits (`& 0x3FFF`) como entrada en una
  tabla de punteros a registros de textura en `0x006488F4`**. Esa tabla se
  construye desde el fichero de nivel **`.VFX`**, mientras que el flujo de nivel
  viene del **`.DFX`**.

Por tanto, las hipótesis de U-1a…U-1e sobre «stride de 12 bytes», «cálculo de w»
y «0x00432DD1 = ruta 3D» **son falsas** y se corrigen en §7.

**Resultado negativo principal:** los argumentos de `0x00432BBD` y `0x00432DD1`
no proceden de DFX/VFX, no indexan buffers de vértices y no tienen relación
alguna con la geometría 3D del mundo.

---

## 1. Metodología y herramientas

| Elemento | Detalle |
|---|---|
| Desensamblador | `llvm-objdump` (Intel syntax), `--start-address/--stop-address` |
| Lector PE | `tools/gexmap/pe.mjs` (ya en el repo) para VA↔offset y tabla de imports |
| Referencias cruzadas | `analysis/EU/functions.tsv`, `analysis/EU/calls.tsv` (artefactos ya comprometidos) |
| REA | **no viable**: `rea doctor` → `healthy: false`, `host: win32 x64`, sin Hopper ni Ghidra (`missing_analysis_engine`). No se instaló nada. |
| Ejecución | ninguna. Sin breakpoints, sin ventanas, sin emulador |

Los artefactos de `analysis/EU` se usaron solo como índice de llamadas; **todas las
afirmaciones de instrucción de este documento se verificaron sobre el desensamblado
real** del binario original.

Ficheros temporales de trabajo (fuera del repo, no versionados):

* `%TEMP%\opencode\gex_full.asm` — desensamblado completo (25 435 457 B)
* `%TEMP%\opencode\u1f_432bbd.txt`, `u1f_432dd1.txt`, `u1f_43293f.txt`
* `%TEMP%\opencode\u1f_caller_2d.txt`, `u1f_caller_3d.txt`
* `%TEMP%\opencode\u1f_3d_465e50.txt`, `u1f_3d_469700.txt`

---

## 2. Resolución de thunks (imports de `glide2x.dll`)

Los `call` de las rutas 2D y 3D no van a funciones propias: van a *thunks*
`jmp dword ptr [IAT]`. La tabla de imports resuelve cada uno sin ambigüedad:

| Thunk | IAT | Import |
|---|---|---|
| `0x0054051A` | `0x0065F4B0` | `glide2x.dll!grDrawTriangle@12` |
| `0x00540520` | `0x0065F4AC` | `glide2x.dll!grTexSource@16` |
| `0x00540526` | `0x0065F4A8` | `glide2x.dll!guColorCombineFunction@4` |
| `0x0054052C` | `0x0065F4A4` | `glide2x.dll!grAlphaBlendFunction@16` |
| `0x00540532` | `0x0065F4A0` | `glide2x.dll!grAlphaCombine@20` |
| `0x00540538` | `0x0065F49C` | `glide2x.dll!grConstantColorValue4@16` |
| `0x00540502` | `0x0065F4C0` | `glide2x.dll!grTexDownloadMipMap@16` |
| `0x00540508` | `0x0065F4BC` | `glide2x.dll!grTexTextureMemRequired@8` |
| `0x0054050E` | `0x0065F4B8` | `glide2x.dll!gu3dfGetInfo@8` |
| `0x00540514` | `0x0065F4B4` | `glide2x.dll!gu3dfLoad@8` |
| `0x0054057A` | `0x0065F474` | `glide2x.dll!grChromakeyMode@4` |
| `0x005405B6` | `0x0065F45C` | `glide2x.dll!grDrawLine@8` |
| `0x005405BC` | `0x0065F4CC` | `glide2x.dll!grDrawPlanarPolygonVertexList@8` |
| `0x005405C2` | `0x0065F4D0` | `glide2x.dll!grTexDownloadTable@12` |

`0x005407E0` / `0x005407F0` / `0x005406B0` son CRT enlazado estáticamente
(`strcpy` / `strcat` / `sprintf`), no imports: se identifican por su uso con
`0x54F86C` (`"\font.3df"`), `0x54FAD0` (`"%s.dfx"`), `0x54FAD8` (`"%s.vfx"`) y
`0x551900` / `0x551914` (`"%s\level\map7.dfx"` / `"%s\level\map7.vfx"`).

---

## 3. `0x00432BBD` — glifo 2D: índice → textura

**Firma observada (cdecl):** `void f(char glyph /*[ebp+8]*/, int x /*[ebp+0xc]*/, int y /*[ebp+0x10]*/)`.

Tamaño 532 B. Único llamador: `0x0043293F`.

### 3.1 Cálculo de coordenadas de textura

Con `C0 = [0x54C054] = 256.0f`, `C1 = [0x54C058] = 16.0f`,
`C2 = [0x54C05C] = 1.0f`:

| Campo | Instrucciones | Fórmula resultante |
|---|---|---|
| `u0` | `0x432BC6..0x432BEE` (`& 0xF`, `fild`, `fmul C0`, `fdiv C1`) | `(glyph & 0x0F) * 16` |
| `u1` | `0x432BF1..0x432C22` (`(glyph & 0xF) + 1`, `*256/16`, `-1.0`) | `((glyph & 0x0F) + 1) * 16 - 1` |
| `v0` | `0x432C25..0x432C44` (`sar 4`, `*256/16`) | `(glyph >> 4) * 16` |
| `v1` | `0x432C4A..0x432C72` (`sar 4`, `+1`, `*256/16`, `-1.0`) | `((glyph >> 4) + 1) * 16 - 1` |

Es decir: el byte de glifo se parte en `columna = glyph & 0xF` y
`fila = glyph >> 4`, y las coordenadas resultantes dividen la textura en una
**rejilla de 16×16 celdas** en el espacio de coordenadas de textura de Glide
(rango 0…255 sobre el mipmap completo; de ahí el `256/16`). El tamaño de cada
celda en texeles depende de la dimensión del mipmap y **no se ha verificado
directamente**: con `FONT.3DF` a 128×128 (dato del inventario) cada celda sería
de 8×8 texeles. El `-1.0` recorta una unidad de coordenada en el borde de la
celda como margen anti-sangrado; su equivalencia exacta en texeles tampoco se ha
verificado.

El llamador `0x0043293F` restringe el índice: `(carácter - 0x20) & 0x3F`, luego el
rango realmente usado es **0…63** (filas 0…3 de la rejilla). No se ha observado
ningún otro valor.

### 3.2 Vértices

Se escriben cuatro estructuras consecutivas con **stride 0x3C (60 B)**, bases
`[ebp-0xF8]`, `[ebp-0xBC]`, `[ebp-0x80]`, `[ebp-0x44]`:

| Vértice | x (+0x00) | y (+0x04) | `1.0f` (+0x20) | u (+0x24) | v (+0x28) |
|---|---|---|---|---|---|
| A | `x` | `y` | `1.0f` | `u0` | `v0` |
| B | `x+8` | `y` | `1.0f` | `u1` | `v0` |
| C | `x+8` | `y+8` | `1.0f` | `u1` | `v1` |
| D | `x` | `y+8` | `1.0f` | `u0` | `v1` |

El quad mide **8×8 píxeles**; el `+8` es tamaño de quad, **no** un cálculo de `w`.

**Sobre el campo de `+0x20`:** el documento anterior lo llamaba `oow`. Lo
demostrado es que se escribe el flotante `1.0f` en `vértice+0x20`; **el nombre
del campo no se ha verificado** contra los headers de Glide 2.x (no están en el
repo). Con `u`/`v` en `+0x24`/`+0x28`, el layout habitual de `GrVertex` coloca
`ooz` en `+0x20` y `oow` en `+0x1C`, pero no se ha contrastado. En cualquier caso
**no hay cálculo de `w`**: se escribe una constante.

Triángulos (llamadas reales, orden de argumentos tras el `push` inverso):
`grDrawTriangle(A, B, C)` en `0x432DB4` y `grDrawTriangle(A, C, D)` en `0x432DC8`.

### 3.3 Estado y recurso

```
0x432D65  grConstantColorValue4(0x43000000 (=128.0f), 0, 0, 0)
0x432D74  grAlphaCombine(1, 0, 1, 2, 0)
0x432D81  grAlphaBlendFunction(1, 1, 0, 0)
0x432D88  guColorCombineFunction(4)
0x432D9D  grTexSource(0, [0x0057B5EC], 3, &0x0065BFA0)
```

* `0x0057B5EC` es **una dirección de memoria de textura global**, no un índice.
* `0x0065BFA0` es la estructura `GrTexInfo` de la fuente, rellenada por `0x004329EF`.
* **No hay selección de recurso por glifo**: el recurso es siempre el mismo; el
  glifo solo elige la celda dentro de él.

### 3.4 Conclusión de la ruta

> Relación demostrada: `glifo (byte) → (col, fila) → (u, v) dentro de FONT.3DF`.
> No hay tabla de índices, ni offsets de fichero, ni buffer intermedio.

---

## 4. `0x00432DD1` — rectángulo 2D sin textura

**Firma observada:** `void f(int xy /*[ebp+8]*/, int wh /*[ebp+0xc]*/, int unused /*[ebp+0x10]*/)`.

Tamaño 264 B. Único llamador: `0x00434F18` (que siempre pasa `0` como tercer argumento).
El cuerpo **no lee** `[ebp+0x10]`.

### 4.1 Desempaquetado

| Instrucción | Operación |
|---|---|
| `0x432DEA` `and eax,0xFF` | `x = xy & 0xFF` |
| `0x432DF8` `sar ecx,8` + `0x432DFB` `add ecx,3` | `y = (xy >> 8) + 3` |
| `0x432E07` `and edx,0xFF`, `0x432E0D` `shl edx,3`, `0x432E10` `shl edx` | `w = (wh & 0xFF) << 4` (×16) |
| `0x432E18` `sar eax,8`, `0x432E1B` `imul eax,eax,0xC` | `h = (wh >> 8) * 12` |

**Corrección importante:** el `imul …,0xC` de `0x00432E1B` es la **altura en
píxeles** (alto en unidades de celda × 12), no un *stride* de 12 bytes. De ahí
viene el error de U-1a/U-1b.

### 4.2 Vértices y dibujo

Solo se escriben **tres slots de dos flotantes** (x,y), cada uno de 8 B:

| Slot | Base | Contenido |
|---|---|---|
| V1 | `[ebp-0xC4]` | `(x, y)` |
| V2 | `[ebp-0x3C]` | `(x+w, y)` |
| V3 | `[ebp-0x78]` | `(x, y+h)` |

Antes del segundo triángulo, V1 se **reescribe** con `(x+w, y+h)`
(`0x432EA3..0x432EBB`). Las dos llamadas son:

```
0x432E9E  grDrawTriangle(V1=(x,y),     V2=(x+w,y), V3=(x,y+h))
0x432ED0  grDrawTriangle(V1=(x+w,y+h), V2=(x+w,y), V3=(x,y+h))
```

Estado: `grAlphaBlendFunction(0,0,0,6)`, `grConstantColorValue4(0x43000000 ×3, 0)`,
`grAlphaCombine(1,0,1,2,0)`, `guColorCombineFunction(3)`.
**No se llama a `grTexSource` en toda la función**: el rectángulo es sólido.

### 4.3 Cuestión abierta (registrada, no resuelta)

En esta ruta **no se escribe ningún campo de `GrVertex` salvo x e y**: ni color,
ni `ooz`/`oow`, ni u/v. Los tres punteros pasados a `grDrawTriangle` no están
equiespaciados (`0xC4→0x3C = 0x88`; `0x3C→0x78 = 0x3C`), y en medio de cada slot
quedan huecos de pila (`[ebp-0xC0]`, `[ebp-0x38]`, `[ebp-0x74]` contienen la
segunda coordenada, y el resto no se inicializa). **No se ha establecido** si
Glide 2.x tolera este vértice parcial en el modo `guColorCombineFunction(3)` y
`grAlphaCombine(1,0,1,2,0)`, ni si el juego depende de valores residuales de la
pila. **No se afirma nada al respecto.**

---

## 5. Lista de visualización 2D (`0x0065BFC0`) y sus dos consumidores

Toda la ruta de texto/HUD se alimenta de una lista de registros de **6 bytes**:

```
off 0x00: word  (x o argumento A)
off 0x02: word  (y o argumento B)
off 0x04: byte  estilo
off 0x05: byte  glifo / dato extra
```

* Base `0x0065BFC0`, contador `0x0065CBE0` (descarta el registro cuando el
  contador ya vale `0x1FF`, es decir, caben 511 registros).
* Único emisor observado: `0x00432852`, con firma
  `(byte estilo, int A, int B, byte extra)`; guarda `A & 0xFF` y `B & 0xFF` como
  words y solo escribe el byte `extra` en +5 cuando `(estilo & 0x7F) != 0x40`.

Consumidores:

| Función | Comportamiento |
|---|---|
| `0x00434F18` | `estilo == 0x20` → omite; `estilo == 0x40` → rectángulo; `estilo & 0x80` → omite; resto → `0x0043293F(registro)` |
| `0x00434FE9` | omite `estilo == 0x20` y `estilo == 0x40`; requiere `estilo & 0x80`: llama a `0x0043293F` y después marca `estilo = 0x20` (consumido) |

La lista se vacía en `0x004350E5`, que solo escribe `[0x0065CBE0] = 0`.
Además, `0x00434F18` escribe `[0x0065CBD0]=0x0A` y `[0x0065CBD4]=0x10`, y
`0x00434FE9` escribe `[0x0065CBD8]=0x28` y `[0x0065CBDC]=0x30`; **el significado
de esos cuatro globales no se ha determinado**.

**Hecho relevante para la relación índice→recurso:** en la rama `0x40`,
`0x00434F18` toma como base `rec = 0x0065BFC0 + i*6` y construye

```
arg1 (xy) = word[rec+0] + (word[rec+2] << 8)
arg2 (wh) = word[rec+6] + (word[rec+8] << 8)
```

Es decir, **la ranura actual aporta x,y y la siguiente aporta w,h**;
`arg3 = 0` se empuja literalmente (`0x434FA6`). Después incrementa `i` dos veces
(`0x434FD7` y el `jmp` a `0x434F48`), de modo que un rectángulo consume **dos
ranuras de 6 bytes**. Esto se deduce de las instrucciones, no de una hipótesis
semántica.

En `0x0043293F` (176 B), que sí es el despachador de glifos:

1. `estilo = byte[+4]`; `flag = estilo & 0x80`; `estilo &= 0x7F`.
2. Si `'a' ≤ estilo ≤ 'z'` → `estilo -= 0x20` (normalización a mayúsculas).
3. Si `flag && [0x0057B600] != 0` → `0x004338D7(estilo, x, y, byte[+5])`.
4. En caso contrario → `0x00432BBD((estilo - 0x20) & 0x3F, x, y)`.

`0x004338D7` (5 697 B) es una rama especial que usa el contexto global
`0x0057B600`; **no se ha analizado en U-1f** y queda como cuestión abierta.

---

## 6. Carga del recurso de fuente: `0x004329EF`

Tamaño 462 B. Único llamador: `0x00463705` (inicialización del juego).

```
0x4329F8  strcpy(0x64FD80, 0x64FC80)          ; raíz del juego
0x432A0A  strcat(0x64FD80, "\font.3df")       ; 0x54F86C
0x432A2E  gu3dfGetInfo(0x64FD80, &info @ ebp-0x424)
0x432A78  malloc([ebp-0x8])                   ; 0x464FB3
0x432A8E  gu3dfLoad(0x64FD80, &info)
0x432AFB  grTexTextureMemRequired(3, &0x65BFA0)
0x432B82  grTexDownloadMipMap(0, [0x6487C4], 3, &0x65BFA0)
0x432BAA  [0x6487C4] += tamaño
```

* Copia 4 dwords desde `info+8 … info+0x14` a `0x65BFA0 … 0x65BFAC`, y el puntero
  de datos a `0x65BFB0`.
* `[0x0057B5EC] = [0x006487C4]` (dirección base de la textura de fuente).
* Si el bloque cruza un límite de 2 MB (`and 0xFFE00000`), realinea
  `[0x57B5EC]` al siguiente múltiplo de 2 MB y vuelve a descargar.
* Errores: `"Cannot load font\n"` (`0x54F878`), `"Cannot load font\n%s"` (`0x54F88C`),
  `"Cannot load font 2\n"` (`0x54F8C8`).

Contexto de llamada: `0x00463705` fija antes `[0x646814]=0x100`,
`[0x646818]=0xC0`, `[0x646850]=0x140`, `[0x6474E8]=0xA0` y, justo después,
`[0x6487C8] = [0x6487C4]` (baseline de memoria de textura que se restaura al
cargar nivel en `0x004639E6`).

**Conclusión:** el único recurso de la ruta 2D de texto es `FONT.3DF`
(128×128 según el inventario, rgb332, `.3df` v1.1), cargado como una textura
Glide y referenciado por una dirección global, no por índice.

---

## 7. Corrección explícita de U-1a … U-1e

| Afirmación previa | Estado | Corrección |
|---|---|---|
| «Stride de 12 bytes en `0x00432DD1`» | **FALSA** | `imul …,0xC` en `0x00432E1B` = alto en píxeles. No hay struct de 12 B: el glifo usa stride 0x3C y el rectángulo escribe pares (x,y). |
| «`w = 1.0f` en 2D» | **INCOMPLETA** | El `1.0f` se escribe en `vértice+0x20` (campo de `GrVertex` no verificado); no es un «cálculo de `w`». |
| «`0x00432DD1` = ruta 3D, `w = valor + 8.0f`» | **FALSA** | `0x00432DD1` es un rectángulo 2D sin textura. El `+8` es el tamaño del quad de glifo (`x+8`, `y+8`), no `w`. En `0x00432DD1` no se escribe ni color ni profundidad ni u/v. |
| «`0x00432BBD` y `0x00432DD1` generan geometría del mundo» | **FALSA** | Ambas son HUD/texto 2D. La geometría 3D está en `0x0046558A`. |
| «H1 confirmada: ambas funciones generan geometría» | **RETIRADA** | Generan quads 2D; no geometría 3D. |
| «H6: rutas 2D y 3D comparten estructura» | **RETIRADA** | No comparten estructura ni datos: `0x432BBD` solo lo llama `0x43293F` y `0x432DD1` solo lo llama `0x434F18`. |
| «Los argumentos podrían ser índices a tablas de coordenadas» | **RESUELTA (2D)** | En `0x432BBD` el argumento 1 **sí es un índice**, pero a celdas de una textura, no a una tabla de coordenadas. |

Los documentos `U1E_RESULTADOS.md` y `docs/U1_GEOMETRY_RECOVERY_PLAN.md` se
actualizan para no seguir propagando estas afirmaciones.

---

## 8. Ruta 3D real: `0x0046558A`

Tamaño 17 068 B. Es la única función del binario, aparte de la ruta 2D, que llama
a `grDrawTriangle` (9 sitios: `0x465E50`, `0x466362`, `0x4673CB`, `0x4673DF`,
`0x467893`, `0x467E5B`, `0x4684EA`, `0x4696F8`, `0x46970C`). Único llamador:
`0x00469836`.

### 8.1 Entrada: flujo de comandos, no buffer de vértices

* `[ebp+8]` es un puntero a un **flujo de comandos**.
* `opcode = byte[+0x0B]`.
* Si `opcode == 0xE1`: lee `dword[+8]`, `(valor & 0x9FF) >> 5 & 3`, lo guarda en
  `[0x0057B8EC]`, avanza `+4` y vuelve a leer `opcode` (`0x4655B3..0x4655E3`).
* `opcode & 0xFC` = comando; bits 0, 1 y 2 = flags (`0x4655E6..0x465607`).
* El propio puntero del flujo se reutiliza como descriptor de geometría:
  `[ebp-0x44] = [ebp+8]` en `0x465D39`, y de ahí se leen campos de 16 bits
  (`[+0x12]`, `[+0x14]`, `[+0x16]`) para rellenar los vértices.

### 8.2 Índice de 14 bits → tabla de texturas

En `0x465643` y `0x4656AE` (y de nuevo en `0x46611F`, `0x4670DD`, `0x467B68`,
`0x46812F`):

```
mov  cx, word ptr [registro + 0x1E]   ; o +0x1A en la otra variante
and  ecx, 0x3FFF
mov  edx, [0x006488F4]
mov  eax, [edx + 4*ecx]               ; puntero a registro de textura
```

* **Hecho demostrado:** un campo de 14 bits del flujo de comandos indexa una
  tabla de punteros en `0x006488F4`. Si el puntero es nulo, la rama salta
  (`0x46565E`, `0x4656CE`).
* **Hecho demostrado:** el puntero se usa como `GrTexInfo*` (`grTexSource`,
  `grTexTextureMemRequired`, `grTexDownloadMipMap`), y a continuación la función
  compara `registro[+0x0C]` con `0x0C` y con `0` (`0x465667`, `0x465679`) para
  fijar dos banderas y elegir entre dos llamadas a `grAlphaCombine`:
  `(3,4,1,0,0)` en un caso y `(1,0,0,2,0)` en el otro.
* ~~**Hipótesis no verificada**~~ **RESUELTA (2026-11-11, auditoría
  correctiva de materiales).** Por la disposición de `GrTexInfo`
  (`largeLod,+0`; `smallLod,+4`; `aspectRatio,+8`; `format,+0xC`; `data,+0x10`),
  `+0x0C` es el **formato de textura**, confirmado de dos formas:
  1. el `map7.vfx` del juego tiene los formatos 1 (YIQ_422), 11 (ARGB_1555) y
     12 (ARGB_4444) (`docs/VFX_TEXTURE_FORMAT.md` § 4.2), y
  2. comparando comando a comando los 1123 comandos del frame 0 de MAP7 con el
     estado de Glide que fija el original, las seis texturas que este mezcla
     con alfa (`grAlphaCombine(3,4,1,0,0)`) son **exactamente** las de
     formato **ARGB_4444** (`0x0C`), y las demás son opacas
     (`grAlphaCombine(1,0,0,2,0)`).
  Lo que queda abierto no es el significado del campo, sino la **aritmética
  exacta** de `grAlphaCombine`: los encabezados del SDK Glide 2.x no están en
  el repo, de modo que la equivalencia con la mezcla `SRC_ALPHA`/`INV_SRC_ALPHA`
  que usa el port es la lectura habitual, no una lectura verificada
  (`docs/D3D11_FIRST_SCENE_PLAN.md` § 19.6 y § 19.7).
* **Material localizado para cerrarlo (2026-11-11).** La DLL real disponible en
  el entorno, `C:\GEX_REZ\SMOKE_H4A\glide2x_gex_real.dll`, es una
  **implementación software de Glide 2.x, sin empaquetar**: su
  `grAlphaCombine` (RVA `0x182A` → `0x1004CB90` → `0x10011510`) compara
  los cinco parámetros con el estado cacheado y, cuando cambian, **genera un
  programa** para la unidad de mezcla del Voodoo copiando plantillas de
  operandos elegidas por el valor de cada parámetro (`mov r5, v0`,
  `mul r8, r8, r7`, `lrp r8, r7, r6, r5`, `sub r7, c6, r6.a`, …). Las
  plantillas están en `0x1013D010..0x1013D31C`. Es el material que
  permitiría derivar la ecuación exacta; la derivación no está hecha y
  la ecuación queda **pendiente** (`docs/D3D11_FIRST_SCENE_PLAN.md` § 19.8).
* ~~**Ecuación pendiente**~~ **RESUELTA (2026-11-12).** El material anterior
  era el correcto y la derivación se ha hecho **sobre el driver real**, no
  sobre las plantillas:
  1. **Identidad del driver**: `glide2x_gex_real.dll` es **nGlide 2.10**
     (Zeus Software), una implementación *software* de Glide 2.x sobre D3D9;
     la cadena `Software\Zeus Software\nGlide2` está en `0x1013CA58` y el
     `glide2x.dll` empaquetado le reenvía las exportaciones
     (`glide2x_gex_real._grAlphaCombine@20` en `0xB5F3`).
  2. **Programa generado**: un *harness* i686 carga la DLL, llama a
     `grAlphaCombine` y vuelca el buffer de programa (RVA `0x159250`). Para
     `(3,4,1,0,0)` el programa es `mov r5, c0 / mov r6, v0 / mov r7, r4.a /
     mul r8, r7, r6 / mov r9.a, r8`, o sea
     **`r9.a = r4.a × v0.a` = alfa del texel × alfa del color de vértice**.
  3. **Estado de mezcla**: `grAlphaBlendFunction` traduce sus cuatro factores
     a `D3DRS_SRCBLEND`/`DESTBLEND`/`SRCBLENDALPHA`/`DESTBLENDALPHA`
     (19, 20, 207, 208; constantes `D3DBLEND` verificadas en `d3d9types.h`).
  4. **Llamadas del juego**: desensamblando `GEX3D.EXE` (`0x46558A`) se ve que
     el caso ARGB_4444 se empareja con `grAlphaBlendFunction(1,5,4,0)` →
     **`SRC_ALPHA`/`INV_SRC_ALPHA`, alfa `ONE`/`ZERO`**.

  El port hace exactamente eso (`color.a * t.a` y el estado global): los 32
  triángulos ARGB_4444 **no requieren cambio** y el renderer **no se toca**.
  Detalle, tabla de factores y límites en
  `docs/D3D11_FIRST_SCENE_PLAN.md` § 19.8; pendiente nuevo (el modo de mezcla
  por comando) en § 19.10.

### 8.3 Vértices

Cuatro buffers globales de vértice, separados 0x40:

| Buffer | Dirección |
|---|---|
| v0 | `0x00647580` |
| v1 | `0x006475C0` |
| v2 | `0x00647600` |
| v3 | `0x00647640` |

Se rellenan desde los campos de 16 bits del flujo (por ejemplo
`0x465D8A..0x465DCF` escribe `[0x6475C4]`, `[0x647600]`, `[0x647604]`; y
`0x4669B7`, `0x4672CC`, `0x469643` escriben `[0x647640]`). Las llamadas son
`grDrawTriangle(0x647580, 0x6475C0, 0x647600)` (`0x465E50`, …) y
`grDrawTriangle(0x647580, 0x647600, 0x647640)` (`0x46970C`), que forman un quad
(v0,v1,v2) + (v0,v2,v3).

**No hay index buffer ni vertex buffer externo**: los datos vienen *inline* en
el flujo de comandos y en buffers globales fijos, no en arrays por objeto.

### 8.4 Conjunto Glide real de la función

`analysis/EU/calls.tsv` da **64 llamadas a thunks** desde `0x0046558A`
(63 a Glide y 1 a la CRT):

| Thunk | Import | Nº |
|---|---|---:|
| `0x00540526` | `guColorCombineFunction@4` | 11 |
| `0x00540538` | `grConstantColorValue4@16` | 10 |
| `0x0054051A` | `grDrawTriangle@12` | 9 |
| `0x0054052C` | `grAlphaBlendFunction@16` | 9 |
| `0x00540532` | `grAlphaCombine@20` | 7 |
| `0x005405B6` | `grDrawLine@8` | 6 |
| `0x00540520` | `grTexSource@16` | 4 |
| `0x005405C2` | `grTexDownloadTable@12` | 4 |
| `0x0054057A` | `grChromakeyMode@4` | 2 |
| `0x005405BC` | `grDrawPlanarPolygonVertexList@8` | 1 |
| `0x005407A0` | (CRT, no Glide) | 1 |

**Corrección de un resultado negativo previo:** `grDrawPlanarPolygonVertexList`
**sí se usa**, y precisamente dentro de esta ruta 3D: `0x466B32` empuja `4` y
`lea eax,[ebp-0x138]` y llama al thunk `0x005405BC` en `0x466B34`, es decir
`grDrawPlanarPolygonVertexList(4, &[ebp-0x138])`. El array local
`[ebp-0x138]` es un cuarto formato de vértice (el «planar polygon» de Glide), y
**no se ha determinado su layout** en U-1f. La función también emite
`grDrawLine` (6 sitios) y descarga tablas de paleta con `grTexDownloadTable`
(4 sitios); ambas cosas quedan sin analizar.

**Argumentos de las llamadas de estado (2026-11-12).** Los argumentos
exactos de los 9 `grAlphaBlendFunction` y los 7 `grAlphaCombine` de esta ruta
se leen desensamblando el `push` de cada llamada en `GEX3D.EXE` (no están en
el thunk):

| Llamada | Argumentos | Dónde |
|---|---|---|
| `grAlphaCombine` | `(3,4,1,0,0)` | `0x465696` (formato ARGB_4444) |
| `grAlphaCombine` | `(3,4,1,2,0)` | `0x465706` (segunda ruta texturizada) |
| `grAlphaCombine` | `(1,0,0,2,0)` | `0x4656A7`, `0x465732` (opaco) |
| `grAlphaCombine` | `(1,0,1,2,0)` | `0x465717`, `0x465743`, `0x465754` |
| `grAlphaBlendFunction` | `(1,5,4,0)` | `0x465A3D` (**mezcla alfa**) |
| `grAlphaBlendFunction` | `(4,0,0,0)` | `0x4657BC` (substituye) |
| `grAlphaBlendFunction` | `(4,4,0,0)` | `0x465831`, `0x465881` (aditivo) |
| `grAlphaBlendFunction` | `(0,0,6,0)` | `0x465859`, `0x4659CA` (borra) |
| `grAlphaBlendFunction` | `(1,1,4,0)` | `0x4658A3`, `0x4659F2` |
| `grAlphaBlendFunction` | `(1,4,4,0)` | `0x465A14` |
| `guColorCombineFunction` | `2`, `3`, `6`, `7` | 11 sitios (`0x465E33` … `0x4696DC`) |

Con la tabla de factores del driver (§ 8.2) el caso ARGB_4444 queda
`SRC_ALPHA`/`INV_SRC_ALPHA` con alfa `ONE`/`ZERO`. Detalle en
`docs/D3D11_FIRST_SCENE_PLAN.md` § 19.8.

---

## 9. Construcción de la tabla `0x006488F4` desde `.VFX`

### 9.1 Lector de fichero `0x00461B90`

```
fopen(ruta, "rb")            ; 0x5515CC = "rb"
fseek/ftell → tamaño
malloc(tamaño)               ; 0x464FB3
fread
fclose
si falla → "Unable to load %s." (0x5515D0)
```

### 9.2 Constructor de tabla `0x00461C32`

```
info  = 0x461B90(ruta)
count = info[0]                      ; primer dword = número de texturas
tabla = malloc(count * 4)            ; → [0x006488F4]
info += 4                            ; primer registro
for i in 0..count-1:
    tabla[i] = info                  ; el registro ES un GrTexInfo
    tabla[i][+0x10] = tabla[i] + 0x8C        ; campo `data` de GrTexInfo
    siguiente = tabla[i] + 0x8C + tabla[i][+0x84]
    grTexTextureMemRequired(3, tabla[i])
    grTexDownloadMipMap(0, [0x6487C4], 3, tabla[i])
```

Por `analysis/EU/calls.tsv`, `0x00461C32` llama a `grTexTextureMemRequired` y
`grTexDownloadMipMap`: la tabla es, por tanto, una **tabla de texturas cargadas**,
no de geometría.

Hechos adicionales leídos en el código:

* el bucle hace `grTexTextureMemRequired(3, tabla[i])` y
  `grTexDownloadMipMap(0, [0x6487C4], 3, tabla[i])`, es decir trata cada registro
  como un `GrTexInfo` completo y lo descarga al bloque de memoria de textura
  gestionado con el puntero `[0x6487C4]` (el mismo que usa `FONT.3DF`);
* el registro apunta a sus datos con `+0x10 = registro + 0x8C`, y el tamaño del
  bloque se obtiene con `+0x84` para avanzar al registro siguiente. **No se ha
  verificado** qué ocupa los 0x8C bytes de cabecera/cola ni el significado de
  `+0x84`;
* hay realineado a múltiplos de 2 MB (`and 0xFFE00000`) igual que en la ruta de
  fuente.

### 9.3 Quién llama y con qué fichero

`0x0043B11D(dfx, vfx, nivel)` es el cargador de nivel:

| Dirección | Acción |
|---|---|
| `0x43B287` | `0x00461C32([ebp+0xC])` → **`.VFX`** construye la tabla `0x006488F4` |
| `0x43B298` | `0x00461E0B([ebp+8])` → **`.DFX`** carga el flujo de nivel |

`0x00461E0B` lee el `.DFX` con `0x00461B90` y llama a `0x004735C0`, que recorre
`count` dwords y suma una base a `dst[valor >> 2]` (relocalización de una tabla de
offsets). Después libera el buffer original con `0x0046506B`. **No se ha
determinado el formato exacto de cabecera del `.DFX`/`.VFX`.**

### 9.4 Nombres de fichero (hecho)

| Constante | Cadena | Uso |
|---|---|---|
| `0x54FAB4` | `"%s\\audio\\%s.tad"` | `0x43E902` → nivel+0x4D48 |
| `0x54FAC4` | `"%s\\level\\%s"` | `0x43E91D` |
| `0x54FAD0` | `"%s.dfx"` | `0x43E943` → nivel+0x4CA8 |
| `0x54FAD8` | `"%s.vfx"` | `0x43E963` → nivel+0x4CF8 |
| `0x5518F4` / `0x551900` / `0x551914` | `…\map7.dfx` / `…\map7.vfx` | inicialización `0x463380..0x463432` |

El nombre base del nivel está en `nivel+0x4D98` y se genera desde `"%s\\level\\%s"`.

---

## 10. Contraste 2D vs 3D

| Aspecto | 2D texto (`0x432BBD`) | 2D rect (`0x432DD1`) | 3D (`0x46558A`) |
|---|---|---|---|
| Entrada | glifo + x,y | x,y,w,h empaquetados | flujo de comandos |
| Recurso | `FONT.3DF` (global) | ninguno | tabla `0x6488F4` (`.VFX`) |
| Índice | glifo → celda 16×16 | no | 14 bits → tabla de texturas |
| Vértices | 4, stride 0x3C | 3 slots (x,y); uno se reescribe | 4 globales, separados 0x40 |
| Textura | 1 fija | no | sí, por índice |
| Origen del dato | lista `0x65BFC0` | lista `0x65BFC0` | `.DFX` |
| Relación con DFX/VFX | **ninguna** | **ninguna** | DFX = flujo; VFX = texturas |

---

## 11. Resultados negativos (explícitos)

1. **No existe** en `0x00432BBD` ni en `0x00432DD1` ninguna referencia a `.DFX`,
   `.VFX`, `0x6488F4`, `0x6487C4` o a un buffer de vértices.
2. **No existe** una tabla de coordenadas indexada por los argumentos de
   `0x00432BBD`: el índice se resuelve aritméticamente contra la rejilla de la
   textura.
3. **No existe** un índice de dibujo (*draw index*) en ninguna de las dos rutas:
   los triángulos se emiten con punteros directos a vértices en pila.
4. **No se ha encontrado** ningún camino por el que los argumentos 2D alimenten
   la geometría 3D. Las listas de llamadores son disjuntas.
5. **No se ha establecido** el layout del vértice de
   `grDrawPlanarPolygonVertexList` ni el de la ruta de rectángulo, ni el
   significado de `registro[+0x0C]`, ni el formato de `.DFX`/`.VFX`.

> **Corrección dentro de esta misma ronda:** una comprobación inicial de U-1f
> dio por no usada `grDrawPlanarPolygonVertexList`. Se usa una sola vez, dentro
> de `0x0046558A` (`0x466B34`, 4 vértices desde `[ebp-0x138]`), y **no** aparece
> en las rutas 2D. Ver §8.4.

---

## 12. Cuestiones abiertas

1. **`0x004338D7`** (rama `estilo & 0x80`, contexto `0x0057B600`): 5 697 B sin
   analizar. Podría ser el renderizador de menús/consola con recursos propios.
2. **Estructura de vértice real** de la ruta de rectángulo: solo se escriben
   `(x,y)` por slot y los punteros no son equiespaciados. Requiere los headers de
   Glide 2.x para saber qué lee `grDrawTriangle` y si el vértice parcial es válido.
3. **Significado de `registro[+0x0C] == 0x0C` / `== 0`** en la ruta 3D:
   hipótesis pendiente de contrastar con los headers de Glide 2.x — sería el
   campo `format` del `GrTexInfo` (`GR_TEXFMT_AP_88` frente a `GR_TEXFMT_RGB_332`).
4. **Formato de cabecera** de `.DFX` y `.VFX`: se sabe que el primer dword es un
   contador y que `0x4735C0` relocaliza offsets, pero no el resto del formato.
5. **`0x57B600`**: puntero a contexto de UI; su estructura no se ha determinado.
6. **Layout del vértice de `grDrawPlanarPolygonVertexList`** usado en
   `0x466B32` (4 vértices desde `[ebp-0x138]`).
7. **Uso de `grDrawLine` y `grTexDownloadTable`** dentro de `0x0046558A`
   (6 y 4 sitios): no analizados.

---

## 13. Relación con las fases siguientes

* **U-1g (relación con DFX)**: U-1f deja el camino preparado. Ya está demostrado
  que el `.DFX` alimenta el flujo de comandos de `0x0046558A` y que el `.VFX`
  construye la tabla de texturas `0x6488F4`. U-1g debería atacar el **formato del
  flujo de comandos** (opcodes, tamaño de registro, campos de 16 bits) y la
  cabecera del `.DFX`.
* **U-1h (interpretación final)**: solo podrá cerrarse cuando U-1g determine la
  semántica de los campos del flujo.

**No se avanza a U-1g en esta ronda.**

---

## 14. Documentación actualizada en esta ronda

* `U1F_RESULTADOS.md` (este documento).
* `docs/U1_GEOMETRY_RECOVERY_PLAN.md` — corregido §U-1a…U-1e y marcado U-1f.
* `ROADMAP.md` — U-1f marcado como completado.
* `CHANGELOG.md` — entrada U-1f y corrección de las afirmaciones falsas.
* `PROJECT_STATE.md` — estado vigente de U-1.
* `docs/SUBSYSTEM_SEGMENTS.md` y `docs/RENDERER_ARCHITECTURE_RESEARCH.md` —
  descripción de `0x00432BBD` / `0x00432DD1` corregida a HUD/texto 2D.

`PROJECT_STATUS.svg` **no cambia**: no hay evolución de MAIN ni hito verificado
del panel; esto es investigación estática en `dev/gex`.
