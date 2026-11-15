/*
 * test_menu.c - escena del menú del televisor (S-4).
 *
 * Fija las constantes **medidas** de `gex/menu.c` para que un cambio accidental
 * no pase inadvertido: número de triángulos, texturas por rótulo (y la
 * variante resaltada +4), UV, colores y los vértices de mundo que salen de la
 * base Q12 y de la posición del registro de animación.
 *
 * Los valores esperados se calculan a mano con la misma aritmética que el
 * original (`mundo = posición + (base · local) sar 12`, con sar aritmético),
 * a partir de los registros volcados en ejecución (ver
 * `S3_MENU_CAMERA_SEQUENCE.md` § 11). No se lee ningún fichero: si el `.DFX`
 * cambia, lo detecta `test_actor`; aquí se comprueba que la escena usa los
 * valores medidos.
 */

#include <stdio.h>
#include <string.h>

#include "gex/emit.h"
#include "gex/menu.h"

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

/* Los 12 primeros triángulos son el panel; los 8 siguientes, los rótulos. */
#define PANEL_TRIS 12u
#define LABEL_TRIS 8u

static void check_vertex(const gex_menu_scene *scene, size_t tri, size_t v,
                         int x, int y, int z)
{
    CHECK_EQ_I(scene->tris[tri].v[v][0], x);
    CHECK_EQ_I(scene->tris[tri].v[v][1], y);
    CHECK_EQ_I(scene->tris[tri].v[v][2], z);
}

static void test_scene(void)
{
    gex_menu_scene scene;
    size_t i;

    CHECK(gex_menu_scene_measured(NULL, 0) == 0);

    /* Sin selección: las cuatro texturas normales. */
    CHECK(gex_menu_scene_measured(&scene, -1) == 1);
    CHECK_EQ_I(scene.count, PANEL_TRIS + LABEL_TRIS);
    CHECK_EQ_I(scene.selected, -1);

    /* Panel: 12 triángulos sin textura, con el color de la carcasa. */
    for (i = 0; i < PANEL_TRIS; i++) {
        CHECK_EQ_I(scene.tris[i].texture, 0);
        CHECK_EQ_I(scene.tris[i].color, 0xFF101018u);
        CHECK_EQ_I(scene.tris[i].u[0], 0);
        CHECK_EQ_I(scene.tris[i].v_uv[0], 0);
    }

    /* Panel, triángulo 0: vértices 0, 1 y 2 de la caja del .DFX. */
    check_vertex(&scene, 0, 0, 3230, -5, -110);
    check_vertex(&scene, 0, 1, 2662, 134, -110);
    check_vertex(&scene, 0, 2, 2577, -210, -110);
    /* Panel, triángulo 10 = {4,7,6}: su vértice 1 es el 7 de la caja (z = 0). */
    check_vertex(&scene, 10, 1, 3145, -350, -528);

    /* Rótulos: texto blanco y las texturas normales en orden de pantalla. */
    for (i = PANEL_TRIS; i < scene.count; i++) {
        CHECK_EQ_I(scene.tris[i].color, 0xFFFFFFFFu);
    }
    for (i = 0; i < 4; i++) {
        static const int expected[4] = {600, 603, 602, 601};
        CHECK_EQ_I(scene.tris[PANEL_TRIS + i * 2 + 0].texture, expected[i]);
        CHECK_EQ_I(scene.tris[PANEL_TRIS + i * 2 + 1].texture, expected[i]);
    }

    /* UV del rótulo: la textura se recorre una vez (registro dividido por 2). */
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].u[0], 127);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].v_uv[0], 31);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].u[1], 127);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].v_uv[1], 0);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].u[2], 0);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].v_uv[2], 0);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 1].u[0], 127);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 1].v_uv[0], 31);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 1].u[1], 0);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 1].v_uv[1], 0);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 1].u[2], 0);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 1].v_uv[2], 31);

    /* START: quad de 4 vértices con la base medida (mira a la cámara). */
    check_vertex(&scene, PANEL_TRIS + 0, 0, 2767, 351, 198);
    check_vertex(&scene, PANEL_TRIS + 0, 1, 2767, 351, 225);
    check_vertex(&scene, PANEL_TRIS + 0, 2, 2748, 470, 225);
    check_vertex(&scene, PANEL_TRIS + 1, 0, 2767, 351, 198);
    check_vertex(&scene, PANEL_TRIS + 1, 1, 2748, 470, 225);
    check_vertex(&scene, PANEL_TRIS + 1, 2, 2748, 470, 198);

    /* Las otras tres opciones comparten base y solo cambian de altura. */
    check_vertex(&scene, PANEL_TRIS + 2, 0, 2767, 351, 170);
    check_vertex(&scene, PANEL_TRIS + 4, 0, 2767, 351, 143);
    check_vertex(&scene, PANEL_TRIS + 6, 0, 2767, 351, 115);

    gex_menu_scene_free(&scene);
    CHECK(scene.tris == NULL);
    CHECK_EQ_I(scene.count, 0);
    CHECK_EQ_I(scene.capacity, 0);
    CHECK_EQ_I(scene.selected, -1);
    gex_menu_scene_free(&scene); /* idempotente */
    gex_menu_scene_free(NULL);

    /* El resaltado es la variante +4 del rótulo elegido. */
    CHECK(gex_menu_scene_measured(&scene, 0) == 1);
    CHECK_EQ_I(scene.selected, 0);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].texture, 604);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 1].texture, 604);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 2].texture, 603);
    gex_menu_scene_free(&scene);

    CHECK(gex_menu_scene_measured(&scene, 2) == 1);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 4].texture, 606);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 0].texture, 600);
    gex_menu_scene_free(&scene);

    CHECK(gex_menu_scene_measured(&scene, 3) == 1);
    CHECK_EQ_I(scene.tris[PANEL_TRIS + 6].texture, 605);
    gex_menu_scene_free(&scene);

    /* Fuera de rango: ningún rótulo resaltado, sin inventar un índice. */
    CHECK(gex_menu_scene_measured(&scene, GEX_MENU_LABEL_COUNT) == 1);
    CHECK_EQ_I(scene.selected, -1);
    gex_menu_scene_free(&scene);
}

