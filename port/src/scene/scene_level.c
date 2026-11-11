/*
 * scene_level.c - carga de un nivel real (.DFX) hasta geometría (portable).
 *
 * Véase gex/scene_level.h. Encadena los módulos existentes (DFX, BSP, emisión
 * y geometría) sin duplicar ninguna implementación. Sin Windows ni D3D11.
 */

#include "gex/scene_level.h"

#include <stdlib.h>
#include <string.h>

/* Predicado de visibilidad del BSP: todas las hojas. La visibilidad real de
 * la cámara (prueba de esfera de `gex_camera`) necesita el estado de cámara
 * del juego, que el port todavía no tiene; usarla aquí sería inventar. */
static int scene_visible_all(const uint8_t *image, size_t image_size,
                              uint32_t node_offset, void *ctx)
{
    (void)image;
    (void)image_size;
    (void)node_offset;
    (void)ctx;
    return 1;
}

void gex_scene_diagnostic_view(gex_emit_matrix *mat, gex_emit_fog *fog)
{
    static const uint8_t fog_rgb[3] = {40, 60, 90};

    if (mat != NULL) {
        memset(mat, 0, sizeof(*mat));
        /* Matriz de emisión sintética: escala 0,25 (0x400 en Q12) en los tres
         * ejes y traslación de profundidad +0x1000. Con cx=256 y cy=192 el
         * centro de pantalla cae donde el original lo tenía. */
        mat->m[0] = 0x0400;
        mat->m[4] = 0x0400;
        mat->m[8] = 0x0400;
        mat->focal = 0x140;  /* 320 */
        mat->cx = 0x100;     /* 256 */
        mat->cy = 0x0C0;     /* 192 */
        mat->near_z = 0xA0;  /* 160 */
        mat->t[2] = 0x1000;
    }
    if (fog != NULL) {
        /* Niebla con el generador 0x461895 y parametros ajustados al rango
         * de profundidad real de la vista diagnostica (z entre 3053 y 4896 en
         * AZTEC16): con total_z=400 y none_z=100 la niebla saturaba al
         * 100 % en todo el nivel y la imagen salia de un solo color.
         *
         * El generador da factor 0 en z <= none_z y 4096 (niebla total) en
         * z >= total_z, asi que con none_z=3000 y total_z=5000 el degrade va
         * de 0 (cerca) a 100 % (lejos) justo en el rango de la vista.
         *
         * split_z = 0 selecciona la ruta del emisor lejano (registro
         * principal + pasada de niebla), la misma que usa la prueba de
         * emision. */
        gex_emit_fog_generate(fog, 5000, 3000, fog_rgb);
        fog->split_z = 0;
    }
}

/*
 * Encuadre de la vista diagnóstica.
 *
 * La primera emisión deja la geometría donde cae con cx=256 y cy=192, que
 * son el centro de pantalla del original pero no el del nivel (en AZTEC16 la
 * geometría occupies x 18..259 e y 124..408 del espacio lógico 512x384:
 * queda a un lado y se sale por abajo). Se recentra desplazando cx y cy, que
 * son un desplazamiento en pantalla puro dentro de la proyección.
 *
 * Es determinista: depende solo de la emisión anterior.
 */
/*
 * Encuadre de la vista diagnóstica.
 *
 * La primera emisión deja la geometría donde cae con cx=256 y cy=192, que
 * son el centro de pantalla del original pero no el del nivel (en AZTEC16 la
 * geometría ocupa x 18..259 e y 124..408 del espacio lógico 512x384:
 * queda a un lado y se sale por abajo). Se recentra desplazando cx y cy, que
 * son un desplazamiento en pantalla puro dentro de la proyección.
 *
 * El encuadre se calcula sobre la **unión** de las cajas de las emisiones
 * sucesivas: recentrar hace entrar polígonos nuevos por el gate, así que la
 * caja crece y una única estimación no basta.
 *
 * Es determinista: depende solo de las emisiones anteriores.
 */
