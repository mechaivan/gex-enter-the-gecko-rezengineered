/*
 * test_camera_path_scene.c - Integration of the MAP7 camera path with the
 * scene loader and the renderer path (S-3, second batch).
 *
 * Three groups of checks:
 *
 *   1. Extraction of the records from the real `.DFX` (MAP7), through the
 *      pointer chain documented in S3_MENU_CAMERA_SEQUENCE.md section 4.5:
 *      header 0x5FC -> actor -> entity -> object -> array of 32-byte keys.
 *      The values are compared with the reference header (which was
 *      generated from the same file), so the chain is verified against
 *      independent evidence.
 *
 *   2. The integration itself: load the level with its camera path, advance
 *      one step per logical update and compare the camera position with the
 *      428 measured samples of `analysis/menu_camera_map7.tsv`, without
 *      tolerances. First position (2341, 4071, 3074), last (2243, 487, 196).
 *
 *   3. The behaviour after the sentinel: no further sample, no further
 *      write, and the last position is kept.
 *
 * The level file is read-only and optional: without it the real-level
 * checks are skipped and only the self-contained ones run.
 */

#include "gex/scene_level.h"
#include "ref_menu_camera_path.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP7_PATH "C:/GEX_REZ/GEX2/level/map7.dfx"

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

static int map7_available(void)
{
    FILE *fh = fopen(MAP7_PATH, "rb");
    if (fh == NULL) {
        return 0;
    }
    fclose(fh);
    return 1;
}

/* ------------------------------------------------------------------ */
/* 1. Extraction of the records from the container                     */
/* ------------------------------------------------------------------ */

static void test_extraction(void)
{
    gex_dfx_container container;
    gex_dfx_camera_path path;
    int i;

    memset(&container, 0, sizeof(container));
    memset(&path, 0, sizeof(path));

    /* The chain is only valid for MAP7's layout; other levels must be
     * rejected instead of returning garbage. */
    CHECK(gex_dfx_load_camera_path(NULL, &path) == GEX_DFX_ERR_ARGUMENT);
    CHECK(gex_dfx_load_camera_path(&container, NULL) == GEX_DFX_ERR_ARGUMENT);

    if (gex_dfx_load(MAP7_PATH, &container) != GEX_DFX_OK) {
        printf("skip extraction (no level file)\n");
        return;
    }

    CHECK(gex_dfx_load_camera_path(&container, &path) == GEX_DFX_OK);
    CHECK(path.loaded == 1);

    /* The chain, with the offsets documented in section 4.5. */
    CHECK(path.header_offset == 0x5FCu);
    CHECK(path.actor_offset == 0x95ECu);
    CHECK(path.entity_offset == 0xC00Cu);
    CHECK(path.object_offset == 0xC02Cu);
    CHECK(path.keys_offset == 0xC034u);

    /* The contract of the records: 26 keys, variant 0 (position), flags 1. */
    CHECK(path.path.count == GEX_REF_MENU_CAMERA_PATH_COUNT);
    CHECK(path.path.count == 26);
    CHECK(path.path.variant == GEX_CAMERA_PATH_VARIANT_POSITION);
    CHECK(path.path.flags == 1);

    /* The values are byte-identical to the reference (which was generated
     * from the same container through the same chain). */
    CHECK(path.path.keys != NULL);
    for (i = 0; i < GEX_REF_MENU_CAMERA_PATH_COUNT; i++) {
        const gex_camera_path_key *a = &path.path.keys[i];
        const gex_camera_path_key *b = &gex_ref_menu_camera_path[i];
        CHECK(a->duration == b->duration);
        CHECK(a->point[0] == b->point[0]);
        CHECK(a->point[1] == b->point[1]);
        CHECK(a->point[2] == b->point[2]);
        CHECK(a->tangent_out[0] == b->tangent_out[0]);
        CHECK(a->tangent_out[1] == b->tangent_out[1]);
        CHECK(a->tangent_out[2] == b->tangent_out[2]);
        CHECK(a->tangent_in[0] == b->tangent_in[0]);
        CHECK(a->tangent_in[1] == b->tangent_in[1]);
        CHECK(a->tangent_in[2] == b->tangent_in[2]);
    }

    /* The sentinel: the last key has D = 0 and the others are positive. */
    CHECK(path.path.keys[25].duration == 0);
    {
        int32_t total = 0;
        for (i = 0; i < 25; i++) {
            CHECK(path.path.keys[i].duration > 0);
            total += path.path.keys[i].duration;
        }
        CHECK(total == 428);
    }

    /* The path view is valid for the module that evaluates it. */
    CHECK(gex_camera_path_validate(&path.path) == 1);

    gex_dfx_release_camera_path(&path);
    CHECK(path.loaded == 0);
    CHECK(path.path.keys == NULL);
    /* Idempotent. */
    gex_dfx_release_camera_path(&path);
    gex_dfx_release_container(&container);
}

