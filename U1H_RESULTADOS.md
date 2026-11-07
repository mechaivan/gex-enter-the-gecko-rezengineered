# RESULTADOS DE U-1h — INTERPRETACIÓN FINAL DEL FLUJO 3D Y DEL GRAFO `.DFX` (2026-11-06)

**Binario analizado:** `C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE`
(PE32 i386, 1 557 504 B, EU v1.00.000; `imageBase = 0x00400000`).

**Assets contrastados (solo lectura):**

| Fichero | Tamaño | Contenido |
|---|---|---|
| `LEVEL\AZTEC16.DFX` | 544 586 B | nivel completo (BSP + geometría + objetos) |
| `LEVEL\CIRCUIT0.DFX` | 339 414 B | nivel completo |
| `LEVEL\REZOP2.DFX` | 137 468 B | nivel completo |
| `GEX.DFX` | 146 008 B | modelo/personaje (sin BSP) |
| `GEX.VFX` | 1 588 B | texturas y paletas |

**Modo:** análisis estático read-only. No se ha ejecutado el juego, no se ha
abierto ninguna ventana, no se ha modificado el ejecutable ni ningún fichero de
`C:\GEX_REZ`, y no se ha instalado ni configurado ninguna herramienta. No hay
breakpoints, ni traza dinámica, ni emulador.

**Alcance de esta ronda:** cerrar la semántica de los campos del flujo 3D y del
grafo `.DFX`, y resolver estáticamente las dos anomalías que U-1g dejó abiertas.
Las cuestiones que **solo** se pueden zanjar ejecutando el binario se marcan
explícitamente como tales en §14; no se afirma nada dinámico.

---

## 0. Resumen ejecutivo

1. **El contenedor `.DFX` queda descifrado y validado en tres niveles reales.**
   `count` + tabla de `count` offsets + relleno hasta
   `aligned = (count + 0x200) & ~0x1FF` dwords; la imagen empieza en
   `4 * aligned`. Los offsets de la tabla son **relativos a la imagen**, no al
   fichero.

2. **La imagen de nivel empieza con un descriptor** (`image[0]`). **No todos
   sus campos están identificados**: 10 tienen semántica demostrada (raíz del
   BSP, nº y tabla de registros de 12 B, vértices, polígonos, normales, tabla
   de vértices, inicio de polígonos, tabla de normales y lista de objetos),
   `+0x38` es una hipótesis, `+0x34`/`+0x3C` son parciales y `+0x04`/`+0x08`/
   `+0x0C` no están identificados (estado campo a campo en §3.1 y en
   [`U1_CONSOLIDACION.md`](U1_CONSOLIDACION.md) §5). Lo que sí está demostrado
   es que el encadenado de regiones cierra exactamente en los tres niveles
   (§3.2).

3. **El nodo del BSP mide `0x20` B y es una unión interno/hoja**, no dos
   estructuras distintas. Interno: centro + radio + normal de plano (3×`int16`,
   escala 4096) + distancia + dos hijos. Hoja: centro + radio + puntero y
   contador de polígonos + AABB de 6×`int16`. El campo `+0x12` es un **tipo**
   (`1` = interno, `2` = hoja), no un bitmask arbitrario.

4. **Los polígonos (0x14 B), vértices (12 B), normales (6 B) y el registro
   auxiliar de UV (12 B) quedan descritos con precisión.** El registro auxiliar
   contiene, por vértice, un par de coordenadas de textura de 8 bits; la
   **mitad alta del segundo dword** es el índice de textura (14 bits) y el
   emisor le añade `0x4000` al escribirlo en el comando.

5. **La ruta de dibujo del nivel queda completa:** `0x00401C00` recorre el BSP,
   prueba la esfera de cada nodo contra el volumen de vista y anexa las hojas
   visibles a la lista `[0x64FEA4]` (máximo 2048); `0x004269C2` emite los
   polígonos de esas hojas; `0x00426256`/`0x00426588` escriben los comandos;
   `0x00469836` recorre la lista de comandos y `0x0046558A` despacha a Glide.

6. **`0x0041A203` es colisión segmento–BSP, no dibujo.** U-1g §7.3 no lo
   clasificó como dibujo: dejó el recorrido de `0x0041A1E0`/`0x0041A203` como
   **hipótesis sin algoritmo** y sin decidir entre dibujo y colisión. U-1h lo
   demuestra leyendo el código (§6.3 y §4.5).
   `0x004024B9` es una **consulta esfera–BSP** que, con el callback
   `0x005015A9`, recorre los polígonos de cada hoja y marca con `0x8000 | 0x40`
   los que llevan el flag `0x20` y caen dentro de la esfera. Su uso como
   *pickups*/disparadores es una **hipótesis**: en `AZTEC16`, `CIRCUIT0` y
   `REZOP2` no hay ningún polígono con `0x20`, de modo que el efecto no se ha
   observado sobre datos reales. No es dibujo.

7. **La tabla de despacho de opcodes de U-1g estaba desplazada.** Corregida y
   verificada leyendo el mapa `0x469765` y los 12 punteros de `0x469735`:
   solo hay **11 comandos activos** (`0x20`, `0x24`, `0x28`, `0x2C`, `0x30`,
   `0x34`, `0x40`, `0x48`, `0x4C`, `0xB4`, `0xF0`).

8. **Ambas anomalías de U-1g se resuelven estáticamente.** El opcode no se
   solapa con el color porque los emisores lo escriben **después** del payload;
   y el handler real de `0x30` (`0x004673E9`) consume exactamente `0x20` B, sin
   desbordamiento. La confusión de U-1g venía de asignar a `0x30` el handler de
   `0x24` (`0x00465E5A`).

9. **`0x64FB40` no es un comando vacío:** es un **rectángulo 2D de fondo**
   (opcode `0xF0`, `0x14` B, `0x200×0x180`, negro) que se reencadena como
   último comando de cada frame. Handler `0x00469268`.

10. **`GEX.DFX` no es un nivel.** Usa el mismo contenedor pero su imagen tiene
    otra cabecera: `image[0]` no está relocalizado y apunta a un bloque de
    ceros; no hay descriptor BSP ni cadena de regiones. Es un modelo/personaje
    con tabla de frames y nombre de 8 caracteres (`"gex______"`).

---

## 1. Metodología y herramientas

| Elemento | Detalle |
|---|---|
| Desensamblador | `llvm-objdump` (Intel syntax), `--start-address/--stop-address` |
| Lector PE | `tools/gexmap/pe.mjs` para VA↔RVA↔offset y tabla de imports |
| Referencias cruzadas | `analysis/EU/functions.tsv`, `analysis/EU/calls.tsv` |
| Validación de assets | lectura binaria directa de `.DFX`/`.VFX` (solo lectura) |
| Ejecución | ninguna. Sin breakpoints, sin ventanas, sin emulador |
| REA | **no viable** (`missing_analysis_engine`). No se instaló nada |

Los artefactos de `analysis/EU` se usaron solo como índice; **todas las
afirmaciones de instrucción se verificaron sobre el desensamblado real**.

Ficheros temporales (fuera del repo, no versionados, en
`%TEMP%\opencode`):

| Fichero | Uso |
|---|---|
| `gex_full.asm` | desensamblado completo del EXE |
| `u1h_graph2.mjs` | recorrido y validación del BSP (nodos, hojas, polígonos) |
| `u1h_stats.mjs` | estadísticas de nodos, normales, centro/radio y nombre |
| `u1h_radius.mjs` | prueba radio = ⌈máx. distancia centro→vértice⌉ |
| `u1h_header.mjs` | volcado de cabecera e imagen `.DFX` |
| `u1h_map.mjs` | lectura del mapa `0x469765` y tabla `0x469735` |
| `u1h_handlers.mjs` | llamadas Glide por handler |
| `u1h_425010.txt` | desensamblado del emisor de caras de modelo |

**Criterio de validación:** un campo se da por identificado solo si (a) el
código lo consume con una semántica inequívoca y (b) los datos de los tres
niveles lo respetan sin excepciones. Todo lo demás se marca como hipótesis o
como no identificado.

---

## 2. El contenedor `.DFX`

### 2.1 Cabecera y alineación

```
+0x00  u32 count
+0x04  u32 offsets[count]      ; offsets relativos a la imagen
...    relleno hasta 4*aligned
4*aligned: imagen
```

```
aligned = (count + 0x200) & ~0x1FF      ; en dwords
```

La fórmula no es una suposición: se comprobó que `4*aligned` coincide con el
primer dword de la imagen (`image[0]`, que es el descriptor) y que el hueco
entre la tabla y la imagen contiene relleno.

| Fichero | `count` | `aligned` (dwords) | Base imagen | Tamaño imagen | `image[0]` |
|---|---:|---:|---:|---:|---:|
| `AZTEC16.DFX` | 16 082 | 16 384 | `0x10000` | 479 050 B | `0x543C` |
| `CIRCUIT0.DFX` | 9 344 | 9 728 | `0x9800` | 300 502 B | `0x5D24` |
| `REZOP2.DFX` | 3 005 | 3 072 | `0x3000` | 125 180 B | `0x3094` |
| `GEX.DFX` | 322 | 512 | `0x800` | 143 960 B | `0x2E0` |

### 2.2 Relocalización y compactación

`0x004735C0(reloc*, end, delta)` recorre el array de offsets y suma `delta` al
dword **apuntado** por cada offset:

```
for (p = reloc; p < end; p++)
    *(u32*)(image + *p) += delta;
```

`0x00461E0B` lee el fichero, llama al relocalizador con la base de la imagen y
**compacta** la imagen sobre el hueco que ocupaba la tabla de offsets. El
resultado se guarda en `level[+4]` desde `0x0043B11D`.

Consecuencia práctica: **todos los punteros internos son offsets de imagen**;
no son direcciones absolutas ni VA del ejecutable.

Prueba estructural (campo relocalizado = puntero real):

| Campo | `AZTEC16` | `CIRCUIT0` | `REZOP2` | `GEX.DFX` |
|---|:-:|:-:|:-:|:-:|
| `image+0` (descriptor) | sí | sí | sí | **no** |
| `desc+0x00` (raíz) | sí | sí | sí | **no** |
| `desc+0x14` (tabla 12 B) | sí | sí | sí | **no** |
| `desc+0x24` (tabla vértices) | sí | sí | sí | **no** |
| `desc+0x28` (inicio polígonos) | sí | sí | sí | **no** |
| `desc+0x2C` (tabla normales) | sí | sí | sí | **no** |
| `desc+0x30` (lista de objetos) | sí | sí | sí | **no** |
| `desc+0x34` (tabla desconocida) | sí | sí | sí | **no** |
| `image+0x8C` (nombre) | sí | sí | sí | **sí** |

