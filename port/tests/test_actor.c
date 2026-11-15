/*
 * test_actor.c - clases y modelos de actor del .DFX (S-4).
 *
 * Comprueba contra MAP7.DFX los valores medidos en S-4:
 *
 *   tvmenu__ (clase 0xDFE68): 1 modelo, rama de panel (field_0x10 = 0),
 *     modelo de 8 vértices / 12 polígonos / 1 subparte, radio 552, y todos
 *     los polígonos sin textura (uv_ptr 0x20800080).
 *
 *   tvbutn__ (clase 0xDC538): 20 modelos; el 2 y el 3 son el mismo quad de
 *     4 vértices / 2 triángulos con textura 600 y 604 respectivamente.
 *
 * Si el fichero no está, el test se salta (igual que el resto de la suite).
 */

#include <stdio.h>
#include <string.h>

#include "gex/actor.h"
#include "gex/asset/dfx.h"

static int failures;
static int checks;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                      \
    } while (0)

#define CHECK_EQ_U(a, b)                                                       \
    do {                                                                       \
        unsigned long long va = (unsigned long long)(a);                       \
        unsigned long long vb = (unsigned long long)(b);                       \
        checks++;                                                              \
        if (va != vb) {                                                        \
            failures++;                                                        \
            printf("FAIL %s:%d: %s = 0x%llX, esperado 0x%llX\n", __FILE__,     \
                   __LINE__, #a, va, vb);                                      \
        }                                                                      \
    } while (0)

#define CHECK_EQ_I(a, b)                                                       \
    do {                                                                       \
        long long va = (long long)(a);                                         \
        long long vb = (long long)(b);                                         \
        checks++;                                                              \
        if (va != vb) {                                                        \
            failures++;                                                        \
            printf("FAIL %s:%d: %s = %lld, esperado %lld\n", __FILE__,         \
                   __LINE__, #a, va, vb);                                      \
        }                                                                      \
    } while (0)

/* Offsets medidos en la imagen de MAP7.DFX (S-4). */
#define TVMENU_CLASS_OFFSET 0x0DFE68u
#define TVBUTN_CLASS_OFFSET 0x0DC538u

static const char *find_map7(void)
{
    static const char *candidates[] = {
        "C:/GEX_REZ/GEX2/level/map7.dfx",
        "C:/GEX_REZ/GEX2/LEVEL/MAP7.DFX",
        "C:/GEX_REZ/SMOKE_H4A/LEVEL/MAP7.DFX",
    };
    size_t i;

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f != NULL) {
            fclose(f);
            return candidates[i];
        }
    }
    return NULL;
}

static void test_tvmenu(const uint8_t *image, size_t size)
{
    gex_actor_class cls;
    gex_actor_model model;
    gex_actor_subpart sub;
    gex_actor_polygon poly;
    int16_t xyz[3];
    uint32_t i;
    int textured = 0;

    CHECK(gex_actor_class_read(image, size, TVMENU_CLASS_OFFSET, &cls) ==
          GEX_ACTOR_OK);
    CHECK(strcmp(cls.name, "tvmenu__") == 0);
    CHECK_EQ_U(cls.model_count, 1);
    CHECK_EQ_U(cls.field_0x10, 0); /* rama de panel 0x4718CF */

    CHECK(gex_actor_model_read(image, size, &cls, 0, &model) == GEX_ACTOR_OK);
    CHECK_EQ_U(model.vertex_count, 8);
    CHECK_EQ_U(model.polygon_count, 12);
    CHECK_EQ_U(model.subpart_count, 1);
    CHECK_EQ_U(model.radius, 552);
    CHECK_EQ_U(model.normal_table, 0);

    /* Caja cerrada: la cara frontal está en z = 418 y la trasera en z = 0. */
    CHECK(gex_actor_vertex(image, size, &model, 0, xyz) == GEX_ACTOR_OK);
    CHECK_EQ_I(xyz[0], -302);
    CHECK_EQ_I(xyz[1], -156);
    CHECK_EQ_I(xyz[2], 418);
    CHECK(gex_actor_vertex(image, size, &model, 7, xyz) == GEX_ACTOR_OK);
    CHECK_EQ_I(xyz[0], -302);
    CHECK_EQ_I(xyz[1], 199);
    CHECK_EQ_I(xyz[2], 0);

    CHECK(gex_actor_read_subpart(image, size, &model, 0, &sub) == GEX_ACTOR_OK);
    CHECK_EQ_I(sub.last_polygon, 11);
    CHECK_EQ_I(sub.first_vertex, 0);
    CHECK_EQ_I(sub.last_vertex, 7);

    /* El panel no tiene textura: uv_ptr = 0x20800080 no cae en la imagen. */
    for (i = 0; i < model.polygon_count; i++) {
        CHECK(gex_actor_read_polygon(image, size, &model, i, &poly) == GEX_ACTOR_OK);
        if (gex_actor_polygon_textured(image, size, &poly)) {
            textured = 1;
        }
    }
    CHECK(textured == 0);

    /* Fuera de rango: no se inventa nada. */
    CHECK(gex_actor_model_read(image, size, &cls, 1, &model) ==
          GEX_ACTOR_ERR_RANGE);
}

