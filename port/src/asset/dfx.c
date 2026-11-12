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

/* ------------------------------------------------------------------ */
/* Trayectoria de camara dentro del contenedor                        */
/* ------------------------------------------------------------------ */

/*
 * Lecturas little-endian con límite de rango. La imagen del `.DFX` es un
 * bloque de bytes plano, no una estructura del host: se lee byte a byte para
 * no depender del orden de bytes ni del alineamiento del compilador.
 */
static int dfx_u16(const uint8_t *image, size_t size, uint32_t off,
                   uint16_t *out) {
    if ((size_t)off + 2u > size) {
        return 0;
    }
    *out = (uint16_t)((uint16_t)image[off] | ((uint16_t)image[off + 1u] << 8));
    return 1;
}

static int dfx_u32(const uint8_t *image, size_t size, uint32_t off,
                   uint32_t *out) {
    if ((size_t)off + 4u > size) {
        return 0;
    }
    *out = (uint32_t)image[off] | ((uint32_t)image[off + 1u] << 8) |
           ((uint32_t)image[off + 2u] << 16) | ((uint32_t)image[off + 3u] << 24);
    return 1;
}

static int dfx_i16(const uint8_t *image, size_t size, uint32_t off,
                   int16_t *out) {
    uint16_t v;
    if (!dfx_u16(image, size, off, &v)) {
        return 0;
    }
    *out = (int16_t)v;
    return 1;
}

static int dfx_i32(const uint8_t *image, size_t size, uint32_t off,
                   int32_t *out) {
    uint32_t v;
    if (!dfx_u32(image, size, off, &v)) {
        return 0;
    }
    *out = (int32_t)v;
    return 1;
}

/*
 * Lee la lista de slots de la cabecera de nivel.
 *
 * La lista es una secuencia de entradas (opcode u32, indice u32, puntero
 * u32) terminada por el opcode 0x0E. De MAP7 se conocen dos entradas, las
 * dos con opcode 0x4F: el slot 0 (trayectoria de posicion) y el slot 1 (una
 * spline hermana cuyo papel no esta establecido).
 *
 * Se busca el slot 0 con opcode 0x4F. Si no esta, el nivel no tiene la
 * estructura documentada y se devuelve GEX_DFX_ERR_PATH: no se adivina.
 */
static gex_dfx_status dfx_find_camera_slot(const uint8_t *image, size_t size,
                                           uint32_t header, uint32_t *actor_out,
                                           uint32_t *slot_out) {
    uint32_t off = header;
    uint32_t i;

    for (i = 0; i < GEX_DFX_MAX_SLOTS; i++) {
        uint32_t opcode;
        uint32_t index;
        uint32_t pointer;

        if (!dfx_u32(image, size, off, &opcode)) {
            return GEX_DFX_ERR_PATH;
        }
        off += 4u;
        if (opcode == GEX_DFX_SLOT_OPCODE_END) {
            break; /* fin de la lista */
        }
        if (!dfx_u32(image, size, off, &index)) {
            return GEX_DFX_ERR_PATH;
        }
        off += 4u;
        if (!dfx_u32(image, size, off, &pointer)) {
            return GEX_DFX_ERR_PATH;
        }
        off += 4u;
        if (index == GEX_DFX_SLOT_CAMERA && opcode == GEX_DFX_SLOT_OPCODE_PATH) {
            *actor_out = pointer;
            *slot_out = index;
            return GEX_DFX_OK;
        }
    }
    return GEX_DFX_ERR_PATH;
}

