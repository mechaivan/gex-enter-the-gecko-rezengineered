/*
 * test_camera_path.c - Tests for the camera path of the original GEX3D.EXE
 * (S-3: the fly-through of the main menu over MAP7).
 *
 * The reference is not a model: `ref_menu_camera_path.h` carries the 26
 * keys read from LEVEL\MAP7.DFX through the container chain (level header
 * -> actor -> entity -> object -> keys) and the 428 positions measured on
 * the retail executable frame by frame. The checks below are exact, with
 * no tolerance:
 *
 *   - 26 keys, 25 durations summing 428, sentinel (D = 0) at the end;
 *   - the 428 samples of the flight, axis by axis;
 *   - the first sample (2341, 4071, 3074) and the last (2243, 487, 196);
 *   - after the sentinel no sample is produced and the position is not
 *     written again;
 *   - invalid arguments, out-of-range nodes, the variant this module does
 *     not model, and the defensive limit of the last key.
 *
 * The arithmetic is integer only (Q15 Hermite, 32-bit wrapping products,
 * arithmetic shifts, truncation to int16): the exact match is the proof
 * that no floating point slipped in.
 */

#include "gex/camera_path.h"

#include "ref_menu_camera_path.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if(!(cond)){ failures++; \
    printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);} } while(0)

static gex_camera_path ref_path(void) {
    gex_camera_path path;
    path.keys = gex_ref_menu_camera_path;
    path.count = GEX_REF_MENU_CAMERA_PATH_COUNT;
    path.variant = GEX_CAMERA_PATH_VARIANT_POSITION;
    path.flags = 1; /* objeto +0x07 del DFX */
    return path;
}

static int same_point(const int16_t a[3], int16_t x, int16_t y, int16_t z) {
    return a[0] == x && a[1] == y && a[2] == z;
}

static int same_key_point(const int16_t a[3], const gex_camera_path_key *k) {
    return a[0] == k->point[0] && a[1] == k->point[1] && a[2] == k->point[2];
}

/* ------------------------------------------------------------------ */
/* Datos de referencia                                                */
/* ------------------------------------------------------------------ */

static void test_reference_layout(void) {
    gex_camera_path path = ref_path();
    int32_t total = 0;
    int i;

    CHECK(path.count == 26);
    CHECK(GEX_REF_MENU_CAMERA_PATH_FRAMES == 428);

    for (i = 0; i < path.count - 1; i++) total += path.keys[i].duration;
    CHECK(total == 428);
    CHECK(total == GEX_REF_MENU_CAMERA_PATH_DURATION_TOTAL);

    /* Centinela: D = 0 y el punto final medido. */
    CHECK(path.keys[25].duration == 0);
    CHECK(same_key_point(path.keys[25].point, &path.keys[25]));
    CHECK(path.keys[25].point[0] == 2243);
    CHECK(path.keys[25].point[1] == 487);
    CHECK(path.keys[25].point[2] == 196);
    CHECK(path.keys[25].tangent_out[0] == 0);
    CHECK(path.keys[25].tangent_in[0] == 0);

    /* Primer registro y un par de tangentes. */
    CHECK(path.keys[0].point[0] == 2333);
    CHECK(path.keys[0].point[1] == 4061);
    CHECK(path.keys[0].point[2] == 3078);
    CHECK(path.keys[0].tangent_out[0] == 0);
    CHECK(path.keys[1].tangent_out[0] == 1958);
    CHECK(path.keys[1].tangent_out[1] == 1210);
    CHECK(path.keys[1].tangent_out[2] == -615);
    CHECK(path.keys[24].duration == 18);
    CHECK(path.keys[24].tangent_out[0] == 1822);
    CHECK(path.keys[24].tangent_out[1] == -299);
    CHECK(path.keys[24].tangent_out[2] == -117);

    CHECK(gex_camera_path_validate(&path) == 1);
}

/* ------------------------------------------------------------------ */
/* Avance del nodo                                                    */
/* ------------------------------------------------------------------ */

