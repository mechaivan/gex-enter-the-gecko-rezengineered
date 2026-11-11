/*
 * test_scene_level.c - Módulo 5B: carga de un nivel real hasta geometría
 * (portable).
 *
 * Comprueba la cadena DFX -> descriptor -> BSP -> emisión -> geometría, sus
 * estadísticas, sus fallos y la liberación. Sin Windows ni D3D11.
 *
 * El caso real usa AZTEC16.DFX si está disponible (solo lectura); los casos
 * de error usan archivos temporales propios.
 */

#include "gex/scene_level.h"
#include "ref_emit_frames.h"
#include "ref_map7_frames.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AZTEC_PATH "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX"
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

static void test_status_names(void)
{
    CHECK(strcmp(gex_scene_status_name(GEX_SCENE_OK), "ok") == 0);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_ARG) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_LOAD) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_LEVEL) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_BSP) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_EMIT) != NULL);
    CHECK(gex_scene_status_name(GEX_SCENE_ERR_GEOMETRY) != NULL);
    CHECK(gex_scene_status_name((gex_scene_status)999) != NULL);
}

/* ------------------------------------------------------------------ */
/* Argumentos inválidos                                               */
/* ------------------------------------------------------------------ */

static void test_bad_arguments(void)
{
    gex_scene_level scene;

    memset(&scene, 0, sizeof(scene));
    CHECK(gex_scene_level_load(NULL, AZTEC_PATH) == GEX_SCENE_ERR_ARG);
    CHECK(gex_scene_level_load(&scene, NULL) == GEX_SCENE_ERR_ARG);
    CHECK(gex_scene_level_load(&scene, "") == GEX_SCENE_ERR_ARG);
    /* La escena sigue vacía y es liberable. */
    CHECK(scene.loaded == 0);
    gex_scene_level_free(&scene);
    gex_scene_level_free(NULL);
}

/* ------------------------------------------------------------------ */
/* Errores de carga                                                    */
/* ------------------------------------------------------------------ */

