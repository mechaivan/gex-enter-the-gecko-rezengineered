# U-1 — CONSOLIDACIÓN DOCUMENTAL FINAL (2026-11-07)

**Objeto:** Subfase C — Recuperación de geometría original (U-1a…U-1h).

**Binario de referencia:** `C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE`
(PE32 i386, 1 557 504 B, EU v1.00.000; `imageBase = 0x00400000`).

**Assets contrastados (solo lectura):** `LEVEL\AZTEC16.DFX`, `LEVEL\CIRCUIT0.DFX`,
`LEVEL\REZOP2.DFX`, `GEX.DFX` y `GEX.VFX`.

**Modo:** análisis estático *read-only*. No se ha ejecutado el juego, no se ha
abierto ninguna ventana, no se ha modificado el ejecutable ni ningún fichero de
`C:\GEX_REZ`, no se ha modificado código del port ni assets, y no se ha
instalado ninguna herramienta. `PROJECT_STATUS.svg` no se toca.

---

## 0. Resumen ejecutivo

1. **U-1 queda cerrada documentalmente**, pero «cerrada» no significa «todo
   identificado». La consolidación separa de forma explícita tres ámbitos que
   las rondas anteriores mezclaban a veces:
   * **geometría BSP de nivel** — demostrada de extremo a extremo;
   * **geometría de modelos** (p. ej. `GEX.DFX`) — demostrado que *no* es la de
     nivel, **no descodificada**;
   * **formatos de comandos** — handlers y layouts demostrados, con **dos
     emisores y un registro todavía abiertos**.

2. **Se corrige la afirmación de U-1h §0.2** de que todos los campos del
   descriptor de nivel estaban «identificados al 100 %». El estado real es:
   **10 campos con semántica demostrada**, **1 hipótesis** (`+0x38`),
   **3 sin identificar** (`+0x04`, `+0x08`, `+0x0C`) y **2 parciales o sin
   consumidor** (`+0x34`, `+0x3C`). Ningún encadenado de regiones se ve
   afectado: las tres igualdades de §3.2 de U-1h siguen verificadas.

3. **Se corrige la descripción de los emisores de modelo.** `0x00425010` usa
   vértices de **16 B** (índice `<<4`), pero `0x004258BD` usa una tabla de
   vértices de **12 B** (`imul …,0xC`). U-1h §7.3 y §12 los presentaban con el
   mismo stride.

4. **El opcode `0x28` sí tiene emisores localizados**: `0x0042DEC1` y
   `0x00504C44` escriben registros de `0x1C` B (color + 4 pares `xy`). Lo que
   sigue abierto es el **layout exacto de las cuatro estructuras** que el
   handler `0x0046636C` pasa a `grDrawPlanarPolygonVertexList`.

5. **El opcode `0x2C` sigue sin emisor localizado.** Su handler
   (`0x00466B3E`), su longitud (`0x2C`) y su layout de cuadrilátero están
   demostrados; la ruta que lo produce, no. Las escrituras inmediatas de `0x2C`
   son longitudes de registros del opcode `0x34`.

6. **`GEX.DFX` no es un nivel y su formato de modelo no está descodificado.**
   Se ha demostrado qué no es, no cómo se recorre.

7. **El segundo registro de `0x00426588`** (`0x24` B, prefijo `0xE1`, opcode
   `0x32` en `+0x0F`) se emite siempre, pero su propósito sigue sin establecer.

8. **Ninguna conclusión técnica se ha cambiado sin evidencia del ejecutable.**
   Las correcciones de esta consolidación son de alcance y de precisión; las
   hipótesis retiradas lo fueron por desensamblado, no por preferencia
   documental.

---

## 1. Metodología y fuentes

| Elemento | Detalle |
|---|---|
| Desensamblado | `gex_full.asm` (volcado completo de `.text` con `llvm-objdump`) |
| Verificación puntual | extractor de rangos sobre el mismo volcado, con dirección inicial/final |
| Assets | lectura binaria directa, sin escritura, de los `.DFX`/`.VFX` citados |
| Contraste | cada afirmación nueva se buscó en el código antes de escribirla |
| Herramientas | ninguna instalada en esta ronda; no hay depurador ni emulador |

