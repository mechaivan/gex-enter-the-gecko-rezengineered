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
    if (!out || !image) return 0;
    if (!node_in_bounds(image_size, node_offset)) return 0;
    if (field + 2 > GEX_BSP_NODE_SIZE) return 0;
    if ((size_t)node_offset + field + 2 > image_size) return 0;
    *out = READ_U16(image + node_offset + field);
    return 1;
}

static int read_u32(const uint8_t *image, size_t image_size,
                    uint32_t node_offset, uint32_t field, uint32_t *out) {
    if (!out || !image) return 0;
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

/* Safe growth of the visited arrays (offsets + colors) as a unit. */
static gex_bsp_status grow_visited(uint32_t **off, uint8_t **col, uint32_t *cap) {
    if (*cap > UINT32_MAX / 2) return GEX_BSP_ERR_OVERFLOW;
    uint32_t newcap = *cap * 2;
    if (newcap < *cap) return GEX_BSP_ERR_OVERFLOW;
    /* overflow in byte sizes */
    if ((size_t)newcap > SIZE_MAX / sizeof(uint32_t)) return GEX_BSP_ERR_OVERFLOW;
    if ((size_t)newcap > SIZE_MAX) return GEX_BSP_ERR_OVERFLOW;

    uint32_t *no = malloc((size_t)newcap * sizeof(uint32_t));
    uint8_t *nc = malloc((size_t)newcap);
    if (!no || !nc) {
        free(no); free(nc);
        return GEX_BSP_ERR_MEMORY;
    }
    memcpy(no, *off, (size_t)(*cap) * sizeof(uint32_t));
    memcpy(nc, *col, (size_t)(*cap));
    free(*off);
    free(*col);
    *off = no;
    *col = nc;
    *cap = newcap;
    return GEX_BSP_OK;
}

/* Safe growth of a uint32 array. */
static gex_bsp_status grow_u32(uint32_t **arr, uint32_t *cap) {
    if (*cap > UINT32_MAX / 2) return GEX_BSP_ERR_OVERFLOW;
    uint32_t newcap = *cap * 2;
    if (newcap < *cap) return GEX_BSP_ERR_OVERFLOW;
    if ((size_t)newcap > SIZE_MAX / sizeof(uint32_t)) return GEX_BSP_ERR_OVERFLOW;
    uint32_t *na = malloc((size_t)newcap * sizeof(uint32_t));
    if (!na) return GEX_BSP_ERR_MEMORY;
    memcpy(na, *arr, (size_t)(*cap) * sizeof(uint32_t));
    free(*arr);
    *arr = na;
    *cap = newcap;
    return GEX_BSP_OK;
}

typedef struct { uint32_t node; int is_exit; } frame_t;

static gex_bsp_status grow_frames(frame_t **fr, uint32_t *cap) {
    if (*cap > UINT32_MAX / 2) return GEX_BSP_ERR_OVERFLOW;
    uint32_t newcap = *cap * 2;
    if (newcap < *cap) return GEX_BSP_ERR_OVERFLOW;
    if ((size_t)newcap > SIZE_MAX / sizeof(frame_t)) return GEX_BSP_ERR_OVERFLOW;
    frame_t *nf = malloc((size_t)newcap * sizeof(frame_t));
    if (!nf) return GEX_BSP_ERR_MEMORY;
    memcpy(nf, *fr, (size_t)(*cap) * sizeof(frame_t));
    free(*fr);
    *fr = nf;
    *cap = newcap;
    return GEX_BSP_OK;
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

    uint32_t vis_cap = 64;
    uint32_t *vis_off = malloc(vis_cap * sizeof(uint32_t));
    uint8_t *vis_col = malloc(vis_cap);
    if (!vis_off || !vis_col) { free(vis_off); free(vis_col); return GEX_BSP_ERR_MEMORY; }
    uint32_t nvis = 0;

    uint32_t fr_cap = 64;
    frame_t *fr = malloc(fr_cap * sizeof(frame_t));
    if (!fr) { free(vis_off); free(vis_col); return GEX_BSP_ERR_MEMORY; }
    uint32_t fr_sp = 0;
    fr[fr_sp].node = root_offset;
    fr[fr_sp].is_exit = 0;
    fr_sp++;

    uint32_t leaves_cap = 64;
    uint32_t *leaves = malloc(leaves_cap * sizeof(uint32_t));
    if (!leaves) { free(fr); free(vis_off); free(vis_col); return GEX_BSP_ERR_MEMORY; }
    uint32_t nleaves = 0;

    gex_bsp_status result = GEX_BSP_OK;

    while (fr_sp > 0 && result == GEX_BSP_OK) {
        frame_t f = fr[--fr_sp];

        if (f.is_exit) {
            for (uint32_t i = 0; i < nvis; i++) {
                if (vis_off[i] == f.node) { vis_col[i] = 2; break; }
            }
            continue;
        }

        uint32_t node = f.node;

        /* find color */
        uint8_t color = 0;
        int found = 0;
        for (uint32_t i = 0; i < nvis; i++) {
            if (vis_off[i] == node) { color = vis_col[i]; found = 1; break; }
        }
        if (found && color == 2) {
            continue; /* shared reference, already processed */
        }
        if (found && color == 1) {
            result = GEX_BSP_ERR_CYCLE;
            break;
        }

        if (!node_in_bounds(image_size, node)) {
            result = GEX_BSP_ERR_BOUNDS;
            break;
        }

        int vis = 1;
        if (visible) vis = visible(image, image_size, node, ctx);
        if (!vis) {
            if (!found) {
                if (nvis >= vis_cap) {
                    gex_bsp_status g = grow_visited(&vis_off, &vis_col, &vis_cap);
                    if (g != GEX_BSP_OK) { result = g; break; }
                }
                vis_off[nvis] = node;
                vis_col[nvis] = 2;
                nvis++;
            } else {
                for (uint32_t i = 0; i < nvis; i++) if (vis_off[i] == node) vis_col[i] = 2;
            }
            continue;
        }

        /* mark in-progress */
        if (!found) {
            if (nvis >= vis_cap) {
                gex_bsp_status g = grow_visited(&vis_off, &vis_col, &vis_cap);
                if (g != GEX_BSP_OK) { result = g; break; }
            }
            vis_off[nvis] = node;
            vis_col[nvis] = 1;
            nvis++;
        } else {
            for (uint32_t i = 0; i < nvis; i++) if (vis_off[i] == node) vis_col[i] = 1;
        }

        uint16_t type = 0;
        if (!read_u16(image, image_size, node, 0x12, &type)) {
            result = GEX_BSP_ERR_BOUNDS;
            break;
        }

        if (type & GEX_BSP_LEAF_FLAG) {
            if (nleaves >= GEX_BSP_LIST_CAP) {
                result = GEX_BSP_ERR_LEAF_CAP;
                break;
            }
            if (nleaves >= leaves_cap) {
                gex_bsp_status g = grow_u32(&leaves, &leaves_cap);
                if (g != GEX_BSP_OK) { result = g; break; }
            }
            leaves[nleaves++] = node;
            /* mark done */
            for (uint32_t i = 0; i < nvis; i++) if (vis_off[i] == node) vis_col[i] = 2;
        } else {
            uint32_t c1 = 0, c2 = 0;
            int ok1 = read_u32(image, image_size, node, 0x18, &c1);
            int ok2 = read_u32(image, image_size, node, 0x1C, &c2);
            if (!ok1 || !ok2) { result = GEX_BSP_ERR_BOUNDS; break; }
            /* ensure frame capacity for exit + up to 2 children */
            while (fr_sp + 3 > fr_cap) {
                gex_bsp_status g = grow_frames(&fr, &fr_cap);
                if (g != GEX_BSP_OK) { result = g; break; }
            }
            if (result != GEX_BSP_OK) break;
            fr[fr_sp].node = node; fr[fr_sp].is_exit = 1; fr_sp++;
            if (c2 != 0) { fr[fr_sp].node = c2; fr[fr_sp].is_exit = 0; fr_sp++; }
            if (c1 != 0) { fr[fr_sp].node = c1; fr[fr_sp].is_exit = 0; fr_sp++; }
        }
    }

    free(fr);
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
