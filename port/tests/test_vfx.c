/*
 * test_vfx.c - Módulo 4A: parser del contenedor .VFX (portable).
 *
 * Cubre el parser, los límites del contenedor, las entradas truncadas, los
 * offsets inválidos, los tamaños desbordados, los formatos observados en los
 * ficheros originales y la correspondencia entre los índices de textura de
 * AZTEC16.DFX y las entradas de AZTEC16.VFX.
 *
 * Los ficheros originales se usan SOLO en lectura.
 */

#include "gex/vfx.h"
#include "gex/vfx_texture.h"
#include "ref_material.h"
#include "ref_emit_frames.h"
#include "gex/emit.h"
#include "gex/bsp.h"
#include "gex/asset/dfx.h"
#include "ref_material.h"

#include "gex/asset/dfx.h"
#include "gex/bsp.h"
#include "gex/emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AZTEC_VFX "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.VFX"
#define GEX_VFX "C:/GEX_REZ/E2_A_ORIGINAL/GEX.VFX"
#define AZTEC_DFX "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX"

static int checks;
static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

/* ------------------------------------------------------------------ */
/* Utilidades para construir VFX sintéticos                            */
/* ------------------------------------------------------------------ */

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Histograma de índices de textura (para el informe). */
static uint32_t g_index_hist[16];
static uint32_t g_index_other;

/* Escribe un registro en `buf` y devuelve su tamaño total (0x8C + datos). */
static size_t put_record(uint8_t *buf, size_t at, uint32_t a, uint32_t b,
                         uint32_t c, uint32_t format, const uint8_t *data,
                         size_t data_size)
{
    memset(buf + at, 0, GEX_VFX_RECORD_HEADER_SIZE);
    put32(buf + at + 0x00, a);
    put32(buf + at + 0x04, b);
    put32(buf + at + 0x08, c);
    put32(buf + at + 0x0C, format);
    put32(buf + at + 0x10, 0x0064FE04u); /* valor de relleno real */
    put32(buf + at + GEX_VFX_RECORD_SIZE_OFFSET, (uint32_t)data_size);
    if (data_size > 0 && data != NULL) {
        memcpy(buf + at + GEX_VFX_RECORD_DATA_OFFSET, data, data_size);
    }
    return GEX_VFX_RECORD_HEADER_SIZE + data_size;
}

/* ------------------------------------------------------------------ */
/* Nombres y argumentos                                               */
/* ------------------------------------------------------------------ */

static void test_names_and_args(void)
{
    gex_vfx_file file;

    CHECK(strcmp(gex_vfx_status_name(GEX_VFX_OK), "ok") == 0);
    CHECK(gex_vfx_status_name(GEX_VFX_ERR_ARG) != NULL);
    CHECK(gex_vfx_status_name(GEX_VFX_ERR_OPEN) != NULL);
    CHECK(gex_vfx_status_name(GEX_VFX_ERR_FORMAT) != NULL);
    CHECK(gex_vfx_status_name(GEX_VFX_ERR_RANGE) != NULL);
    CHECK(gex_vfx_status_name(GEX_VFX_ERR_OVERFLOW) != NULL);
    CHECK(gex_vfx_status_name(GEX_VFX_ERR_NOMEM) != NULL);
    CHECK(gex_vfx_status_name((gex_vfx_status)999) != NULL);

    memset(&file, 0, sizeof(file));
    CHECK(gex_vfx_parse(NULL, (const uint8_t *)"x", 1) == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_parse(&file, NULL, 1) == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_open(NULL, "x") == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_open(&file, NULL) == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_open(&file, "") == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_open(&file, "no_existe_exactamente_este_fichero.vfx") ==
          GEX_VFX_ERR_OPEN);
    /* Una cadena vacía no es un VFX válido. */
    CHECK(gex_vfx_parse(&file, (const uint8_t *)"", 0) == GEX_VFX_ERR_FORMAT);
    gex_vfx_close(&file);
    gex_vfx_close(NULL);
}

/* ------------------------------------------------------------------ */
/* Contenedor sintético                                               */
/* ------------------------------------------------------------------ */

static void test_synthetic_container(void)
{
    uint8_t buf[4 + 2 * (GEX_VFX_RECORD_HEADER_SIZE + 8)];
    gex_vfx_file file;
    static const uint8_t d0[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    static const uint8_t d1[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    size_t at = 4;

    memset(buf, 0, sizeof(buf));
    put32(buf, 2);
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_YIQ_422, d0, sizeof(d0));
    at += put_record(buf, at, 2, 2, 5, GEX_VFX_FORMAT_ARGB_4444, d1, sizeof(d1));

    CHECK(gex_vfx_parse(&file, buf, at) == GEX_VFX_OK);
    CHECK(file.declared_count == get32(buf));
    CHECK(file.declared_count == 2u);
    CHECK(file.record_count == 2u);
    CHECK(file.size == at);

    CHECK(file.records[0].small_lod == 4u);
    CHECK(file.records[0].large_lod == 4u);
    CHECK(file.records[0].aspect_ratio == 3u);
    CHECK(file.records[0].format == GEX_VFX_FORMAT_YIQ_422);
    CHECK(file.records[0].stored_pointer == 0x0064FE04u);
    CHECK(file.records[0].data_size == 4u);
    CHECK(file.records[0].data != NULL);
    CHECK(memcmp(file.records[0].data, d0, sizeof(d0)) == 0);
    CHECK(file.records[0].file_offset == 4u);
    CHECK(file.records[0].ncc_table != NULL);
    CHECK(file.records[0].ncc_table_size == 0x84u - 0x14u);

    CHECK(file.records[1].format == GEX_VFX_FORMAT_ARGB_4444);
    CHECK(file.records[1].data_size == 8u);
    CHECK(memcmp(file.records[1].data, d1, sizeof(d1)) == 0);
    CHECK(file.records[1].file_offset == 4u + GEX_VFX_RECORD_HEADER_SIZE + 4u);

    /* La tabla NCC SOLO existe para el formato YIQ_422 (1): para los formatos
     * ARGB el parser no publica ninguna tabla. */
    CHECK(file.records[0].ncc_table != NULL);
    CHECK(file.records[0].ncc_table_size == 0x84u - 0x14u);
    CHECK(file.records[1].ncc_table == NULL);
    CHECK(file.records[1].ncc_table_size == 0u);

    /* La tabla NCC solo existe para el formato YIQ_422 (1). */
    CHECK(gex_vfx_format_has_ncc_table(GEX_VFX_FORMAT_YIQ_422) == 1);
    CHECK(gex_vfx_format_has_ncc_table(GEX_VFX_FORMAT_ARGB_1555) == 0);
    CHECK(gex_vfx_format_has_ncc_table(GEX_VFX_FORMAT_ARGB_4444) == 0);

    /* Índice de textura de 14 bits. */
    CHECK(gex_vfx_find_index(&file, 0) == 0);
    CHECK(gex_vfx_find_index(&file, 1) == 1);
    CHECK(gex_vfx_find_index(&file, 2) == -1);
    CHECK(gex_vfx_find_index(&file, 0x3FFFu) == -1);
    CHECK(gex_vfx_find_index(NULL, 0) == -1);

    gex_vfx_close(&file);
    CHECK(file.data == NULL && file.records == NULL);
    gex_vfx_close(&file); /* idempotente */
}

/* ------------------------------------------------------------------ */
/* Entradas truncadas, offsets inválidos y tamaños desbordados         */
/* ------------------------------------------------------------------ */

static void test_broken_containers(void)
{
    uint8_t buf[512];
    gex_vfx_file file;
    size_t at;

    /* 1. El conteo no cabe en el fichero. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 100);
    CHECK(gex_vfx_parse(&file, buf, 8) == GEX_VFX_ERR_RANGE);

    /* 2. La cabecera del registro se sale del fichero. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 1);
    CHECK(gex_vfx_parse(&file, buf, 4 + 0x10) == GEX_VFX_ERR_RANGE);

    /* 3. El tamaño de los datos se sale del fichero. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 1);
    at = 4;
    {
        size_t rec = at;
        at += put_record(buf, rec, 4, 4, 3, GEX_VFX_FORMAT_YIQ_422,
                         (const uint8_t *)"ab", 2);
        /* Se declara un tamaño enorme. */
        put32(buf + rec + GEX_VFX_RECORD_SIZE_OFFSET, 0xFFFFFF00u);
    }
    CHECK(gex_vfx_parse(&file, buf, at) == GEX_VFX_ERR_RANGE);

    /* 4. Tamaño exactamente en el límite: el bloque termina en el EOF. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 1);
    at = 4;
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_YIQ_422, (const uint8_t *)"ab",
                     2);
    CHECK(gex_vfx_parse(&file, buf, at) == GEX_VFX_OK);
    CHECK(file.record_count == 1u);
    CHECK(file.records[0].data_size == 2u);
    gex_vfx_close(&file);

    /* 5. Fichero de menos de 4 bytes. */
    CHECK(gex_vfx_parse(&file, (const uint8_t *)"ab", 2) == GEX_VFX_ERR_FORMAT);
}

