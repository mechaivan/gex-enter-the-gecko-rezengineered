#include "gex/bsp.h"
#include "gex/camera.h"

#include <stdlib.h>
#include <string.h>

#ifdef GEX_BSP_TEST_FAULT_INJECT
static int g_grow_visited_fail = 0;
void gex_bsp_set_grow_visited_fail(int which) { g_grow_visited_fail = which; }
void gex_bsp_reset_grow_visited_fail(void) { g_grow_visited_fail = 0; }
#endif

#define READ_U16(p) ((uint16_t)((p)[0] | ((uint16_t)(p)[1] << 8)))
#define READ_U32(p) ((uint32_t)((p)[0] | ((uint32_t)(p)[1] << 8) | \
                   ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24)))

static int node_in_bounds(size_t image_size, uint32_t node_offset) {
    if (node_offset > image_size) return 0;
    return GEX_BSP_NODE_SIZE <= (image_size - node_offset);
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
    if (!image) return 0;
    /* Original predicate: (type & 1) == 0  →  hoja.
     * GEX_BSP_LEAF_FLAG (& 2) coincides for observed types 1/2 but is not
     * the predicate the executable tests. */
    uint16_t t = gex_bsp_node_type(image, image_size, node_offset);
    return (t & 1u) == 0u;
}

/* Safe growth of visited arrays: copies only nvalid elements. */
static gex_bsp_status grow_visited(uint32_t **off, uint8_t **col, uint32_t *cap,
                                   uint32_t nvalid) {
#ifdef GEX_BSP_TEST_FAULT_INJECT
    if (g_grow_visited_fail == 1) return GEX_BSP_ERR_MEMORY;
#endif
    if (*cap > UINT32_MAX / 2) return GEX_BSP_ERR_OVERFLOW;
    uint32_t newcap = *cap * 2;
    if (newcap < *cap) return GEX_BSP_ERR_OVERFLOW;
    if ((size_t)newcap > SIZE_MAX / sizeof(uint32_t)) return GEX_BSP_ERR_OVERFLOW;
    if ((size_t)newcap > SIZE_MAX) return GEX_BSP_ERR_OVERFLOW;

    uint32_t *no = malloc((size_t)newcap * sizeof(uint32_t));
#ifdef GEX_BSP_TEST_FAULT_INJECT
    if (g_grow_visited_fail == 2) { free(no); return GEX_BSP_ERR_MEMORY; }
#endif
    uint8_t *nc = malloc((size_t)newcap);
    if (!no || !nc) {
        free(no); free(nc);
        return GEX_BSP_ERR_MEMORY;
    }
    memcpy(no, *off, (size_t)nvalid * sizeof(uint32_t));
    memcpy(nc, *col, (size_t)nvalid);
    free(*off);
    free(*col);
    *off = no;
    *col = nc;
    *cap = newcap;
    return GEX_BSP_OK;
}

static gex_bsp_status grow_u32(uint32_t **arr, uint32_t *cap, uint32_t nvalid) {
    if (*cap > UINT32_MAX / 2) return GEX_BSP_ERR_OVERFLOW;
    uint32_t newcap = *cap * 2;
    if (newcap < *cap) return GEX_BSP_ERR_OVERFLOW;
    if ((size_t)newcap > SIZE_MAX / sizeof(uint32_t)) return GEX_BSP_ERR_OVERFLOW;
    uint32_t *na = malloc((size_t)newcap * sizeof(uint32_t));
    if (!na) return GEX_BSP_ERR_MEMORY;
    memcpy(na, *arr, (size_t)nvalid * sizeof(uint32_t));
    free(*arr);
    *arr = na;
    *cap = newcap;
    return GEX_BSP_OK;
}

typedef struct { uint32_t node; int is_exit; } frame_t;

static gex_bsp_status grow_frames(frame_t **fr, uint32_t *cap, uint32_t nvalid) {
    if (*cap > UINT32_MAX / 2) return GEX_BSP_ERR_OVERFLOW;
    uint32_t newcap = *cap * 2;
    if (newcap < *cap) return GEX_BSP_ERR_OVERFLOW;
    if ((size_t)newcap > SIZE_MAX / sizeof(frame_t)) return GEX_BSP_ERR_OVERFLOW;
    frame_t *nf = malloc((size_t)newcap * sizeof(frame_t));
    if (!nf) return GEX_BSP_ERR_MEMORY;
    memcpy(nf, *fr, (size_t)nvalid * sizeof(frame_t));
    free(*fr);
    *fr = nf;
    *cap = newcap;
    return GEX_BSP_OK;
}

