/*
 * menu.c - escena del menú del televisor (S-4, portable).
 *
 * Véase gex/menu.h. Todos los números de colocación son **medidos**: salen del
 * registro de animación (0x20 B por subparte) que el original tiene para cada
 * actor del menú de MAP7 y que se volcó en ejecución con el trazador
 * (docs/S3_MENU_CAMERA_SEQUENCE.md § 11).
 *
 * Formato del registro de animación (medido):
 *
 *   +0x00  9 x int16   base ortonormal Q12 del actor, por **columnas**:
 *                      col0 = eje X local, col1 = eje Y, col2 = eje Z
 *   +0x12  int16       relleno
 *   +0x14  int32       x de mundo
 *   +0x18  int32       y de mundo
 *   +0x1C  int32       z de mundo
 *
 * El original transforma las tres columnas con la matriz de cámara
 * (0x4698B0) y proyecta la posición con 0x469B02; el port reproduce el mismo
 * resultado pasando los vértices de mundo por `gex_emit_project`, que es la
 * proyección del nivel.
 */

#include "gex/menu.h"

#include <stdlib.h>
#include <string.h>

/* Color del panel del televisor cuando no hay textura. El original dibuja
 * este polígono sin registro UV (uv_ptr = 0x20800080), así que no hay textura
 * que reproducir: se usa el color oscuro de la carcasa del CRT. */
#define MENU_PANEL_COLOR 0xFF101018u

/* Colocación medida de un actor del menú. */
typedef struct menu_actor_placement {
    int16_t basis[3][3];   /* columnas X, Y, Z en Q12 */
    int32_t position[3];   /* +0x14, +0x18, +0x1C del registro */
    uint16_t texture;      /* índice normal; el resaltado es texture + 4 */
} menu_actor_placement;

/*
 * Panel del televisor (`tvmenu__`, modelo 0): caja cerrada de 8 vértices y
 * 12 triángulos, del `.DFX` (0xDFF04). Base y posición medidas: el panel queda
 * **fuera** del encuadre del menú (se proyecta por encima y a la derecha de la
 * pantalla), así que no tapa la escena; se dibuja igualmente porque es el
 * actor que el original recorre.
 */
static const int16_t kPanelVertex[8][3] = {
    {-302, -156, 418}, {283, -156, 418}, {283, 199, 418}, {-302, 199, 418},
    {-302, -156, 0},   {283, -156, 0},   {283, 199, 0},   {-302, 199, 0},
};

static const uint8_t kPanelTriangle[12][3] = {
    {0, 1, 2}, {0, 2, 3}, {0, 4, 5}, {0, 5, 1}, {1, 5, 6}, {1, 6, 2},
    {2, 6, 7}, {2, 7, 3}, {3, 7, 4}, {3, 4, 0}, {4, 7, 6}, {4, 6, 5},
};

static const menu_actor_placement kPanelPlacement = {
    {{-3977, 976, 0}, {-976, -3977, 0}, {0, 0, 4096}},
    {2900, -84, -528},
    0,
};

/*
 * Rótulos: cada botón (`tvbutn__`, modelos 2 y 3) es el mismo quad de
 * 4 vértices y 2 triángulos, en el plano XZ local (y = 0). Vértices medidos
 * del `.DFX` (0xDD820 / 0xDD9A0).
 */
static const int16_t kLabelVertex[4][3] = {
    {61, 0, 0}, {61, 0, 27}, {-59, 0, 27}, {-59, 0, 0},
};

static const uint8_t kLabelTriangle[2][3] = {
    {0, 1, 2}, {0, 2, 3},
};