static void test_first_sample(void) {
    gex_camera_path path = ref_path();
    gex_camera_path_node node;
    int16_t pos[3] = { 0, 0, 0 };

    gex_camera_path_init_node(&node);
    CHECK(node.index == 0);
    CHECK(node.fraction == 0);
    CHECK(gex_camera_path_fraction(&node) == 0);
    CHECK(gex_camera_path_mode(&node) == 0);

    /* La primera evaluacion es con frac = 1, no con frac = 0. */
    CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    CHECK(node.index == 0);
    CHECK(gex_camera_path_fraction(&node) == 1);
    CHECK(same_point(pos, 2341, 4071, 3074));
    CHECK(same_point(pos, gex_ref_menu_camera_position[0][0],
                     gex_ref_menu_camera_position[0][1],
                     gex_ref_menu_camera_position[0][2]));
}

static void test_segment_boundaries(void) {
    gex_camera_path path = ref_path();
    gex_camera_path_node node;
    int16_t pos[3];
    int i;

    gex_camera_path_init_node(&node);

    /* 20 frames del primer segmento (D = 20): al llegar a D se cambia de
     * segmento y la fraccion vuelve a 0. */
    for (i = 0; i < 19; i++) {
        CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    }
    CHECK(node.index == 0);
    CHECK(gex_camera_path_fraction(&node) == 19);
    CHECK(!same_key_point(pos, &path.keys[1]));

    CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    CHECK(node.index == 1);
    CHECK(gex_camera_path_fraction(&node) == 0);
    CHECK(same_key_point(pos, &path.keys[1]));
    CHECK(same_point(pos, gex_ref_menu_camera_position[19][0],
                     gex_ref_menu_camera_position[19][1],
                     gex_ref_menu_camera_position[19][2]));

    /* 13 frames del segundo segmento (D = 13) -> indice 2, fraccion 0. */
    for (i = 0; i < 13; i++) {
        CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    }
    CHECK(node.index == 2);
    CHECK(gex_camera_path_fraction(&node) == 0);
    CHECK(same_key_point(pos, &path.keys[2]));
    CHECK(same_point(pos, gex_ref_menu_camera_position[32][0],
                     gex_ref_menu_camera_position[32][1],
                     gex_ref_menu_camera_position[32][2]));

    /* El centinela se evalua una vez (copia su punto) y a partir de ahi el
     * avance devuelve 0. */
    CHECK(gex_camera_path_advance(&path, &node) == 1);
}

/* ------------------------------------------------------------------ */
/* Los 428 frames                                                     */
/* ------------------------------------------------------------------ */

static void test_full_trajectory(void) {
    gex_camera_path path = ref_path();
    gex_camera_path_node node;
    int16_t pos[3] = { 0, 0, 0 };
    int frame;

    gex_camera_path_init_node(&node);

    for (frame = 0; frame < GEX_REF_MENU_CAMERA_PATH_FRAMES; frame++) {
        int ok = gex_camera_path_step(&path, &node, pos) == 1 &&
                 same_point(pos,
                            gex_ref_menu_camera_position[frame][0],
                            gex_ref_menu_camera_position[frame][1],
                            gex_ref_menu_camera_position[frame][2]);
        checks++;
        if (!ok) {
            failures++;
            printf("FAIL frame %d: got (%d,%d,%d) expected (%d,%d,%d)\n",
                   frame, pos[0], pos[1], pos[2],
                   gex_ref_menu_camera_position[frame][0],
                   gex_ref_menu_camera_position[frame][1],
                   gex_ref_menu_camera_position[frame][2]);
            break;
        }
    }
    CHECK(frame == GEX_REF_MENU_CAMERA_PATH_FRAMES);
    CHECK(same_point(pos, 2243, 487, 196));
    CHECK(node.index == 25);
    CHECK(gex_camera_path_fraction(&node) == 0);
}

