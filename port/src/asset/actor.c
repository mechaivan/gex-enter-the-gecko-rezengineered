/*
 * actor.c - lectura de clases y modelos de actor del `.DFX` (portable).
 *
 * Véase gex/actor.h. Todo offset que llega aquí es un offset **dentro de la
 * imagen** del contenedor y se valida antes de leer: ninguna suma puede
 * desbordar ni salirse del buffer.
 */

#include "gex/actor.h"

#include <string.h>

/* Campos de la clase, medidos en MAP7.DFX. */
#define CLS_MODEL_COUNT  0x08u
#define CLS_MODEL_TABLE  0x0Cu
#define CLS_BRANCH       0x10u
#define CLS_NAME_PTR     0x24u

/* Campos del descriptor de modelo. */
#define MDL_VERTEX_COUNT  0x00u
#define MDL_VERTEX_TABLE  0x04u
#define MDL_POLYGON_COUNT 0x10u
#define MDL_POLYGON_TABLE 0x14u
#define MDL_SUBPART_COUNT 0x18u
#define MDL_SUBPART_TABLE 0x1Cu
#define MDL_NORMAL_TABLE  0x20u
#define MDL_RADIUS        0x24u

/* Campos de la subparte. */
#define SUB_LAST_POLYGON 0x06u
#define SUB_FIRST_VERTEX 0x08u
#define SUB_LAST_VERTEX  0x0Au

static int actor_in_range(size_t image_size, uint32_t offset, size_t len)
{
    if (offset > image_size) {
        return 0;
    }
    return (size_t)offset + len <= image_size;
}

static uint32_t actor_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int16_t actor_s16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint16_t actor_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* Copia el nombre de una clase comprobando que es una cadena imprimible y
 * terminada dentro de `GEX_ACTOR_CLASS_NAME_MAX`. */
static gex_actor_status actor_read_name(const uint8_t *image, size_t image_size,
                                        uint32_t ptr, char *out)
{
    size_t i;

    if (!actor_in_range(image_size, ptr, 1)) {
        return GEX_ACTOR_ERR_RANGE;
    }
    for (i = 0; i < GEX_ACTOR_CLASS_NAME_MAX; i++) {
        uint8_t c;
        if (!actor_in_range(image_size, ptr + (uint32_t)i, 1)) {
            return GEX_ACTOR_ERR_RANGE;
        }
        c = image[ptr + i];
        if (c == 0) {
            if (i == 0) {
                return GEX_ACTOR_ERR_NAME;
            }
            out[i] = '\0';
            return GEX_ACTOR_OK;
        }
        if (c < 0x20 || c > 0x7E) {
            return GEX_ACTOR_ERR_NAME;
        }
        out[i] = (char)c;
    }
    return GEX_ACTOR_ERR_NAME; /* sin terminador en el máximo permitido */
}

gex_actor_status gex_actor_class_read(const uint8_t *image, size_t image_size,
                                      uint32_t offset, gex_actor_class *out)
{
    gex_actor_status st;
    uint32_t count, table;

    if (image == NULL || out == NULL) {
        return GEX_ACTOR_ERR_ARG;
    }
    if (!actor_in_range(image_size, offset, GEX_ACTOR_MODEL_SIZE)) {
        return GEX_ACTOR_ERR_RANGE;
    }

    count = actor_u32(image + offset + CLS_MODEL_COUNT);
    table = actor_u32(image + offset + CLS_MODEL_TABLE);

    /* Un actor sin modelos no dibuja nada; un recuento absurdo indica que el
     * offset no es una clase. El límite es deliberadamente holgado pero
     * acotado para que la tabla quepa en la imagen. */
    if (count == 0 || count > 1024) {
        return GEX_ACTOR_ERR_FORMAT;
    }
    if (!actor_in_range(image_size, table, (size_t)count * 4u)) {
        return GEX_ACTOR_ERR_FORMAT;
    }

    memset(out, 0, sizeof(*out));
    out->offset = offset;
    out->model_count = count;
    out->model_table = table;
    out->field_0x10 = actor_u32(image + offset + CLS_BRANCH);

    st = actor_read_name(image, image_size,
                         actor_u32(image + offset + CLS_NAME_PTR), out->name);
    if (st != GEX_ACTOR_OK) {
        return st;
    }
    return GEX_ACTOR_OK;
}