/* ------------------------------------------------------------------ */
/* Validación estricta del EOF                                         */
/* ------------------------------------------------------------------ */

static void test_trailing_bytes(void)
{
    uint8_t buf[512];
    gex_vfx_file file;
    size_t at;
    size_t i;

    /* Contenedor bien formado: el último bloque termina exactamente en el
     * EOF -> válido. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 1);
    at = 4;
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_YIQ_422, (const uint8_t *)"ab",
                     2);
    CHECK(gex_vfx_parse(&file, buf, at) == GEX_VFX_OK);
    CHECK(file.record_count == 1u);
    gex_vfx_close(&file);

    /* Con un byte de más al final: la tabla no cubre el fichero -> error. */
    for (i = 1; i <= 8u; i++) {
        memset(buf, 0, sizeof(buf));
        put32(buf, 1);
        at = 4;
        at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_YIQ_422,
                         (const uint8_t *)"ab", 2);
        memset(buf + at, 0x5A, i);
        CHECK(gex_vfx_parse(&file, buf, at + i) == GEX_VFX_ERR_FORMAT);
        CHECK(file.data == NULL && file.records == NULL);
    }

    /* Con dos registros y basura al final, también. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 2);
    at = 4;
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_YIQ_422, (const uint8_t *)"ab",
                     2);
    at += put_record(buf, at, 2, 2, 3, GEX_VFX_FORMAT_ARGB_4444, (const uint8_t *)"cd",
                     2);
    memset(buf + at, 0x33, 16u);
    CHECK(gex_vfx_parse(&file, buf, at + 16u) == GEX_VFX_ERR_FORMAT);

    /* Y un contenedor con `count` 0 y datos: también es incoherente. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 0);
    CHECK(gex_vfx_parse(&file, buf, 4) == GEX_VFX_OK);
    CHECK(file.record_count == 0u);
    gex_vfx_close(&file);
    CHECK(gex_vfx_parse(&file, buf, 8) == GEX_VFX_ERR_FORMAT);
}

/* ------------------------------------------------------------------ */
/* Ficheros originales (solo lectura)                                 */
/* ------------------------------------------------------------------ */

static int file_exists(const char *path)
{
    FILE *fh = fopen(path, "rb");
    if (fh == NULL) {
        return 0;
    }
    fclose(fh);
    return 1;
}

static void test_real_aztec(void)
{
    gex_vfx_file file;
    uint64_t offset;
    uint32_t i;
    uint32_t formats[8];
    int n_formats = 0;
    uint32_t f1 = 0, f11 = 0, f12 = 0, otros = 0;

    if (!file_exists(AZTEC_VFX)) {
        printf("skip aztec vfx (no file)\n");
        return;
    }

    CHECK(gex_vfx_open(&file, AZTEC_VFX) == GEX_VFX_OK);
    CHECK(file.declared_count == 260u);
    CHECK(file.record_count == 260u);

    /* Los bloques de datos teselan el fichero: el recorrido termina
     * exactamente en el EOF. */
    offset = 4u;
    for (i = 0; i < file.record_count; i++) {
        CHECK(file.records[i].file_offset == offset);
        CHECK(file.records[i].data ==
              file.data + offset + GEX_VFX_RECORD_DATA_OFFSET);
        offset += GEX_VFX_RECORD_HEADER_SIZE + file.records[i].data_size;
    }
    CHECK(offset == (uint64_t)file.size);

    /* Los formatos observados en el fichero original. */
    for (i = 0; i < file.record_count; i++) {
        uint32_t fmt = file.records[i].format;
        int seen = 0;
        int k;
        for (k = 0; k < n_formats; k++) {
            if (formats[k] == fmt) {
                seen = 1;
            }
        }
        if (!seen && n_formats < 8) {
            formats[n_formats++] = fmt;
        }
        if (fmt == GEX_VFX_FORMAT_YIQ_422) {
            f1++;
        } else if (fmt == GEX_VFX_FORMAT_ARGB_1555) {
            f11++;
        } else if (fmt == GEX_VFX_FORMAT_ARGB_4444) {
            f12++;
        } else {
            otros++;
        }
        /* Los tamaños de bloque son potencias de dos en los originales. */
        CHECK(file.records[i].data_size > 0u);
        CHECK((file.records[i].data_size & (file.records[i].data_size - 1u)) ==
              0u);
        /* La tabla NCC solo se publica para el formato YIQ_422 (1). */
        if (fmt == GEX_VFX_FORMAT_YIQ_422) {
            CHECK(file.records[i].ncc_table != NULL);
            CHECK(file.records[i].ncc_table_size == 0x84u - 0x14u);
        } else {
            CHECK(file.records[i].ncc_table == NULL);
            CHECK(file.records[i].ncc_table_size == 0u);
        }
    }
    printf("AZTEC16.VFX: count=%u formatos 1=%u 11=%u 12=%u otros=%u\n",
           file.record_count, f1, f11, f12, otros);
    CHECK(otros == 0u);
    CHECK(f1 > 0u && f11 > 0u && f12 > 0u);

    gex_vfx_close(&file);
}