En los tres niveles, la cadena `image+0 → desc → regiones` está relocalizada
por completo. En `GEX.DFX` **solo** `image+0x8C` está relocalizado.

### 2.3 Mapa de la imagen de nivel

Regiones y punteros (offsets de imagen):

| Región | Campo que la apunta | `AZTEC16` | `CIRCUIT0` | `REZOP2` |
|---|---|---:|---:|---:|
| Nombre del nivel | `image+0x8C` | `0xE0` | `0xE0` | `0xE0` |
| Tabla `desc+0x34` | `desc[+0x34]` | `0x2AB4` | `0x28F0` | `0x1D44` |
| Registros de 12 B | `desc[+0x14]` | `0x403C` | `0x4110` | `0x2024` |
| Raíz BSP | `desc[+0x00]` | `0x44A44` | `0x2CEB8` | `0x9D34` |
| Descriptor | `image[0]` | `0x543C` | `0x5D24` | `0x3094` |
| Polígonos | `desc[+0x28]` | `0x5474` | `0x5D5C` | `0x30CC` |
| Vértices | `desc[+0x24]` | `0x34328` | `0x21854` | `0x8054` |
| Normales | `desc[+0x2C]` | `0x3F920` | `0x2BC40` | `0x9A34` |
| Lista de objetos | `desc[+0x30]` | `0x58920` | `0x37E30` | `0xBCB4` |

Las regiones no están ordenadas por dirección: los vértices y normales pueden
quedar antes o después de los polígonos. **Solo los punteros del descriptor
son fiables**; no se debe asumir un layout lineal.

---

## 3. Descriptor de nivel (`image[0]`)

### 3.1 Campos

| Offset | Tipo | Semántica | `AZTEC16` | `CIRCUIT0` | `REZOP2` |
|---|---|---|---:|---:|---:|
| `+0x00` | ptr | raíz del BSP | `0x44A44` | `0x2CEB8` | `0x9D34` |
| `+0x04` | — | 0 en los tres niveles | 0 | 0 | 0 |
| `+0x08` | — | 0 en los tres niveles | 0 | 0 | 0 |
| `+0x0C` | — | 0 en los tres niveles | 0 | 0 | 0 |
| `+0x10` | u32 | nº de registros de 12 B | 75 | 138 | 12 |
| `+0x14` | ptr | tabla de registros de 12 B | `0x403C` | `0x4110` | `0x2024` |
| `+0x18` | u32 | nº de vértices | 3 882 | 3 497 | 552 |
| `+0x1C` | u32 | nº de polígonos | 9 609 | 5 670 | 1 018 |
| `+0x20` | u32 | nº de normales | 3 462 | 788 | 128 |
| `+0x24` | ptr | tabla de vértices (12 B) | `0x34328` | `0x21854` | `0x8054` |
| `+0x28` | ptr | inicio de polígonos (0x14 B) | `0x5474` | `0x5D5C` | `0x30CC` |
| `+0x2C` | ptr | tabla de normales (6 B) | `0x3F920` | `0x2BC40` | `0x9A34` |
| `+0x30` | ptr | lista de objetos `{count, ptrs[]}` | `0x58920` | `0x37E30` | `0xBCB4` |
| `+0x34` | ptr | tabla no identificada | `0x2AB4` | `0x28F0` | `0x1D44` |
| `+0x38` | u32 | `0x10000` (constante) | `0x10000` | `0x10000` | `0x10000` |
| `+0x3C` | u32 | constantes; ver §3.5 | `0xC0800002` | `0x00010002` | `0x00000002` |

**Estado campo a campo (corrige la lectura de «todos identificados»):**

* **Demostrados (7):** `+0x00` (raíz del BSP), `+0x18`/`+0x1C`/`+0x20`
  (contadores), `+0x24`/`+0x28`/`+0x2C` (tablas de vértices, polígonos y
  normales). Cierran las tres igualdades de §3.2 y los leen los emisores.
* **Demostrados estructuralmente, sin consumidor localizado (3):** `+0x10` y
  `+0x14` (nº y tabla de registros de 12 B) y `+0x30` (lista de objetos).
* **Hipótesis (1):** `+0x38 = 0x10000`, posible `1.0` en 16.16; no se ha
  localizado su consumidor.
* **Parciales (2):** `+0x34` (tabla no identificada, sin consumidor) y `+0x3C`
  (constantes; mitad baja siempre `0x0002`, sin consumidor).
* **Sin identificar (3):** `+0x04`, `+0x08` y `+0x0C`; son cero en los tres
  niveles y no se puede afirmar que sean punteros ni contadores.

La tabla completa con evidencia por campo está en
[`U1_CONSOLIDACION.md` §5](U1_CONSOLIDACION.md). El encadenado de §3.2 no
depende de los campos abiertos.

`+0x38 = 0x10000` es `1.0` en formato 16.16; es la única lectura que encaja con
la constante, pero **no se ha localizado su consumidor**, así que queda como
hipótesis.

### 3.2 Encadenado exacto (prueba de coherencia)

En los tres niveles se cumple, sin excepción:

```
[desc+0x28] + nPolys * 0x14  ==  [desc+0x24]
[desc+0x24] + nVerts * 0x0C  ==  [desc+0x2C]
[desc+0x2C] + nNormals * 0x06 == [desc+0x00]
```

| Nivel | `0x5474 + 9609*0x14` | `= 0x34328` | `0x34328 + 3882*0xC` | `= 0x3F920` | `0x3F920 + 3462*6` | `= 0x44A44` |
|---|---|:-:|---|---|---|:-:|
| `AZTEC16` | `0x34328` | ✔ | `0x3F920` | ✔ | `0x44A44` | ✔ |
| `CIRCUIT0` | `0x21854` | ✔ | `0x2BC40` | ✔ | `0x2CEB8` | ✔ |
| `REZOP2` | `0x8054` | ✔ | `0x9A34` | ✔ | `0x9D34` | ✔ |

Este encadenado es la prueba más fuerte de que las tres tablas y sus contadores
están bien identificados: cualquier error de stride rompería la igualdad en
algún nivel.

### 3.3 Tabla de registros de 12 B

`desc[+0x14]` apunta a `desc[+0x10]` registros de 12 B. Están **dispersamente
poblados**: los slots vacíos son tres dwords a cero.

| Nivel | Registros | No nulos | Primer dword válido |
|---|---:|---:|---|
| `AZTEC16` | 75 | 47 | apunta a un sub-objeto con cabecera propia |
| `CIRCUIT0` | 138 | 95 | idem |
| `REZOP2` | 12 | 6 | idem |

El primer dword del registro apunta a un bloque con el mismo patrón que la
cabecera de objeto de `AZTEC16` (`0x733EC`): `{u32 tamaño/id, 0, u32 nº de
subelementos, ptr, 0, u64, ptr, ptr, ptr}`. El segundo y tercer dword del
registro de 12 B son cero en la mayoría de los casos; cuando no lo son
(`0xC42B, 0x80`, `0x2856, 0x8`) no se ha identificado su semántica.

**Resultado negativo:** ni `desc+0x10`/`+0x14` ni los registros de 12 B
intervienen en la ruta de dibujo, en la de colisión ni en la de la consulta
esférica. Su consumidor no está localizado.

### 3.4 Lista de objetos (`desc+0x30`)

```
+0x00  u32 count
+0x04  ptr objects[count]
```

Cada objeto de `REZOP2` ocupa `0x30` B y empieza por un identificador
(`0x38`, `0x39`, `0x3A`…). Campos observados: `+0x04`/`+0x08` = `0x40`/`0x40`,
`+0x18` = `2`, `+0x1C` (crece de 8 en 8), `+0x20` y `+0x28` (crecen de 3 en 3).
Encaja con una rejilla/atlas 2D, pero **no se ha identificado**.

Su único consumidor localizado es `0x00473238(list, level+0x104, level+0x24,
pool)`: recorre los objetos, divide por `obj[+0x1C]` y `obj[+0x18]` y construye
un puntero `obj + 8*resto + 0x20`. La operación es coherente con un índice 2D
sobre una tabla de elementos de 8 B; el significado de esos elementos queda
abierto.

### 3.5 Constantes `+0x38`/`+0x3C`

`+0x38` es `0x10000` en los tres niveles. `+0x3C` vale `0xC0800002`,
`0x00010002` y `0x00000002`: la mitad baja es siempre `0x0002`; la alta cambia
(`0xC080`, `0x0001`, `0x0000`). Ni el dibujo ni la colisión leen estos campos
en las rutas trazadas. **No identificados.**

### 3.6 Nombre del nivel

`image+0x8C` es un puntero relocalizado a una cadena de **12 caracteres** en
`image+0xE0`:

| Nivel | Cadena |
|---|---|
| `AZTEC16` | `"aztec______"` |
| `CIRCUIT0` | `"circuit____"` |
| `REZOP2` | `"rezop______"` |

`0x0043B2BB` hace `strcpy(0x54FA90, image[+0x8C])` **solo si**
`level[+0x4E38] == 2`. Es el único uso de `image+0x8C` localizado. En
`GEX.DFX` el mismo campo apunta a datos no textuales: es la prueba de que la
cabecera de imagen del modelo **no** es la del nivel.

---

## 4. Nodo del BSP (`0x20` B)

### 4.1 Unión interno/hoja

| Offset | Interno | Hoja |
|---|---|---|
| `+0x00` | `int16` centro X | `int16` centro X |
| `+0x02` | `int16` centro Y | `int16` centro Y |
| `+0x04` | `int16` centro Z | `int16` centro Z |
| `+0x06` | `int16` radio | `int16` radio |
| `+0x08` | **no identificado** | **no identificado** |
| `+0x0A` | **no identificado** | **no identificado** |
| `+0x0C` | `int16` normal X | `ptr` a polígonos |
| `+0x0E` | `int16` normal Y | (parte alta del puntero) |
| `+0x10` | `int16` normal Z | `int16` nº de polígonos |
| `+0x12` | `int16` tipo = `1` | `int16` tipo = `2` |
| `+0x14` | `int32` distancia del plano | `int16` AABB xmin |
| `+0x16` | — | `int16` AABB ymin |
| `+0x18` | `ptr` hijo A | `int16` AABB zmin |
| `+0x1A` | (parte alta) | `int16` AABB xmax |
| `+0x1C` | `ptr` hijo B | `int16` AABB ymax |
| `+0x1E` | (parte alta) | `int16` AABB zmax |

La unión no es una interpretación libre: el campo `+0x12` toma **exactamente**
los valores `1` (interno) y `2` (hoja) en los 2 807 nodos de los tres niveles;
no aparece ningún otro valor ni bits adicionales.

| Nivel | Nodos | Internos (`+0x12=1`) | Hojas (`+0x12=2`) | Profundidad |
|---|---:|---:|---:|---:|
| `AZTEC16` | 1 629 | 814 | 815 | 15 |
| `CIRCUIT0` | 1 001 | 500 | 501 | 13 |
| `REZOP2` | 177 | 88 | 89 | 8 |

