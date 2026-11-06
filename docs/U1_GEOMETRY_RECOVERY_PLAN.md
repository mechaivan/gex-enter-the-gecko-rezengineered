# U-1 — PLAN DE RECUPERACIÓN DE GEOMETRÍA ORIGINAL (corregido 2026-11-05)

## Resumen ejecutivo

Este documento detalla el plan y los resultados de la recuperación de la geometría
original del juego GEX: Enter the Gecko. El objetivo es entender cómo se genera y
procesa la geometría en el renderer original.

**Corrección de fondo (U-1f, 2026-11-05):** las funciones `0x00432BBD` y
`0x00432DD1` **no son geometría 3D**. Son, respectivamente, el dibujo de un glifo
de texto 2D y el dibujo de un rectángulo 2D sólido (HUD/consola). La geometría 3D
del mundo se dibuja en `0x0046558A`, que no comparte argumentos, tablas ni
recursos con ellas. Las hipótesis de U-1a…U-1e sobre «stride de 12 bytes» y
«cálculo de w» eran lecturas incorrectas y quedan retiradas. Evidencia completa
en [`U1F_RESULTADOS.md`](../U1F_RESULTADOS.md).

## Objetivos

1. **U-1a (stride y campos):** identificar el stride y los campos de estructura.
2. **U-1b (cálculo de w):** comprender cómo se calcula el componente w.
3. **U-1c (geometría completa):** reconstruir cómo se generan los vértices de cada quad.
4. **U-1d (recuperación de índices):** seguir el origen de los argumentos.
5. **U-1e (relación geometría-índices):** analizar la relación vértices/índices.
6. **U-1f (relación índices-recursos):** analizar la relación entre argumentos/índices y recursos.

## Evidencias recopiladas

### U-1a (stride y campos): COMPLETADO — CORREGIDO EN U-1f

**Afirmación original (retirada):** «stride de 12 bytes; 3 campos de 4 bytes».

**Hecho demostrado:** no existe tal estructura de 12 bytes.

- El `imul eax,eax,0xC` de `0x00432E1B` calcula la **altura en píxeles** del
  rectángulo (`alto en unidades de celda × 12`), no un stride.
- En `0x00432BBD` los cuatro vértices del glifo están separados **0x3C (60) bytes**.
- En `0x00432DD1` solo se escriben pares `(x, y)` en pila; no hay struct de vértice
  de 12 B demostrado.
- Los argumentos de `0x00432DD1` son **bytes empaquetados**:
  `arg1 = x | (y << 8)`, `arg2 = w | (h << 8)`.

### U-1b (cálculo de w): COMPLETADO — CORREGIDO EN U-1f

**Afirmación original (retirada):** «w = 1.0f en 2D; w = valor + 8.0f en 3D».

**Hecho demostrado:** no se ha encontrado ningún cálculo de `w`.

- `0x00432BBD` escribe el flotante `1.0f` en la posición **+0x20 de cada
  vértice**. El nombre de ese campo no se ha verificado contra los headers de
  Glide 2.x (con u/v en `+0x24`/`+0x28`, el layout habitual pondría `ooz` en
  `+0x20`); lo seguro es que **no hay cálculo de `w`**.
- El `+8` es el **tamaño del quad** (`x+8`, `y+8`), no un cálculo de `w`.
- `0x00432DD1` **no escribe** ni profundidad ni color ni u/v: solo `(x,y)`.

### U-1c (geometría completa): COMPLETADO — CORREGIDO EN U-1f

**Hecho demostrado:**

- `0x00432BBD`: 4 vértices, quad de 8×8 px, u/v de una celda de la rejilla 16×16.
  `grDrawTriangle(A,B,C)` + `grDrawTriangle(A,C,D)`.
- `0x00432DD1`: 3 slots `(x,y)`, `(x+w,y)`, `(x,y+h)`; el primero se reescribe como
  `(x+w,y+h)` antes del segundo triángulo. Sin textura.
  `grDrawTriangle((x,y+h),(x+w,y),(x,y))` + `grDrawTriangle((x,y+h),(x+w,y),(x+w,y+h))`.

