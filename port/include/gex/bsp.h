/*
 * gex/bsp.h - BSP traversal for level geometry (S-2).
 */
#ifndef GEX_BSP_H
#define GEX_BSP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEX_BSP_NODE_SIZE    32u
#define GEX_BSP_LEAF_FLAG    0x0002u
#define GEX_BSP_LIST_CAP     0x800u

typedef enum gex_bsp_status {
    GEX_BSP_OK = 0,
    GEX_BSP_ERR_ARGUMENT,
    GEX_BSP_ERR_BOUNDS,
    GEX_BSP_ERR_CYCLE,
    GEX_BSP_ERR_MEMORY,
    GEX_BSP_ERR_OVERFLOW,
    GEX_BSP_ERR_LEAF_CAP
} gex_bsp_status;

typedef struct gex_bsp_walk_result {
    uint32_t *leaves;
    uint32_t count;
    uint32_t capacity;
} gex_bsp_walk_result;

typedef int (*gex_bsp_visible_fn)(const uint8_t *image, size_t image_size,
                                  uint32_t node_offset, void *ctx);

gex_bsp_status gex_bsp_walk(const uint8_t *image, size_t image_size,
                            uint32_t root_offset,
                            gex_bsp_visible_fn visible, void *ctx,
                            const void *cam_ctx,
                            gex_bsp_walk_result *out);

void gex_bsp_walk_free(gex_bsp_walk_result *r);

uint16_t gex_bsp_node_type(const uint8_t *image, size_t image_size,
                           uint32_t node_offset);
int      gex_bsp_node_is_leaf(const uint8_t *image, size_t image_size,
                              uint32_t node_offset);

#ifdef __cplusplus
}
#endif

#endif /* GEX_BSP_H */