static void test_end_holds(void) {
    gex_camera_path path = ref_path();
    gex_camera_path_node node;
    int16_t pos[3] = { 0, 0, 0 };
    int16_t sentinel[3];
    int i;

    gex_camera_path_init_node(&node);
    for (i = 0; i < GEX_REF_MENU_CAMERA_PATH_FRAMES; i++) {
        CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    }

    /* La evaluacion directa del centinela copia su punto... */
    CHECK(gex_camera_path_eval(&path, &node, sentinel) == 1);
    CHECK(same_point(sentinel, 2243, 487, 196));

    /* ...pero el avance ya no produce muestras y no escribe la posicion. */
    pos[0] = 111;
    pos[1] = 222;
    pos[2] = 333;
    for (i = 0; i < 20; i++) {
        CHECK(gex_camera_path_step(&path, &node, pos) == 0);
    }
    CHECK(same_point(pos, 111, 222, 333));
    CHECK(node.index == 25);
    CHECK(gex_camera_path_fraction(&node) == 0);
    CHECK(gex_camera_path_mode(&node) == 0);
}

/* ------------------------------------------------------------------ */
/* Ramas no usadas por MAP7 y comprobaciones defensivas               */
/* ------------------------------------------------------------------ */

static void test_mode_bit(void) {
    gex_camera_path path = ref_path();
    gex_camera_path_node node;
    int16_t pos[3] = { 0, 0, 0 };

    /* Modo 1 (bit 0 del campo +2): la fraccion se conserva y el bit se
     * mantiene; la evaluacion usa el limite 0x8000, de modo que t = frac
     * (Q15). Con frac = 16384 (medio segmento) la muestra es la media
     * Hermite de los registros 0 y 1, calculada a mano:
     *   t=16384, t2=8192, t3=4096
     *   h00=16384, h01=16384, h10=4096, h11=-4096
     *   x = (2333*16384 + 4122*16384 + 0 - 1958*4096) >> 15 = 2982
     *   y = (4061*16384 + 5900*16384 + 0 - 1210*4096) >> 15 = 4829
     *   z = (3078*16384 + 2457*16384 + 0 + 615*4096) >> 15 = 2844
     * Si el limite inicial fuese 0x10000 el resultado seria otro. */
    node.index = 0;
    node.fraction = (uint16_t)((16384u << 1) | 1u);
    CHECK(gex_camera_path_mode(&node) == 1);
    CHECK(gex_camera_path_fraction(&node) == 16384);
    CHECK(gex_camera_path_eval(&path, &node, pos) == 1);
    CHECK(same_point(pos, 2982, 4829, 2844));

    /* El avance mantiene el bit de modo y suma 1 a la fraccion. */
    node.fraction = 1; /* modo 1, frac 0 */
    CHECK(gex_camera_path_advance(&path, &node) == 1);
    CHECK(gex_camera_path_mode(&node) == 1);
    CHECK(gex_camera_path_fraction(&node) == 1);
    CHECK(node.fraction == 3);
    CHECK(gex_camera_path_eval(&path, &node, pos) == 1);
    CHECK(same_key_point(pos, &path.keys[0]));
}

static void test_wrap_flags(void) {
    gex_camera_path path = ref_path();
    gex_camera_path_node node;
    int16_t pos[3] = { 0, 0, 0 };
    int i;

    /* Bits 1/2 de +0x07: al llegar al centinela el indice vuelve a 0 en el
     * mismo frame (rama medida; su significado no esta establecido), de
     * modo que ese frame copia el primer registro y no el centinela. Se
     * ejercita con los mismos datos. */
    path.flags = GEX_CAMERA_PATH_FLAG_WRAP_A;
    gex_camera_path_init_node(&node);
    for (i = 0; i < GEX_REF_MENU_CAMERA_PATH_FRAMES; i++) {
        CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    }
    CHECK(node.index == 0);
    CHECK(gex_camera_path_fraction(&node) == 0);
    CHECK(same_key_point(pos, &path.keys[0]));
    CHECK(!same_point(pos, 2243, 487, 196));

    /* El siguiente frame reanuda la trayectoria desde el principio. */
    CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    CHECK(same_point(pos, gex_ref_menu_camera_position[0][0],
                     gex_ref_menu_camera_position[0][1],
                     gex_ref_menu_camera_position[0][2]));
}

