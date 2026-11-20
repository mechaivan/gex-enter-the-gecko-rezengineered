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

/* ------------------------------------------------------------------ */
/* Fondo de pantalla (S-11)                                           */
/* ------------------------------------------------------------------ */

static void test_background_color(void)
{
    float rgba[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
    unsigned r, g, b;

    gex_scene_background_rgba(NULL); /* no debe tocar nada ni fallar */

    gex_scene_background_rgba(rgba);
    /* El clear de D3D11 escribe UNORM: 0..1 * 255 debe devolver el byte
     * medido en el original sin desviarse. */
    r = (unsigned)(rgba[0] * 255.0f + 0.5f);
    g = (unsigned)(rgba[1] * 255.0f + 0.5f);
    b = (unsigned)(rgba[2] * 255.0f + 0.5f);
    CHECK(r == GEX_SCENE_BACKGROUND_R);
    CHECK(g == GEX_SCENE_BACKGROUND_G);
    CHECK(b == GEX_SCENE_BACKGROUND_B);
    CHECK(r == 8u && g == 9u && b == 40u); /* medido en la grabacion */
    CHECK(rgba[3] == 1.0f);
    /* El azul de marca anterior (51,51,204) queda explicitamente descartado. */
    CHECK(!(r == 51u && g == 51u && b == 204u));
    CHECK(rgba[0] >= 0.0f && rgba[0] <= 1.0f);
    CHECK(rgba[1] >= 0.0f && rgba[1] <= 1.0f);
    CHECK(rgba[2] >= 0.0f && rgba[2] <= 1.0f);
}

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
    /* El numero de vertices cambio al corregir el offset del indice de
     * textura del registro UV (la textura forma parte de la clave de
     * deduplicacion de vertices). */
    CHECK(scene.stats.vertices == 5148u);
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
 * S-10: la visibilidad del BSP depende de la camara en todo el vuelo.
 *
 * La escena recorre el BSP con la prueba de esfera original en cada paso, no
 * solo en la pose inicial. Se contrasta con los 18 fotogramas medidos del
 * ejecutable (`ref_map7_frames.h`) y se comprueba que:
 *
 *   - el conjunto visible cambia con la camara (nunca son las 1483 hojas);
 *   - el estado despues de N pasos coincide con el fotograma medido N-1
 *     (el primer paso repite la clave 0, como en la trayectoria medida);
 *   - la escena reconstruye la cadena de orientacion aunque el llamador le
 *     pase una camara con A/B/a_cam/b_cam vacios;
 *   - dos escenas independientes dan la misma lista (determinismo).
 * ------------------------------------------------------------------------- */
static void test_map7_visibility_flight(void)
{
    gex_scene_level scene, other;
    gex_camera cam, cam2;
    gex_bsp_walk_result all;
    FILE *f = fopen(MAP7_PATH, "rb");
    uint32_t seen[REF_FRAME_COUNT];
    int distinct = 0;
    int i, s, steps, total_steps;

    if (!f) {
        printf("skip map7 visibility flight (no file)\n");
        return;
    }
    fclose(f);

    memset(&scene, 0, sizeof(scene));
    gex_scene_reference_camera(&cam);
    CHECK(gex_scene_level_load_camera_path(&scene, MAP7_PATH, &cam) ==
          GEX_SCENE_OK);

    /* Todas las hojas del nivel (sin camara), para acotar la comparacion. */
    memset(&all, 0, sizeof(all));
    CHECK(gex_bsp_walk(scene.container.image, scene.container.image_size,
                       scene.level.descriptor.root_bsp, NULL, NULL, NULL,
                       &all) == GEX_BSP_OK);
    CHECK(all.count > REF_FRAMES[0].count);

    /* Estado inicial = fotograma medido 0. */
    CHECK(cam.pos[0] == REF_FRAMES[0].pos[0]);
    CHECK(cam.pos[1] == REF_FRAMES[0].pos[1]);
    CHECK(cam.pos[2] == REF_FRAMES[0].pos[2]);
    CHECK(scene.leaves.count == REF_FRAMES[0].count);

    /* El primer paso repite la clave 0 (comportamiento de la trayectoria
     * medida): el estado antes de pasos ya es el fotograma 0 y el paso 0
     * vuelve a producirlo. Desde el paso 1 cada paso avanza un fotograma. */
    total_steps = 0;
    for (i = 0; i < REF_FRAME_COUNT; i++) {
        int dup = 0;
        CHECK(gex_scene_level_camera_path_step(&scene, &cam) == 1);
        total_steps++;
        CHECK(cam.pos[0] == REF_FRAMES[i].pos[0]);
        CHECK(cam.pos[1] == REF_FRAMES[i].pos[1]);
        CHECK(cam.pos[2] == REF_FRAMES[i].pos[2]);
        CHECK(scene.leaves.count == REF_FRAMES[i].count);
        CHECK(scene.stats.camera_mode == 1);
        for (s = 0; s < 16 && s < (int)scene.leaves.count; s++) {
            CHECK(scene.leaves.leaves[s] == REF_FRAMES[i].first[s]);
        }
        CHECK(scene.leaves.count < all.count);

        for (s = 0; s < distinct; s++) {
            if (seen[s] == scene.leaves.count) {
                dup = 1;
            }
        }
        if (!dup) {
            seen[distinct++] = scene.leaves.count;
        }
    }
    /* El conteo no es constante: el recorrido realmente se rehace por frame. */
    CHECK(distinct >= 5);

    /* Pose final de la trayectoria. */
    for (steps = 0; steps < 4096; steps++) {
        if (gex_scene_level_camera_path_step(&scene, &cam) <= 0) {
            break;
        }
        total_steps++;
    }
    CHECK(total_steps == 428);
    CHECK(steps == 410);
    CHECK(scene.path_frames == 428u);
    CHECK(cam.pos[0] == 2243 && cam.pos[1] == 487 && cam.pos[2] == 196);
    /* 249 hojas visibles frente a las 1483 del nivel. */
    CHECK(scene.leaves.count == 249u);
    CHECK(scene.leaves.count < all.count);
    CHECK(scene.stats.triangles == 1722u);

    /* Determinismo: otra escena, mismas 20 muestras, misma lista. */
    memset(&other, 0, sizeof(other));
    gex_scene_reference_camera(&cam2);
    CHECK(gex_scene_level_load_camera_path(&other, MAP7_PATH, &cam2) ==
          GEX_SCENE_OK);
    for (steps = 0; steps < 20; steps++) {
        CHECK(gex_scene_level_camera_path_step(&other, &cam2) == 1);
    }
    CHECK(other.leaves.count > 0);
    {
        /* Compara con un tercer barrido de la primera escena hasta la muestra
         * 20 (misma camara que `other`). */
        gex_scene_level again;
        gex_camera cam3;
        memset(&again, 0, sizeof(again));
        gex_scene_reference_camera(&cam3);
        CHECK(gex_scene_level_load_camera_path(&again, MAP7_PATH, &cam3) ==
              GEX_SCENE_OK);
        for (steps = 0; steps < 20; steps++) {
            CHECK(gex_scene_level_camera_path_step(&again, &cam3) == 1);
        }
        CHECK(again.leaves.count == other.leaves.count);
        CHECK(memcmp(again.leaves.leaves, other.leaves.leaves,
                     (size_t)again.leaves.count * sizeof(uint32_t)) == 0);

        /* Recuento intermedio del vuelo (pose congelada 101 y 278): el
         * conjunto visible cambia y es mucho menor que las 1483 hojas. */
        while (steps < 101) {
            CHECK(gex_scene_level_camera_path_step(&again, &cam3) == 1);
            steps++;
        }
        CHECK(cam3.pos[0] == 1609 && cam3.pos[1] == 584 &&
              cam3.pos[2] == 567);
        CHECK(again.leaves.count == 122u);
        CHECK(again.stats.triangles == 414u);
        while (steps < 278) {
            CHECK(gex_scene_level_camera_path_step(&again, &cam3) == 1);
            steps++;
        }
        CHECK(cam3.pos[0] == 8430 && cam3.pos[1] == -1239 &&
              cam3.pos[2] == 1187);
        CHECK(again.leaves.count == 119u);
        CHECK(again.stats.triangles == 437u);
        gex_scene_level_free(&again);
    }

    /* La escena reconstruye la cadena de orientacion: una camara con A/B,
     * a_cam y b_cam vacios debe dar el mismo conjunto visible. */
    {
        gex_scene_level stale;
        gex_camera sc;
        memset(&stale, 0, sizeof(stale));
        gex_scene_reference_camera(&sc);
        memset(sc.mat_a, 0, sizeof(sc.mat_a));
        memset(sc.mat_b, 0, sizeof(sc.mat_b));
        memset(sc.a_cam, 0, sizeof(sc.a_cam));
        memset(sc.b_cam, 0, sizeof(sc.b_cam));
        CHECK(gex_scene_level_load_camera(&stale, MAP7_PATH, &sc) ==
              GEX_SCENE_OK);
        CHECK(stale.stats.camera_mode == 1);
        CHECK(stale.leaves.count == REF_FRAMES[0].count);
        CHECK(stale.leaves.count < all.count);
        gex_scene_level_free(&stale);
    }

    gex_scene_level_free(&other);
    gex_bsp_walk_free(&all);
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

/* ------------------------------------------------------------------ */
/* S-6: orden del menú dentro del frame                               */
/* ------------------------------------------------------------------ */

/* Clave de bucket del original: `(maxz >> 2) + 4 * layer`, descendente. */
static int32_t tri_key(const gex_emit_triangle *t)
{
    int32_t m = t->z[0];
    if (t->z[1] > m) m = t->z[1];
    if (t->z[2] > m) m = t->z[2];
    return (m >> 2) + (int32_t)t->layer * 4;
}

/*
 * El menú no se dibuja por encima del nivel. Medido en MAP7: el panel sin
 * textura tiene clave 291 y el suelo 212, así que el suelo se dibuja después y
 * lo tapa (el original muestra suelo en toda la proyección del panel); los
 * rótulos, en cambio, quedan encima (el original muestra su color sobre
 * `poly#116`, clave 25). Por eso el panel entra en el ordenado y los rótulos se
 * añaden al final.
 */
static void test_map7_menu_order(void)
{
    gex_scene_level scene;
    gex_camera cam;
    FILE *f = fopen(MAP7_PATH, "rb");
    size_t before, i;
    size_t last_panel = 0, first_label = 0, floor_poly = 0;
    int have_panel = 0, have_label = 0, have_floor = 0;
    int steps = 0;
    static const uint16_t label_textures[8] = {
        604, 604, 603, 603, 602, 602, 601, 601
    };

    if (!f) {
        printf("skip map7 menu order (no file)\n");
        return;
    }
    fclose(f);

    memset(&scene, 0, sizeof(scene));
    gex_scene_reference_camera(&cam);
    CHECK(gex_scene_level_load_camera_path(&scene, MAP7_PATH, &cam) ==
          GEX_SCENE_OK);
    while (steps < 4096 &&
           gex_scene_level_camera_path_step(&scene, &cam) > 0) {
        steps++;
    }
    /* Fotograma de referencia del vuelo: 428 muestras. */
    CHECK(scene.path_frames == 428u);
    before = scene.emit.count;
    CHECK(before > 0u);

    CHECK(gex_scene_level_add_menu(&scene, 0) == GEX_SCENE_OK);
    CHECK(scene.emit.count == before + 20u); /* 12 panel + 8 rótulos */
    CHECK(scene.geometry_loaded == 1);
    CHECK(scene.stats.triangles == scene.emit.count);

    /* Localiza el panel de mayor clave, el primer rótulo y el suelo bajo el
     * panel (`poly#136`, ganador del píxel (341,341) en la captura). */
    for (i = 0; i < scene.emit.count; i++) {
        const gex_emit_triangle *t = &scene.emit.tris[i];
        if (t->texture == 0u && t->color[0] == 0xFF08080Cu) {
            if (!have_panel || tri_key(t) > tri_key(&scene.emit.tris[last_panel])) {
                last_panel = i;
            }
            have_panel = 1;
        } else if (t->color[0] == 0xFFFFFFFFu && t->texture >= 601u &&
                   t->texture <= 604u) {
            if (!have_label) {
                first_label = i;
                have_label = 1;
            }
        } else if (t->source_poly == 136u && t->texture == 265u) {
            if (!have_floor) {
                floor_poly = i;
                have_floor = 1;
            }
        }
    }
    CHECK(have_panel == 1);
    CHECK(have_label == 1);
    CHECK(have_floor == 1);

    /* El panel se ordena CON el nivel: la parte ordenada (todo menos los ocho
     * rótulos) está en orden descendente de clave. */
    for (i = 1; i + 8u < scene.emit.count; i++) {
        CHECK(tri_key(&scene.emit.tris[i - 1]) >=
              tri_key(&scene.emit.tris[i]));
    }
    /* Y el panel de mayor clave queda detrás del suelo que lo tapa. */
    CHECK(last_panel < floor_poly);
    CHECK(tri_key(&scene.emit.tris[last_panel]) >
          tri_key(&scene.emit.tris[floor_poly]));

    /* ...y los rótulos son los últimos ocho, en el orden medido. */
    CHECK(first_label + 8u == scene.emit.count);
    for (i = 0; i < 8u; i++) {
        CHECK(scene.emit.tris[first_label + i].texture == label_textures[i]);
    }
    for (i = first_label; i < scene.emit.count; i++) {
        CHECK(scene.emit.tris[i].source_poly >= 12u);
        CHECK(scene.emit.tris[i].source_poly < 20u);
    }

    gex_scene_level_free(&scene);
    CHECK(scene.emit.tris == NULL);
}

int main(void)
{
    test_background_color();
    test_status_names();
    test_bad_arguments();
    test_load_errors();
    test_diagnostic_view();
    test_aztec_real();
    test_map7_camera_mode();
    test_map7_visibility_flight();
    test_map7_menu_order();

    printf("test_scene_level: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
