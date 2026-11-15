/*
 * gex/menu.h - escena del menú principal del televisor (S-4, portable).
 *
 * El menú de GEX no es un overlay 2D: es un actor (`tvmenu__`, la pantalla
 * del televisor) más cuatro botones (`tvbutn__`) colocados en el mundo, que
 * el original dibuja con su ruta de actores (0x471552 -> 0x4718CF / 0x470F96)
 * y proyecta con la matriz de emisión de la cámara (0x4698B0 + 0x469B02).
 *
 * Este módulo **no inventa** ni la geometría ni la colocación:
 *
 *   - Los vértices del panel y de los rótulos son los que el original tiene
 *     en el registro de animación de cada actor, volcados en ejecución con
 *     el trazador (ver docs/S3_MENU_CAMERA_SEQUENCE.md § 11). Están en
 *     coordenadas de **mundo**.
 *   - Las texturas (600..607) y los UV son los del `.VFX` y el `.DFX`,
 *     leídos por `gex/actor.h`.
 *   - La proyección usa `gex_emit_project`, la misma que el BSP.
 *
 * Lo que NO reproduce todavía: el billboard 0x470C7B (la reflexión de
 * Householder que el original aplica al registro antes de proyectar) ni la
 * selección de modelo por estado. La escena medida es la del menú en reposo
 * con START seleccionado, que es la que se capturó del original.
 */

#ifndef GEX_MENU_H
#define GEX_MENU_H

#include <stddef.h>
#include <stdint.h>

#include "gex/emit.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Número de rótulos del menú: START, LOAD, OPTIONS, QUIT. */
#define GEX_MENU_LABEL_COUNT 4u

/* Triángulo de la escena del menú, en mundo. */
typedef struct gex_menu_triangle {
    int16_t  v[3][3];    /* tres vértices en mundo */
    uint8_t  u[3], v_uv[3];
    uint16_t texture;    /* 0 = sin textura (panel) */
    uint32_t color;      /* AARRGGBB cuando no hay textura */
} gex_menu_triangle;

typedef struct gex_menu_scene {
    gex_menu_triangle *tris;
    size_t             count;
    size_t             capacity;
    /* Índice del rótulo resaltado (0 = START), o -1 si ninguno. */
    int                selected;
} gex_menu_scene;

/*
 * Rellena la escena con la medida del menú de MAP7: el panel del televisor
 * (caja de 8 vértices, 12 triángulos, sin textura) y los cuatro rótulos
 * (dos triángulos cada uno, con la textura del modelo normal o resaltado).
 *
 * `selected` elige el rótulo que usa la variante resaltada (+4 en el índice
 * de textura: 604, 605, 606, 607).
 *
 * Devuelve 0 si no hay memoria.
 */
int gex_menu_scene_measured(gex_menu_scene *scene, int selected);

void gex_menu_scene_free(gex_menu_scene *scene);

/*
 * Proyecta la escena con la matriz de emisión y añade los triángulos a
 * `out` (que puede traer ya la geometría del nivel: el menú se dibuja
 * encima, en el mismo buffer).
 */
gex_emit_status gex_menu_emit(const gex_menu_scene *scene,
                              const gex_emit_matrix *mat,
                              gex_emit_output *out);

#ifdef __cplusplus
}
#endif

#endif /* GEX_MENU_H */