**Criterio de evidencia.** Un campo o una ruta se marca como *demostrado* solo
si (a) una cadena aritmética cierra con los datos reales, o (b) un consumidor
del ejecutable lo lee con esa semántica. Si solo encaja «por forma», se marca
como *hipótesis*. Si no hay consumidor ni cadena, se marca como *sin
identificar*.

**Criterio de corrección.** Este documento no reescribe los resultados
históricos de U-1e…U-1h: los interpreta. Las correcciones puntuales se aplican
en el documento correspondiente y aquí se registra el estado consolidado.

---

## 2. Mapa documental

| Documento | Contenido | Estado tras esta consolidación |
|---|---|---|
| [`docs/U1_GEOMETRY_RECOVERY_PLAN.md`](docs/U1_GEOMETRY_RECOVERY_PLAN.md) | objetivos, evidencias por ronda y pendientes | corregido: descriptor y pendientes |
| [`U1E_RESULTADOS.md`](U1E_RESULTADOS.md) | relación geometría-índices (U-1e) | histórico; nota de U-1f y remisión a esta consolidación |
| [`U1F_RESULTADOS.md`](U1F_RESULTADOS.md) | relación argumentos/índices-recursos; retirada de H1/H6 | histórico válido; remisión a esta consolidación |
| [`U1G_RESULTADOS.md`](U1G_RESULTADOS.md) | ruta 3D y contenedores `.DFX`/`.VFX` | histórico; nota de corrección de U-1h y remisión |
| [`U1H_RESULTADOS.md`](U1H_RESULTADOS.md) | interpretación final del flujo 3D y del grafo `.DFX` | referencia técnica principal; §19 = esta consolidación |
| **`U1_CONSOLIDACION.md`** (este documento) | estado final, ámbitos, pendientes y decisiones | nuevo |
| `ROADMAP.md`, `CHANGELOG.md`, `PROJECT_STATE.md` | estado de proyecto | actualizados con la consolidación |

No existen documentos independientes `U1A`–`U1D`: su historial se reconstruye
desde el plan, U-1E, el ROADMAP y el CHANGELOG, y se marca como reconstrucción.

---

## 3. Historial U-1a…U-1h

### 3.1 U-1a…U-1d — hipótesis originales, retiradas por U-1f

Estas cuatro rondas (2026-11-04) no dejaron documento propio. Lo que el plan y
los documentos posteriores conservan es lo siguiente, **como historial y no
como resultado vigente**:

| Ronda | Hipótesis original | Estado |
|---|---|---|
| **U-1a** — stride y campos | «stride de 12 bytes; 3 campos de 4 bytes» | **retirada** por U-1f |
| **U-1b** — cálculo de w | «w = 1.0f en 2D; w = valor + 8.0f en 3D» | **retirada** por U-1f |
| **U-1c** — geometría completa | reconstrucción de quads como si fueran geometría 3D | **retirada** por U-1f |
| **U-1d** — recuperación de índices | los argumentos eran coordenadas de una ruta 3D | **retirada** por U-1f |

La causa común del error fue clasificar `0x00432BBD` y `0x00432DD1` como rutas
3D. U-1f demostró, leyendo el código y `FONT.3DF`, que son:

* `0x00432BBD` — **glifo de texto 2D**: primer argumento = índice de glifo
  (`(char - 0x20) & 0x3F`), resuelto contra una rejilla de 16×16 celdas de una
  única textura global (`FONT.3DF`); quad de 8×8; cuatro vértices separados
  `0x3C` B; `grDrawTriangle` ×2.
* `0x00432DD1` — **rectángulo 2D sólido**: `x = arg1 & 0xFF`,
  `y = (arg1 >> 8) + 3`, `w = (arg2 & 0xFF) << 4`, `h = (arg2 >> 8) * 12`; sin
  textura; `grDrawTriangle` ×2.

El `imul …,0xC` de `0x00432E1B` es la **altura en píxeles** de ese rectángulo,
no un stride. El `1.0f` de `0x00432BBD` se escribe en `vértice+0x20` (campo no
verificado) y el `+8` es el tamaño del quad, no un cálculo de `w`. La geometría
3D real está en `0x0046558A`.

**Decisión:** las hipótesis de U-1a…U-1d se conservan en el historial como
«retiradas por U-1f», nunca como resultados. El plan las presenta ya así.

### 3.2 U-1e — relación geometría-índices (2026-11-04)

