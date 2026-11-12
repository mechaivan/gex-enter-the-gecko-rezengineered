# Planificación Técnica: Primer Escenario Original de GEX en Direct3D 11

**Fecha:** 2026-11-07  
**Objetivo:** Determinar el camino mínimo, verificable y seguro para transformar la geometría BSP de un nivel original en datos renderizables por nuestro port D3D11.

---

## 1. Revisión de Documentación y Arquitectura Existente

### 1.1 Componentes D3D11 Implementados

- **R-1: Dispositivo D3D11** (`gex_d3d11_device`) - ✅ COMPLETADO
  - Creación/destrucción del dispositivo en modo HARDWARE/WARP
  - Feature levels 11_1→9_1 registrados
  - Contexto inmediato y manejo de COM
  - 332 checks, 0 fallos

- **R-2: Swapchain DXGI** (`gex_d3d11_swapchain`) - ✅ COMPLETADO
  - Creación sobre HWND existente de I-4
  - 2 buffers R8G8B8A_UNORM con efecto DISCARD
  - Manejo de resize y liberación ordenada de recursos
  - 572 checks, 0 fallos

- **R-3: Renderer D3D11** (`gex_d3d11_renderer`) - ✅ COMPLETADO
  - Render target view desde backbuffer
  - Clear de color configurable y Present
  - Primer salida gráfica visible
  - 145 checks, 0 fallos

- **R-4: Diagnóstico y VSync** - ✅ COMPLETADO
  - VSync configurable, Debug Layer, diagnóstico de dispositivo
  - Medición de tiempo entre frames
  - 145 checks, 0 fallos

- **I-4: Aplicación nativa** (`gex_i4_app`) - ✅ COMPLETADO
  - Integración de ventana, entrada, frame loop y renderer
  - Soporte para D3D11 y null renderer
  - 1,009 checks, 0 fallos

### 1.2 Evidencia de Geometría BSP de Nivel (U-1)

La Subfase C (U-1a…U-1h) está cerrada documentalmente con consolidación final en `U1_CONSOLIDACION.md`. Hechos clave:

- **Contenedor .DFX:** `count` + offsets relativos + imagen relocalizable
- **Descriptor de nivel:** 16 campos con 10 demostrados, 1 hipótesis, 3 sin identificar, 2 parciales
- **Nodo BSP:** 32 bytes (centro, radio, normal, distancia, hijos, tipo)
- **Polígono:** 20 bytes (índices, flags, puntero a UV)
- **Vértice:** 12 bytes (x, y, z)
- **Normal:** 6 bytes (escala 4096)
- **Auxiliar UV:** 12 bytes (U, V, índice de textura de 14 bits)
- **Recorrido de dibujo:** `0x401C00` → `0x4269C2` → emisores → `0x46558A`
- **Opcodes demostrados:** 0x20 (triángulo sólido), 0x24 (triángulo texturizado), 0x28 (cuadrilátero planar), 0x30 (Gouraud sólido), 0x34 (Gouraud texturizado), 0x40/0x48/0x4C (líneas), 0xB4 (texturizado con bbox), 0xF0 (rectángulo de fondo)

> **Auditoría 2026-11-08.** La mecánica de lectura y relocalización del
> contenedor está verificada en
> [`U1_DFX_RELOCATION_AUDIT.md`](../U1_DFX_RELOCATION_AUDIT.md): carga completa,
> `aligned = (count + 0x200) & ~0x1FF` dwords, relocalización
> `*(u32*)(B + 4*aligned + off) += B` y compactación a `B` (offset 0). La
> variante `0x00461ECA` también está analizada (audit §H): es una ruta alternativa
> **sin llamadores** en este build, así que S-1 sigue solo la ruta de
> `0x00461E0B`. Esto **documenta** la mecánica necesaria para S-1, que **sí está
> implementada** (ver §5.2 y [`S1_DFX_PARSER.md`](../S1_DFX_PARSER.md)); S-2
> (recorrido BSP) también está implementado (ver [`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md)).

---

## 2. Selección de Nivel de Prueba

**Nivel elegido: AZTEC16.DFX**

**Justificación:**
- **Complejidad:** Es el nivel más completo (544,586 B, 16,082 entradas, 3,882 vértices, 9,609 polígonos, 3,462 normales)
- **Documentación:** El más detallado en U-1h y U1_CONSOLIDACION.md
- **Representatividad:** Contiene una variedad de geometría BSP que representa mejor el juego completo
- **Evidencia:** Tiene la cadena de regiones más completa para validar el encadenado

**Alternativas consideradas:**
- **CIRCUIT0.DFX:** Más pequeño pero menos completo
- **REZOP2.DFX:** El más pequeño, con menos geometría para validar

---

## 3. Flujo Completo desde DFX hasta Renderizado D3D11

### 3.1 Pipeline de Procesamiento

```
DFX → Parser DFX → Descriptor → Recorrido BSP → Polígonos/Vértices → 
Buffers D3D11 → Shaders → Materiales → Renderizado D3D11
```

### 3.2 Etapas Detalladas

1. **Parser DFX**
   - Lectura del contenedor: `count` + offsets relativos + imagen
   - Relocalización de punteros internos (`0x004735C0`: `imageBase = B + 4*aligned`, `delta = B`)
   - Compactación de la imagen a `B` (offset 0) y descarte de la tabla
   - Extracción del descriptor (`image[0]`)
   - Ruta de referencia: la del cargador principal `0x00461E0B`; la variante
     `0x00461ECA` (audit §H) no se implementa: no tiene llamadores y su copia
     truncada produce punteros colgantes en AZTEC16
   - Referencia: [`U1_DFX_RELOCATION_AUDIT.md`](../U1_DFX_RELOCATION_AUDIT.md)

2. **Recorrido BSP**
   - Algoritmo de `0x401C00`: recorre nodos y anexa hojas visibles
   - *Culling* de esfera contra el volumen de vista: **documentado, no
     implementado** ([`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)).
     Es una prueba de esfera `(centro, radio)` contra 6 semiplanos (near, far y
     cuatro laterales) con tolerancia `-r`, usando dos matrices `int16` 3×3
     (`cam+0x88` = A, `cam+0xA8` = B) y `0x0046999C` (`(M·v) sar 12`). No es
     *clipping*: no recorta geometría.
   - Recorte por coordenadas proyectadas y emisión de la lista de polígonos
     visibles (`0x004269C2`): **implementado** en `port/src/emit/emit.c`
     (`gex_level_emit`, §11). Produce `gex_emit_output`, una lista de
     triángulos ya proyectados, clasificados, subdivididos, con niebla aplicada
     y ordenados. El recorrido y la visibilidad están validados dinámicamente
     (§ 4.3) y la emisión de nivel está validada contra el original
     (§ 18.8 y § 4.3).

3. **Generación de Buffers**
   - Conversión de `gex_emit_output` a vértices e índices: **implementada** en
     `port/src/geometry/geometry.c` (§12), portable y sin D3D11
   - Creación de vertex/index buffers en D3D11: **implementada** en
     `port/src/render/d3d11_mesh.c` (§13), Windows-only
   - El `D3D11_INPUT_ELEMENT_DESC` y el `ID3D11InputLayout` **no** se crean
     todavía: necesitan la firma de un shader (Módulo 5). El layout de 28 B está
     fijado con constantes `GEX_VERTEX_OFFSET_*` y `_Static_assert`

4. **Materiales y Texturas**
   - Carga de .VFX: tabla de texturas en `0x006488F4`
   - Conversión de formatos NCC → D3D11
   - Creación de texture views y samplers

5. **Renderizado D3D11**
   - Preparación del pipeline con shaders
   - Actualización de constant buffers
   - Dibujo mediante DrawIndexed

---

## 4. Formatos Demostrados vs. Pendientes de Validación

### 4.1 Suficientemente Demostrados

- **Contenedor .DFX:** Estructura completa validada en tres niveles
- **Descriptor de nivel:** Encadenado exacto demostrado
- **Nodo BSP:** Estructura y tipos validados
- **Polígono:** Formato completo con flags
- **Vértice/Normal/Auxiliar UV:** Tamaños y contenido confirmados
- **Opcodes:** Handlers demostrados para 0x20, 0x24, 0x28, 0x30, 0x34, 0x40/0x48/0x4C, 0xB4, 0xF0
- **Tabla de texturas:** Índice de 14 bits → .VFX

### 4.2 Necesitan Validación (estado a 2026-11-12)

- **Shaders:** **resuelto.** Los escriben los módulos 5A y 4C: vertex/pixel
  shader propios, compilados en ejecución con `d3dcompiler_47.dll`, con
  muestreo de textura y modulación por el color por vértice (§14 y §18).
- **Formato de texturas NCC:** **resuelto.** El bloque de 112 B de `+0x14` es
  una `GuNccTable` (no una paleta) y los formatos `YIQ_422`, `ARGB_1555` y
  `ARGB_4444` están decodificados en CPU con salida RGBA8 (§17 y §18).
- **Clipping:** El algoritmo BSP original (`0x401C00`) no incluye clipping
  explícito; sí hace *culling* de esfera contra 6 semiplanos, ya documentado en
  [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)
- **Alpha blending:** **resuelto** (2026-11-12). Lo **demostrado** es que el
  original activa la mezcla **por polígono** según el formato de la textura
  (`0x46558A` compara el campo `+0x0C` del registro de textura en tiempo de
  ejecución con `0x0C` = ARGB_4444) y que llama a `grAlphaCombine` con
  `(3,4,1,0,0)` para esas texturas y con `(1,0,0,2,0)` para las opacas. La
  **aritmética** queda cerrada sobre el driver real (nGlide 2.10): el programa
  que genera es `mov r5,c0 / mov r6,v0 / mov r7,r4.a / mul r8,r7,r6 /
  mov r9.a,r8`, o sea `alfa de salida = alfa del texel × alfa del vértice`, y el
  juego lo empareja con `grAlphaBlendFunction(1,5,4,0)`, que en ese driver es
  `SRC_ALPHA`/`INV_SRC_ALPHA` con alfa `ONE`/`ZERO`: exactamente lo que hace el
  port (§19.8). Quedan pendientes solo el **modo de mezcla por comando**
  (§19.10) y el significado del byte alto del color de vértice (§19.8).

---

### 4.3 Estado tras la validación dinámica de S-2 (2026-11-11)

- **Cadena de cámara: validada dinámicamente.** El port reproduce **bit a
  bit** M, A, B, `a_cam` y `b_cam` del ejecutable original para la cámara
  capturada en la ruta 3D de AZTEC16. Se corrigieron tres defectos reales (los
  ángulos Euler se niegan, `M2` se aplica por columnas y la tabla
  trigonométrica trunca). Detalle y valores en
  [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md) § 14.
