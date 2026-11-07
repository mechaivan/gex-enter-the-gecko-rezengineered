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
   - Relocalización de punteros internos
   - Extracción del descriptor (`image[0]`)

2. **Recorrido BSP**
   - Algoritmo de `0x401C00`: recorre nodos y anexa hojas visibles
   - Clipping por viewport (implementar después)
   - Generación de lista de polígonos visibles

3. **Generación de Buffers**
   - Conversión de vértices (12B → D3D11_INPUT_ELEMENT_DESC)
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
- **Clipping:** El algoritmo BSP original no incluye clipping explícito
- **Alpha blending:** Semántica no completamente documentada en U-1

---

## 5. Arquitectura Mínima por Módulos

### 5.1 Módulos Requeridos

#### Módulo 1: Parser DFX (`gex_asset_dfx.h/c`)
- **Responsabilidades:**
  - Lectura y validación del contenedor .DFX
  - Relocalización de punteros internos
  - Extracción del descriptor de nivel
- **Estructuras de datos:**
  ```c
  typedef struct {
      uint32_t count;
      uint32_t *offsets;
      uint8_t *image;
      size_t image_size;
  } gex_dfx_container;
  
  typedef struct {
      uint32_t root_bsp;
      uint32_t vertex_count;
      uint32_t polygon_count;
      uint32_t normal_count;
      float *vertices;
      uint8_t *polygons;
      int16_t *normals;
      // ... otros campos demostrados
  } gex_dfx_level;
  ```
- **Interfaz:**
  ```c
  gex_dfx_status gex_dfx_parse(const char *path, gex_dfx_container *out);
  gex_dfx_status gex_dfx_load_level(gex_dfx_container *dfx, gex_dfx_level *out);
  void gex_dfx_release(gex_dfx_container *dfx);
  ```

#### Módulo 2: Recorrido BSP (`gex_bsp_traversal.h/c`)
- **Responsabilidades:**
  - Implementar el algoritmo de `0x401C00`
  - Recorrer nodos y anadir hojas visibles
  - Generar lista de polígonos visibles
- **Estructuras de datos:**
  ```c
  typedef struct {
      float center[3];
      float radius;
      float normal[3];
      float distance;
      uint32_t child_front;
      uint32_t child_back;
      uint8_t type; // 1 = interno, 2 = hoja
  } gex_bsp_node;
  
  typedef struct {
      uint32_t indices[3];
      uint16_t flags;
      uint32_t uv_aux_offset;
      // ... otros campos del polígono
  } gex_bsp_polygon;
  
  typedef struct {
      gex_bsp_polygon *polygons;
      uint32_t count;
  } gex_visible_polygon_list;
  ```
- **Interfaz:**
  ```c
  gex_bsp_status gex_bsp_traverse(
      const gex_dfx_level *level,
      const float *viewpoint,
      gex_visible_polygon_list *out);
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
- **Objetivo:** Leer AZTEC16.DFX y extraer descriptor
- **Criterio de éxito:** 
  - `gex_dfx_parse` carga AZTEC16.DFX sin errores
  - Descriptor extraído con conteos correctos (3,882 vértices, 9,609 polígonos)
  - Tests unitarios: 95% cobertura

### Hito 2: Recorrido BSP Funcional
- **Objetivo:** Implementar algoritmo de `0x401C00`
- **Criterio de éxito:**
  - Recorrido completo del árbol BSP
  - Lista de polígonos visibles generada
  - Validación con nodos conocidos de AZTEC16

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
3. **Clipping:** Implementación futura con viewport
4. **Alpha blending:** Semántica no completamente documentada

### 9.3 Evidencias Clave

- `U1_CONSOLIDACION.md`: Estado final de geometría BSP
- `U1H_RESULTADOS.md`: Estructuras de datos demostradas
- Código existente R-1 a R-4: Base para renderizado
- Datos de AZTEC16.DFX: Caso de prueba completo

---

## 10. Siguiente Hito Concreto

**Siguiente paso: Implementar Módulo 1 (Parser DFX)**

- **Objetivo específico:** Implementar `gex_dfx_parse` y `gex_dfx_load_level`
- **Criterios de éxito:**
  - Lectura exitosa de AZTEC16.DFX
  - Extracción correcta del descriptor con conteos validados
  - Creación de estructuras de datos internas
- **Tiempo estimado:** 3-5 días
- **Pruebas unitarias incluidas:** Validación de estructura, conteos, punteros

Este hito es el fundamento para todas las etapas posteriores y permite validar la comprensión inicial del formato .DFX antes de avanzar a componentes más complejos.