Conclusión vigente tras U-1f: no hay secuencia de índices de dibujo; los
triángulos se emiten con punteros directos a vértices en pila. En 2D el índice
de glifo se resuelve aritméticamente contra la rejilla de 16×16, sin tabla de
coordenadas. Las rutas 2D y 3D no comparten estructura ni datos.

Hipótesis **H1** («ambas funciones generan geometría 3D») y **H6** («las rutas
2D y 3D comparten estructura») quedan **retiradas**.

### 3.3 U-1f — relación índices-recursos (2026-11-05)

Esta ronda fija la corrección de fondo y la ruta 3D:

* `0x00432BBD` → `FONT.3DF` (`[0x0057B5EC]`, `GrTexInfo` en `0x0065BFA0`);
  sin tabla intermedia.
* `0x00432DD1` → sin recurso; rectángulo sólido.
* `0x004329EF` carga `font.3df`; único llamador `0x00463705`.
* Ruta 3D (`0x0046558A`): flujo de comandos; campo de **14 bits**
  (`& 0x3FFF`) → tabla de texturas `0x006488F4`; vértices *inline*.
* `0x00461C32` construye la tabla desde el `.VFX`; `0x00461E0B` carga y
  relocaliza el `.DFX`; `0x0043B11D` orquesta `.VFX` + `.DFX`.

### 3.4 U-1g — relación con `.DFX`/`.VFX` (2026-11-06)

Demuestra el flujo de comandos (un comando por llamada; lista enlazada;
cabecera de 8 B; opcode en `byte[+0xB]`; prefijo `0xE1`; pool de doble buffer),
el índice de 14 bits y los formatos de contenedor `.DFX`/`.VFX`. Deja abiertas
cuestiones que U-1h cierra, y otras que siguen abiertas hoy (§6).

### 3.5 U-1h — interpretación final (2026-11-06; auditada 2026-11-07)

Cierra el contenedor, el descriptor de nivel, el nodo BSP, el polígono, el
vértice, la normal, el auxiliar UV, los recorridos, los emisores de nivel, la
tabla de despacho y las dos anomalías de U-1g. Su §18 registra la auditoría
final contra el ejecutable.

**Correcciones que esta consolidación añade a U-1h:**

| Punto | Antes | Ahora |
|---|---|---|
| §0.2 descriptor | «campos identificados al 100 %» | 10 demostrados, 1 hipótesis, 3 sin identificar, 2 parciales (§5) |
| §7.3 vértices de modelo | ambos emisores con vértices de 16 B | `0x425010` = 16 B; `0x4258BD` = 12 B |
| §12 fila de emisores | «caras 12 B, vértices 16 B» para los dos | separado por emisor |
| §13.3/§14 pendientes | mezclados con limitaciones generales | lista explícita en §6 |

**Auditoría de la relocalización (2026-11-08):** la carga y compactación `.DFX`
quedan verificadas instrucción a instrucción en
[`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md). Precisión sobre la
descripción previa de §2.2 de U-1h:

| Punto | Antes | Ahora |
|---|---|---|
| Firma del relocalizador | `0x004735C0(reloc*, end, delta)` | `0x004735C0(container*, imageBase, delta)` |
| `delta` | «la base» (sin precisar) | **`B`**, la base del buffer |
| Destino de la compactación | «sobre el hueco de la tabla» (ambiguo) | **`B` (offset 0)**, longitud `4*(roundUp4(tamaño)/4 − aligned)` |
| `aligned` en `0x00461E68` | se atribuía `ECX = count` | `0x00461E62` recarga `ECX = aligned`; corrección de transcripción |

**Variante `0x00461ECA` (analizada el 2026-11-08):** no es una continuación ni
una rama de `0x00461E0B`, sino una función independiente con otro contrato:
`void *dfx_load_at(const char *path, void *dest)`. Comparte lectura
(`0x00461B90`), cabecera, alineación, localización de la imagen y relocalizador;
difiere en que relocaliza con **`delta = dest`**, copia la imagen a un **buffer
externo** en lugar de compactarla in situ, **no libera** `4*aligned` (abre y
cierra un ámbito de arena: `0x4650B0`/`0x4650FD`), sobrescribe `[0x647FAC]` con
la longitud copiada y devuelve `dest`. Si `dest == -1`, calcula
`dest = 0x801FC000 − roundUp4(tamaño)`. Su copia termina `0x200` DWORDs
(2 048 B) antes que la del cargador principal, lo que en `AZTEC16.DFX` deja
**22 punteros copiados apuntando fuera del bloque** y **99 posiciones sin
copiar**; en `GEX.DFX` no pierde punteros. **No tiene ningún llamador estático**
en este build (sin `call`/`jmp`, sin VA literal y sin `.reloc`), por lo que es
código no referenciado. Detalle: [`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md) §H.

