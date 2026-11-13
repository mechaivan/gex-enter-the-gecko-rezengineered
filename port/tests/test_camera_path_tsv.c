/*
 * test_camera_path_tsv.c - The 428 camera states against the measured TSVs
 * themselves (S-3, second batch).
 *
 * Two files of `analysis/` are the raw capture of the original executable,
 * 428 rows each, one per frame of the menu fly-through:
 *
 *   menu_camera_map7.tsv          frame, t, position, Euler, far
 *   menu_camera_rotation_map7.tsv frame, position, look-at target, Euler
 *
 * The rotation file is generated from `LEVEL\MAP7.DFX` with the integer
 * arithmetic of the original (0x53E98C evaluation, 0x406515 -> 0x40AE51
 * look-at), so it also carries the look-at target of every frame.
 *
 * The other test (`test_camera_path_scene.c`) compares the port against
 * `ref_menu_camera_path.h`, a header *generated* from the first TSV. This
 * one closes the loop: it parses the TSVs at run time and compares them with
 * the camera the port produces, so a mistake in a generator cannot hide a
 * difference between the port and the reference.
 *
 * The TSVs live in the repository, one directory above `port/`. If they are
 * not there (a checkout without `analysis/`), the check is skipped and says
 * so.
 */

#include "gex/scene_level.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP7_PATH "C:/GEX_REZ/GEX2/level/map7.dfx"
#define TSV_PATH "../analysis/menu_camera_map7.tsv"
#define ROT_TSV_PATH "../analysis/menu_camera_rotation_map7.tsv"
#define TSV_MAX_FRAMES 1024

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

typedef struct tsv_sample {
    int32_t frame;
    int32_t pos[3];
    int32_t euler[3];
} tsv_sample;

typedef struct rot_sample {
    int32_t frame;
    int32_t pos[3];
    int32_t mira[3];
    int32_t euler[3];
} rot_sample;

/*
 * Lee el TSV de posicion: lineas de comentario (#) y la cabecera se saltan;
 * cada fila de datos es `frame t_ms pos_x pos_y pos_z euler_x euler_y euler_z
 * far`.
 */
static int read_tsv(tsv_sample *out, int max_samples)
{
    FILE *fh = fopen(TSV_PATH, "r");
    char line[512];
    int count = 0;

    if (fh == NULL) {
        return -1;
    }
    while (fgets(line, (int)sizeof(line), fh) != NULL) {
        tsv_sample s;
        int n;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        if (strncmp(line, "frame", 5) == 0) {
            continue; /* cabecera */
        }
        n = sscanf(line, "%d %*d %d %d %d %d %d %d %*d", &s.frame, &s.pos[0],
                   &s.pos[1], &s.pos[2], &s.euler[0], &s.euler[1],
                   &s.euler[2]);
        if (n != 7) {
            continue;
        }
        if (count >= max_samples) {
            break;
        }
        out[count++] = s;
    }
    fclose(fh);
    return count;
}

/*
 * Lee el TSV de orientacion: `frame cam_x cam_y cam_z mira_x mira_y mira_z
 * euler_x euler_y euler_z`.
 */
static int read_rot_tsv(rot_sample *out, int max_samples)
{
    FILE *fh = fopen(ROT_TSV_PATH, "r");
    char line[512];
    int count = 0;

    if (fh == NULL) {
        return -1;
    }
    while (fgets(line, (int)sizeof(line), fh) != NULL) {
        rot_sample s;
        int n;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        if (strncmp(line, "frame", 5) == 0) {
            continue; /* cabecera */
        }
        n = sscanf(line, "%d %d %d %d %d %d %d %d %d %d", &s.frame,
                   &s.pos[0], &s.pos[1], &s.pos[2], &s.mira[0], &s.mira[1],
                   &s.mira[2], &s.euler[0], &s.euler[1], &s.euler[2]);
        if (n != 10) {
            continue;
        }
        if (count >= max_samples) {
            break;
        }
        out[count++] = s;
    }
    fclose(fh);
    return count;
}