- **Nivel de referencia (2026-11-11, 2.ª tanda):** el juego carga
  `C:\GEX_REZ\GEX2\level\map7.dfx`, no `AZTEC16.DFX`. Con ese nivel, el
  recorrido del port reproduce **18/18 frames** con el conteo y la lista
  completa de hojas visibles idénticos al original (§ 9 de
  [`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md)).
- **Contexto de emisión (2026-11-11, 3.ª y 4.ª tanda):** capturado el contexto
  real del original (`0x65CD60`: la `O` escalada por 1,6; traslación en
  `+0x14`; focal/cx/cy/near_z; LUT y umbrales de niebla) y reproducido **bit a
  bit** en el port, incluida la **proyección** (las profundidades capturadas en
  `[0x647430]`/`[0x64742C]`/`[0x6474A0]`). La profundidad de referencia del
  original es el **máximo** de las tres (`0x426CC1..0x426D06`); una pasada
  anterior la tomó erróneamente como mínimo y dejó una regresión en el orden
  de dibujo, ya corregida.
- **Cámara original integrada en el renderer (2026-11-11):** el escenario
  `map7.dfx` se dibuja con la cámara real del juego. El port reconstruye
  la matriz de emisión `cam+0xE0` desde el estado de cámara
  (`gex_cam_build_emit_matrix`), recorre el BSP con la prueba de esfera
  original y proyecta con los parámetros del juego (focal 320, centro
  256/192, plano cercano 160), sin matriz sintética ni encuadre automático.
  Resultado frente al original: mismas 307 hojas visibles (mismo orden),
  misma matriz de emisión bit a bit y los mismos 1123 triángulos **en el
  mismo orden de dibujo**. Verificado con GPU hardware (D3D11 11_1) y
  ventana Win32 de 960×540. El modo sintético se conserva con
  `--diagnostic-view`. Detalle en
  [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md) § 15.
- **Contrato de emisión (§ 11): validado contra el original.** La emisión de
  nivel reproduce **triángulo a triángulo** el resultado del original: mismo
  conjunto (1123 en el frame de referencia, con las profundidades idénticas
  bit a bit en los 3429 polígonos), **mismo orden de dibujo** (1123/1123) y
  mismas fases de subdivisión (27 polígonos, 25 abortos por `det2d`, 39
  subtriángulos intentados, 0 emitidos). La aparente diferencia "1252 frente a
  1123" era un artefacto de medición: `0x46558A` cuenta comandos de todo tipo
  y 129 de ellos no son triángulos de nivel. Ver
  [`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md) § 9.5.

## 5. Arquitectura Mínima por Módulos

### 5.1 Módulos Requeridos

#### Módulo 1: Parser DFX — implementado (`gex/asset/dfx.h`, `src/asset/dfx.c`)
- **Responsabilidades:**
  - Lectura y validación del contenedor .DFX
  - Relocalización de punteros internos
  - Extracción del descriptor de nivel
- **Estructuras de datos (implementadas):**
  ```c
  typedef struct gex_dfx_container {
      uint32_t count;
      uint32_t *offsets;
      uint8_t *image;
      size_t image_size;
      size_t data_size;
      uint32_t aligned;
  } gex_dfx_container;

  typedef struct gex_dfx_descriptor {
      uint32_t root_bsp;
      uint32_t field_0x04;      /* ... campos demostrados / sin identificar */
      /* vertex_count, polygon_count, normal_count, tablas, ... */
  } gex_dfx_descriptor;

  typedef struct gex_dfx_level {
      gex_dfx_descriptor descriptor;
      uint8_t *vertex_data;
      uint8_t *polygon_data;
      int16_t *normal_data;
  } gex_dfx_level;
  ```
- **Interfaz (implementada):**
  ```c
  gex_dfx_status gex_dfx_parse(const void *data, size_t size, gex_dfx_container *out);
  gex_dfx_status gex_dfx_load(const char *path, gex_dfx_container *out);
  gex_dfx_status gex_dfx_relocate(gex_dfx_container *container, uint32_t delta);
  gex_dfx_status gex_dfx_load_level(const gex_dfx_container *container,
                                    gex_dfx_level *out);
  void gex_dfx_release_container(gex_dfx_container *container);
  void gex_dfx_release_level(gex_dfx_level *level);
  ```

#### Módulo 2: Recorrido BSP — implementado (`gex/bsp.h`, `src/bsp/bsp.c`)
- **Responsabilidades:**
  - Recorrer el árbol BSP (`0x401C00`): **estructural, hecho**
  - Recorrer nodos y añadir hojas visibles: **hecho con predicado inyectado**
  - Aplicar la visibilidad original de cámara: **implementada y auditada**
    ([`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)). Incluye la cadena
    de orientación (`0x46DD40` → `0x46E643` → `0x405C41`, §6 del documento
    citado) para construir `M = *[cam+0xE8]`, las matrices A/B y la prueba de
    esfera contra seis semiplanos. Se activa pasando `cam_ctx` a `gex_bsp_walk`.
    La integración con la emisión de polígonos **está hecha**: `gex_level_emit`
    consume el resultado del recorrido (§11).
  - Generar lista de polígonos visibles: **implementada** en
    `port/src/emit/emit.c` (`gex_level_emit`, §11), con contrato documentado en
    [`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md) §5. La cadena original
    `0x43DC3A` → `0x43DED0` → `0x4269C2` → `0x426256`/`0x426588` → buckets →
    `0x469836` → `0x46558A` está reproducida en código; lo que **no** existe
    es la comparación cuantitativa de la emisión contra el ejecutable, así que S-2 sigue abierto.
  - Requisitos que el generador cubre (resumen de §5.12, ya implementados
    por `gex_level_emit` y consumidos tal cual por el Módulo 3):
    transformación con `M = *[cam+0xE0]`, *culling* 2D por determinante,
    subdivisión a cuatro triángulos si `maxz < 0x6A4`, color por vértice con
    niebla, UV de 8 bits + word de textura de 14 bits, orden por
    `(z>>2) + 4*capa` y respeto de los flags `0x8000` (oculto) y `0x0004`
    (forzar sólido). La capacidad se reserva **por triángulo emitido**, no por
    polígono: el original reserva `0x34` B por polígono y puede desbordar al
    subdividir.
- **Estructuras de datos (implementadas):**
  ```c
  typedef struct gex_bsp_walk_result {
      uint32_t *leaves;
      uint32_t count;
      uint32_t capacity;
  } gex_bsp_walk_result;

  typedef int (*gex_bsp_visible_fn)(const uint8_t *image, size_t image_size,
                                    uint32_t node_offset, void *ctx);
  ```
- **Interfaz (implementada):**
  ```c
  gex_bsp_status gex_bsp_walk(const uint8_t *image, size_t image_size,
                              uint32_t root_offset,
                              gex_bsp_visible_fn visible, void *ctx,
                              const void *cam_ctx,
                              gex_bsp_walk_result *out);
  void gex_bsp_walk_free(gex_bsp_walk_result *r);
  ```

#### Módulo 3: Generador de geometría y buffers D3D11 — IMPLEMENTADO (2026-11-11)

Se implementó en **dos módulos separados**, no en un `gex_buffer_builder.h/c`
único: la conversión es portable y la creación de recursos es Windows-only.
El diseño original de esta sección (que partía de `gex_visible_polygon_list`,
`vertices` y `normals` en crudo) quedó obsoleto cuando `gex_level_emit` pasó a
ser la entrada real: los triángulos ya llegan proyectados, con color por
vértice, UV, textura, capa, flags y niebla aplicada.

##### 3a. Conversión portable — `gex/geometry.h`, `src/geometry/geometry.c`

```c
typedef struct {
    float    x, y, z;   /* píxeles proyectados + profundidad de emisión */
    uint32_t color;     /* AARRGGBB por vértice (niebla ya aplicada)        */
    float    u, v;      /* UV normalizadas 0..1 desde bytes 0..255           */
    uint16_t texture;   /* índice de textura del registro UV (14 bits)       */
    int8_t   layer;     /* byte[uv+0x0B] con signo                           */
    uint8_t  flags;     /* GEX_EMIT_F_* del triángulo de origen              */
} gex_vertex;           /* 28 B, sin relleno                                 */

typedef struct {
    gex_vertex *vertices;
    uint32_t   *indices;
    size_t      vertex_count;
    size_t      index_count;      /* 3 * triangle_count */
    size_t      triangle_count;
    size_t      skipped_clipped;  /* GEX_EMIT_F_DEPTH_CLIPPED descartados */
} gex_geometry;

gex_geom_status gex_geometry_build(const gex_emit_output *emit,
                                   gex_geometry *out);
void gex_geometry_free(gex_geometry *geom);
const char *gex_geom_status_name(gex_geom_status status);
```

Estados: `GEX_GEOM_OK`, `GEX_GEOM_ERR_ARG`, `GEX_GEOM_ERR_RANGE`,
`GEX_GEOM_ERR_NOMEM`, `GEX_GEOM_ERR_OVERFLOW`.

##### 3b. Recursos D3D11 — `gex/render/d3d11_mesh.h`, `src/render/d3d11_mesh.c`

```c
typedef struct gex_d3d11_mesh {
    ID3D11Buffer *vertex_buffer;
    ID3D11Buffer *index_buffer;
    UINT          vertex_count, index_count;
    UINT          vertex_stride;   /* GEX_VERTEX_STRIDE = 28 */
    UINT          index_stride;    /* 2 o 4                  */
    DXGI_FORMAT   index_format;    /* R16_UINT o R32_UINT    */
    UINT          vertex_buffer_bytes, index_buffer_bytes;
    HRESULT       last_hresult;
    int           initialized;
} gex_d3d11_mesh;

gex_d3d11_mesh_status gex_d3d11_mesh_create(ID3D11Device *device,
                                            const gex_geometry *geom,
                                            gex_d3d11_mesh *mesh);
gex_d3d11_mesh_status gex_d3d11_mesh_create_from_emit(
    ID3D11Device *device, const gex_emit_output *emit, gex_d3d11_mesh *mesh);
void gex_d3d11_mesh_destroy(gex_d3d11_mesh *mesh);
const char *gex_d3d11_mesh_status_name(gex_d3d11_mesh_status status);
```

No se crea `D3D11_INPUT_ELEMENT_DESC` ni `ID3D11InputLayout`: requieren la firma
del shader, que pertenece al Módulo 5. El layout queda descrito por
`GEX_VERTEX_OFFSET_POSITION/COLOR/UV/TEXTURE/LAYER/FLAGS` y verificado con
`_Static_assert`. Detalles y decisiones: §12 y §13.

#### Módulo 4: Cargador de Texturas — PARCIAL (4A: el contenedor, 2026-11-11)

La interfaz que se había supuesto aquí (`gex_texture_loader.h/c` con
`gex_vfx_parse` y `gex_texture_table_build`) **no existe**: se ha sustituido por
la arquitectura real, la del **Módulo 4A**, que trabaja sobre el contenedor
`.VFX` ya demostrado contra el ejecutable original.

**4A. Contenedor `.VFX` — IMPLEMENTADO** (`gex/vfx.h`, `src/asset/vfx.c`, portable)

- **Responsabilidades (las que sí están demostradas):**
  - cargar el `.VFX` del nivel y construir su **tabla de texturas**, la misma
    que el original construye en `0x006488F4` desde `0x00461C32`;
  - exponer, por entrada, el formato (`+0x0C`), el bloque de datos
    (`+0x8C`, tamaño en `+0x84`) y la **tabla NCC** del formato `YIQ_422`
    (`+0x14`, 112 B; el nombre "paleta" era erróneo y se corrigió en el
    Módulo 4B, §17);
  - validar estrictamente ruta, cabecera, offsets, tamaños, conteos y
    multiplicaciones, liberando todo en error.
- **Interfaz real:**
  ```c
  gex_vfx_status gex_vfx_open(gex_vfx_file *file, const char *path);
  gex_vfx_status gex_vfx_parse(gex_vfx_file *file, const uint8_t *data,
                                size_t size);
  void          gex_vfx_close(gex_vfx_file *file);
  int           gex_vfx_find_index(const gex_vfx_file *file, uint32_t index14);
  ```
- **Correspondencia con la geometría:** el índice de textura de 14 bits del
  `.DFX` indexa **directamente** esta tabla (en AZTEC16: 9 609 polígonos usan
  los índices 0..3 sobre 260 entradas).
- **Lo que se implementó después (Módulo 4B):** el **decodificador de píxel**,
  con el formato de píxel, las dimensiones y la tabla NCC **demostrados**
  (ver §4B). Detalle, evidencia y límites:
  [`docs/VFX_TEXTURE_FORMAT.md`](../VFX_TEXTURE_FORMAT.md).

**4B. Investigación del formato de píxel (2026-11-11) — CERRADO**

- El **Módulo 4B** (2026-11-11, con auditoría correctiva) cerró el **formato
  de píxel**, las **dimensiones** y la **tabla NCC**: los identificadores 1, 11
  y 12 son `GR_TEXFMT_YIQ_422`, `GR_TEXFMT_ARGB_1555` y `GR_TEXFMT_ARGB_4444`
  de Glide 2.x; el bloque de 112 B de `+0x14` es una `GuNccTable` (no una
  paleta) y las dimensiones salen del LOD (`256 >> lod`) y del aspecto
  (`8x1`…`1x8`), con el tamaño del bloque coincidiendo en **15 251 de 15 251
  registros** de los 37 `.VFX` originales.
- Hay **decodificadores CPU portables** (`port/src/asset/vfx_decode.c`:
  `gex_vfx_decode_argb1555`, `gex_vfx_decode_argb4444`,
  `gex_vfx_decode_yiq422`, `gex_vfx_decode_level`) con salida **RGBA8** y alfa
  recto, más `gex_vfx_texture_info_get` para las dimensiones. 2 968
  comprobaciones, 0 fallos.

**4C. Integración de texturas en D3D11 — CERRADO (2026-11-11)**

- **Banco de texturas portable** (`gex/vfx_texture.h`, `src/asset/vfx_texture.c`):
  carga el `.VFX` del nivel y decodifica a RGBA8 las entradas que la geometría
  usa de verdad, indexadas por el índice de 14 bits.
- **Atlas D3D11** (`gex/render/d3d11_texture.h`, `src/render/d3d11_texture.c`):
  una textura 2D con todas las del nivel dentro (cada una con un texel de
  borde replicado), su `ID3D11ShaderResourceView`, un sampler **lineal + WRAP**
  (los valores por defecto de Glide 2.x, que el juego nunca cambia) y la tabla
  constante que traduce el índice del `.VFX` en posición y tamaño.
- **Shader**: el vertex shader pasa la UV y el índice de textura
  (`nointerpolation uint`) y el pixel shader muestrea el atlas y modula el
  color por vértice con el texel (`textura * color`, alfa incluido).
- **Aplicación**: con `--level` se deduce el `.VFX` del `.DFX`, se carga, se
  crea el atlas y se enlaza al pipeline. Sin `--level` (el cubo de prueba) no
  se toca nada.
- **Verificado en ejecución real** con GPU hardware y ventana Win32: el nivel
  de AZTEC16 sale **texturizado** (3 101 colores distintos en la captura de la
  ventana, antes era una maqueta de color plano). Detalle en §18.

- Crear `ID3D11Texture2D`/`ID3D11ShaderResourceView` a partir de las
  entradas decodificadas y atender el índice de 14 bits desde el shader.
- Ya no depende de cerrar el decodificador: el dato es real y está decodificado.
- Falta decidir el formato de recurso (`DXGI_FORMAT_R8G8B8A8_UNORM`), la
  cadena de mipmap y el modo de filtrado, y conectar el índice de 14 bits del
  `.DFX` con la textura en el shader.
#### Módulo 5: Integración con el renderer D3D11 — PARCIAL (5A implementado, 2026-11-11)

La interfaz original de esta sección (`gex_d3d11_scene.h/c` con
`gex_d3d11_scene_load` y `gex_d3d11_scene_render`) **no existe en el port** y
queda sustituida por la arquitectura real: el renderer R-3 (`gex_d3d11_renderer`)
consigue una geometría opcional y la dibuja antes de cada `Present`. No hay
un objeto "escena" porque todavía no hay materiales con iluminación (las
texturas del `.VFX` sí están, desde el Módulo 4C: §18).

**5A. Pipeline de dibujo — `gex/render/d3d11_pipeline.h`, `src/render/d3d11_pipeline.c`** (Windows-only)

```c
typedef struct gex_d3d11_pipeline {
    ID3D11Device *device;              /* prestado (R-1) */
    ID3D11DeviceContext *context;      /* prestado (R-1) */
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
    ID3D11RasterizerState *rasterizer;
    ID3D11BlendState *blend;
    ID3D11DepthStencilState *depth_stencil;
    ID3D11Buffer *constant_buffer;
    HMODULE       compiler_module;
    HRESULT       last_hresult;
    char          last_error[192];
    int           initialized;
    uint64_t      draw_calls;
    uint64_t      triangles_drawn;
} gex_d3d11_pipeline;

gex_d3d11_pipeline_status gex_d3d11_pipeline_init(gex_d3d11_pipeline *pipeline,
                                                   const gex_d3d11_device *device);
gex_d3d11_pipeline_status gex_d3d11_pipeline_set_target(
    gex_d3d11_pipeline *pipeline, ID3D11RenderTargetView *rtv,
    UINT width, UINT height, UINT ref_width, UINT ref_height);
gex_d3d11_pipeline_status gex_d3d11_pipeline_draw(gex_d3d11_pipeline *pipeline,
                                                   const gex_d3d11_mesh *mesh);
void gex_d3d11_pipeline_shutdown(gex_d3d11_pipeline *pipeline);
```

**5B. Integración en el renderer — `gex/render/d3d11_renderer.h`**

```c
gex_d3d11_renderer_status gex_d3d11_renderer_set_geometry(
    gex_d3d11_renderer *renderer,
    gex_d3d11_pipeline *pipeline,
    gex_d3d11_mesh *mesh,
    UINT ref_width, UINT ref_height);
```

El renderer guarda la geometría como **punteros prestados** y la dibuja entre
el `ClearRenderTargetView` y el `Present`. Si no hay geometría, el
comportamiento es exactamente el anterior (limpiar y presentar).

**Pendiente del Módulo 5 (estado a 2026-11-11):** materiales NCC con
iluminación, **estado de profundidad real** (*depth buffer*) y el
`D3D11InputLayout` definitivo cuando exista la firma del shader de
producción. Las **texturas del Módulo 4 ya están** (Módulo 4C, §18) y el
nivel real ya se carga (§16): lo que queda es el material y la profundidad.

### 5.2 Orden de Implementación

1. **Módulo 1: Parser DFX** - Base para todos los demás
2. **Módulo 2: Recorrido BSP** - Lógica de selección de geometría
3. **Módulo 3: Generador de Buffers** - Conexión con D3D11
4. **Módulo 4: Cargador de Texturas** - Recursos para renderizado
5. **Módulo 5: Integración** - Punto de entrada completo

**Estado real (2026-11-11):** el parser DFX (Módulo 1) está implementado en
`port/` ([`S1_DFX_PARSER.md`](../S1_DFX_PARSER.md)); el recorrido BSP
estructural, la visibilidad de cámara y la emisión de polígonos (Módulo 2)
están implementados y auditados ([`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md),
[`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)): 889 comprobaciones
de cámara + 37 de BSP + 38 855 de emisión, 0 fallos. La **emisión de polígonos**
también está implementada (`gex_level_emit`, §11) y auditada con 38 855
comprobaciones; la validación dinámica del recorrido y la visibilidad está hecha (§ 4.3), pero falta la comparación cuantitativa de la emisión contra el ejecutable
original, así que no se declara cerrada.

El **Módulo 3 está implementado en sus dos mitades**: la conversión portable
`gex_emit_output → gex_geometry` (§12) y la creación de vertex/index buffers
D3D11 (§13), con 6 963 y 108 comprobaciones respectivamente. El **Módulo 5A**
también: shaders, `ID3D11InputLayout`, estados y `DrawIndexed` (§14), con 137
comprobaciones offscreen con WARP. Lo que **no** existe todavía es el Módulo 4
(cargador de texturas) y el resto del Módulo 5 (materiales, profundidad real,
texturas). La investigación de la carga `.DFX` está cerrada
([`U1_DFX_RELOCATION_AUDIT.md`](../U1_DFX_RELOCATION_AUDIT.md), incluida la
variante `0x00461ECA` en §H).

---

## 6. Pruebas Unitarias y de Integración

### 6.1 Pruebas Unitarias

- **Parser DFX:**
  - Validación de estructura .DFX (AZTEC16.DFX)
  - Corrección de relocalización de punteros
  - Extracción exacta del descriptor

- **Recorrido BSP:**
  - Recorrido completo del árbol BSP
  - Selección correcta de polígonos visibles
  - Validación con datos conocidos de AZTEC16

- **Generador de Buffers (implementado, §12 y §13):**
  - Conversión exacta de cada triángulo emitido a tres vértices de 28 B con
    posición, profundidad, color, UV, textura, capa y flags
  - Índices correctos para todos los triángulos, incluidas subdivisiones y
    pasadas de niebla
  - Deduplicación exacta y determinista por igualdad de los 28 B
  - Triángulos clipeados por profundidad descartados y contados
  - Creación real de `ID3D11Buffer` de vértices e índices con WARP y hardware
  - Errores: punteros nulos, conteos incoherentes, overflow, fallo de reserva,
    índice fuera de rango, doble creación y liberación repetida

- **Cargador de Texturas:**
  - Parseo correcto de .VFX
  - Creación de texturas D3D11
  - Resolución de índices de 14 bits

### 6.2 Pruebas de Integración

- **Carga completa de AZTEC16:**
  - Desde .DFX hasta buffers D3D11
  - Validación de conteos (vértices, polígonos, normales)
  - Verificación de integridad de datos

- **Renderizado básico (implementado el 2026-11-11, §14):**
  - Dibujo de geometría sin texturas: **hecho** (color por vértice); desde el
    Módulo 4C el pipeline además **muestrea la textura** del `.VFX` (§18)
  - Validación visual simple (siluetas): pendiente de una escena real; hoy se
    comprueba **pixel a pixel** en render offscreen con WARP (posición,
    colores e interpolación)
  - Sin ejecución del juego, solo renderizado: las pruebas son offscreen, sin
    ventanas ni demos interactivas

### 6.3 Herramientas de Prueba

- **Fixtures:** Datos de AZTEC16.DFX preprocesados para validación
- **Mock de D3D11:** Para pruebas sin dispositivo real
- **Comparación binaria:** Verificación exacta de estructuras de datos

---

## 7. Hitos Pequeños y Verificables

### Hito 1: Parser DFX Operativo
- **Estado:** ✅ IMPLEMENTADO (2026-11-08). El parser del contenedor (Módulo 1)
  está implementado en `port/` ([`S1_DFX_PARSER.md`](../S1_DFX_PARSER.md)) y
  valida AZTEC16.DFX (16 082 entradas, 3 882 vértices, 9 609 polígonos). Esto no
  cierra el escenario completo: la integración sigue pendiente.
- **Objetivo:** Leer AZTEC16.DFX y extraer descriptor
- **Criterio de éxito:**
  - `gex_dfx_load` carga AZTEC16.DFX sin errores (hecho)
  - Descriptor extraído con conteos correctos (16 082 entradas, 3 882 vértices,
    9 609 polígonos)
  - Tests unitarios: cobertura de los casos documentados

### Hito 2: Recorrido BSP Funcional
- **Estado:** 🔄 PARCIAL (2026-11-11). Recorrido estructural, visibilidad de
  cámara **y emisión de polígonos** implementados y auditados en `port/`
  ([`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md),
  [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)); falta la
  **comparación cuantitativa de la emisión contra el ejecutable original**, por lo que el hito no
  se declara completo.
- **Objetivo:** Implementar algoritmo de `0x401C00` para recorrer nodos y generar
  lista de polígonos visibles.
- **Criterio de éxito:**
  - Recorrido completo del árbol BSP (estructural: hecho)
  - Lista de polígonos visibles generada (`gex_level_emit`, §11: hecho a nivel
    estático y estructural)
  - Validación con nodos conocidos de AZTEC (estructural: hecho; dinámica contra
    el ejecutable: pendiente)

**Estado 2026-11-11:** recorrido estructural implementado en `port/`
(`gex_bsp_walk`, `S2_BSP_TRAVERSAL.md`): AZTEC16 produce 815 hojas con predicado
NULL. La visibilidad original (cámara) está implementada en
[`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md) — prueba de esfera
contra 6 semiplanos con matrices A/B y orden de apilado — y auditada contra el
desensamblado: 889 comprobaciones de cámara + 37 de BSP, 0 fallos, integradas
en `build.ps1 test` y `make test`. `port/` sigue usando el predicado inyectado
cuando no se pasa `cam_ctx`.

**Emisión (implementada el 2026-11-11):** la ruta está descrita de extremo a
extremo en [`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md) §5 (llamada,
descriptores, proyección a píxeles de 10 bits, subdivisión, niebla, emisores,
buckets, diferencias con modelos y con la primitiva 2D `0x4FF06F`) y se
implementa en `gex_level_emit` (§11), con 38 855 comprobaciones. Sigue
pendiente la comparación cuantitativa de la emisión contra el ejecutable: sin ella no se puede
afirmar que el port reproduce el orden y los colores reales de una escena.


### Hito 3: Buffers D3D11 Generados
- **Estado:** ✅ IMPLEMENTADO (2026-11-11) en lo que el hito pedía —conversión y
  creación de buffers—, con una corrección de criterio: el objetivo «3 882
  vértices» describía el **vertex buffer del nivel completo**, no lo que produce
  la emisión. `gex_level_emit` emite triángulos ya transformados, y la
  deduplicación exacta reduce 6 726 esquinas a 3 918 vértices en AZTEC16 con la
  cámara sintética de prueba. Las 3 882 entradas de la tabla de vértices DFX son
  otra cosa (la fuente sin proyectar) y no se usan como vértices de dibujo.
- **Objetivo:** Convertir geometría a buffers D3D11
- **Criterio de éxito:**
  - Vertex buffer creado con los vértices de la geometría emitida (hecho:
    3 918 en AZTEC16 con la cámara de prueba)
  - Index buffer creado con índices correctos (hecho: 6 726 índices, todos en
    rango, verificados también por lectura de vuelta desde la GPU)
  - Tests de conversión: exactitud bit a bit verificada (6 963 comprobaciones
    portables + 108 de D3D11, 0 fallos)
- **Lo que este hito no incluye:** shaders, `ID3D11InputLayout`, texturas y
  `DrawIndexed`. Los buffers existen y son verificables; **no hay renderizado
  visible**.

### Hito 4: Texturas Cargadas
- **Objetivo:** Cargar .VFX y crear texturas D3D11
- **Criterio de éxito:**
  - Tabla de texturas construida desde GEX.VFX
  - Texturas D3D11 creadas exitosamente
  - Resolución correcta de índices de 14 bits

### Hito 5: Primer Renderizado Completo
- **Estado:** 🔄 PARCIAL (2026-11-11). El **Módulo 5A** ya tiene el pipeline de
  dibujo completo (§14): shaders, `ID3D11InputLayout`, estados, `DrawIndexed` y
  una geometría 3D de prueba (el cubo) visible en la ventana de la aplicación.
  Lo que falta para cerrar el hito: los **materiales NCC con iluminación** y el
*depth buffer* (las **texturas** del Hito 4 están hechas en el Módulo 4C,
§18; el *depth buffer* y la cámara original siguen pendientes),
  blending del original y la carga del nivel real (`.DFX`) en el bucle de
  frames. **El primer escenario original no está terminado.**
- **Objetivo:** Renderizar AZTEC16 con D3D11
- **Criterio de éxito:**
  - Geometría renderizada (siluetas reconocibles)
  - Sin crashes o errores de D3D11
  - Presentación estable a 60 FPS

---

## 8. Bloqueos Reales y Riesgos

### 8.1 Bloqueos Técnicos

1. **Shaders:** 
   - No están documentados en U-1
   - Solución: Implementar shaders básicos (posición, color simple)
   - Riesgo: Posible necesidad de shaders más complejos

2. **Formato de Texturas NCC:**
   - Conversión a D3D11 no documentada
   - Solución: Investigar formato NCC y probar conversiones
   - Riesgo: Posible necesidad de herramientas de conversión

3. **Sistema de Coordenadas:**
   - Transformaciones entre espacios de coordenadas no documentadas
   - Solución: Inferir desde datos de vértices
   - Riesgo: Posible necesidad de ajustes manuales

### 8.2 Riesgos de Proyecto

1. **Complejidad de AZTEC16:**
   - Nivel grande puede exceder memoria o rendimiento
   - Mitigación: ImplementarLOD o usar nivel más pequeño si necesario

2. **Dependencias de D3D11:**
   - Dispositivos con diferentes feature levels
   - Mitigación: Validar en WARP y hardware mínimo

3. **Validación Visual:**
   - Sin ejecución del juego, difícil validar renderizado correcto
   - Mitigación: Comparación con capturas de pantalla originales

---

## 9. Evidencias, Decisiones y Pendientes

### 9.1 Decisiones Técnicas

1. **Enfoque incremental:** Implementar hitos pequeños y verificables
2. **Uso exclusivo de datos demostrados:** No asumir funcionalidades no documentadas
3. **AZTEC16 como nivel de prueba:** Completo y bien documentado
4. **Integración con renderer existente:** Reutilizar componentes R-1 a R-4

### 9.2 Pendientes Expresos (estado a 2026-11-12)

1. ~~**Shaders:** Necesarios pero fuera del alcance de U-1~~ → **hecho**
   (Módulo 5A: vertex/pixel shader con `d3dcompiler_47.dll`; Módulo 4C: además
   muestreo de textura y modulación por el color por vértice).
2. ~~**Formato NCC:** Requiere investigación adicional~~ → **hecho** (Módulo 4B
   con la auditoría correctiva: `GuNccTable`, `GR_TEXFMT_YIQ_422` /
   `_ARGB_1555` / `_ARGB_4444` y decodificadores CPU verificados; Módulo 4C:
   las texturas se suben a D3D11).
3. **Clipping:** Implementación futura con viewport (el *culling* de esfera de
   `0x401C00` ya está implementado en
   [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md) y `port/src/camera/camera.c`)
4. **Alpha blending:** **resuelto** (2026-11-12). La aritmética exacta está
   **demostrada** sobre el driver real (§19.8): `grAlphaCombine(3,4,1,0,0)`
   genera `alfa de salida = alfa del texel × alfa del vértice`, y el juego lo
   empareja con `grAlphaBlendFunction(1,5,4,0)` = `SRC_ALPHA`/`INV_SRC_ALPHA`
   con alfa `ONE`/`ZERO`: lo que el port ya hace. Quedan dos puntos abiertos,
   ambos con su evidencia y su método: el **modo de mezcla por comando**
   (§19.10) y el **byte alto del color de vértice** (§19.11), que el port usa
   como alfa y que el original **no** entrega a Glide.
5. **Materiales de producción, *depth buffer* y niebla después de la
   textura:** el recorrido, la visibilidad y la emisión de nivel ya están
   validados contra el ejecutable original (§ 4.3 y § 18.8) y la **cámara real
   del juego está integrada en el renderer** (matriz `cam+0xE0` reconstruida
   bit a bit, misma visibilidad y mismo orden de dibujo). Quedan pendientes
   los materiales, el *depth buffer* y la niebla por polígono.

### 9.3 Evidencias Clave

- `U1_CONSOLIDACION.md`: Estado final de geometría BSP
- `U1H_RESULTADOS.md`: Estructuras de datos demostradas
- Código existente R-1 a R-4: Base para renderizado
- Datos de AZTEC16.DFX: Caso de prueba completo

---

## 10. Siguiente Hito Concreto

**Siguiente paso: el contexto de emisión real del juego (S-2) y el *depth buffer*.** El
Módulo 4C ya sube las texturas del `.VFX` a D3D11 (§18): el nivel de AZTEC16
sale texturizado en ejecución real. Lo que falta para acercarse al original es
validar la cámara del juego contra el ejecutable (S-2, sigue abierto) y
añadir el *depth buffer* y el blending por polígono del original.

- **Ya hecho:** parser DFX (Módulo 1), recorrido BSP + visibilidad + emisión
  de polígonos (Módulo 2, §11), el Módulo 3 completo (conversión portable §12 y
  vertex/index buffers D3D11 §13) y el **Módulo 5A**: shaders, input layout,
  estados y `DrawIndexed` (§14), con geometría de prueba visible en la
  ventana de la aplicación.
- **Lo que falta antes de ver una escena real:** el blending del original por
  polígono, el *depth buffer*, y la carga del nivel con el `.DFX` y
  la emisión real dentro del bucle de frames (hoy la geometría de la
  aplicación es el cubo de prueba, no un nivel).
  ~~Estado 2026-11-11~~ → **actualizado 2026-11-12:** el blending por polígono
  está **resuelto** (§19.8: la aritmética de `grAlphaCombine` está
  demostrada sobre el driver real y el port la reproduce) y el nivel **ya se
  carga y se dibuja** en el bucle de frames (§ 4.3). Quedan por cerrar dos
  puntos de ese bloque: el **modo de mezcla por comando** (§19.10) y el
  **byte alto del color de vértice** (§19.11).
- **Criterio de éxito del siguiente hito:** `gex_texture_load` construye la
  tabla de texturas desde `GEX.VFX`, resuelve índices de 14 bits y crea
  `ID3D11ShaderResourceView`; las pruebas se ejecutan con WARP sin abrir
  ventanas.
- **Ya no sigue pendiente:** la emisión de S-2 está validada contra el
  ejecutable original para la ruta de MAP7 (mismas hojas visibles, misma
  matriz de emisión bit a bit y los mismos 1123 triángulos en el mismo orden
  de dibujo; § 18.8). Quedan abiertos el emisor lejano, la LUT de niebla por
  vértice y los materiales.

El Módulo 5A deja la geometría real dibujándose en pantalla: cada vértice
lleva posición, profundidad, color, UV, textura, capa y flags, y los índices
cubren todos los triángulos emitidos, incluidas subdivisiones y pasadas de
niebla.

---

## 11. Contrato de `gex_level_emit` (2026-11-10, revisado 2026-11-11)

Implementado en `port/src/emit/emit.c` (cabecera `port/include/gex/emit.h`).
Deriva de la reconstrucción estática de
[`S2_BSP_TRAVERSAL.md` §5](../S2_BSP_TRAVERSAL.md) y de
[`S2_CAMERA_VISIBILITY.md` §13](../S2_CAMERA_VISIBILITY.md). La revisión del
2026-11-11 (18.ª pasada) corrige el contrato de `gex_emit_classify`,
`gex_emit_subdivide` y `gex_level_emit` (`state_flags`), el tipo con signo de
`layer`, el nombre de `GEX_EMIT_F_DEPTH_CLIPPED` y la clave de orden (`maxz`,
no `promedio(z)`).

### 11.1 Principios

1. **Matriz de emisión propia.** `gex_level_emit` **no** acepta `gex_camera` ni
   usa `gex_camera.m` (`+0xE8`, visibilidad). Recibe un contexto de emisión
   independiente con la matriz de `cam+0xE0` (`gex_emit_matrix`).
2. **Separación de fases.** Extracción de polígonos, transformación,
   clasificación, subdivisión y ordenación son funciones distintas con
   estructuras propias; ninguna fase llama a otra implícitamente.
3. **Capacidad segura.** El original reserva 0x34 B por polígono y puede
   desbordar al subdividir (hasta cuatro principales y tres *slivers*). El
   módulo reserva por triángulo emitido y devuelve `GEX_EMIT_ERR_OVERFLOW` si no
   puede crecer.
4. **Sin dependencias de plataforma.** Nada de D3D11, Win32 ni Glide; solo
   `stdint.h`/`stddef.h`.
5. **Incógnitas aisladas.** Lo no demostrado (override `+0x50`, capa
   `byte[uv+0x0B]`, segundo color de niebla) se expone como parámetro explícito
   con valor documentado, nunca se inventa una fórmula.

### 11.2 Contexto de emisión

```c
/* Matriz de emisión: 3x3 Q12 row-major + traslación int32.
 * Equivale al buffer de 0x20 B apuntado por cam+0xE0.
 * NO es gex_cam_mat3 y NO es la matriz de visibilidad (gex_cam.m, +0xE8). */
typedef struct {
    int16_t m[9];      /* fila 0 = x pantalla, 1 = y pantalla, 2 = profundidad */
    int32_t t[3];      /* traslación, ya en unidades de profundidad */
    int32_t focal;     /* [0x646850] = 0x140 */
    int32_t cx, cy;    /* [0x646814] = 0x100, [0x646818] = 0xC0 */
    int32_t near_z;    /* [0x6474E8] = 0xA0 */
} gex_emit_matrix;
```

### 11.3 Niebla

```c
typedef struct {
    int16_t lut[3464];   /* [0x64FEA8]; índice z>>2; relleno por 0x461895 */
    uint8_t fog_rgb[3];  /* +0x40/+0x41/+0x42 */
    int32_t fog_total_z; /* +0x4C */
    int32_t fog_none_z;  /* +0x4E */
    int32_t threshold;   /* [0x65BF80] */
    int32_t split_z;     /* [0x65BF04] */
    int have_lut;        /* 0 => usar la fórmula analítica A/z + B */
    int32_t a_q12, b_q12;/* [0x647564], [0x647570] */
} gex_emit_fog;

int gex_emit_fog_factor(const gex_emit_fog *fog, int32_t z);   /* 0..0x1000 */
```

`gex_emit_fog_factor` acota el índice a `0..3463` (el original no lo hace) y
devuelve el factor sin mezclar colores.

### 11.4 Triángulo normalizado de salida

```c
typedef struct {
    uint32_t color[3];     /* AARRGGBB por vértice, ya con niebla si procede */
    int16_t  x[3], y[3];   /* píxeles, recortados a [-0x400,0x3FF] como 0x46A1D0 */
    int32_t  z[3];         /* profundidad de emisión (sin recorte) */
    uint8_t  u[3], v[3];   /* UV 8 bits */
    uint16_t texture;      /* índice de textura, 14 bits */
    int8_t   layer;        /* byte[uv+0x0B] con signo (movsx) [A] */
    uint8_t  flags;        /* GEX_EMIT_F_* */
    uint32_t source_poly;  /* índice de polígono de origen (trazabilidad) */
    uint32_t seq;          /* secuencia de inserción (desempate LIFO en sort) */
} gex_emit_triangle;
```

Banderas: `GEX_EMIT_F_DEPTH_CLIPPED` (0x01, algún vértice con
`z < near_z` o `z > 0xFFFF`: `[0x647578] |= 0x40000`),
`GEX_EMIT_F_SUBDIVIDED` (0x02), `GEX_EMIT_F_FAR_EMITTER` (0x04),
`GEX_EMIT_F_NEAR_EMITTER` (0x08) y `GEX_EMIT_F_FOG_PASS` (0x10).

- `z` se conserva como `int32` sin recortar; `x`/`y` sí reproducen el clamp
  `[-0x400,0x3FF]` de `0x46A1D0` (el empaquetado de 10 bits es del consumidor).
- Si algún vértice se clipea por profundidad, el original abandona el polígono
  entero; el port publica `x/y = 0` para los tres y marca
  `GEX_EMIT_F_DEPTH_CLIPPED` para que el llamador lo descarte.
- `color` ya incluye la niebla cuando corresponde; la pasada de niebla de
  `0x426588` se emite como triángulo separado con `GEX_EMIT_F_FOG_PASS` y
  colores de niebla puros (base `0x00FFFFFF`), igual que el original.
- `source_poly` permite contrastar contra el ejecutable en la futura validación
  dinámica.

### 11.5 API

```c
typedef enum {
    GEX_EMIT_OK = 0,
    GEX_EMIT_ERR_ARG,        /* puntero nulo o descriptor incoherente */
    GEX_EMIT_ERR_RANGE,      /* offset/índice fuera de la imagen DFX */
    GEX_EMIT_ERR_INCONSIST,  /* conteos o tablas que no cuadran */
    GEX_EMIT_ERR_NOMEM,      /* fallo de reserva */
    GEX_EMIT_ERR_OVERFLOW    /* capacidad máxima de seguridad alcanzada */
} gex_emit_status;

typedef struct {
    gex_emit_triangle *tris;
    size_t             count;
    size_t             capacity;
    size_t             fog_pass_count;  /* subconjunto con GEX_EMIT_F_FOG_PASS */
    int                overflow;        /* !=0 si se forzó parada por capacidad */
} gex_emit_output;

typedef enum {
    GEX_EMIT_NEAR = 0, /* 0x426256: cercano o forzado */
    GEX_EMIT_FAR   = 1 /* 0x426588: lejano, con pasada de niebla */
} gex_emit_emitter;

typedef struct {
    int32_t          maxz;        /* max(z0,z1,z2); -1 = descartado por el gate */
    int32_t          bucket;      /* (maxz sar 2) + 4*layer; -1 si no se calculó */
    int              subdivide;   /* maxz < 0x6A4 (prioridad sobre el gate) */
    int              fog_applies; /* maxz >= threshold (solo si no subdivide) */
    gex_emit_emitter emitter;
    uint32_t         fog_color[3];/* color con niebla aplicada por vértice */
} gex_emit_class;

/* Fase 1: hojas visibles -> polígonos (sin transformar).
 * `image`/`image_size` son la imagen DFX cargada; los offsets de hoja y de
 * polígono son offsets dentro de ella, nunca punteros absolutos. Toda suma se
 * comprueba en size_t antes de leer. */
gex_emit_status gex_emit_collect(const uint8_t *image, size_t image_size,
                                 const gex_dfx_descriptor *desc,
                                 const gex_bsp_walk_result *leaves,
                                 gex_emit_poly_list *out);

/* Fase 2: transformar y proyectar un polígono (0x46A1D0).
 * `color`, `u` y `v` admiten NULL. */
gex_emit_status gex_emit_project(const gex_emit_matrix *mat,
                                 const int16_t vtx[9],   /* x0,y0,z0,x1,y1,z1,x2,y2,z2 */
                                 const uint32_t color[3],
                                 const uint8_t u[3], const uint8_t v[3],
                                 uint16_t texture, int8_t layer,
                                 uint32_t source_poly,
                                 gex_emit_triangle *tri);

/* Fase 3: clasificar y decidir emisor. La subdivisión (maxz < 0x6A4) tiene
 * prioridad sobre el determinante, el bucket y el gate de pantalla.
 * `state_flags` = [0x647578]; si el gate descarta, cls->maxz = -1. */
gex_emit_status gex_emit_classify(const gex_emit_fog *fog,
                                  const gex_emit_triangle *p,
                                  uint16_t poly_flags,
                                  uint32_t state_flags,
                                  gex_emit_class *cls);

/* Fase 4: cuatro principales + hasta tres slivers; solo emisor cercano y sin
 * niebla. Un principal rechazado aborta la subdivisión. */
gex_emit_status gex_emit_subdivide(const gex_emit_matrix *mat,
                                   const int16_t vtx[9],
                                   const uint32_t color[3],
                                   const uint8_t u[3], const uint8_t v[3],
                                   uint16_t texture, int8_t layer,
                                   uint16_t poly_flags,
                                   uint32_t state_flags,
                                   uint32_t source_poly,
                                   gex_emit_output *out);

/* Fase 5: ordenar por (maxz sar 2) + 4 * layer, descendente; desempate LIFO
 * por secuencia de inserción. */
gex_emit_status gex_emit_sort(gex_emit_triangle *tris, size_t count);

/* Orquestador: collect -> project -> classify -> (subdivide) -> sort. */
gex_emit_status gex_level_emit(const uint8_t *image, size_t image_size,
                               const gex_dfx_descriptor *desc,
                               const gex_bsp_walk_result *leaves,
                               const gex_emit_matrix *mat,
                               const gex_emit_fog *fog,
                               uint32_t state_flags,
                               gex_emit_output *out);

void gex_emit_output_free(gex_emit_output *out);
void gex_emit_poly_list_free(gex_emit_poly_list *list);
const char *gex_emit_status_name(gex_emit_status status);
```

### 11.6 Errores y garantías

- `GEX_EMIT_ERR_RANGE` cubre los offsets de hoja tratados como **offsets dentro
  de la imagen**, nunca como punteros absolutos; todas las sumas se hacen en
  `size_t` y se comprueban antes de leer (un offset `0xFFFFFFFF` no envuelve).
- `GEX_EMIT_ERR_INCONSIST` cubre el descriptor DFX frente al descriptor del
  buffer de comandos: son estructuras distintas y el módulo solo acepta el
  primero.
- El *bucket array* no se modela: `gex_level_emit` produce una lista de
  triángulos ordenada por la misma clave que los buckets
  (`(maxz sar 2) + 4*layer`, descendente, LIFO dentro de la clave); el
  equivalente al buffer secuencial de comandos es responsabilidad del
  renderizador.
- El **gate** reproduce `0x426256`/`0x426588`: `[0x647578] & 0x60000`, vértice
  clipeado por profundidad, `det2d >= 0`, bucket fuera de `[0, 0xC00)`,
  `poly+0x06 & 0x8000` y descarte cuando todos los x/y caen al mismo lado de
  `0`, `0x200` o `0x180`. La subdivisión se decide **antes** que el gate.
- Toda reserva se comprueba; `GEX_EMIT_ERR_OVERFLOW` sustituye al desbordamiento
  silencioso del original.
- La aritmética de 32 bits reproduce el wrap de x86 con helpers explícitos
  (`add_wrap`, `sub_wrap`, `mul_wrap`, `neg_wrap`, `sar32`, `idiv32`): sin
  desbordamiento con signo ni desplazamientos de implementación definida.
- La geometría de modelo (`0x4013A0`, traslación `sar 4`) **no** entra en este
  módulo: tiene su propia ruta y su propio contrato.

Validación actual (18.ª pasada): `port/tests/test_level_emit.c` con **38 855
comprobaciones**, 0 fallos, limpio en Clang `-O0`/`-O2` y ASan/UBSan. AZTEC16
se comprueba de forma estructural y de determinismo (815 hojas, 9609 polígonos,
rangos, gate, ambas rutas y repetición bit a bit), **no** visual.

### 11.7 Lo que este contrato no decide

- El valor real de `+0x50` (override) y de `+0x44..+0x46`.
- El generador de `byte[uv+0x0B]`; se transporta tal cual desde la imagen.
- La equivalencia bit a bit de las tablas trigonométricas.
- El modo de mezcla Glide del prefijo `0xE1`.

---

## 12. Módulo 3a: conversión portable `gex_emit_output` → geometría (2026-11-11)

Implementado en `port/include/gex/geometry.h` y `port/src/geometry/geometry.c`.
Pruebas en `port/tests/test_geometry.c`. Sin Windows, sin D3D11, sin Win32 y sin
Glide: solo C11, `stdint.h`, `stddef.h`, `stdlib.h` y `string.h`.

### 12.1 Entrada real

La entrada **no** es la tabla de vértices DFX (12 B) ni `gex_visible_polygon_list`
(el diseño original de §5.1, ya obsoleto): es el `gex_emit_output` que produce
`gex_level_emit` (§11). Cada `gex_emit_triangle` llega con:

- `x[3]`, `y[3]`: píxeles proyectados de 10 bits ya recortados/gateados
- `z[3]`: profundidad de emisión (`int32`, en `[near_z, 0xFFFF]` si el triángulo
  fue aceptado)
- `color[3]`: AARRGGBB por vértice, con la niebla ya mezclada cuando corresponde
- `u[3]`, `v[3]`: bytes 0..255 del registro UV
- `texture`: índice de 14 bits, `layer`: `int8_t`, `flags`: `GEX_EMIT_F_*`

No se recalcula nada: la conversión es de representación, no de geometría. El
resultado de `gex_level_emit` no se altera.

### 12.2 Vértice

`gex_vertex` mide **28 B y no tiene relleno**. El layout es el contrato:

| Offset | Campo | Tipo | Contenido |
|-------:|-------|------|-----------|
| 0  | `x, y, z` | `float` | píxeles proyectados + profundidad |
| 12 | `color` | `uint32_t` | AARRGGBB por vértice |
| 16 | `u, v` | `float` | UV normalizadas 0..1 |
| 24 | `texture` | `uint16_t` | índice de textura |
| 26 | `layer` | `int8_t` | capa con signo |
| 27 | `flags` | `uint8_t` | `GEX_EMIT_F_*` del triángulo de origen |

Constantes públicas: `GEX_VERTEX_STRIDE` (28) y `GEX_VERTEX_OFFSET_POSITION`,
`_COLOR`, `_UV`, `_TEXTURE`, `_LAYER`, `_FLAGS`. `geometry.c` las verifica con
`_Static_assert` sobre `sizeof` y `offsetof`, así que un cambio de ABI rompe la
compilación en vez de corromper la GPU.

**Por qué `z` es `float`:** los triángulos aceptados tienen profundidad en
`[near_z, 0xFFFF]`, muy por debajo de 2^24, así que la conversión `int32 →
float` es exacta y reversible. La profundidad no se usa para el *depth buffer*
todavía (eso necesita la proyección del Módulo 5), pero se conserva para no
perder información en fases posteriores.

**Por qué `u`/`v` se normalizan:** el registro UV original guarda bytes 0..255.
`byte / 255.0f` es exacto en binario para esos valores y `round(u * 255)`
recupera el byte original; la prueba lo comprueba esquina por esquina.

### 12.3 Índices y deduplicación

`indices` es siempre `uint32_t` y `index_count == 3 * triangle_count`. El módulo
D3D11 decide después el formato real (`R16_UINT` o `R32_UINT`).

La deduplicación es **exacta**: dos esquinas comparten vértice solo si sus 28 B
son idénticos (`memcmp`), con hash FNV-1a y tabla abierta de potencia de dos y
carga ≤ 0.5. Es determinista por construcción: el primer vértice visto fija su
índice y el orden del vertex buffer, y el index buffer conserva el orden de
emisión. Una segunda conversión de la misma entrada produce índices idénticos
byte a byte (comprobado con AZTEC16).

Consecuencia práctica en AZTEC16 (cámara sintética de prueba): 2 242 triángulos
emitidos → 6 726 índices y **3 918 vértices** en vez de 6 726 esquinas. No se
duplica geometría y no se fusiona nada que difiera en color, UV, textura, capa
o flags.

### 12.4 Triángulos clipeados

Los triángulos con `GEX_EMIT_F_DEPTH_CLIPPED` se **descartan** y se cuentan en
`skipped_clipped`. El contrato de `gex_emit_project` (§11.4) obliga al llamador
a saltarlos, y sus `x`/`y` valen 0: dibujarlos sería geometría falsa en el
origen de la pantalla.

### 12.5 Validación y errores

Antes de reservar se comprueba, en este orden:

| Condición | Resultado |
|-----------|-----------|
| `out == NULL` | `GEX_GEOM_ERR_ARG` |
| `emit == NULL` | `GEX_GEOM_ERR_ARG` |
| `count > 0 && tris == NULL` | `GEX_GEOM_ERR_ARG` |
| `count > capacity` | `GEX_GEOM_ERR_RANGE` |
| `count == 0` | `GEX_GEOM_OK` vacío, punteros `NULL` |
| `count > SIZE_MAX/3` | `GEX_GEOM_ERR_OVERFLOW` |
| `count*3 > UINT32_MAX` | `GEX_GEOM_ERR_OVERFLOW` |
| `count*3` no cabe en bytes | `GEX_GEOM_ERR_OVERFLOW` |
| tabla hash no representable | `GEX_GEOM_ERR_OVERFLOW` |
| `malloc` falla | `GEX_GEOM_ERR_NOMEM` |

Ninguna multiplicación se calcula antes de comprobar que no desborda. En
cualquier error `out` queda a cero: nunca hay memoria a medias ni punteros
colgantes. `gex_geometry_free` es idempotente y tolera `NULL`.

`gex_geometry_build` **no** consulta `emit->overflow`: una lista truncada sigue
siendo coherente, y quien la construyó ya recibió `GEX_EMIT_ERR_OVERFLOW`.

### 12.6 Pruebas

`port/tests/test_geometry.c` — **6 963 comprobaciones, 0 fallos** con
`-DGEX_GEOM_TEST_FAULT_INJECT`; 6 952 sin él (se omiten las de inyección de
fallo). Ejecutado en Clang `-O0`, `-O2` y `-O1 -fsanitize=address,undefined`
con `-fno-sanitize-recover=all`. Grupos:

1. **Un triángulo:** los 9 atributos de las 3 esquinas, índices `0,1,2`,
   reversibilidad de UV.
2. **Varios triángulos:** orden de índices, reutilización de vértices, conteos.
3. **Clipeados:** se saltan, se cuentan, no entran vértices con `x=y=0`; todo
   clipeado → geometría vacía válida.
4. **Pasada de niebla:** el registro principal y la pasada de niebla del mismo
   triángulo **no** se fusionan (colores y `GEX_EMIT_F_FOG_PASS` distintos).
5. **Subdivisión real** (`gex_emit_subdivide`): 4 principales + 1 *sliver*, todos
   con `GEX_EMIT_F_SUBDIVIDED`.
6. **Extremo a extremo** con una imagen DFX sintética: emisor lejano (principal
   + niebla, 2 triángulos, 6 vértices) y subdivisión (4 triángulos, sin niebla).
7. **Entrada vacía y argumentos nulos**, incluidos nombres de estado y valores
   desconocidos.
8. **Límites y overflow:** `count > capacity`, `count*3` no representable,
   `count*3 > UINT32_MAX`, tabla hash no representable.
9. **Fallo de reserva inyectado:** las tres reservas fallan por turno; `out`
   queda a cero y liberar es seguro.
10. **Liberación repetida** y estructuras ya a cero.
11. **AZTEC16 real** (solo lectura): 2 242 triángulos, 1 121 de niebla, 3 918
    vértices, 6 726 índices, todos en rango, y determinismo bit a bit del index
    buffer entre dos conversiones.

---

## 13. Módulo 3b: vertex/index buffers D3D11 (2026-11-11)

Implementado en `port/include/gex/render/d3d11_mesh.h` y
`port/src/render/d3d11_mesh.c`. Pruebas en `port/tests/test_d3d11_mesh.c`.
Windows-only, igual que `d3d11_device.h`: el código portable no incluye la
cabecera y el `Makefile` POSIX no compila la fuente.

### 13.1 Alcance exacto

**Hace:** crea un `ID3D11Buffer` de vértices y otro de índices a partir de una
`gex_geometry` ya convertida, valida la geometría, elige el formato de índice y
libera todo en `destroy`.

**No hace:** no crea shaders, ni `D3D11_INPUT_ELEMENT_DESC`, ni
`ID3D11InputLayout`, ni texturas, ni samplers, ni constant buffers, ni llama a
`IASetVertexBuffers`/`DrawIndexed`. Los buffers existen y son verificables;
**no hay renderizado visible**.

### 13.2 Estructura

```c
typedef struct gex_d3d11_mesh {
    ID3D11Buffer *vertex_buffer;
    ID3D11Buffer *index_buffer;
    UINT          vertex_count, index_count;
    UINT          vertex_stride;        /* 28 */
    UINT          index_stride;         /* 2 o 4 */
    DXGI_FORMAT   index_format;         /* R16_UINT o R32_UINT */
    UINT          vertex_buffer_bytes, index_buffer_bytes;
    HRESULT       last_hresult;         /* S_OK o el fallo de CreateBuffer */
    int           initialized;          /* 1 = vivo */
} gex_d3d11_mesh;
```

`GEX_D3D11_MESH_INITIALIZER` produce exactamente el mismo estado que
`memset(&mesh, 0, sizeof mesh)` salvo `last_hresult = S_OK`, que en cero también
es `S_OK` (0). `destroy` devuelve la estructura a ese estado, así que se puede
reutilizar y una segunda llamada es un no-op.

### 13.3 Decisiones

1. **`D3D11_USAGE_IMMUTABLE`, sin acceso de CPU.** La geometría de un nivel no
   cambia por frame; inmutable es el uso más barato y el que menos puede
   romperse. `BindFlags` es `VERTEX_BUFFER` o `INDEX_BUFFER` según el buffer.
2. **Formato de índice automático.** `vertex_count <= 65536` →
   `DXGI_FORMAT_R16_UINT` (índices 0..65535 caben); por encima,
   `DXGI_FORMAT_R32_UINT`. La conversión a 16 bits se hace en un bloque temporal
   que se libera siempre, y solo después de validar que todo índice está en
   rango.
3. **Validación antes de crear nada.** Se rechaza la geometría vacía, los
   punteros nulos con conteos > 0, `index_count % 3 != 0`, cualquier índice
   `>= vertex_count`, y cualquier tamaño que no quepa en `UINT` o que desborde
   `size_t`.
4. **Sin fugas en error.** Si falla el buffer de índices se libera el de vértices
   antes de devolver; si falla la reserva temporal no se crea nada.
5. **`BAD_STATE` protege un mesh vivo.** Un segundo `create` sobre un mesh
   inicializado devuelve error y no toca ni un campo: el llamador debe destruir
   primero. Así un fallo de doble creación no filtra dos buffers.
6. **`create_from_emit`** encadena `gex_geometry_build` + `create` y libera
   siempre la geometría intermedia. Los errores de conversión se traducen a
   `BAD_GEOMETRY` (argumentos/conteos) o `OUT_OF_MEMORY` (reserva), con
   `last_hresult = S_OK`.

### 13.4 Estados

| Estado | Significado |
|--------|-------------|
| `GEX_D3D11_MESH_OK` | ambos buffers creados |
| `GEX_D3D11_MESH_BAD_ARGUMENT` | device, geometry o mesh nulos |
| `GEX_D3D11_MESH_BAD_STATE` | el mesh ya estaba inicializado |
| `GEX_D3D11_MESH_BAD_GEOMETRY` | vacía, conteos o índices inválidos, tamaño no representable |
| `GEX_D3D11_MESH_BUFFER_FAILED` | `CreateBuffer` falló; el `HRESULT` está en `last_hresult` |
| `GEX_D3D11_MESH_OUT_OF_MEMORY` | falló la reserva temporal de índices de 16 bits |

### 13.5 Pruebas

`port/tests/test_d3d11_mesh.c` — **108 comprobaciones, 0 fallos** en Clang
`-O0` y `-O2`, con `-Wall -Wextra -Wpedantic` y sin warnings. Sin ventanas, sin
swapchain y sin demos interactivas.

1. **Nombres de estado** y contrato del inicializador (idéntico a cero,
   `destroy` de un mesh inicial es no-op, `destroy(NULL)` seguro).
2. **WARP** (el rasterizador software de Windows): 3 triángulos → 9 vértices y
   9 índices; se comprueban `ByteWidth`, `Usage`, `BindFlags` y
   `CPUAccessFlags` con `GetDesc`, y el **contenido exacto** copiando cada buffer
   a un *staging* y leyéndolo: los 28 B de cada vértice y los índices son los
   convertidos, con color, UV, textura, capa y flags intactos.
3. **Frontera de formato de índice:** 65 536 vértices → `R16_UINT` con
   `0xFFFF` leído de vuelta; 65 537 → `R32_UINT` con `0x10000` leído de vuelta.
4. **Errores:** punteros nulos, geometría vacía, punteros nulos con conteos > 0,
   `index_count` no múltiplo de 3, índice fuera de rango, doble creación
   (`BAD_STATE` con el mesh intacto) y `create_from_emit` con conteos
   incoherentes y con lista vacía.
5. **Hardware** cuando el runtime lo permite; en esta máquina se obtuvo
   *feature level* 11_1 y los buffers se crearon también en hardware. Si el
   runtime lo rechazara, la prueba imprime el límite y omite el bloque (nunca un
   aprobado falso).
6. **AZTEC16 real:** 2 242 triángulos, 3 918 vértices y 6 726 índices →
   vertex buffer de 109 704 B y index buffer de 13 452 B, creados con WARP, con
   los índices leídos de la GPU comprobados uno a uno contra la conversión.

### 13.6 Lo que sigue sin poder afirmarse

- Que la escena se **vea**: no hay shaders, ni input layout, ni pipeline, ni
  `DrawIndexed`.
- Que la emisión sea **idéntica** al ejecutable: falta la comparación cuantitativa
  de S-2.
- Que las texturas estén resueltas: el índice de 14 bits se transporta por
  vértice, pero el `.VFX`/NCC no se carga (Módulo 4).
- Que el *depth buffer* use `z`: la profundidad se conserva por vértice, pero su
  transformación a espacio de proyección pertenece al Módulo 5.

---

## 14. Módulo 5A: primer renderizado de geometría 3D (2026-11-11)

Implementado en `port/include/gex/render/d3d11_pipeline.h` y
`port/src/render/d3d11_pipeline.c` (Windows-only), con la geometría de prueba
en `port/include/gex/geometry_shapes.h` +
`port/src/geometry/geometry_shapes.c` (portable). Pruebas en
`port/tests/test_d3d11_pipeline.c` y `port/tests/test_geometry_shapes.c`.

### 14.1 Qué hace

Cierra el camino `gex_emit_output → gex_geometry → ID3D11Buffer → DrawIndexed`:
compila un vertex shader y un pixel shader mínimos, crea el
`ID3D11InputLayout` que describe los 28 B reales del vértice, configura los
estados, conecta los buffers con `IASetVertexBuffers`/`IASetIndexBuffer` y
dibuja con `DrawIndexed`.

**No hace:** texturas, materiales NCC, blending complejo, *depth buffer*, ni
el `ID3D11InputLayout` definitivo de producción. Es color por vértice.

### 14.2 Shaders y compilación

**Auditoría del entorno (2026-11-11):**

| Herramienta | Estado |
|---|---|
| `fxc.exe` (Windows SDK) | **no disponible** en el entorno |
| `d3dcompiler.h` | disponible en el toolchain (mingw-w64) |
| `libd3dcompiler.a` | disponible en el toolchain |
| `d3dcompiler_47.dll` | **presente** en `C:\Windows\System32` |

Decisión: **compilación HLSL en ejecución** con `d3dcompiler_47.dll`, que es un
componente de Windows con el mismo criterio que `d3d11.dll` y `dsound.dll` ya
usados por el port. No se instala nada ni se modifica la configuración. La DLL
se carga bajo demanda con `LoadLibraryA`/`GetProcAddress`, de modo que la
aplicación no tiene dependencia de arranque: si falta, el pipeline devuelve
`GEX_D3D11_PIPELINE_NO_COMPILER` con el texto del error y el port sigue
funcionando (limpia y presenting).

El HLSL vive como cadenas C en `src/render/d3d11_pipeline.c` (`GEX_HLSL_VERTEX`
y `GEX_HLSL_PIXEL`): no hay bytecode precompilado en el repositorio, así que
no hay que regenerar ni versionar nada, y el fuente es legible y editable.

**Hallazgo importante (comprobado con WARP).** El enlazador de Direct3D 11 no
conecta la entrada de un pixel shader declarada como parámetro suelto con la
salida del vertex shader: `float4 GexPS(float4 color : COLOR)` recibía el
**registro 0** de la salida del VS (SV_POSITION, interpolada como *varying*
normal), de modo que el color que llegaba era en realidad la posición recortada. La
solución es que **el pixel shader reciba la misma estructura que el vertex
shader declara como salida** (el estilo canónico de las muestras de D3D11):

```hlsl
struct VSOut {
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
};
float4 GexPS(VSOut input) : SV_TARGET { return input.color; }
```

Con esa forma el color llega correcto (verificado píxel a píxel). La prueba
`test_d3d11_pipeline` cubre el caso, de modo que una regresión en el enlazado
se detectaría.

### 14.3 Formato de entrada y orden de bytes del color

El `ID3D11InputLayout` describe exactamente los 28 B de `gex_vertex`:

| Semántica | Formato DXGI | Offset |
|---|---|---:|
| `POSITION` | `R32G32B32_FLOAT` | 0 |
| `COLOR` | `R8G8B8A8_UNORM` | 12 |
| `TEXCOORD` | `R32G32_FLOAT` | 16 |
| `TEXINDEX` | `R16_UINT` | 24 |
| `LAYER` | `R8_SINT` | 26 |
| `FLAGS` | `R8_UINT` | 27 |

Dos detalles que se comprobaron contra el SDK de Windows:

1. **`D3D11_INPUT_ELEMENT_DESC` no tiene campo `Offset`.** El byte offset
   dentro del vertex buffer va en `AlignedByteOffset` (el `Offset` de D3D10 no
   existe aquí). Los offsets 26 y 27 son válidos: el runtime acepta el layout
   completo.
2. **El color es AARRGGBB en un `uint32` y D3D11 no admite
   `B8G8R8A8_UNORM` en un vertex buffer.** Con `R8G8B8A8_UNORM` los bytes se
   leen en orden de memoria (BB, GG, RR, AA en little-endian), así que el
   shader recibe los canales R,G,B,A con R y B **intercambiados**. El vertex
   shader reordena: `float4(color.b, color.g, color.r, color.a)`. La prueba
   usa un color con R y B distintos (`0xFF3366CC`) para que un intercambio
   sea detectable.

El render target de las pruebas es `DXGI_FORMAT_R8G8B8A8_UNORM`, cuyos bytes en
memoria son R, G, B, A.

### 14.4 Estados

| Estado | Valor | Motivo |
|---|---|---|
| Topología | `D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST` | la emisión produce triángulos |
| Rasterizador | `FILL_SOLID`, `CULL_NONE`, `DepthClipEnable = TRUE` | la emisión ya descartó por determinante 2D; no se supone el sentido de giro |
| Blend | desactivado, escritura de todos los canales | el color del vértice se escribe tal cual |
| Depth/Stencil | `DepthEnable = FALSE`, escritura cero | sin depth buffer: el orden de dibujo es el de emisión (algoritmo del pintor, como el original) |
| Viewport | el tamaño real del render target | |
| Constant buffer | `viewport_size` (xy = píxeles, z = divisor de z) y `reference_size` (xy = resolución lógica) | dinámico, `MAP_WRITE_DISCARD` |

### 14.5 Coordenadas

Los vértices llevan **X/Y ya proyectadas en píxeles** del espacio lógico del
original (512x384, centro en 256/192) y **Z como profundidad entera**. El
vertex shader **no aplica ninguna matriz de cámara**: solo convierte píxeles
lógicos a espacio de clip, que es lo que exige el rasterizador.

```
p        = pos.xy - reference_size.xy * 0.5      /* centrado */
scale    = min(vp.x / ref.x, vp.y / ref.y)        /* escala uniforme */
ndc.xy   = ( p.xy * scale * 2 ) / vp.xy, con la Y negada
ndc.z    = pos.z / 65535                          /* profundidad -> [0,1] */
```

Con la escala uniforme la imagen lógica no se deforma al cambiar el tamaño de
la ventana, y la profundidad entra en [0,1] para no ser recortada por el
*depth clip* (el clip space exige 0 ≤ z ≤ w). La prueba lo comprueba: con un
viewport de 256x192 y una referencia de 512x384, el vértice lógico (64,64)
cae exactamente en el píxel (32,32); con un viewport de 320x240, en el (40,40).

### 14.6 Integración con el renderer

`gex_d3d11_renderer_set_geometry(renderer, pipeline, mesh, ref_w, ref_h)`
registra la geometría (punteros **prestados**) y el renderer la dibuja en cada
`gex_d3d11_renderer_present`, entre el `ClearRenderTargetView` y el `Present`.
Si no hay pipeline o malla, el renderer se comporta exactamente como antes:
limpia y presenta. Un fallo de dibujo se cuenta en `draw_errors`, se anota en
`last_hresult` y la llamada devuelve error, pero el frame se presenta igualmente.

En la aplicación (`src/app/i4_main_win32.c`) se crea el pipeline, el cubo de
prueba y la malla, y se registran en el renderer. Si algo falla, la aplicación
avisa por consola y sigue con el comportamiento anterior (solo limpiar).

### 14.7 Geometría de prueba

`gex_shape_cube(out, angle_radians)` (portable) emite un cubo con las
estructuras reales de la emisión:

- 6 caras x 2 triángulos = 12 triángulos, en el espacio lógico 512x384;
- color por cara (rojo, verde, azul, amarillo, cian, magenta) sombreado por
  su normal con una luz fija (Lambert; la normal se normaliza, §15.1);
- profundidad por vértice, con las caras ordenadas de atrás hacia delante
  (algoritmo del pintor);
- giro alrededor de Y parametrizable e inclinación fija de 0,35 rad para que
  se vean tres caras a la vez;
- `flags = GEX_EMIT_F_NEAR_EMITTER`, sin niebla y sin texturas.

### 14.8 Pruebas

`port/tests/test_d3d11_pipeline.c` — **137 comprobaciones, 0 fallos** sin
inyección de fallos y **287, 0 fallos** con `-DGEX_PIPELINE_TEST_FAULT_INJECT`
(Clang `-O0` y `-O2`), **offscreen con WARP**: sin ventanas, sin swapchain y
sin demos interactivas. Cubre:

1. nombres de estado y contrato del inicializador;
2. el input layout frente a los offsets y formatos publicados, y que el
   último byte describe los 28 B;
3. init: errores (argumento nulo, dispositivo no inicializado, doble init) y
   que tras un error no queda ningún recurso a medias;
4. init correcto: shaders compilados, los seis recursos vivos y
   `last_error` vacío; cierre y reutilización;
5. dibujo: se generan píxeles distintos del color de limpieza, el color del
   vértice llega con R y B en su sitio y el alfa intacto, y fuera del triángulo
   sigue el color de limpieza;
6. interpolación del color por vértice (el centroide es el promedio);
7. resize: con otro tamaño de render target, la geometría se coloca en la
   posición que corresponde a la nueva escala;
8. geometría vacía, malla nula, destino nulo, tamaños 0 y estados inválidos;
9. recursos parcialmente vivos: una malla con un índice fuera de rango no crea
   nada y el pipeline sigue utilizable;
10. el cubo de prueba se dibuja y cubre una parte apreciable del viewport;
11. el contrato de `gex_d3d11_renderer_set_geometry` (argumentos nulos y
    renderer no inicializado);
12. con la inyección de fallos: un fallo en cada una de las diez etapas de
    inicialización del pipeline no deja ningún recurso vivo y conserva el
    diagnóstico (§15.5).

`port/tests/test_geometry_shapes.c` — **355 comprobaciones, 0 fallos**
(portable, Clang `-O0`, `-O2` y ASan/UBSan): forma de la emisión, colores,
profundidades, orden del pintor, determinismo, efecto del giro y conversión a
`gex_geometry`.

### 14.9 Limitaciones y siguiente paso

- **La validación dinámica del recorrido y la visibilidad está hecha** (§ 4.3); falta la comparación de la emisión: S-2 sigue
  abierto y el primer escenario original **no** está terminado.
- ~~**No hay texturas ni materiales**~~: en esta sección (§14, el Módulo
  5A) era así; desde el Módulo 4C **sí hay texturas** (§18) y lo que sigue
  sin existir son los materiales con iluminación. El índice de
  textura viaja por vértice pero no se resuelve.
- **No hay depth buffer**: el orden de las caras lo resuelve el algoritmo del
  pintor, igual que el original. Con geometría que se solape de forma
  interpenetrada haría falta un *depth buffer* real.
- **La geometría de prueba es estática**: los buffers son inmutables, así que
  animarla exigiría recrearlos o usar buffers dinámicos.
- **El color se escribe sin blending**: el canal alfa del vértice se escribe en
  el render target pero no se usa para transparency; el blending del original
  está pendiente.
  ~~Estado 2026-11-11~~ → **actualizado 2026-11-12:** el blending del original
  ya no está pendiente en su forma básica: su aritmética está **demostrada**
  (§19.8) y el port la aplica. Lo pendiente es el **modo de mezcla por
  comando** (§19.10) y el **alfa de vértice** (§19.11).
- Siguiente paso: **Módulo 4C (carga de texturas `.VFX` en D3D11; el Módulo 4B
  ya decodifica el píxel y las dimensiones)** y, con él, la firma definitiva del
  shader de producción, el *depth buffer* y el `ID3D11InputLayout` con los
  atributos que consuma el material.

---

## 15. Auditoría de inicialización, limpieza y primera ejecución real (2026-11-11)

Auditoría del Módulo 5A sobre el commit `967726e`: orden de recursos COM,
liberación, punteros prestados, diagnóstico de errores y **primera ejecución
real de la aplicación** con ventana Win32 y swapchain.

### 15.1 Defectos encontrados y corregidos

1. **Liberación parcial (el defecto principal).** La limpieza de pipeline, malla
   y geometría estaba dentro de `if (geometry_ready)`, una bandera que solo se
   activaba si la geometría llegaba a registrarse en el renderer. Si la malla
   fallaba **después** de que el pipeline se hubiera creado, el pipeline
   entero (shaders, input layout, estados, constant buffer y el handle de
   `d3dcompiler_47.dll`) y los triángulos del cubo se perdían. **Corrección:**
   cada recurso se libera según su propio estado y todas las llamadas son
   idempotentes, de modo que el cierre es completo aunque cualquier etapa
   intermedia haya fallado.
2. **El diagnóstico se perdía al limpiar.** `release_all()` deja la estructura
   a cero, así que el mensaje de error (el texto del compilador HLSL, o «no se
   pudo cargar d3dcompiler_47.dll») se borraba exactamente cuando el
   llamador iba a leerlo. **Corrección:** helper `fail_init()` que copia el
   mensaje y el `HRESULT` a variables locales, limpia y los restaura.
3. **Normal de la cara sin normalizar (defecto cosmético, encontrado con la
   ejecución real).** El sombreado Lambert del cubo usaba el producto vectorial
   de dos aristas **sin normalizar** (las aristas miden 2 unidades, así que el
   producto vectorial mide hasta 4). El factor de sombra llegaba a 2,6 y las
   caras se quemaban a 255 en el canal rojo. **Corrección:** normalización de
   la normal; los factores quedan en [0,35, 1,0] y los colores ya no saturan.
   Solo se detectó al mirar la ventana real: las pruebas offscreen medían
   posiciones y colores del vértice, no el sombreado de la forma.
4. **Los errores de dibujo no se diagnosticaban.** El renderer contaba los
   fallos pero no exponía la causa. **Corrección:** el renderer guarda
   `last_draw_status` y `last_draw_hresult`, y la aplicación imprime el estado
   del pipeline cuando un frame falla.

### 15.2 Orden de recursos COM

**Creación** (cada paso depende del anterior):

```
ventana Win32
  -> gex_d3d11_device_init           (dispositivo + contexto)
  -> gex_d3d11_swapchain_init        (swapchain + backbuffer)
  -> gex_i4_app_enable_d3d11         (renderer + RTV del backbuffer)
  -> gex_d3d11_pipeline_init         (shaders, layout, estados, constant buffer)
  -> gex_shape_cube                  (geometría en memoria del host)
  -> gex_d3d11_mesh_create_from_emit (vertex/index buffers)
  -> gex_d3d11_renderer_set_geometry (el renderer toma los punteros)
```

**Destrucción** (orden inverso, cada recurso según su propio estado):

```
gex_d3d11_renderer_set_geometry(NULL, NULL)  -> el renderer suelta la geometría
gex_d3d11_mesh_destroy                        -> vertex/index buffers
gex_d3d11_pipeline_shutdown                   -> shaders, layout, estados, DLL
gex_emit_output_free                          -> triángulos del host
gex_i4_app_disable_d3d11                      -> renderer + RTV
gex_d3d11_swapchain_shutdown                  -> swapchain + backbuffer
gex_d3d11_device_shutdown                     -> contexto + dispositivo
```

El paso 1 es el que garantiza que **no se dibuje con recursos destruidos**:
el renderer solo tiene punteros prestados y deja de referenciarlos antes de que
la malla y el pipeline se liberen.

### 15.3 Punteros prestados

| Prestador | Prestado a | Ciclo de vida |
|---|---|---|
| `gex_d3d11_device` | renderer, pipeline | propiedad de R-1; el renderer y el pipeline solo lo apuntan |
| `gex_d3d11_swapchain` | renderer | propiedad de R-2 |
| `gex_d3d11_pipeline` | renderer | propiedad de la aplicación; el renderer no lo libera |
| `gex_d3d11_mesh` | renderer | propiedad de la aplicación; el renderer no lo libera |

El renderer nunca libera lo que no es suyo: `gex_d3d11_renderer_shutdown` deja
la estructura a cero, con lo que los punteros prestados desaparecen.

### 15.4 Comportamiento sin `d3dcompiler_47.dll`

El pipeline devuelve `GEX_D3D11_PIPELINE_NO_COMPILER` con el mensaje «no se
pudo cargar d3dcompiler_47.dll» y **sin dejar ningún recurso vivo**. La
aplicación lo muestra y sigue funcionando con el comportamiento anterior
(limpiar y presentar). Comprobado con una variante de la aplicación que fuerza
ese fallo (etapa 1 de la inyección de fallos):

```
geometry: none (pipeline: shader compiler unavailable, shape: not attempted, mesh: bad argument)
shader error: no se pudo cargar d3dcompiler_47.dll
...
exit: clean, frame errors=0, clock warnings=0
```

### 15.5 Fallos de inicialización en cada etapa

Se añadió inyección de fallos al pipeline (`GEX_PIPELINE_TEST_FAULT_INJECT` y
`gex_d3d11_pipeline_test_fail_stage`), con el mismo patrón que
`GEX_BSP_TEST_FAULT_INJECT` y `GEX_GEOM_TEST_FAULT_INJECT`. Las diez etapas
son: cargar el compilador, compilar el VS, compilar el PS, crear el VS, crear
el PS, crear el input layout, crear el rasterizador, crear el estado de
blend, crear el estado de profundidad y crear el constant buffer.

Para cada etapa se comprueba que: el estado devuelto es el esperado para esa
etapa, **no queda ningún puntero COM vivo** en la estructura, el
diagnóstico sobrevive a la limpieza (`last_error` no vacío y `last_hresult`
distinto de `S_OK`), el handle de la DLL queda liberado y **el dispositivo
sigue siendo utilizable** (un reintento sin fallo tiene éxito).

Limitación: la API pública de D3D11 no expone el recuento de referencias, así
que la ausencia de fugas se comprueba estructuralmente (todos los punteros a
cero) y por el comportamiento observable (el dispositivo sigue vivo), no con
un contador de referencias.

### 15.6 Redimensionado y presentación

- El renderer recrea el RTV tras un `ResizeBuffers` (`gex_d3d11_renderer_resize`).
- El pipeline **no guarda el RTV**: lo recibe en cada `set_target`, que además
  actualiza el viewport y el constant buffer. Por eso un cambio de tamaño se
  refleja en el siguiente frame sin recrear ningún recurso.
- La aplicación crea la ventana a 960x540 fijos y **no** gestiona `WM_SIZE`, así
  que el redimensionado real de la ventana no se ejercita en la ejecución real;
  el caso se cubre con render targets de distinto tamaño en la prueba
  offscreen (256x192 y 320x240), que comprueba además que la proyección
  cambia de escala como debe.

### 15.7 Primera ejecución real

Sí se pudo ejecutar la aplicación real. Entorno: sesión interactiva de
escritorio (sesión 1, 1920x1080), compilador Clang 22.1.8, `d3d11.dll` del
sistema y **GPU real** (el dispositivo se creó en modo hardware, no WARP).

```
Gex native loop - I-4
window: class=GexNativeLoopWindow, 960x540 client area
renderer: Direct3D 11 (hardware)
backbuffer: 960 x 540, 2 buffers
clear color: RGB(0.2, 0.2, 0.8)
geometry: cube, 12 triangles, vertex colors, no textures (512x384 logical)
frame limit reached (30 presents)
final: t=0.38s presents=30 fps=79.6 ...
exit: clean, frame errors=0, clock warnings=0
```

**Evidencia visual:** la ventana se capturó con `PrintWindow`
(`PW_RENDERFULLCONTENT`) mientras la aplicación estaba presenting y se
analizaron los píxeles:

| Comprobación | Resultado |
|---|---|
| Ventana visible | sí, área de cliente 960x540 |
| Píxeles distintos del color de limpieza | 105 959 de 470 000 analizados (22,5 %) |
| Caja del cubo | x=311..663, y=121..479 (centrada en la ventana) |
| Colores presentes | (206,59,206) magenta, (91,26,26) rojo oscuro, (147,147,42) amarillo apagado |

Los tres colores son exactamente las tres caras visibles del cubo con el
sombreado Lambert aplicado (base × factor, sin saturar). La captura
**no** se publica en el repositorio: es una evidencia local, no un asset del
proyecto.

### 15.8 Las cuatro categorías de evidencia (separadas)

| Categoría | Estado | Cómo se comprobó |
|---|---|---|
| **Pipeline validado con pruebas offscreen** | **SÍ** | `test_d3d11_pipeline`: 287 comprobaciones (con inyección de fallos) y 137 (sin ella), 0 fallos; WARP, render target propio, sin ventanas |
| **Aplicación compilada** | **SÍ** | `gexloop.exe` enlazado con device, swapchain, renderer, pipeline, malla y geometría; compilación sin warnings propios |
| **Ejecución real con ventana comprobada** | **SÍ** | la aplicación se ejecutó con ventana Win32 y swapchain real: 30 frames presentados, 0 errores de frame, salida limpia |
| **Evidencia visual confirmada** | **SÍ** | captura de la ventana real: el cubo aparece, con las tres caras y el color esperado |

Lo que **no** se afirma: que esto sea una escena del juego original. En
esta sección (§15, el Módulo 5A) la geometría visible es el cubo de prueba.
**Actualización posterior (2026-11-11):** el nivel real y sus texturas ya se
cargan en la aplicación (§16 y §18) y la emisión de S-2 está validada contra
el ejecutable original para la ruta de MAP7 (§18.8). La cámara real del juego
está además integrada en el renderer (§15 de `S2_CAMERA_VISIBILITY.md`): el
nivel se dibuja con la matriz `cam+0xE0` y la visibilidad original, y el
modo diagnóstico queda como opción (`--diagnostic-view`).

### 15.9 Limitaciones que siguen abiertas

Al cierre del Módulo 5A quedaban estas; **el estado actual es**:

- ~~La geometría de la aplicación es el cubo de prueba, no un nivel~~ →
  **cerrado por el Módulo 5B**: con `--level` se carga el `.DFX` y se dibuja su
  geometría real (§16).
- ~~Sin texturas~~ → **cerrado por el Módulo 4C**: las texturas del `.VFX` se
  decodifican y se muestrean (§18).
- El cubo es estático (buffers inmutables); animarlo exige buffers dinámicos →
  **sigue abierto**.
- **Sin materiales NCC con iluminación, sin *depth buffer* y con el blending
  alfa aplicado a todo el dibujo** (el original lo activa por polígono) →
  **sigue abierto** (§18.5 y §18.8).
- La ventana no se redimensiona (no hay gestión de `WM_SIZE`) → **sigue
  abierto**.
- S-2 tiene el recorrido, la visibilidad y la emisión de nivel (ruta MAP7)
  validados dinámicamente contra el ejecutable original, y la cámara real
  integrada en el renderer; siguen abiertos el emisor lejano, la LUT de
  niebla por vértice y los materiales.

## 16. Módulo 5B: primera geometría real de AZTEC16.DFX (2026-11-11)

Sustituye el cubo de prueba por geometría real de un nivel `.DFX` en la
ventana de `gexloop.exe`. Reutiliza los módulos que ya existían; no hay una
arquitectura paralela.

### 16.1 Módulo nuevo: `gex/scene_level.h`, `src/scene/scene_level.c`

Portable C11 (sin Windows ni D3D11). Encadena los módulos existentes:

```
gex_dfx_load            -> contenedor .DFX en memoria
gex_dfx_load_level      -> descriptor del nivel
gex_bsp_walk            -> hojas visibles del BSP
gex_level_emit          -> triángulos emitidos
gex_geometry_build      -> vértices e índices
```

y expone las estadísticas (`gex_scene_stats`) y la vista diagnóstica
(`gex_scene_diagnostic_view`). La conversión a buffers D3D11 no vive aquí: la
aplicación llama a `gex_d3d11_mesh_create` con la geometría ya convertida y
registra la malla en el renderer con `gex_d3d11_renderer_set_geometry`, igual
que el cubo.

Cada bloque de la escena tiene su propia bandera (`container_loaded`,
`level_loaded`, `leaves_loaded`, `emit_loaded`, `geometry_loaded`), así que
`gex_scene_level_free` libera exactamente lo que se llegó a crear, aunque la
carga falle a mitad (mismo criterio que la auditoría del Módulo 5A).

### 16.2 La vista diagnóstica (NO es la cámara original)

`gex_scene_diagnostic_view` deja una matriz de emisión **sintética y
determinista**, con estos parámetros:

| Parámetro | Valor | Significado |
|---|---|---|
| `m[0] = m[4] = m[8]` | `0x0400` (0,25 en Q12) | escala de los tres ejes |
| `t[2]` | `0x1000` | traslación de profundidad |
| `focal` | `0x140` (320) | el del original |
| `cx`, `cy` | 256, 192 al inicio; ajustados por el encuadre | centro de pantalla de la proyección |
| `near_z` | `0xA0` (160) | el del original |
| niebla | `none_z = 3000`, `total_z = 5000`, `split_z = 0` | generador 0x461895, ruta del emisor lejano |

`cx` y `cy` son un **desplazamiento en pantalla puro** dentro de la
proyección (`xs = cam0*focal/cam2 + cx`), así que recentrar el nivel no altera
la perspectiva.

Esto **no** es la cámara del juego: el original usa la matriz `cam+0xE0` con
la orientación real de la cámara, y el port todavía no tiene ese estado. La
visibilidad del BSP es además «todas las hojas», porque la prueba de esfera de
`gex_camera` necesita el estado de cámara del juego, ya capturado y validado (§ 4.3). **La comparación
de la cámara, del recorrido BSP y de la emisión contra el ejecutable ya está
hecha** (§ 4.3 y § 18.8) y la cámara real está integrada en el renderer
(§ 15 de `S2_CAMERA_VISIBILITY.md`); esta vista sintética se conserva como
opción de diagnóstico y no sustituye a la original.

### 16.3 Encuadre automático

Con `cx = 256` y `cy = 192` la geometría de AZTEC16 cae en x 18..259 e
y 124..408 del espacio lógico 512x384: queda a un lado y se sale por abajo. Se
recentra y se reduce la escala hasta que cabe, en cuatro pasadas de emisión:

1. se emite con la vista base;
2. se acumula la caja envolvente de todas las emisiones (unión, no la última:
   recentrar hace entrar polígonos nuevos por el gate);
3. se calcula el desplazamiento de `cx`/`cy` y, si la caja no cabe en
   512x384, se reduce `m[0]` y `m[4]` por igual (la perspectiva no se deforma);
4. se vuelve a emitir con esa vista.

Es determinista (depende solo de las emisiones anteriores) y vale para
cualquier nivel, no solo para AZTEC16.

### 16.4 Hallazgos durante la implementación

1. **La niebla saturaba y la imagen salía de un solo color.** Con los
   parámetros de niebla de la prueba (`none_z = 100`, `total_z = 400`) y el
   rango de profundidad real de la vista (z entre 3053 y 4896) el factor de
   niebla era 4096 (100 %) en **todo** el nivel: la captura de la ventana
   mostraba un único color, el de la niebla. Corrección: `none_z = 3000`,
   `total_z = 5000`, que dan un degrade de 0 % a 100 % justo en el rango de
   la vista.
2. **El divisor de la LUT es `z`, no `z/4`.** La LUT se indexa con `z >> 2`,
   pero el valor se calculó con `i ≈ z` (el bucle escribe `lut[i>>2]` con
   `i = 4k+1`). Por eso el factor es 0 en `z <= none_z` y total en
   `z >= total_z`, que es justo lo que dicen los nombres de los campos. La
   primera lectura (divisor `z/4`) daba un rango de niebla imposible.
3. **`a_q12` y `b_q12` quedaban a cero.** El generador público
   `gex_emit_fog_generate` los rellena ahora con la convención documentada
   (`b_q12 = B`, `a_q12 = A << 12`), de modo que la ruta analítica
   (0x46A150) da el mismo factor que la LUT. Comprobado con una prueba nueva
   que compara ambas rutas.
4. **El gate depende de la vista.** Al recentrar la geometría entran
   polígonos que antes quedaban fuera de pantalla, así que el número de
   triángulos cambia (2242 → 2854 en AZTEC16). Por eso el encuadre itera.
5. **Un error de signo en el primer encuadre** desplazaba la geometría en
   sentido contrario y el gate descartaba casi todo (2242 → 472 triángulos).
   Detectado al medir la caja envolvente; corregido.
6. **El generador de niebla estaba duplicado** entre la prueba de emisión y
   el consumidor. Ahora es público (`gex_emit_fog_generate`) y la prueba lo
   usa, así que hay una sola implementación.

### 16.5 Estadísticas reales (AZTEC16.DFX)

```
level: entries=16082 vertices=3882 polygons=9609 leaves=815
emitted: triangles=2854 (fog passes=1427) discarded by depth=0
geometry: vertices=4914 indices=8562
view: DIAGNOSTIC: scale=0x3C0 cx=315 cy=154
```

Las 815 hojas y los conteos del descriptor son los del nivel; los triángulos
son los de la **vista diagnóstica** (no los del juego con su cámara real, ya validada).

### 16.6 Integración en la aplicación

`gexloop.exe` acepta `--level <ruta>`:

- con `--level`: carga el nivel, crea la malla y dibuja la geometría real;
- sin `--level`: el cubo de prueba sigue disponible;
- si la carga falla, se informa del error por consola y la aplicación **no
  dibuja geometría** (no se sustituye por resultados inventados): sigue
  limpiando y presentando y cierra con normalidad.

### 16.7 Pruebas

`port/tests/test_scene_level.c` — **8 627 comprobaciones, 0 fallos** (Clang
`-O0`, `-O2` y ASan/UBSan): nombres de estado, argumentos nulos, ruta
inexistente, archivo que no es un contenedor DFX, la vista diagnóstica
(determinista, con su niebla y su degrade) y el nivel real (estadísticas,
coherencia de índices, determinismo entre dos cargas y liberación
idempotente).

`port/tests/test_level_emit.c` sube a **38 855 comprobaciones** (antes
38 855) con el generador de niebla público y la coherencia LUT/analítica.

Suite completa sin regresiones: `test_sag` 69 305, `test_camera` 889,
`test_bsp` 37, `test_level_emit` 38 855, `test_geometry` 6 963,
`test_geometry_shapes` 355, `test_scene_level` 8 627, `test_adpcm` 60 052,
`test_audio` 215, `test_frame` 410, `test_input` 826, `test_i4_app` 117,
`test_d3d11_device` 332, `test_d3d11_mesh` 108, `test_d3d11_pipeline` 287.

### 16.8 Primera visualización real

Sí: `gexloop.exe --level C:\GEX_REZ\E2_A_ORIGINAL\LEVEL\AZTEC16.DFX` con
ventana Win32 y **GPU real** (dispositivo *hardware*). 15 frames presentados,
0 errores de frame, salida limpia. La ventana se capturó con `PrintWindow` y
se analizaron los píxeles:

| Comprobación | Resultado |
|---|---|
| Píxeles distintos del color de limpieza | 88 630 de 470 000 (18,9 %) |
| Caja de la geometría | x=220..574, y=152..539 |
| Colores distintos | 23 574 |

La imagen muestra estructuras arquitectónicas del nivel (paredes, columnas,
repisas y rampas) con el degrade de niebla por profundidad y los artefactos
propios del algoritmo del pintor (sin *depth buffer*). La captura es una
evidencia local y **no se publica** en el repositorio.

### 16.9 Lo que sigue pendiente

- **Texturas**: la imagen usa solo el color por vértice; el nivel real se ve
  como una maqueta monocroma. El contenedor `.VFX` (Módulo 4A, §17), el
  **decodificador de píxel** (Módulo 4B, con formato de píxel y dimensiones
  demostrados) y los **recursos D3D11 con el muestreo de textura** (Módulo 4C,
  §18) ya están implementados: el nivel sale texturizado. Siguen pendientes
  los **materiales de producción**, el ***depth buffer***, el **blending por
  polígono** y la **niebla aplicada después de la textura** (§18.8).
  ~~Estado 2026-11-11~~ → **actualizado 2026-11-12:** el *blending por
  polígono* está **resuelto** en su aritmética (§19.8) y aplicado por el port;
  lo que queda de ese bloque es el **modo de mezcla por comando** (§19.10) y
  el **alfa de vértice** (§19.11).
- **Cámara real**: el estado de cámara del juego ya está capturado y validado; la vista sigue siendo diagnóstica en cuanto a la
  juego para usar `cam+0xE0` y la visibilidad por esfera.
- **Sin *depth buffer***: el orden del pintor produce solapes visibles.
- **Sin *blending***: el canal alfa (0 en la pasada de niebla) no se mezcla.
- La geometría se carga una vez al arrancar; no hay animación ni cámara
  interactiva.

---

## 17. Módulos 4A y 4B: el contenedor de texturas `.VFX` y el formato de píxel (2026-11-11)

El documento técnico completo está en
[`VFX_TEXTURE_FORMAT.md`](../VFX_TEXTURE_FORMAT.md). Resumen:

- **Contenedor demostrado** contra el ejecutable original y los ficheros
  originales: `count` en `+0x00`; tabla de registros con cabecera de `0x8C` B,
  tamaño del bloque de datos en `+0x84` y datos en `+0x8C`; el siguiente
  registro está en `registro + 0x8C + dword[+0x84]`. El recorrido termina
  exactamente en el EOF de los 37 `.VFX` de `E2_A_ORIGINAL`.
- **El puntero de `+0x10` es un valor de relleno en el fichero** (el juego lo
  sobrescribe con `registro + 0x8C`): hay que ignorarlo.
- **El registro es un `GrTexInfo` de Glide 2.x**: `smallLod` en `+0x00`,
  `largeLod` en `+0x04`, `aspectRatio` en `+0x08`, `format` en `+0x0C` y el
  puntero a los datos en `+0x10`.
- **El formato de la textura está en `+0x0C`** y sus valores son los de la
  enumeración `GrTextureFormat_t` de Glide 2.x: **1 = `GR_TEXFMT_YIQ_422`**,
  **11 = `GR_TEXFMT_ARGB_1555`** y **12 = `GR_TEXFMT_ARGB_4444`**. Solo el 1
  lleva **tabla NCC** (`GuNccTable`, 112 B) en `+0x14` y solo el 12 activa el
  blending alfa.
- **Las dimensiones se derivan del LOD y del aspecto** (LOD → lado mayor,
  aspecto → división): el tamaño del bloque de datos coincide con el previsto
  en **15 251 de 15 251 registros** de los 37 `.VFX`.
- **El índice de 14 bits del `.DFX` indexa esta tabla directamente**; en
  AZTEC16 los 9 609 polígonos usan los índices 0..3 sobre 260 entradas.
- **Implementado** en `gex/vfx.h` + `src/asset/vfx.c` + `src/asset/vfx_decode.c`
  (portable) con `port/tests/test_vfx.c`: **3 010 comprobaciones, 0 fallos**
  (`-O0`, `-O2` y ASan/UBSan).
- **El decodificador de píxel existe** (Módulo 4B): el formato de píxel
  (`YIQ_422`, `ARGB_1555`, `ARGB_4444`), las dimensiones (LOD y aspecto de
  Glide 2.x) y la tabla NCC (`GuNccTable`) están **demostrados** y verificados
  contra los 37 `.VFX` originales.

## 18. Módulo 4C: las texturas del `.VFX` en D3D11 (2026-11-11)

### 18.1 Qué se ha implementado

| Pieza | Dónde | Naturaleza |
|---|---|---|
| Banco de texturas decodificadas (`.VFX` → RGBA8 por índice de 14 bits) | `gex/vfx_texture.h`, `src/asset/vfx_texture.c` | **portable** |
| Atlas D3D11 + `ID3D11ShaderResourceView` + sampler + tabla de búsqueda | `gex/render/d3d11_texture.h`, `src/render/d3d11_texture.c` | **Windows-only** |
| Muestreo de textura en el pixel shader (UV + índice por polígono + color por vértice) | `src/render/d3d11_pipeline.c` | Windows-only |
| Carga del `.VFX` del nivel en la aplicación | `src/app/i4_main_win32.c` | Windows-only |

La ruta del cubo de prueba **no cambia**: sin `--level` no se carga ninguna
textura y el shader no tiene nada que muestrear.

### 18.2 La coordenada de textura: de dónde sale

- El byte de UV del `.DFX` (8 bits por vértice) llega al vértice como
  `float u, v` normalizados **dividiendo entre 256** (antes era 255: con 255
  el último vértice caía exactamente en 1.0 y el sampler con direccionamiento
  WRAP lo devolvía al primer texel, con una costura en el borde de cada
  polígono).
- El **original calcula la coordenada como `byte / 16`** (demons trado en el
  desensamblado, `0x0046619D`), con un divisor **constante**: el espacio de
  coordenadas no depende del tamaño de la textura.
- La lectura elegida por defecto es la de **texel**: el byte es la coordenada en
  texels y la textura **se repite** cada `w × h` texels. El argumento es que
  así la **densidad de texels es la misma para texturas de distinta
  resolución** (una textura de 64×64 y otra de 256×256 se reparten el mismo
  número de texels por unidad de mundo), que es lo que necesita un nivel; la
  lectura "normalizada" (el byte = fracción de la textura) haría que la
  densidad dependiera de la resolución de cada textura.
- El modo es un parámetro (`GEX_NCC_UV_MODE_TEXEL` / `..._NORMALIZED`) que se
  aplica en el shader, así que se puede cambiar sin tocar la geometría. Las
  dos lecturas producen imágenes distintas (el 69 % de los píxeles con
  geometría cambia), y se han capturado las dos en ejecución real (§18.5).
- **Límite conocido:** el factor de zoom exacto (cuántos texels por byte de UV)
  **no se ha podido cerrar** con la evidencia estática: depende de la escala
  de la cámara real del juego, ya validada dinámicamente (S-2). El
  período de repetición (cada `w × h` texels) sí es el mismo en las dos
  lecturas.

### 18.3 Estrategia de recursos: por qué un atlas y no una texture array

El índice de textura es **por polígono** y el nivel usa varias texturas de
**dimensiones distintas**, así que hace falta un recurso único del que el
shader elija (varias llamadas de dibujo romperían el orden del painter, que es
como dibuja el port y como el original).

- **Descartado: varias llamadas de dibujo.** El port dibuja en el orden de
  emisión (algoritmo del painter, sin depth buffer); reordenar las primitivas
  por textura cambiaría el resultado visual.
- **Descartado en la práctica: texture array.** Es la opción más directa (una
  rebanada por textura), pero en el equipo de desarrollo **la creación de un
  array 2D con datos iniciales falla con `E_INVALIDARG`** tanto en hardware
  como en WARP, y llega a colgar el driver. Es un límite del entorno, no del
  diseño, pero hace la opción insegura aquí.
- **Elegido: un atlas.** Una sola textura 2D con todas las texturas del nivel
  dentro, cada una con **un texel de borde replicado** para que el filtrado
  bilineal no mezcle una textura con la vecina. El shader calcula la posición
  del texel dentro de la textura y la traduce a la del atlas.

Consecuencias: las imágenes originales se conservan **intactas y con sus
dimensiones** (están en `gex_vfx_texture`); el recurso del atlas es una
empaquetación; y el índice de 14 bits del `.VFX` no es el número de textura
del atlas, sino una entrada de una **tabla constante** (índice → tamaño y
posición) que el shader recorre.

### 18.4 Modos de filtrado y direccionamiento (los del original)

El ejecutable **no importa** `grTexFilterMode`, `grTexClampMode` ni
`grTexMipMapMode`: usa los **valores por defecto de Glide 2.x**, que son

- filtrado **lineal** (bilineal) en minificación y magnificación,
- direccionamiento **WRAP** (la textura se repite),
- **mipmapping desactivado** (solo el nivel `largeLod`), lo que encaja con que
  los 15 251 registros medidos tengan `smallLod == largeLod`.

El port usa un sampler lineal con WRAP y **no genera mipmaps** ✓.

### 18.5 Composición en el shader

- El **color por vértice** se conserva (sigue siendo `AARRG8B8` leído como
  `R8G8B8A8` y reordenado en el vertex shader) y se **modula** con el texel:
  `color = textura * color` en RGB y `alfa = alfa_textura * alfa_vértice`.
- La **niebla** sigue viajando dentro del color por vértice (así lo emite el
  módulo 2). **Diferencia con el original:** el original aplica la niebla
  **después** de multiplicar por la textura (la niebla de Glide actúa en el
  combinador de color), mientras que aquí se aplica antes. En las zonas con
  poca niebla la diferencia es despreciable; con niebla fuerte, el color de la
  niebla queda multiplicado por el texel. Queda como corrección pendiente
  (habría que transportar el factor de niebla por vértice).
- El **índice de textura** viaja como atributo `nointerpolation uint`: los
  tres vértices de un triángulo llevan el mismo valor (viene del polígono) y el
  rasterizador no lo interpola ✓.
- El **blending alfa** se activa con `SRC_ALPHA` / `ONE_MINUS_SRC_ALPHA`, que
  ya no es una **hipótesis** sino la lectura **demostrada** de
  `grAlphaCombine(3,4,1,0,0)` en el driver real (§19.8). Con alfa 255 (el cubo
  de prueba y las texturas `YIQ_422`) el resultado es idéntico al de no
  mezclar, y en el modo de cámara las texturas opacas devuelven alfa 1 y
  substituyen el destino. Queda pendiente el matiz del alfa de vértice
  (§19.11) y el modo de mezcla por comando (§19.10).

### 18.6 Pruebas

- `port/tests/test_vfx.c`: **3 010 comprobaciones, 0 fallos** (`-O0`, `-O2` y
  ASan/UBSan). Incluye el banco de texturas (carga, decodificación, índices
  válidos e inválidos, entradas que no se pueden decodificar) y el contraste
  con los originales.
- `port/tests/test_d3d11_pipeline.c`: **324 comprobaciones, 0 fallos**
  (WARP). Incluye la creación y destrucción del atlas, los argumentos
  inválidos, el estado "ya inicializado" y una **prueba de muestreo**: un
  `.VFX` sintético de 64×64 en `ARGB_4444` con un degradado conocido, un
  triángulo con la UV en el byte 32 y la comprobación de que el píxel
  resultante es exactamente el del texel 8 (R = 136, G = 136, B = 255).
- Suite completa: **16/16 sin regresiones**.

### 18.7 Ejecución real (GPU hardware, ventana Win32)

`gexloop.exe --level C:\GEX_REZ\E2_A_ORIGINAL\LEVEL\AZTEC16.DFX` sobre el
dispositivo **hardware** (960×540, 120 presents, 0 errores de frame):

```
textures: 2 decodificadas de '...\AZTEC16.vfx' (atlas 132 x 34), 0 fallidas
geometry: level '...\AZTEC16.DFX'
  emitted: triangles=2854 (fog passes=1427) discarded by depth=0
  geometry: vertices=4914 indices=8562
final: t=1.49s presents=120 fps=80.5 ... exit: clean, frame errors=0
```

La **captura de la ventana** (`PrintWindow`) confirma que la geometría sale
**texturizada**: 3 101 colores distintos en la imagen (antes de este módulo el
nivel se veía como una maqueta de color plano) y el contenido de la textura se
reconoce en el panel con marco de la pared.

### 18.8 Lo que queda pendiente

1. **El contexto de emisión real del juego (S-2)**: hasta que la emisión original no esté
   validada, el encuadre es el **diagnóstico** del port y no se puede
   comparar con el original.
2. **El factor de zoom de la UV** (texels por byte) depende de esa cámara.
3. **La niebla** debería aplicarse después de la textura (§18.5).
4. El blending alfa se aplica a **todo** el dibujo; el original lo activa
   **por polígono** según el formato de la textura.
5. El atlas tiene un límite de 64 texturas y 4 096 px de ancho por fila
   (`GEX_D3D11_TEXTURE_MAX_ENTRIES`, `..._ATLAS_MAX_WIDTH`): con más
   texturas habría que revisar el empaquetado.

## 19. Materiales de MAP7: estado por polígono (2026-11-11)

La emisión ya reproduce el original triángulo a triángulo (§ 18.8). Este
apartado documenta el **material** de cada polígono, que es lo que decide el
aspecto: la textura, el color, la mezcla y la niebla.

### 19.1 El estado de material por polígono, en el original

El comando de 0x34 B que escribe `0x426256` lleva por polígono:

| Offset | Contenido |
|---|---|
| `+0x08` | color0 (AARRGGBB) |
| `+0x0C` | xy0 empaquetado `(y<<16)\|x` |
| `+0x10` | uv0: `u8 u`, `u8 v`, `u16` (indice de textura) |
| `+0x14` | color1 |
| `+0x18` | xy1 |
| `+0x1C` | uv1 |
| `+0x20` | color2 |
| `+0x24` | xy2 |
| `+0x28` | uv2 |
| `+0x2C/+0x2E/+0x30` | `z0`, `z1`, `z2` |
| `+0x0B` | opcode `0xB4 \| ((color0>>24)&3)` |

**El byte de alfa de `color0` es el opcode**: el opcode se escribe en `+0x0B`,
que es exactamente el byte de alfa de `color0`, y lo hace **después** del
payload. Por eso el color del comando del original siempre muestra `0xB4` en
esa posición aunque el color del vértice tuviera otro alfa.

### 19.2 El índice de textura: el segundo dword del registro UV

`0x46558A` toma el índice de textura de `word[cmd+0x1E] & 0x3FFF`, es decir
de la **mitad alta del segundo dword** del registro UV de 12 B (offset `+6`),
no del primero (`+2`).

> **Defecto corregido en el port.** Se leía el primer dword (`+2`), de modo
> que el port seleccionaba **texturas equivocadas**: con `map7.vfx` cargaba 4
> texturas en lugar de 62 y muestrejaba otra cosa. Corregido y verificado:
> los 1123 índices de textura del port coinciden ahora uno a uno con los del
> original (ver `port/tests/ref_material.h`).

### 19.3 El modo de mezcla, por polígono (no global)

`0x46558A` decide el modo de mezcla antes de cada comando, a partir del
**registro de textura en tiempo de ejecución** (`[0x6488F4][índice]`, campo
`+0x0C` = el formato con el que el juego tiene la textura):

| Formato en tiempo de ejecución | `grAlphaCombine` | `grAlphaBlendFunction` | Efecto |
|---|---|---|---|
| `0x0C` (ARGB_4444: trae alfa) | `(3, 4, 1, 0, 0)` | `(1, 5, 4, 0)` | **MEZCLA** con alfa: `src·a + dst·(1-a)` |
| `0x00` (opaco de 8 bits) | `(1, 0, 0, 2, 0)` | `(1, 5, 4, 0)` | **opaco**: con alfa de salida 1 substituye el destino |
| otros (ni alfa ni opaco) | no llama | no llama | el estado anterior persiste |

Evidencia dinámica (MAP7, frame 0, 1123 comandos de nivel):

| `grAlphaCombine` | llamadas | texturas |
|---|---|---|
| `(3, 4, 1, 0, 0)` | 32 | 6, 8, 185, 188, 234, 241 |
| `(1, 0, 0, 2, 0)` | 1091 | las demás |
| `(1, 0, 1, 2, 0)` | 128 | las primitivas 2D (opcode `0x28`), fuera del nivel |

Las **seis** texturas que el original mezcla con alfa son exactamente las de
**formato ARGB_4444** del `map7.vfx`: el juego convierte el YIQ_422 y el
ARGB_1555 a un formato opaco de 8 bits y solo conserva el canal alfa del
ARGB_4444.

**Implementación en el port.** El estado de mezcla viaja con la textura, como
en el original: la tabla de texturas del shader lleva un flag por entrada
("trae alfa") y el *pixel shader* devuelve alfa 1 para las texturas opacas.
Con el estado de mezcla global `SRC_ALPHA/INV_SRC_ALPHA` eso equivale a
substituir el destino (`src*1 + dst*0`), de modo que **el orden de dibujo no
cambia** y no hace falta un *depth buffer* ni agrupar por modo.

La aritmética está **confirmada contra el driver real** (§19.8): para el caso
ARGB_4444 el original usa `alfa de salida = alfa del texel × alfa del vértice`
y el estado `SRC_ALPHA`/`INV_SRC_ALPHA` con alfa `ONE`/`ZERO`, que es
exactamente lo que hace el *pixel shader* (`color.a * t.a`) y el estado global
del port.

### 19.4 La niebla, antes que la textura

En la ruta de nivel (`0x4269C2`) el original mezcla la niebla en el **color
del vértice** (antes de la textura): `0x46B727` mezcla cada byte del color
base hacia el color de niebla con el factor `LUT[z>>2]`, y ese color ya
mezclado es el que va al comando. El port hace lo mismo ✓.

El color destino de la mezcla son `[0x6468A0]` (B), `[0x647574]` (G) y
`[0x647560]` (R); el original los actualiza por objeto durante la emisión.
Para MAP7 en el frame 0 el valor general es `B=0x14, G=0x14, R=0x5A`.

> Con esos valores el port reproduce **los 1123 colores por vértice** del
> original (los tres canales RGB, triángulo a triángulo).

La ruta lejana (`0x426588`, que MAP7 no usa porque `[0x65BF04] = 0xFFFF`) sí
hace la niebla **después** de la textura, con un segundo comando de 0x24 B.

### 19.5 La niebla por objeto: hipótesis descartada (2026-11-11)

El módulo anterior dejó abierta la idea de que el color de niebla cambiaba por
polígono u objeto. **Es incorrecta**, y la auditoría lo demuestra capturando
el color destino y el factor en el punto exacto de la mezcla (`0x46B727`, con
breakpoint en esa dirección) para las 3476 mezclas del frame 0 de MAP7:

| Color destino (B,G,R) | mezclas | con factor ≠ 0 |
|---|---|---|
| `(0x14, 0x14, 0x5A)` | 749 | 722 |
| `(0, 0, 0)` | 2728 | **0** |

- El color destino **no cambia** durante la emisión del nivel (un solo cambio
  en 3476 mezclas consecutive), salvo el paso desde el estado anterior.
- Las mezclas con color negro tienen **factor 0**: el color no influye en el
  resultado (queda el color base del vértice).
- Por tanto la variación de color entre polígonos la produce el **factor** de
  niebla (que depende de la profundidad mediante la LUT), no un tinte por
  objeto.

Consecuencia: el port, que usa el valor general del frame
`(B=0x14, G=0x14, R=0x5A)`, reproduce los **1123 colores por vértice de
forma exacta**. La regresión `test_material_reference()` exige ahora la
coincidencia exacta (`same_rgb == 1123`) y no una coincidencia parcial.

**Lo que sí es cierto** es que el juego *mecanismo* actualiza el color por
objeto: `0x43DE14` lo toma de los bytes `+0x48/+0x49/+0x4A` de una estructura
y `0x43DE4D` de los bytes `+0x40/+0x41/+0x42` de la misma (después de la
llamada a `0x471E5F`). En MAP7 todos los objetos del nivel tienen el mismo
valor, así que no se nota; un nivel con niebla distinta por sector sí
divergiría. La procedencia de esa estructura en el `.DFX` sigue sin
identificarse.

### 19.6 La tabla NCC y la iluminación

**La tabla NCC no es una paleta ni una fuente de iluminación**: es la tabla
de descompresión del formato **YIQ_422** (112 B: `yRGB[16]`, `iRGB[4][3]`,
`qRGB[4][3]`, `packed_data[12]`), y el juego la sube a la TMU con
`grTexDownloadTable(tmu=0, type=0, registro+0x14)` **solo** para las texturas
de formato 1 (`0x466136`). Comprobado en ejecución: en el punto de la llamada
los argumentos son `tmu=0`, `type=0` (= `GR_TEXTABLE_NCC0`) y el puntero a la
tabla de cada textura.

Es decir: en Glide la expansión YIQ→RGB la hace el hardware con esa tabla; el
port la reproduce **en la CPU** al decodificar el `.VFX` y sube el resultado
al atlas. La equivalencia está verificada contra la referencia del SDK
(`docs/VFX_TEXTURE_FORMAT.md` § 6: el `packed_data` del fichero coincide byte
a byte con el que calcula el SDK, y los decodificadores del port son los del
SDK). Lo que **no** se ha verificado es una comparación **a nivel de píxel**
con la salida del hardware original, que exigiría capturar el framebuffer del
juego.

**La iluminación no interviene en la ruta de emisión de nivel.** El camino de
nivel (`0x4269C2` → `0x426256`) solo aplica la niebla al color del vértice;
la routine que modula por la normal y la luz (`0x46AA14`, que usa
`0x646860`/`0x646880`) es la de **modelos** (`0x46AA14` en `S2_BSP_TRAVERSAL.md`
§ 5.7) y no se ha implementado en el port (el port todavía no dibuja modelos).

Las diferencias que el port debe reproducir de forma explícita son, por
tanto, solo tres: la expansión YIQ_422 con la tabla NCC (hecha en CPU), el
formato de píxel por textura (ARGB_4444 con alfa de 4 bits, ARGB_1555 con un
bit de alfa, YIQ_422 sin alfa) y el modo de mezcla por polígono.

### 19.7 Qué queda abierto

- El color de niebla por objeto: el mecanismo existe (`0x43DE14`/`0x43DE4D`
  lo actualizan durante la emisión) pero en MAP7 no produce diferencias
  (§ 19.5); la procedencia de la estructura en el `.DFX` no está identificada.
- La equivalencia **a nivel de píxel** de la expansión YIQ_422 del port con la
  del hardware original: verificada contra la referencia del SDK, no contra el
  framebuffer del juego.
- La semántica exacta de los argumentos de `grAlphaCombine`: el port usa la
  mezcla alfa habitual (`SRC_ALPHA`/`INV_SRC_ALPHA`), que es la lectura
  habitual de `grAlphaCombine(3,4,1,0,0)`; los encabezados del SDK Glide 2.x
  no están en el repo, así que la aritmética exacta de la llamada queda sin
  verificar (afecta a 32 de los 1123 triángulos del frame 0).
- El emisor lejano y su segunda pasada de niebla: implementados en el port
  pero sin validación dinámica (MAP7 no los usa).
- La tabla NCC y los modos de filtrado: el port usa el atlas con filtrado
  bilineal y direccionamiento *wrap*, como el original.


### 19.8 `grAlphaCombine`: la ecuación, cerrada sobre el driver real (2026-11-12)

**El driver que se usa de verdad no es el de 3dfx.** La DLL de Glide del
entorno es **nGlide 2.10** (Zeus Software): una implementación *software* de
Glide 2.x sobre D3D9. No es el driver del SST-1, y eso condiciona lo que se
puede afirmar (ver «Lo que sigue sin poder afirmarse»).

| Evidencia | Dónde |
|---|---|
| Cadena `Software\Zeus Software\nGlide2` | VA `0x1013CA58` de `glide2x_gex_real.dll` |
| `glide2x.dll` (empaquetado, 243 472 B) reenvía sus exportaciones a `glide2x_gex_real.dll` | cadenas `glide2x_gex_real._grAlphaCombine@20` (`0xB5F3`), `glide2x_gex_real._grAlphaBlendFunction@16` (`0xB5B0`), … |
| El driver genera shaders D3D9 (`ps_2_0`) con un mini-ensamblador propio | `ps_2_0` (`0x1013CBB0`), `dcl t0.xy` (`0x1013CB90`), `texld r0, t0, s0` (`0x1013CB78`) |

**Método: el programa se obtiene dinámicamente, no leído a mano.** Un *harness*
i686 (`i686-w64-mingw32-gcc`) carga la DLL, llama a `grGlideInit` y después a
`grAlphaCombine(...)`, y vuelca el buffer de programa (RVA `0x159250`). El
contenido es texto plano en el mini-lenguaje del driver, así que el barrido de
los cinco parámetros es **exhaustivo y no interpretativo**: no hay que deducir
qué plantilla corresponde a qué parámetro, el driver la elige y se lee.

(Las llamadas capturadas en ejecución y sus recuentos —32 con alfa, 1091
opacas, 128 de primitivas 2D— están en § 19.3; aquí lo que se cierra es la
**aritmética** que esas llamadas producen.)

**El programa para `(3,4,1,0,0)`** (el caso ARGB_4444):

```text
mov r5, c0
mov r6, v0
mov r7, r4.a
mul r8, r7, r6
mov r9.a, r8
```

Los registros del driver (deducidos de las plantillas del propio binario):

| Registro | Qué es |
|---|---|
| `r0` / `r1` | texel de `t0` / `t1` (`texld r0, t0, s0`) |
| `r4` | **texel efectivo** (el de la unidad activa: `mov r4, r1` / `mov r4, r0`) |
| `v0` | **color de vértice interpolado** (`dcl v0`) |
| `v1` | factor de niebla (`dcl v1`; `lrp r10.rgb, v1.a, r9, c7`) |
| `c0` | color constante de `grConstantColorValue4` |
| `c5` / `c6` | `0.0` / `1.0` (`def c5, 0.0…`, `def c6, 1.0…`) |
| `r9` | color calculado (RGB y alfa) |
| `r10` | color tras la niebla; `mov oC0, r10` es la salida |

**Ecuación del caso ARGB_4444:**

```text
r9.a = r4.a × v0.a          (alfa del texel × alfa del color de vértice)
```

Los otros programas que el juego usa:

| Llamada | Programa | Resultado |
|---|---|---|
| `(3,4,1,0,0)` | `mov r5,c0 / mov r6,v0 / mov r7,r4.a / mul r8,r7,r6 / mov r9.a,r8` | `r9.a = texel.a × v0.a` |
| `(3,4,1,2,0)` | `mov r5,c0 / mov r6,c0 / mov r7,r4.a / mul r8,r7,r6 / mov r9.a,r8` | `r9.a = texel.a × c0.a` (segunda ruta texturizada) |
| `(1,0,0,2,0)` | `mov r5,v0 / mov r6,c0 / mov r7,c5 / mov r8,r5 / mov r9.a,r8` | `r9.a = v0.a` (opaco) |
| `(1,0,1,2,0)` | `mov r5,c0 / mov r6,c0 / mov r7,c5 / mov r8,r5 / mov r9.a,r8` | `r9.a = c0.a` = 1.0 (el juego pone el alfa del color constante a `0x437F0000`) |

**`grAlphaBlendFunction` también está descifrado, y es la otra mitad de la
ecuación.** El driver traduce los cuatro factores a estados D3D9:

| Argumento | Estado | Valor |
|---|---|---|
| `srcRGB` | `D3DRS_SRCBLEND` | `19` (`0x13`) |
| `dstRGB` | `D3DRS_DESTBLEND` | `20` (`0x14`) |
| `srcA` | `D3DRS_SRCBLENDALPHA` | `207` (`0xCF`) |
| `dstA` | `D3DRS_DESTBLENDALPHA` | `208` (`0xD0`) |
| — | `D3DRS_ALPHABLENDENABLE` | `27` (`0x1B`) |

… con esta tabla de factores (constantes `D3DBLEND` verificadas en
`d3d9types.h`):

| Factor | RGB (`SRCBLEND`, `DESTBLENDALPHA`) | Alfa (`DESTBLEND`, `SRCBLENDALPHA`) |
|---|---|---|
| `0` | `ZERO` (1) | `ZERO` (1) |
| `1` | `SRC_ALPHA` (5) | `SRC_ALPHA` (5) |
| `2` | `DEST_COLOR` (9) | `SRC_COLOR` (3) |
| `3` | `DEST_ALPHA` (7) | `DEST_ALPHA` (7) |
| `4` | `ONE` (2) | `ONE` (2) |
| `5` | `INV_SRC_ALPHA` (6) | `INV_SRC_ALPHA` (6) |
| `6` | `INV_DEST_COLOR` (10) | `INV_SRC_COLOR` (4) |
| `7` | `INV_DEST_ALPHA` (8) | `INV_DEST_ALPHA` (8) |
| `15` | `SRC_ALPHA_SAT` (11) | `SRC_ALPHA_SAT` (11) |
| otros | `ZERO` (1) | `ZERO` (1) |

La tabla **no** procede de los encabezados del SDK (no existen en el entorno):
es el comportamiento del driver real, y es **coherente con las nueve llamadas
del juego**, que de otro modo no tendrían sentido gráfico.

**Las llamadas reales del juego** (desensamblado de
`C:\GEX_REZ\SMOKE_H4A\GEX3D.EXE`, función `0x46558A`):

| `grAlphaBlendFunction(src,dst,srcA,dstA)` | Dónde | Efecto con la tabla |
|---|---|---|
| `(4,0,0,0)` | `0x4657BC` | `ONE/ZERO` → **substituye** el destino |
| `(4,4,0,0)` | `0x465831`, `0x465881` | `ONE/ONE` → **aditivo** |
| `(0,0,6,0)` | `0x465859`, `0x4659CA` | borra el RGB del destino |
| `(1,1,4,0)` | `0x4658A3`, `0x4659F2` | `(src+dst)·a` |
| `(1,4,4,0)` | `0x465A14` | `src·a + dst` |
| **`(1,5,4,0)`** | `0x465A3D` | **`src·a + dst·(1-a)`** |

Y el color: `guColorCombineFunction` con `2`, `3`, `6` y `7` (11 llamadas:
`0x465E33`, `0x466346`, `0x466B1D`, `0x4673AF`, `0x467876`, `0x467E3E`,
`0x4684CE`, `0x46873A`, `0x468AEB`, `0x4691B8`, `0x4696DC`). El modo `6` es el
que modula por el color de vértice: `mov r5, v0 / mov r6, r4 / mov r3, r6 /
mov r7, r5 / mul r8, r7, r6 / mov r9.rgb, r8` → `r9.rgb = v0.rgb × texel.rgb`.

**Cómo elige el juego el caso ARGB_4444** (`0x46558A`): el byte de comando
(`[ebp-0x34]`) se descompone en banderas; con la bandera `0x10` el juego
consulta el registro de textura (`[0x6488F4][índice & 0x3FFF]`) y compara su
campo `+0x0C` (el **formato**) con `0x0C`; si es ARGB_4444 marca
`[ebp-0x14] = 1` y elige `grAlphaCombine(3,4,1,0,0)` (`0x465696`) y, en la
misma pasada, `grAlphaBlendFunction(1,5,4,0)` (`0x465A3D`: camino
`[ebp-8] != 0` → `0x4658B7` → `[ebp-0x28] == 0` → `0x465A25`).

**La ecuación completa del caso auditado** (con `a = texel.a × v0.a`):

```text
rgb            = v0.rgb × texel.rgb
alfa de salida = texel.a × v0.a
framebuffer    = src.rgb·a + dst.rgb·(1-a)      (canal alfa: src·a)
```

**Contraste con el port: coincide. No se cambia el renderer.**

| Elemento | Original (nGlide 2.10) | Port |
|---|---|---|
| Mezcla del alfa | `r9.a = texel.a × v0.a` | `color = float4(color.rgb·t.rgb, color.a·t.a)` (`port/src/render/d3d11_pipeline.c:160`) ✔ |
| Estado de mezcla | `SRC_ALPHA`/`INV_SRC_ALPHA`, alfa `ONE`/`ZERO` | `D3D11_BLEND_SRC_ALPHA`/`INV_SRC_ALPHA`, `ONE`/`ZERO` (`d3d11_pipeline.c:558-561`) ✔ |
| Caso opaco | `r9.a = 1.0` con `ONE/ZERO` → substituye | alfa 1 con el mismo estado → substituye ✔ |
| RGB | `v0.rgb × texel.rgb` (modo `6`) | `color.rgb × t.rgb` ✔ |

La lectura `SRC_ALPHA`/`ONE_MINUS_SRC_ALPHA` de `(3,4,1,0,0)` queda
**confirmada con evidencia**, y los **32 triángulos** ARGB_4444 **no necesitan
ningún cambio**.

**Lo que sigue sin poder afirmarse.**

- La semántica **original del SDK 3dfx**: los encabezados de `glide.h` no están
  en el repo ni en el entorno. Lo demostrado es la semántica **de nGlide 2.10**,
  el driver que el proyecto usa de verdad, más la coherencia interna de su
  tabla con las nueve llamadas del juego.
- La equivalencia **píxel a píxel** con el Voodoo original: la captura de
  ventana del `glide2x` original devuelve negro, así que no hay framebuffer con
  el que comparar; la comparación es a nivel de **ecuación**.
- **Qué es exactamente `v0.a`.** **Resuelto (2026-11-12, § 19.11):** el
  vértice que el juego entrega a Glide **no tiene canal alfa** (el color
  viaja en tres flotantes B, G, R); el byte alto del color del comando es el
  **opcode**, no un alfa. El alfa de la unidad de mezcla es una constante
  1.0, y el port ya no multiplica por ese byte.
- **Los modos de mezcla por comando.** **Medidos (2026-11-12, § 19.10):** MAP7
  usa solo tres modos — `(1,5,4,0)` para la geometría del nivel, `(1,4,4,0)`
  para **6** triángulos (la textura 370) y `(4,0,0,0)` para las 128 primitivas
  2D. Los modos aditivos y de borrado **no aparecen**. El estado global del
  port coincide en todo salvo esos 6 triángulos; el diseño de la corrección
  queda cerrado en § 19.10.

### 19.9 Auditoría de YIQ_422

**Qué hace el port.** `gex_vfx_decode_yiq422()`
(`port/src/asset/vfx_decode.c`) decodifica **un byte por texel**:

```text
index = byte
iy = (index >> 4) & 0x0F   ->  luminancia   yRGB[iy]
ia = (index >> 2) & 0x03   ->  croma I      iRGB[ia]
ib =  index        & 0x03  ->  croma Q      qRGB[ib]
R = saturar(y + iRGB[ia][0] + qRGB[ib][0])   (y G B igual)
alfa = 255 (el formato no lleva alfa)
```

**Contraste con la referencia.** La referencia es el decodificador software del
propio SDK de Glide 2.x (`swlibs/texus/lib/dequant.c`,
`_txImgDequantizeYIQ422`, que indexa una paleta de 256 construida con
`txYABtoPal256`) y el constructor de la red neuronal que genera esas tablas
(`nccnnet.c`: `iy = (i>>4)&0x0f; ia = (i>>2)&0x03; ib = i&0x03;` y
`color = Y[iy] + A[ia] + B[ib]`), documentado en
[`VFX_TEXTURE_FORMAT.md`](VFX_TEXTURE_FORMAT.md) § 4.4.

| Aspecto | Port | Referencia del SDK | Estado |
|---|---|---|---|
| Disposición de bits | `iy/ia/ib` como arriba | idéntica | ✔ coincide |
| Coeficientes | `Y + A + B` (suma directa, escala 1:1) | idéntica | ✔ coincide |
| Croma | 9 bits con signo (complemento a dos) | idéntico | ✔ coincide |
| Saturación | `clamp` a 0..255 | idéntica | ✔ coincide |
| Alfa | 255 (el formato no lleva alfa) | idéntica | ✔ coincide |
| Pares de píxeles | **no hay**: un byte por texel | el "422" del nombre es el nombre del SDK para el formato YAB de 8 bits con tabla NCC; los datos del `.VFX` son de un byte por texel (la lectura de 16 bits deja la imagen inservible) | ✔ coherente |
| Rondeo | entero, sin redondeo | entero | ✔ coincide |

**Construcción de la tabla NCC.** `gex_vfx_ncc_table_pack()` reproduce el
empaquetado del SDK: los 16 niveles de luminancia en cuatro palabras y los
vectores de croma I y Q en tres campos de 9 bits por palabra. El test
`test_ncc_table()` del port **recalcula el empaquetado de las 7 723 tablas** de
los 37 `.VFX` originales y lo compara byte a byte: coinciden las doce palabras
de todas.

**Lo que NO se puede verificar con los datos disponibles.** La equivalencia
**a nivel de píxel** con la salida del hardware original: el juego sube la
tabla a la TMU (`grTexDownloadTable(tmu=0, type=0, registro+0x14)`, comprobado
en ejecución) y el Voodoo expande el YIQ en silicio, mientras que el port lo
expande en CPU. La comparación exigiría capturar el framebuffer del juego, que
no es accesible con las herramientas disponibles (la captura de ventana del
`glide2x` original devuelve negro salvo en el estado ya capturado). Por tanto
**no se afirma equivalencia píxel a píxel**: lo demostrado es la equivalencia
con la **referencia software del SDK**, más el contraste empírico de la
documentación (imagen coherente, saturación del 0,2 % con croma con signo
frente al 66 % sin signo).

### 19.10 Modos de mezcla por comando: qué usa MAP7 de verdad (2026-11-12)

El original **cambia el estado de mezcla según el comando**, no por polígono de
forma global. La selección ocurre en `0x46558A` y depende de:

- las banderas del byte de comando (`[ebp-0x34]`): `0x04` → camino de
  alphablend (`0x4658B7`), `0x02` → camino de «modo de iluminación»;
- el **modo de iluminación** en `[0x57B8EC]`, que el comando `0xE1` fija con
  los bits 5-6 de una palabra (`0x4655C2`) y que las rutas `0xB4`/`0x34`/
  `0x24`/`0x2C` reescriben con los dos bits altos del índice de textura
  (`0x4658C1`…`0x46593C`).

De ahí salen las seis cuádruplas de `grAlphaBlendFunction` de § 19.8: `ONE/ZERO`
(substituye), `ONE/ONE` (aditivo), `ZERO/ZERO` (borra), `(1,1,4,0)`,
`(1,4,4,0)` y `(1,5,4,0)` (mezcla alfa clásica).

**Captura dinámica (2026-11-12).** Con el trazador del repo
(`tools/frame-trace/tracer.cs`, breakpoints de hardware, **sin tocar un byte**
del ejecutable) sobre una **copia de trabajo** del juego (`md5` del exe
`692b12825003417cc4a5a9d4db13eebe`, idéntico al original) y dos thunks:

| Thunk | Dirección | Qué captura |
|---|---|---|
| `grAlphaBlendFunction@16` | `0x54052C` | la cuádrupla vigente |
| `grAlphaCombine@20` | `0x540532` | la llamada de mezcla alfa |

160 086 impactos en 45 s. El frame de nivel (1 261 llamadas) da:

| `grAlphaCombine` | Llamadas | `grAlphaBlendFunction` | Llamadas |
|---|---|---|---|
| `(1,0,0,2,0)` (opaco) | 1 090 | `(1,5,4,0)` | 1 116 |
| `(1,0,1,2,0)` (primitivas 2D) | 128 | `(1,4,4,0)` | **6** |
| `(3,4,1,0,0)` (ARGB_4444) | 32 | `(4,0,0,0)` | 127 |

**Conclusiones:**

1. Los modos **aditivo** `(4,4,0,0)` y **de borrado** `(0,0,6,0)` **no aparecen
   nunca** en MAP7: 0 ocurrencias en 160 086 llamadas. Las otras dos ramas de
   `grAlphaBlendFunction` (`0x465831`/`0x465881` y `0x465859`/`0x4659CA`) solo
   se alcanzan con el «modo de iluminación» 1, 2 o 3, y en MAP7 el modo es 0
   (los índices de textura son < 16384).
2. El `(4,0,0,0)` (substituye) se aplica a las **128 primitivas 2D** (opcode
   `0x28`), no a la geometría del nivel.
3. La geometría del nivel se dibuja con **`(1,5,4,0)`** (mezcla alfa clásica) y
   **6 triángulos** con **`(1,4,4,0)`** (`SRC_ALPHA`/`ONE`: el destino **no** se
   atenúa, se suma).

**Qué geometría es esa.** Los 6 comandos con `(1,4,4,0)` son los del opcode
`0xB6` (el «modo de textura 1»: `word[+0x1E] & 0x4000`) cuya textura **no** es
ARGB_4444. En la referencia del port (`port/tests/ref_material.h`, opcode
deducido como `0xB4 | ((color0 >> 24) & 3)`) son exactamente los **6
triángulos con la textura 370**; los otros 12 con opcode `0xB6` usan texturas
ARGB_4444 (185, 234, 241) y por tanto van a `(1,5,4,0)`. La correspondencia
cuadra uno a uno.

**Contraste con el port.** El port usa un **único** estado global
(`SRC_ALPHA`/`INV_SRC_ALPHA`, alfa `ONE`/`ZERO`):

| Grupo | Original | Port | ¿Coincide? |
|---|---|---|---|
| 1 116 comandos (nivel opaco + ARGB_4444) | `(1,5,4,0)` | `(1,5,4,0)` | ✔ (con alfa 1: `src·1 + dst·0 = src`) |
| 6 triángulos (textura 370) | `(1,4,4,0)` | `(1,5,4,0)` | ✘ (el original **suma** al destino) |
| 128 primitivas 2D | `(4,0,0,0)` | `(1,5,4,0)` | ✔ con alfa 1 |

**La diferencia de los 6 triángulos está demostrada, pero no se corrige en esta
ronda.** Reproducirla exige representar el modo **por triángulo**, y el modo
depende de dos cosas que hoy no viajan en la emisión: el opcode (bit 1 del byte
alto del color, que el port sí tiene) y el **formato** de la textura (solo el
ARGB_4444 cambia de modo), que vive en la tabla de texturas del renderer.
El cambio mínimo diseñado es:

1. en la emisión, marcar el triángulo (un bit más en `GEX_EMIT_F_*`, que ya
   viaja por vértice) cuando su opcode es `0xB6`;
2. en el renderer, resolver el formato por índice de textura (la tabla de
   texturas ya lo sabe) y **agrupar los índices en tramos contiguos** con el
   mismo modo, emitiendo un `DrawIndexed` por tramo con el estado de mezcla
   correspondiente (dos estados: `SRC_ALPHA`/`INV_SRC_ALPHA` y
   `SRC_ALPHA`/`ONE`);
3. **sin tocar** la cámara, la profundidad, el orden de dibujo (los tramos se
   emiten en el mismo orden) ni la niebla.

Afecta a **6 de 1 123 triángulos** (0,5 %): el cambio se difiere por su coste
relativo y porque el resto del modelo de mezcla ya está verificado. Se deja
como paso siguiente con el diseño cerrado.

### 19.11 El byte alto del color de vértice (2026-11-12)

Hay que distinguir **dos** cosas que se confunden:

| Dónde | Qué es el byte alto |
|---|---|
| **Flujo de comandos** (el registro de 0x34 B) | el **opcode**: `0xB4 | ((color0 >> 24) & 3)`. El emisor lo escribe **después** del color (`0x426530`, `0x4268EC`), así que el byte alto del color del comando **es** el opcode (§ 8 de `S2_BSP_TRAVERSAL.md`) |
| **Vértice que se entrega a Glide** | **no existe**: el color viaja en **tres** flotantes |

**El vértice real, capturado en ejecución.** El trazador volcó 6 000 vértices
en el punto de la llamada a `grDrawTriangle` (`0x54051A`, argumento 0). El
vértice son 0x40 B y su contenido es:

| Offset | Contenido | Valor observado (MAP7) |
|---|---|---|
| `+0x00` / `+0x04` | x, y en píxeles | 379.0, 16.0 … |
| `+0x08` | — | 0.0 en los 6 000 |
| `+0x0C` / `+0x10` / `+0x14` | **B, G, R** en el espacio 0..2 de Glide | 40.0, 40.0, 180.0 (= 2 × 20, 20, 90) |
| `+0x18` | — | 0.0 en los 6 000 |
| `+0x1C` | — | 255.0 en los 6 000 |
| `+0x20` | 1/z | 0.0001 … |
| `+0x24` / `+0x28` | u, v en unidades 1/16 | 0 … 15.9375 |

Los tres flotantes del color coinciden **exactamente** con los tres canales RGB
del color del port (0x…5A1414 → 2 × 20, 20, 90) ✓, y **no hay ningún canal
alfa**: los dos huecos son constantes (0.0 y 255.0) en las 6 000 muestras.

⟹ **El byte alto del color del comando (0x34/0xB4/0x36/0xB6) nunca llega a
la GPU**: es información del flujo de comandos (el opcode), no del material.

**Qué alfa usa entonces la unidad de mezcla.** En nGlide el alfa del vértice
(`v0.a` del programa generado, § 19.8) es una **constante 1.0**:

- el vértice no tiene alfa que propagar, y
- el juego **habilita el *alpha test*** para las texturas opacas
  (`grAlphaTest(1)`, `0x46578A`): con el valor por defecto (función
  «mayor que», referencia 0) un alfa 0 **descartaría toda la geometría** del
  nivel, que se ve; y
- con el estado `(1,5,4,0)` (`src·a + dst·(1-a)`) un alfa distinto de 1
  haría **translúcida** la escena entera.

**Consecuencia: el port multiplicaba mal el alfa.** El *pixel shader* hacía
`color.a = color.a * t.a`, es decir el alfa del texel **por el byte alto del
color por vértice** (0xB4 → 0,706; 0x34 → 0,204). Ese byte no es un alfa: es
el opcode. Corregido el 2026-11-12: el alfa de salida es **el alfa del texel**
(y 1.0 si la textura no trae canal alfa o si no hay textura), nunca el byte
alto del color por vértice (`port/src/render/d3d11_pipeline.c`).

El caso opaco no cambia (ya devolvía alfa 1, que es lo que hace el original) y
los **32 triángulos** ARGB_4444 ahora se mezclan con el alfa real de la
textura en lugar de attenuated por un byte sin sentido gráfico.

**Límites.** Todo lo anterior está medido sobre **nGlide 2.10**, el driver que
el proyecto usa de verdad; el comportamiento del **SST-1 de 3dfx** no es
observable aquí (y su formato de vértice tampoco: el juego entrega vértices con
flotantes, que no es el `GrVertex` clásico). La captura de ventana del juego
devuelve negro, así que no hay verificación píxel a píxel: la comparación es a
nivel de **ecuación y de estado de mezcla**.

