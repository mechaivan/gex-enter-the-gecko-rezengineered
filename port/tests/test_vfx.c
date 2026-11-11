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
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_1, d0, sizeof(d0));
    at += put_record(buf, at, 2, 2, 5, GEX_VFX_FORMAT_12, d1, sizeof(d1));

    CHECK(gex_vfx_parse(&file, buf, at) == GEX_VFX_OK);
    CHECK(file.declared_count == get32(buf));
    CHECK(file.declared_count == 2u);
    CHECK(file.record_count == 2u);
    CHECK(file.size == at);

    CHECK(file.records[0].field_a == 4u);
    CHECK(file.records[0].field_b == 4u);
    CHECK(file.records[0].field_c == 3u);
    CHECK(file.records[0].format == GEX_VFX_FORMAT_1);
    CHECK(file.records[0].stored_pointer == 0x0064FE04u);
    CHECK(file.records[0].data_size == 4u);
    CHECK(file.records[0].data != NULL);
    CHECK(memcmp(file.records[0].data, d0, sizeof(d0)) == 0);
    CHECK(file.records[0].file_offset == 4u);
    CHECK(file.records[0].palette != NULL);
    CHECK(file.records[0].palette_size == 0x84u - 0x14u);

    CHECK(file.records[1].format == GEX_VFX_FORMAT_12);
    CHECK(file.records[1].data_size == 8u);
    CHECK(memcmp(file.records[1].data, d1, sizeof(d1)) == 0);
    CHECK(file.records[1].file_offset == 4u + GEX_VFX_RECORD_HEADER_SIZE + 4u);

    /* La tabla de paleta SOLO existe para el formato 1: para los formatos 11
     * y 12 el parser no publica ninguna paleta. */
    CHECK(file.records[0].palette != NULL);
    CHECK(file.records[0].palette_size == 0x84u - 0x14u);
    CHECK(file.records[1].palette == NULL);
    CHECK(file.records[1].palette_size == 0u);

    /* La tabla de paleta solo existe para el formato 1. */
    CHECK(gex_vfx_format_has_palette(GEX_VFX_FORMAT_1) == 1);
    CHECK(gex_vfx_format_has_palette(GEX_VFX_FORMAT_11) == 0);
    CHECK(gex_vfx_format_has_palette(GEX_VFX_FORMAT_12) == 0);

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
        at += put_record(buf, rec, 4, 4, 3, GEX_VFX_FORMAT_1,
                         (const uint8_t *)"ab", 2);
        /* Se declara un tamaño enorme. */
        put32(buf + rec + GEX_VFX_RECORD_SIZE_OFFSET, 0xFFFFFF00u);
    }
    CHECK(gex_vfx_parse(&file, buf, at) == GEX_VFX_ERR_RANGE);

    /* 4. Tamaño exactamente en el límite: el bloque termina en el EOF. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 1);
    at = 4;
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_1, (const uint8_t *)"ab",
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
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_1, (const uint8_t *)"ab",
                     2);
    CHECK(gex_vfx_parse(&file, buf, at) == GEX_VFX_OK);
    CHECK(file.record_count == 1u);
    gex_vfx_close(&file);

    /* Con un byte de más al final: la tabla no cubre el fichero -> error. */
    for (i = 1; i <= 8u; i++) {
        memset(buf, 0, sizeof(buf));
        put32(buf, 1);
        at = 4;
        at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_1,
                         (const uint8_t *)"ab", 2);
        memset(buf + at, 0x5A, i);
        CHECK(gex_vfx_parse(&file, buf, at + i) == GEX_VFX_ERR_FORMAT);
        CHECK(file.data == NULL && file.records == NULL);
    }

    /* Con dos registros y basura al final, también. */
    memset(buf, 0, sizeof(buf));
    put32(buf, 2);
    at = 4;
    at += put_record(buf, at, 4, 4, 3, GEX_VFX_FORMAT_1, (const uint8_t *)"ab",
                     2);
    at += put_record(buf, at, 2, 2, 3, GEX_VFX_FORMAT_12, (const uint8_t *)"cd",
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
        if (fmt == GEX_VFX_FORMAT_1) {
            f1++;
        } else if (fmt == GEX_VFX_FORMAT_11) {
            f11++;
        } else if (fmt == GEX_VFX_FORMAT_12) {
            f12++;
        } else {
            otros++;
        }
        /* Los tamaños de bloque son potencias de dos en los originales. */
        CHECK(file.records[i].data_size > 0u);
        CHECK((file.records[i].data_size & (file.records[i].data_size - 1u)) ==
              0u);
        /* La paleta solo se publica para el formato 1. */
        if (fmt == GEX_VFX_FORMAT_1) {
            CHECK(file.records[i].palette != NULL);
            CHECK(file.records[i].palette_size == 0x84u - 0x14u);
        } else {
            CHECK(file.records[i].palette == NULL);
            CHECK(file.records[i].palette_size == 0u);
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
        CHECK(file.records[i].format == GEX_VFX_FORMAT_1);
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

    /* Formato 1: es el único con paleta (el juego la sube con
     * grTexDownloadTable desde +0x14) y no activa el blending alfa. */
    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_1, &info) == 1);
    CHECK(info.format == GEX_VFX_FORMAT_1);
    CHECK(info.has_palette == 1);
    CHECK(info.enables_alpha == 0);
    CHECK(info.palette_bytes == GEX_VFX_PALETTE_BYTES);
    CHECK(info.palette_bytes == 0x70u);

    /* Formato 11: sin paleta y sin alfa. */
    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_11, &info) == 1);
    CHECK(info.format == GEX_VFX_FORMAT_11);
    CHECK(info.has_palette == 0);
    CHECK(info.enables_alpha == 0);
    CHECK(info.palette_bytes == 0u);

    /* Formato 12: sin paleta, pero el juego activa el blending alfa. */
    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_12, &info) == 1);
    CHECK(info.format == GEX_VFX_FORMAT_12);
    CHECK(info.has_palette == 0);
    CHECK(info.enables_alpha == 1);
    CHECK(info.palette_bytes == 0u);

    /* Formato 0: el juego lo distingue con otro modo de alfa (no aparece en
     * los .VFX originales). */
    CHECK(gex_vfx_format_info_get(0, &info) == 1);
    CHECK(info.has_palette == 0);
    CHECK(info.enables_alpha == 1);

    /* Formato desconocido: no se afirma nada. */
    CHECK(gex_vfx_format_info_get(999, &info) == 0);
    CHECK(info.format == 999);
    CHECK(info.has_palette == 0 && info.enables_alpha == 0);

    CHECK(gex_vfx_format_info_get(GEX_VFX_FORMAT_1, NULL) == 0);

    /* Coherencia con el predicado simple. */
    CHECK(gex_vfx_format_has_palette(GEX_VFX_FORMAT_1) == 1);
    CHECK(gex_vfx_format_has_palette(GEX_VFX_FORMAT_11) == 0);
    CHECK(gex_vfx_format_has_palette(GEX_VFX_FORMAT_12) == 0);
}

