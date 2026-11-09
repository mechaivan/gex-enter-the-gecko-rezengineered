# S-2 — Recorrido BSP de AZTEC16

**Objeto:** recorrido estructural del BSP de niveles (S-2, primer escenario original), sin visibilidad original de cámara, sin emisión de polígonos, sin buffers D3D11, shaders ni texturas.

**Referencias:** `S1_DFX_PARSER.md`, `U1_CONSOLIDACION.md`, `U1H_RESULTADOS.md`, `docs/D3D11_FIRST_SCENE_PLAN.md`, `U1_DFX_RELOCATION_AUDIT.md`.

**Estado actual:** recorrido estructural implementado y validado en `port/`. No se declara S-2 completo: la visibilidad original y la emisión de geometría siguen pendientes.

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
| +0x06 | int16 | radio |
| +0x12 | uint16 | tipo (bit 1 = hoja) |
| +0x18 | uint32 | hijo 1 (offset relativo a la imagen; 0 = ausente) |
| +0x1C | uint32 | hijo 2 (offset relativo; 0 = ausente) |

## 3. Evidencia que lo respalda

- `U1H_RESULTADOS.md` §6.1: firma `void walk(descriptor*, camera*, list*)`,
  pila explícita, arranque con `desc[0]` (raíz = `descriptor.root_bsp`),
  anexa hojas con tope `0x800` (`out[4 + 4*out[0]] = leaf`).
- Nodo interno apila `+0x18` y `+0x1C` si no son nulos.
- El descriptor se obtiene del contenedor vía `gex_dfx_load_level`
  (offset = `image[0]`); `root_bsp` está en `+0x00` del descriptor.

## 4. Dependencias no resueltas (documentadas, no inventadas)

- La prueba de visibilidad original (esfera vs. volumen de vista) requiere la
  cámara: matriz de vista desde `cam+0x88` (7 int16), `cam+0x5C`, planos en
  `cam+0xC8/+0xD4/+0xD8`, y `0x0046999C` para transformar centros. La
  población de la cámara y la matriz no se reproduce en S-2; se inyectan como
  predicado. Esto es una limitación explícita.
- `0x004269C2` (emisión de hojas visibles) y los emisores `0x00426256`/
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

Cifras históricas (7, 18, 23, 44 comprobaciones) corresponden a estados
intermedios y no son el resultado vigente. El resultado vigente es 29
comprobaciones, 0 fallos.

## 6. Limitaciones

- Sin visibilidad real (requiere cámara, no implementada).
- Sin emisión de polígonos ni buffers.
- Sin D3D11, shaders, texturas.
- `PROJECT_STATUS.svg` sin cambios (MAIN sigue en 40 %; S-2 no completo).

---

**No se declara S-2 completo**: la visibilidad original y la emisión de
geometría siguen pendientes.