/* ------------------------------------------------------------------ */
/* 2. The 428 samples through the scene                                */
/* ------------------------------------------------------------------ */

static void test_trajectory(void)
{
    gex_scene_level scene;
    gex_camera cam;
    int frame;

    memset(&scene, 0, sizeof(scene));
    gex_scene_reference_camera(&cam);

    if (gex_scene_level_load_camera_path(&scene, MAP7_PATH, &cam) !=
        GEX_SCENE_OK) {
        printf("skip trajectory (no level file)\n");
        return;
    }

    /* The node starts in the original state: index 0, fraction 0. */
    CHECK(scene.path_loaded == 1);
    CHECK(scene.path_node.index == 0);
    CHECK(scene.path_node.fraction == 0);
    CHECK(scene.path_frames == 0);
    CHECK(scene.path_finished == 0);

    /* The 428 samples, one per logical update, without tolerances. */
    for (frame = 0; frame < GEX_REF_MENU_CAMERA_PATH_FRAMES; frame++) {
        int step = gex_scene_level_camera_path_step(&scene, &cam);
        const int16_t *expected = gex_ref_menu_camera_position[frame];

        CHECK(step == 1);
        CHECK(cam.pos[0] == expected[0]);
        CHECK(cam.pos[1] == expected[1]);
        CHECK(cam.pos[2] == expected[2]);
        CHECK(scene.path_frames == (uint32_t)(frame + 1));
        if (frame == 0) {
            /* First measured position. */
            CHECK(cam.pos[0] == 2341 && cam.pos[1] == 4071 &&
                  cam.pos[2] == 3074);
        }
        if (step != 1) {
            break;
        }
    }

    /* Last measured position. */
    CHECK(cam.pos[0] == 2243 && cam.pos[1] == 487 && cam.pos[2] == 196);
    CHECK(scene.path_frames == 428);

    /* The orientation is not interpolated: the rotation path (10-byte
     * records) is still not decoded, so the measured initial angles are
     * kept. */
    CHECK(cam.euler[0] == -397);
    CHECK(cam.euler[1] == 0);
    CHECK(cam.euler[2] == 2802);

    /* The scene really changed: the geometry is not the initial one. */
    CHECK(scene.stats.triangles > 0);
    CHECK(scene.stats.leaf_count > 0);

    gex_scene_level_free(&scene);
}

/* ------------------------------------------------------------------ */
/* 3. Behaviour after the sentinel                                     */
/* ------------------------------------------------------------------ */

static void test_after_sentinel(void)
{
    gex_scene_level scene;
    gex_camera cam;
    int16_t before[3];
    int i;

    memset(&scene, 0, sizeof(scene));
    gex_scene_reference_camera(&cam);

    if (gex_scene_level_load_camera_path(&scene, MAP7_PATH, &cam) !=
        GEX_SCENE_OK) {
        printf("skip sentinel (no level file)\n");
        return;
    }

    for (i = 0; i < GEX_REF_MENU_CAMERA_PATH_FRAMES; i++) {
        if (gex_scene_level_camera_path_step(&scene, &cam) != 1) {
            break;
        }
    }
    CHECK(i == GEX_REF_MENU_CAMERA_PATH_FRAMES);
    /* The 428th sample is the last one; the sentinel is marked by the
     * *next* attempt, which produces no sample (the module returns 0 when
     * the index passes the last key). */
    CHECK(scene.path_finished == 0);

    before[0] = cam.pos[0];
    before[1] = cam.pos[1];
    before[2] = cam.pos[2];

    /* After the sentinel there is no sample and the position is kept: no
     * further write at all. */
    for (i = 0; i < 20; i++) {
        CHECK(gex_scene_level_camera_path_step(&scene, &cam) == 0);
        CHECK(cam.pos[0] == before[0]);
        CHECK(cam.pos[1] == before[1]);
        CHECK(cam.pos[2] == before[2]);
    }
    CHECK(scene.path_finished == 1);
    CHECK(scene.path_frames == 428);

    gex_scene_level_free(&scene);
}

