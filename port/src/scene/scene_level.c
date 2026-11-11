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


/* ------------------------------------------------------------------ */
/* Camara original del juego                                           */
/* ------------------------------------------------------------------ */

/*
 * Camara de referencia del escenario MAP7.
 *
 * Estado capturado del objeto de camara del GEX3D.EXE (0x0065CD80) en el
 * frame 0 del escenario `map7.dfx` (el mismo nivel que carga el original),
 * con el breakpoint de hardware en 0x004269C2:
 *
 *   cam+0x00..0x05  posicion   (2341, 4071, 3074)
 *   cam+0x30..0x35  Euler      (-397, 0, 2802)   Q12 de vuelta
 *   cam+0x40        focal      0x140 (320)
 *   cam+0x48        ancho      0x180 (384)
 *   cam+0x5C        corte lejano 8500
 *   cam+0x490/492/494 escala por fila de la matriz de emision 0x1000
 *   cam+0x43C       bandera de base identica 0
 *
 * Los parametros de proyeccion (centro 256/192 y plano cercano 160) los fija
 * el juego una vez en 0x4636DD..0x4636FB y no dependen de la camara.
 */
void gex_scene_reference_camera(gex_camera *cam) {
    if (cam == NULL) {
        return;
    }
    memset(cam, 0, sizeof(*cam));
    cam->pos[0] = 2341;
    cam->pos[1] = 4071;
    cam->pos[2] = 3074;
    cam->euler[0] = -397;
    cam->euler[1] = 0;
    cam->euler[2] = 2802;
    cam->focal = 0x140;       /* 320 */
    cam->field_0x44 = 0x200;  /* 512 */
    cam->width = 0x180;       /* 384 */
    cam->far_plane = 8500;
    cam->row_scale[0] = 0x1000;
    cam->row_scale[1] = 0x1000;
    cam->row_scale[2] = 0x1000;
    cam->proj_cx = 256;       /* [0x646814] */
    cam->proj_cy = 192;       /* [0x646818] */
    cam->near_z = 160;        /* [0x6474E8] */
    /* Marco local, O, M, A/B, a_cam y b_cam (la cadena completa). */
    gex_cam_update(cam);
}

/*
 * Matriz de emision y niebla de la camara original (escenario MAP7).
 *
 * La matriz es la de `cam+0xE0` reconstruida por
 * `gex_cam_build_emit_matrix` (0x46DD40): las mismas nueve palabras que el
 * juego copia a 0x646BE0 y la misma traslacion que deja en +0x14/+0x18/+0x1C.
 *
 * Los parametros de niebla son los capturados del original en ese frame:
 * umbral 6299 ([0x65BF80]), split 0xFFFF ([0x65BF04], emisor cercano siempre),
 * A = -93584512 ([0x647564]) y B = 14848 ([0x647570]). El factor por vertice
 * se evalua con la formula analitica (el juego usa su LUT de [0x64FEA8], que
 * el port no reproduce todavia: solo afecta al color, no a la geometria).
 */
static void scene_camera_view(const gex_camera *cam, gex_emit_matrix *mat,
                               gex_emit_fog *fog) {
    gex_cam_emit_matrix em;

    if (mat != NULL) {
        memset(mat, 0, sizeof(*mat));
        if (cam != NULL && gex_cam_build_emit_matrix(cam, &em) == 0) {
            memcpy(mat->m, em.m, sizeof(mat->m));
            mat->t[0] = em.t[0];
            mat->t[1] = em.t[1];
            mat->t[2] = em.t[2];
            mat->focal = cam->focal;
            mat->cx = cam->proj_cx;
            mat->cy = cam->proj_cy;
            mat->near_z = cam->near_z;
        }
    }
    if (fog != NULL) {
        static const uint8_t fog_rgb[3] = {0, 0, 0}; /* [0x6474C4] en el frame 0 */
        memset(fog, 0, sizeof(*fog));
        memcpy(fog->fog_rgb, fog_rgb, sizeof(fog->fog_rgb));
        fog->threshold = 6299;       /* [0x65BF80] */
        fog->split_z = 0xFFFF;       /* [0x65BF04]: emisor cercano */
        fog->a_q12 = -93584512;      /* [0x647564] */
        fog->b_q12 = 14848;          /* [0x647570] */
        fog->have_lut = 0;
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
    scene->stats.camera_mode = 0;

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

gex_scene_status gex_scene_level_load_camera(gex_scene_level *scene,
                                             const char *path,
                                             const gex_camera *cam) {
    gex_emit_matrix mat;
    gex_emit_fog fog;
    gex_dfx_status dst;
    gex_bsp_status bst;
    gex_emit_status est;
    gex_geom_status gst;

    if (scene == NULL || path == NULL || path[0] == '\0' || cam == NULL) {
        return GEX_SCENE_ERR_ARG;
    }
    if (scene->loaded) {
        return GEX_SCENE_ERR_ARG;
    }

    /* 1. Contenedor .DFX y descriptor del nivel (igual que en el modo
     *    diagnostico). */
    dst = gex_dfx_load(path, &scene->container);
    if (dst != GEX_DFX_OK) {
        return GEX_SCENE_ERR_LOAD;
    }
    scene->container_loaded = 1;
    scene->stats.container_entries = scene->container.count;

    dst = gex_dfx_load_level(&scene->container, &scene->level);
    if (dst != GEX_DFX_OK) {
        return GEX_SCENE_ERR_LEVEL;
    }
    scene->level_loaded = 1;
    scene->stats.descriptor_vertices = scene->level.descriptor.vertex_count;
    scene->stats.descriptor_polygons = scene->level.descriptor.polygon_count;

    /* 2. Recorrido del BSP con la camara original: la prueba de esfera
     *    contra los seis semiplanos de A/B (0x00401C00) y el orden de
     *    apilado por plano. */
    bst = gex_bsp_walk(scene->container.image, scene->container.image_size,
                       scene->level.descriptor.root_bsp, scene_visible_all,
                       NULL, cam, &scene->leaves);
    if (bst != GEX_BSP_OK) {
        return GEX_SCENE_ERR_BSP;
    }
    scene->leaves_loaded = 1;
    scene->stats.leaf_count = (uint32_t)scene->leaves.count;

    /* 3. Emision con la matriz de la camara: una sola pasada, sin encuadre
     *    automatico. */
    scene_camera_view(cam, &mat, &fog);
    est = gex_level_emit(scene->container.image, scene->container.image_size,
                         &scene->level.descriptor, &scene->leaves, &mat, &fog,
                         0, &scene->emit);
    if (est != GEX_EMIT_OK) {
        return GEX_SCENE_ERR_EMIT;
    }
    scene->emit_loaded = 1;
    scene->stats.triangles = scene->emit.count;
    scene->stats.fog_triangles = scene->emit.fog_pass_count;
    scene->stats.view_cx = mat.cx;
    scene->stats.view_cy = mat.cy;
    scene->stats.view_scale = mat.m[0];
    scene->stats.camera_mode = 1;
    memcpy(scene->stats.emit_m, mat.m, sizeof(mat.m));
    scene->stats.emit_t[0] = mat.t[0];
    scene->stats.emit_t[1] = mat.t[1];
    scene->stats.emit_t[2] = mat.t[2];
    scene->stats.proj_focal = mat.focal;
    scene->stats.proj_near_z = mat.near_z;

    /* 4. Conversion a vertices e indices. */
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