En los tres árboles: **cero** hijos inválidos, **cero** ciclos, y la suma de
polígonos de las hojas es **exactamente** `desc[+0x1C]` (9 609 / 5 670 /
1 018). Todos los rangos `[ptr, ptr + n*0x14)` caen dentro de
`[[desc+0x28], [desc+0x24])`.

### 4.2 Centro y radio

- El centro `(+0x00, +0x02, +0x04)` cae **siempre** dentro del AABB de la hoja
  (100 % de las 815/501/89 hojas).
- El radio `+0x06` es `⌈máx. distancia centro→vértice⌉` de los vértices de los
  polígonos de la hoja: 814/815, 500/501 y 89/89 quedan a **menos de 2 unidades**
  del techo. Los dos casos que llegan exactamente a 2 unidades de diferencia son
  `0x51524` (`AZTEC16`, `r = 1184` frente a `⌈1185,1⌉ = 1186`) y `0x34B58`
  (`CIRCUIT0`, `r = 1277` frente a `⌈1278,1⌉ = 1279`); en `REZOP2` no hay
  ninguno.

De los tres recorridos del BSP, el radio del nodo (`+0x06`) **solo lo consume
`0x00401C00`**, en la prueba de visibilidad de la hoja (§6.1). `0x004024B9`
**no** usa el radio del nodo: en los nodos internos prueba el plano con el radio
de la **esfera de consulta** más `320` (§6.4).

### 4.3 Plano del nodo interno

`(+0x0C, +0x0E, +0x10)` es una normal unitaria en formato 12 bits
(`4096 = 1.0`): 814/814, 500/500 y 88/88 tienen longitud 4096 ± 1.

`+0x14` es un `int32` con la distancia del plano, en las mismas unidades que
las posiciones de vértice.

Ecuación usada por los walkers:

```
d(p) = (p.x*N.x + p.y*N.y + p.z*N.z) >> 12  -  node[+0x14]
```

### 4.4 Hoja: AABB y rango de polígonos

- `+0x0C` = puntero al primer polígono.
- `+0x10` = `int16` con el número de polígonos.
- `+0x14..+0x1E` = `xmin, ymin, zmin, xmax, ymax, zmax` (`int16`).
- Los polígonos son contiguos con stride `0x14`; el rango no se solapa con el
  de otra hoja en los tres niveles (comprobado).

### 4.5 Convención de lados

`0x0041A203` deja la convención demostrada:

```
dA = d(p0);  dB = d(p1);  eps = [0x54D04C] = 320

if (dA >=  eps && dB >=  eps)  -> hijo A
if (dA <= -eps && dB <= -eps)  -> hijo B
si no                          -> ambos
```

Por tanto: **hijo A = lado positivo de la normal, hijo B = lado negativo**.
El épsilon `320` es compartido por la colisión y la consulta esférica.

### 4.6 Campos no identificados del nodo

`+0x08` y `+0x0A` no los lee ninguno de los tres walkers trazados
(`0x00401C00`, `0x0041A203`, `0x004024B9`); en `0x004269C2` tampoco se
consumen. En las hojas, `+0x0A` toma valores pequeños (1..~200) y `+0x08`
valores arbitrarios de 16 bits; en los internos, `+0x0A` también es pequeño.
Podrían ser un contador de profundidad y un identificador, pero **no hay
evidencia**. Quedan como desconocidos.

---

## 5. Polígono, vértice, normal y registro UV

### 5.1 Polígono (`0x14` B)

| Offset | Tipo | Semántica |
|---|---|---|
| `+0x00` | `u16` | índice de vértice 0 |
| `+0x02` | `u16` | índice de vértice 1 |
| `+0x04` | `u16` | índice de vértice 2 |
| `+0x06` | `u16` | flags (ver §5.2) |
| `+0x08` | `int16` | índice con signo a normal de v0 |
| `+0x0A` | `int16` | índice con signo a normal de v1 |
| `+0x0C` | `int16` | índice con signo a normal de v2 |
| `+0x0E` | `int16` | cuarto índice de normal (cuadriláteros / reservado) |
| `+0x10` | `ptr` | registro auxiliar de UV de 12 B |

Los índices de normal son **con signo**: `0x00419850(idx, tabla, out)` resuelve
la normal y la niega si `idx < 0`. Se validaron 38 436 referencias en los tres
niveles: **cero** fuera de rango.

`+0x10` apunta a un registro de 12 B que **no** está en una tabla global: cada
polígono puede apuntar a un registro distinto, y los registros están
entremezclados con otras regiones. El 100 % de los punteros cae dentro de la
imagen y está alineado a 4.

### 5.2 Flags del polígono

Distribución real (número de polígonos con cada bit):

| Bit | `AZTEC16` | `CIRCUIT0` | `REZOP2` | Consumidor / significado |
|---|---:|---:|---:|---|
| `0x0001` | 0 | 886 | 0 | desconocido |
| `0x0002` | 0 | 242 | 0 | desconocido |
| `0x0008` | 0 | 0 | 36 | desconocido (posible marca de impacto) |
| `0x0020` | 0 | 0 | 0 | marca de la consulta esférica: lo exige `0x005015A9` (hipótesis: trigger) |
| `0x0040` | 2 394 | 0 | 0 | **especial**: filtrado en colisión y en la consulta esférica |
| `0x0080` | 6 740 | 449 | 0 | desconocido |
| `0x0100` | 2 454 | 48 | 0 | **requerido** por el filtro de colisión (`mask2`) |
| `0x0200` | 246 | 0 | 0 | desconocido |
| `0x0400` | 0 | 69 | 0 | desconocido |
| `0x0800` | 52 | 0 | 0 | desconocido |
| `0x2000` | 4 188 | 2 416 | 427 | desconocido (presente en ~44 %) |
| `0x4000` | 268 | 311 | 0 | **excluido de la colisión** |
| `0x8000` | 2 728 | 428 | 66 | **oculto**: el dibujo lo salta |

Hechos verificados:

- `0x8000`: `0x004269C2` salta el polígono salvo que
  `[level+0xA0] & 1` (modo de visualización de geometría oculta).
- `0x4000`: `0x0041A203` lo descarta siempre en el bucle de impacto
  (`0x41A9C0..0x41A9CF`).
- `0x0040`: en colisión, si está puesto exige además el bit de `mask2`
  (`0x0041A9D1..0x41A9EB`); en `0x005015A9` se salta el polígono.
- `0x0020`: `0x005015A9` lo exige; es la marca de "objeto interactivo".
- `mask1 = 0x4480` (`0x4000|0x400|0x80`) y `mask2 = 0x100` en el llamador
  `0x0040BF69`: si el polígono tiene algún bit de `mask1`, necesita `0x100`
  para ser considerado.

Los demás bits no tienen consumidor localizado. **No se les asigna
significado.**

### 5.3 Vértice (`12` B)

| Offset | Tipo | Semántica |
|---|---|---|
| `+0x00` | `int16` | X |
| `+0x02` | `int16` | Y |
| `+0x04` | `int16` | Z |
| `+0x06` | 6 B | **no identificado** |

Rangos observados (unidades del mundo):

| Nivel | X | Y | Z |
|---|---|---|---|
| `AZTEC16` | −12 814…21 994 | −14 413…20 890 | −14 921…3 363 |
| `CIRCUIT0` | −21 542…10 713 | −19 956…19 853 | −10 368…32 255 |
| `REZOP2` | 1 364…5 844 | 189…6 629 | −4 050…5 670 |

La ruta de dibujo solo lee `+0/+2/+4` (índice × 12 desde `desc+0x24`). Los
6 B restantes no se consumen en ninguna ruta trazada; `+0x0A` se agrupa en
torno a `0x3400..0x3480` (13 312…13 440) en los tres niveles, lo que sugiere
un atributo empaquetado, pero **no está demostrado**.

### 5.4 Normal (`6` B)

Tres `int16` con escala 4096. Se usan:

- `0x00419850(idx, ntab, out)` para resolverlas (con negación si `idx < 0`).
- `0x0046999C` para transformarlas por la matriz de cámara (tres productos
  escalares `>> 12` en `0x6474B0/B4/B8` y `0x646B80/84/88`).
- El *culling* de caras de `0x0041A203` y el *shading* de los emisores.

### 5.5 Registro auxiliar de UV (`12` B)

Tres dwords, uno por vértice:

```
dword[i] = (índice_textura << 16) | (v << 8) | u        ; i = 0,1,2
```

pero **solo el dword 1 lleva el índice de textura**:

| Campo | Uso demostrado |
|---|---|
| `byte[aux+0]` | U del vértice 0 (8 bits) |
| `byte[aux+1]` | V del vértice 0 |
| `byte[aux+2..3]` | no consumido por el handler `0x34` |
| `byte[aux+4]` | U del vértice 1 |
| `byte[aux+5]` | V del vértice 1 |
| `word[aux+6]` | **índice de textura** (14 bits) + bit `0x4000` |
| `byte[aux+8]` | U del vértice 2 |
| `byte[aux+9]` | V del vértice 2 |
| `byte[aux+10..11]` | no consumido por el handler `0x34` |

El índice ocupa siempre la **palabra que sigue al par UV del vértice 1**. El
offset depende del *stride* del comando:

| Handler | Dónde lee el índice | Posición del vértice 1 |
|---|---|---|
| `0x24` (`0x466115`) | `word[rec+0x1A] & 0x3FFF` | u/v en `rec+0x18`/`0x19` (stride 8) |
| `0x2C` (`0x4670D3`) | `word[rec+0x1A] & 0x3FFF` | u/v en `rec+0x18`/`0x19` (stride 8) |
| `0x34` (`0x467B5E`) | `word[rec+0x1E] & 0x3FFF` | u/v en `rec+0x1C`/`0x1D` (stride 12) |
| `0xB4` (`0x468126`) | `word[rec+0x1E] & 0x3FFF` | u/v en `rec+0x1C`/`0x1D` (stride 12) |

`0x24` y `0x2C` comparten *stride* 8 por vértice (`x`,`y`,`u`,`v`,`índice`),
pero `0x24` dibuja un triángulo (3 vértices, longitud `0x24`) y `0x2C` un
cuadrilátero (4 vértices, longitud `0x2C`).

`0x24` lee además `u`/`v` del tercer vértice en `byte[rec+0x20]`/`byte[rec+0x21]`;
no usa `word[aux+2]`, que queda sin consumir.

Prueba en el emisor `0x00426588`:

```
; uv0, uv1, uv2 se copian tal cual a los comandos
0x42686B  rec[+0x10] = *arg3        ; dword UV vértice 0
0x426876  rec[+0x1C] = *arg4        ; dword UV vértice 1
0x426881  rec[+0x28] = *arg5        ; dword UV vértice 2
0x4268AC  cx = word[rec+0x1E]       ; mitad alta del dword UV 1
0x4268B0  cx &= 0x3FFF              ; índice de textura
0x4268B6  ch |= 0x40                ; activa bit 0x4000
0x4268BC  word[rec+0x1E] = cx
```

Y en el handler `0x0046789D`:

```
0x467C29  S0 = byte[rec+0x10] * [0x54C188] / [0x54C18C]
0x467C52  T0 = byte[rec+0x11] * [0x54C188] / [0x54C18C]
0x467CFB  S1 = byte[rec+0x1C] ...
0x467DCD  S2 = byte[rec+0x28] ...
0x467B5E  índice = word[rec+0x1E] & 0x3FFF
```

El escalado `[0x54C188] / [0x54C18C]` convierte el byte 0..255 en coordenada
normalizada. La constante `0x4000` no forma parte del índice: es una marca que
el emisor añade y que el handler enmascara.

Los bytes `aux+2/+3` y `aux+10/+11` no los lee el handler `0x34`; sus valores
observados varían (`0x0002`, `0x00FB`, `0x1E02`…). **No identificados.**

---

## 6. Recorridos

### 6.1 Visibilidad y construcción de la lista de hojas — `0x00401C00`

Firma observada: `void walk(descriptor *desc, camera *cam, list *out)`.

- Prepara la matriz de vista a partir de `cam+0x88` (7 `int16`) y `cam+0..4`,
  llamando dos veces a `0x0046999C`.
- Pila explícita en `0x647740`; arranca con `desc[0]` (raíz).
- Para cada nodo transforma su centro y prueba la esfera `(centro, radio)`
  contra el volumen de vista: comparaciones con `-radio`, `radio + cam[+0x5C]`
  y los planos laterales (`cam+0xC8`, `cam+0xD4`, `cam+0xD8`).
- Si el nodo es hoja (`word[+0x12] & 2`) y pasa la prueba, se anexa a la lista:

```
if (out[0] < 0x800) {
    out[4 + 4*out[0]] = leaf;
    out[0]++;
}
```

- Si es interno, apila los hijos (`+0x18`, y `+0x1C` si no es nulo).

`[0x64FEA4]` se asigna en `0x0043B047` con tamaño `0x2004` B: exactamente
`4` B de contador + `0x800` punteros. Es la lista de **hojas visibles** del
frame.

Llamador único: `0x0043DECE`, dentro de `0x0043DC39`, justo antes de
`0x004269C2`. Si `level[+0x4C94] & 0x200` está puesto, el nivel **no se dibuja**
(`0x0043DEF3..0x43DF01`).

### 6.2 Emisión de las hojas visibles — `0x004269C2`

Firma: `int emit(list *visible, descriptor *desc, camera *cam, pool *out,
u32 poolBase)`.

- Recorre `visible` desde `+4`, un puntero por entrada.
- Exige `word[node+0x12] & 2`; si no, salta.
- Reserva en el pool **13 dwords (`0x34` B) por polígono** (peor caso) y
  comprueba capacidad antes de escribir (`0x426B6C..0x426B7E`).
- Para cada polígono:
  - salta si `flag & 0x8000` y no está activo `[level+0xA0] & 1`;
  - proyecta los tres vértices con `0x0046A1D0` / `0x0046B727`;
  - descarta si las coordenadas proyectadas caen fuera del rango
    `0x8000`/`0x1800200` (recorte);
  - elige emisor: sólido (`0x00426256`) o texturizado (`0x00426588`) según el
    polígono;
  - usa `desc+0x24` (vértices 12 B) y `poly+0x10` (auxiliar UV 12 B).
- Devuelve el puntero final del pool, que `0x0043DF29` guarda en
  `level[+0x24][+4]`.

### 6.3 Colisión segmento–BSP — `0x0041A203`

Firma: `poly *hit(root, vec3 *p0, vec3 *p1, int oneSided, ctx *c,
u16 mask1, u16 mask2)`.

- Pila explícita en `0x647720`; buffers en `0x6476C0`, `0x6476E0`,
  `0x6476F4`, `0x647700`, `0x647710`, `0x647718`.
- Test interno con `dA`/`dB` y épsilon `320` (§4.5).
- Hoja: rechazo conservador segmento–AABB por *slabs*, filtro de flags
  (§5.2), *culling* por normales e impacto si `dist² < 0x10000`.
- Devuelve el polígono impactado en `[ebp-4]` (o 0).
- `0x400` dispara un handler vía `0x0053781C`; `0x008` registra el impacto.

Llamadores: `0x0041A1E0` (envoltorio con máscaras 0 y `oneSided = 1`),
`0x0040BF69` (máscaras `0x4480`/`0x100`) y `0x0040B475` (envoltorio que pasa
`dfx[0]` como raíz). Ninguno dibuja.

### 6.4 Consulta esfera–BSP y marcado de polígonos — `0x004024B9`

Firma: `void query(root, sphere *s, leafCallback cb)`.

- Pila explícita; prueba `word[node+0x12] & 2`; transforma la normal con
  `0x0046999C` y compara con `radio_esfera + 320`, es decir, con el radio de la
  **esfera de consulta**, no con el radio del nodo.
- Para cada hoja candidata llama al callback con el puntero a la hoja.
- Único llamador: `0x005019AF` con callback `0x005015A9`.

Callback `0x005015A9` (recorre los polígonos de la hoja):

```
si !(flag & 0x20)        -> saltar
si  (flag & 0x8000)      -> saltar
si  (flag & 0x0040)      -> saltar
si dist²(centro_esfera, vértice[poly[0]]) >= r² -> saltar
si no:
    ctx->items[ctx->count++] = poly          ; array en ctx+8
    poly[+6] |= 0x8000                        ; ocultar
    poly[+6] |= 0x0040                        ; marcar como recogido
```

La esfera **no sale del nodo**. En `0x005015A9` la cadena es:

```
ctx = [[0x6466A8] + 0x18] + 0x1C
sphere = ctx[0]                  ; {int16 x,y,z; int16 radio; u32 radio²}
radio² = sphere[+0x8]
ctx[+4] = contador
ctx[+8] = array de polígonos
```

El callback compara `dist²(centro_esfera, vértice[poly[0]])` contra
`sphere[+0x8]` y, si es menor, añade el polígono a `ctx[+8]`, incrementa
`ctx[+4]`, y marca `0x8000 | 0x40` en `poly[+6]`. El único constructor localizado,
`0x005019AF`, crea una esfera de radio `0x640` (1600) y `radio² = 0x271000`.

Que el destino sea *pickups*/disparadores es una **hipótesis** basada en el
marcado `0x8000 | 0x40`: **no hay ningún polígono con flag `0x20` en los tres
niveles auditados**, así que el camino no se ha observado con datos reales. Lo
demostrado es la consulta esfera–BSP y el marcado; no su uso de juego. No es
dibujo ni colisión de movimiento.

### 6.5 Tabla comparativa

| Función | Entrada | Salida | Uso |
|---|---|---|---|
| `0x00401C00` | descriptor + cámara + lista | lista de hojas visibles | visibilidad |
| `0x004269C2` | lista de hojas + descriptor + cámara + pool | comandos 3D | dibujo |
| `0x0041A203` | raíz + segmento + máscaras | polígono impactado | colisión |
| `0x004024B9` | raíz + esfera + callback | llamadas por hoja | consulta esférica (hipótesis: recogida) |

Ninguna de las cuatro es intercambiable: comparten el nodo y el épsilon `320`,
pero tienen contratos distintos.

---

## 7. Emisores de comandos

### 7.1 `0x00426256` — emisor sólido / texturizado de nivel

- Comprueba `[0x647578] & 0x60000` (modo de recorte) y sale si está activo.
- Escribe un registro de longitud `0x18` con opcode `0x20` (sólido plano):
  `+0x08` color, `+0x0C/+0x10/+0x14` posiciones proyectadas.
- Variante texturizada: longitud `0x34`, UVs en `+0x10/+0x1C/+0x28`, colores
  en `+0x08/+0x14/+0x20`, opcode `0xB4 | ((color >> 24) & 3)`.
- Escribe bbox en `+0x2C/+0x2E/+0x30` desde `0x647430/0x64742C/0x6474A0`.
- **El opcode se escribe al final**, después de colores y UVs.

### 7.2 `0x00426588` — emisor texturizado de nivel (dos registros)

**Registro 1:** `0x34` B, opcode `0xB6` (handler `0xB4`).

```
+0x08 color0   +0x0C xy0   +0x10 uv0
+0x14 color1   +0x18 xy1   +0x1C uv1
+0x20 color2   +0x24 xy2   +0x28 uv2
+0x2C/+0x2E/+0x30 bbox
```

**Registro 2:** `0x24` B, con prefijo `0xE1` en `+0x0B` y opcode `0x32`
(→ handler `0x30`, Gouraud sólido) en `+0x0F`:

```
+0x08 dword 0xE1000040      ; prefijo: valor 0x40 -> modo de transparencia 2
+0x0C color0   +0x10 xy0
+0x14 color1   +0x18 xy1
+0x1C color2   +0x20 xy2
```

El consumidor `0x0046558A` trata el prefijo así:

```
opcode = byte[rec+0x0B]
if (opcode == 0xE1) {
    [0x57B8EC] = (dword[rec+0x08] & 0x9FF) >> 5 & 3;
    rec += 4;
    opcode = byte[rec+0x0B];
}
```

Es decir: el prefijo **no es un registro aparte**; son 4 B dentro del mismo
registro, y desplazan la base del comando real. Por eso el opcode del comando
siguiente queda en `rec+0x0F` y las posiciones en `+0x10/+0x18/+0x20`.

El propósito del segundo registro **no está establecido**. Se emite siempre,
junto al texturizado, y usa el mismo triángulo en modo sólido con transparencia
2. Hipótesis: segunda pasada / reserva para texturas con alpha o para el
*z-buffer*. No se puede confirmar sin traza.

### 7.3 `0x004258BD` y `0x00425010` — emisores de modelo

| Función | Entrada | Registro de cara | Vértice |
|---|---|---|---|
| `0x004258BD` | `{+4 inicio, +6 fin}`, tabla de índices, `modelDesc{+4 vertexArray, +0x14 faceArray}`, pool | 12 B | **12 B** |
| `0x00425010` | `faceSet{+4 vertexArray, +0x10 count, +0x14 faces}`, vertexArray, out, pool | 12 B | **16 B** |

`0x00425010` escribe caras de 12 B con stride `0xC` y usa una tabla de
vértices de **16 B** (índice `shl 4` en `0x4250A4`; lee dwords en
`+0/+4/+8`); el color está en `face+8` y el opcode se elige por `face+7`.