gex_actor_status gex_actor_class_find(const uint8_t *image, size_t image_size,
                                      const char *name, gex_actor_class *out)
{
    uint32_t offset;
    size_t len;

    if (image == NULL || name == NULL || out == NULL) {
        return GEX_ACTOR_ERR_ARG;
    }
    len = strlen(name);
    if (len == 0 || len >= GEX_ACTOR_CLASS_NAME_MAX) {
        return GEX_ACTOR_ERR_ARG;
    }

    for (offset = 0; actor_in_range(image_size, offset, GEX_ACTOR_MODEL_SIZE);
         offset += 4u) {
        gex_actor_class cls;
        if (gex_actor_class_read(image, image_size, offset, &cls) !=
            GEX_ACTOR_OK) {
            continue;
        }
        if (strcmp(cls.name, name) == 0) {
            *out = cls;
            return GEX_ACTOR_OK;
        }
    }
    return GEX_ACTOR_ERR_NAME;
}

gex_actor_status gex_actor_model_read(const uint8_t *image, size_t image_size,
                                      const gex_actor_class *cls, uint32_t index,
                                      gex_actor_model *out)
{
    uint32_t model_offset;

    if (image == NULL || cls == NULL || out == NULL) {
        return GEX_ACTOR_ERR_ARG;
    }
    if (index >= cls->model_count) {
        return GEX_ACTOR_ERR_RANGE;
    }
    if (!actor_in_range(image_size, cls->model_table + index * 4u, 4)) {
        return GEX_ACTOR_ERR_RANGE;
    }
    model_offset = actor_u32(image + cls->model_table + index * 4u);
    if (!actor_in_range(image_size, model_offset, GEX_ACTOR_MODEL_SIZE)) {
        return GEX_ACTOR_ERR_RANGE;
    }

    memset(out, 0, sizeof(*out));
    out->offset = model_offset;
    out->vertex_count = actor_u32(image + model_offset + MDL_VERTEX_COUNT);
    out->vertex_table = actor_u32(image + model_offset + MDL_VERTEX_TABLE);
    out->polygon_count = actor_u32(image + model_offset + MDL_POLYGON_COUNT);
    out->polygon_table = actor_u32(image + model_offset + MDL_POLYGON_TABLE);
    out->subpart_count = actor_u32(image + model_offset + MDL_SUBPART_COUNT);
    out->subpart_table = actor_u32(image + model_offset + MDL_SUBPART_TABLE);
    out->normal_table = actor_u32(image + model_offset + MDL_NORMAL_TABLE);
    out->radius = actor_u32(image + model_offset + MDL_RADIUS);

    if (out->vertex_count == 0 || out->polygon_count == 0) {
        return GEX_ACTOR_ERR_FORMAT;
    }
    if (!actor_in_range(image_size, out->vertex_table,
                        (size_t)out->vertex_count * GEX_ACTOR_VERTEX_SIZE)) {
        return GEX_ACTOR_ERR_FORMAT;
    }
    if (!actor_in_range(image_size, out->polygon_table,
                        (size_t)out->polygon_count * GEX_ACTOR_POLYGON_SIZE)) {
        return GEX_ACTOR_ERR_FORMAT;
    }
    if (out->subpart_count != 0 &&
        !actor_in_range(image_size, out->subpart_table,
                        (size_t)out->subpart_count * GEX_ACTOR_SUBPART_SIZE)) {
        return GEX_ACTOR_ERR_FORMAT;
    }
    return GEX_ACTOR_OK;
}

gex_actor_status gex_actor_vertex(const uint8_t *image, size_t image_size,
                                  const gex_actor_model *model, uint32_t index,
                                  int16_t xyz[3])
{
    uint32_t offset;

    if (image == NULL || model == NULL || xyz == NULL) {
        return GEX_ACTOR_ERR_ARG;
    }
    if (index >= model->vertex_count) {
        return GEX_ACTOR_ERR_RANGE;
    }
    offset = model->vertex_table + index * GEX_ACTOR_VERTEX_SIZE;
    if (!actor_in_range(image_size, offset, GEX_ACTOR_VERTEX_SIZE)) {
        return GEX_ACTOR_ERR_RANGE;
    }
    xyz[0] = actor_s16(image + offset);
    xyz[1] = actor_s16(image + offset + 2);
    xyz[2] = actor_s16(image + offset + 4);
    return GEX_ACTOR_OK;
}

