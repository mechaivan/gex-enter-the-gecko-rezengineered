# S-2 — Recorrido BSP de AZTEC16 (2026-11-08)

**Objeto:** implementación del recorrido BSP original (primer escenario), sin
buffers D3D11, sin shaders ni texturas, sin integración con el renderer.

**Referencias:** `S1_DFX_PARSER.md`, `U1_CONSOLIDACION.md`, `U1H_RESULTADOS.md`,
`docs/D3D11_FIRST_SCENE_PLAN.md`, `U1_DFX_RELOCATION_AUDIT.md`.

**Estado:** recorrido estructural implementado y probado. La visibilidad original
(esfera vs. volumen de vista) se inyecta como predicado opcional; con predicado
NULL se aceptan todas las hojas (útil para verificar la estructura).

---

## 1. Arquitectura

Módulo `port/` (nuevo):
- `include/gex/bsp.h`, `src/bsp/bsp.c`, `tests/test_bsp.c`.

API:
```c
gex_bsp_status gex_bsp_walk(const uint8_t *image, size_t image_size,
                            uint32_t root_offset,
                            gex_bsp_visible_fn visible, void *ctx,
                            gex_bsp_walk_result *out);
void gex_bsp_walk_free(gex_bsp_walk_result *r);
```

El recorrido:
- Pila explícita de offsets de nodo (capacidad dinámica).
- Conjunto de visitados (lineal) para evitar ciclos.
- Para cada nodo: comprobación de límites (`node_offset + 32 <= image_size`),
  lectura de tipo en `+0x12` (bit 1 = hoja), hijos en `+0x18`/`+0x1C`.
- Si es hoja y hay capacidad (< 0x800), se anexa a la lista.
- Si es interno, se apilan los hijos no nulos.
- Predicado de visibilidad opcional; NULL acepta todo.

## 2. Nodo BSP (0x20 B)

| Offset | Tipo | Campo |
|---|---|---|
| +0x00 | int16 | centro X |
| +0x02 | int16 | centro Y |
| +0x04 | int16 | centro Z |
| +0x06 | int16 | radio |
| +0x12 | uint16 | tipo (bit 1 = hoja) |
| +0x18 | uint32 | hijo 1 (offset relativo) |
| +0x1C | uint32 | hijo 2 (offset relativo, 0 = ausente) |

## 3. Evidencia que lo respalda

- `U1H_RESULTADOS.md` §6.1: firma `void walk(descriptor*, camera*, list*)`,
  pila en `0x647740`, arranque con `desc[0]` (raíz = descriptor.root_bsp),
  anexa hojas con tope `0x800` (`out[4 + 4*out[0]] = leaf`).
- Nodo interno apila `+0x18` y `+0x1C` si no es nulo.
- El descriptor se obtiene del contenedor vía `gex_dfx_load_level`
  (offset = `image[0]`); `root_bsp` está en `+0x00` del descriptor.

## 4. Dependencias no resueltas (documentadas, no inventadas)

- La prueba de visibilidad original (esfera vs. volumen de vista) requiere la
  cámara: matriz de vista desde `cam+0x88` (7 int16), `cam+0x5C`, planos en
  `cam+0xC8/+0xD4/+0xD8`, y `0x0046999C` para transformar centros. La
  población de la cámara y la matriz no se reproduce en S-2; se inyectan como
  predicado. Esto es una limitación explícita, no un valor inventado.
- `0x004269C2` (emisión de hojas visibles) y los emisores `0x00426256`/
  `0x00426588` no se implementan en S-2 (fuera de alcance: buffers D3D11).

## 5. Pruebas

Suite `port/tests/test_bsp.c`: 7 comprobaciones, 0 fallos. Cubre nodo interno y
hojas, recorrido de ambos hijos, referencias inválidas y límites, y recorrido
del contenedor original `AZTEC16.DFX` (815 hojas con predicado NULL).

Regresiones existentes: sin fallos.

## Segunda auditoría correctiva (2026-11-08)

Defectos de la primera corrección y su causa raíz:

1. **DFS no auténtico.** El código marcaba el nodo como terminado
   (`vis_col = 2`) inmediatamente después de apilar sus hijos, por lo que el
   estado 1 no permanecía activo durante el descenso y la detección de ciclos
   no funcionaba. Corregido con marcos de entrada/salida (enter/exit): el nodo
   se marca en proceso (1) al entrar, se apila un marco de salida que lo marca
   terminado (2) tras procesar los hijos, y los hijos se apilan como marcos de
   entrada.
2. **Capacidad compartida.** Una única variable `cap` se usaba para la pila DFS,
   la tabla de offsets visitados y la tabla de estados; el `realloc` parcial
   podía perder el bloque original. Ahora cada estructura tiene su propia
   capacidad (`fr_cap`, `vis_cap`, `leaves_cap`) y los `realloc` se asignan a
   variables temporales antes de sobrescribir.
3. **`calloc` temporal de 65536 bytes** eliminado.
4. **Ciclos reales.** La raíz se permite en offset distinto de cero (el offset 0
   sigue reservado como hijo ausente). Las pruebas construyen autorreferencia y
   ciclos de dos nodos con raíz en `GEX_BSP_NODE_SIZE`.
5. **Referencias compartidas** se procesan una sola vez (nodo en estado 2 se
   omite sin error).

Nuevos casos de prueba (23 comprobaciones, 0 fallos): autorreferencia, ciclo de
dos nodos, referencia compartida, hijo fuera de límites, raíz inválida, imagen
nula/truncada, predicado de visibilidad, límite exacto de 0x800 hojas y
superación (`GEX_BSP_ERR_LEAF_CAP`), AZTEC16.DFX (exactamente 815 hojas).