gex_dfx_status gex_dfx_load_camera_path(const gex_dfx_container *container,
                                        gex_dfx_camera_path *out) {
    const uint8_t *image;
    size_t size;
    uint32_t actor;
    uint32_t slot;
    uint32_t entity;
    uint32_t object;
    uint32_t keys;
    uint16_t count;
    uint8_t variant;
    uint8_t flags;
    gex_camera_path_key *keys_mem;
    uint32_t i;

    if (container == NULL || out == NULL) {
        return GEX_DFX_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (container->image == NULL) {
        return GEX_DFX_ERR_ARGUMENT;
    }
    image = container->image;
    size = container->image_size;

    /* 1. Cabecera de nivel -> slot 0 -> registro de actor. */
    if (dfx_find_camera_slot(image, size, GEX_DFX_LEVEL_HEADER_OFFSET, &actor,
                              &slot) != GEX_DFX_OK) {
        return GEX_DFX_ERR_PATH;
    }

    /* 2. Actor + 0x28 -> entidad. */
    if (!dfx_u32(image, size, actor + GEX_DFX_ACTOR_ENTITY_OFFSET, &entity)) {
        return GEX_DFX_ERR_PATH;
    }
    /* 3. Entidad + 0x00 -> objeto. */
    if (!dfx_u32(image, size, entity, &object)) {
        return GEX_DFX_ERR_PATH;
    }
    /* 4. Objeto -> array de registros, cantidad, variante y flags. */
    if (!dfx_u32(image, size, object, &keys)) {
        return GEX_DFX_ERR_PATH;
    }
    if (!dfx_u16(image, size, object + 0x04u, &count)) {
        return GEX_DFX_ERR_PATH;
    }
    if ((size_t)object + 0x08u > size) {
        return GEX_DFX_ERR_PATH;
    }
    variant = image[object + 0x06u];
    flags = image[object + 0x07u];

    /* 5. Contrato de los registros: variante de posicion (32 B), al menos
     *    dos registros (uno de segmento y el centinela), el array dentro de la
     *    imagen, duraciones no negativas y centinela al final. */
    if (variant != GEX_CAMERA_PATH_VARIANT_POSITION) {
        return GEX_DFX_ERR_PATH;
    }
    if (count < 2u) {
        return GEX_DFX_ERR_PATH;
    }
    if ((size_t)keys + (size_t)count * GEX_CAMERA_PATH_KEY_SIZE > size) {
        return GEX_DFX_ERR_PATH;
    }

    keys_mem = (gex_camera_path_key *)calloc((size_t)count,
                                            sizeof(gex_camera_path_key));
    if (keys_mem == NULL) {
        return GEX_DFX_ERR_MEMORY;
    }

    for (i = 0; i < count; i++) {
        uint32_t off = keys + i * GEX_CAMERA_PATH_KEY_SIZE;
        gex_camera_path_key *k = &keys_mem[i];
        int16_t v;
        int32_t w;
        int a;

        if (!dfx_i16(image, size, off + GEX_CAMERA_PATH_KEY_DURATION, &v)) {
            free(keys_mem);
            return GEX_DFX_ERR_PATH;
        }
        k->duration = v;
        for (a = 0; a < 3; a++) {
            if (!dfx_i16(image, size,
                         off + GEX_CAMERA_PATH_KEY_POINT + (uint32_t)(a * 2),
                         &v)) {
                free(keys_mem);
                return GEX_DFX_ERR_PATH;
            }
            k->point[a] = v;
        }
        for (a = 0; a < 3; a++) {
            if (!dfx_i32(image, size,
                         off + GEX_CAMERA_PATH_KEY_TANGENT_OUT +
                             (uint32_t)(a * 4),
                         &w)) {
                free(keys_mem);
                return GEX_DFX_ERR_PATH;
            }
            k->tangent_out[a] = w;
        }
        for (a = 0; a < 3; a++) {
            if (!dfx_i32(image, size,
                         off + GEX_CAMERA_PATH_KEY_TANGENT_IN +
                             (uint32_t)(a * 4),
                         &w)) {
                free(keys_mem);
                return GEX_DFX_ERR_PATH;
            }
            k->tangent_in[a] = w;
        }
    }

    /* Duraciones: no negativas y centinela (D == 0) en el ultimo registro. */
    for (i = 0; i + 1u < count; i++) {
        if (keys_mem[i].duration < 0) {
            free(keys_mem);
            return GEX_DFX_ERR_PATH;
        }
    }
    if (keys_mem[count - 1u].duration != 0) {
        free(keys_mem);
        return GEX_DFX_ERR_PATH;
    }

    out->path.keys = keys_mem;
    out->owned_keys = keys_mem;
    out->path.count = (int16_t)count;
    out->path.variant = variant;
    out->path.flags = flags;
    out->header_offset = GEX_DFX_LEVEL_HEADER_OFFSET;
    out->actor_offset = actor;
    out->entity_offset = entity;
    out->object_offset = object;
    out->keys_offset = keys;
    out->loaded = 1;
    return GEX_DFX_OK;
}

void gex_dfx_release_camera_path(gex_dfx_camera_path *path) {
    if (path == NULL) return;
    free(path->owned_keys);
    memset(path, 0, sizeof(*path));
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
        case GEX_DFX_ERR_PATH: return "camera path not found or invalid";
        default: return "unknown error";
    }
}
