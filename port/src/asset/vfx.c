/*
 * vfx.c - parser del contenedor de texturas .VFX (Módulo 4A, portable).
 *
 * Véase gex/vfx.h y docs/VFX_TEXTURE_FORMAT.md. Sin Windows ni D3D11.
 */

#include "gex/vfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Recorre la tabla de registros exactamente como lo hace el juego:
 *
 *   0x00461C32 (el original):
 *     count = dword[0]
 *     registro = fichero + 4
 *     registro[i]->[0x10] = registro[i] + 0x8C   (puntero a los datos)
 *     registro[i+1]      = registro[i] + 0x8C + dword[registro[i]+0x84]
 *
 * El recorrido se comprueba contra el final del fichero: en los ficheros
 * originales el último registro termina exactamente en el EOF.
 */
static gex_vfx_status parse_records(gex_vfx_file *file)
{
    uint64_t offset;
    uint32_t i;

    if (file->size < 4u) {
        return GEX_VFX_ERR_FORMAT;
    }
    file->declared_count = (uint32_t)file->data[0] |
                           ((uint32_t)file->data[1] << 8) |
                           ((uint32_t)file->data[2] << 16) |
                           ((uint32_t)file->data[3] << 24);

    /* La tabla de registros se reserva de una vez: count * sizeof(record). */
    if (file->declared_count > 0u) {
        uint64_t bytes = (uint64_t)file->declared_count *
                         (uint64_t)sizeof(gex_vfx_record);
        if (bytes > (uint64_t)SIZE_MAX) {
            return GEX_VFX_ERR_OVERFLOW;
        }
        file->records = (gex_vfx_record *)calloc(file->declared_count,
                                                 sizeof(gex_vfx_record));
        if (file->records == NULL) {
            return GEX_VFX_ERR_NOMEM;
        }
    }

    offset = 4u; /* tras el dword de conteo */
    for (i = 0; i < file->declared_count; i++) {
        gex_vfx_record *rec = &file->records[i];
        uint64_t data_size;
        uint64_t data_end;

        /* La cabecera del registro debe caber entera en el fichero. */
        if (offset > (uint64_t)file->size ||
            (uint64_t)file->size - offset < GEX_VFX_RECORD_HEADER_SIZE) {
            file->record_count = i;
            return GEX_VFX_ERR_RANGE;
        }

        /* El registro es un `GrTexInfo` de Glide 2.x: smallLod, largeLod,
         * aspectRatio, format y el puntero a los datos. */
        rec->small_lod = (uint32_t)file->data[offset + 0x00] |
                         ((uint32_t)file->data[offset + 0x01] << 8) |
                         ((uint32_t)file->data[offset + 0x02] << 16) |
                         ((uint32_t)file->data[offset + 0x03] << 24);
        rec->large_lod = (uint32_t)file->data[offset + 0x04] |
                         ((uint32_t)file->data[offset + 0x05] << 8) |
                         ((uint32_t)file->data[offset + 0x06] << 16) |
                         ((uint32_t)file->data[offset + 0x07] << 24);
        rec->aspect_ratio = (uint32_t)file->data[offset + 0x08] |
                            ((uint32_t)file->data[offset + 0x09] << 8) |
                            ((uint32_t)file->data[offset + 0x0A] << 16) |
                            ((uint32_t)file->data[offset + 0x0B] << 24);
        rec->format = (uint32_t)file->data[offset + 0x0C] |
                      ((uint32_t)file->data[offset + 0x0D] << 8) |
                      ((uint32_t)file->data[offset + 0x0E] << 16) |
                      ((uint32_t)file->data[offset + 0x0F] << 24);
        rec->stored_pointer = (uint32_t)file->data[offset + 0x10] |
                              ((uint32_t)file->data[offset + 0x11] << 8) |
                              ((uint32_t)file->data[offset + 0x12] << 16) |
                              ((uint32_t)file->data[offset + 0x13] << 24);
        rec->file_offset = offset;

        data_size = (uint64_t)file->data[offset + 0x84] |
                    ((uint64_t)file->data[offset + 0x85] << 8) |
                    ((uint64_t)file->data[offset + 0x86] << 16) |
                    ((uint64_t)file->data[offset + 0x87] << 24);
        if (data_size > 0xFFFFFFFFu) {
            file->record_count = i;
            return GEX_VFX_ERR_OVERFLOW;
        }
        rec->data_size = (uint32_t)data_size;

        /* El bloque de datos empieza en +0x8C y tiene data_size bytes: tiene
         * que caber dentro del fichero. */
        data_end = offset + GEX_VFX_RECORD_DATA_OFFSET + data_size;
        if (data_end > (uint64_t)file->size) {
            file->record_count = i;
            return GEX_VFX_ERR_RANGE;
        }
        rec->data = file->data + offset + GEX_VFX_RECORD_DATA_OFFSET;

        /* La tabla NCC solo existe para el formato YIQ_422 (1): es el único
         * que el juego sube con grTexDownloadTable, y el único para el que
         * Glide espera un `GuNccTable` de 112 B. Para los demás formatos el
         * bloque +0x14 está a cero en los ficheros originales. */
        if (rec->format == GEX_VFX_FORMAT_YIQ_422) {
            rec->ncc_table = file->data + offset + GEX_VFX_RECORD_NCC_TABLE_OFFSET;
            rec->ncc_table_size = GEX_VFX_NCC_TABLE_BYTES;
        } else {
            rec->ncc_table = NULL;
            rec->ncc_table_size = 0;
        }

        offset = data_end;
        file->record_count = i + 1u;
    }

    /* El recorrido tiene que cubrir el fichero EXACTO: en los 37 .VFX del
     * inventario el último bloque termina en el EOF. Si sobran bytes, la
     * tabla que describe `count` no cubre el fichero y el contenedor es
     * incoherente. */
    if (offset != (uint64_t)file->size) {
        return GEX_VFX_ERR_FORMAT;
    }

    return GEX_VFX_OK;
}

