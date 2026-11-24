# S-14 — El «fondo 2D» del menú de MAP7 es un anillo de 16 paneles de cielo

> **Sesión:** S-14 (plan de escena D3D11) · **Fecha:** 2026-11-24 ·
> **Rama:** `dev/gex` · **Alcance:** investigación estática con REA + Ghidra.
> **Nada de este informe se ha implementado en el port.** No se ha modificado
> ningún ejecutable, asset ni fichero de `C:\GEX_REZ`.

Este informe **corrige** la hipótesis de trabajo de S-11/S-13 («el cielo del
menú es una textura estirada del grupo `331..333`/`342`; falta el emisor») y la
sustituye por la cadena real, localizada y verificada en el ejecutable original.

---

## 1. Resumen ejecutivo

1. **El fondo del menú no es un fondo 2D.** Es un **anillo de 16 paneles
   texturados** colocados alrededor del centro del nivel, con 48 triángulos y 35
   vértices cada uno (768 triángulos en total). Se dibujan todos en cada frame;
   no hay animación ni selección de fotograma.
2. Los paneles están en el propio `.DFX` del nivel, en un **banco de 16
   registros de 24 B** en `image + 0x45CC`, referenciado por la **cabecera de
   nivel** en `+0x20` (cuenta = 16) y `+0x24` (puntero). No son geometría del
   BSP: es una **lista BG independiente**.
3. El **emisor original** está localizado y decompilado:
   `FUN_0043DD7A` (frame) → `FUN_0040172B` (pase BG, culling + transformación)
   → `FUN_004013A0` (emite los 48 triángulos de cada panel como registros
   `0x24` de la lista de comandos 2D) → `FUN_004659DA` (rasterizador Glide).
4. Las **41 texturas** `325..365` se usan **todas**, en ventanas de 4×6 celdas
   por panel. El **sol** está en el bloque `350..363` de los paneles que miran a
   `+X` (paneles 8 y 4, ángulos −4,6° y 13,1°).
5. `hideBG`/`unhideBG`/`hideBGObject` actúan sobre el **bit 0** del primer `u16`
   de cada registro: es el flag de ocultación que comprueba el pase BG.

---

## 2. Entorno y método

| Elemento | Estado verificado |
|---|---|
| Host de análisis | **WSL2 Ubuntu** (PE32 x86 admitido por Ghidra sin la restricción P0 de Windows) |
| JDK | 21.0.12.1 (`/usr/lib/jvm/java-21-openjdk-amd64`) |
| Ghidra | **12.1.4 PUBLIC**, SHA-256 `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db` |
| REA | **6.7.0**; `rea doctor --provider ghidra` → **`healthy:true`** |
| Binario | copia de `GEX3D.EXE`, SHA-256 `7a9b68516867db5460b6ec0d152841742192b1bb47198d2c7b0357d7aa3e8254` (1 559 040 B) |
| Datos | `MAP7.DFX` (1 030 848 B) y `MAP7.VFX`, leídos **en copia**, sin modificar |

Detalle de la instalación y de los límites, en
[`docs/REA_WINDOWS_ENABLEMENT.md`](../../REA_WINDOWS_ENABLEMENT.md) §8.

Método: `rea inspect` (PE32 x86, `image_base 0x400000`), `rea decompile` por
dirección y volcado estático del `.DFX` con el contrato ya documentado
(`GEX_POLYGON_SIZE`, tabla de reubicaciones, `ALIGN_MASK 0x1FF`). Las
direcciones son **VA** (RVA + `0x400000`); los offsets del `.DFX` son
**offsets de imagen** (imagen = fichero a partir de `0x19800`).

---

## 3. Dónde está el cielo en el `.DFX`

La cabecera de nivel es el inicio de la imagen (`image + 0x0`) y contiene pares
`(cuenta, puntero)`:

| Offset | Valor | Interpretación |
|---|---|---|
| `+0x10` / `+0x14` | `3` / `0x694` | lista auxiliar (3 registros) |
| `+0x18` / `+0x1C` | `2` / `0x70C` | lista auxiliar (2 registros) |
| **`+0x20` / `+0x24`** | **`16` / `0x45CC`** | **paneles de cielo (BG)** |
| `+0x78` / `+0x7C` | `151` / `0x7CCC` | objetos del nivel (151 × 48 B) |