El parser del contenedor (S-1) se ha implementado en `port/`
([`S1_DFX_PARSER.md`](S1_DFX_PARSER.md)) y utiliza únicamente la ruta de
`0x00461E0B`; la variante sigue sin uso.

Ninguna de estas precisiones cambia los resultados estructurales de §4 y §5:
afectan a la **mecánica de carga**, no al formato del descriptor ni a la
geometría.

---

## 4. Los tres ámbitos de geometría y formatos

### 4.1 Geometría BSP de nivel — demostrada

| Pieza | Tamaño / forma | Evidencia |
|---|---|---|
| Contenedor `.DFX` | `count` + `count` offsets + relleno a `4*((count+0x200)&~0x1FF)` | validado en tres niveles |
| Descriptor (`image[0]`) | 16 dwords; ver §5 | encadenado exacto en los tres niveles |
| Nodo BSP | `0x20` B; unión interno/hoja; tipo `+0x12` = 1/2 | 2 807 nodos, sin otros valores |
| Polígono | `0x14` B; índices `+0/+2/+4`, flags `+0x06`, 4 índices de normal, puntero UV `+0x10` | §5 de U-1h |
| Vértice | `12` B; `x/y/z` en `+0/+2/+4` | tabla `desc+0x24` |
| Normal | `6` B; escala 4096 | `|n| = 4096 ± 1` |
| Auxiliar UV | `12` B; U/V de 8 bits y `word[+6]` = índice de textura de 14 bits | emisor añade `0x4000` |
| Recorrido de dibujo | `0x401C00` → `[0x64FEA4]` → `0x4269C2` → `0x426256`/`0x426588` → pool → `0x469836` → `0x46558A` | trazas estáticas |
| Colisión | `0x41A203` segmento–BSP | no es dibujo |
| Consulta esférica | `0x4024B9` + `0x5015A9`, marca `0x8000 \| 0x40` | uso de juego = hipótesis |

**Cierre:** la geometría de nivel está descrita de extremo a extremo; lo que
queda abierto son **campos sin consumidor** y **una rama sin datos reales**
(§6, §7), no la estructura.