### U-1d (recuperación de índices): COMPLETADO — CORREGIDO EN U-1f

**Hecho demostrado:**

- `0x00432BBD` lo llama **únicamente** `0x0043293F`, que a su vez lo llama
  **únicamente** `0x00434F18` y `0x00434FE9`.
- `0x00432DD1` lo llama **únicamente** `0x00434F18`.
- Los datos proceden de una lista de registros de **6 bytes** en `0x0065BFC0`,
  con contador en `0x0065CBE0` (máx. `0x1FF`), alimentada por `0x00432852`.
- El primer argumento de `0x00432BBD` es un **índice de glifo**, no una
  coordenada: `(carácter - 0x20) & 0x3F`.

### U-1e (relación geometría-índices): COMPLETADO — CORREGIDO EN U-1f

**Hecho demostrado:**

- No hay secuencia de índices de dibujo: los triángulos se emiten con punteros
  directos a vértices en pila.
- En 2D el índice de glifo se resuelve **aritméticamente** contra una rejilla de
  16×16 celdas de la textura (no contra una tabla de coordenadas).
- Las rutas 2D y 3D **no comparten** estructura ni datos.

**Hipótesis retiradas:** H1 («ambas funciones generan geometría [3D]») y H6
(«las rutas 2D y 3D comparten la misma estructura»).

### U-1f (relación índices-recursos): COMPLETADO (2026-11-05)

**Hechos demostrados:**

- **`0x00432BBD`**: `glifo → (col = glyph & 0xF, fila = glyph >> 4) →
  (u,v) = (col*16, fila*16)` en coordenadas de textura Glide, con margen de 1
  unidad en el borde de la celda, sobre **una única textura global**
  (`FONT.3DF`), referenciada por la dirección `[0x0057B5EC]` +
  `GrTexInfo` en `0x0065BFA0`. Sin tabla intermedia.
- **`0x00432DD1`**: sin recurso; rectángulo sólido (`guColorCombineFunction(3)`,
  sin `grTexSource`).
- **Carga del recurso**: `0x004329EF` construye `<raíz>\font.3df`, llama a
  `gu3dfGetInfo` / `gu3dfLoad`, reserva memoria de textura con
  `grTexTextureMemRequired`, descarga con `grTexDownloadMipMap` y guarda la
  dirección base en `0x0057B5EC`. Único llamador: `0x00463705` (init).
- **Ruta 3D real (`0x0046558A`)**: lee un flujo de comandos; un campo de **14 bits
  (`& 0x3FFF`)** indexa la tabla de punteros a texturas `0x006488F4`. Los vértices
  vienen *inline* en el flujo; no hay vertex/index buffer.
- **Tabla `0x006488F4`**: la construye `0x00461C32` desde el **`.VFX`** del nivel
  (`0x0043B11D` le pasa `[ebp+0xC]`); descarga las texturas con
  `grTexTextureMemRequired` / `grTexDownloadMipMap`.
- **`.DFX`**: `0x00461E0B` lo carga (lector `0x00461B90`, `fopen "rb"`) y lo
  relocaliza con `0x004735C0`; alimenta el flujo que consume `0x0046558A`.
- **Nombres**: `"%s.dfx"` (`0x54FAD0`) y `"%s.vfx"` (`0x54FAD8`) a partir del
  nombre de nivel en `nivel+0x4D98`, generado con `"%s\\level\\%s"` (`0x54FAC4`).

**Resultados negativos:**

- Ninguna referencia a `.DFX`/`.VFX`, `0x6488F4` ni buffers de vértices en
  `0x00432BBD` / `0x00432DD1`.
- Ningún camino de datos entre las rutas 2D y la 3D.
- `grDrawPlanarPolygonVertexList` se usa una sola vez, dentro de `0x0046558A`
  (`0x466B34`, 4 vértices); no aparece en las rutas 2D.

