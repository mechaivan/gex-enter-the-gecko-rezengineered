/*
 * test_dfx.c - unit tests for the .DFX container parser.
 */
#include "gex/asset/dfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

static uint8_t *create_minimal_dfx(uint32_t count, uint32_t *out_size) {
    uint32_t aligned = (count + GEX_DFX_ALIGN_ADD) & ~GEX_DFX_ALIGN_MASK;
    size_t image_logical = 256;
    size_t size = (size_t)aligned * 4u + image_logical;
    if (size < 4u + (size_t)count * 4u) return NULL;

    uint8_t *data = calloc(size, 1);
    if (!data) return NULL;

    *(uint32_t *)data = count;

    for (uint32_t i = 0; i < count; i++) {
        ((uint32_t *)data)[1 + i] = 0x40u;
    }

    uint8_t *image = data + (size_t)aligned * 4u;
    /* descriptor at image+0x40 (image[0] = 0x40) */
    *(uint32_t *)image = 0x40u;
    uint32_t *desc = (uint32_t *)(image + 0x40);
    desc[0x00/4] = 0x10;
    desc[0x18/4] = 1;
    desc[0x1C/4] = 1;
    desc[0x20/4] = 2;
    desc[0x24/4] = 0x40 + 0x40;  /* vertex_table relative to image */
    desc[0x28/4] = 0x40 + 0x70;
    desc[0x2C/4] = 0x40 + 0xA0;

    memset(image + 0x40 + 0x40, 0, 12);
    memset(image + 0x40 + 0x70, 0, 20);
    memset(image + 0x40 + 0xA0, 0, 12);

    *out_size = (uint32_t)size;
    return data;
}

static void test_minimal_valid(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    gex_dfx_status status = gex_dfx_parse(data, size, &container);

    CHECK(status == GEX_DFX_OK);
    CHECK(container.count == 1);
    uint32_t aligned = (1u + GEX_DFX_ALIGN_ADD) & ~GEX_DFX_ALIGN_MASK;
    CHECK(container.aligned == aligned);
    CHECK(container.image_size == (size_t)size - (size_t)aligned * 4u);

    gex_dfx_level level;
    memset(&level, 0, sizeof(level));
    status = gex_dfx_load_level(&container, &level);
    CHECK(status == GEX_DFX_OK);
    CHECK(level.descriptor.root_bsp == 0x10);
    CHECK(level.descriptor.vertex_count == 1);
    CHECK(level.descriptor.polygon_count == 1);
    CHECK(level.descriptor.normal_count == 2);
    CHECK(level.descriptor.vertex_table == 0x80);
    CHECK(level.descriptor.polygon_table == 0xB0);
    CHECK(level.descriptor.normal_table == 0xE0);
    CHECK(level.vertex_data != NULL);

    gex_dfx_release_level(&level);
    gex_dfx_release_container(&container);
    free(data);
}

static void test_multiple_entries(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(5, &size);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    gex_dfx_status status = gex_dfx_parse(data, size, &container);

    CHECK(status == GEX_DFX_OK);
    CHECK(container.count == 5);
    for (uint32_t i = 0; i < 5; i++) {
        CHECK(container.offsets[i] == 0x40u);
    }
    gex_dfx_release_container(&container);
    free(data);
}

static void test_truncated_file(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, 4, &container) == GEX_DFX_ERR_TRUNCATED);
    CHECK(gex_dfx_parse(data, 6, &container) == GEX_DFX_ERR_TRUNCATED);

    gex_dfx_release_container(&container);
    free(data);
}

static void test_zero_count(void) {
    uint32_t aligned = (0u + GEX_DFX_ALIGN_ADD) & ~GEX_DFX_ALIGN_MASK;
    size_t size = (size_t)aligned * 4u + 64u;
    uint8_t *data = calloc(size, 1);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_ERR_FORMAT);

    free(data);
}

static void test_large_count(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);
    *(uint32_t *)data = 0x1000000u;

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    gex_dfx_status s = gex_dfx_parse(data, size, &container);
    CHECK(s == GEX_DFX_ERR_FORMAT || s == GEX_DFX_ERR_TRUNCATED);

    free(data);
}

static void test_invalid_alignment(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    ((uint32_t *)data)[1] = 0x41u;

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_ERR_ALIGNMENT);

    free(data);
}

static void test_offset_out_of_image(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    uint32_t aligned = (1u + GEX_DFX_ALIGN_ADD) & ~GEX_DFX_ALIGN_MASK;
    ((uint32_t *)data)[1] = (uint32_t)((size - (size_t)aligned * 4u) + 4u);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_ERR_POINTER);

    free(data);
}

static void test_null_pointer(void) {
    gex_dfx_container container;
    CHECK(gex_dfx_parse(NULL, 100, &container) == GEX_DFX_ERR_ARGUMENT);
    CHECK(gex_dfx_parse("test", 100, NULL) == GEX_DFX_ERR_ARGUMENT);
}

