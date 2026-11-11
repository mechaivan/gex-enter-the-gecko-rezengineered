/*
 * vfx_decode.c - decodificadores CPU de los formatos de píxel de las texturas
 * .VFX (Módulo 4B, portable C11).
 *
 * Los tres formatos que aparecen en los ficheros originales están
 * implementados porque su interpretación está DEMOSTRADA con el SDK público de
 * Glide 2.x y comprobada contra los ficheros originales:
 *
 *   GR_TEXFMT_YIQ_422   1 byte por texel: el byte codifica un índice de la
 *                       paleta de 256 colores que se deriva de la tabla NCC
 *                       (4 bits de luminancia, 2 bits de croma I, 2 bits de
 *                       croma Q).
 *   GR_TEXFMT_ARGB_1555 2 bytes por texel, palabra de 16 bits little-endian:
 *                       A(1) R(5) G(5) B(5).
 *   GR_TEXFMT_ARGB_4444 2 bytes por texel, palabra de 16 bits little-endian:
 *                       A(4) R(4) G(4) B(4).
 *
 * La salida es siempre RGBA8 (R, G, B, A) con alfa recto.
 *
 * Véase gex/vfx.h y docs/VFX_TEXTURE_FORMAT.md.
 */

#include "gex/vfx.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Tabla NCC                                                           */
/* ------------------------------------------------------------------ */

/*
 * Los 24 bytes de cada vector de croma son valores de 9 bits con signo
 * (complemento a dos): el hardware los interpreta en ese rango y el SDK de
 * Glide 2.x los empaqueta en campos de 9 bits.
 */
static int16_t sign_extend9(uint16_t raw)
{
    uint32_t v = (uint32_t)(raw & 0x1FFu);

    if (v >= 0x100u) {
        v -= 0x200u;
    }
    return (int16_t)(int32_t)v;
}

int gex_vfx_ncc_table_parse(const uint8_t *raw, size_t size,
                             gex_vfx_ncc_table *out)
{
    uint32_t i;

    if (raw == NULL || out == NULL) {
        return 0;
    }
    if (size < GEX_VFX_NCC_TABLE_BYTES) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    /* yRGB[16]: un byte por nivel de luminancia. */
    for (i = 0; i < 16u; i++) {
        out->y_rgb[i] = raw[i];
    }
    /* iRGB[4][3] y qRGB[4][3]: 12 valores de 16 bits con signo cada uno. */
    for (i = 0; i < 12u; i++) {
        uint32_t at = 16u + i * 2u;
        uint16_t v = (uint16_t)((uint16_t)raw[at] |
                                ((uint16_t)raw[at + 1u] << 8));
        out->i_rgb[i / 3u][i % 3u] = sign_extend9(v);
        at = 40u + i * 2u;
        v = (uint16_t)((uint16_t)raw[at] | ((uint16_t)raw[at + 1u] << 8));
        out->q_rgb[i / 3u][i % 3u] = sign_extend9(v);
    }
    /* packed_data[12]: la forma que consume el hardware. */
    for (i = 0; i < 12u; i++) {
        uint32_t at = 64u + i * 4u;
        out->packed_data[i] = (uint32_t)raw[at] |
                              ((uint32_t)raw[at + 1u] << 8) |
                              ((uint32_t)raw[at + 2u] << 16) |
                              ((uint32_t)raw[at + 3u] << 24);
    }
    return 1;
}

void gex_vfx_ncc_table_pack(const gex_vfx_ncc_table *table,
                            uint32_t packed[12])
{
    uint32_t i;

    if (table == NULL || packed == NULL) {
        return;
    }
    /* Los 16 niveles de luminancia, 4 bytes en cada palabra. */
    for (i = 0; i < 4u; i++) {
        packed[i] = (uint32_t)table->y_rgb[i * 4u + 0u] |
                    ((uint32_t)table->y_rgb[i * 4u + 1u] << 8) |
                    ((uint32_t)table->y_rgb[i * 4u + 2u] << 16) |
                    ((uint32_t)table->y_rgb[i * 4u + 3u] << 24);
    }
    /* Los vectores de croma I, tres campos de 9 bits en cada palabra. */
    for (i = 0; i < 4u; i++) {
        uint32_t v0 = (uint32_t)table->i_rgb[i][0] & 0x1FFu;
        uint32_t v1 = (uint32_t)table->i_rgb[i][1] & 0x1FFu;
        uint32_t v2 = (uint32_t)table->i_rgb[i][2] & 0x1FFu;
        packed[i + 4u] = (v0 << 18) | (v1 << 9) | v2;
    }
    /* Los vectores de croma Q. */
    for (i = 0; i < 4u; i++) {
        uint32_t v0 = (uint32_t)table->q_rgb[i][0] & 0x1FFu;
        uint32_t v1 = (uint32_t)table->q_rgb[i][1] & 0x1FFu;
        uint32_t v2 = (uint32_t)table->q_rgb[i][2] & 0x1FFu;
        packed[i + 8u] = (v0 << 18) | (v1 << 9) | v2;
    }
}