> **Precisión 2026-11-09.** El tipo `+0x12` es 1/2, pero el predicado del
> original es `(word[+0x12] & 1) == 0` (hoja); `port/` usa `& 2`, equivalente
> solo para esos dos valores. La prueba de visibilidad de `0x401C00` no usa
> planos en `cam+0xC8/0xD4/0xD8`, sino dos matrices `int16` de 3×3 en
> `cam+0x88` (A) y `cam+0xA8` (B); detalle en
> [`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md).
> La matriz `M = *[cam+0xE8]` que alimenta A/B se genera por la cadena
> `0x46DD40` → `0x46E643`: rotaciones Euler Z → Y → X con ángulos negados
> (`cam+0x30/0x32/0x34`, o `cam+0x128/0x12A/0x12C` en rama alternativa),
> matriz base `*[cam+0xF0]` o identidad si `cam+0x43C != 0`, cambio de ejes
> `M2`, `O = M2 · R`, y `M = transpose(O) = R^T · M2^T` (§6 del documento
> citado).

### 4.2 Geometría de modelos — parcial

Los emisores de modelo son *function pointers* instalados en el struct de nivel
(`0x0043DC7E`), no llamadas directas:

```
level[+0x4C98] = 0x004258BD   ; rango de caras de modelo
level[+0x4C9C] = 0x00425010   ; todas las caras de modelo
```

| Emisor | Cara | Vértice | Evidencia |
|---|---:|---:|---|
| `0x00425010` | 12 B | **16 B** | índice `shl 4` en `0x4250A4`; lee dwords `+0/+4/+8` |
| `0x004258BD` | 12 B | **12 B** | índice `imul …,0xC` en `0x425948`/`0x425962`; lee `x/y/z` en `+0/+2/+4` |

**Lo demostrado:** que estos emisores no son del BSP de nivel, que las caras
miden 12 B y que los dos usan tablas de vértices de distinto stride.

**Lo no demostrado:** el formato completo de `GEX.DFX` (cabecera de modelo,
tabla de frames, recorrido y semántica de los 16 B del vértice de
`0x425010`). `GEX.DFX` usa el mismo contenedor pero **no** el descriptor de
nivel: `image[0] = 0x2E0` no está relocalizado, no hay BSP ni cadena de
regiones, y aparecen `image+0x08 = 0x00350001`, `image+0x0C = 0x4C` (tabla de
frames), `image+0x34 = "gex______"` e `image+0x8C` como puntero a datos no
textuales.

### 4.3 Formatos de comandos — parcial

**Cerrado:** despacho (`cmd = opcode & ~3`, índice `cmd-0x20` en el mapa
`0x469765`, 12 punteros en `0x469735`, **11 handlers activos**), cabecera común
de 8 B, regla «el opcode se escribe el último», longitudes y layouts de
`0x20`, `0x24`, `0x28`, `0x30`, `0x34`, `0x40`/`0x48`/`0x4C`, `0xB4` y `0xF0`,
y las llamadas Glide de cada handler.

**Abierto:**

| Elemento | Qué falta | Evidencia de lo que sí se sabe |
|---|---|---|
| `0x2C` | **emisor** que escriba el opcode | handler `0x466B3E`; 4 vértices stride 8; 2 triángulos; longitud `0x2C` |
| `0x28` | **layout/semántica** de los 4 vértices pasados a Glide | emisores `0x42DEC1`/`0x504C44`; registro `0x1C` B; handler `0x46636C` construye 4 structs de `0x3C` B |
| `0x426588` | propósito del **segundo registro** | se emite siempre: `0x24` B, prefijo `0xE1`, opcode `0x32` en `+0x0F` |

---

## 5. Descriptor de nivel: estado campo a campo

Sustituye a la afirmación «todos los campos identificados al 100 %».

| Offset | Tipo | Semántica | Estado | Base de la evidencia |
|---|---|---|---|---|
| `+0x00` | ptr | raíz del BSP | **demostrado** | cierra con `[+0x2C] + nNormals*6` y con el recorrido |
| `+0x04` | — | 0 en los tres niveles | **sin identificar** | no hay consumidor; no se puede afirmar que sea puntero |
| `+0x08` | — | 0 en los tres niveles | **sin identificar** | idem |
| `+0x0C` | — | 0 en los tres niveles | **sin identificar** | idem |
| `+0x10` | u32 | nº de registros de 12 B | **demostrado estructuralmente** | `[+0x14]` apunta a `[+0x10]` registros; consumidor no localizado |
| `+0x14` | ptr | tabla de registros de 12 B | **demostrado estructuralmente** | idem; slots vacíos = 3 dwords a cero |
| `+0x18` | u32 | nº de vértices | **demostrado** | `[+0x24] + nVerts*0x0C == [+0x2C]` en los tres niveles |
| `+0x1C` | u32 | nº de polígonos | **demostrado** | `[+0x28] + nPolys*0x14 == [+0x24]` en los tres niveles |
| `+0x20` | u32 | nº de normales | **demostrado** | `[+0x2C] + nNormals*6 == [+0x00]` en los tres niveles |
| `+0x24` | ptr | tabla de vértices (12 B) | **demostrado** | cadena anterior + emisores |
| `+0x28` | ptr | inicio de polígonos (0x14 B) | **demostrado** | cadena anterior |
| `+0x2C` | ptr | tabla de normales (6 B) | **demostrado** | cadena anterior |
| `+0x30` | ptr | lista de objetos `{count, ptrs[]}` | **demostrado estructuralmente** | consumidor `0x473238`; significado de los elementos, abierto |
| `+0x34` | ptr | tabla no identificada | **parcial** | apunta dentro de la imagen; sin consumidor localizado |
| `+0x38` | u32 | `0x10000` (posible `1.0` en 16.16) | **hipótesis** | constante en los tres niveles; consumidor no localizado |
| `+0x3C` | u32 | constantes; mitad baja siempre `0x0002` | **parcial / sin identificar** | `0xC0800002`, `0x00010002`, `0x00000002`; sin consumidor |

**Recuento:** 10 campos con semántica demostrada (7 por cadena/consumidor +
3 estructurales), 1 hipótesis (`+0x38`), 3 sin identificar (`+0x04`, `+0x08`,
`+0x0C`) y 2 parciales (`+0x34`, `+0x3C`).

**Lo que no cambia:** las tres igualdades de U-1h §3.2 se cumplen sin excepción
en `AZTEC16`, `CIRCUIT0` y `REZOP2`. El encadenado no depende de los campos
abiertos.

---

## 6. Pendientes explícitos

### 6.1 `GEX.DFX` — formato de modelo no descodificado

* **Demostrado:** usa el contenedor `.DFX` (`count = 322`, `aligned = 512`,
  imagen de 143 960 B); `image[0] = 0x2E0` **no** está relocalizado y apunta a
  un bloque de ceros; `desc+0x00 = 0`; no hay cadena de regiones; no es un
  nivel.
* **Observado:** tabla de frames en `image+0x0C` (`0x4C`), offsets
  `image+0x2C → 0x124`, `image+0x30 = 8`, nombre `"gex______"` en `image+0x34`.
* **Pendiente:** descodificar la cabecera de modelo, la tabla de frames, el
  recorrido y la semántica de los vértices de 16 B usados por `0x425010`.
* **No se generaliza** el descriptor BSP a todos los `.DFX`.

### 6.2 Opcode `0x2C` — handler de cuadriláteros sin emisor

* **Demostrado:** handler `0x00466B3E`; lee cuatro posiciones
  (`+0x0C/+0x14/+0x1C/+0x24`, `+0x0E/+0x16/+0x1E/+0x26`), cuatro pares UV
  (`+0x10/+0x18/+0x20/+0x28`), color en `+0x08/+0x09/+0x0A` e índice de
  textura en `word[+0x1A]`; dibuja dos triángulos (`0x4673CB`, `0x4673DF`);
  longitud `0x2C`.
* **Resultado negativo:** ninguna escritura inmediata de `0x2C` en `+0x0B`.
  Los stores de `0x2C` localizados (`0x42564D`, `0x426218`, `0x427A5B`,
  `0x42B03A`, `0x42DC81`, `0x437B0F`) son el campo de **longitud** de registros
  del opcode `0x34`, no el opcode.
* **Pendiente:** localizar el emisor (literal o calculado) que produce la ruta
  de cuadriláteros texturizados.

### 6.3 Opcode `0x28` — emisores localizados, layout de Glide abierto

* **Demostrado:** emisores `0x0042DEC1` y `0x00504C44`, ambos con registro de
  `0x1C` B: color en `+8`, cuatro pares `xy` en `+0x0C..+0x1A`, pool con `[+4]`
  actual y `[+8]` fin, avance de `0x1C`.
* **Demostrado:** handler `0x0046636C` prepara cuatro estructuras consecutivas
  en `[ebp-0x138]` con stride `0x3C` (`xy` en `+0/+4`) y llama
  `grDrawPlanarPolygonVertexList(4, &[ebp-0x138])` en `0x466B34`.
* **Pendiente:** el layout y la semántica completos de esas cuatro estructuras
  de `0x3C` B (qué campos alimentan `ooz`, color, UV, etc.). No es un problema
  de emisión: los emisores están localizados.

### 6.4 Segundo registro de `0x00426588`

* **Demostrado:** `0x426588` escribe el registro texturizado de `0x34` B con
  opcode `0xB6` y `0x4000` en `word[+0x1E]`, **más** un segundo registro de
  `0x24` B con prefijo `0xE1` y opcode `0x32` en `+0x0F`.
* **Estado:** el segundo registro se emite siempre; su propósito (segunda
  pasada, reserva para alpha, *z-buffer* o *fallback*) **no está establecido**.
* **Pendiente:** requiere observar el resultado en ejecución o localizar un
  consumidor específico del opcode `0x32`.

### 6.5 Otros pendientes ya registrados

* Consumidor de `desc+0x34`; semántica de `desc+0x3C`; consumidor de los
  registros de 12 B (`desc+0x10/+0x14`) y de la lista de objetos
  (`desc+0x30`).
* Bits de flag de polígono sin consumidor (`0x2000`, `0x80`, `0x100`, `0x08`,
  `0x01`/`0x02`).
* Semántica de juego de `0x4024B9`/`0x5015A9`: no hay polígonos `0x20` en los
  tres niveles auditados.
* `0x004338D7` (rama `estilo & 0x80`) y layout del vértice del rectángulo 2D.
* **Variante `0x00461ECA` del cargador `.DFX`:** **analizada** (2026-11-08,
  [`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md) §H). No es una
  continuación de `0x00461E0B` ni comparte su contrato: es
  `void *dfx_load_at(const char *path, void *dest)`, copia a un buffer externo,
  no compacta in situ y no libera `4*aligned`. **No tiene llamadores estáticos**
  en este build. Quedan abiertos su propósito, el significado de `0x801FC000` y
  el motivo del recorte de `0x200` DWORDs. Además, la identidad exacta de los
  thunks de E/S (`0x541060`, `0x540F90`, `0x540DE0`, `0x540CA0`, `0x540C20`)
  sigue inferida por argumentos, y `[0x647FAC]` es «tamaño de la última
  asignación», no un campo del formato.

