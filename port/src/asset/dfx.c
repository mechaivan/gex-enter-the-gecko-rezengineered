/*
 * dfx.c - reader for the ".DFX" container of GEX3D.EXE (EU).
 */
#include "gex/asset/dfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int validate_descriptor(const gex_dfx_container *container,
                              const gex_dfx_descriptor *desc) {
    if (desc->vertex_count > 0) {
        size_t bytes = (size_t)desc->vertex_count * GEX_VERTEX_SIZE;
        if (bytes / GEX_VERTEX_SIZE != desc->vertex_count) return 0;
        if ((size_t)desc->vertex_table + bytes > container->image_size) return 0;
    }
    if (desc->polygon_count > 0) {
        size_t bytes = (size_t)desc->polygon_count * GEX_POLYGON_SIZE;
        if (bytes / GEX_POLYGON_SIZE != desc->polygon_count) return 0;
        if ((size_t)desc->polygon_table + bytes > container->image_size) return 0;
    }
    if (desc->normal_count > 0) {
        size_t bytes = (size_t)desc->normal_count * GEX_NORMAL_SIZE;
        if (bytes / GEX_NORMAL_SIZE != desc->normal_count) return 0;
        if ((size_t)desc->normal_table + bytes > container->image_size) return 0;
    }
    if (desc->object_table != 0) {
        if ((size_t)desc->object_table + sizeof(uint32_t) > container->image_size) return 0;
    }
    return 1;
}

gex_dfx_status gex_dfx_parse(const void *data, size_t size, gex_dfx_container *out) {
    if (data == NULL || out == NULL) {
        return GEX_DFX_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));

    if (size < sizeof(uint32_t)) {
        return GEX_DFX_ERR_TRUNCATED;
    }

    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t count = *(const uint32_t *)bytes;

    if (count == 0) {
        return GEX_DFX_ERR_FORMAT;
    }
    if (count > 0x100000u) {
        return GEX_DFX_ERR_FORMAT;
    }
    if ((size_t)count > (size - sizeof(uint32_t)) / sizeof(uint32_t)) {
        return GEX_DFX_ERR_TRUNCATED;
    }

    if (count > UINT32_MAX - GEX_DFX_ALIGN_ADD) {
        return GEX_DFX_ERR_OVERFLOW;
    }
    uint32_t aligned = (count + GEX_DFX_ALIGN_ADD) & ~GEX_DFX_ALIGN_MASK;

    size_t table_bytes = (size_t)count * sizeof(uint32_t);
    size_t header_bytes = sizeof(uint32_t) + table_bytes;
    if (header_bytes > size) {
        return GEX_DFX_ERR_TRUNCATED;
    }
    size_t image_offset = (size_t)aligned * sizeof(uint32_t);
    if (image_offset < header_bytes || image_offset > size) {
        return GEX_DFX_ERR_ALIGNMENT;
    }
    size_t image_size = size - image_offset;
    if (image_size == 0) {
        return GEX_DFX_ERR_TRUNCATED;
    }

    const uint32_t *src_offsets = (const uint32_t *)(bytes + sizeof(uint32_t));
    for (uint32_t i = 0; i < count; i++) {
        uint32_t off = src_offsets[i];
        if (off % sizeof(uint32_t) != 0) {
            return GEX_DFX_ERR_ALIGNMENT;
        }
        if ((size_t)off + sizeof(uint32_t) > image_size) {
            return GEX_DFX_ERR_POINTER;
        }
        uint32_t val = *(const uint32_t *)(bytes + image_offset + off);
        if ((size_t)val + sizeof(uint32_t) > image_size) {
            return GEX_DFX_ERR_POINTER;
        }
    }

    gex_dfx_container c;
    memset(&c, 0, sizeof(c));
    c.count = count;
    c.aligned = aligned;
    c.data_size = size;
    c.image_size = image_size;

    c.offsets = malloc(table_bytes);
    if (c.offsets == NULL) {
        return GEX_DFX_ERR_MEMORY;
    }
    memcpy(c.offsets, src_offsets, table_bytes);

    c.image = malloc(image_size);
    if (c.image == NULL) {
        free(c.offsets);
        return GEX_DFX_ERR_MEMORY;
    }
    memcpy(c.image, bytes + image_offset, image_size);

    *out = c;
    return GEX_DFX_OK;
}