/* UV medidos del registro de 12 B (0xDD8E4/0xDD934): el rótulo ocupa las 32
 * filas superiores de una página de 128x128, es decir v 0..63 de 0..255.
 * u = 255 es el borde derecho del texto y v = 0 el superior.
 *
 * Escala (S-5): los bytes de UV del `.DFX` **no** se dividen. El original
 * escribe `s = byte / 16` con `oow = 1/16` (0x4661A6..0x4661B8 y 0x4684EF),
 * es decir `s = byte`, y el espacio de coordenadas de Glide es
 * **256 unidades = lado mayor de la textura** (medido en el SDK:
 * `test17.c`/`qatest00.c` usan `255.0f * oow` para la textura entera, y la
 * ruta de fuentes 0x432BE2 usa `*256/16` con `oow = 1.0`). Para 128x32 eso
 * da `u = byte * 128/256 = byte/2` y `v = byte/2`: el registro (255, 63)
 * recorre la textura exactamente una vez. El shader aplica esa escala
 * (`uv * max(w,h)`); aquí solo se guarda el byte medido. */
static const uint8_t kLabelU[4] = {255, 255, 0, 0};
static const uint8_t kLabelV[4] = {63, 0, 0, 63};

/*
 * Los cuatro botones comparten la **misma** base medida (los cuatro registros
 * traen los mismos nueve int16): el quad mira a la cámara del menú. Solo
 * cambian la posición y la textura.
 *
 *   X local = ( 663, -4041, 0) / 4096   (ancho del rótulo)
 *   Y local = (4041,   663, 0) / 4096   (no se usa: el modelo es plano)
 *   Z local = (   0,     0, 4096) / 4096 (alto del rótulo, hacia arriba)
 */
static const menu_actor_placement kLabelPlacement[GEX_MENU_LABEL_COUNT] = {
    {{{663, -4041, 0}, {4041, 663, 0}, {0, 0, 4096}}, {2758, 412, 198}, 600},
    {{{663, -4041, 0}, {4041, 663, 0}, {0, 0, 4096}}, {2758, 412, 170}, 603},
    {{{663, -4041, 0}, {4041, 663, 0}, {0, 0, 4096}}, {2758, 412, 143}, 602},
    {{{663, -4041, 0}, {4041, 663, 0}, {0, 0, 4096}}, {2758, 412, 115}, 601},
};

static int menu_reserve(gex_menu_scene *scene, size_t extra)
{
    size_t needed = scene->count + extra;
    gex_menu_triangle *grown;

    if (needed <= scene->capacity) {
        return 1;
    }
    if (scene->capacity == 0) {
        scene->capacity = 32;
    }
    while (scene->capacity < needed) {
        if (scene->capacity > (size_t)-1 / 2u) {
            return 0;
        }
        scene->capacity *= 2;
    }
    if (scene->capacity > (size_t)-1 / sizeof(*scene->tris)) {
        return 0;
    }
    grown = (gex_menu_triangle *)realloc(
        scene->tris, scene->capacity * sizeof(*scene->tris));
    if (grown == NULL) {
        return 0;
    }
    scene->tris = grown;
    return 1;
}

/* mundo = posicion + (base * local) >> 12, con la base por columnas.
 *
 * El desplazamiento es el `sar` de 32 bits del original (`0x4698B0`), escrito
 * de forma definida en C para cualquier compilador: `>>` sobre negativos es
 * implementación, no contrato. */
static int32_t menu_sar12(int32_t v)
{
    return (v >= 0) ? (v >> 12) : ~(~v >> 12);
}

static void menu_transform(const menu_actor_placement *place,
                           const int16_t local[3], int16_t out[3])
{
    int i;

    for (i = 0; i < 3; i++) {
        int32_t acc = (int32_t)place->basis[0][i] * local[0] +
                      (int32_t)place->basis[1][i] * local[1] +
                      (int32_t)place->basis[2][i] * local[2];
        acc = menu_sar12(acc) + place->position[i];
        if (acc > 32767) {
            acc = 32767;
        } else if (acc < -32768) {
            acc = -32768;
        }
        out[i] = (int16_t)acc;
    }
}

static int menu_add_triangle(gex_menu_scene *scene, const int16_t v[3][3],
                             const uint8_t u[3], const uint8_t v_uv[3],
                             uint16_t texture, uint32_t color)
{
    gex_menu_triangle *t;

    if (!menu_reserve(scene, 1)) {
        return 0;
    }
    t = &scene->tris[scene->count++];
    memcpy(t->v, v, sizeof(t->v));
    memcpy(t->u, u, sizeof(t->u));
    memcpy(t->v_uv, v_uv, sizeof(t->v_uv));
    t->texture = texture;
    t->color = color;
    return 1;
}

