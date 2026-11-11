/*
 * gex/geometry.h - Conversión portable de la emisión de S-2 a geometría de
 * vértices e índices (Módulo 3, fase portable).
 *
 * Entrada: `gex_emit_output`, la lista de triángulos que produce
 * `gex_level_emit` (port/include/gex/emit.h). Salida: vértices e índices
 * listos para un vertex/index buffer, sin ninguna dependencia de Windows,
 * D3D11, Win32 ni Glide.
 *
 * Esta fase NO crea recursos gráficos: eso lo hace `gex/render/d3d11_mesh.h`
 * (Windows-only). La separación permite compilar y probar toda la conversión
 * en cualquier plataforma (docs/D3D11_FIRST_SCENE_PLAN.md §12).
 *
 * Decisiones del port (marcadas [C] en la documentación):
 *  - Un vértice por cada esquina de triángulo emitido, deduplicado por
 *    igualdad exacta de todos sus atributos. La deduplicación es determinista:
 *    el primer vértice visto fija el índice y el orden del vertex buffer, y el
 *    index buffer conserva el orden de emisión.
 *  - `z` se guarda como `float`. Los triángulos aceptados por la emisión tienen
 *    `z` en [near_z, 0xFFFF] (0x46A1D0 descarta el resto), muy por debajo de
 *    2^24, así que la conversión es exacta.
 *  - `u`/`v` se normalizan a 0..1 dividiendo entre 255: la conversión es
 *    exacta y reversible (`round(u * 255)` recupera el byte original).
 *  - Los triángulos con `GEX_EMIT_F_DEPTH_CLIPPED` se descartan aquí: el
 *    contrato de `gex_emit_project` obliga al llamador a saltarlos y sus x/y
 *    valen 0, así que dibujarlos sería geometría falsa. Se cuentan en
 *    `skipped_clipped`.
 */

#ifndef GEX_GEOMETRY_H
#define GEX_GEOMETRY_H

#include <stddef.h>
#include <stdint.h>

#include "gex/emit.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Vértice: 28 B, sin relleno (verificado con _Static_assert en geometry.c).
 * El orden de los campos es el orden del layout de entrada que el Módulo 5
 * traducirá a D3D11_INPUT_ELEMENT_DESC; este módulo no crea el layout porque
 * necesita la firma de un shader que todavía no existe.
 * ======================================================================== */
typedef struct {
    float    x, y, z;   /* píxeles proyectados + profundidad de emisión */
    uint32_t color;     /* AARRGGBB por vértice (niebla ya aplicada si procede) */
    float    u, v;      /* UV normalizadas 0..1 desde los bytes 0..255 */
    uint16_t texture;   /* índice de textura del registro UV (14 bits útiles) */
    int8_t   layer;     /* byte[uv+0x0B] con signo */
    uint8_t  flags;     /* GEX_EMIT_F_* del triángulo de origen */
} gex_vertex;

#define GEX_VERTEX_STRIDE          28u
#define GEX_VERTEX_OFFSET_POSITION  0u
#define GEX_VERTEX_OFFSET_COLOR    12u
#define GEX_VERTEX_OFFSET_UV       16u
#define GEX_VERTEX_OFFSET_TEXTURE  24u
#define GEX_VERTEX_OFFSET_LAYER    26u
#define GEX_VERTEX_OFFSET_FLAGS    27u

/* ========================================================================
 * Geometría convertida. `indices` siempre es uint32_t; el módulo D3D11 elige
 * el formato real del índice (R16_UINT cuando cabe, R32_UINT si no).
 * ======================================================================== */
typedef struct {
    gex_vertex *vertices;
    uint32_t   *indices;
    size_t      vertex_count;
    size_t      index_count;      /* siempre 3 * triangle_count */
    size_t      triangle_count;   /* triángulos aceptados (sin clipeados) */
    size_t      skipped_clipped;  /* descartados por GEX_EMIT_F_DEPTH_CLIPPED */
} gex_geometry;

typedef enum {
    GEX_GEOM_OK = 0,
    GEX_GEOM_ERR_ARG,       /* puntero nulo */
    GEX_GEOM_ERR_RANGE,     /* conteos incoherentes (count > capacity) */
    GEX_GEOM_ERR_NOMEM,     /* fallo de reserva */
    GEX_GEOM_ERR_OVERFLOW   /* tamaños no representables / sin espacio */
} gex_geom_status;

/*
 * Convierte una lista de triángulos emitidos en vértices e índices.
 *
 *   emit == NULL o out == NULL          -> ERR_ARG (out no se escribe)
 *   emit->count > 0 y tris == NULL      -> ERR_ARG
 *   emit->count > emit->capacity        -> ERR_RANGE
 *   count * 3 no representable          -> ERR_OVERFLOW
 *   sin memoria                         -> ERR_NOMEM
 *   count == 0                          -> OK con todo a cero y punteros NULL
 *
 * En éxito `out` es propiedad del llamador: libéralo con `gex_geometry_free`.
 * En error `out` queda a cero (nunca con memoria a medias).
 *
 * No consulta `emit->overflow`: una lista truncada sigue siendo coherente y
 * quien la construyó ya recibió GEX_EMIT_ERR_OVERFLOW de la emisión.
 */
gex_geom_status gex_geometry_build(const gex_emit_output *emit,
                                   gex_geometry *out);

/* Libera vértices e índices y deja la estructura a cero. Idempotente. */
void gex_geometry_free(gex_geometry *geom);

const char *gex_geom_status_name(gex_geom_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_GEOMETRY_H */