typedef struct scene_box {
    int32_t min_x, max_x, min_y, max_y;
} scene_box;

static void scene_box_add(scene_box *box, const gex_emit_output *emit)
{
    size_t i;

    for (i = 0; i < emit->count; i++) {
        const gex_emit_triangle *t = &emit->tris[i];
        int c;
        if (t->flags & GEX_EMIT_F_DEPTH_CLIPPED) {
            continue;
        }
        for (c = 0; c < 3; c++) {
            if (t->x[c] < box->min_x) box->min_x = t->x[c];
            if (t->x[c] > box->max_x) box->max_x = t->x[c];
            if (t->y[c] < box->min_y) box->min_y = t->y[c];
            if (t->y[c] > box->max_y) box->max_y = t->y[c];
        }
    }
}

static void scene_frame_view(gex_emit_matrix *mat, const scene_box *box)
{
    int32_t center_x, center_y;
    int32_t width, height;
    int32_t fit;

    if (box->min_x > box->max_x || box->min_y > box->max_y) {
        return; /* nada emitido: no hay nada que encuadrar */
    }
    center_x = (box->min_x + box->max_x) / 2;
    center_y = (box->min_y + box->max_y) / 2;
    /* cx y cy son el centro de pantalla de la proyección: desplazarlos mueve
     * la imagen en pantalla sin tocar la perspectiva. */
    mat->cx = 256 + (256 - center_x);
    mat->cy = 192 + (192 - center_y);

    /* Si el nivel no cabe en la pantalla lógica, se reduce la escala (m[0] y
     * m[4] escalan por igual, así que la perspectiva no se deforma). */
    width = box->max_x - box->min_x + 1;
    height = box->max_y - box->min_y + 1;
    fit = 0x1000;
    if (width > 512) {
        fit = (int32_t)((int64_t)0x1000 * 512 / width);
    }
    if (height > 384 && (int32_t)((int64_t)0x1000 * 384 / height) < fit) {
        fit = (int32_t)((int64_t)0x1000 * 384 / height);
    }
    if (fit < 0x1000) {
        /* Escala absoluta: se parte siempre de la escala base de la vista
         * diagnóstica (0x400 en Q12), no de la iteración anterior. */
        mat->m[0] = (int16_t)(((int32_t)0x400 * fit) / 0x1000);
        mat->m[4] = mat->m[0];
    }
}