gex_actor_status gex_actor_read_polygon(const uint8_t *image, size_t image_size,
                                   const gex_actor_model *model, uint32_t index,
                                   gex_actor_polygon *out)
{
    uint32_t offset;

    if (image == NULL || model == NULL || out == NULL) {
        return GEX_ACTOR_ERR_ARG;
    }
    if (index >= model->polygon_count) {
        return GEX_ACTOR_ERR_RANGE;
    }
    offset = model->polygon_table + index * GEX_ACTOR_POLYGON_SIZE;
    if (!actor_in_range(image_size, offset, GEX_ACTOR_POLYGON_SIZE)) {
        return GEX_ACTOR_ERR_RANGE;
    }
    out->vtx[0] = actor_u16(image + offset);
    out->vtx[1] = actor_u16(image + offset + 2);
    out->vtx[2] = actor_u16(image + offset + 4);
    out->flags = actor_u16(image + offset + 6);
    out->uv_ptr = actor_u32(image + offset + 8);
    return GEX_ACTOR_OK;
}

gex_actor_status gex_actor_read_subpart(const uint8_t *image, size_t image_size,
                                   const gex_actor_model *model, uint32_t index,
                                   gex_actor_subpart *out)
{
    uint32_t offset;

    if (image == NULL || model == NULL || out == NULL) {
        return GEX_ACTOR_ERR_ARG;
    }
    if (index >= model->subpart_count) {
        return GEX_ACTOR_ERR_RANGE;
    }
    offset = model->subpart_table + index * GEX_ACTOR_SUBPART_SIZE;
    if (!actor_in_range(image_size, offset, GEX_ACTOR_SUBPART_SIZE)) {
        return GEX_ACTOR_ERR_RANGE;
    }
    out->last_polygon = actor_s16(image + offset + SUB_LAST_POLYGON);
    out->first_vertex = actor_s16(image + offset + SUB_FIRST_VERTEX);
    out->last_vertex = actor_s16(image + offset + SUB_LAST_VERTEX);
    return GEX_ACTOR_OK;
}

int gex_actor_polygon_textured(const uint8_t *image, size_t image_size,
                               const gex_actor_polygon *poly)
{
    if (image == NULL || poly == NULL) {
        return 0;
    }
    return actor_in_range(image_size, poly->uv_ptr, GEX_ACTOR_UV_SIZE) ? 1 : 0;
}

gex_actor_status gex_actor_read_uv(const uint8_t *image, size_t image_size,
                              const gex_actor_polygon *poly, gex_actor_uv *out)
{
    uint32_t i;

    if (image == NULL || poly == NULL || out == NULL) {
        return GEX_ACTOR_ERR_ARG;
    }
    if (!gex_actor_polygon_textured(image, image_size, poly)) {
        return GEX_ACTOR_ERR_RANGE;
    }
    memset(out, 0, sizeof(*out));
    for (i = 0; i < 3; i++) {
        uint32_t base = poly->uv_ptr + i * 4u;
        out->u[i] = image[base];
        out->v[i] = image[base + 1];
    }
    /* El índice de textura es la palabra del **segundo** grupo (offset +6),
     * medida en los 20 modelos de `tvbutn__` y en el panel: el primero y el
     * tercero llevan valores que no son índices de textura (1 y 31). La capa
     * es el byte +0x0B del registro. */
    out->texture = actor_u16(image + poly->uv_ptr + 6u) & 0x3FFFu;
    out->layer = (int8_t)image[poly->uv_ptr + 0x0Bu];
    return GEX_ACTOR_OK;
}

const char *gex_actor_status_name(gex_actor_status status)
{
    switch (status) {
    case GEX_ACTOR_OK: return "ok";
    case GEX_ACTOR_ERR_ARG: return "argumento invalido";
    case GEX_ACTOR_ERR_RANGE: return "offset fuera de la imagen";
    case GEX_ACTOR_ERR_FORMAT: return "estructura incoherente";
    case GEX_ACTOR_ERR_NAME: return "clase no encontrada o nombre invalido";
    }
    return "desconocido";
}