static void test_invalid_arguments(void) {
    gex_camera_path path = ref_path();
    gex_camera_path bad;
    gex_camera_path_node node;
    gex_camera_path_node saved;
    int16_t pos[3] = { 7, 8, 9 };
    int16_t out[3] = { 0, 0, 0 };

    gex_camera_path_init_node(&node);

    CHECK(gex_camera_path_step(NULL, &node, pos) == 0);
    CHECK(gex_camera_path_step(&path, NULL, pos) == 0);
    CHECK(gex_camera_path_step(&path, &node, NULL) == 0);
    CHECK(gex_camera_path_advance(NULL, &node) == 0);
    CHECK(gex_camera_path_advance(&path, NULL) == 0);
    CHECK(gex_camera_path_eval(NULL, &node, out) == 0);
    CHECK(gex_camera_path_eval(&path, NULL, out) == 0);
    CHECK(gex_camera_path_eval(&path, &node, NULL) == 0);
    CHECK(gex_camera_path_validate(NULL) == 0);
    CHECK(same_point(pos, 7, 8, 9));
    CHECK(node.index == 0);
    CHECK(node.fraction == 0);

    /* Variante 1 (registros de 10 B, rotacion): no modelada. */
    bad = path;
    bad.variant = GEX_CAMERA_PATH_VARIANT_ROTATION;
    CHECK(gex_camera_path_advance(&bad, &node) == 0);
    CHECK(gex_camera_path_eval(&bad, &node, out) == 0);
    CHECK(gex_camera_path_validate(&bad) == 0);

    /* Contadores y punteros invalidos. */
    bad = path;
    bad.count = 0;
    CHECK(gex_camera_path_advance(&bad, &node) == 0);
    CHECK(gex_camera_path_eval(&bad, &node, out) == 0);
    CHECK(gex_camera_path_validate(&bad) == 0);

    bad = path;
    bad.count = 1;
    CHECK(gex_camera_path_validate(&bad) == 0);

    bad = path;
    bad.keys = NULL;
    CHECK(gex_camera_path_advance(&bad, &node) == 0);
    CHECK(gex_camera_path_eval(&bad, &node, out) == 0);
    CHECK(gex_camera_path_validate(&bad) == 0);

    /* Indice fuera de rango. */
    node.index = -1;
    CHECK(gex_camera_path_advance(&path, &node) == 0);
    CHECK(gex_camera_path_eval(&path, &node, out) == 0);
    node.index = path.count;
    CHECK(gex_camera_path_advance(&path, &node) == 0);
    CHECK(gex_camera_path_eval(&path, &node, out) == 0);

    /* Duracion negativa: el contrato de registros validados la rechaza. */
    {
        gex_camera_path_key copy[GEX_REF_MENU_CAMERA_PATH_COUNT];
        memcpy(copy, gex_ref_menu_camera_path, sizeof copy);
        bad = path;
        bad.keys = copy;
        copy[0].duration = -1;
        CHECK(gex_camera_path_validate(&bad) == 0);

        /* Ultimo registro con D > 0 y fraccion > 0: la evaluacion no lee
         * el registro siguiente (que no existe) y devuelve 0. */
        copy[0].duration = gex_ref_menu_camera_path[0].duration;
        copy[25].duration = 5;
        saved.index = 25;
        saved.fraction = 2; /* frac = 1 */
        CHECK(gex_camera_path_eval(&bad, &saved, out) == 0);
    }

    /* Un nodo valido no se toca cuando la llamada falla. */
    gex_camera_path_init_node(&node);
    CHECK(gex_camera_path_step(&path, &node, pos) == 1);
    CHECK(same_point(pos, 2341, 4071, 3074));
}

int main(void) {
    printf("Running camera path tests...\n");
    test_reference_layout();
    test_first_sample();
    test_segment_boundaries();
    test_full_trajectory();
    test_end_holds();
    test_mode_bit();
    test_wrap_flags();
    test_invalid_arguments();
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
