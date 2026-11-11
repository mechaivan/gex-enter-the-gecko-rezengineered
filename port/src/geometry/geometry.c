/*
 * geometry.c - Conversión portable de gex_emit_output a vértices e índices
 * (Módulo 3, fase portable). Sin Windows, sin D3D11, sin Glide.
 *
 * Véase gex/geometry.h para el contrato y docs/D3D11_FIRST_SCENE_PLAN.md §12
 * para las decisiones y los límites.
 */

#include "gex/geometry.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* El layout debe ser exactamente el documentado: el Módulo 5 construye el
 * D3D11_INPUT_ELEMENT_DESC con estos offsets y ningún relleno oculto. */
_Static_assert(sizeof(gex_vertex) == GEX_VERTEX_STRIDE,
               "gex_vertex debe medir 28 B sin relleno");
_Static_assert(offsetof(gex_vertex, color) == GEX_VERTEX_OFFSET_COLOR,
               "offset de color");
_Static_assert(offsetof(gex_vertex, u) == GEX_VERTEX_OFFSET_UV,
               "offset de UV");
_Static_assert(offsetof(gex_vertex, texture) == GEX_VERTEX_OFFSET_TEXTURE,
               "offset de texture");
_Static_assert(offsetof(gex_vertex, layer) == GEX_VERTEX_OFFSET_LAYER,
               "offset de layer");
_Static_assert(offsetof(gex_vertex, flags) == GEX_VERTEX_OFFSET_FLAGS,
               "offset de flags");

/* Inyección de fallo de reserva para las pruebas de errores (mismo patrón que
 * GEX_BSP_TEST_FAULT_INJECT). N=0 desactiva; N>0 hace fallar la N-ésima
 * reserva de una llamada a gex_geometry_build. */
#ifdef GEX_GEOM_TEST_FAULT_INJECT
int gex_geom_test_fail_alloc = 0;
static int geom_alloc_calls;

static int geom_fault(void)
{
    if (gex_geom_test_fail_alloc <= 0) {
        return 0;
    }
    geom_alloc_calls++;
    return geom_alloc_calls == gex_geom_test_fail_alloc;
}
#else
static int geom_fault(void)
{
    return 0;
}
#endif

static void *geom_alloc(size_t bytes)
{
    if (geom_fault()) {
        return NULL;
    }
    return malloc(bytes);
}

/* Hash FNV-1a sobre los 28 B del vértice (sin relleno, ver _Static_assert). */
static uint32_t vertex_hash(const gex_vertex *v)
{
    const uint8_t *p = (const uint8_t *)v;
    uint32_t h = 2166136261u;
    size_t i;

    for (i = 0; i < sizeof(*v); i++) {
        h ^= (uint32_t)p[i];
        h *= 16777619u;
    }
    return h;
}

static void vertex_from_triangle(gex_vertex *v, const gex_emit_triangle *tri,
                                 int corner)
{
    v->x = (float)tri->x[corner];
    v->y = (float)tri->y[corner];
    v->z = (float)tri->z[corner];
    v->color = tri->color[corner];
    /* Normalización exacta y reversible: round(u * 255) == byte original. */
    v->u = (float)tri->u[corner] / 255.0f;
    v->v = (float)tri->v[corner] / 255.0f;
    v->texture = tri->texture;
    v->layer = tri->layer;
    v->flags = tri->flags;
}