Coincidencia independiente: `+0x78 = 151` y `+0x7C = 0x7CCC` es exactamente la
tabla de objetos ya identificada en S-13. El par `+0x20/+0x24` sigue el mismo
esquema, así que la lista de 16 paneles es una lista de primer nivel del nivel,
**no** un objeto colocado.

### 3.1 Registro de panel (24 B)

| Offset | Tipo | Valor observado | Significado |
|---|---|---|---|
| `+0x00` | `u16` | `0` | flags; **bit 0 = oculto** |
| `+0x02` | `u16` | `48` | nº de polígonos |
| `+0x04` | `u32` | `0x474C`, … | tabla de vértices (8 B/vértice) |
| `+0x08` | `u32` | `0x4864`, … | tabla de polígonos (12 B/polígono) |
| `+0x0C` | `s16` | p. ej. `12114` | posición X del panel |
| `+0x0E` | `s16` | `−11000` | posición Y |
| `+0x10` | `s16` | `1085` | posición Z |
| `+0x12` | `u16` | `8765` | **radio** (culling) |
| `+0x14` | `u16` | `35` | nº de vértices |
| `+0x16` | `u16` | `0` | relleno |

Verificado en los 16 registros: `polys − verts = 280 = 35 × 8` y
`polys + 48 × 12 = verts` del registro siguiente (bloques contiguos de 856 B).
El registro 17 leído con stride 24 cae ya en la tabla de vértices del panel 0:
**el banco tiene exactamente 16 entradas**.

### 3.2 Vértices y polígonos

- **Vértice (8 B):** `{s16 x, s16 y, s16 z, u16 relleno}`.
- **Polígono (12 B):** `{u16 v0, u16 v1, u16 v2, u16 flags, u32 uv_ptr}`.
  `flags = 0` en los 768 triángulos.
- La rejilla es **5 columnas × 7 filas = 35 vértices** con `v = fila*5 + col`
  (fila 0 abajo). Cada par de triángulos consecutivos forma un quad:
  **4 × 6 = 24 quads = 48 triángulos** por panel.

### 3.3 Registro UV (12 B) y textura

12 B por triángulo, copiados tal cual al comando `0x24`:

| Offset | Tipo | Contenido |
|---|---|---|
| `+0x00` | `u8 u0` | U del vértice 0 (0..255) |
| `+0x01` | `u8 v0` | V del vértice 0 |
| `+0x02` | `u16` | auxiliar: `3` o `4` |
| `+0x04` | `u8 u1` | U del vértice 1 |
| `+0x05` | `u8 v1` | V del vértice 1 |
| **`+0x06`** | **`u16`** | **índice de textura (325..365)** |
| `+0x08` | `u8 u2` | U del vértice 2 |
| `+0x09` | `u8 v2` | V del vértice 2 |
| `+0x0A` | `u16` | auxiliar: `31` siempre |

Ejemplo (panel 0, triángulo 0): `ff 00 04 00 | ff ff 45 01 | 00 ff 1f 00` →
`(u,v) = (255,0) (255,255) (0,255)`, textura `0x0145 = 325`.

Confirmado en el rasterizador: el caso de quad `0x2C`
(`0x467502..0x467834`) lee `word[cmd + 0x1A] & 0x3FFF` como textura
(`0x467523..0x467527`) y las UV como **bytes** (`+0x10/+0x11`, `+0x18/+0x19`,
`+0x20/+0x21`). Para un triángulo `0x24`, `+0x1A` es la mitad alta del segundo
dword UV, es decir el `u16 +0x06` del registro UV. Los dos `u16` auxiliares
(`+0x02`, `+0x0A`) **no** participan en el muestreo.

---

## 4. Tabla de colocación (los 16 paneles)

Ordenada por ángulo alrededor del centro del nivel `(−4364, −3239)`.
«Panel» es el índice en el banco (orden de emisión).

