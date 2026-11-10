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
     visibles (`0x004269C2`): pendiente

3. **Generación de Buffers**
   - Conversión de vértices (12B): los datos son vértices; el descriptor
     (D3D11_INPUT_ELEMENT_DESC) define cómo interpreta D3D11 esos datos
   - Generación de índices para triángulos
   - Creación de vertex/index buffers en D3D11

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
    La integración con la emisión de polígonos sigue pendiente.
  - Generar lista de polígonos visibles: **ruta reconstruida, no implementada**
    ([`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md) §5). La cadena original
    `0x43DC3A` → `0x43DED0` → `0x4269C2` → `0x426256`/`0x426588` → buckets →
    `0x469836` → `0x46558A` está documentada con evidencia estática, pero en
    `port/` no existe todavía ni lista de polígonos ni registro de comando.
  - Requisitos que el futuro generador debe cubrir (resumen de §5.12):
    transformación con `M = *[cam+0xE0]`, *culling* 2D por determinante,
    subdivisión a cuatro triángulos si `maxz < 0x6A4`, color por vértice con
    niebla, UV de 8 bits + word de textura de 14 bits, orden por
    `(z>>2) + 4*capa` y respeto de los flags `0x8000` (oculto) y `0x0004`
    (forzar sólido). La capacidad debe reservarse **por comando**, no por
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

#### Módulo 3: Generador de Buffers D3D11 (`gex_buffer_builder.h/c`)
- **Responsabilidades:**
  - Convertir vértices DFX a formato D3D11
  - Generar índices para triángulos
  - Crear vertex/index buffers
- **Estructuras de datos:**
  ```c
  typedef struct {
      ID3D11Buffer *vertex_buffer;
      ID3D11Buffer *index_buffer;
      uint32_t vertex_count;
      uint32_t index_count;
      D3D11_INPUT_ELEMENT_DESC *input_layout;
      uint32_t input_layout_count;
  } gex_mesh_buffers;
  ```
- **Interfaz:**
  ```c
  gex_buffer_status gex_buffers_build_from_polygons(
      ID3D11Device *device,
      const gex_visible_polygon_list *polygons,
      const float *vertices,
      const int16_t *normals,
      gex_mesh_buffers *out);
  ```

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

#### Módulo 5: Integración con Renderer D3D11 (`gex_d3d11_scene.h/c`)
- **Responsabilidades:**
  - Integrar módulos anteriores con renderer existente
  - Implementar shaders para renderizar geometría
  - Manejar materiales y texturas
- **Interfaz:**
  ```c
  typedef struct {
      gex_mesh_buffers mesh;
      gex_texture_table textures;
      // ... otros recursos
  } gex_d3d11_scene;
  
  gex_scene_status gex_d3d11_scene_load(
      ID3D11Device *device,
      const char *dfx_path,
      const char *vfx_path,
      gex_d3d11_scene *out);
  
  void gex_d3d11_scene_render(
      gex_d3d11_renderer *renderer,
      const gex_d3d11_scene *scene,
      const float *view_matrix,
      const float *proj_matrix);
  ```

### 5.2 Orden de Implementación

1. **Módulo 1: Parser DFX** - Base para todos los demás
2. **Módulo 2: Recorrido BSP** - Lógica de selección de geometría
3. **Módulo 3: Generador de Buffers** - Conexión con D3D11
4. **Módulo 4: Cargador de Texturas** - Recursos para renderizado
5. **Módulo 5: Integración** - Punto de entrada completo

**Estado real (2026-11-10):** el parser DFX (Módulo 1) está implementado en
`port/` ([`S1_DFX_PARSER.md`](../S1_DFX_PARSER.md)); el recorrido BSP
estructural y la visibilidad de cámara (Módulo 2) están implementados y
auditados ([`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md),
[`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)): 889 comprobaciones
de cámara + 37 de BSP, 0 fallos. La emisión de polígonos sigue pendiente, y con
ella la integración del culling en un pipeline real. Los Módulos 3 (generador
de buffers D3D11), 4 (cargador de texturas) y 5 (integración con renderer)
siguen pendientes. La investigación de la carga `.DFX` está cerrada
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

- **Generador de Buffers:**
  - Conversión correcta de vértices (12B → D3D11)
  - Generación de índices para triángulos
  - Creación exitosa de buffers en D3D11

- **Cargador de Texturas:**
  - Parseo correcto de .VFX
  - Creación de texturas D3D11
  - Resolución de índices de 14 bits

### 6.2 Pruebas de Integración

- **Carga completa de AZTEC16:**
  - Desde .DFX hasta buffers D3D11
  - Validación de conteos (vértices, polígonos, normales)
  - Verificación de integridad de datos

- **Renderizado básico:**
  - Dibujo de geometría sin texturas
  - Validación visual simple (siluetas)
  - Sin ejecución del juego, solo renderizado

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
- **Estado:** 🔄 PARCIAL (2026-11-10). Recorrido estructural y visibilidad de
  cámara implementados y auditados en `port/`
  ([`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md),
  [`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md)); la **ruta de
  emisión de polígonos está reconstruida con evidencia estática** (§5 del primer
  documento) pero **no implementada**, por lo que el hito no se declara
  completo.
- **Objetivo:** Implementar algoritmo de `0x401C00` para recorrer nodos y generar
  lista de polígonos visibles.
- **Criterio de éxito:**
  - Recorrido completo del árbol BSP (estructural: hecho)
  - Lista de polígonos visibles generada (pendiente: implementar emisión a
    partir de la especificación de §5)
  - Validación con nodos conocidos de AZTEC

**Estado 2026-11-10:** recorrido estructural implementado en `port/`
(`gex_bsp_walk`, `S2_BSP_TRAVERSAL.md`): AZTEC16 produce 815 hojas con predicado
NULL. La visibilidad original (cámara) está implementada en
[`S2_CAMERA_VISIBILITY.md`](../S2_CAMERA_VISIBILITY.md) — prueba de esfera
contra 6 semiplanos con matrices A/B y orden de apilado — y auditada contra el
desensamblado: 889 comprobaciones de cámara + 37 de BSP, 0 fallos, integradas
en `build.ps1 test` y `make test`. No hay emisión ni validación dinámica contra
el ejecutable; `port/` sigue usando el predicado inyectado cuando no se pasa
`cam_ctx`.

**Emisión (2026-11-10):** la ruta está descrita de extremo a extremo en
[`S2_BSP_TRAVERSAL.md`](../S2_BSP_TRAVERSAL.md) §5 (llamada, descriptores,
proyección a píxeles de 10 bits, subdivisión, niebla, emisores, buckets,
diferencias con modelos y con la primitiva 2D `0x4FF06F`). Es documentación, no
código: el Módulo 3 (generador de buffers) sigue sin implementar y debe usar
§5.12 como especificación de entrada.


### Hito 3: Buffers D3D11 Generados
- **Objetivo:** Convertir geometría a buffers D3D11
- **Criterio de éxito:**
  - Vertex buffer creado con 3,882 vértices
  - Index buffer creado con índices correctos
  - Tests de conversión: 100% precisión

### Hito 4: Texturas Cargadas
- **Objetivo:** Cargar .VFX y crear texturas D3D11
- **Criterio de éxito:**
  - Tabla de texturas construida desde GEX.VFX
  - Texturas D3D11 creadas exitosamente
  - Resolución correcta de índices de 14 bits

### Hito 5: Primer Renderizado Completo
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

**Siguiente paso: implementar la emisión de polígonos de S-2 (lista de hojas
visibles → polígonos) y, después, el Módulo 3 (Generador de Buffers D3D11).**

- **Prerrequisito real:** la visibilidad de cámara ya está implementada y
  auditada, pero la emisión de polígonos sigue pendiente; sin ella no existe la
  lista de polígonos visibles que alimenta al Módulo 3. También falta la
  validación dinámica contra el ejecutable original.
- **Objetivo específico del Módulo 3:** implementar
  `gex_buffers_build_from_polygons` para convertir los vértices DFX (12 B) en
  datos de vértice e índice y crear los buffers D3D11 correspondientes.
- **Criterios de éxito:**
  - Vertex buffer creado con 3 882 vértices de AZTEC16
  - Index buffer creado con índices correctos a partir de polígonos visibles
  - Tests de conversión: 100 % precisión
- **Prerrequisitos:** Parser DFX (implementado), Recorrido BSP estructural
  (implementado) y visibilidad de cámara implementada
  ([`S2_CAMERA_VISIBILITY.md` §6](../S2_CAMERA_VISIBILITY.md)); la emisión de
  polígonos sigue sin implementarse.
- **Pruebas unitarias incluidas:** Validación de estructura de buffers, conteos de vértices/índices.

Este hito conecta la geometría BSP con D3D11 y es el fundamento para los Módulos 4
(cargador de texturas) y 5 (integración con renderer).

---

## 11. Contrato de `gex_level_emit` (2026-11-10)

Implementado en `port/src/emit/emit.c` (cabecera `port/include/gex/emit.h`).
Deriva de la reconstrucción estática de
[`S2_BSP_TRAVERSAL.md` §5](../S2_BSP_TRAVERSAL.md) y de
[`S2_CAMERA_VISIBILITY.md` §13](../S2_CAMERA_VISIBILITY.md).

### 11.1 Principios

1. **Matriz de emisión propia.** `gex_level_emit` **no** acepta `gex_camera` ni
   usa `gex_camera.m` (`+0xE8`, visibilidad). Recibe un contexto de emisión
   independiente con la matriz de `cam+0xE0` (`gex_emit_matrix`).
2. **Separación de fases.** Extracción de polígonos, transformación,
   clasificación, subdivisión y ordenación son funciones distintas con
   estructuras propias; ninguna fase llama a otra implícitamente.
3. **Capacidad segura.** El original reserva 0x34 B por polígono y puede
   desbordar al subdividir en cuatro. El módulo reserva por triángulo emitido y
   devuelve `GEX_EMIT_ERR_OVERFLOW` si no puede crecer.
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
    int16_t  x[3], y[3];   /* píxeles enteros (sin recortar a 10 bits) */
    int32_t  z[3];         /* profundidad de emisión */
    uint8_t  u[3], v[3];   /* UV 8 bits */
    uint16_t texture;      /* índice de textura, 14 bits */
    uint8_t  layer;        /* byte[uv+0x0B]: desplazamiento de bucket */
    uint8_t  flags;        /* GEX_EMIT_F_* */
    uint32_t source_poly;  /* índice de polígono de origen (trazabilidad) */
    uint32_t seq;          /* secuencia de inserción (desempate LIFO en sort) */
} gex_emit_triangle;
```

Banderas: `GEX_EMIT_F_CLIPPED_XY`, `GEX_EMIT_F_SUBDIVIDED`,
`GEX_EMIT_F_FAR_EMITTER`, `GEX_EMIT_F_NEAR_EMITTER`, `GEX_EMIT_F_FOG_PASS`.

- `z` se conserva como `int32` sin recortar: el recorte a `0xFFFF` y el
  `[-0x400,0x3FF]` son decisiones del renderizador, no del extractor.
- `color` ya incluye la niebla cuando corresponde; la pasada de niebla de
  `0x426588` se emite como triángulo separado con `GEX_EMIT_F_FOG_PASS` y
  colores de niebla puros, igual que el original.
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
    gex_emit_status    status;
} gex_emit_output;

/* Fase 1: hojas visibles -> polígonos (sin transformar).
 * `image`/`image_size` son la imagen DFX cargada; los offsets de hoja y de
 * polígono son offsets dentro de ella, nunca punteros absolutos. */
gex_emit_status gex_emit_collect(const uint8_t *image, size_t image_size,
                                 const gex_dfx_descriptor *desc,
                                 const gex_bsp_walk_result *leaves,
                                 gex_emit_poly_list *out);

/* Fase 2: transformar y proyectar un polígono (0x46A1D0). */
gex_emit_status gex_emit_project(const gex_emit_matrix *mat,
                                 const int16_t vtx[9],   /* x0,y0,z0,x1,y1,z1,x2,y2,z2 */
                                 const uint32_t color[3],
                                 const uint8_t u[3], const uint8_t v[3],
                                 uint16_t texture, uint8_t layer,
                                 uint32_t source_poly,
                                 gex_emit_triangle *tri);

/* Fase 3: clasificar (determinante 2D + umbrales) y decidir emisor. */
gex_emit_status gex_emit_classify(const gex_emit_fog *fog,
                                  const gex_emit_triangle *p,
                                  uint16_t poly_flags,
                                  gex_emit_class *cls);

/* Fase 4: subdividir en cuatro (0x6A4), solo emisor cercano. */
gex_emit_status gex_emit_subdivide(const gex_emit_matrix *mat,
                                   const gex_emit_fog *fog,
                                   int fog_applies,
                                   const int16_t vtx[9],
                                   const uint32_t color[3],
                                   const uint8_t u[3], const uint8_t v[3],
                                   uint16_t texture, uint8_t layer,
                                   uint32_t source_poly,
                                   gex_emit_output *out);

/* Fase 5: ordenar por (promedio(z) >> 2) + 4 * layer, descendente;
 * desempate LIFO por secuencia de inserción. */
gex_emit_status gex_emit_sort(gex_emit_triangle *tris, size_t count);

/* Orquestador: collect -> project -> classify -> (subdivide) -> sort. */
gex_emit_status gex_level_emit(const uint8_t *image, size_t image_size,
                               const gex_dfx_descriptor *desc,
                               const gex_bsp_walk_result *leaves,
                               const gex_emit_matrix *mat,
                               const gex_emit_fog *fog,
                               gex_emit_output *out);

void gex_emit_output_free(gex_emit_output *out);
void gex_emit_poly_list_free(gex_emit_poly_list *list);
const char *gex_emit_status_name(gex_emit_status status);
```

### 11.6 Errores y garantías

- `GEX_EMIT_ERR_RANGE` cubre los offsets de hoja tratados como **offsets dentro
  de la imagen**, nunca como punteros absolutos.
- `GEX_EMIT_ERR_INCONSIST` cubre el descriptor DFX frente al descriptor del
  buffer de comandos: son estructuras distintas y el módulo solo acepta el
  primero.
- El *bucket array* no se modela: `gex_level_emit` produce una lista de
  triángulos ordenada; el equivalente al buffer secuencial de comandos es
  responsabilidad del renderizador.
- Toda reserva se comprueba; `GEX_EMIT_ERR_OVERFLOW` sustituye al desbordamiento
  silencioso del original.
- La geometría de modelo (`0x4013A0`, traslación `sar 4`) **no** entra en este
  módulo: tiene su propia ruta y su propio contrato.

### 11.7 Lo que este contrato no decide

- El valor real de `+0x50` (override) y de `+0x44..+0x46`.
- El generador de `byte[uv+0x0B]`; se transporta tal cual desde la imagen.
- La equivalencia bit a bit de las tablas trigonométricas.
- El modo de mezcla Glide del prefijo `0xE1`.