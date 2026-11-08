#include "gex/bsp.h"

#include <stdlib.h>
#include <string.h>

static int node_in_bounds(const uint8_t *image, size_t image_size,
                          uint32_t node_offset) {
    return (size_t)node_offset + GEX_BSP_NODE_SIZE <= image_size;
}

uint16_t gex_bsp_node_type(const uint8_t *image, size_t image_size,
                           uint32_t node_offset) {
    if (!node_in_bounds(image, image_size, node_offset)) return 0;
    return *(const uint16_t *)(image + node_offset + 0x12);
}

int gex_bsp_node_is_leaf(const uint8_t *image, size_t image_size,
                         uint32_t node_offset) {
    return (gex_bsp_node_type(image, image_size, node_offset) & GEX_BSP_LEAF_FLAG) != 0;
}

gex_bsp_status gex_bsp_walk(const uint8_t *image, size_t image_size,
                            uint32_t root_offset,
                            gex_bsp_visible_fn visible, void *ctx,
                            gex_bsp_walk_result *out) {
    if (image == NULL || out == NULL) {
        return GEX_BSP_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));

    if (!node_in_bounds(image, image_size, root_offset)) {
        return GEX_BSP_ERR_BOUNDS;
    }

    /* Pila de offsets de nodo. */
    uint32_t stack_cap = 1024;
    uint32_t *stack = malloc(stack_cap * sizeof(uint32_t));
    if (!stack) return GEX_BSP_ERR_MEMORY;
    uint32_t sp = 0;
    stack[sp++] = root_offset;

    /* Conjunto de visitados (lineal, suficiente para <= 0x800 hojas). */
    uint32_t visited_cap = 4096;
    uint32_t *visited = malloc(visited_cap * sizeof(uint32_t));
    if (!visited) { free(stack); return GEX_BSP_ERR_MEMORY; }
    uint32_t nvisited = 0;

    uint32_t leaves_cap = 256;
    uint32_t *leaves = malloc(leaves_cap * sizeof(uint32_t));
    if (!leaves) { free(stack); free(visited); return GEX_BSP_ERR_MEMORY; }
    uint32_t nleaves = 0;

    int cycle = 0;
    while (sp > 0) {
        uint32_t node = stack[--sp];

        /* visited check */
        int already = 0;
        for (uint32_t i = 0; i < nvisited; i++) {
            if (visited[i] == node) { already = 1; break; }
        }
        if (already) { cycle = 1; continue; }
        if (nvisited >= visited_cap) { free(stack); free(visited); free(leaves); return GEX_BSP_ERR_OVERFLOW; }
        visited[nvisited++] = node;

        if (!node_in_bounds(image, image_size, node)) {
            free(stack); free(visited); free(leaves);
            return GEX_BSP_ERR_BOUNDS;
        }

        /* visibility test */
        int vis = 1;
        if (visible) vis = visible(image, image_size, node, ctx);
        if (!vis) {
            /* no descend */
            continue;
        }

        uint16_t type = *(const uint16_t *)(image + node + 0x12);
        if (type & GEX_BSP_LEAF_FLAG) {
            if (nleaves >= GEX_BSP_LIST_CAP) {
                /* tope alcanzado: dejar de anexar pero seguir */
            } else {
                if (nleaves >= leaves_cap) {
                    leaves_cap *= 2;
                    uint32_t *nl = realloc(leaves, leaves_cap * sizeof(uint32_t));
                    if (!nl) { free(stack); free(visited); free(leaves); return GEX_BSP_ERR_MEMORY; }
                    leaves = nl;
                }
                leaves[nleaves++] = node;
            }
        } else {
            uint32_t c1 = *(const uint32_t *)(image + node + 0x18);
            uint32_t c2 = *(const uint32_t *)(image + node + 0x1C);
            if (c2 != 0 && c2 != c1) {
                if (sp >= stack_cap) {
                    stack_cap *= 2;
                    uint32_t *ns = realloc(stack, stack_cap * sizeof(uint32_t));
                    if (!ns) { free(stack); free(visited); free(leaves); return GEX_BSP_ERR_MEMORY; }
                    stack = ns;
                }
                stack[sp++] = c2;
            }
            if (c1 != 0) {
                if (sp >= stack_cap) {
                    stack_cap *= 2;
                    uint32_t *ns = realloc(stack, stack_cap * sizeof(uint32_t));
                    if (!ns) { free(stack); free(visited); free(leaves); return GEX_BSP_ERR_MEMORY; }
                    stack = ns;
                }
                stack[sp++] = c1;
            }
        }

        if (nvisited > 100000) { /* safety */
            cycle = 1;
            break;
        }
    }

    free(stack);
    free(visited);

    out->leaves = leaves;
    out->count = nleaves;
    out->capacity = leaves_cap;

    if (cycle) {
        /* no es error fatal; se documenta */
    }
    return GEX_BSP_OK;
}

void gex_bsp_walk_free(gex_bsp_walk_result *r) {
    if (!r) return;
    free(r->leaves);
    memset(r, 0, sizeof(*r));
}