| Panel | Ángulo | Posición (x, y, z) | Radio | Texturas distintas |
|---:|---:|---|---:|---:|
| 10 | −157,4° | (−15386, −7060, 1129) | 8762 | 5 |
| 9 | −125,2° | (−11595, −12289, 1129) | 8763 | 5 |
| 3 | −93,5° | (−5925, −15562, 1144) | 8871 | 5 |
| 2 | −66,6° | (687, −16485, 1085) | 8765 | 8 |
| 1 | −43,7° | (6946, −14879, 1125) | 8765 | 13 |
| 0 | −23,3° | (12114, −11000, 1085) | 8765 | 13 |
| **8** | **−4,6°** | **(15807, −4105, 1092)** | 8748 | **18** |
| **4** | **13,1°** | **(16313, 905, 1090)** | 8758 | **18** |
| 5 | 30,4° | (14069, 8417, 1084) | 8765 | 13 |
| 6 | 47,5° | (9778, 13248, 1125) | 8765 | 11 |
| 11 | 58,9° | (−16886, −765, 1126) | 8769 | 5 |
| 7 | 64,9° | (3965, 16069, 1085) | 8765 | 8 |
| 15 | 82,7° | (−1067, 16580, 1128) | 8764 | 5 |
| 14 | 101,6° | (−7313, 14990, 1141) | 8747 | 5 |
| 13 | 122,1° | (−12495, 11160, 1133) | 8761 | 5 |
| 12 | 145,2° | (−15865, 5641, 1130) | 8768 | 5 |

- Radio de anclaje **8 747..8 871** (≈ 8 765): es un **cilindro** alrededor del
  nivel, con separación angular de ~18–22°.
- Altura: cada panel abarca `z ∈ [−4516, 7286]`; el anclaje está a
  `z ≈ 1 100`. El cilindro sube por encima del nivel y baja por debajo del
  horizonte.
- Las 41 texturas se usan **todas**; ningún índice queda sin asignar.

### 4.1 Rejilla 4×6 de texturas (fila 5 = arriba)

Nueve paneles se resuelven con la rejilla `v = fila*5 + col` sin ambigüedad:

```
panel 0 (−23,3°)          panel 8 (−4,6°)  ← sol      panel 1 (−43,7°)
327 327 327 327           327 327 362 363             327 327 327 327
334 335 336 337           358 359 360 361             341 342 343 344
330 331 332 333           354 355 356 357             328 338 339 340
325 325 328 329           350 351 352 353             365 364 325 325
325 325 326 326           326 326 325 349             345 325 345 325
325 325 325 325           325 325 325 326             325 325 325 325

panel 2 (−66,6°)          panel 4 (13,1°) ← sol       panel 5 (30,4°)
327 327 327 327           327 327 362 363             327 327 327 327
326 346 347 348           358 359 360 361             334 335 336 337
364 364 364 325           354 355 356 357             330 331 332 333
364 364 364 364           350 351 352 353             325 325 328 329
325 325 325 345           325 326 326 349             364 364 325 325
325 325 325 325           325 325 325 326             325 325 325 325
```

Nueve paneles se resuelven sin ambigüedad con `v = fila*5 + col`
(**0, 1, 2, 4, 5, 6, 7, 8, 9**); arriba se muestran seis de ellos. Los otros
siete (**3, 10, 11, 12, 13, 14, 15**) tienen una topología de vértices distinta:
con el mapeo fila-mayor solo se resuelven 12–15 de las 24 celdas (el resto son
quads que caen en celdas ya ocupadas). Su conjunto de triángulos, UV y texturas
es exacto y está en la tabla §4; **el orden de celdas de esos siete queda
abierto** (§11).

**Sol.** El bloque `350..363` aparece solo en los paneles 8 y 4, que miran a
`+X`. `358..361` es el disco solar; `362/363` y `327` son las bandas verde
oscuras del horizonte. Es el sol del atardecer que se ve en la captura del
menú.

---

## 5. La cadena de emisión en código (CONFIRMADO)

### 5.1 `FUN_0043DD7A` — renderizador de frame

Registra el pase BG en un puntero de la estructura de frame y lo invoca:

```c
*(code **)(param_1 + 0x4c98) = FUN_004259fd;
*(code **)(param_1 + 0x4c9c) = FUN_00425150;
*(code **)(param_1 + 0x4ca0) = FUN_00426b02;   /* emisor de escena */
*(code **)(param_1 + 0x4ca4) = FUN_0040172b;   /* pase BG */
...
(**(code **)(param_1 + 0x4ca4))(piVar1, uVar6, *(undefined4 *)(param_1 + 0x24), puVar5);
```

`piVar1 = *(int **)(param_1 + 4)` es la cabecera de nivel;
`puVar5 = &DAT_00650080 + frame * 0x6000` es el buffer de comandos 2D.

### 5.2 `FUN_0040172B` — pase de fondo