/* ------------------------------------------------------------------ */
/* Utilidades de expansión                                             */
/* ------------------------------------------------------------------ */

/* 5 bits -> 8 bits por replicación de bits (0..31 -> 0..255). */
static uint8_t expand5(uint32_t v)
{
    return (uint8_t)((v << 3) | (v >> 2));
}

/* 4 bits -> 8 bits (0..15 -> 0..255). */
static uint8_t expand4(uint32_t v)
{
    return (uint8_t)(v * 17u);
}

static uint8_t clamp255(int32_t v)
{
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return (uint8_t)v;
}

/* Comprueba las dimensiones y los tamaños de entrada y salida. */
static gex_vfx_decode_status check_sizes(uint32_t width, uint32_t height,
                                         uint32_t bytes_per_texel,
                                         size_t src_size, size_t dst_size)
{
    uint64_t need_src;
    uint64_t need_dst;

    if (width == 0u || height == 0u) {
        return GEX_VFX_DEC_ERR_ARG;
    }
    /* Un nivel de Glide 2.x no pasa de 256x256 texels. */
    if (width > 256u || height > 256u) {
        return GEX_VFX_DEC_ERR_ARG;
    }
    need_src = (uint64_t)width * (uint64_t)height *
               (uint64_t)bytes_per_texel;
    need_dst = (uint64_t)width * (uint64_t)height * 4u;
    if (need_src > (uint64_t)SIZE_MAX || need_dst > (uint64_t)SIZE_MAX) {
        return GEX_VFX_DEC_ERR_OVERFLOW;
    }
    if ((uint64_t)src_size < need_src) {
        return GEX_VFX_DEC_ERR_RANGE;
    }
    if ((uint64_t)dst_size < need_dst) {
        return GEX_VFX_DEC_ERR_RANGE;
    }
    return GEX_VFX_DEC_OK;
}

/* ------------------------------------------------------------------ */
/* Decodificadores                                                     */
/* ------------------------------------------------------------------ */

gex_vfx_decode_status gex_vfx_decode_argb1555(const uint8_t *src,
                                               size_t src_size, uint32_t width,
                                               uint32_t height, uint8_t *dst,
                                               size_t dst_size)
{
    gex_vfx_decode_status st;
    uint32_t x;
    uint32_t y;

    if (src == NULL || dst == NULL) {
        return GEX_VFX_DEC_ERR_ARG;
    }
    st = check_sizes(width, height, 2u, src_size, dst_size);
    if (st != GEX_VFX_DEC_OK) {
        return st;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            size_t si = ((size_t)y * (size_t)width + (size_t)x) * 2u;
            size_t di = ((size_t)y * (size_t)width + (size_t)x) * 4u;
            uint32_t word = (uint32_t)src[si] | ((uint32_t)src[si + 1u] << 8);

            dst[di + 0] = expand5((word >> 10) & 0x1Fu);
            dst[di + 1] = expand5((word >> 5) & 0x1Fu);
            dst[di + 2] = expand5(word & 0x1Fu);
            dst[di + 3] = (word & 0x8000u) ? 255u : 0u;
        }
    }
    return GEX_VFX_DEC_OK;
}

gex_vfx_decode_status gex_vfx_decode_argb4444(const uint8_t *src,
                                               size_t src_size, uint32_t width,
                                               uint32_t height, uint8_t *dst,
                                               size_t dst_size)
{
    gex_vfx_decode_status st;
    uint32_t x;
    uint32_t y;

    if (src == NULL || dst == NULL) {
        return GEX_VFX_DEC_ERR_ARG;
    }
    st = check_sizes(width, height, 2u, src_size, dst_size);
    if (st != GEX_VFX_DEC_OK) {
        return st;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            size_t si = ((size_t)y * (size_t)width + (size_t)x) * 2u;
            size_t di = ((size_t)y * (size_t)width + (size_t)x) * 4u;
            uint32_t word = (uint32_t)src[si] | ((uint32_t)src[si + 1u] << 8);

            dst[di + 0] = expand4((word >> 8) & 0x0Fu);
            dst[di + 1] = expand4((word >> 4) & 0x0Fu);
            dst[di + 2] = expand4(word & 0x0Fu);
            dst[di + 3] = expand4((word >> 12) & 0x0Fu);
        }
    }
    return GEX_VFX_DEC_OK;
}

