# S-1 — Parser del contenedor DFX (2026-11-08)

**Objeto:** implementación del parser del contenedor `.DFX` (S-1, primer hito del
primer escenario original), sin recorrido BSP, sin buffers D3D11, sin shaders ni
texturas.

**Referencias:** `U1_DFX_RELOCATION_AUDIT.md`, `U1_CONSOLIDACION.md`,
`U1H_RESULTADOS.md`, `U1G_RESULTADOS.md`, `docs/D3D11_FIRST_SCENE_PLAN.md`.

**Estado:** parser + relocalizador + carga de nivel implementados y probados.
**S-1 no se marca como completado**: el parser es el primer hito; el recorrido
BSP, los buffers y el renderizado siguen pendientes.

---

## 1. Arquitectura y responsabilidades

Tres responsabilidades separadas, en `port/src/asset/dfx.c`:

1. **Parser** (`gex_dfx_parse`): recibe un buffer y su tamaño, valida el
   contenedor, copia `count`, la tabla de offsets y la imagen en buffers propios.
   No modifica la entrada. No presupone que el DFX sea un nivel BSP.
2. **Relocalizador** (`gex_dfx_relocate` / `gex_dfx_relocate_raw`): operación
   independiente, `*(u32 *)(image + offset) += delta`. Recibe imagen, tabla y
   `delta` explícito; no presupone que `delta` sea la base original del buffer.
3. **Carga de nivel** (`gex_dfx_load_level`): extrae el descriptor y los punteros
   a vértices/polígonos/normales. Asume que los campos de la tabla son
   desplazamientos relativos a la imagen (delta 0). No asigna memoria adicional.

Lectura de fichero: `gex_dfx_load` (wrapper sobre `gex_dfx_parse`).

## 2. Formato binario implementado

```
+0x00  u32 count
+0x04  u32 offsets[count]        (posiciones de DWORD, relativas a la imagen)
+...   relleno a cero hasta 4*aligned
4*aligned: imagen                (tamaño lógico = tamaño_fichero - 4*aligned)
```

- `aligned = (count + 0x200) & ~0x1FF` (en DWORDs).
- Inicio de imagen: `4 * aligned` (bytes).
- Relocalización: `*(u32 *)(imageBase + off) += delta`.
- La imagen lógica se limita a los bytes realmente presentes
  (`image_size = size - 4*aligned`); el redondeo a 4 bytes de la reserva (si
  hiciera falta) se separa del tamaño lógico.

## 3. API pública

```c
gex_dfx_status gex_dfx_parse(const void *data, size_t size, gex_dfx_container *out);
gex_dfx_status gex_dfx_load(const char *path, gex_dfx_container *out);
gex_dfx_status gex_dfx_relocate(gex_dfx_container *container, uint32_t delta);
gex_dfx_status gex_dfx_relocate_raw(uint8_t *image, size_t image_size,
                                    const uint32_t *offsets, uint32_t count,
                                    uint32_t delta);
gex_dfx_status gex_dfx_load_level(const gex_dfx_container *container,
                                  gex_dfx_level *out);
void gex_dfx_release_container(gex_dfx_container *container);
void gex_dfx_release_level(gex_dfx_level *level);
const char *gex_dfx_status_name(gex_dfx_status status);
```

El descriptor se lee desde `image + image[0]` (desplazamiento al descriptor),
siguiendo el layout confirmado de `AZTEC16.DFX`.

## 4. Gestión de memoria y propiedad de buffers

- `gex_dfx_parse` asigna `offsets` e `image` con `malloc` y los copia; el
  llamante los libera con `gex_dfx_release_container`.
- `gex_dfx_load` lee el fichero en un buffer temporal, parsea y libera el
  temporal; el contenedor resultante es independiente.
- `gex_dfx_load_level` no asigna memoria: los punteros apuntan al buffer del
  contenedor.
- `NULL` es aceptado por las funciones de liberación.

## 5. Validaciones implementadas

- Argumentos nulos / tamaño mínimo.
- `count` no nulo y dentro de un límite razonable (`0x100000`).
- Anti-desbordamiento en `count * 4` y en `count + 0x200`.
- Cabecera y tabla dentro del buffer; imagen no vacía.
- `image_offset = 4*aligned` entre `header_bytes` y `size`.
- Cada offset: múltiplo de 4, dentro de la imagen, y el DWORD apuntado cae
  dentro de la imagen.
- El valor relativo apuntado cae dentro de la imagen.
- Descriptor: tablas de vértices/polígonos/normales dentro de la imagen, sin
  desbordamiento aritmético.
- Entrada inalterada (el parser solo lee).

## 6. Relocalización y uso de `delta`

- `gex_dfx_relocate_raw` recibe `image`, `image_size`, `offsets`, `count`,
  `delta` y aplica `*(u32 *)(image + off) += delta`, comprobando límites
  antes de escribir.
- `gex_dfx_relocate` es el envoltorio sobre un contenedor.
- Se probaron `delta = 0` y `delta != 0` (aditivo, comprobando valores
  resultantes).
- No se implementa `0x00461ECA`: sin llamadores estáticos demostrados y su
  copia truncada produce punteros colgantes en `AZTEC16`.

## 7. Pruebas

Suite en `port/tests/test_dfx.c`: 76 comprobaciones, 0 fallos. Cubre contenedor
mínimo válido, múltiples entradas, truncado, count cero, count excesivo,
alineación, offset fuera de imagen, punteros nulos, tamaño inválido, carga de
nivel, nombres de estado, relocalización con delta 0 y distinto de cero,
inmutabilidad de la entrada, liberación de memoria y validación con los DFX
originales.

Validación con originales (solo lectura, `C:\GEX_REZ`):
- `AZTEC16.DFX`: 544 586 B, count 16 082, aligned 16 384, imagen 479 050 B,
  carga de nivel correcta (3 882 vértices).
- `GEX.DFX`: 146 008 B, count 322, aligned 512, imagen 143 960 B (no es nivel;
  `gex_dfx_load_level` puede fallar, lo cual es aceptable).

No se copiaron los originales al repositorio.

## 8. Limitaciones y cuestiones desconocidas

- El descriptor de `GEX.DFX` no es el de un nivel BSP; `gex_dfx_load_level`
  puede fallar para él.
- Los campos no demostrados del descriptor se conservan sin interpretar.
- No se implementa compactación ni la ruta de `0x00461ECA`.
- No se ha implementado recorrido BSP, buffers D3D11, shaders ni texturas.
- Los 4 borradores locales pasan a ser código versionable (aún no versionados
  en el commit de S-1 si se decide publicarlos).

## 9. Verificación

- Compilación limpia con clang (C11, `-Wall -Wextra`).
- `build.ps1 test` ejecutado: sin fallos en la suite DFX ni en las regresiones
  existentes.
- Diff revisado: sin archivos ajenos ni datos personales.

---

**No se marca S-1 como completado**: el parser es el primer hito; el recorrido
BSP, los buffers y el renderizado siguen pendientes.