int gex_menu_scene_measured(gex_menu_scene *scene, int selected)
{
    int16_t world[8][3];
    uint8_t u[3], v_uv[3];
    size_t i;

    if (scene == NULL) {
        return 0;
    }
    memset(scene, 0, sizeof(*scene));
    scene->selected = (selected >= 0 && selected < (int)GEX_MENU_LABEL_COUNT)
                          ? selected
                          : -1;

    /* Panel: caja sin textura. */
    for (i = 0; i < 8; i++) {
        menu_transform(&kPanelPlacement, kPanelVertex[i], world[i]);
    }
    for (i = 0; i < 12; i++) {
        int16_t v[3][3];
        int k;
        for (k = 0; k < 3; k++) {
            int idx = kPanelTriangle[i][k];
            v[k][0] = world[idx][0];
            v[k][1] = world[idx][1];
            v[k][2] = world[idx][2];
        }
        memset(u, 0, sizeof(u));
        memset(v_uv, 0, sizeof(v_uv));
        if (!menu_add_triangle(scene, v, u, v_uv, 0, MENU_PANEL_COLOR)) {
            gex_menu_scene_free(scene);
            return 0;
        }
    }

    /* Rótulos: dos triángulos por botón. */
    for (i = 0; i < GEX_MENU_LABEL_COUNT; i++) {
        int16_t lw[4][3];
        int k;
        uint16_t texture = kLabelPlacement[i].texture;

        if ((int)i == scene->selected) {
            texture = (uint16_t)(texture + 4u); /* variante resaltada */
        }
        for (k = 0; k < 4; k++) {
            menu_transform(&kLabelPlacement[i], kLabelVertex[k], lw[k]);
        }
        for (k = 0; k < 2; k++) {
            int16_t v[3][3];
            uint8_t tu[3], tv[3];
            int c;
            for (c = 0; c < 3; c++) {
                int idx = kLabelTriangle[k][c];
                v[c][0] = lw[idx][0];
                v[c][1] = lw[idx][1];
                v[c][2] = lw[idx][2];
                tu[c] = kLabelU[idx];
                tv[c] = kLabelV[idx];
            }
            if (!menu_add_triangle(scene, v, tu, tv, texture, 0xFFFFFFFFu)) {
                gex_menu_scene_free(scene);
                return 0;
            }
        }
    }
    return 1;
}

void gex_menu_scene_free(gex_menu_scene *scene)
{
    if (scene == NULL) {
        return;
    }
    free(scene->tris);
    scene->tris = NULL;
    scene->count = 0;
    scene->capacity = 0;
    scene->selected = -1;
}

gex_emit_status gex_menu_emit(const gex_menu_scene *scene,
                              const gex_emit_matrix *mat,
                              gex_emit_output *out)
{
    size_t i;

    if (scene == NULL || mat == NULL || out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    for (i = 0; i < scene->count; i++) {
        const gex_menu_triangle *t = &scene->tris[i];
        int16_t flat[9];
        uint32_t color[3];
        gex_emit_triangle tri;
        gex_emit_status st;
        int c;

        for (c = 0; c < 3; c++) {
            flat[c * 3 + 0] = t->v[c][0];
            flat[c * 3 + 1] = t->v[c][1];
            flat[c * 3 + 2] = t->v[c][2];
            color[c] = t->color;
        }
        st = gex_emit_project(mat, flat, color, t->u, t->v_uv, t->texture, 0,
                              (uint32_t)i, &tri);
        if (st != GEX_EMIT_OK) {
            return st;
        }
        if (tri.flags & GEX_EMIT_F_DEPTH_CLIPPED) {
            continue; /* detrás del plano cercano: el original no lo dibuja */
        }
        if (gex_emit_output_push(out, &tri) != GEX_EMIT_OK) {
            return GEX_EMIT_ERR_NOMEM;
        }
    }
    return GEX_EMIT_OK;
}