static void test_real_gex_vfx(void)
{
    gex_vfx_file file;
    uint64_t offset;
    uint32_t i;

    if (!file_exists(GEX_VFX)) {
        printf("skip gex vfx (no file)\n");
        return;
    }
    CHECK(gex_vfx_open(&file, GEX_VFX) == GEX_VFX_OK);
    CHECK(file.declared_count == 4u);
    CHECK(file.record_count == 4u);
    offset = 4u;
    for (i = 0; i < file.record_count; i++) {
        CHECK(file.records[i].file_offset == offset);
        offset += GEX_VFX_RECORD_HEADER_SIZE + file.records[i].data_size;
    }
    CHECK(offset == (uint64_t)file.size);
    /* Las cuatro texturas globales del juego: 256 bytes cada una. */
    for (i = 0; i < file.record_count; i++) {
        CHECK(file.records[i].data_size == 256u);
        CHECK(file.records[i].format == GEX_VFX_FORMAT_YIQ_422);
    }
    gex_vfx_close(&file);
}

/* ------------------------------------------------------------------ */
/* Correspondencia DFX -> VFX                                          */
/* ------------------------------------------------------------------ */

static int visible_all(const uint8_t *image, size_t image_size,
                       uint32_t node_offset, void *ctx)
{
    (void)image;
    (void)image_size;
    (void)node_offset;
    (void)ctx;
    return 1;
}

static void test_dfx_to_vfx_correspondence(void)
{
    gex_vfx_file vfx;
    gex_dfx_container container;
    gex_dfx_level level;
    gex_bsp_walk_result leaves;
    gex_emit_poly_list polys;
    uint32_t max_index = 0;
    uint32_t distinct = 0;
    size_t i;
    int missing = 0;

    if (!file_exists(AZTEC_VFX) || !file_exists(AZTEC_DFX)) {
        printf("skip dfx/vfx correspondence (no files)\n");
        return;
    }
    CHECK(gex_vfx_open(&vfx, AZTEC_VFX) == GEX_VFX_OK);

    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_load(AZTEC_DFX, &container) == GEX_DFX_OK);
    memset(&level, 0, sizeof(level));
    CHECK(gex_dfx_load_level(&container, &level) == GEX_DFX_OK);
    memset(&leaves, 0, sizeof(leaves));
    CHECK(gex_bsp_walk(container.image, container.image_size,
                      level.descriptor.root_bsp, visible_all, NULL, NULL,
                      &leaves) == GEX_BSP_OK);
    memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(container.image, container.image_size,
                           &level.descriptor, &leaves, &polys) == GEX_EMIT_OK);
    CHECK(polys.count > 0u);

    /* Cada polígono lleva el índice de textura de 14 bits: tiene que
     * existir una entrada con ese índice en el .VFX del mismo nivel. */
    for (i = 0; i < polys.count; i++) {
        uint32_t index = polys.polys[i].texture & 0x3FFFu;
        if (index > max_index) {
            max_index = index;
        }
        if (index < 16u) {
            g_index_hist[index]++;
        } else {
            g_index_other++;
        }
        if (gex_vfx_find_index(&vfx, index) < 0) {
            missing++;
        }
    }
    printf("DFX->VFX: poligonos=%zu indice_max=%u entradas_vfx=%u\n",
           polys.count, max_index, vfx.record_count);
    for (i = 0; i < 16u; i++) {
        if (g_index_hist[i] > 0u) {
            printf("   indice %zu: %u poligonos\n", i, g_index_hist[i]);
        }
    }
    printf("   indices >= 16: %u\n", g_index_other);
    CHECK(missing == 0);
    CHECK(max_index < vfx.record_count);
    distinct = vfx.record_count;
    CHECK(distinct > 0u);

    gex_emit_poly_list_free(&polys);
    gex_bsp_walk_free(&leaves);
    gex_dfx_release_level(&level);
    gex_dfx_release_container(&container);
    gex_vfx_close(&vfx);
}

/* ------------------------------------------------------------------ */
/* Datos demostrados por formato                                        */
/* ------------------------------------------------------------------ */

static void test_format_info(void)
{
    gex_vfx_format_info info;

    /* Formato YIQ_422 (1): es el único con paleta (el juego la sube con
     * grTexDownloadTable desde +0x14) y no activa el blending alfa. */
    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_YIQ_422, &info) == 1);
    CHECK(info.format == GEX_VFX_FORMAT_YIQ_422);
    CHECK(info.has_ncc_table == 1);
    CHECK(info.enables_alpha == 0);
    CHECK(info.ncc_table_bytes == GEX_VFX_NCC_TABLE_BYTES);
    CHECK(info.ncc_table_bytes == 0x70u);

    /* Formato ARGB_1555 (11): sin tabla NCC y sin alfa. */
    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_ARGB_1555, &info) == 1);
    CHECK(info.format == GEX_VFX_FORMAT_ARGB_1555);
    CHECK(info.has_ncc_table == 0);
    CHECK(info.enables_alpha == 0);
    CHECK(info.ncc_table_bytes == 0u);

    /* Formato ARGB_4444 (12): sin tabla NCC, pero el juego activa el blending
     * alfa. */
    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_ARGB_4444, &info) == 1);
    CHECK(info.format == GEX_VFX_FORMAT_ARGB_4444);
    CHECK(info.has_ncc_table == 0);
    CHECK(info.enables_alpha == 1);
    CHECK(info.ncc_table_bytes == 0u);

    /* Formato 0: el juego lo distingue con otro modo de alfa (no aparece en
     * los .VFX originales). */
    CHECK(gex_vfx_format_info_get(0, &info) == 1);
    CHECK(info.has_ncc_table == 0);
    CHECK(info.enables_alpha == 1);

    /* Formato desconocido: no se afirma nada. */
    CHECK(gex_vfx_format_info_get(999, &info) == 0);
    CHECK(info.format == 999);
    CHECK(info.has_ncc_table == 0 && info.enables_alpha == 0);

    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_YIQ_422, NULL) == 0);

    /* Coherencia con el predicado simple. */
    CHECK(gex_vfx_format_has_ncc_table(GEX_VFX_FORMAT_YIQ_422) == 1);
    CHECK(gex_vfx_format_has_ncc_table(GEX_VFX_FORMAT_ARGB_1555) == 0);
    CHECK(gex_vfx_format_has_ncc_table(GEX_VFX_FORMAT_ARGB_4444) == 0);
}

/* ------------------------------------------------------------------ */
/* Dimensiones deducidas de los campos LOD y aspecto                    */
/* ------------------------------------------------------------------ */

/* Comprueba las dimensiones de un registro contra la tabla de tamaños de
 * Glide 2.x: el LOD fija el lado mayor y el aspecto lo divide. */
