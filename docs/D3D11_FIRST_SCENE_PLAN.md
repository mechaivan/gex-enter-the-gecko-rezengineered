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
  - 314 checks, 0 fallos

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
     y ordenados. La validación dinámica contra el ejecutable original sigue
     pendiente.

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

### 4.2 Necesitan Validación

- **Shaders:** Necesarios para renderizar, pero no están en U-1 (fuera de alcance)
- **Formato de texturas NCC:** Conversiones a D3D11 (requieren investigación)
- **Clipping:** El algoritmo BSP original (`0x401C00`) no incluye clipping
  explícito; sí hace *culling* de esfera contra 6 semiplanos, ya documentado en
  [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)
- **Alpha blending:** Semántica no completamente documentada en U-1

---

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
    es la validación dinámica contra el ejecutable, así que S-2 sigue abierto.
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

#### Módulo 4: Cargador de Texturas (`gex_texture_loader.h/c`)
- **Responsabilidades:**
  - Cargar archivo .VFX
  - Construir tabla de texturas como en `0x006488F4`
  - Convertir formatos NCC a D3D11
- **Estructuras de datos:**
  ```c
  typedef struct {
      ID3D11Texture2D *texture;
      ID3D11ShaderResourceView *srv;
      D3D11_TEXTURE2D_DESC desc;
  } gex_texture;
  
  typedef struct {
      gex_texture *textures;
      uint32_t count;
  } gex_texture_table;
  ```
- **Interfaz:**
  ```c
  gex_texture_status gex_vfx_parse(const char *path, gex_texture_table *out);
  gex_texture_status gex_texture_table_build(
      ID3D11Device *device,
      const char *vfx_path,
      gex_texture_table *out);
  ```

#### Módulo 5: Integración con el renderer D3D11 — PARCIAL (5A implementado, 2026-11-11)

La interfaz original de esta sección (`gex_d3d11_scene.h/c` con
`gex_d3d11_scene_load` y `gex_d3d11_scene_render`) **no existe en el port** y
queda sustituida por la arquitectura real: el renderer R-3 (`gex_d3d11_renderer`)
consigue una geometría opcional y la dibuja antes de cada `Present`. No hay
un objeto "escena" porque todavía no hay materiales ni texturas.

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

**Pendiente del Módulo 5:** materiales NCC, texturas del Módulo 4, estado de
profundidad real y el `D3D11InputLayout` definitivo cuando exista la firma del
shader de producción.

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
de cámara + 37 de BSP + 38 809 de emisión, 0 fallos. La **emisión de polígonos**
también está implementada (`gex_level_emit`, §11) y auditada con 38 809
comprobaciones, pero le falta la validación dinámica contra el ejecutable
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
  - Dibujo de geometría sin texturas: **hecho** (color por vértice)
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
  **validación dinámica contra el ejecutable original**, por lo que el hito no
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
implementa en `gex_level_emit` (§11), con 38 809 comprobaciones. Sigue
pendiente la validación dinámica contra el ejecutable: sin ella no se puede
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
  Lo que falta para cerrar el hito: **texturas** (Hito 4), *depth buffer*,
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

### 9.2 Pendientes Expresos

1. **Shaders:** Necesarios pero fuera del alcance de U-1
2. **Formato NCC:** Requiere investigación adicional
3. **Clipping:** Implementación futura con viewport (el *culling* de esfera de
   `0x401C00` ya está implementado en
   [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md) y `port/src/camera/camera.c`)
4. **Alpha blending:** Semántica no completamente documentada

### 9.3 Evidencias Clave

- `U1_CONSOLIDACION.md`: Estado final de geometría BSP
- `U1H_RESULTADOS.md`: Estructuras de datos demostradas
- Código existente R-1 a R-4: Base para renderizado
- Datos de AZTEC16.DFX: Caso de prueba completo

---

## 10. Siguiente Hito Concreto

**Siguiente paso: el Módulo 4 (carga y conversión de texturas `.VFX`/NCC) y,
con él, el material definitivo: shader de producción, *depth buffer* y el
`ID3D11InputLayout` con los atributos que consuma la textura.**

- **Ya hecho:** parser DFX (Módulo 1), recorrido BSP + visibilidad + emisión
  de polígonos (Módulo 2, §11), el Módulo 3 completo (conversión portable §12 y
  vertex/index buffers D3D11 §13) y el **Módulo 5A**: shaders, input layout,
  estados y `DrawIndexed` (§14), con geometría de prueba visible en la
  ventana de la aplicación.
- **Lo que falta antes de ver una escena real:** las texturas del Módulo 4, el
  blending del original, el *depth buffer*, y la carga del nivel (`.DFX`) con
  la emisión real dentro del bucle de frames (hoy la geometría de la
  aplicación es el cubo de prueba, no un nivel).
- **Criterio de éxito del siguiente hito:** `gex_texture_load` construye la
  tabla de texturas desde `GEX.VFX`, resuelve índices de 14 bits y crea
  `ID3D11ShaderResourceView`; las pruebas se ejecutan con WARP sin abrir
  ventanas.
- **Sigue pendiente y no se declara cerrado:** la validación dinámica de S-2
  contra el ejecutable original. Mientras no exista, la emisión es
  «reconstruida y coherente», no «demostrada idéntica».

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

Validación actual (18.ª pasada): `port/tests/test_level_emit.c` con **38 809
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
- Que la emisión sea **idéntica** al ejecutable: falta la validación dinámica
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
  su normal con una luz fija (Lambert);
- profundidad por vértice, con las caras ordenadas de atrás hacia delante
  (algoritmo del pintor);
- giro alrededor de Y parametrizable e inclinación fija de 0,35 rad para que
  se vean tres caras a la vez;
- `flags = GEX_EMIT_F_NEAR_EMITTER`, sin niebla y sin texturas.

### 14.8 Pruebas

`port/tests/test_d3d11_pipeline.c` — **137 comprobaciones, 0 fallos** (Clang
`-O0` y `-O2`), **offscreen con WARP**: sin ventanas, sin swapchain y sin
demos interactivas. Cubre:

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
    renderer no inicializado).

`port/tests/test_geometry_shapes.c` — **355 comprobaciones, 0 fallos**
(portable, Clang `-O0`, `-O2` y ASan/UBSan): forma de la emisión, colores,
profundidades, orden del pintor, determinismo, efecto del giro y conversión a
`gex_geometry`.

### 14.9 Limitaciones y siguiente paso

- **No hay validación dinámica contra el ejecutable original**: S-2 sigue
  abierto y el primer escenario original **no** está terminado.
- **No hay texturas ni materiales**: el Módulo 4 sigue entero; el índice de
  textura viaja por vértice pero no se resuelve.
- **No hay depth buffer**: el orden de las caras lo resuelve el algoritmo del
  pintor, igual que el original. Con geometría que se solape de forma
  interpenetrada haría falta un *depth buffer* real.
- **La geometría de prueba es estática**: los buffers son inmutables, así que
  animarla exigiría recrearlos o usar buffers dinámicos.
- **El color se escribe sin blending**: el canal alfa del vértice se escribe en
  el render target pero no se usa para transparency; el blending del original
  está pendiente.
- Siguiente paso: **Módulo 4 (carga y conversión de texturas `.VFX`/NCC)** y,
  con él, la firma definitiva del shader de producción, el *depth buffer* y el
  `ID3D11InputLayout` con los atributos que consuma el material.