gex_vfx_status gex_vfx_parse(gex_vfx_file *file, const uint8_t *data,
                              size_t size)
{
    gex_vfx_status st;

    if (file == NULL || data == NULL) {
        return GEX_VFX_ERR_ARG;
    }
    memset(file, 0, sizeof(*file));
    if (size == 0u) {
        return GEX_VFX_ERR_FORMAT;
    }
    file->data = (uint8_t *)malloc(size);
    if (file->data == NULL) {
        return GEX_VFX_ERR_NOMEM;
    }
    memcpy(file->data, data, size);
    file->size = size;

    st = parse_records(file);
    if (st != GEX_VFX_OK) {
        gex_vfx_close(file);
        return st;
    }
    return GEX_VFX_OK;
}

gex_vfx_status gex_vfx_open(gex_vfx_file *file, const char *path)
{
    FILE *fh;
    long length;
    uint8_t *buffer;
    size_t read_bytes;
    gex_vfx_status st;

    if (file == NULL || path == NULL || path[0] == '\0') {
        return GEX_VFX_ERR_ARG;
    }
    memset(file, 0, sizeof(*file));

    fh = fopen(path, "rb");
    if (fh == NULL) {
        return GEX_VFX_ERR_OPEN;
    }
    if (fseek(fh, 0, SEEK_END) != 0) {
        fclose(fh);
        return GEX_VFX_ERR_OPEN;
    }
    length = ftell(fh);
    if (length <= 0) {
        fclose(fh);
        return GEX_VFX_ERR_OPEN;
    }
    if (fseek(fh, 0, SEEK_SET) != 0) {
        fclose(fh);
        return GEX_VFX_ERR_OPEN;
    }
    buffer = (uint8_t *)malloc((size_t)length);
    if (buffer == NULL) {
        fclose(fh);
        return GEX_VFX_ERR_NOMEM;
    }
    read_bytes = fread(buffer, 1, (size_t)length, fh);
    fclose(fh);
    if (read_bytes != (size_t)length) {
        free(buffer);
        return GEX_VFX_ERR_OPEN;
    }

    file->data = buffer;
    file->size = (size_t)length;
    st = parse_records(file);
    if (st != GEX_VFX_OK) {
        gex_vfx_close(file);
        return st;
    }
    return GEX_VFX_OK;
}