gex_geom_status gex_geometry_build(const gex_emit_output *emit,
                                   gex_geometry *out)
{
    gex_vertex *vertices;
    uint32_t *indices;
    uint32_t *table;
    size_t max_vertices;
    size_t table_size;
    size_t mask;
    size_t vcount = 0;
    size_t icount = 0;
    size_t skipped = 0;
    size_t t;

    if (out == NULL) {
        return GEX_GEOM_ERR_ARG;
    }
    /* Se deja a cero de entrada: en error nunca queda memoria a medias. */
    memset(out, 0, sizeof(*out));

    if (emit == NULL) {
        return GEX_GEOM_ERR_ARG;
    }
    if (emit->count > 0 && emit->tris == NULL) {
        return GEX_GEOM_ERR_ARG;
    }
    if (emit->count > emit->capacity) {
        return GEX_GEOM_ERR_RANGE;
    }
    if (emit->count == 0) {
        return GEX_GEOM_OK; /* geometría vacía válida */
    }

    /* Una esquina por vértice, como máximo tres por triángulo. */
    if (emit->count > SIZE_MAX / 3u) {
        return GEX_GEOM_ERR_OVERFLOW;
    }
    max_vertices = emit->count * 3u;
    /* Los índices son uint32: el mayor índice posible debe caber. */
    if (max_vertices > (size_t)UINT32_MAX) {
        return GEX_GEOM_ERR_OVERFLOW;
    }
    if (max_vertices > SIZE_MAX / sizeof(gex_vertex) ||
        max_vertices > SIZE_MAX / sizeof(uint32_t)) {
        return GEX_GEOM_ERR_OVERFLOW;
    }
    /* Tabla hash: potencia de dos con factor de carga <= 0.5. */
    if (max_vertices > SIZE_MAX / 2u) {
        return GEX_GEOM_ERR_OVERFLOW;
    }
    table_size = 1u;
    while (table_size < max_vertices * 2u) {
        if (table_size > SIZE_MAX / 2u) {
            return GEX_GEOM_ERR_OVERFLOW;
        }
        table_size *= 2u;
    }
    if (table_size > SIZE_MAX / sizeof(uint32_t)) {
        return GEX_GEOM_ERR_OVERFLOW;
    }
    mask = table_size - 1u;

#ifdef GEX_GEOM_TEST_FAULT_INJECT
    geom_alloc_calls = 0;
#endif

    vertices = (gex_vertex *)geom_alloc(max_vertices * sizeof(*vertices));
    if (vertices == NULL) {
        return GEX_GEOM_ERR_NOMEM;
    }
    indices = (uint32_t *)geom_alloc(max_vertices * sizeof(*indices));
    if (indices == NULL) {
        free(vertices);
        return GEX_GEOM_ERR_NOMEM;
    }
    table = (uint32_t *)geom_alloc(table_size * sizeof(*table));
    if (table == NULL) {
        free(indices);
        free(vertices);
        return GEX_GEOM_ERR_NOMEM;
    }
    memset(table, 0, table_size * sizeof(*table));

    for (t = 0; t < emit->count; t++) {
        const gex_emit_triangle *tri = &emit->tris[t];
        int corner;

        /* El contrato de la emisión obliga a saltar los clipeados por
         * profundidad: sus x/y valen 0 y dibujarlos sería geometría falsa. */
        if (tri->flags & GEX_EMIT_F_DEPTH_CLIPPED) {
            skipped++;
            continue;
        }

        for (corner = 0; corner < 3; corner++) {
            gex_vertex v;
            size_t slot;

            vertex_from_triangle(&v, tri, corner);
            slot = (size_t)vertex_hash(&v) & mask;
            for (;;) {
                uint32_t entry = table[slot];
                if (entry == 0) {
                    /* Vértice nuevo: se añade al final del vertex buffer. */
                    vertices[vcount] = v;
                    vcount++;
                    table[slot] = (uint32_t)vcount; /* guarda índice + 1 */
                    indices[icount] = (uint32_t)(vcount - 1u);
                    break;
                }
                if (memcmp(&vertices[entry - 1u], &v, sizeof(v)) == 0) {
                    indices[icount] = entry - 1u;
                    break;
                }
                slot = (slot + 1u) & mask;
            }
            icount++;
        }
    }

    free(table);

    if (vcount == 0) {
        /* Todos los triángulos estaban clipeados: geometría vacía válida. */
        free(indices);
        free(vertices);
        out->skipped_clipped = skipped;
        return GEX_GEOM_OK;
    }

    /* Ajuste opcional al tamaño real; si falla se conserva el bloque mayor. */
    if (vcount < max_vertices) {
        gex_vertex *shrunk =
            (gex_vertex *)realloc(vertices, vcount * sizeof(*shrunk));
        if (shrunk != NULL) {
            vertices = shrunk;
        }
    }
    if (icount < max_vertices) {
        uint32_t *shrunk = (uint32_t *)realloc(indices, icount * sizeof(*shrunk));
        if (shrunk != NULL) {
            indices = shrunk;
        }
    }

    out->vertices = vertices;
    out->indices = indices;
    out->vertex_count = vcount;
    out->index_count = icount;
    out->triangle_count = icount / 3u;
    out->skipped_clipped = skipped;
    return GEX_GEOM_OK;
}

void gex_geometry_free(gex_geometry *geom)
{
    if (geom == NULL) {
        return;
    }
    free(geom->vertices);
    free(geom->indices);
    memset(geom, 0, sizeof(*geom));
}

const char *gex_geom_status_name(gex_geom_status status)
{
    switch (status) {
    case GEX_GEOM_OK:
        return "ok";
    case GEX_GEOM_ERR_ARG:
        return "bad argument";
    case GEX_GEOM_ERR_RANGE:
        return "inconsistent counts";
    case GEX_GEOM_ERR_NOMEM:
        return "out of memory";
    case GEX_GEOM_ERR_OVERFLOW:
        return "size overflow";
    }
    return "unknown";
}