static void test_invalid_size(void) {
    gex_dfx_container container;
    CHECK(gex_dfx_parse("test", 7, &container) == GEX_DFX_ERR_TRUNCATED ||
          gex_dfx_parse("test", 7, &container) == GEX_DFX_ERR_FORMAT);
}

static void test_load_level(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_OK);

    gex_dfx_level level;
    memset(&level, 0, sizeof(level));
    CHECK(gex_dfx_load_level(&container, &level) == GEX_DFX_OK);
    CHECK(level.descriptor.root_bsp == 0x10);
    CHECK(level.descriptor.vertex_table == 0x80);
    CHECK(level.vertex_data != NULL);

    gex_dfx_release_level(&level);
    gex_dfx_release_container(&container);
    free(data);
}

static void test_status_names(void) {
    CHECK(strcmp(gex_dfx_status_name(GEX_DFX_OK), "ok") == 0);
    CHECK(strcmp(gex_dfx_status_name(GEX_DFX_ERR_ARGUMENT), "invalid argument") == 0);
    CHECK(strcmp(gex_dfx_status_name(GEX_DFX_ERR_OVERFLOW), "overflow") == 0);
    CHECK(strcmp(gex_dfx_status_name((gex_dfx_status)999), "unknown error") == 0);
}

static void test_relocate_delta_zero(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_OK);

    uint32_t before = *(uint32_t *)(container.image + 0x40);
    CHECK(gex_dfx_relocate(&container, 0) == GEX_DFX_OK);
    CHECK(*(uint32_t *)(container.image + 0x40) == before);

    gex_dfx_release_container(&container);
    free(data);
}

static void test_relocate_delta_nonzero(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_OK);

    uint32_t before = *(uint32_t *)(container.image + 0x40);
    uint32_t delta = 0x12345678u;
    CHECK(gex_dfx_relocate(&container, delta) == GEX_DFX_OK);
    CHECK(*(uint32_t *)(container.image + 0x40) == before + delta);

    CHECK(gex_dfx_relocate(&container, 1) == GEX_DFX_OK);
    CHECK(*(uint32_t *)(container.image + 0x40) == before + delta + 1);

    gex_dfx_release_container(&container);
    free(data);
}

static void test_input_unchanged(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);
    uint8_t *copy = malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, data, size);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_OK);

    CHECK(memcmp(data, copy, size) == 0);

    gex_dfx_release_container(&container);
    free(copy);
    free(data);
}

static void test_memory_release(void) {
    uint32_t size;
    uint8_t *data = create_minimal_dfx(1, &size);
    CHECK(data != NULL);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_parse(data, size, &container) == GEX_DFX_OK);

    gex_dfx_release_container(&container);
    CHECK(container.image == NULL);
    CHECK(container.offsets == NULL);
    CHECK(container.count == 0);

    gex_dfx_release_container(&container);
    gex_dfx_release_container(NULL);

    free(data);
}

static void test_real_aztec(void) {
    const char *path = "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX";
    FILE *f = fopen(path, "rb");
    if (!f) { printf("skip: %s not available\n", path); return; }
    fclose(f);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_load(path, &container) == GEX_DFX_OK);
    if (container.image == NULL) return;

    CHECK(container.count == 16082u);
    CHECK(container.aligned == 16384u);
    CHECK(container.image_size == 479050u);

    gex_dfx_level level;
    memset(&level, 0, sizeof(level));
    CHECK(gex_dfx_load_level(&container, &level) == GEX_DFX_OK);
    CHECK(level.descriptor.vertex_count == 3882u);
    CHECK(level.vertex_data != NULL);

    gex_dfx_release_level(&level);
    gex_dfx_release_container(&container);
}

static void test_real_gex(void) {
    const char *path = "C:/GEX_REZ/E2_A_ORIGINAL/GEX.DFX";
    FILE *f = fopen(path, "rb");
    if (!f) { printf("skip: %s not available\n", path); return; }
    fclose(f);

    gex_dfx_container container;
    memset(&container, 0, sizeof(container));
    CHECK(gex_dfx_load(path, &container) == GEX_DFX_OK);
    if (container.image == NULL) return;

    CHECK(container.count == 322u);
    CHECK(container.aligned == 512u);
    CHECK(container.image_size == 143960u);

    gex_dfx_release_container(&container);
}

int main(void) {
    printf("Running DFX parser tests...\n");

    test_minimal_valid();
    test_multiple_entries();
    test_truncated_file();
    test_zero_count();
    test_large_count();
    test_invalid_alignment();
    test_offset_out_of_image();
    test_null_pointer();
    test_invalid_size();
    test_load_level();
    test_status_names();
    test_relocate_delta_zero();
    test_relocate_delta_nonzero();
    test_input_unchanged();
    test_memory_release();
    test_real_aztec();
    test_real_gex();

    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