```c
iVar1 = *(int *)(param_1 + 0x20);            /* 16 */
iVar2 = *(int *)(param_1 + 0x24);            /* 0x45CC */
for (local_8 = *(ushort **)(param_1 + 0x24);
     local_8 < (ushort *)(iVar2 + iVar1 * 0x18);
     local_8 = local_8 + 0xc) {              /* 24 B por panel */
  if ((*local_8 & 1) == 0) {                 /* bit 0 = oculto */
    ... DAT_006469a0 = local_8[6];           /* +0x0C: x */
    ... DAT_006469a2 = local_8[7];           /* +0x0E: y */
    ... DAT_006469a4 = local_8[8];           /* +0x10: z */
    FUN_00469dec();                          /* fija matriz del panel */
    ... culling con -(short)local_8[9]       /* +0x12: radio */
    FUN_004013a0(*(param_2 + 0x70), local_8, param_3, param_4);
  }
}
```

### 5.3 `FUN_004013A0` — emisor de un panel

```c
sVar4 = *(short *)(param_2 + 2);             /* 48 polígonos */
iVar5 = *(int *)(param_2 + 8);               /* tabla de polígonos */
for (local_1c = *(short **)(param_2 + 8);
     local_1c < (short *)(iVar5 + sVar4 * 0xc);
     local_1c = local_1c + 6) {              /* 12 B por polígono */
  puVar1 = (undefined2 *)(*(int *)(param_2 + 4) + *local_1c * 8);
  ...
  FUN_0046a620();                            /* proyecta */
  if (... clipping ...) {
    if (*(int *)(local_1c + 4) != 0) {       /* +0x08: uv_ptr */
      local_8[3] = uVar6;  local_8[5] = uVar8;  local_8[7] = uVar7;   /* x,y */
      local_8[4] = **(undefined4 **)(local_1c + 4);                   /* uv0 */
      local_8[6] = *(undefined4 *)(*(int *)(local_1c + 4) + 4);       /* uv1 */
      local_8[8] = *(undefined4 *)(*(int *)(local_1c + 4) + 8);       /* uv2 */
      local_8[2] = 0x25808080;
      *local_8 = 0x24;                       /* opcode: triángulo texturado */
      *(undefined4 **)(param_4 + 0x5ffc) = local_8;
      local_8 = local_8 + 9;
    }
  }
}
```

Es decir: **cada triángulo del panel se convierte en un comando `0x24` de
36 B** (opcode, enlace, color, 3 × (x,y) + 3 × (uv,textura)), encadenado en el
buffer del frame.

### 5.4 `FUN_004659DA` — rasterizador

El dispatcher de la lista de comandos consume `0x24`/`0x2C` (comprobado en el
descompilado de `FUN_004659DA`: dos ramas `else if (local_30 == 0x24)` /
`else if (local_30 == 0x2c)`), resuelve la textura con
`grTexSource(0, texinfo->0x88, 3, texinfo)` y dibuja con `grDrawTriangle`:
`grTexSource` en `0x467563` (thunk `0x5409A6`), `grDrawTriangle` en `0x46781B` y
`0x46782F` (thunk `0x5409A0`; caso quad `0x2C`, dos triángulos). El triángulo
`0x24` sigue el mismo camino con un solo `grDrawTriangle`.

---

## 6. Visibilidad: `hideBG` / `unhideBG` / `hideBGObject`

| Handler | VA | Efecto |
|---|---|---|
| `hideBG` | `0x535899` | recorre los 16 registros y hace `*u16 |= 1` |
| `unhideBG` | `0x5358F4` | `*u16 &= ~1` |
| `hideBGObject` | `0x53594F` | `*u16 |= 1` sobre **un** registro (`param_2+4`) |
| `unhideBGObject` | `0x53596E` | `*u16 &= ~1` sobre uno |

El pase BG dibuja solo si `(*local_8 & 1) == 0`: **bit 0 = oculto**. Esto cierra
la pregunta de S-13 sobre quién apaga el cielo: es el propio sistema BG del
mapa, no el BSP.

`FUN_005357A8` recorre la **otra** lista BG (`+0x78`/`+0x7C`, registros de
48 B) y limpia `0x100` en `+0x1C` y bits en el sub-objeto; es el pase de
objetos BG, no el del cielo.

---

## 7. Estrellas

- El **sol** es geometría de cielo con textura: bloque `350..363` (§4.1).
- Las **estrellas** visibles en la captura del menú **no** corresponden a una
  malla colocada: `starz___` (clase `0x0C1F7C`, modelo de 60 vértices y 48
  polígonos, **sin textura**, radio 105) existe en el `.DFX` pero **no figura**
  en la tabla de objetos `0x7CCC`, así que no está colocado en el nivel.
  Quedan como puntos claros dentro de los tiles de cielo; su origen exacto
  (baked en textura vs. malla no colocada) queda **abierto**.

