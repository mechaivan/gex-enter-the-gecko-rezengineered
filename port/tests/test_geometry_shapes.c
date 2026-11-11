/*
 * test_geometry_shapes.c - geometrías de prueba del Módulo 5A (portable).
 *
 * Comprueba el cubo de prueba sin Windows ni D3D11: forma de la emisión,
 * orden del pintor, colores, determinismo y conversión a geometría.
 */

#include "gex/geometry_shapes.h"

#include "gex/geometry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_null_argument(void)
{
    CHECK(gex_shape_cube(NULL, 0.0f) == -1);
}

static void test_cube_shape(void)
{
    gex_emit_output out;
    size_t i;

    memset(&out, 0, sizeof(out));
    CHECK(gex_shape_cube(&out, 0.6f) == 0);
    CHECK(out.count == GEX_SHAPE_CUBE_TRIANGLES);
    CHECK(out.capacity == GEX_SHAPE_CUBE_TRIANGLES);
    CHECK(out.tris != NULL);
    CHECK(out.fog_pass_count == 0);
    CHECK(out.overflow == 0);

    if (out.tris == NULL) {
        return;
    }

    for (i = 0; i < out.count; i++) {
        const gex_emit_triangle *t = &out.tris[i];
        int c;
        CHECK((t->flags & GEX_EMIT_F_NEAR_EMITTER) != 0);
        CHECK((t->flags & GEX_EMIT_F_FOG_PASS) == 0);
        CHECK((t->flags & GEX_EMIT_F_DEPTH_CLIPPED) == 0);
        CHECK(t->layer == 0);
        CHECK(t->texture == 0);
        for (c = 0; c < 3; c++) {
            /* Dentro del espacio lógico 512x384 (con margen). */
            CHECK(t->x[c] > -200 && t->x[c] < 700);
            CHECK(t->y[c] > -200 && t->y[c] < 600);
            /* Profundidad positiva y por debajo del máximo de la emisión. */
            CHECK(t->z[c] > 0);
            CHECK(t->z[c] <= 0xFFFF);
            /* Color opaco y no negro. */
            CHECK((t->color[c] >> 24) == 0xFFu);
            CHECK((t->color[c] & 0x00FFFFFFu) != 0u);
        }
    }

    /* Orden del pintor: las caras se emiten de atrás (profundidad mayor)
     * hacia delante. Cada cara ocupa dos triángulos consecutivos con el
     * mismo `source_poly`. */
    {
        float previous = 1e9f;
        size_t i = 0;
        while (i < out.count) {
            size_t j = i;
            float sum = 0.0f;
            int n = 0;
            while (j < out.count && out.tris[j].source_poly == out.tris[i].source_poly) {
                const gex_emit_triangle *t = &out.tris[j];
                sum += ((float)t->z[0] + (float)t->z[1] + (float)t->z[2]) / 3.0f;
                n++;
                j++;
            }
            CHECK(n == 2);
            if (n > 0) {
                float avg = sum / (float)n;
                CHECK(avg <= previous + 0.001f);
                previous = avg;
            }
            i = j;
        }
    }

    /* La geometría convertida conserva los 12 triángulos. */
    {
        gex_geometry geom;
        CHECK(gex_geometry_build(&out, &geom) == GEX_GEOM_OK);
        CHECK(geom.triangle_count == GEX_SHAPE_CUBE_TRIANGLES);
        CHECK(geom.index_count == GEX_SHAPE_CUBE_TRIANGLES * 3u);
        CHECK(geom.vertex_count > 0u);
        CHECK(geom.vertex_count <= geom.index_count);
        if (geom.indices != NULL) {
            size_t k;
            for (k = 0; k < geom.index_count; k++) {
                CHECK(geom.indices[k] < geom.vertex_count);
            }
        }
        gex_geometry_free(&geom);
    }

    gex_emit_output_free(&out);
    CHECK(out.tris == NULL);
    CHECK(out.count == 0);
}

static void test_determinism_and_rotation(void)
{
    gex_emit_output a;
    gex_emit_output b;
    gex_emit_output c;
    size_t i;
    int different = 0;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&c, 0, sizeof(c));
    CHECK(gex_shape_cube(&a, 0.6f) == 0);
    CHECK(gex_shape_cube(&b, 0.6f) == 0);
    CHECK(gex_shape_cube(&c, 1.9f) == 0);

    if (a.tris != NULL && b.tris != NULL) {
        CHECK(a.count == b.count);
        for (i = 0; i < a.count && i < b.count; i++) {
            CHECK(memcmp(&a.tris[i], &b.tris[i], sizeof(a.tris[i])) == 0);
        }
    }
    /* Girar el cubo cambia la geometría. */
    if (a.tris != NULL && c.tris != NULL) {
        for (i = 0; i < a.count && i < c.count; i++) {
            if (a.tris[i].x[0] != c.tris[i].x[0] ||
                a.tris[i].y[0] != c.tris[i].y[0]) {
                different = 1;
            }
        }
    }
    CHECK(different == 1);

    gex_emit_output_free(&a);
    gex_emit_output_free(&b);
    gex_emit_output_free(&c);
}

int main(void)
{
    test_null_argument();
    test_cube_shape();
    test_determinism_and_rotation();

    printf("test_geometry_shapes: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
