/*
 * gex/asset/dfx.h - reader for the ".DFX" container of GEX3D.EXE (EU).
 */
#ifndef GEX_ASSET_DFX_H
#define GEX_ASSET_DFX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEX_DFX_ALIGN_MASK 0x1FFu
#define GEX_DFX_ALIGN_ADD  0x200u

#define GEX_DESCRIPTOR_SIZE 64u
#define GEX_VERTEX_SIZE     12u
#define GEX_POLYGON_SIZE    20u
#define GEX_NORMAL_SIZE     6u

typedef enum gex_dfx_status {
    GEX_DFX_OK = 0,
    GEX_DFX_ERR_ARGUMENT,
    GEX_DFX_ERR_IO,
    GEX_DFX_ERR_FORMAT,
    GEX_DFX_ERR_TRUNCATED,
    GEX_DFX_ERR_ALIGNMENT,
    GEX_DFX_ERR_POINTER,
    GEX_DFX_ERR_MEMORY,
    GEX_DFX_ERR_DESCRIPTOR,
    GEX_DFX_ERR_OVERFLOW
} gex_dfx_status;

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
    uint32_t field_0x04;
    uint32_t field_0x08;
    uint32_t field_0x0C;
    uint32_t field_0x10;
    uint32_t field_0x14;
    uint32_t vertex_count;
    uint32_t polygon_count;
    uint32_t normal_count;
    uint32_t vertex_table;
    uint32_t polygon_table;
    uint32_t normal_table;
    uint32_t object_table;
    uint32_t field_0x34;
    uint32_t field_0x38;
    uint32_t field_0x3C;
} gex_dfx_descriptor;

typedef struct gex_dfx_level {
    gex_dfx_descriptor descriptor;
    uint8_t *vertex_data;
    uint8_t *polygon_data;
    int16_t *normal_data;
} gex_dfx_level;

gex_dfx_status gex_dfx_parse(const void *data, size_t size, gex_dfx_container *out);
gex_dfx_status gex_dfx_load(const char *path, gex_dfx_container *out);
gex_dfx_status gex_dfx_relocate(gex_dfx_container *container, uint32_t delta);
gex_dfx_status gex_dfx_relocate_raw(uint8_t *image, size_t image_size,
                                    const uint32_t *offsets, uint32_t count,
                                    uint32_t delta);
gex_dfx_status gex_dfx_load_level(const gex_dfx_container *container,
                                  gex_dfx_level *out);
void gex_dfx_release_container(gex_dfx_container *container);
void gex_dfx_release_level(gex_dfx_level *level);
const char *gex_dfx_status_name(gex_dfx_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_ASSET_DFX_H */