gex_dfx_status gex_dfx_load(const char *path, gex_dfx_container *out) {
    if (path == NULL || out == NULL) {
        return GEX_DFX_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return GEX_DFX_ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return GEX_DFX_ERR_IO;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return GEX_DFX_ERR_IO;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return GEX_DFX_ERR_IO;
    }
    if ((unsigned long)file_size > 0x10000000UL) {
        fclose(file);
        return GEX_DFX_ERR_FORMAT;
    }
    size_t size = (size_t)file_size;

    uint8_t *data = NULL;
    if (size > 0) {
        data = malloc(size);
        if (data == NULL) {
            fclose(file);
            return GEX_DFX_ERR_MEMORY;
        }
        size_t bytes_read = fread(data, 1, size, file);
        if (bytes_read != size) {
            free(data);
            fclose(file);
            return GEX_DFX_ERR_IO;
        }
    }
    fclose(file);

    gex_dfx_status status = gex_dfx_parse(data, size, out);
    free(data);
    return status;
}

gex_dfx_status gex_dfx_relocate_raw(uint8_t *image, size_t image_size,
                                    const uint32_t *offsets, uint32_t count,
                                    uint32_t delta) {
    if (image == NULL || offsets == NULL) {
        return GEX_DFX_ERR_ARGUMENT;
    }
    if (count == 0) {
        return GEX_DFX_OK;
    }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t off = offsets[i];
        if (off % sizeof(uint32_t) != 0) {
            return GEX_DFX_ERR_ALIGNMENT;
        }
        if ((size_t)off + sizeof(uint32_t) > image_size) {
            return GEX_DFX_ERR_POINTER;
        }
        uint32_t *ptr = (uint32_t *)(image + off);
        *ptr += delta;
    }
    return GEX_DFX_OK;
}

gex_dfx_status gex_dfx_relocate(gex_dfx_container *container, uint32_t delta) {
    if (container == NULL) {
        return GEX_DFX_ERR_ARGUMENT;
    }
    return gex_dfx_relocate_raw(container->image, container->image_size,
                                container->offsets, container->count, delta);
}

gex_dfx_status gex_dfx_load_level(const gex_dfx_container *container,
                                  gex_dfx_level *out) {
    if (container == NULL || out == NULL) {
        return GEX_DFX_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));

    if (container->image_size < sizeof(uint32_t)) {
        return GEX_DFX_ERR_DESCRIPTOR;
    }
    uint32_t desc_offset = *(const uint32_t *)container->image;
    if (desc_offset == 0 || (size_t)desc_offset + GEX_DESCRIPTOR_SIZE > container->image_size) {
        return GEX_DFX_ERR_DESCRIPTOR;
    }
    memcpy(&out->descriptor, container->image + desc_offset, GEX_DESCRIPTOR_SIZE);

    if (!validate_descriptor(container, &out->descriptor)) {
        return GEX_DFX_ERR_DESCRIPTOR;
    }

    out->vertex_data = container->image + out->descriptor.vertex_table;
    out->polygon_data = container->image + out->descriptor.polygon_table;
    out->normal_data = (int16_t *)(container->image + out->descriptor.normal_table);
    return GEX_DFX_OK;
}

void gex_dfx_release_container(gex_dfx_container *container) {
    if (container == NULL) return;
    free(container->offsets);
    free(container->image);
    memset(container, 0, sizeof(*container));
}

void gex_dfx_release_level(gex_dfx_level *level) {
    if (level == NULL) return;
    memset(level, 0, sizeof(*level));
}

const char *gex_dfx_status_name(gex_dfx_status status) {
    switch (status) {
        case GEX_DFX_OK: return "ok";
        case GEX_DFX_ERR_ARGUMENT: return "invalid argument";
        case GEX_DFX_ERR_IO: return "I/O error";
        case GEX_DFX_ERR_FORMAT: return "invalid format";
        case GEX_DFX_ERR_TRUNCATED: return "truncated";
        case GEX_DFX_ERR_ALIGNMENT: return "alignment error";
        case GEX_DFX_ERR_POINTER: return "invalid pointer";
        case GEX_DFX_ERR_MEMORY: return "memory allocation failed";
        case GEX_DFX_ERR_DESCRIPTOR: return "invalid descriptor";
        case GEX_DFX_ERR_OVERFLOW: return "overflow";
        default: return "unknown error";
    }
}