**Cuestiones abiertas:** `0x004338D7` (rama `estilo & 0x80`), estructura de vértice
de la ruta de rectángulo, significado de `registro[+0x0C]` en la ruta 3D, formato
de cabecera de `.DFX`/`.VFX`, estructura de `0x0057B600`.

### U-1g (relación con DFX/VFX): COMPLETADO (2026-11-06)

**Hechos demostrados:**

- **`0x0046558A` dibuja un solo comando**; el recorrido lo hace `0x00469836`
  sobre una lista enlazada de registros. Cabecera de 8 B: `[0]` = longitud/flag
  de dibujo, `[4]` = siguiente (`-1` = fin). Opcode en `byte[+0xB]`; `0xE1` es
  prefijo de 4 B que fija `[0x57B8EC]`. Vértices *inline*.
- **Pool y doble buffer:** `0x64FF00 + 0x6000*level[+0x14]`, punteros
  `0x64FEE0`/`0x64FEE4`, head `buf+0x5FF8`, inicialización
  `0x00465530(buf, 0xC00)`.
- **Despacho:** `cmd = opcode & ~3`; índice `cmd-0x20` en el mapa `0x00469765`;
  12 handlers en `0x00469735`.
- **Recurso:** `word[record+0x1A] & 0x3FFF` → tabla `0x006488F4` → registro
  `.VFX`; bits altos = nivel; `record[+0x0C]==1` activa `grTexDownloadTable`.
- **`.VFX`:** `count` + registros `0x8C + [+0x84]` B; validado con `GEX.VFX`
  (4 registros, fin exacto en `0x634`).
- **`.DFX`:** `{count, offsets[]}` + imagen relocalizable; `0x004735C0` suma la
  base y `0x00461E0B` compacta; validado con `GEX.DFX` (322 offsets, imagen de
  143 960 B, raíz en `0x2E0`).
- **Orden de carga:** `0x0043B11D` carga **primero** el `.VFX` (tabla de
  texturas) y **después** el `.DFX` (`level[+4]`).

**Resultados negativos:**

- Sin referencias cruzadas `.DFX`↔`.VFX`; sin tabla de texturas en el `.DFX`;
  sin identificador de recurso por comando distinto del índice de 14 bits; sin
  buffer de vértices separado.

**Cuestiones abiertas:** solapamiento `+0x8`/`+0xB` en triángulos; longitud
`0x20` del opcode `0x30` frente a la lectura de `+0x20/+0x21`; estructura
interna del grafo `.DFX`; semántica de `record[+0x0C]` y de `+0x14` del `.VFX`;
registro de cierre `0x64FB40`.

## Cuestiones pendientes

1. **U-1h (interpretación final):** semántica de los campos del flujo 3D y del
   grafo `.DFX`; incluye las cuestiones abiertas de U-1g.
2. **`0x004338D7`:** renderizador especial de la lista 2D.
3. **`grDrawPlanarPolygonVertexList`:** layout del vértice usado por el opcode
   `0x40`.

## Próximos pasos

1. **U-1h:** interpretación final, apoyada en los hechos de U-1g.
2. Contrastar el campo de `+0x20` (`oow`/`ooz`) con los headers reales de
   `GrVertex` de Glide 2.x.
3. Si se autoriza ejecución, confirmar dinámicamente el byte `+0xB` de los
   triángulos y la longitud real del opcode `0x30`.

## Documentación relacionada

- [U1G_RESULTADOS.md](../U1G_RESULTADOS.md): resultados detallados de U-1g.
- [U1F_RESULTADOS.md](../U1F_RESULTADOS.md): resultados detallados de U-1f.
- [U1E_RESULTADOS.md](../U1E_RESULTADOS.md): resultados de U-1e (corregidos por U-1f).
- [CHANGELOG.md](../CHANGELOG.md): registro de cambios del proyecto.
- [ROADMAP.md](../ROADMAP.md): roadmap del proyecto.
- [PROJECT_STATE.md](../PROJECT_STATE.md): estado actual del proyecto.