static void test_emit(void)
{
    gex_menu_scene scene;
    gex_emit_matrix mat;
    gex_emit_output out;
    size_t i;

    /* Vista sintética: la matriz de emisión traslada el menú al plano
     * cercano para que ningún vértice caiga detrás de `near_z`. */
    memset(&mat, 0, sizeof(mat));
    mat.m[0] = 4096;
    mat.m[4] = 4096;
    mat.m[8] = 4096;
    mat.t[2] = 3000;
    mat.focal = 320;
    mat.cx = 256;
    mat.cy = 192;
    mat.near_z = 160;

    memset(&out, 0, sizeof(out));

    CHECK(gex_menu_scene_measured(&scene, 0) == 1);
    CHECK(gex_menu_emit(NULL, &mat, &out) == GEX_EMIT_ERR_ARG);
    CHECK(gex_menu_emit(&scene, NULL, &out) == GEX_EMIT_ERR_ARG);
    CHECK(gex_menu_emit(&scene, &mat, NULL) == GEX_EMIT_ERR_ARG);
    CHECK(gex_menu_emit(&scene, &mat, &out) == GEX_EMIT_OK);
    CHECK_EQ_I(out.count, scene.count);
    CHECK_EQ_I(out.fog_pass_count, 0);
    for (i = 0; i < out.count; i++) {
        CHECK_EQ_I(out.tris[i].seq, i);
        CHECK_EQ_I(out.tris[i].flags & GEX_EMIT_F_DEPTH_CLIPPED, 0);
    }
    /* El orden se conserva: primero el panel, después los rótulos. */
    CHECK_EQ_I(out.tris[0].texture, 0);
    CHECK_EQ_I(out.tris[PANEL_TRIS].texture, 604);
    /* source_poly identifica el triángulo de la escena. */
    CHECK_EQ_I(out.tris[PANEL_TRIS + 1].source_poly, PANEL_TRIS + 1);

    /* Un vértice detrás del plano cercano descarta el triángulo, como el
     * original (`0x46A1D0` abandona el polígono entero). */
    memset(&out, 0, sizeof(out));
    mat.t[2] = -400; /* z de mundo 115..418 -> negativo: todo detrás */
    CHECK(gex_menu_emit(&scene, &mat, &out) == GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 0);

    gex_emit_output_free(&out);
    gex_menu_scene_free(&scene);
}

static void test_push(void)
{
    gex_emit_output out;
    gex_emit_triangle tri;

    memset(&out, 0, sizeof(out));
    memset(&tri, 0, sizeof(tri));
    CHECK(gex_emit_output_push(NULL, &tri) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_output_push(&out, NULL) == GEX_EMIT_ERR_ARG);
    tri.texture = 601;
    CHECK(gex_emit_output_push(&out, &tri) == GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 1);
    CHECK_EQ_I(out.tris[0].texture, 601);
    CHECK_EQ_I(out.tris[0].seq, 0);
    gex_emit_output_free(&out);
}

int main(void)
{
    test_scene();
    test_emit();
    test_push();

    printf("test_menu: %d comprobaciones, %d fallos\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
