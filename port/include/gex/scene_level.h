/*
 * gex/scene_level.h - carga de un nivel real (.DFX) hasta geometría
 * (portable).
 *
 * Es el módulo 5B: encadena los módulos que ya existen, sin duplicar
 * ninguna implementación:
 *
 *   gex_dfx_load / gex_dfx_load_level   -> contenedor y descriptor
 *   gex_bsp_walk                        -> hojas visibles del BSP
 *   gex_level_emit                      -> triángulos emitidos
 *   gex_geometry_build                  -> vértices e índices
 *
 * La conversión a buffers D3D11 no vive aquí (es Windows-only y ya existe en
 * `gex/render/d3d11_mesh.h`): este módulo se queda en la CPU.
 *
 * Portable C11: sin Windows, sin D3D11, sin Win32.
 */

#ifndef GEX_SCENE_LEVEL_H
#define GEX_SCENE_LEVEL_H

#include <stddef.h>
#include <stdint.h>

#include "gex/asset/dfx.h"
#include "gex/bsp.h"
#include "gex/emit.h"
#include "gex/geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Estadísticas de la carga: lo que la aplicación imprime y las pruebas
 * comprueban. */
typedef struct gex_scene_stats {
    uint32_t container_entries;   /* entradas del contenedor .DFX */
    uint32_t descriptor_vertices;  /* vértices del descriptor del nivel */
    uint32_t descriptor_polygons;  /* polígonos del descriptor del nivel */
    uint32_t leaf_count;           /* hojas del BSP recorridas */
    size_t   triangles;            /* triángulos emitidos */
    size_t   fog_triangles;        /* de ellos, pasadas de niebla */
    size_t   vertices;             /* vértices de la geometría */
    size_t   indices;              /* índices de la geometría */
    size_t   skipped_clipped;      /* descartados por profundidad */
    int32_t  view_cx;              /* centro de pantalla de la vista usada */
    int32_t  view_cy;
    int16_t  view_scale;           /* escala Q12 de la vista (m[0] = m[4]) */
} gex_scene_stats;

typedef enum gex_scene_status {
    GEX_SCENE_OK = 0,
    GEX_SCENE_ERR_ARG,         /* ruta nula */
    GEX_SCENE_ERR_LOAD,        /* el .DFX no se pudo cargar */
    GEX_SCENE_ERR_LEVEL,       /* no se pudo obtener el descriptor */
    GEX_SCENE_ERR_BSP,         /* falló el recorrido del BSP */
    GEX_SCENE_ERR_EMIT,        /* falló la emisión de polígonos */
    GEX_SCENE_ERR_GEOMETRY,    /* falló la conversión a geometría */
    GEX_SCENE_ERR_NOMEM        /* falta memoria */
} gex_scene_status;

/*
 * Estado de una escena cargada. Cada bloque tiene su propia bandera para que
 * `gex_scene_level_free` libere exactamente lo que se llegó a crear, incluso
 * si la carga falló a mitad.
 */
typedef struct gex_scene_level {
    gex_dfx_container container;
    int container_loaded;
    gex_dfx_level level;
    int level_loaded;
    gex_bsp_walk_result leaves;
    int leaves_loaded;
    gex_emit_output emit;
    int emit_loaded;
    gex_geometry geometry;
    int geometry_loaded;
    gex_scene_stats stats;
    int loaded;
} gex_scene_level;

/*
 * Vista diagnóstica determinista para ver la geometría de un nivel.
 *
 * **NO es la cámara del juego.** Es una matriz de emisión sintética y
 * reproducible (escala 0,25 en los tres ejes y traslación de profundidad
 * +0x1000) más una niebla generada con el mismo generador 0x461895 que usa
 * la prueba de emisión. Sirve para producir una imagen estable y auditable
 * del nivel; no sustituye a la cámara original, cuya validación dinámica
 * contra el ejecutable sigue pendiente.
 *
 * La visibilidad del BSP es "todas las hojas": la prueba de esfera de la
 * cámara original (`cam_ctx`) necesita el estado de cámara del juego, que el
 * port todavía no tiene.
 */
void gex_scene_diagnostic_view(gex_emit_matrix *mat, gex_emit_fog *fog);

/*
 * Carga un nivel completo: `.DFX` -> descriptor -> BSP -> emisión ->
 * geometría. En éxito, `scene->geometry` es la geometría lista para
 * `gex_d3d11_mesh_create`.
 *
 * Ante un error devuelve el estado correspondiente y deja la escena
 * liberable con `gex_scene_level_free` (que es idempotente).
 */
gex_scene_status gex_scene_level_load(gex_scene_level *scene, const char *path);

/* Libera todo lo que la escena haya creado. Idempotente. */
void gex_scene_level_free(gex_scene_level *scene);

const char *gex_scene_status_name(gex_scene_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_SCENE_LEVEL_H */