static void check_dims(uint32_t small_lod, uint32_t large_lod,
                       uint32_t aspect, uint32_t format, uint32_t exp_w,
                       uint32_t exp_h, uint32_t exp_bytes)
{
    gex_vfx_record rec;
    gex_vfx_texture_info info;

    memset(&rec, 0, sizeof(rec));
    rec.small_lod = small_lod;
    rec.large_lod = large_lod;
    rec.aspect_ratio = aspect;
    rec.format = format;

    CHECK(gex_vfx_texture_info_get(&rec, &info) == 1);
    CHECK(info.width == exp_w);
    CHECK(info.height == exp_h);
    CHECK(info.levels == (small_lod - large_lod) + 1u);
    CHECK(info.total_bytes == exp_bytes);
}

static void test_texture_dimensions(void)
{
    gex_vfx_record rec;
    gex_vfx_texture_info info;
    uint32_t w;
    uint32_t h;

    /* Formato YIQ_422: 1 byte por texel. */
    check_dims(4, 4, 3, GEX_VFX_FORMAT_YIQ_422, 16, 16, 16 * 16);
    check_dims(0, 0, 3, GEX_VFX_FORMAT_YIQ_422, 256, 256, 256 * 256);
    check_dims(0, 0, 5, GEX_VFX_FORMAT_YIQ_422, 64, 256, 64 * 256);
    check_dims(2, 2, 2, GEX_VFX_FORMAT_YIQ_422, 64, 32, 64 * 32);
    check_dims(2, 2, 4, GEX_VFX_FORMAT_YIQ_422, 32, 64, 32 * 64);
    /* Formato ARGB: 2 bytes por texel. */
    check_dims(2, 2, 3, GEX_VFX_FORMAT_ARGB_4444, 64, 64, 64 * 64 * 2);
    check_dims(1, 1, 4, GEX_VFX_FORMAT_ARGB_1555, 64, 128, 64 * 128 * 2);

    /* La cadena de mipmap: de large_lod (más detallado) a small_lod. */
    memset(&rec, 0, sizeof(rec));
    rec.small_lod = 5;
    rec.large_lod = 2;
    rec.aspect_ratio = 3;
    rec.format = GEX_VFX_FORMAT_ARGB_4444;
    CHECK(gex_vfx_texture_info_get(&rec, &info) == 1);
    CHECK(info.levels == 4u);
    CHECK(info.width == 64u && info.height == 64u);
    /* 64x64 + 32x32 + 16x16 + 8x8, a 2 bytes por texel. */
    CHECK(info.total_bytes == (64u * 64u + 32u * 32u + 16u * 16u + 8u * 8u) * 2u);
    CHECK(gex_vfx_texture_level_size(&info, 0, &w, &h) == 1);
    CHECK(w == 64u && h == 64u);
    CHECK(gex_vfx_texture_level_size(&info, 1, &w, &h) == 1);
    CHECK(w == 32u && h == 32u);
    CHECK(gex_vfx_texture_level_size(&info, 3, &w, &h) == 1);
    CHECK(w == 8u && h == 8u);
    CHECK(gex_vfx_texture_level_size(&info, 4, &w, &h) == 0);
    CHECK(gex_vfx_texture_level_offset(&info, 0) == 0u);
    CHECK(gex_vfx_texture_level_offset(&info, 1) == 64u * 64u * 2u);
    CHECK(gex_vfx_texture_level_offset(&info, 2) ==
          (64u * 64u + 32u * 32u) * 2u);

    /* Registros incoherentes. */
    memset(&rec, 0, sizeof(rec));
    rec.small_lod = 2;
    rec.large_lod = 2;
    rec.aspect_ratio = 3;
    rec.format = GEX_VFX_FORMAT_ARGB_4444;
    CHECK(gex_vfx_texture_info_get(&rec, &info) == 1);
    rec.small_lod = 9; /* LOD fuera de rango */
    CHECK(gex_vfx_texture_info_get(&rec, &info) == 0);
    rec.small_lod = 2;
    rec.aspect_ratio = 7; /* aspecto fuera de rango */
    CHECK(gex_vfx_texture_info_get(&rec, &info) == 0);
    rec.aspect_ratio = 3;
    rec.large_lod = 4; /* cadena al reves */
    rec.small_lod = 2;
    CHECK(gex_vfx_texture_info_get(&rec, &info) == 0);
    rec.format = 99; /* formato desconocido */
    rec.small_lod = 2;
    rec.large_lod = 2;
    CHECK(gex_vfx_texture_info_get(&rec, &info) == 0);
    CHECK(gex_vfx_texture_info_get(NULL, &info) == 0);
    CHECK(gex_vfx_texture_info_get(&rec, NULL) == 0);
}

/* ------------------------------------------------------------------ */
/* Tabla NCC: interpretacion y forma empaquetada                        */
/* ------------------------------------------------------------------ */

static void test_ncc_table(void)
{
    uint8_t raw[GEX_VFX_NCC_TABLE_BYTES];
    gex_vfx_ncc_table table;
    uint32_t packed[12];
    uint32_t i;

    /* Una tabla sintética: luminancia 0, 17, 34 ... y croma con signo. */
    memset(raw, 0, sizeof(raw));
    for (i = 0; i < 16u; i++) {
        raw[i] = (uint8_t)(i * 17u);
    }
    /* iRGB[0] = (0x1E2, 0x1E8, 0x04B) -> con signo: -30, -24, 75 */
    raw[16] = 0xE2; raw[17] = 0x01;
    raw[18] = 0xE8; raw[19] = 0x01;
    raw[20] = 0x4B; raw[21] = 0x00;
    /* qRGB[0] = (0x1F4, 0x1F5, 0x032) -> -12, -11, 50 */
    raw[40] = 0xF4; raw[41] = 0x01;
    raw[42] = 0xF5; raw[43] = 0x01;
    raw[44] = 0x32; raw[45] = 0x00;

    CHECK(gex_vfx_ncc_table_parse(raw, sizeof(raw), &table) == 1);
    CHECK(table.y_rgb[0] == 0u && table.y_rgb[1] == 17u && table.y_rgb[15] == 255u);
    CHECK(table.i_rgb[0][0] == -30);
    CHECK(table.i_rgb[0][1] == -24);
    CHECK(table.i_rgb[0][2] == 75);
    CHECK(table.q_rgb[0][0] == -12);
    CHECK(table.q_rgb[0][1] == -11);
    CHECK(table.q_rgb[0][2] == 50);

    /* La forma empaquetada: 4 palabras de luminancia y 4+4 de croma con
     * campos de 9 bits. */
    gex_vfx_ncc_table_pack(&table, packed);
    CHECK(packed[0] == 0x33221100u);
    CHECK(packed[4] == ((0x1E2u << 18) | (0x1E8u << 9) | 0x04Bu));
    CHECK(packed[8] == ((0x1F4u << 18) | (0x1F5u << 9) | 0x032u));

    /* Con la tabla ya empaquetada en el bloque, el round trip es exacto. */
    memcpy(raw + 64, packed, 48);
    {
        gex_vfx_ncc_table again;
        uint32_t packed2[12];
        CHECK(gex_vfx_ncc_table_parse(raw, sizeof(raw), &again) == 1);
        gex_vfx_ncc_table_pack(&again, packed2);
        for (i = 0; i < 12u; i++) {
            CHECK(packed2[i] == packed[i]);
            CHECK(again.packed_data[i] == packed[i]);
        }
    }

    /* Argumentos y tamaños inválidos. */
    CHECK(gex_vfx_ncc_table_parse(NULL, sizeof(raw), &table) == 0);
    CHECK(gex_vfx_ncc_table_parse(raw, sizeof(raw), NULL) == 0);
    CHECK(gex_vfx_ncc_table_parse(raw, GEX_VFX_NCC_TABLE_BYTES - 1u, &table) == 0);
    gex_vfx_ncc_table_pack(NULL, packed);
}