`0x004258BD` escribe caras de 12 B y usa una tabla de vértices de **12 B**
(`imul …,0xC` en `0x425948` y `0x425962`; lee `x/y/z` en `+0/+2/+4`). Por
tanto **los dos emisores no comparten stride de vértice**: la afirmación
anterior de que ambos usaban 16 B era incorrecta (corregido en la
consolidación de 2026-11-07).

**Estos emisores no son del BSP de nivel.** Se instalan como callbacks en el
struct de nivel (`0x0043DC7E`):

```
level[+0x4C98] = 0x004258BD   ; rango de caras de modelo
level[+0x4C9C] = 0x00425010   ; todas las caras de modelo
level[+0x4CA0] = 0x004269C2   ; hojas visibles del nivel
level[+0x4CA4] = 0x0040172B   ; setup de proyección/cámara
```

Llamadas indirectas localizadas:

| Callback | Llamadores |
|---|---|
| `+0x4C98` (`0x4258BD`) | `0x423772`, `0x4238A5`, `0x45B224`, `0x45B468`, `0x45B72F`, `0x460BE3`, `0x471E0A`, `0x471E47` |
| `+0x4C9C` (`0x425010`) | `0x45AC97`, `0x45AE39`, `0x45AECB`, `0x471642`, `0x4717D1`, `0x4718BB` |
| `+0x4CA0` (`0x4269C2`) | `0x43DF20` |
| `+0x4CA4` (`0x40172B`) | `0x43DDE9` |

Ninguno de los emisores de modelo tiene llamada directa en `.text`: son
*function pointers* del nivel, lo que explica que U-1g no los viera en el
grafo de llamadas.

---

## 8. Flujo de comandos: tabla de despacho corregida

### 8.1 Mapa real

`idx = (opcode & ~3) - 0x20`, guarda `idx <= 0xD0`; mapa de bytes en
`0x00469765`; 12 punteros en `0x00469735`.

| `opcode` | Handler | Estado |
|---|---|---|
| `0x20` | `0x00465A89` | activo — triángulo sólido plano |
| `0x24` | `0x00465E5A` | activo — triángulo texturizado |
| `0x28` | `0x0046636C` | activo — cuadrilátero planar |
| `0x2C` | `0x00466B3E` | activo — cuadrilátero texturizado (2 triángulos) |
| `0x30` | `0x004673E9` | activo — Gouraud sólido |
| `0x34` | `0x0046789D` | activo — Gouraud texturizado |
| `0x38`, `0x3C` | `0x00469711` | no-op |
| `0x40` | `0x00468512` | activo — línea |
| `0x44` | `0x00469711` | no-op |
| `0x48` | `0x0046875B` | activo — polilínea |
| `0x4C` | `0x00468B54` | activo — línea (3 llamadas) |
| `0x50`–`0xB0` | `0x00469711` | no-op (incluye `0x84`–`0xAC`) |
| `0xB4` | `0x00467E65` | activo — texturizado con bbox |
| `0xB8`–`0xEC` | `0x00469711` | no-op |
| `0xF0` | `0x00469268` | activo — rectángulo relleno |

Handlers activos: **11** (el duodécimo, `0x469711`, es `ret`). Los emisores
conocidos escriben literalmente `0x20`, `0x24`, `0x28`, `0x30`, `0x34`, `0x40`,
`0x48`, `0xB4`–`0xB7` y `0xF0`; **no se ha localizado** un emisor que escriba
`0x2C` (véase §18.4).

### 8.2 Layouts de registro verificados

Cabecera común de 8 B: `+0x00` longitud, `+0x04` siguiente; color/opcode en
`+0x08..+0x0B`.

| Opcode | Longitud | Contenido |
|---|---:|---|
| `0x20` | `0x18` | color + 3×xy |
| `0x24` | `0x24` | color + (xy,uv)×3; índice en `word[+0x1A]` |
| `0x28` | `0x1C` | color + 4×xy |
| `0x2C` | `0x2C` | color + (xy,uv)×4 (cuadrilátero); índice en `word[+0x1A]` |
| `0x30` | `0x20` | color0,xy0,color1,xy1,color2,xy2 |
| `0x34` | `0x2C` | (color,xy,uv)×3; índice en `word[+0x1E]` |
| `0xB4` (`0xB6`) | `0x34` | (color,xy,uv)×3 + `+0x2C/+0x30`; índice en `word[+0x1E]` |
| `0x40`/`0x48`/`0x4C` | `0x14` | 2 puntos (línea) |
| `0xF0` | `0x14` | color + x0,y0,x1,y1 |

Hay dos *strides* de vértice:

* **8 B** (`0x24` triángulo, `0x2C` cuadrilátero): `x` `+0x0C/+0x14/+0x1C/+0x24`,
  `y` `+0x0E/+0x16/+0x1E/+0x26`, `u/v` `+0x10/+0x18/+0x20/+0x28`,
  índice de textura en `+0x12/+0x1A/+0x22/+0x2A`.
* **12 B** (`0x34`, `0xB4`): `color` `+0x08/+0x14/+0x20`, `x`
  `+0x0C/+0x18/+0x24`, `y` `+0x0E/+0x1A/+0x26`, `u/v`
  `+0x10/+0x1C/+0x28`, índice en `+0x12/+0x1E/+0x2A`.

El handler `0x30` lee colores en `+0x08/+0x10/+0x18` y posiciones en
`+0x0C/+0x14/+0x1C` y `+0x0E/+0x16/+0x1E`; **no** toca `+0x20/+0x21` (esa
lectura era la anomalía de U-1g y pertenece al handler `0x24`).

### 8.3 Glide por handler

| Handler | Llamadas Glide |
|---|---|
| `0x20` | `grConstantColorValue4` + `guColorCombineFunction` + `grDrawTriangle` |
| `0x24` | `grTexDownloadTable` + `grTexSource` + `grConstantColorValue4` + `guColorCombineFunction` + `grDrawTriangle` |
| `0x28` | `grConstantColorValue4` + `guColorCombineFunction` + `grDrawPlanarPolygonVertexList` |
| `0x2C` | `grTexDownloadTable` + `grTexSource` + `grConstantColorValue4` + `guColorCombineFunction` + 2×`grDrawTriangle` |
| `0x30` | `guColorCombineFunction` + `grDrawTriangle` |
| `0x34` | `grTexSource` + `guColorCombineFunction` + `grDrawTriangle` |
| `0x40` / `0x48` / `0x4C` | `grConstantColorValue4` + `guColorCombineFunction` + `grDrawLine` |
| `0xB4` (`0xB6`) | `grTexDownloadTable` + `grTexSource` + `guColorCombineFunction` + `grDrawTriangle` |
| `0xF0` | `grConstantColorValue4` + `guColorCombineFunction` + 2×`grDrawTriangle` |

El handler `0x30` **no** llama a `grConstantColorValue4`; su única llamada de
estado es `guColorCombineFunction`. Tampoco lo hace `0x34`/`0xB4`.

Thunks verificados contra la tabla de imports de `glide2x.dll`:
`0x54051A` `grDrawTriangle`, `0x540520` `grTexSource`, `0x540526`
`guColorCombineFunction`, `0x540538` `grConstantColorValue4`, `0x5405B6`
`grDrawLine`, `0x5405BC` `grDrawPlanarPolygonVertexList`, `0x5405C2`
`grTexDownloadTable`, `0x540508` `grTexTextureMemRequired`, `0x540502`
`grTexDownloadMipMap`.

### 8.4 Regla del opcode

`+0x0B` es el **byte alto del dword de color**. En la ruta 3D el dword fuente
ya lleva el opcode en ese byte; en la ruta 2D (`0x004FF06F`) se escribe después
del color. La regla vale para ambos caminos porque **el opcode siempre se
escribe el último**.

---

## 9. Resolución de las dos anomalías de U-1g

### 9.1 Solapamiento `+0x8` / `+0xB` en los triángulos

**U-1g afirmaba:** el emisor escribía `byte[+0xB] = opcode` antes de copiar un
dword a `+0x8`, de modo que el byte alto del dword borraría el opcode y este
solo podría existir en runtime. Quedaba como cuestión abierta.

**Resuelto:** el orden real es el inverso. En los tres emisores trazados el
opcode se escribe **después** del payload:

| Emisor | Escritura del color | Escritura del opcode |
|---|---|---|
| `0x00426256` (sólido) | `0x4263C6` (`+0x08`) | `0x4263D2` (`+0xB = 0x20`) |
| `0x00426256` (texturizado) | `0x4264E1..0x4264F6` | `0x426530` (`+0xB = 0xB4｜color>>24&3`) |
| `0x00426588` (texturizado) | `0x42688C..0x4268A4` | `0x4268EC` (`+0xB = 0xB6`) |
| `0x00426588` (sólido 2.º) | `0x426957..0x42696F` | `0x426975` (`+0x0F = 0x32`) |

No hay solapamiento destructivo porque el opcode es la última escritura del
registro. **No se necesita ningún valor de runtime**: el byte alto del dword de
color es el opcode y se escribe explícitamente. La anomalía era una lectura
incorrecta del orden de instrucciones en U-1g.

### 9.2 Longitud `0x20` del opcode `0x30`

**U-1g afirmaba:** el handler del `0x30` leía `u`/`v` del tercer vértice en
`+0x20/+0x21`, fuera de la longitud declarada, lo que sugería reutilización de
los 4 B siguientes del pool o una inconsistencia del original.

**Resuelto:** el handler del `0x30` es `0x004673E9`, no `0x00465E5A`.
`0x00465E5A` es el handler del `0x24` (texturizado, longitud `0x24`): lee el
índice de textura en `word[rec+0x1A]` (palabra tras el par UV del vértice 1,
`rec+0x18`/`0x19`) y las UV del tercer vértice en `byte[rec+0x20]`/`byte[rec+0x21]`.
No lee `word[aux+2]`. `0x2C` usa el mismo *stride* 8 y el mismo offset de índice,
pero recorre **cuatro** vértices (cuadrilátero); `0x34`/`0xB4` usan *stride* 12 y
leen el índice en `rec+0x1E` (§5.5). `0x004673E9` lee:

```
colores  : +0x08, +0x10, +0x18
posiciones: +0x0C, +0x14, +0x1C  (dwords)
```

Cubre exactamente `0x20` B, **sin leer nada fuera del registro**. La
inconsistencia desaparece: no hay desbordamiento ni reutilización del pool.

La causa raíz de ambas anomalías es la misma: la tabla opcode→handler de U-1g
estaba desplazada una fila (asignaba al `0x20`/`0x24` el `0x465A89`, al
`0x30`/`0x34` el `0x465E5A`, etc.). La tabla correcta está en §8.1.

---

## 10. Registro estático `0x64FB40`

`0x0064FB40` no es un comando vacío. Es un **rectángulo 2D de fondo/clear**:

- Inicialización en `0x004635B3`: longitud `+0x00 = 4`, opcode `0x64FB4B = 0xF0`,
  RGB `0,0,0` en `+0x08..+0x0A`, `x/y = 0` en `+0x0C/+0x0E`, `w/h = 0x200/0x180`
  en `+0x10/+0x12`.
- Reencadenado como **último comando del frame** en
  `0x00462240..0x004622AB`:

```
if ([0x551644][+0x6FB0] > 0) { +0x12 = 0x180; +0x10 = 0x200; }
else                         { +0x12 = 0x0E0; }
+0x0E = ([0x551644][+0x14] * 0x180) & 0xFFFF
+0x04 = (lista + 8*(0xBFF - i))->next   ; siguiente anterior
+0x00 = 0x14                            ; longitud final
(lista + 8*(0xBFF - i))->next = 0x64FB40
```

  El índice de enlace es `0xBFF - [ebp-8]`, no una posición fija.
- Handler `0x00469268`: normaliza esquinas, descarta si
  `|dx| > 0x800` o `|dy| > 0x800`, ajusta `y` y dibuja dos triángulos
  (`grConstantColorValue4` + `guColorCombineFunction` + 2×`grDrawTriangle`).

Es la base del *clear* de pantalla por software (no usa `grBufferClear`). U-1g
lo había clasificado como no-op; queda corregido, incluida la longitud inicial
`4` que el reencadenado sustituye por `0x14`.

---

## 11. `.VFX`: formato de textura y tabla

`record[+0x0C]` es `GrTexInfo.format`:

| Valor | Observado | Uso |
|---|---|---|
| `0` | sí | sin tabla; solo `grTexDownloadMipMap` |
| `1` | sí (`GEX.VFX`) | **con tabla**: `grTexDownloadTable(0, 0, record+0x14)` |
| `0xC` | sí | otro formato, sin tabla en la ruta trazada |

`record[+0x14]` son **datos NCC *inline***, no un puntero: se pasa
`record+0x14` como `data` a `grTexDownloadTable`. El registro `.VFX` tiene
tamaño `0x8C + record[+0x84]`, y los píxeles empiezan en `+0x8C`.

`0x00461C32` **no** llama a `grTexDownloadTable`: solo
`grTexTextureMemRequired` (`0x461CF3`) y `grTexDownloadMipMap` (`0x461DEE`).
Sobrescribe `record[+0x10]` con `record+0x8C` y `record[+0x88]` con la
dirección asignada. Si `record[0] < 7 && record[+0x0C] == 1`, copia
`byte[record+0x8C]` a `record+0x8D` y `record+0x8E` (`0x461D63..0x461DB9`);
no descarga ninguna tabla.

La descarga de la tabla ocurre **por comando**, en los handlers `0x24`, `0x2C`
y `0xB4`, con esta forma (`0x4670EB..0x4670FC` para `0x2C`):

```
if (record[+0x0C] == 1)
    grTexDownloadTable(0, 0, record + 0x14);
```

Esto corrige la lectura de U-1g, que atribuía la descarga de tabla a
`0x00461C32` y solo citaba los handlers `0x24`/`0xB4`.

---

## 12. `GEX.DFX` frente a los niveles

`GEX.DFX` usa el mismo contenedor (`count = 322`, `aligned = 512`, imagen de
143 960 B) pero **no es un nivel**:

| Aspecto | Niveles (`AZTEC16`…) | `GEX.DFX` |
|---|---|---|
| `image[0]` | descriptor real, relocalizado | `0x2E0`, **no relocalizado**, bloque de ceros |
| `desc+0x00` | raíz BSP | 0 |
| Cadena de regiones | cierra exactamente | no existe |
| `image+0x08` | `0` | `0x00350001` |
| `image+0x0C` | `0` | `0x4C` (tabla de frames) |
| `image+0x34` | — | `"gex______"` (8 caracteres) |
| `image+0x8C` | nombre de nivel (12 chars) | puntero a datos no textuales |
| Emisores | `0x426256`/`0x426588` (BSP) | `0x425010` (caras 12 B, vértices 16 B) y `0x4258BD` (caras 12 B, vértices 12 B; no es BSP) |

La cabecera de modelo contiene una tabla de offsets de frames
(`image+0x2C` → `0x124`, `image+0x30 = 8`) y datos de vértice/cara con otro
stride. **No se ha descodificado el formato de modelo**; solo se ha demostrado
que **no** es el de nivel y que no debe generalizarse el descriptor BSP a
todos los `.DFX`.

---

## 13. Hechos, hipótesis y resultados negativos

### 13.1 Hechos demostrados

1. `.DFX` = `count` + tabla de offsets + imagen en `4*((count+0x200)&~0x1FF)`.
2. Los offsets y todos los punteros internos son relativos a la imagen.
3. `0x004735C0` relocaliza sumando la base; `0x00461E0B` compacta.
4. El descriptor de nivel tiene los campos de §3.1 y el encadenado de §3.2.
5. El nodo BSP mide `0x20` B; `+0x12` es un tipo `1`/`2`; centro y radio son
   coherentes con la geometría; la normal es unitaria (escala 4096).
6. Los polígonos son de `0x14` B, los vértices de 12 B, las normales de 6 B y
   el auxiliar de UV de 12 B.
7. El índice de textura está en `word[aux+6]` y el emisor le añade `0x4000`.
8. El dibujo del nivel es `0x401C00` → `[0x64FEA4]` → `0x4269C2` →
   `0x426256`/`0x426588` → pool → `0x469836` → `0x46558A` → Glide.
9. `0x41A203` es colisión de segmento; `0x4024B9` + `0x5015A9` es una consulta
   esférica que marca polígonos `0x20` (su uso como *pickups*/triggers es
   hipótesis, §6.4).
10. La tabla de opcodes correcta es la de §8.1, con 11 handlers activos.
11. `0x64FB40` es el rectángulo de fondo `0xF0`.
12. `record[+0x0C]` es el formato de textura y `record[+0x14]` son datos NCC
    *inline*.
13. `GEX.DFX` no contiene BSP ni descriptor de nivel.

### 13.2 Hipótesis (sin confirmar)

1. `desc+0x38 = 0x10000` es `1.0` en 16.16.
2. `desc+0x34` es una tabla de sombras/iluminación o de nombres de textura; no
   se ha localizado su consumidor.
3. Los registros de 12 B de `desc+0x14` son "materiales" o instancias de
   sub-objeto; su primer dword apunta a una cabecera con el mismo patrón que la
   de `0x733EC`.
4. Los objetos de `desc+0x30` describen un atlas/rejilla 2D de 8 B por celda.
5. Los bits `0x2000`, `0x80`, `0x100`, `0x08`, `0x01`/`0x02` de los polígonos
   codifican propiedades de superficie (agua, suelo, pared, animación); no hay
   consumidor localizado.
6. El segundo registro sólido de `0x426588` es una pasada de reserva o de
   transparencia.
7. Los 6 B no usados del vértice y los 4 B no usados del auxiliar UV son
   atributos empaquetados (posible segunda UV o color de luz).
8. Los polígonos con flag `0x20` son objetos recogibles o disparadores: el
   callback `0x005015A9` los marca con `0x8000 | 0x40`, pero **no hay ninguno**
   en los tres niveles auditados, así que su uso de juego no está confirmado.

### 13.3 Resultados negativos

1. **No existe** un buffer de vértices separado: los vértices van *inline*.
2. **No existe** un identificador de recurso por comando distinto del índice
   de 14 bits.
3. **No existe** referencia del `.DFX` al `.VFX` ni viceversa.
4. **No hay** una tabla de texturas en el `.DFX`.
5. **No se ha localizado** el consumidor de `desc+0x10/+0x14`, `desc+0x30`,
   `desc+0x34` ni de los bits de flag sin marcar.
6. **No se ha demostrado** que el flujo de comandos se genere en un único
   punto: el pool lo escriben varios emisores (nivel, modelo, 2D) y solo el
   recorrido final está unificado.
7. **No hay** ningún polígono con flag `0x20` en `AZTEC16`, `CIRCUIT0` ni
   `REZOP2`: el efecto del callback `0x005015A9` no se ha observado sobre datos
   reales.

---

## 14. Limitaciones y cuestiones que exigen instrumentación dinámica

Las siguientes cuestiones **no se pueden zanjar estáticamente** con el nivel de
evidencia exigido en esta ronda. Se documentan como pendientes y no se afirma
nada sobre su comportamiento en ejecución:

1. **Origen y poblado de `[0x64FEA4]` fuera del dibujo del nivel.** Se conoce
   al productor (`0x401C00`) y al consumidor (`0x4269C2`), pero no si otros
   sistemas (HUD, modelos, efectos) añaden entradas en el mismo frame.
2. **Valor real de `[0x647578]`** (máscara de recorte de `0x426256`) y del
   resto de globales de recorte (`0x1800200`, `0x80008000`). Son constantes
   estáticas, pero su calibración depende del estado de la cámara.
3. **Semántica de los bits `0x2000`, `0x80`, `0x100`, `0x08`, `0x01`/`0x02`** de
   los polígonos. Requiere observar qué geometría desaparece o cambia con cada
   nivel.
4. **Propósito del segundo registro sólido de `0x426588`.** Está demostrado
   que se emite siempre; su efecto visual (alpha, *z-buffer*, *fallback*) solo
   se puede ver en ejecución.
5. **Interpretación de `desc+0x34`, `desc+0x30` y `desc+0x14`.** No tienen
   consumidor en las rutas trazadas; podrían usarse en sistemas no cubiertos
   (animación, colisión especial, audio).
6. **Formato exacto del modelo `GEX.DFX`** (tabla de frames, vértices de 16 B,
   caras de 12 B). Se ha demostrado que no es el de nivel, no cómo se recorre.
7. **Comportamiento del *culling* en los bordes del frustum** (`320` de épsilon
   y los planos de `cam+0xC8/0xD4/0xD8`). La aritmética está verificada; los
   valores límite dependen del runtime.

Ninguna de estas limitaciones invalida los hechos de §13.1: afectan solo a
campos y ramas que no se han podido cerrar por lectura estática.

---

## 15. Correcciones a U-1g