---

## 7. Resultados negativos que se mantienen

1. No existe un buffer de vértices separado: los vértices van *inline*.
2. No existe un identificador de recurso por comando distinto del índice de
   14 bits.
3. No hay referencia del `.DFX` al `.VFX` ni viceversa.
4. No hay tabla de texturas en el `.DFX`.
5. No hay ningún polígono con flag `0x20` en `AZTEC16`, `CIRCUIT0` ni
   `REZOP2`.
6. No se ha localizado ningún emisor literal del opcode `0x2C`.
7. No se ha localizado consumidor para `desc+0x34`, ni para los bits de flag
   sin marcar, ni para el segundo registro de `0x426588`.
8. `GEX.DFX` no contiene BSP ni descriptor de nivel.
9. `0x00461C32` no descarga tablas de textura; la descarga ocurre por comando
   en `0x24`, `0x2C` y `0xB4`.

---

## 8. Limitaciones

* Todo el análisis es **estático**. Ninguna afirmación sobre el resultado
  visual en ejecución (alpha, *z-buffer*, *culling* en bordes del frustum,
  *pickups*) se puede cerrar sin traza dinámica.
* Los niveles disponibles son tres. Un cuarto nivel podría contener polígonos
  `0x20` o emisores `0x2C`; la ausencia observada no es universal.