gex_bsp_status gex_bsp_walk(const uint8_t *image, size_t image_size,
                            uint32_t root_offset,
                            gex_bsp_visible_fn visible, void *ctx,
                            const void *cam_ctx,
                            gex_bsp_walk_result *out) {
    if (image == NULL || out == NULL) {
        return GEX_BSP_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));

    if (!node_in_bounds(image_size, root_offset)) {
        return GEX_BSP_ERR_BOUNDS;
    }

    const gex_camera *cam = (const gex_camera *)cam_ctx;

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

        uint8_t color = 0;
        int found = 0;
        for (uint32_t i = 0; i < nvis; i++) {
            if (vis_off[i] == node) { color = vis_col[i]; found = 1; break; }
        }
        if (found && color == 2) {
            continue;
        }
        if (found && color == 1) {
            result = GEX_BSP_ERR_CYCLE;
            break;
        }

        if (!node_in_bounds(image_size, node)) {
            result = GEX_BSP_ERR_BOUNDS;
            break;
        }

        /* Visibility: callback predicate AND (if camera given) sphere
         * vs 6 semiplanos test. Both must pass. */
        int vis = 1;
        if (visible) vis = visible(image, image_size, node, ctx);
        if (vis && cam != NULL) {
            /* Read node center (+0x00/0x02/0x04) and radius (+0x06). */
            int16_t center[3];
            uint16_t radius;
            uint16_t c0, c1, c2;
            if (!read_u16(image, image_size, node, GEX_BSP_CENTER_X, &c0) ||
                !read_u16(image, image_size, node, GEX_BSP_CENTER_Y, &c1) ||
                !read_u16(image, image_size, node, GEX_BSP_CENTER_Z, &c2) ||
                !read_u16(image, image_size, node, GEX_BSP_RADIUS, &radius)) {
                result = GEX_BSP_ERR_BOUNDS;
                break;
            }
            center[0] = (int16_t)c0;
            center[1] = (int16_t)c1;
            center[2] = (int16_t)c2;
            vis = gex_cam_sphere_visible(cam, center, radius);
        }
        if (!vis) {
            if (!found) {
                if (nvis >= vis_cap) {
                    gex_bsp_status g = grow_visited(&vis_off, &vis_col, &vis_cap, nvis);
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

        if (!found) {
            if (nvis >= vis_cap) {
                gex_bsp_status g = grow_visited(&vis_off, &vis_col, &vis_cap, nvis);
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

        if ((type & 1u) == 0u) {               /* hoja: predicado original */
            if (nleaves >= GEX_BSP_LIST_CAP) {
                result = GEX_BSP_ERR_LEAF_CAP;
                break;
            }
            if (nleaves >= leaves_cap) {
                gex_bsp_status g = grow_u32(&leaves, &leaves_cap, nleaves);
                if (g != GEX_BSP_OK) { result = g; break; }
            }
            leaves[nleaves++] = node;
            for (uint32_t i = 0; i < nvis; i++) if (vis_off[i] == node) vis_col[i] = 2;
        } else {
            uint32_t c1 = 0, c2 = 0;
            int ok1 = read_u32(image, image_size, node, 0x18, &c1);
            int ok2 = read_u32(image, image_size, node, 0x1C, &c2);
            if (!ok1 || !ok2) { result = GEX_BSP_ERR_BOUNDS; break; }
            while (fr_sp + 3 > fr_cap) {
                gex_bsp_status g = grow_frames(&fr, &fr_cap, fr_sp);
                if (g != GEX_BSP_OK) { result = g; break; }
            }
            if (result != GEX_BSP_OK) break;
            fr[fr_sp].node = node; fr[fr_sp].is_exit = 1; fr_sp++;

            if (cam != NULL) {
                /* Original child order: d = ((camPos · N) >> 12) − distance.
                 * Read normal (+0x0C/0x0E/0x10) and distance (+0x14). */
                int16_t normal[3];
                int32_t distance;
                uint16_t n0, n1, n2;
                if (!read_u16(image, image_size, node, GEX_BSP_NORMAL_X, &n0) ||
                    !read_u16(image, image_size, node, GEX_BSP_NORMAL_Y, &n1) ||
                    !read_u16(image, image_size, node, GEX_BSP_NORMAL_Z, &n2)) {
                    result = GEX_BSP_ERR_BOUNDS;
                    break;
                }
                normal[0] = (int16_t)n0;
                normal[1] = (int16_t)n1;
                normal[2] = (int16_t)n2;
                if (!read_u32(image, image_size, node, GEX_BSP_DISTANCE,
                              (uint32_t *)&distance)) {
                    result = GEX_BSP_ERR_BOUNDS;
                    break;
                }
                int32_t d = gex_cam_plane_distance(cam, normal, distance);
                if (d >= 0) {
                    /* push hi (0x1C) then lo (0x18) → visit lo first */
                    if (c2 != 0) { fr[fr_sp].node = c2; fr[fr_sp].is_exit = 0; fr_sp++; }
                    if (c1 != 0) { fr[fr_sp].node = c1; fr[fr_sp].is_exit = 0; fr_sp++; }
                } else {
                    /* push lo (0x18) then hi (0x1C) → visit hi first */
                    if (c1 != 0) { fr[fr_sp].node = c1; fr[fr_sp].is_exit = 0; fr_sp++; }
                    if (c2 != 0) { fr[fr_sp].node = c2; fr[fr_sp].is_exit = 0; fr_sp++; }
                }
            } else {
                /* Fixed order (backward compatible): push hi then lo. */
                if (c2 != 0) { fr[fr_sp].node = c2; fr[fr_sp].is_exit = 0; fr_sp++; }
                if (c1 != 0) { fr[fr_sp].node = c1; fr[fr_sp].is_exit = 0; fr_sp++; }
            }
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