| Punto de U-1g | Afirmación previa | Corrección en U-1h |
|---|---|---|
| §2.5 tabla opcode→handler | desplazada una fila (`0x20`/`0x24` → `0x465A89`, `0x30`/`0x34` → `0x465E5A`, …) | tabla correcta en §8.1: `0x20`→`0x465A89`, `0x24`→`0x465E5A`, `0x28`→`0x46636C`, `0x2C`→`0x466B3E`, `0x30`→`0x4673E9`, `0x34`→`0x46789D`, `0x40`→`0x468512`, `0x48`→`0x46875B`, `0x4C`→`0x468B54`, `0xB4`→`0x467E65`, `0xF0`→`0x469268` |
| §3.1 "handler `0x30` (`0x00465E5A`)" | el índice `word[+0x1A]` pertenece al `0x30` | pertenece al `0x24` (`0x00465E5A`); el `0x30` es `0x004673E9` |
| §4.1 "`0x00425010` = caras del `.DFX` a comandos" | se insinuaba como emisor del nivel | es emisor de **modelo/objeto** (caras 12 B, vértices 16 B); el nivel usa `0x426256`/`0x426588` |
| §7.3 «`0x0041A1E0` recorre un árbol» (hipótesis) | U-1g **no** afirmó «dibujo» ni «colisión»: dejó el recorrido como hipótesis sin algoritmo | U-1h demuestra que es **colisión** segmento–BSP; el dibujo es `0x401C00`/`0x4269C2` |
| §10.2 / §11.3 «consulta esférica de *triggers*» | se presentaba como hecho | es una consulta esférica que marca polígonos `0x20`; su uso como *pickups*/triggers es **hipótesis** (§6.4) |
| §8.3 handler `0x30` | se le asignaba `grConstantColorValue4` | `0x30` solo llama `guColorCombineFunction` + `grDrawTriangle` (§8.3) |
| §11.7 reencadenado `0x64FB40` | `0x462296..0x4622AB`, `+0x12 = 0xE0` fijo | rango `0x462240..0x4622AB`; `+0x12 = 0x180` si `[level+0x6FB0] > 0`, si no `0xE0`; `+0x10 = 0x200`; `+0x00 = 0x14` (§10) |
| §11.7 «opcode 0 en la imagen» | el opcode valía 0 | el opcode `0xF0` se escribe en la inicialización `0x4635BD`; solo la longitud inicial era `4`, sustituida por `0x14` (§10) |
| §11.6 descarga de tabla | solo handlers `0x24`/`0xB4` | también `0x2C` (§11) |
| §11.1 solapamiento `+0x8`/`+0xB` | "no se puede decidir estáticamente" | resuelto: el opcode se escribe **después** del color (§9.1) |
| §11.2 longitud `0x20` del `0x30` | posible desbordamiento | resuelto: handler `0x4673E9` lee exactamente `0x20` B (§9.2) |
| §11.7 `0x64FB40` | "opcode 0, no-op" | es el rectángulo de fondo `0xF0` (§10) |
| §7.2/§3.3 `0x461C32` | descarga la tabla NCC | **no** la descarga; solo `grTexTextureMemRequired` y `grTexDownloadMipMap` (§11) |
| §11.3 estructura del `.DFX` | "no se conoce el tamaño ni el significado de los nodos" | nodo `0x20` B, unión interno/hoja, descriptor y cadenas demostrados (§3, §4) |

Las afirmaciones de U-1g que siguen siendo válidas (un comando por llamada,
lista enlazada, pool de doble buffer, índice de 14 bits, orden `.VFX` antes que
`.DFX`, contenedor relocalizable) se mantienen y se han reutilizado aquí.

---

## 16. Relación con las fases siguientes

- **U-1h queda completada** en lo que respecta a: contenedor, descriptor,
  nodo, polígono, vértice, normal, auxiliar UV, recorridos, emisores, tabla de
  opcodes, layouts y resolución de las anomalías de U-1g.
- **Subfase C (U-1)** puede cerrarse: la geometría original está descrita de
  extremo a extremo, con la única deuda de los campos marcados como no
  identificados en §13.3 y §14.
- **Port gráfico:** ya es posible (a) extraer la geometría de un nivel
  recorriendo el BSP con las reglas de §4/§5 y (b) generar comandos
  equivalentes con los layouts de §8.2. No es necesario reproducir el pool ni
  el dispatcher: el port puede construir sus propios *batches*.
- **Fase 3 no se inicia en esta ronda.** Cualquier trabajo de implementación
  queda fuera del alcance read-only de U-1.

---

## 17. Documentación actualizada

* `U1H_RESULTADOS.md` (este documento; §18 añade la auditoría final del
  2026-11-07).
* `ROADMAP.md` — U-1h marcada como completada; Subfase C cerrada y auditada.
* `CHANGELOG.md` — entrada U-1h y entrada de auditoría final.
* `PROJECT_STATE.md` — estado vigente de U-1 y pendientes.
* `docs/U1_GEOMETRY_RECOVERY_PLAN.md` — U-1h completada, auditada y próximos pasos.
* `U1G_RESULTADOS.md` — notas de corrección, tabla de opcodes y precisiones de
  §7.3/§11.6/§11.7.
* `U1_CONSOLIDACION.md` — consolidación final de U-1: ámbitos, estado del
  descriptor y pendientes expresos.

`PROJECT_STATUS.svg` **no cambia**: no hay evolución de MAIN ni hito verificado
del panel; esto es investigación estática en `dev/gex`.

---

## 18. Auditoría final y contraste con el ejecutable (2026-11-07)

### 18.1 Alcance y método

Se releyó cada afirmación de U-1h contra el desensamblado completo del EXE
original (`gex_full.asm`, generado con `llvm-objdump` en la ronda anterior) y se
revalidaron los assets en modo *read-only*. Los thunks de Glide se comprobaron
contra la tabla de imports del PE (no por suposición): `0x65F4A8` =
`guColorCombineFunction`, `0x65F4B0` = `grDrawTriangle`, `0x65F49C` =
`grConstantColorValue4`, `0x65F4D0` = `grTexDownloadTable`, `0x65F4CC` =
`grDrawPlanarPolygonVertexList` y `0x65F45C` = `grDrawLine`.

No se ejecutó el juego, no se abrió ninguna ventana, no se modificó el ejecutable
ni ningún fichero de `C:\GEX_REZ` y no se instaló ninguna herramienta. Esta
auditoría solo corrige lo que el código y los datos permiten afirmar.

### 18.2 Correcciones aplicadas

| Sección | Afirmación anterior | Corrección | Evidencia |
|---|---|---|---|
| §0.6 | «`0x41A203` es colisión (ya apuntado en U-1g)» | U-1g §7.3 **no** lo clasificó: lo dejó como hipótesis sin algoritmo | texto original de `U1G_RESULTADOS.md` §7.3 |
| §4.2 | el radio del nodo se usa en `0x401C00` y en `0x4024B9` | solo lo usa `0x401C00`; `0x4024B9` usa `radio_esfera + 320` | `0x401C87`, `0x401E7B`, `0x401F2D`; `0x4026A7..0x4026B0` |
| §5.5, §9.2 | el índice de textura estaba descrito como `+0x1A`/`+0x20` sin distinguir handlers | `0x24`/`0x2C` leen `rec+0x1A`; `0x34`/`0xB4` leen `rec+0x1E`; `0x24` lee UV2 en `+0x20/+0x21`; `word[aux+2]` no se usa | `0x466115`, `0x4670D3`, `0x467B5E`, `0x468126`, `0x466297`, `0x4662BF` |
| §6.4 | «el radio² sale de `[[0x6466A8]+0x18]`» y los *pickups* como hecho | cadena completa: `[[0x6466A8]+0x18]+0x1C → ctx[0] → sphere+0x8`; los *pickups* son **hipótesis** | `0x5015B0..0x5015B8`, `0x5016D5..0x5016DD`, `0x5019AF` |
| §8.1 | no-op `0x50..0x80` y `0xB0` | no-op `0x50..0xB0` (incluye `0x84..0xAC`) | volcado del mapa `0x469765` y tabla `0x469735` |
| §8.1, §8.2, §8.3 | `0x2C` descrito como «textura + triángulo», longitud `0x2C` con 3 vértices y 1 `grDrawTriangle` | `0x2C` es un **cuadrilátero** de 4 vértices (stride 8) que dibuja **2 triángulos**; longitud `0x2C` correcta | `0x466B3E` lee `+0x0C/+0x14/+0x1C/+0x24` y `+0x0E/+0x16/+0x1E/+0x26`; dibuja en `0x4673CB` y `0x4673DF` |
| §8.3 | `0x30` llama a `grConstantColorValue4` | `0x30` solo llama `guColorCombineFunction` + `grDrawTriangle`; tabla completa corregida | `0x467876`, `0x467893`; imports `0x65F4A8`, `0x65F4B0`, `0x65F49C` |
| §10 | `0x0054FB40`; rango `0x462296..0x4622AB`; `+0x12 = 0xE0` fijo | `0x0064FB40`; rango `0x462240..0x4622AB`; `+0x12` condicional (`0x180`/`0xE0`); `+0x10 = 0x200`; `+0x00 = 0x14` | `0x4635B3..0x4635F6`; `0x462240..0x4622AB` |
| §11 | la tabla se descarga en `0x24`/`0xB4` | también en `0x2C` | `0x4670EB..0x4670FC` y equivalentes |
| §13, §15 | «consulta esférica de *triggers*» como hecho; cita de U-1g §7.3 | hipótesis de recogida; la cita decía «recorre un árbol», no «dibujo» | §6.4 y `U1G_RESULTADOS.md` §7.3 |

### 18.3 Evidencias de los puntos centrales

- **Colisión `0x0041A203`:** en hojas prueba el AABB con `+0x14/+0x1A`,
  `+0x16/+0x1C` y `+0x18/+0x1E`; filtra con `flag & 0x4480` y exige
  `flag & 0x100` (`0x41A3AE`); en el impacto descarta `0x4000` y, si está
  `0x40`, exige `0x100` (`0x41A9C0..0x41A9EB`); usa `dist² < 0x10000`; el hijo A
  es el lado positivo de la normal y el épsilon es `[0x54D04C] = 320`.
- **Visibilidad `0x00401C00`:** lee el radio del nodo en `+0x06`
  (`0x401C87`, `0x401E7B`, `0x401F2D`, `0x401F43`); prueba contra
  `cam+0x5C` y los planos `cam+0xC8/0xD4/0xD8`; el anexado tiene tope `0x800`;
  el llamador único es `0x43DECE` y `level[+0x4C94] & 0x200` bloquea el dibujo
  (`0x43DEF3..0x43DF01`).
- **Consulta `0x004024B9` + callback `0x005015A9`:** en los nodos internos
  compara el plano con `radio_esfera + 320` (`0x4026AC..0x4026BC` y
  `0x4026F6..0x4026FC`); en las hojas llama al callback directamente
  (`0x402621`). El callback exige `0x20` (`0x5015F3`), salta `0x8000`
  (`0x501605`) y `0x40` (`0x50161A`), compara `dist² < r²`
  (`0x5016B0..0x5016DD`), guarda el polígono en `ctx+8` (`0x5016E2`) y marca
  `0x8000 | 0x40` (`0x5016FE`, `0x501710`). `0x5019AF` construye la esfera con
  radio `0x640` y `radio² = 0x271000` (`0x501986`, `0x50198C`).