* El formato de modelo y los campos sin consumidor pueden depender de
  subsistemas no cubiertos (animación, audio, colisión especial).
* Las herramientas de esta ronda no incluyen decompilador; la lectura es de
  ensamblador y de datos.

---

## 9. Decisiones documentales

1. **U-1h sigue siendo la referencia técnica principal.** Esta consolidación no
   duplica sus tablas: fija el estado, separa ámbitos y enumera pendientes.
2. **Los documentos históricos no se reescriben.** U-1E/U-1F/U-1G conservan su
   texto y su nota de corrección; solo se les añade la remisión a esta
   consolidación.
3. **Las hipótesis de U-1a…U-1d se citan siempre como retiradas.** No se
   presentan como resultados en ningún documento vigente.
4. **«Cerrada» no equivale a «completa».** El cierre de la Subfase C significa
   que la geometría de nivel está descrita de extremo a extremo y que lo que
   falta está identificado y acotado, no que todo esté resuelto.
5. **No se toca `PROJECT_STATUS.svg`.** No hay evolución de MAIN ni hito
   verificado del panel; esta ronda es documental y estática.

---

## 10. Documentos actualizados por esta consolidación

* `U1_CONSOLIDACION.md` (nuevo; este documento).
* `U1H_RESULTADOS.md` — §0.2, §3.1, §7.3, §12, §17 y nuevo §19.
* `docs/U1_GEOMETRY_RECOVERY_PLAN.md` — descriptor y pendientes.
* `ROADMAP.md` — Subfase C: consolidación documental.
* `CHANGELOG.md` — entrada de consolidación.
* `PROJECT_STATE.md` — estado y pendientes.
* `U1E_RESULTADOS.md`, `U1F_RESULTADOS.md`, `U1G_RESULTADOS.md` — remisión a
  esta consolidación.

**Actualización 2026-11-08:** se añade
[`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md), auditoría de la
carga/relocalización `.DFX` y referencia técnica principal de esa mecánica.