int main(void)
{
    tsv_sample samples[TSV_MAX_FRAMES];
    rot_sample rot[TSV_MAX_FRAMES];
    gex_scene_level scene;
    gex_camera cam;
    int count;
    int rot_count;
    int i;

    printf("Running camera path TSV tests...\n");

    count = read_tsv(samples, TSV_MAX_FRAMES);
    if (count < 0) {
        printf("skip tsv (no %s)\n", TSV_PATH);
        return 0;
    }
    rot_count = read_rot_tsv(rot, TSV_MAX_FRAMES);
    if (rot_count < 0) {
        printf("skip rotation tsv (no %s)\n", ROT_TSV_PATH);
        return 0;
    }
    /* The capture has 428 rows of the flight. The row that follows
     * ("# estable") is a comment in the TSV and the parser skips it. */
    CHECK(count == 428);
    CHECK(rot_count == 428);
    CHECK(samples[0].frame == 0);
    CHECK(samples[0].pos[0] == 2341);
    CHECK(samples[0].pos[1] == 4071);
    CHECK(samples[0].pos[2] == 3074);
    CHECK(samples[0].euler[0] == -397);
    CHECK(samples[0].euler[1] == 0);
    CHECK(samples[0].euler[2] == 2802);
    CHECK(samples[427].frame == 427);
    CHECK(samples[427].pos[0] == 2243);
    CHECK(samples[427].pos[1] == 487);
    CHECK(samples[427].pos[2] == 196);
    CHECK(samples[427].euler[0] == -29);
    CHECK(samples[427].euler[1] == 0);
    CHECK(samples[427].euler[2] == 2842);
    CHECK(rot[0].frame == 0);
    CHECK(rot[0].mira[0] == 2817);
    CHECK(rot[0].mira[1] == 3862);
    CHECK(rot[0].mira[2] == 2711);
    CHECK(rot[427].frame == 427);
    CHECK(rot[427].mira[0] == 2742);
    CHECK(rot[427].mira[1] == 304);
    CHECK(rot[427].mira[2] == 172);

    memset(&scene, 0, sizeof(scene));
    gex_scene_reference_camera(&cam);
    if (gex_scene_level_load_camera_path(&scene, MAP7_PATH, &cam) !=
        GEX_SCENE_OK) {
        printf("skip tsv trajectory (no level file)\n");
        gex_scene_level_free(&scene);
        return 0;
    }

    /* The camera the port produces, frame by frame, against the captures:
     * position, look-at target (the second trajectory) and orientation. */
    for (i = 0; i < 428; i++) {
        int step = gex_scene_level_camera_path_step(&scene, &cam);
        CHECK(step == 1);
        CHECK(cam.pos[0] == samples[i].pos[0]);
        CHECK(cam.pos[1] == samples[i].pos[1]);
        CHECK(cam.pos[2] == samples[i].pos[2]);
        CHECK(scene.lookat_last[0] == rot[i].mira[0]);
        CHECK(scene.lookat_last[1] == rot[i].mira[1]);
        CHECK(scene.lookat_last[2] == rot[i].mira[2]);
        CHECK(cam.euler[0] == rot[i].euler[0]);
        CHECK(cam.euler[1] == rot[i].euler[1]);
        CHECK(cam.euler[2] == rot[i].euler[2]);
        if (step != 1) {
            break;
        }
    }
    CHECK(i == 428);
    CHECK(cam.pos[0] == samples[427].pos[0]);
    CHECK(cam.pos[1] == samples[427].pos[1]);
    CHECK(cam.pos[2] == samples[427].pos[2]);
    CHECK(cam.euler[0] == rot[427].euler[0]);
    CHECK(cam.euler[1] == rot[427].euler[1]);
    CHECK(cam.euler[2] == rot[427].euler[2]);
    /* The two captures agree with each other on the angles. */
    for (i = 0; i < 428; i++) {
        CHECK(samples[i].euler[0] == rot[i].euler[0]);
        CHECK(samples[i].euler[1] == rot[i].euler[1]);
        CHECK(samples[i].euler[2] == rot[i].euler[2]);
    }

    /* After the sentinel neither the position nor the orientation change. */
    for (i = 0; i < 20; i++) {
        CHECK(gex_scene_level_camera_path_step(&scene, &cam) == 0);
        CHECK(cam.pos[0] == samples[427].pos[0]);
        CHECK(cam.pos[1] == samples[427].pos[1]);
        CHECK(cam.pos[2] == samples[427].pos[2]);
        CHECK(cam.euler[0] == rot[427].euler[0]);
        CHECK(cam.euler[1] == rot[427].euler[1]);
        CHECK(cam.euler[2] == rot[427].euler[2]);
    }

    gex_scene_level_free(&scene);
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