gex_vfx_decode_status gex_vfx_decode_yiq422(const uint8_t *src, size_t src_size,
                                            const gex_vfx_ncc_table *ncc,
                                            uint32_t width, uint32_t height,
                                            uint8_t *dst, size_t dst_size)
{
    gex_vfx_decode_status st;
    uint32_t x;
    uint32_t y;

    if (src == NULL || dst == NULL || ncc == NULL) {
        return GEX_VFX_DEC_ERR_ARG;
    }
    st = check_sizes(width, height, 1u, src_size, dst_size);
    if (st != GEX_VFX_DEC_OK) {
        return st;
    }

    /*
     * El byte de cada texel es un índice de la paleta de 256 colores que
     * sale de la tabla NCC: 4 bits de luminancia (yRGB), 2 bits de croma I
     * (iRGB) y 2 bits de croma Q (qRGB). El color es la suma de los tres,
     * saturada a 0..255.
     */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            size_t si = (size_t)y * (size_t)width + (size_t)x;
            size_t di = si * 4u;
            uint32_t index = src[si];
            uint32_t iy = (index >> 4) & 0x0Fu;
            uint32_t ia = (index >> 2) & 0x03u;
            uint32_t ib = index & 0x03u;
            int32_t lum = (int32_t)ncc->y_rgb[iy];

            dst[di + 0] = clamp255(lum + (int32_t)ncc->i_rgb[ia][0] +
                                   (int32_t)ncc->q_rgb[ib][0]);
            dst[di + 1] = clamp255(lum + (int32_t)ncc->i_rgb[ia][1] +
                                   (int32_t)ncc->q_rgb[ib][1]);
            dst[di + 2] = clamp255(lum + (int32_t)ncc->i_rgb[ia][2] +
                                   (int32_t)ncc->q_rgb[ib][2]);
            dst[di + 3] = 255u; /* el formato YIQ_422 no lleva alfa */
        }
    }
    return GEX_VFX_DEC_OK;
}

gex_vfx_decode_status gex_vfx_decode_level(const gex_vfx_record *rec,
                                            const gex_vfx_texture_info *info,
                                            uint32_t level, uint8_t *dst,
                                            size_t dst_size)
{
    uint32_t width;
    uint32_t height;
    uint32_t offset;

    if (rec == NULL || info == NULL || dst == NULL) {
        return GEX_VFX_DEC_ERR_ARG;
    }
    /* El formato decide si hay decodificador: se comprueba antes que el
     * tamaño de los datos para que el error sea el utile. */
    if (rec->format != GEX_VFX_FORMAT_ARGB_1555 &&
        rec->format != GEX_VFX_FORMAT_ARGB_4444 &&
        rec->format != GEX_VFX_FORMAT_YIQ_422) {
        return GEX_VFX_DEC_ERR_FORMAT;
    }
    if (!gex_vfx_texture_level_size(info, level, &width, &height)) {
        return GEX_VFX_DEC_ERR_ARG;
    }
    if (rec->data == NULL) {
        return GEX_VFX_DEC_ERR_RANGE;
    }
    offset = gex_vfx_texture_level_offset(info, level);
    if ((uint64_t)offset +
            (uint64_t)width * (uint64_t)height *
                (uint64_t)info->bytes_per_texel >
        (uint64_t)rec->data_size) {
        return GEX_VFX_DEC_ERR_RANGE;
    }

    switch (rec->format) {
    case GEX_VFX_FORMAT_ARGB_1555:
        return gex_vfx_decode_argb1555(rec->data + offset,
                                       (size_t)rec->data_size - offset, width,
                                       height, dst, dst_size);
    case GEX_VFX_FORMAT_ARGB_4444:
        return gex_vfx_decode_argb4444(rec->data + offset,
                                       (size_t)rec->data_size - offset, width,
                                       height, dst, dst_size);
    default: {
        gex_vfx_ncc_table ncc;
        if (rec->ncc_table == NULL ||
            rec->ncc_table_size < GEX_VFX_NCC_TABLE_BYTES) {
            return GEX_VFX_DEC_ERR_RANGE;
        }
        if (!gex_vfx_ncc_table_parse(rec->ncc_table, rec->ncc_table_size,
                                     &ncc)) {
            return GEX_VFX_DEC_ERR_RANGE;
        }
        return gex_vfx_decode_yiq422(rec->data + offset,
                                    (size_t)rec->data_size - offset, &ncc,
                                    width, height, dst, dst_size);
    }
    }
}

const char *gex_vfx_decode_status_name(gex_vfx_decode_status status)
{
    switch (status) {
    case GEX_VFX_DEC_OK:
        return "ok";
    case GEX_VFX_DEC_ERR_ARG:
        return "bad argument";
    case GEX_VFX_DEC_ERR_FORMAT:
        return "unsupported pixel format";
    case GEX_VFX_DEC_ERR_RANGE:
        return "truncated data or destination too small";
    case GEX_VFX_DEC_ERR_OVERFLOW:
        return "size overflow";
    }
    return "unknown";
}