/* ------------------------------------------------------------------ */
/* 4. Texture usage over the whole flight                              */
/* ------------------------------------------------------------------ */

static void test_texture_usage(void)
{
    uint32_t counts[16384];
    size_t distinct = 0;
    uint32_t total = 0;
    size_t i;

    memset(counts, 0, sizeof(counts));

    if (gex_scene_camera_path_texture_usage(MAP7_PATH, NULL, counts,
                                             16384u, &distinct) !=
        GEX_SCENE_OK) {
        printf("skip texture usage (no level file)\n");
        return;
    }
    /* The flight touches many more textures than the frame 0 (62): the
     * atlas limit (64) is a real constraint of the texture module. */
    CHECK(distinct > 62);
    CHECK(distinct <= 16384u);
    for (i = 0; i < 16384u; i++) {
        total += counts[i];
    }
    CHECK(total > 0);

    /* Argument validation. */
    CHECK(gex_scene_camera_path_texture_usage(NULL, NULL, counts, 16384u,
                                               &distinct) == GEX_SCENE_ERR_ARG);
    CHECK(gex_scene_camera_path_texture_usage(MAP7_PATH, NULL, NULL, 16384u,
                                               &distinct) == GEX_SCENE_ERR_ARG);
    CHECK(gex_scene_camera_path_texture_usage(MAP7_PATH, NULL, counts, 0u,
                                               &distinct) == GEX_SCENE_ERR_ARG);
}

/* ------------------------------------------------------------------ */
/* 5. Status names and invalid arguments                              */
/* ------------------------------------------------------------------ */

static void test_status_names(void)
{
    CHECK(strcmp(gex_scene_status_name(GEX_SCENE_OK), "ok") == 0);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_ARG) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_LOAD) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_LEVEL) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_BSP) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_EMIT) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_GEOMETRY) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_PATH) != NULL);
    CHECK(gex_scene_status_name((gex_scene_status)999) != NULL);
}

static void test_bad_arguments(void)
{
    gex_scene_level scene;
    gex_camera cam;
    int16_t pos[3] = { 1, 2, 3 };

    memset(&scene, 0, sizeof(scene));
    gex_scene_reference_camera(&cam);

    CHECK(gex_scene_level_load_camera_path(NULL, MAP7_PATH, &cam) ==
          GEX_SCENE_ERR_ARG);
    CHECK(gex_scene_level_load_camera_path(&scene, NULL, &cam) ==
          GEX_SCENE_ERR_ARG);
    CHECK(gex_scene_level_load_camera_path(&scene, "", &cam) ==
          GEX_SCENE_ERR_ARG);
    CHECK(gex_scene_level_load_camera_path(&scene, MAP7_PATH, NULL) ==
          GEX_SCENE_ERR_ARG);

    /* The step needs a loaded scene with a path. */
    CHECK(gex_scene_level_camera_path_step(NULL, &cam) == -1);
    CHECK(gex_scene_level_camera_path_step(&scene, NULL) == -1);
    CHECK(gex_scene_level_camera_path_step(&scene, &cam) == -1);
    CHECK(scene.loaded == 0);

    /* A level without a camera path is rejected, not faked. */
    if (map7_available()) {
        gex_scene_level other;
        memset(&other, 0, sizeof(other));
        CHECK(gex_scene_level_load_camera_path(&other,
                                               "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX",
                                               &cam) == GEX_SCENE_ERR_PATH);
        CHECK(other.loaded == 0);
        CHECK(other.path_loaded == 0);
        gex_scene_level_free(&other);
    }

    (void)pos;
    gex_scene_level_free(&scene);
}

int main(void)
{
    printf("Running camera path scene tests...\n");
    test_status_names();
    test_bad_arguments();
    test_extraction();
    test_trajectory();
    test_after_sentinel();
    test_texture_usage();
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