void gex_vfx_close(gex_vfx_file *file)
{
    if (file == NULL) {
        return;
    }
    free(file->records);
    free(file->data);
    memset(file, 0, sizeof(*file));
}

int gex_vfx_find_index(const gex_vfx_file *file, uint32_t index14)
{
    if (file == NULL) {
        return -1;
    }
    /* El índice de la geometría es de 14 bits y el juego lo usa tal cual
     * (`& 0x3FFF`): si supera el número de texturas, no hay entrada. */
    if (index14 >= (uint32_t)file->record_count) {
        return -1;
    }
    return (int)index14;
}

int gex_vfx_format_has_ncc_table(uint32_t format)
{
    /* Solo el formato YIQ_422 (1) lleva tabla NCC: es el único para el que
     * el juego llama a grTexDownloadTable y el único para el que Glide
     * espera una tabla de descompresión (GuNccTable). Para los demás, el
     * bloque +0x14 está a cero en los ficheros originales. */
    return format == GEX_VFX_FORMAT_YIQ_422 ? 1 : 0;
}

int gex_vfx_format_info_get(uint32_t format, gex_vfx_format_info *out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->format = format;
    switch (format) {
    case GEX_VFX_FORMAT_YIQ_422:
        /* Tabla NCC desde +0x14 (112 B) y ninguna activación de alfa. */
        out->name = "GR_TEXFMT_YIQ_422";
        out->has_ncc_table = 1;
        out->enables_alpha = 0;
        out->ncc_table_bytes = GEX_VFX_NCC_TABLE_BYTES;
        out->bytes_per_texel = 1;
        out->decodable = 1;
        return 1;
    case GEX_VFX_FORMAT_ARGB_1555:
        /* Sin tabla y sin activación de alfa. */
        out->name = "GR_TEXFMT_ARGB_1555";
        out->bytes_per_texel = 2;
        out->decodable = 1;
        return 1;
    case GEX_VFX_FORMAT_ARGB_4444:
        /* Sin tabla, pero el juego activa el blending alfa. */
        out->name = "GR_TEXFMT_ARGB_4444";
        out->enables_alpha = 1;
        out->bytes_per_texel = 2;
        out->decodable = 1;
        return 1;
    case GEX_VFX_FORMAT_RGB_332:
        /* El juego lo distingue con otro modo de alfa; no aparece en los
         * .VFX originales, así que no hay decodificador. */
        out->name = "GR_TEXFMT_RGB_332";
        out->enables_alpha = 1;
        out->bytes_per_texel = 1;
        out->decodable = 0;
        return 1;
    default:
        return 0;
    }
}