static void test_load_errors(void)
{
    gex_scene_level scene;
    const char *bogus = "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/__no_existe__.DFX";
    const char *tmp = "test_scene_level_tmp.bin";

    /* Ruta inexistente: error de carga y nada vivo. */
    memset(&scene, 0, sizeof(scene));
    CHECK(gex_scene_level_load(&scene, bogus) == GEX_SCENE_ERR_LOAD);
    CHECK(scene.loaded == 0);
    CHECK(scene.container_loaded == 0);
    CHECK(scene.container.image == NULL);
    gex_scene_level_free(&scene);

    /* Archivo que no es un contenedor DFX: error de carga. */
    {
        FILE *fh = fopen(tmp, "wb");
        if (fh != NULL) {
            static const char junk[] = "esto no es un contenedor DFX, ni lo "
                                       "es un DFX de verdad";
            fwrite(junk, 1, sizeof(junk) - 1u, fh);
            fclose(fh);
            memset(&scene, 0, sizeof(scene));
            CHECK(gex_scene_level_load(&scene, tmp) != GEX_SCENE_OK);
            CHECK(scene.loaded == 0);
            gex_scene_level_free(&scene);
            remove(tmp);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Vista diagnóstica                                                  */
/* ------------------------------------------------------------------ */

static void test_diagnostic_view(void)
{
    gex_emit_matrix mat;
    gex_emit_fog fog;

    memset(&mat, 0, sizeof(mat));
    memset(&fog, 0, sizeof(fog));
    gex_scene_diagnostic_view(&mat, &fog);

    /* Escala 0,25 en los tres ejes y traslación de profundidad. */
    CHECK(mat.m[0] == 0x0400);
    CHECK(mat.m[4] == 0x0400);
    CHECK(mat.m[8] == 0x0400);
    CHECK(mat.t[2] == 0x1000);
    /* Centro de pantalla del original. */
    CHECK(mat.cx == 0x100);
    CHECK(mat.cy == 0x0C0);
    CHECK(mat.focal == 0x140);
    CHECK(mat.near_z == 0xA0);
    /* Niebla generada: LUT presente, split forzado a 0 (ruta lejana) y
     * parametros ajustados al rango de profundidad de la vista (sin niebla
     * hasta 3000, niebla total desde 5000). */
    CHECK(fog.have_lut == 1);
    CHECK(fog.split_z == 0);
    CHECK(fog.fog_total_z == 5000);
    CHECK(fog.fog_none_z == 3000);
    CHECK(fog.lut[0] == 0);
    CHECK(fog.threshold > 0);
    /* El factor crece con la profundidad: 0 cerca y 4096 (total) lejos. */
    CHECK(gex_emit_fog_factor(&fog, 2000) == 0);
    CHECK(gex_emit_fog_factor(&fog, 3000) <= 64);
    CHECK(gex_emit_fog_factor(&fog, 5000) == 4096);
    CHECK(gex_emit_fog_factor(&fog, 6000) == 4096);

    /* Es determinista: dos llamadas dan lo mismo. */
    {
        gex_emit_matrix mat2;
        gex_emit_fog fog2;
        memset(&mat2, 0, sizeof(mat2));
        memset(&fog2, 0, sizeof(fog2));
        gex_scene_diagnostic_view(&mat2, &fog2);
        CHECK(memcmp(&mat, &mat2, sizeof(mat)) == 0);
        CHECK(memcmp(&fog, &fog2, sizeof(fog)) == 0);
    }

    /* NULL no rompe nada. */
    gex_scene_diagnostic_view(NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* Nivel real                                                          */
/* ------------------------------------------------------------------ */

static void test_aztec_real(void)
{
    gex_scene_level scene;
    FILE *fh = fopen(AZTEC_PATH, "rb");
    size_t i;

    if (fh == NULL) {
        printf("skip aztec (no file)\n");
        return;
    }
    fclose(fh);

    memset(&scene, 0, sizeof(scene));
    CHECK(gex_scene_level_load(&scene, AZTEC_PATH) == GEX_SCENE_OK);
    CHECK(scene.loaded == 1);

    /* Estadísticas del nivel. */
    CHECK(scene.stats.container_entries == 16082u);
    CHECK(scene.stats.descriptor_vertices == 3882u);
    CHECK(scene.stats.descriptor_polygons == 9609u);
    CHECK(scene.stats.leaf_count == 815u);
    /* La ruta lejana emite el registro principal y la pasada de niebla. */
    CHECK(scene.stats.triangles == 2854u);
    CHECK(scene.stats.fog_triangles == 1427u);
    CHECK(scene.stats.vertices == 4914u);
    CHECK(scene.stats.indices == 8562u);
    CHECK(scene.stats.skipped_clipped == 0u);
    /* El encuadre se recentró: el centro de pantalla ya no es el del
     * original y la vista se mantiene dentro de la escala base. */
    CHECK(scene.stats.view_cx != 256);
    CHECK(scene.stats.view_cy != 192);
    CHECK(scene.stats.view_scale > 0);

    /* La geometría es coherente con las estadísticas. */
    CHECK(scene.geometry.triangle_count == scene.stats.triangles);
    CHECK(scene.geometry.index_count == scene.stats.indices);
    CHECK(scene.geometry.vertex_count == scene.stats.vertices);
    if (scene.geometry.indices != NULL) {
        for (i = 0; i < scene.geometry.index_count; i++) {
            CHECK(scene.geometry.indices[i] < scene.geometry.vertex_count);
        }
    }

    /* Determinismo: dos cargas dan la misma geometría. */
    {
        gex_scene_level other;
        memset(&other, 0, sizeof(other));
        CHECK(gex_scene_level_load(&other, AZTEC_PATH) == GEX_SCENE_OK);
        CHECK(other.stats.triangles == scene.stats.triangles);
        CHECK(other.stats.vertices == scene.stats.vertices);
        CHECK(other.stats.indices == scene.stats.indices);
        CHECK(memcmp(other.geometry.indices, scene.geometry.indices,
                     scene.geometry.index_count * sizeof(uint32_t)) == 0);
        gex_scene_level_free(&other);
        /* Idempotente. */
        gex_scene_level_free(&other);
    }

    printf("AZTEC16: entries=%u vertices=%u polygons=%u leaves=%u "
           "triangles=%zu fog=%zu geom_vertices=%zu indices=%zu\n",
           scene.stats.container_entries, scene.stats.descriptor_vertices,
           scene.stats.descriptor_polygons, scene.stats.leaf_count,
           scene.stats.triangles, scene.stats.fog_triangles,
           scene.stats.vertices, scene.stats.indices);

    gex_scene_level_free(&scene);
    CHECK(scene.loaded == 0);
    CHECK(scene.geometry.vertices == NULL);
    CHECK(scene.emit.tris == NULL);
    CHECK(scene.leaves.leaves == NULL);
    CHECK(scene.container.image == NULL);
    gex_scene_level_free(&scene);
}


/* ---------------------------------------------------------------------------
 * Modo de camara original con el escenario MAP7.
 *
 * La escena se carga con la camara del juego (matriz de emision cam+0xE0,
 * prueba de esfera del BSP y la misma proyeccion) y debe reproducir lo que
 * el GEX3D.EXE original deja para el frame 0:
 *
 *   - las mismas 307 hojas visibles, en el mismo orden;
 *   - la misma matriz de emision y traslacion, bit a bit;
 *   - los mismos 1123 triangulos, en el mismo orden de dibujo.
 * ------------------------------------------------------------------------- */
static void test_map7_camera_mode(void)
{
    gex_scene_level scene;
    gex_camera cam;
    FILE *f = fopen(MAP7_PATH, "rb");
    int i;

    if (!f) {
        printf("skip map7 camera mode (no file)\n");
        return;
    }
    fclose(f);

    memset(&scene, 0, sizeof(scene));
    gex_scene_reference_camera(&cam);

    /* La camara de referencia es la del frame 0 capturado. */
    CHECK(cam.pos[0] == REF_FRAMES[0].pos[0]);
    CHECK(cam.pos[1] == REF_FRAMES[0].pos[1]);
    CHECK(cam.pos[2] == REF_FRAMES[0].pos[2]);
    CHECK(cam.euler[0] == REF_FRAMES[0].euler[0]);
    CHECK(cam.euler[1] == REF_FRAMES[0].euler[1]);
    CHECK(cam.euler[2] == REF_FRAMES[0].euler[2]);
    CHECK(cam.focal == 320);
    CHECK(cam.width == 384);
    CHECK(cam.proj_cx == 256);
    CHECK(cam.proj_cy == 192);
    CHECK(cam.near_z == 160);

    CHECK(gex_scene_level_load_camera(&scene, MAP7_PATH, &cam) == GEX_SCENE_OK);
    CHECK(scene.loaded == 1);
    CHECK(scene.stats.camera_mode == 1);

    /* Hojas visibles: las mismas y en el mismo orden. */
    CHECK(scene.stats.leaf_count == REF_FULL_FRAME0_COUNT);
    if (scene.leaves.count == REF_FULL_FRAME0_COUNT) {
        for (i = 0; i < (int)REF_FULL_FRAME0_COUNT; i++) {
            CHECK(scene.leaves.leaves[i] == REF_FULL_0[i]);
        }
    }

    /* Matriz de emision: identica a la capturada del original. */
    for (i = 0; i < 9; i++) {
        CHECK(scene.stats.emit_m[i] == REF_EMIT_M[0][i]);
    }
    CHECK(scene.stats.emit_t[0] == REF_EMIT_T[0][0]);
    CHECK(scene.stats.emit_t[1] == REF_EMIT_T[0][1]);
    CHECK(scene.stats.emit_t[2] == REF_EMIT_T[0][2]);

    /* Triangulos: el mismo numero y el mismo orden de dibujo. */
    CHECK(scene.stats.triangles == REF_EMIT_ACCEPTED[0]);
    if (scene.stats.triangles == REF_EMIT_ACCEPTED[0]) {
        for (i = 0; i < (int)REF_EMIT_ACCEPTED[0]; i++) {
            const gex_vertex *v0 =
                &scene.geometry.vertices[scene.geometry.indices[i * 3 + 0]];
            const gex_vertex *v1 =
                &scene.geometry.vertices[scene.geometry.indices[i * 3 + 1]];
            const gex_vertex *v2 =
                &scene.geometry.vertices[scene.geometry.indices[i * 3 + 2]];
            CHECK((int32_t)v0->z == REF_ORDER[i][0]);
            CHECK((int32_t)v1->z == REF_ORDER[i][1]);
            CHECK((int32_t)v2->z == REF_ORDER[i][2]);
        }
    }

    gex_scene_level_free(&scene);
    CHECK(scene.loaded == 0);
    CHECK(scene.geometry.vertices == NULL);
    CHECK(scene.emit.tris == NULL);
    CHECK(scene.leaves.leaves == NULL);
}

int main(void)
{
    test_status_names();
    test_bad_arguments();
    test_load_errors();
    test_diagnostic_view();
    test_aztec_real();
    test_map7_camera_mode();

    printf("test_scene_level: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
