# S-2 — Recorrido BSP de AZTEC16

**Objeto:** recorrido estructural del BSP de niveles (S-2, primer escenario original), sin visibilidad original de cámara implementada, sin emisión de polígonos, sin buffers D3D11, shaders ni texturas.

**Referencias:** `S1_DFX_PARSER.md`, `U1_CONSOLIDACION.md`, `U1H_RESULTADOS.md`, `S2_CAMERA_VISIBILITY.md`, `docs/D3D11_FIRST_SCENE_PLAN.md`, `U1_DFX_RELOCATION_AUDIT.md`.

**Estado actual:** recorrido estructural implementado y validado en `port/`.
La visibilidad original de cámara está **documentada** (no implementada) en
[`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md). No se declara S-2
completo: la emisión de geometría sigue pendiente.

---

## 1. Estado actual (vigente)

- Módulo `port/`: `include/gex/bsp.h`, `src/bsp/bsp.c`, `tests/test_bsp.c`.
- API: `gex_bsp_walk(image, image_size, root_offset, visible, ctx, out)`,
  `gex_bsp_walk_free`, `gex_bsp_node_type`, `gex_bsp_node_is_leaf`.
- Algoritmo: DFS iterativo con marcos enter/exit, protección de ciclos (coloreado
  de 3 estados), referencias compartidas procesadas una vez, tope de 0x800 hojas.
- Lecturas seguras por bytes (`READ_U16`/`READ_U32`), `node_in_bounds` con resta
  anti-overflow, capacidades independientes, liberación en todos los caminos.
- Instrumentación de fallos exclusiva de tests (`GEX_BSP_TEST_FAULT_INJECT`),
  declarada solo en `test_bsp.c`, no en la API pública.
- Pruebas: **29 comprobaciones, 0 fallos**. ASan+UBSan sin errores.
  AZTEC16.DFX = **exactamente 815 hojas** (predicado NULL).
- Compilación normal (sin macro): sin símbolos de inyección.
- Compilación de tests:
  `clang -DGEX_BSP_TEST_FAULT_INJECT -Wall -Wextra -Wpedantic -std=c11 -O2 -I include tests/test_bsp.c src/bsp/bsp.c src/asset/dfx.c -o tests/test_bsp.exe`

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
reales 1/2 equivale a `& 2 != 0`, que es lo que usa `port/`; no son equivalentes
para otros valores de tipo.

## 3. Evidencia que lo respalda

- `U1H_RESULTADOS.md` §6.1: firma `void walk(descriptor*, camera*, list*)`,
  pila explícita, arranque con `desc[0]` (raíz = `descriptor.root_bsp`),
  anexa hojas con tope `0x800` (`out[4 + 4*out[0]] = leaf`).
- Nodo interno: prueba de plano
  `d = ((camPos · N) >> 12) − node+0x14`. Si `d >= 0` apila primero `+0x1C` y
  luego `+0x18` (se visita `+0x18` antes); si `d < 0`, al revés.
- El descriptor se obtiene del contenedor vía `gex_dfx_load_level`
  (offset = `image[0]`); `root_bsp` está en `+0x00` del descriptor.

## 4. Visibilidad original (documentada en `S2_CAMERA_VISIBILITY.md`)

La visibilidad de `0x00401C00` **no** usa planos en `cam+0xC8/+0xD4/+0xD8`.
Usa dos matrices `int16` de 3×3, `cam+0x88` (**A**, 9 palabras) y `cam+0xA8`
(**B**, 9 palabras), construidas por `0x00405C41` a partir de `M = *[cam+0xE8]`
y aplicadas al centro del nodo con `0x0046999C` (`(M·v) sar 12`). `M` es el
resultado de la cadena de orientación
`0x46DD40` → `0x46E643` (documentada en
[`S2_CAMERA_VISIBILITY.md` §6](S2_CAMERA_VISIBILITY.md)): rotaciones Euler
**Z → Y → X** con ángulos negados sobre `cam+0x30/0x32/0x34` (o
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

Para implementar la visibilidad fielmente será necesario reproducir también la
cadena de orientación (`0x46DD40` → `0x46E643` → `0x405C41` con la aritmética
int16/Q12 documentada en [`S2_CAMERA_VISIBILITY.md` §6](S2_CAMERA_VISIBILITY.md)),
incluida la normalización aproximada de `0x00402DC0`.

`0x004269C2` (emisión de hojas visibles) y los emisores `0x00426256`/
`0x00426588` no se implementan en S-2 (fuera de alcance: buffers D3D11).

## 5. Historial de auditorías (no es estado actual)

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
  `0x46E643` → `0x405C41`: rotaciones Euler Z→Y→X con ángulos negados, matriz
  base o identidad, cambio de ejes `M2`, `M = transpose(O)`, ramas alternativa
  y base forzada) (solo documentación; sin cambios de código).

Cifras históricas (7, 18, 23, 44 comprobaciones) corresponden a estados
intermedios y no son el resultado vigente. El resultado vigente es 29
comprobaciones, 0 fallos.

## 6. Limitaciones

- Sin visibilidad implementada: está **documentada** en
  [`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md), pero `port/` sigue
  usando un predicado inyectado.
- Sin emisión de polígonos ni buffers.
- Sin D3D11, shaders, texturas.
- `PROJECT_STATUS.svg` sin cambios (MAIN sigue en 40 %; S-2 no completo).

---

**No se declara S-2 completo**: la visibilidad original está documentada pero no
implementada, y la emisión de geometría sigue pendiente.