int gex_vfx_ncc_table_twin(const gex_vfx_file *file, uint32_t index)
{
    uint32_t i;

    if (file == NULL || index >= (uint32_t)file->record_count) {
        return -1;
    }
    if (file->records[index].ncc_table == NULL ||
        file->records[index].ncc_table_size == 0u) {
        return -1;
    }
    for (i = 0; i < (uint32_t)file->record_count; i++) {
        if (i == index) {
            continue;
        }
        if (file->records[i].ncc_table == NULL ||
            file->records[i].ncc_table_size != file->records[index].ncc_table_size) {
            continue;
        }
        if (memcmp(file->records[i].ncc_table, file->records[index].ncc_table,
                   file->records[index].ncc_table_size) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Dimensiones (reglas de Glide 2.x)                                   */
/* ------------------------------------------------------------------ */

/*
 * Glide 2.x: el valor del LOD fija el lado mayor de la textura
 * (GR_LOD_256 = 0 ... GR_LOD_1 = 8) y la relación de aspecto lo divide
 * (GR_ASPECT_8x1 = 0 ... GR_ASPECT_1x8 = 6). El lado menor nunca baja de
 * un texel.
 */
static uint32_t lod_side(uint32_t lod)
{
    return 256u >> lod; /* lod 0..8 */
}

static void level_size(uint32_t aspect, uint32_t lod, uint32_t *width,
                       uint32_t *height)
{
    uint32_t side = lod_side(lod);
    uint32_t w;
    uint32_t h;

    switch (aspect) {
    case 0: /* 8x1 */
        w = side;
        h = (side + 7u) / 8u;
        break;
    case 1: /* 4x1 */
        w = side;
        h = (side + 3u) / 4u;
        break;
    case 2: /* 2x1 */
        w = side;
        h = (side + 1u) / 2u;
        break;
    case 3: /* 1x1 */
        w = side;
        h = side;
        break;
    case 4: /* 1x2 */
        w = (side + 1u) / 2u;
        h = side;
        break;
    case 5: /* 1x4 */
        w = (side + 3u) / 4u;
        h = side;
        break;
    default: /* 6: 1x8 */
        w = (side + 7u) / 8u;
        h = side;
        break;
    }
    if (w < 1u) {
        w = 1u;
    }
    if (h < 1u) {
        h = 1u;
    }
    *width = w;
    *height = h;
}

int gex_vfx_texture_info_get(const gex_vfx_record *rec,
                             gex_vfx_texture_info *out)
{
    gex_vfx_format_info fmt;
    uint32_t lod;
    uint32_t total;

    if (rec == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    if (!gex_vfx_format_info_get(rec->format, &fmt)) {
        return 0;
    }
    if (rec->small_lod > 8u || rec->large_lod > 8u ||
        rec->aspect_ratio > 6u) {
        return 0;
    }
    /* La cadena de mipmap va de large_lod (el nivel más detallado) a
     * small_lod; si small_lod < large_lod la cadena está vacía. */
    if (rec->small_lod < rec->large_lod) {
        return 0;
    }

    out->format = rec->format;
    out->small_lod = rec->small_lod;
    out->large_lod = rec->large_lod;
    out->aspect_ratio = rec->aspect_ratio;
    out->levels = rec->small_lod - rec->large_lod + 1u;
    out->bytes_per_texel = fmt.bytes_per_texel;
    out->has_ncc_table =
        (rec->ncc_table != NULL && rec->ncc_table_size == GEX_VFX_NCC_TABLE_BYTES)
            ? 1
            : 0;

    level_size(rec->aspect_ratio, rec->large_lod, &out->width, &out->height);

    total = 0;
    for (lod = rec->large_lod; lod <= rec->small_lod; lod++) {
        uint32_t w;
        uint32_t h;
        level_size(rec->aspect_ratio, lod, &w, &h);
        total += w * h * out->bytes_per_texel;
    }
    out->total_bytes = total;
    return 1;
}

int gex_vfx_texture_level_size(const gex_vfx_texture_info *info,
                                uint32_t level, uint32_t *width,
                                uint32_t *height)
{
    uint32_t lod;

    if (info == NULL || width == NULL || height == NULL) {
        return 0;
    }
    if (level >= info->levels) {
        return 0;
    }
    lod = info->large_lod + level;
    if (lod > 8u) {
        return 0;
    }
    level_size(info->aspect_ratio, lod, width, height);
    return 1;
}

uint32_t gex_vfx_texture_level_offset(const gex_vfx_texture_info *info,
                                       uint32_t level)
{
    uint32_t lod;
    uint32_t offset;

    if (info == NULL || level >= info->levels) {
        return 0;
    }
    offset = 0;
    for (lod = info->large_lod; lod < info->large_lod + level; lod++) {
        uint32_t w;
        uint32_t h;
        level_size(info->aspect_ratio, lod, &w, &h);
        offset += w * h * info->bytes_per_texel;
    }
    return offset;
}

const char *gex_vfx_status_name(gex_vfx_status status)
{
    switch (status) {
    case GEX_VFX_OK:
        return "ok";
    case GEX_VFX_ERR_ARG:
        return "bad argument";
    case GEX_VFX_ERR_OPEN:
        return "could not open file";
    case GEX_VFX_ERR_FORMAT:
        return "invalid header";
    case GEX_VFX_ERR_RANGE:
        return "offset or size out of range";
    case GEX_VFX_ERR_OVERFLOW:
        return "size overflow";
    case GEX_VFX_ERR_NOMEM:
        return "out of memory";
    }
    return "unknown";
}