Limitaciones que permanecen: visibilidad original (cámara), emisión de
polígonos, buffers, shaders, texturas. S-2 no se declara completo.

1. **Detección de ciclos:** el conjunto de visitados anterior marcaba un nodo
   como visitado al primer encuentro y lo saltaba en el segundo, confundiendo
   ciclos con referencias compartidas válidas. Sustituido por un coloreado de
   tres estados (0 no visitado, 1 en proceso, 2 terminado): un nodo en estado 1
   reencontrado es un **ciclo** (`GEX_BSP_ERR_CYCLE`); un nodo en estado 2 es
   una **referencia compartida** válida y se omite sin error.
2. **Límite de hojas:** alcanzar `GEX_BSP_LIST_CAP` (0x800) devolvía
   truncamiento silencioso. Ahora devuelve `GEX_BSP_ERR_LEAF_CAP` y deja el
   resultado a cero.
3. **Lecturas binarias:** eliminados los casts directos de puntero
   (`*(uint16_t*)...`); se usan lecturas por bytes (`READ_U16`/`READ_U32`)
   compatibles con C11 y seguras ante desalineación.
4. **Validación de argumentos:** `gex_bsp_node_type` y `gex_bsp_node_is_leaf`
   protegen contra imagen nula/tamaño insuficiente; `gex_bsp_walk` valida
   `image`, `out` y `root_offset`.
5. **Aritmética de límites:** `node_in_bounds` y las lecturas de campos
   comprueban desbordamiento (`node_offset + campo + tamaño <= image_size`).
6. **Memoria:** todos los caminos de error liberan `stack`, `vis_off`, `vis_col`
   y `leaves`, y dejan `out` a cero.

Nuevos casos de prueba: árbol con dos hojas, referencia compartida, hijo fuera de
límites, raíz inválida, imagen nula, imagen truncada, predicado de visibilidad
que acepta/rechaza, AZTEC16.DFX (815 hojas).

Pruebas: 18 comprobaciones, 0 fallos.

Limitaciones que permanecen: visibilidad original (cámara), emisión de
polígonos, buffers, shaders, texturas. S-2 no se declara completo.


## Quinta auditoria (2026-11-08) - aislamiento de fallos y pruebas deterministas

Cambios:

1. Mecanismo de inyeccion de fallos (gex_bsp_set_grow_visited_fail /
   gex_bsp_reset_grow_visited_fail) movido a compilacion condicional
   (GEX_BSP_TEST_FAULT_INJECT). Eliminado de la API publica (bsp.h). La
   compilacion normal usa malloc directamente, sin estado global.
2. Pruebas deterministas de fallo de la primera y segunda reserva de
   grow_visited: cada una devuelve GEX_BSP_ERR_MEMORY con resultado limpio;
   ejecucion normal posterior funciona.
3. Regresion completa: 72 comprobaciones, 0 fallos. ASan+UBSan ejecutados sin
   errores. AZTEC16 = exactamente 815 hojas.

Limitaciones que permanecen: visibilidad original (camara), emision de
poligonos, buffers, shaders, texturas. S-2 no se declara completo.

Correcciones:

1. grow_visited (y grow_u32, grow_frames) reciben ahora el numero de elementos
   validos (nvis/fr_sp/nleaves) y copian exclusivamente esos, no toda la
   capacidad reservada.
2. node_in_bounds usa comprobacion por resta: `node_offset <= image_size` y
   `GEX_BSP_NODE_SIZE <= image_size - node_offset`, evitando overflow en 32 bits.
3. Mecanismo de inyeccion de fallos de asignacion (gex_bsp_set_alloc_fail_after /
   gex_bsp_reset_alloc) exclusivo de tests, sin dependencias externas. Demuestra
   fallo al reservar arrays y resultado GEX_BSP_ERR_MEMORY con resultado limpio.

Pruebas: 44 comprobaciones, 0 fallos. ASan+UBSan ejecutados sin errores.
AZTEC16 = exactamente 815 hojas.

Limitaciones que permanecen: visibilidad original (camara), emision de
poligonos, buffers, shaders, texturas, inyeccion de fallos en codigo de
produccion. S-2 no se declara completo.

Defecto confirmado: en los bloques de ampliacion de vis_off/vis_col, el realloc
secuencial podia dejar el primer puntero actualizado y el segundo fallido.
Corregido sustituyendo ambos realloc por reservas independientes con malloc
(ambas comprobadas antes de liberar los arrays anteriores), copia, liberacion y
sustitucion atomica. Todos los demas realloc (pila, hojas) tambien sustituidos
por malloc+copia+free con variables temporales.

Auditoria adicional: overflow en duplicacion de capacidades comprobado
(newcap < *cap); overflow en multiplicaciones por sizeof comprobado contra
SIZE_MAX; conversiones uint32_t->size_t seguras; incrementos de pila y
contadores comprobados.

Coste de la busqueda lineal de visitados: O(n^2) en el peor caso. Aceptable para
BSP de niveles; se propone (sin implementar) tabla hash o sort por offset para
escenas muy grandes.

Pruebas: 23 comprobaciones, 0 fallos. AZTEC16 = exactamente 815 hojas.
Compilacion C11 con -Wall -Wextra limpia.

Limitaciones que permanecen: visibilidad original (camara), emision de
poligonos, buffers, shaders, texturas, inyeccion de fallos de asignacion en
pruebas. S-2 no se declara completo.