---

## 8. CONFIRMADO vs. HIPÓTESIS

| Afirmación | Estado | Evidencia |
|---|---|---|
| El cielo del menú son 16 paneles de 48 triángulos en `image+0x45CC` | **CONFIRMADO** | cabecera `+0x20/+0x24`; 16 registros coherentes; datos del `.DFX` |
| No es geometría del BSP | **CONFIRMADO** | los 16 292 polígonos del BSP no usan 325..365 (S-13/S-14) |
| `FUN_0040172B` es el pase BG y `FUN_004013A0` el emisor por panel | **CONFIRMADO** | descompilado + registro en `+0x4CA4` de `FUN_0043DD7A` |
| Cada triángulo se emite como comando `0x24` de 36 B | **CONFIRMADO** | `*local_8 = 0x24`, 9 dwords, `uv_ptr` copiado |
| Textura en `u16 +0x06` del registro UV | **CONFIRMADO** | 41 valores 325..365 en 768 triángulos; rasterizador lee `+0x1A` |
| UV = bytes (u,v) en `+0x00/+0x04/+0x08` | **CONFIRMADO** | esquinas 0/255; lectura byte a byte en `0x4675A8..0x467772` |
| Los paneles forman un cilindro de radio ≈ 8 765 | **CONFIRMADO** (datos) | 16 anclajes + radios 8747..8871 |
| No hay animación por fotograma | **CONFIRMADO** | los 16 paneles se dibujan siempre; no hay índice de frame |
| El bloque `350..363` es el sol | **CONFIRMADO** (textura) | mosaico reconstruido + captura del menú |
| Significado de los `u16` auxiliares `+0x02` (3/4) y `+0x0A` (31) | **HIPÓTESIS** | constantes; no los lee el rasterizador |
| Origen de las estrellas (¿baked?) | **ABIERTO** | `starz___` no está colocado |
| Orden de celdas en 7 paneles con topología distinta | **ABIERTO** | 15/24 celdas resueltas con el mapeo fila-mayor |

---

## 9. Consecuencias para el port (NO implementado)

- El port **no necesita** un «fondo 2D» para el cielo: necesita **16 paneles
  con 48 triángulos** y un muestreo por triángulo. Encaja en la capa
  `gex_comp2d` como 24 quads texturados por panel (o 48 triángulos), sin tocar
  la geometría 3D.
- El degradado de S-13 (`gex_comp2d_vertical_gradient`) queda como
  aproximación **sustituible**: la secuencia real de texturas por panel está en
  §4.1 y el sol está en `350..363`.
- La reconstrucción del panorama (montaje 1024×96 con las 41 texturas) se
  generó **localmente** como evidencia y **no se sube al repo** (deriva de
  assets originales). Ruta local:
  `%TEMP%\s14_sky_panorama.png`; script: `%TEMP%\s14_pano3.py`.

---

## 10. Evidencia reproducible

| Artefacto | Ruta local (fuera del repo) |
|---|---|
| Desensamblado completo | `%TEMP%\s13_dis.txt` |
| Descompilados | `~/.rezengineered-rea/work/dec_0x4013a0.md`, `dec_0x40172b.md`, `dec_0x43dd7a.md`, `dec_0x4659da.md` |
| Volcado de paneles/UV | `%TEMP%\s14_table2.py`, `s14_uvraw.py`, `s14_pano3.py` |
| Panorama reconstruido | `%TEMP%\s14_sky_panorama.png` |
| Copia analizada | `~/.rezengineered-rea/work/GEX3D.EXE` (SHA-256 en §2) |

Comandos:

```
rea decompile GEX3D.EXE 0x40172b --provider ghidra --format md
rea decompile GEX3D.EXE 0x4013a0 --provider ghidra --format md
rea decompile GEX3D.EXE 0x43dd7a --provider ghidra --format md
```

---

## 11. Próximo paso técnico

1. Resolver el orden de celdas de los 7 paneles con topología distinta
   (comparar sus 35 vértices con los de un panel completo; probablemente son
   tiras plegadas o reflejadas).
2. Implementar en `gex_comp2d` los 24 quads por panel con las texturas de §4.1
   y validar contra la captura del menú (pose de reposo medida en S-11).
3. Confirmar si las estrellas están *baked* en los tiles o si `starz___` se
   dibuja por otra lista (no está en `0x7CCC`).