/* ------------------------------------------------------------------ */
/* Decodificadores CPU                                                  */
/* ------------------------------------------------------------------ */

/* ARGB_1555: A(1) R(5) G(5) B(5), palabra little-endian. */
static void test_decode_argb1555(void)
{
    uint8_t src[8];
    uint8_t dst[16];

    /* 0xFFFF = A=1, R=31, G=31, B=31 -> blanco opaco. */
    src[0] = 0xFF; src[1] = 0xFF;
    /* 0x8000 = A=1, R=0, G=0, B=0 -> negro opaco. */
    src[2] = 0x00; src[3] = 0x80;
    /* 0x7C00 = A=0, R=31, G=0, B=0 -> rojo transparente. */
    src[4] = 0x00; src[5] = 0x7C;
    /* 0xFC1F = A=1, R=31, G=0, B=31 -> magenta opaco. */
    src[6] = 0x1F; src[7] = 0xFC;

    CHECK(gex_vfx_decode_argb1555(src, sizeof(src), 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_OK);
    CHECK(dst[0] == 255u && dst[1] == 255u && dst[2] == 255u && dst[3] == 255u);
    CHECK(dst[4] == 0u && dst[5] == 0u && dst[6] == 0u && dst[7] == 255u);
    CHECK(dst[8] == 255u && dst[9] == 0u && dst[10] == 0u && dst[11] == 0u);
    CHECK(dst[12] == 255u && dst[13] == 0u && dst[14] == 255u && dst[15] == 255u);

    /* Errores: datos truncados, destino pequeño, punteros nulos. */
    CHECK(gex_vfx_decode_argb1555(src, 7u, 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_RANGE);
    CHECK(gex_vfx_decode_argb1555(src, sizeof(src), 4, 1, dst, 15u) ==
          GEX_VFX_DEC_ERR_RANGE);
    CHECK(gex_vfx_decode_argb1555(NULL, sizeof(src), 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    CHECK(gex_vfx_decode_argb1555(src, sizeof(src), 4, 1, NULL, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    CHECK(gex_vfx_decode_argb1555(src, sizeof(src), 0, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    CHECK(gex_vfx_decode_argb1555(src, sizeof(src), 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_OK);
}

/* ARGB_4444: A(4) R(4) G(4) B(4). */
static void test_decode_argb4444(void)
{
    uint8_t src[8];
    uint8_t dst[16];

    src[0] = 0xFF; src[1] = 0xFF; /* A=F R=F G=F B=F -> blanco opaco */
    src[2] = 0x00; src[3] = 0xF0; /* A=F R=0 G=0 B=0 -> negro opaco */
    src[4] = 0xF0; src[5] = 0x0F; /* A=0 R=F G=F B=0 -> amarillo transparente */
    src[6] = 0x8F; src[7] = 0xF8; /* A=F R=8 G=8 B=F -> magenta opaco */

    CHECK(gex_vfx_decode_argb4444(src, sizeof(src), 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_OK);
    CHECK(dst[0] == 255u && dst[1] == 255u && dst[2] == 255u && dst[3] == 255u);
    CHECK(dst[4] == 0u && dst[5] == 0u && dst[6] == 0u && dst[7] == 255u);
    CHECK(dst[8] == 255u && dst[9] == 255u && dst[10] == 0u && dst[11] == 0u);
    CHECK(dst[12] == 0x88u && dst[13] == 0x88u && dst[14] == 255u &&
          dst[15] == 255u);

    CHECK(gex_vfx_decode_argb4444(src, 7u, 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_RANGE);
    CHECK(gex_vfx_decode_argb4444(src, sizeof(src), 4, 1, dst, 15u) ==
          GEX_VFX_DEC_ERR_RANGE);
    CHECK(gex_vfx_decode_argb4444(NULL, sizeof(src), 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    CHECK(gex_vfx_decode_argb4444(src, sizeof(src), 4, 1, NULL, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
}

/* YIQ_422 con la tabla NCC: el byte es un índice de la paleta de 256. */
static void test_decode_yiq422(void)
{
    gex_vfx_ncc_table ncc;
    uint8_t src[4];
    uint8_t dst[16];

    memset(&ncc, 0, sizeof(ncc));
    /* Luminancia: y = 100. Croma I[0] = (10, 0, -10), Q[0] = (0, 0, 0). */
    ncc.y_rgb[0] = 100u;
    ncc.i_rgb[0][0] = 10;
    ncc.i_rgb[0][1] = 0;
    ncc.i_rgb[0][2] = -10;

    /* Índice 0x00 -> iy=0, ia=0, ib=0 -> (110, 100, 90). */
    src[0] = 0x00;
    /* Índice 0x04 -> iy=0, ia=1, ib=0 -> croma I[1] = (0,0,0) -> (100,100,100) */
    src[1] = 0x04;
    /* Índice 0xF3 -> iy=15, ia=0, ib=3 -> y=0 (no definido -> 0) + (10,0,-10) */
    src[2] = 0xF3;
    /* Índice 0x10 -> iy=1 (y=0), ia=0, ib=0 -> (10, 0, -10) saturado a 0 */
    src[3] = 0x10;

    CHECK(gex_vfx_decode_yiq422(src, sizeof(src), &ncc, 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_OK);
    CHECK(dst[0] == 110u && dst[1] == 100u && dst[2] == 90u && dst[3] == 255u);
    CHECK(dst[4] == 100u && dst[5] == 100u && dst[6] == 100u && dst[7] == 255u);
    CHECK(dst[8] == 10u && dst[9] == 0u && dst[10] == 0u && dst[11] == 255u);
    CHECK(dst[12] == 10u && dst[13] == 0u && dst[14] == 0u && dst[15] == 255u);

    CHECK(gex_vfx_decode_yiq422(src, 3u, &ncc, 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_RANGE);
    CHECK(gex_vfx_decode_yiq422(src, sizeof(src), &ncc, 4, 1, dst, 15u) ==
          GEX_VFX_DEC_ERR_RANGE);
    CHECK(gex_vfx_decode_yiq422(src, sizeof(src), NULL, 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    CHECK(gex_vfx_decode_yiq422(NULL, sizeof(src), &ncc, 4, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
}

/* Decodificación a través del registro: despacha por formato. */
static void test_decode_record(void)
{
    uint8_t buf[4 + GEX_VFX_RECORD_HEADER_SIZE + 16];
    gex_vfx_file file;
    gex_vfx_texture_info info;
    uint8_t dst[4 * 4 * 4];
    uint8_t payload[16];
    size_t at = 4;

    /* Dos pixeles ARGB_4444 y dos ARGB_1555, todos con LOD 0 (256x256) no
     * caben: usamos un registro con tamano de datos coherente con LOD 0 y
     * aspecto 3 son 65536 texels, asi que aqui solo se comprueba el
     * despacho y los errores de tamano. */
    memset(payload, 0, sizeof(payload));
    payload[0] = 0xFF;
    payload[1] = 0xFF;
    payload[2] = 0x00;
    payload[3] = 0x80;

    at += put_record(buf, at, 0, 0, 3, GEX_VFX_FORMAT_ARGB_4444, payload,
                     sizeof(payload));
    memset(buf, 0, sizeof(buf));
    put32(buf + 0, 1);
    put32(buf + 4 + 0x00, 0);          /* smallLod */
    put32(buf + 4 + 0x04, 0);          /* largeLod */
    put32(buf + 4 + 0x08, 3);          /* aspect 1x1 */
    put32(buf + 4 + 0x0C, GEX_VFX_FORMAT_ARGB_4444);
    put32(buf + 4 + GEX_VFX_RECORD_SIZE_OFFSET, (uint32_t)sizeof(payload));
    memcpy(buf + 4 + GEX_VFX_RECORD_DATA_OFFSET, payload, sizeof(payload));

    CHECK(gex_vfx_parse(&file, buf, 4 + GEX_VFX_RECORD_HEADER_SIZE +
                                  sizeof(payload)) == GEX_VFX_OK);
    CHECK(gex_vfx_texture_info_get(&file.records[0], &info) == 1);
    CHECK(info.width == 256u && info.height == 256u);
    CHECK(info.total_bytes == 256u * 256u * 2u);
    /* Los datos son mas cortos que el nivel: la decodificacion falla. */
    CHECK(gex_vfx_decode_level(&file.records[0], &info, 0, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_RANGE);
    CHECK(gex_vfx_decode_level(&file.records[0], &info, 1, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    CHECK(gex_vfx_decode_level(NULL, &info, 0, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    CHECK(gex_vfx_decode_level(&file.records[0], NULL, 0, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_ARG);
    gex_vfx_close(&file);

    /* Un formato sin decodificador devuelve GEX_VFX_DEC_ERR_FORMAT. */
    memset(buf, 0, sizeof(buf));
    put32(buf + 0, 1);
    put32(buf + 4 + 0x0C, GEX_VFX_FORMAT_RGB_332);
    put32(buf + 4 + GEX_VFX_RECORD_SIZE_OFFSET, 4);
    CHECK(gex_vfx_parse(&file, buf, 4 + GEX_VFX_RECORD_HEADER_SIZE + 4) ==
          GEX_VFX_OK);
    CHECK(gex_vfx_texture_info_get(&file.records[0], &info) == 1);
    CHECK(info.bytes_per_texel == 1u);
    CHECK(gex_vfx_decode_level(&file.records[0], &info, 0, dst, sizeof(dst)) ==
          GEX_VFX_DEC_ERR_FORMAT);
    gex_vfx_close(&file);

    CHECK(gex_vfx_decode_status_name(GEX_VFX_DEC_OK) != NULL);
    CHECK(gex_vfx_decode_status_name(GEX_VFX_DEC_ERR_ARG) != NULL);
    CHECK(gex_vfx_decode_status_name(GEX_VFX_DEC_ERR_FORMAT) != NULL);
    CHECK(gex_vfx_decode_status_name(GEX_VFX_DEC_ERR_RANGE) != NULL);
    CHECK(gex_vfx_decode_status_name(GEX_VFX_DEC_ERR_OVERFLOW) != NULL);
    CHECK(gex_vfx_decode_status_name((gex_vfx_decode_status)999) != NULL);
}

/* ------------------------------------------------------------------ */
/* Decodificacion de los ficheros originales                            */
/* ------------------------------------------------------------------ */

static void test_decode_originals(void)
{
    gex_vfx_file file;
    gex_vfx_texture_info info;
    uint32_t i;
    uint32_t decoded = 0;
    uint32_t mismatched = 0;

    if (!file_exists(AZTEC_VFX)) {
        printf("skip decode originals (no file)\n");
        return;
    }
    CHECK(gex_vfx_open(&file, AZTEC_VFX) == GEX_VFX_OK);

    /* El tamano del bloque de datos de cada registro coincide con el que
     * predicen las reglas de Glide 2.x (LOD, aspecto y bytes por texel). */
    for (i = 0; i < file.record_count; i++) {
        const gex_vfx_record *rec = &file.records[i];
        if (!gex_vfx_texture_info_get(rec, &info)) {
            mismatched++;
            continue;
        }
        if (info.total_bytes != rec->data_size) {
            mismatched++;
        }
    }
    CHECK(mismatched == 0u);

    /* Y la decodificacion del primer registro de cada formato funciona y es
     * determinista. */
    for (i = 0; i < file.record_count && decoded < 3u; i++) {
        const gex_vfx_record *rec = &file.records[i];
        uint8_t *a;
        uint8_t *b;
        gex_vfx_decode_status st;

        if (rec->format != GEX_VFX_FORMAT_ARGB_4444 &&
            rec->format != GEX_VFX_FORMAT_ARGB_1555 &&
            rec->format != GEX_VFX_FORMAT_YIQ_422) {
            continue;
        }
        if (!gex_vfx_texture_info_get(rec, &info)) {
            continue;
        }
        a = (uint8_t *)calloc((size_t)info.width * info.height * 4u, 1);
        b = (uint8_t *)calloc((size_t)info.width * info.height * 4u, 1);
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            CHECK(0);
            return;
        }
        st = gex_vfx_decode_level(rec, &info, 0, a,
                                  (size_t)info.width * info.height * 4u);
        CHECK(st == GEX_VFX_DEC_OK);
        CHECK(gex_vfx_decode_level(rec, &info, 0, b,
                                   (size_t)info.width * info.height * 4u) ==
              GEX_VFX_DEC_OK);
        CHECK(memcmp(a, b, (size_t)info.width * info.height * 4u) == 0);
        /* El formato ARGB_4444 tiene alfa de 4 bits: tiene que haber
         * píxeles opacos y, si la textura los usa, transparentes. */
        if (rec->format == GEX_VFX_FORMAT_ARGB_4444) {
            size_t n = (size_t)info.width * info.height;
            size_t k;
            int opaque = 0;
            int transparent = 0;
            for (k = 0; k < n; k++) {
                if (a[k * 4 + 3] == 255u) {
                    opaque++;
                } else if (a[k * 4 + 3] == 0u) {
                    transparent++;
                }
            }
            CHECK(opaque > 0);
            (void)transparent;
        }
        free(a);
        free(b);
        decoded++;
    }
    CHECK(decoded == 3u);
    gex_vfx_close(&file);
}

/* ------------------------------------------------------------------ */
/* Tabla NCC compartida entre entradas                                     */
/* ------------------------------------------------------------------ */

static void test_ncc_table_twins(void)
{
    gex_vfx_file file;
    uint32_t i;
    int pairs = 0;

    if (!file_exists(AZTEC_VFX)) {
        printf("skip ncc table twins (no file)\n");
        return;
    }
    CHECK(gex_vfx_open(&file, AZTEC_VFX) == GEX_VFX_OK);

    /* Solo las entradas de formato YIQ_422 tienen tabla NCC, y su bloque no está a
     * cero; las de formato 11 y 12 lo tienen a cero (dato medido en los
     * 37 .VFX del inventario). */
    for (i = 0; i < file.record_count; i++) {
        const gex_vfx_record *rec = &file.records[i];
        if (rec->format == GEX_VFX_FORMAT_YIQ_422) {
            CHECK(rec->ncc_table != NULL);
            CHECK(rec->ncc_table_size == GEX_VFX_NCC_TABLE_BYTES);
        } else {
            CHECK(rec->ncc_table == NULL);
            CHECK(rec->ncc_table_size == 0u);
        }
    }

    /* La forma empaquetada que trae el fichero tiene que coincidir con la
     * que se recalcula desde los campos crudos: es la prueba de que el
     * bloque de 112 B es una `GuNccTable` de Glide 2.x y no otra cosa. */
    for (i = 0; i < file.record_count; i++) {
        const gex_vfx_record *rec = &file.records[i];
        gex_vfx_ncc_table table;
        uint32_t packed[12];
        uint32_t k;

        if (rec->format != GEX_VFX_FORMAT_YIQ_422) {
            continue;
        }
        CHECK(gex_vfx_ncc_table_parse(rec->ncc_table, rec->ncc_table_size,
                                      &table) == 1);
        gex_vfx_ncc_table_pack(&table, packed);
        for (k = 0; k < 12u; k++) {
            CHECK(packed[k] == table.packed_data[k]);
        }
        /* La luminancia de la tabla es una rampa creciente. */
        CHECK(table.y_rgb[0] <= table.y_rgb[15]);
    }

    /* El juego copia el bloque de tabla NCC entre dos entradas que la
     * comparten (memcpy de 0x70 B en 0x004732FB): en AZTEC16.VFX hay un par
     * (entradas 25 y 83) con tablas NCC idénticas. */
    for (i = 0; i < file.record_count; i++) {
        int twin = gex_vfx_ncc_table_twin(&file, i);
        if (twin >= 0) {
            pairs++;
            CHECK(file.records[twin].ncc_table != NULL);
            CHECK(memcmp(file.records[twin].ncc_table, file.records[i].ncc_table,
                         GEX_VFX_NCC_TABLE_BYTES) == 0);
            /* Y la relación es simétrica. */
            CHECK(gex_vfx_ncc_table_twin(&file, (uint32_t)twin) == (int)i);
        }
    }
    printf("AZTEC16.VFX: entradas con tabla NCC compartida: %d\n", pairs);

    CHECK(gex_vfx_ncc_table_twin(&file, file.record_count) == -1);
    CHECK(gex_vfx_ncc_table_twin(NULL, 0) == -1);
    gex_vfx_close(&file);
}

/* ------------------------------------------------------------------ */
/* Banco de texturas decodificadas (Módulo 4C)                          */
/* ------------------------------------------------------------------ */

static void test_texture_bank(void)
{
    gex_vfx_texture_bank bank;
    uint32_t wanted[4];
    const gex_vfx_texture *tex;
    int i;
    int distinct_colors = 0;

    /* Argumentos inválidos. */
    memset(&bank, 0, sizeof(bank));
    CHECK(gex_vfx_texture_bank_load(NULL, GEX_VFX, wanted, 4) == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_texture_bank_load(&bank, NULL, wanted, 4) == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_texture_bank_load(&bank, GEX_VFX, NULL, 4) == GEX_VFX_ERR_ARG);
    CHECK(gex_vfx_texture_bank_load(&bank, "no_existe_exactamente.vfx", wanted,
                                    4) != GEX_VFX_OK);
    gex_vfx_texture_bank_free(&bank);
    gex_vfx_texture_bank_free(NULL);

    if (!file_exists(AZTEC_VFX)) {
        printf("skip texture bank (no file)\n");
        return;
    }

    /* Las cuatro texturas que usa la geometría del nivel (índices 0..3). */
    wanted[0] = 0;
    wanted[1] = 1;
    wanted[2] = 2;
    wanted[3] = 3;
    memset(&bank, 0, sizeof(bank));
    CHECK(gex_vfx_texture_bank_load(&bank, AZTEC_VFX, wanted, 4) == GEX_VFX_OK);
    CHECK(bank.texture_count == 4u);
    CHECK(bank.decoded_count == 4u);
    CHECK(bank.failed_count == 0u);
    CHECK(bank.max_width == 64u);
    CHECK(bank.max_height == 64u);

    /* Cada índice de 14 bits apunta a su textura, con sus dimensiones
     * originales (el `GrTexInfo` del `.VFX`). */
    for (i = 0; i < 4; i++) {
        tex = gex_vfx_texture_bank_find(&bank, (uint32_t)i);
        CHECK(tex != NULL);
        if (tex == NULL) {
            continue;
        }
        CHECK(tex->index == (uint32_t)i);
        CHECK(tex->rgba != NULL);
        CHECK(tex->width == 64u);
        CHECK(tex->height == 32u || tex->height == 64u);
        CHECK(gex_vfx_texture_bank_slot(&bank, (uint32_t)i) >= 0);
    }

    /* Los índices no pedidos no tienen textura (no se inventa nada). */
    CHECK(gex_vfx_texture_bank_find(&bank, 4u) == NULL);
    CHECK(gex_vfx_texture_bank_find(&bank, 259u) == NULL);
    CHECK(gex_vfx_texture_bank_find(&bank, 16383u) == NULL);
    CHECK(gex_vfx_texture_bank_slot(&bank, 4u) == -1);
    CHECK(gex_vfx_texture_bank_slot(&bank, 16383u) == -1);
    CHECK(gex_vfx_texture_bank_find(NULL, 0u) == NULL);

    /* El contenido decodificado es una imagen de verdad: la textura 0 del
     * nivel (ARGB_4444) no es un color liso y su alfa es el del formato. */
    tex = gex_vfx_texture_bank_find(&bank, 0u);
    if (tex != NULL && tex->rgba != NULL) {
        uint32_t first = ((uint32_t)tex->rgba[0] << 24) |
                         ((uint32_t)tex->rgba[1] << 16) |
                         ((uint32_t)tex->rgba[2] << 8) |
                         (uint32_t)tex->rgba[3];
        uint32_t k;
        for (k = 0; k < tex->width * tex->height; k++) {
            uint32_t c = ((uint32_t)tex->rgba[k * 4 + 0] << 24) |
                         ((uint32_t)tex->rgba[k * 4 + 1] << 16) |
                         ((uint32_t)tex->rgba[k * 4 + 2] << 8) |
                         (uint32_t)tex->rgba[k * 4 + 3];
            if (c != first) {
                distinct_colors++;
                break;
            }
        }
        CHECK(distinct_colors > 0);
    }

    gex_vfx_texture_bank_free(&bank);
    /* Liberar dos veces no debe romper nada. */
    gex_vfx_texture_bank_free(&bank);
    CHECK(bank.texture_count == 0u);
}


/* ---------------------------------------------------------------------------
 * Las texturas que el original MEZCLA con alfa son las de ARGB_4444.
 *
 * El consumidor de comandos (0x46558A) decide el modo de mezcla por comando
 * comparando el campo +0x0C del registro de textura en tiempo de ejecucion
 * (el formato) con 0x0C (ARGB_4444: trae alfa) y con 0 (opaco). Capturado del
 * GEX3D.EXE en ejecucion con el escenario map7.dfx, las seis texturas que el
 * original mezcla con alfa (indices 6, 8, 185, 188, 234 y 241) son
 * exactamente las de formato ARGB_4444 del `map7.vfx`: el juego convierte el
 * YIQ_422 y el ARGB_1555 a un formato opaco de 8 bits.
 *
 * La prueba NO se limita a comprobar seis indices fijos: obtiene los indices
 * de textura REALMENTE usados por la geometria de MAP7 (recorriendo el BSP de
 * referencia y emitiendo el nivel) y exige que:
 *   - el conjunto de texturas usadas con formato ARGB_4444 sea exactamente el
 *     de la referencia (ni una mas, ni una menos);
 *   - cada indice de la referencia se use de verdad y sea ARGB_4444.
 *
 * Si los ficheros originales no estan, la prueba se salta (no pasa en
 * vacio).
 * ------------------------------------------------------------------------- */
#define MAP7_VFX_PATH "C:/GEX_REZ/GEX2/level/map7.vfx"
#define MAP7_DFX_PATH "C:/GEX_REZ/GEX2/level/map7.dfx"

static void test_map7_alpha_textures(void)
{
    gex_vfx_file vfx;
    gex_dfx_container container;
    gex_dfx_level level;
    gex_emit_matrix mat;
    gex_emit_fog fog;
    gex_emit_output out;
    gex_bsp_walk_result leaves;
    const int32_t *p = REF_EMIT_PARAMS[0];
    uint32_t used[0x4000];
    uint32_t i;
    uint32_t n_used = 0;
    uint32_t n_alpha_used = 0;
    uint32_t found = 0;
    FILE *f;

    f = fopen(MAP7_VFX_PATH, "rb");
    if (!f) {
        printf("skip map7 alpha textures (no .vfx)\n");
        return;
    }
    fclose(f);
    f = fopen(MAP7_DFX_PATH, "rb");
    if (!f) {
        printf("skip map7 alpha textures (no .dfx)\n");
        return;
    }
    fclose(f);

    memset(&vfx, 0, sizeof(vfx));
    memset(&container, 0, sizeof(container));
    memset(&level, 0, sizeof(level));
    memset(&mat, 0, sizeof(mat));
    memset(&fog, 0, sizeof(fog));
    memset(&out, 0, sizeof(out));
    memset(&leaves, 0, sizeof(leaves));
    memset(used, 0, sizeof(used));

    if (gex_vfx_open(&vfx, MAP7_VFX_PATH) != GEX_VFX_OK) {
        printf("skip map7 alpha textures (open .vfx)\n");
        return;
    }
    if (gex_dfx_load(MAP7_DFX_PATH, &container) != GEX_DFX_OK) {
        printf("skip map7 alpha textures (load .dfx)\n");
        gex_vfx_close(&vfx);
        return;
    }
    if (gex_dfx_load_level(&container, &level) != GEX_DFX_OK) {
        printf("skip map7 alpha textures (level)\n");
        gex_dfx_release_container(&container);
        gex_vfx_close(&vfx);
        return;
    }

    /* Emision del nivel con la camara de referencia: de aqui salen los
     * indices de textura que la geometria usa de verdad. */
    memcpy(mat.m, REF_EMIT_M[0], sizeof(mat.m));
    mat.t[0] = REF_EMIT_T[0][0];
    mat.t[1] = REF_EMIT_T[0][1];
    mat.t[2] = REF_EMIT_T[0][2];
    mat.focal = p[0]; mat.cx = p[1]; mat.cy = p[2]; mat.near_z = p[3];
    memcpy(fog.lut, REF_FOG_LUT, sizeof(fog.lut));
    fog.have_lut = 1;
    fog.threshold = p[4]; fog.split_z = p[5];
    fog.a_q12 = p[6]; fog.b_q12 = p[7];
    leaves.leaves = (uint32_t *)REF_EMIT_LEAVES[0];
    leaves.count = (uint32_t)p[11];
    if (gex_level_emit(container.image, container.image_size, &level.descriptor,
                       &leaves, &mat, &fog, 0, &out) != GEX_EMIT_OK) {
        CHECK(0);
        gex_emit_output_free(&out);
        gex_dfx_release_level(&level);
        gex_dfx_release_container(&container);
        gex_vfx_close(&vfx);
        return;
    }
    for (i = 0; i < out.count; i++) {
        uint32_t t = out.tris[i].texture;
        if (t < 0x4000u && used[t] == 0u) {
            used[t] = 1u;
            n_used++;
        }
    }
    gex_emit_output_free(&out);

    /* Comprobacion 1: toda textura usada con formato ARGB_4444 esta en la
     * referencia, y toda referencia se usa y es ARGB_4444. */
    for (i = 0; i < vfx.record_count; i++) {
        int in_ref = 0;
        uint32_t k;
        int is_alpha = (vfx.records[i].format == GEX_VFX_FORMAT_ARGB_4444);
        for (k = 0; k < sizeof(REF_ALPHA_TEXTURES) / sizeof(REF_ALPHA_TEXTURES[0]); k++) {
            if (REF_ALPHA_TEXTURES[k] == i) in_ref = 1;
        }
        if (!is_alpha) {
            CHECK(!in_ref); /* la referencia dice que trae alfa y no lo trae */
            continue;
        }
        if (in_ref) {
            CHECK(used[i] == 1u); /* la referencia dice que se usa y no */
            found++;
        }
        if (used[i]) {
            CHECK(in_ref); /* se usa con alfa y no esta en la referencia */
            n_alpha_used++;
        }
    }
    CHECK(found == sizeof(REF_ALPHA_TEXTURES) / sizeof(REF_ALPHA_TEXTURES[0]));
    CHECK(n_alpha_used == found);

    printf("map7: %u texturas usadas por la geometria, %u de ellas con "
           "formato ARGB_4444 (las que el original mezcla con alfa)\n",
           n_used, n_alpha_used);

    gex_dfx_release_level(&level);
    gex_dfx_release_container(&container);
    gex_vfx_close(&vfx);
}

int main(void)
{
    test_names_and_args();
    test_synthetic_container();
    test_broken_containers();
    test_trailing_bytes();
    test_format_info();
    test_texture_dimensions();
    test_ncc_table();
    test_decode_argb1555();
    test_decode_argb4444();
    test_decode_yiq422();
    test_decode_record();
    test_decode_originals();
    test_texture_bank();
    test_map7_alpha_textures();
    test_ncc_table_twins();
    test_real_aztec();
    test_real_gex_vfx();
    test_dfx_to_vfx_correspondence();

    printf("test_vfx: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
