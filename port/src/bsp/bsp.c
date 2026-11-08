#include "gex/bsp.h"

#include <stdlib.h>
#include <string.h>

#define READ_U16(p) ((uint16_t)((p)[0] | ((uint16_t)(p)[1] << 8)))
#define READ_U32(p) ((uint32_t)((p)[0] | ((uint32_t)(p)[1] << 8) | \
                   ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24)))

static int node_in_bounds(size_t image_size, uint32_t node_offset) {
    if (node_offset > image_size) return 0;
    return (size_t)node_offset + GEX_BSP_NODE_SIZE <= image_size;
}

static int read_u16(const uint8_t *image, size_t image_size,
                    uint32_t node_offset, uint32_t field, uint16_t *out) {
    if (!node_in_bounds(image_size, node_offset)) return 0;
    if (field + 2 > GEX_BSP_NODE_SIZE) return 0;
    if ((size_t)node_offset + field + 2 > image_size) return 0;
    *out = READ_U16(image + node_offset + field);
    return 1;
}

static int read_u32(const uint8_t *image, size_t image_size,
                    uint32_t node_offset, uint32_t field, uint32_t *out) {
    if (!node_in_bounds(image_size, node_offset)) return 0;
    if (field + 4 > GEX_BSP_NODE_SIZE) return 0;
    if ((size_t)node_offset + field + 4 > image_size) return 0;
    *out = READ_U32(image + node_offset + field);
    return 1;
}

uint16_t gex_bsp_node_type(const uint8_t *image, size_t image_size,
                           uint32_t node_offset) {
    uint16_t v = 0;
    if (!image) return 0;
    read_u16(image, image_size, node_offset, 0x12, &v);
    return v;
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

    if (!node_in_bounds(image_size, root_offset)) {
        return GEX_BSP_ERR_BOUNDS;
    }

    /* color: 0=unvisited,1=in-progress,2=done */
    /* We use a hash-free visited array with color. */
    size_t max_nodes = 65536;
    uint8_t *color = calloc(max_nodes, 1);
    /* map node_offset -> index? offsets can be large. Use a simple list of (offset,color). */
    /* Simpler: parallel arrays. */
    free(color);
    /* We'll use dynamic arrays of offsets + colors. */
    uint32_t cap = 1024;
    uint32_t *vis_off = malloc(cap * sizeof(uint32_t));
    uint8_t *vis_col = malloc(cap);
    if (!vis_off || !vis_col) { free(vis_off); free(vis_col); return GEX_BSP_ERR_MEMORY; }
    uint32_t nvis = 0;

    uint32_t *stack = malloc(cap * sizeof(uint32_t));
    if (!stack) { free(vis_off); free(vis_col); return GEX_BSP_ERR_MEMORY; }
    uint32_t sp = 0;
    stack[sp++] = root_offset;

    uint32_t leaves_cap = 256;
    uint32_t *leaves = malloc(leaves_cap * sizeof(uint32_t));
    if (!leaves) { free(stack); free(vis_off); free(vis_col); return GEX_BSP_ERR_MEMORY; }
    uint32_t nleaves = 0;

    gex_bsp_status result = GEX_BSP_OK;

    while (sp > 0) {
        uint32_t node = stack[--sp];

        /* find color */
        uint8_t c = 0;
        int found = 0;
        for (uint32_t i = 0; i < nvis; i++) {
            if (vis_off[i] == node) { c = vis_col[i]; found = 1; break; }
        }
        if (found && c == 2) {
            /* shared reference, already processed: skip */
            continue;
        }
        if (found && c == 1) {
            /* cycle */
            result = GEX_BSP_ERR_CYCLE;
            break;
        }
        /* mark in-progress */
        if (nvis >= cap) {
            cap *= 2;
            uint32_t *no = realloc(vis_off, cap * sizeof(uint32_t));
            uint8_t *nc = realloc(vis_col, cap);
            if (!no || !nc) { result = GEX_BSP_ERR_MEMORY; break; }
            vis_off = no; vis_col = nc;
        }
        vis_off[nvis] = node;
        vis_col[nvis] = 1;
        nvis++;

        if (!node_in_bounds(image_size, node)) {
            result = GEX_BSP_ERR_BOUNDS;
            break;
        }

        int vis = 1;
        if (visible) vis = visible(image, image_size, node, ctx);
        if (!vis) {
            /* mark done, no descend */
            for (uint32_t i = 0; i < nvis; i++) if (vis_off[i] == node) vis_col[i] = 2;
            continue;
        }

        uint16_t type = 0;
        if (!read_u16(image, image_size, node, 0x12, &type)) {
            result = GEX_BSP_ERR_BOUNDS; break;
        }

        if (type & GEX_BSP_LEAF_FLAG) {
            if (nleaves >= GEX_BSP_LIST_CAP) {
                result = GEX_BSP_ERR_LEAF_CAP;
                break;
            }
            if (nleaves >= leaves_cap) {
                leaves_cap *= 2;
                uint32_t *nl = realloc(leaves, leaves_cap * sizeof(uint32_t));
                if (!nl) { result = GEX_BSP_ERR_MEMORY; break; }
                leaves = nl;
            }
            leaves[nleaves++] = node;
        } else {
            uint32_t c1 = 0, c2 = 0;
            int ok1 = read_u32(image, image_size, node, 0x18, &c1);
            int ok2 = read_u32(image, image_size, node, 0x1C, &c2);
            if (!ok1 || !ok2) { result = GEX_BSP_ERR_BOUNDS; break; }
            /* push c2 then c1 (so c1 processed first) */
            if (c2 != 0) {
                if (sp >= cap) {
                    cap *= 2;
                    uint32_t *ns = realloc(stack, cap * sizeof(uint32_t));
                    if (!ns) { result = GEX_BSP_ERR_MEMORY; break; }
                    stack = ns;
                    /* also grow vis arrays to keep cap in sync */
                }
                stack[sp++] = c2;
            }
            if (c1 != 0) {
                if (sp >= cap) {
                    cap *= 2;
                    uint32_t *ns = realloc(stack, cap * sizeof(uint32_t));
                    if (!ns) { result = GEX_BSP_ERR_MEMORY; break; }
                    stack = ns;
                }
                stack[sp++] = c1;
            }
        }

        /* mark done */
        for (uint32_t i = 0; i < nvis; i++) if (vis_off[i] == node) vis_col[i] = 2;
    }

    free(stack);
    free(vis_off);
    free(vis_col);

    if (result != GEX_BSP_OK) {
        free(leaves);
        memset(out, 0, sizeof(*out));
        return result;
    }

    out->leaves = leaves;
    out->count = nleaves;
    out->capacity = leaves_cap;
    return GEX_BSP_OK;
}

void gex_bsp_walk_free(gex_bsp_walk_result *r) {
    if (!r) return;
    free(r->leaves);
    memset(r, 0, sizeof(*r));
}