- **Emisores:** `0x426256` escribe el registro sólido de `0x18` B con opcode
  `0x20` y el texturizado de `0x34` B con opcode `0xB4|((color>>24)&3)`;
  `0x426588` escribe el registro de `0x34` B con opcode `0xB6` y `0x4000` en
  `word[+0x1E]`, más un segundo registro de `0x24` B con prefijo `0xE1` y opcode
  `0x32` en `+0x0F`.
- **Handlers de textura:** `0x24`, `0x2C` y `0xB4` descargan la tabla solo si
  `record[+0x0C] == 1`, con `grTexDownloadTable(0, 0, record+0x14)`; `0x34` y
  `0xB4` no llaman a `grConstantColorValue4`.
- **Cuadrilátero `0x2C`:** `0x466B3E` lee cuatro posiciones (`+0x0C/+0x14/+0x1C/+0x24`
  y `+0x0E/+0x16/+0x1E/+0x26`), cuatro pares UV (`+0x10/+0x18/+0x20/+0x28`), el
  color en `+0x08/+0x09/+0x0A` y el índice de textura en `word[+0x1A]`; dibuja
  dos triángulos (`0x4673CB`, `0x4673DF`). El `stride` es 8 B/vértice y la
  longitud `0x2C`. No es un triángulo.
- **Carga `.VFX` `0x00461C32`:** `record[+0x10] = record+0x8C` (`0x461CA5`),
  fin `record+0x8C+record[+0x84]` (`0x461CDB`), `grTexTextureMemRequired`
  (`0x461CF3`), `grTexDownloadMipMap` (`0x461DEE`), `record[+0x88] =
  [0x6487C4]` (`0x461DD0`); con `record[0] < 7 && record[+0x0C] == 1` copia
  `byte[+0x8C]` a `+0x8D/+0x8E` (`0x461D63..0x461DB9`). **No hay** ninguna
  llamada a `grTexDownloadTable` en el rango.
- **Fondo `0x0064FB40`:** inicialización en `0x4635B3..0x4635F6` (longitud `4`,
  opcode `0xF0`, RGB 0, `x/y = 0`, `0x200×0x180`); reencadenado en
  `0x462240..0x4622AB` con `+0x12 = 0x180` y `+0x10 = 0x200` si
  `[0x551644][+0x6FB0] > 0`, si no `+0x12 = 0xE0`; `+0x0E =
  level[+0x14]*0x180`; `+0x00 = 0x14`; enlace en `0xBFF - [ebp-8]`.
- **Despacho:** guarda `idx <= 0xD0` (`0x465A64`), mapa `0x469765`
  (`0x465A7C`), tabla `0x469735` (`0x465A82`). El volcado del mapa confirma
  no-op en `0x38..0x3C`, `0x44`, `0x50..0xB0` y `0xB8..0xEC`.
- **Estadísticas del BSP:** radio `⌈máx. distancia centro→vértice⌉` con
  tolerancia ±2 en 814/815 (`AZTEC16`), 500/501 (`CIRCUIT0`) y 89/89
  (`REZOP2`); centro dentro del AABB en 815/815, 501/501 y 89/89; normal
  unitaria en 814/814, 500/500 y 88/88.
- **Flags:** el reparto de §5.2 muestra **0 polígonos con `0x20`** en los tres
  niveles; los demás bits con consumidor (`0x8000`, `0x4000`, `0x40`, `0x100`)
  tienen el uso indicado en §5.2.

### 18.4 Resultados negativos confirmados

- No hay ningún polígono con `0x20` en los tres niveles auditados: el efecto de
  `0x005015A9` no se ha observado con datos reales y su uso de juego sigue sin
  confirmar.
- `0x00461C32` no descarga tablas de textura; la descarga es por comando en
  `0x24`, `0x2C` y `0xB4`.
- `0x0041A203` no dibuja y `0x004024B9` no usa el radio del nodo.
- `GEX.DFX` no contiene BSP ni descriptor de nivel.
- No se ha localizado consumidor para `desc+0x14`, `desc+0x30`, `desc+0x34` ni
  para los bits de flag sin marcar de §5.2.
- No se ha localizado ningún emisor que escriba el opcode `0x2C` de forma
  literal. El handler `0x466B3E` y su layout de cuadrilátero están demostrados,
  pero la ruta de emisión que lo produce queda pendiente.

### 18.5 Conclusión

Los hechos centrales de U-1h se mantienen: contenedor, descriptor, nodo,
polígono, vértice, normal, auxiliar UV, recorridos, emisores, despacho y
resolución de las anomalías de U-1g. Las correcciones son de **precisión y
alcance**, no invalidan la interpretación estática. Subfase C sigue cerrada; las
cuestiones que exigen ejecución permanecen en §14.

---

## 19. Consolidación documental de U-1 (2026-11-07)

Esta sección no repite la evidencia: fija el estado final y los pendientes.
El documento de referencia es [`U1_CONSOLIDACION.md`](U1_CONSOLIDACION.md).

### 19.1 Tres ámbitos distintos

| Ámbito | Estado | Qué falta |
|---|---|---|
| Geometría BSP de nivel | demostrada de extremo a extremo | consumidores de campos del descriptor; una rama sin datos reales (`0x20`) |
| Geometría de modelos | parcial | formato de `GEX.DFX`; vértices de 16 B de `0x425010` |
| Formatos de comandos | parcial | emisor de `0x2C`; layout de Glide de `0x28`; propósito del segundo registro de `0x426588` |

### 19.2 Correcciones aplicadas en esta sección

* §0.2 y §3.1: el descriptor **no** está identificado al 100 %; estado campo
  a campo con 10 demostrados, 1 hipótesis, 3 sin identificar y 2 parciales.
* §7.3 y §12: `0x425010` usa vértices de 16 B; `0x4258BD` usa vértices de
  12 B. No comparten stride.
* Se registran como pendientes expresos: `GEX.DFX`, el emisor de `0x2C`, el
  layout de `0x28` y el segundo registro de `0x426588`.

### 19.3 Pendientes expresos

1. **`GEX.DFX`**: modelo/personaje, no nivel; formato no descodificado.
2. **`0x2C`**: handler de cuadrilátero demostrado; **sin emisor localizado**.
   Los stores de `0x2C` localizados son longitudes de registros de `0x34`.
3. **`0x28`**: emisores `0x42DEC1` y `0x504C44` localizados (registro de
   `0x1C` B, color en `+8`, cuatro pares `xy` en `+0x0C..+0x1A`); falta el
   layout de las cuatro estructuras de `0x3C` B que `0x46636C` pasa a
   `grDrawPlanarPolygonVertexList` en `0x466B34`.
4. **Segundo registro de `0x426588`**: `0x24` B, prefijo `0xE1`, opcode `0x32`
   en `+0x0F`; se emite siempre, propósito sin establecer.

Ninguna de estas correcciones cambia los hechos centrales de U-1h: contenedor,
descriptor en su parte demostrada, nodo, polígono, vértice, normal, auxiliar
UV, recorridos, emisores de nivel y despacho siguen verificados.

---

## Anexo A — Tabla de funciones citadas

| Dirección | Papel | Evidencia |
|---|---|---|
| `0x00401C00` | recorrido de visibilidad BSP → `[0x64FEA4]` | llamador `0x43DECE`; anexa hojas con tope `0x800` |
| `0x0040172B` | setup de proyección/cámara | instalado en `level+0x4CA4`; llamado en `0x43DDE9` |
| `0x004024B9` | consulta esfera–BSP | llamador `0x5019AF`; callback `0x5015A9` |
| `0x0041A1E0` | envoltorio de colisión | `0x41A1F9` llama a `0x41A203` con máscaras 0 |
| `0x0041A203` | colisión segmento–BSP | §6.3 |
| `0x00419850` | resuelve normal con signo | §5.1 |
| `0x00425010` | emisor de todas las caras de modelo | `faceSet` 12 B/16 B; §7.3 |
| `0x004258BD` | emisor de rango de caras de modelo | §7.3 |
| `0x004269C2` | emisor de hojas visibles | §6.2 |
| `0x00426256` | emisor sólido/texturizado de nivel | §7.1 |
| `0x00426588` | emisor texturizado + registro sólido | §7.2 |
| `0x0043B11D` | orquestador de carga de nivel | §2.2 |
| `0x0043DC39` | render de nivel por frame | instala callbacks, llama a `0x401C00` y `0x4269C2` |
| `0x00461B90` | lector de fichero completo | base de `.DFX`/`.VFX` |
| `0x00461C32` | carga `.VFX` | §11 |
| `0x00461E0B` | carga `.DFX` y compacta | §2.2 |
| `0x0046558A` | dispatcher de un comando | §8 |
| `0x00469836` | recorrido de la lista de comandos | §8 |
| `0x0046999C` | transformación por matriz (3 *dots*) | §4.3, §6.1 |
| `0x0046A1D0` / `0x0046B727` | proyección de vértices | §6.2 |
| `0x004735C0` | relocalizador | §2.2 |
| `0x00473238` | consumidor de la lista de objetos | §3.4 |
| `0x005015A9` | callback de la consulta esférica | §6.4 |
| `0x005019AF` | llamador de la consulta esférica | §6.4 |

---

## Anexo B — Reproducción de las validaciones

Los scripts de `%TEMP%\opencode` son desechables y no se versionan. La
validación se puede repetir con cualquier lector binario aplicando estas
comprobaciones:

1. **Contenedor:** leer `count`, calcular
   `aligned = (count + 0x200) & ~0x1FF`, comprobar que `4*aligned` es el offset
   del descriptor (`image[0]`).
2. **Descriptor:** comprobar las tres igualdades de §3.2 con los contadores
   `+0x18`, `+0x1C`, `+0x20`.
3. **BSP:** recorrer desde `desc[0]` con la pila de hijos `+0x18`/`+0x1C`;
   comprobar que `word[+0x12]` solo toma `1`/`2`, que no hay ciclos y que la
   suma de `word[+0x10]` de las hojas es `desc[+0x1C]`.
4. **Nodo:** comprobar `|normal| = 4096 ± 1` en los internos y que el centro
   cae dentro del AABB en las hojas.
5. **Polígonos:** comprobar que cada `+0x10` cae dentro de la imagen, está
   alineado a 4 y que los índices de normal resueltos con signo están en
   `[0, desc[+0x20])`.
6. **Auxiliar UV:** comprobar que `word[aux+6] & 0x3FFF` es menor que el número
   de registros de `0x6488F4` cuando se disponga del `.VFX` correspondiente.

Estas comprobaciones son las que se aplicaron a `AZTEC16`, `CIRCUIT0` y
`REZOP2`, y las que permitieron detectar que `GEX.DFX` no sigue el formato de
nivel.