static void test_tvbutn(const uint8_t *image, size_t size)
{
    gex_actor_class cls;
    gex_actor_model model;
    gex_actor_polygon poly;
    gex_actor_uv uv;
    int16_t xyz[3];

    CHECK(gex_actor_class_find(image, size, "tvbutn__", &cls) == GEX_ACTOR_OK);
    CHECK_EQ_U(cls.offset, TVBUTN_CLASS_OFFSET);
    CHECK_EQ_U(cls.model_count, 20);
    CHECK_EQ_U(cls.field_0x10, 0);

    /* Modelo 2: quad normal (textura 600). */
    CHECK(gex_actor_model_read(image, size, &cls, 2, &model) == GEX_ACTOR_OK);
    CHECK_EQ_U(model.vertex_count, 4);
    CHECK_EQ_U(model.polygon_count, 2);
    CHECK_EQ_U(model.subpart_count, 1);
    CHECK_EQ_U(model.radius, 66);

    CHECK(gex_actor_vertex(image, size, &model, 0, xyz) == GEX_ACTOR_OK);
    CHECK_EQ_I(xyz[0], 61);
    CHECK_EQ_I(xyz[1], 0);
    CHECK_EQ_I(xyz[2], 0);
    CHECK(gex_actor_vertex(image, size, &model, 2, xyz) == GEX_ACTOR_OK);
    CHECK_EQ_I(xyz[0], -59);
    CHECK_EQ_I(xyz[1], 0);
    CHECK_EQ_I(xyz[2], 27);

    CHECK(gex_actor_read_polygon(image, size, &model, 0, &poly) == GEX_ACTOR_OK);
    CHECK_EQ_U(poly.vtx[0], 0);
    CHECK_EQ_U(poly.vtx[1], 1);
    CHECK_EQ_U(poly.vtx[2], 2);
    CHECK_EQ_U(poly.flags, 0xFFF);
    CHECK(gex_actor_polygon_textured(image, size, &poly) == 1);
    CHECK(gex_actor_read_uv(image, size, &poly, &uv) == GEX_ACTOR_OK);
    CHECK_EQ_U(uv.texture, 600);
    CHECK_EQ_I(uv.layer, 0);
    CHECK_EQ_U(uv.u[0], 255);
    CHECK_EQ_U(uv.v[0], 63);
    CHECK_EQ_U(uv.u[1], 255);
    CHECK_EQ_U(uv.v[1], 0);
    CHECK_EQ_U(uv.u[2], 0);
    CHECK_EQ_U(uv.v[2], 0);

    /* Modelo 3: el mismo quad, resaltado (textura 604). */
    CHECK(gex_actor_model_read(image, size, &cls, 3, &model) == GEX_ACTOR_OK);
    CHECK_EQ_U(model.vertex_count, 4);
    CHECK(gex_actor_read_polygon(image, size, &model, 0, &poly) == GEX_ACTOR_OK);
    CHECK(gex_actor_read_uv(image, size, &poly, &uv) == GEX_ACTOR_OK);
    CHECK_EQ_U(uv.texture, 604);
}

static void test_errors(const uint8_t *image, size_t size)
{
    gex_actor_class cls;

    CHECK(gex_actor_class_read(NULL, size, 0, &cls) == GEX_ACTOR_ERR_ARG);
    CHECK(gex_actor_class_read(image, size, 0x00FFFFFFu, &cls) ==
          GEX_ACTOR_ERR_RANGE);
    CHECK(gex_actor_class_read(image, 64, TVMENU_CLASS_OFFSET, &cls) ==
          GEX_ACTOR_ERR_RANGE);
    CHECK(gex_actor_class_find(image, size, "no_existe__", &cls) ==
          GEX_ACTOR_ERR_NAME);
    CHECK(gex_actor_class_find(image, size, NULL, &cls) == GEX_ACTOR_ERR_ARG);
}

int main(void)
{
    const char *path = find_map7();
    gex_dfx_container container;
    gex_actor_class cls;

    if (path == NULL) {
        printf("skip: MAP7.DFX not available\n");
        return 0;
    }
    if (gex_dfx_load(path, &container) != GEX_DFX_OK) {
        printf("FAIL: no se pudo cargar %s\n", path);
        return 1;
    }

    test_tvmenu(container.image, container.image_size);
    test_tvbutn(container.image, container.image_size);
    test_errors(container.image, container.image_size);

    /* La clase leída por offset y por nombre es la misma. */
    CHECK(gex_actor_class_find(container.image, container.image_size,
                               "tvmenu__", &cls) == GEX_ACTOR_OK);
    CHECK_EQ_U(cls.offset, TVMENU_CLASS_OFFSET);

    gex_dfx_release_container(&container);

    printf("test_actor: %d comprobaciones, %d fallos\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