gex_scene_status gex_scene_level_load(gex_scene_level *scene, const char *path)
{
    gex_emit_matrix mat;
    gex_emit_fog fog;
    gex_dfx_status dst;
    gex_bsp_status bst;
    gex_emit_status est;
    gex_geom_status gst;
    int round;

    if (scene == NULL || path == NULL || path[0] == '\0') {
        return GEX_SCENE_ERR_ARG;
    }
    if (scene->loaded) {
        return GEX_SCENE_ERR_ARG;
    }

    /* 1. Contenedor .DFX. */
    dst = gex_dfx_load(path, &scene->container);
    if (dst != GEX_DFX_OK) {
        return GEX_SCENE_ERR_LOAD;
    }
    scene->container_loaded = 1;
    scene->stats.container_entries = scene->container.count;

    /* 2. Descriptor del nivel. */
    dst = gex_dfx_load_level(&scene->container, &scene->level);
    if (dst != GEX_DFX_OK) {
        return GEX_SCENE_ERR_LEVEL;
    }
    scene->level_loaded = 1;
    scene->stats.descriptor_vertices = scene->level.descriptor.vertex_count;
    scene->stats.descriptor_polygons = scene->level.descriptor.polygon_count;

    /* 3. Recorrido del BSP. */
    bst = gex_bsp_walk(scene->container.image, scene->container.image_size,
                       scene->level.descriptor.root_bsp, scene_visible_all,
                       NULL, NULL, &scene->leaves);
    if (bst != GEX_BSP_OK) {
        return GEX_SCENE_ERR_BSP;
    }
    scene->leaves_loaded = 1;
    scene->stats.leaf_count = (uint32_t)scene->leaves.count;

    /* 4. Emisión de polígonos (triángulos). Se hacen varias pasadas: cada una
     *    sitúa la geometría y la siguiente recentra el encuadre (el
     *    desplazamiento cambia qué polígonos pasan el gate, así que hace
     *    falta iterar hasta que el encuadre deja de cambiar). */
    gex_scene_diagnostic_view(&mat, &fog);
    {
        scene_box box;
        box.min_x = 0x7FFFFFFF;
        box.max_x = -0x7FFFFFFF;
        box.min_y = 0x7FFFFFFF;
        box.max_y = -0x7FFFFFFF;
        for (round = 0; round < 4; round++) {
            if (scene->emit_loaded) {
                gex_emit_output_free(&scene->emit);
                scene->emit_loaded = 0;
            }
            est = gex_level_emit(scene->container.image,
                                 scene->container.image_size,
                                 &scene->level.descriptor, &scene->leaves, &mat,
                                 &fog, 0, &scene->emit);
            if (est != GEX_EMIT_OK) {
                return GEX_SCENE_ERR_EMIT;
            }
            scene->emit_loaded = 1;
            scene_box_add(&box, &scene->emit);
            if (round < 3) {
                scene_frame_view(&mat, &box);
            }
        }
    }
    scene->stats.triangles = scene->emit.count;
    scene->stats.fog_triangles = scene->emit.fog_pass_count;
    scene->stats.view_cx = mat.cx;
    scene->stats.view_cy = mat.cy;
    scene->stats.view_scale = mat.m[0];

    /* 5. Conversión a vértices e índices. */
    gst = gex_geometry_build(&scene->emit, &scene->geometry);
    if (gst != GEX_GEOM_OK) {
        return GEX_SCENE_ERR_GEOMETRY;
    }
    scene->geometry_loaded = 1;
    scene->stats.vertices = scene->geometry.vertex_count;
    scene->stats.indices = scene->geometry.index_count;
    scene->stats.skipped_clipped = scene->geometry.skipped_clipped;

    scene->loaded = 1;
    return GEX_SCENE_OK;
}

void gex_scene_level_free(gex_scene_level *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->geometry_loaded) {
        gex_geometry_free(&scene->geometry);
        scene->geometry_loaded = 0;
    }
    if (scene->emit_loaded) {
        gex_emit_output_free(&scene->emit);
        scene->emit_loaded = 0;
    }
    if (scene->leaves_loaded) {
        gex_bsp_walk_free(&scene->leaves);
        scene->leaves_loaded = 0;
    }
    if (scene->level_loaded) {
        gex_dfx_release_level(&scene->level);
        scene->level_loaded = 0;
    }
    if (scene->container_loaded) {
        gex_dfx_release_container(&scene->container);
        scene->container_loaded = 0;
    }
    memset(&scene->stats, 0, sizeof(scene->stats));
    scene->loaded = 0;
}

const char *gex_scene_status_name(gex_scene_status status)
{
    switch (status) {
    case GEX_SCENE_OK:
        return "ok";
    case GEX_SCENE_ERR_ARG:
        return "bad argument";
    case GEX_SCENE_ERR_LOAD:
        return "level file could not be loaded";
    case GEX_SCENE_ERR_LEVEL:
        return "level descriptor could not be read";
    case GEX_SCENE_ERR_BSP:
        return "BSP traversal failed";
    case GEX_SCENE_ERR_EMIT:
        return "polygon emission failed";
    case GEX_SCENE_ERR_GEOMETRY:
        return "geometry conversion failed";
    case GEX_SCENE_ERR_NOMEM:
        return "out of memory";
    }
    return "unknown";
}