/* ------------------------------------------------------------------ */
/* Paleta compartida entre entradas                                     */
/* ------------------------------------------------------------------ */

static void test_palette_twins(void)
{
    gex_vfx_file file;
    uint32_t i;
    int pairs = 0;

    if (!file_exists(AZTEC_VFX)) {
        printf("skip palette twins (no file)\n");
        return;
    }
    CHECK(gex_vfx_open(&file, AZTEC_VFX) == GEX_VFX_OK);

    /* Solo las entradas de formato 1 tienen paleta, y su bloque no está a
     * cero; las de formato 11 y 12 lo tienen a cero (dato medido en los
     * 37 .VFX del inventario). */
    for (i = 0; i < file.record_count; i++) {
        const gex_vfx_record *rec = &file.records[i];
        if (rec->format == GEX_VFX_FORMAT_1) {
            CHECK(rec->palette != NULL);
            CHECK(rec->palette_size == GEX_VFX_PALETTE_BYTES);
        } else {
            CHECK(rec->palette == NULL);
            CHECK(rec->palette_size == 0u);
        }
    }

    /* El juego copia el bloque de paleta entre dos entradas que la
     * comparten (memcpy de 0x70 B en 0x004732FB): en AZTEC16.VFX hay un par
     * (entradas 25 y 83) con paletas idénticas. */
    for (i = 0; i < file.record_count; i++) {
        int twin = gex_vfx_palette_twin(&file, i);
        if (twin >= 0) {
            pairs++;
            CHECK(file.records[twin].palette != NULL);
            CHECK(memcmp(file.records[twin].palette, file.records[i].palette,
                         GEX_VFX_PALETTE_BYTES) == 0);
            /* Y la relación es simétrica. */
            CHECK(gex_vfx_palette_twin(&file, (uint32_t)twin) == (int)i);
        }
    }
    printf("AZTEC16.VFX: entradas con paleta compartida: %d\n", pairs);

    CHECK(gex_vfx_palette_twin(&file, file.record_count) == -1);
    CHECK(gex_vfx_palette_twin(NULL, 0) == -1);
    gex_vfx_close(&file);
}

int main(void)
{
    test_names_and_args();
    test_synthetic_container();
    test_broken_containers();
    test_trailing_bytes();
    test_format_info();
    test_palette_twins();
    test_real_aztec();
    test_real_gex_vfx();
    test_dfx_to_vfx_correspondence();

    printf("test_vfx: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
