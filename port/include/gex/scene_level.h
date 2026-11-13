/*
 * gex/scene_level.h - carga de un nivel real (.DFX) hasta geometría
 * (portable).
 *
 * Es el módulo 5B: encadena los módulos que ya existen, sin duplicar
 * ninguna implementación:
 *
 *   gex_dfx_load / gex_dfx_load_level   -> contenedor y descriptor
 *   gex_bsp_walk                        -> hojas visibles del BSP
 *   gex_level_emit                      -> triángulos emitidos
 *   gex_geometry_build                  -> vértices e índices
 *
 * La conversión a buffers D3D11 no vive aquí (es Windows-only y ya existe en
 * `gex/render/d3d11_mesh.h`): este módulo se queda en la CPU.
 *
 * Portable C11: sin Windows, sin D3D11, sin Win32.
 */

#ifndef GEX_SCENE_LEVEL_H
#define GEX_SCENE_LEVEL_H

#include <stddef.h>
#include <stdint.h>

#include "gex/asset/dfx.h"
#include "gex/camera.h"
#include "gex/camera_path.h"
#include "gex/bsp.h"
#include "gex/emit.h"
#include "gex/geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Estadísticas de la carga: lo que la aplicación imprime y las pruebas
 * comprueban. */
typedef struct gex_scene_stats {
    uint32_t container_entries;   /* entradas del contenedor .DFX */
    uint32_t descriptor_vertices;  /* vértices del descriptor del nivel */
    uint32_t descriptor_polygons;  /* polígonos del descriptor del nivel */
    uint32_t leaf_count;           /* hojas del BSP recorridas */
    size_t   triangles;            /* triángulos emitidos */
    size_t   fog_triangles;        /* de ellos, pasadas de niebla */
    size_t   vertices;             /* vértices de la geometría */
    size_t   indices;              /* índices de la geometría */
    size_t   skipped_clipped;      /* descartados por profundidad */
    int32_t  view_cx;              /* centro de pantalla de la vista usada */
    int32_t  view_cy;
    int16_t  view_scale;
    /* Modo de vista: 1 = camara original del juego, 0 = vista diagnostica. */
    int      camera_mode;
    /* Matriz de emision usada (cam+0xE0): 9 int16 Q12 + traslacion. */
    int16_t  emit_m[9];
    int32_t  emit_t[3];
    /* Parametros de proyeccion usados. */
    int32_t  proj_focal;
    int32_t  proj_near_z;           /* escala Q12 de la vista (m[0] = m[4]) */
} gex_scene_stats;

typedef enum gex_scene_status {
    GEX_SCENE_OK = 0,
    GEX_SCENE_ERR_ARG,         /* ruta nula */
    GEX_SCENE_ERR_LOAD,        /* el .DFX no se pudo cargar */
    GEX_SCENE_ERR_LEVEL,       /* no se pudo obtener el descriptor */
    GEX_SCENE_ERR_BSP,         /* falló el recorrido del BSP */
    GEX_SCENE_ERR_EMIT,        /* falló la emisión de polígonos */
    GEX_SCENE_ERR_GEOMETRY,    /* falló la conversión a geometría */
    GEX_SCENE_ERR_PATH,        /* el nivel no tiene trayectoria de cámara */
    GEX_SCENE_ERR_NOMEM        /* falta memoria */
} gex_scene_status;

/*
 * Estado de una escena cargada. Cada bloque tiene su propia bandera para que
 * `gex_scene_level_free` libere exactamente lo que se llegó a crear, incluso
 * si la carga falló a mitad.
 */
typedef struct gex_scene_level {
    gex_dfx_container container;
    int container_loaded;
    gex_dfx_level level;
    int level_loaded;
    gex_bsp_walk_result leaves;
    int leaves_loaded;
    gex_emit_output emit;
    int emit_loaded;
    gex_geometry geometry;
    int geometry_loaded;
    /* Trayectoria de camara del nivel (S-3), si la tiene. */
    gex_dfx_camera_path dfx_path;
    int path_loaded;
    gex_camera_path_node path_node;
    uint32_t path_frames;   /* muestras producidas hasta ahora */
    int path_finished;      /* el centinela ya se alcanzo */
    /* Segunda trayectoria del nivel: el punto de mira del look-at. */
    gex_dfx_camera_path dfx_lookat_path;
    int lookat_loaded;
    gex_camera_path_node lookat_node;
    int lookat_finished;    /* su centinela ya se alcanzo */
    int16_t lookat_last[3]; /* ultimo objetivo valido (se conserva) */
    gex_scene_stats stats;
    int loaded;
} gex_scene_level;

/*
 * Vista diagnóstica determinista para ver la geometría de un nivel.
 *
 * **NO es la cámara del juego.** Es una matriz de emisión sintética y
 * reproducible (escala 0,25 en los tres ejes y traslación de profundidad
 * +0x1000) más una niebla generada con el mismo generador 0x461895 que usa
 * la prueba de emisión. Sirve para producir una imagen estable y auditable
 * del nivel; no sustituye a la cámara original, cuya validación dinámica
 * contra el ejecutable sigue pendiente.
 *
 * La visibilidad del BSP es "todas las hojas": la prueba de esfera de la
 * cámara original (`cam_ctx`) necesita el estado de cámara del juego, que el
 * port todavía no tiene.
 */
/*
 * Cámara de referencia del juego para el escenario MAP7.
 *
 * Estado capturado del objeto de cámara del GEX3D.EXE (0x0065CD80) en el
 * frame 0 del escenario `map7.dfx`, el mismo nivel que carga el original:
 * posición (2341, 4071, 3074), ángulos Euler (-397, 0, 2802) en Q12 de
 * vuelta, focal 0x140 (320), ancho 0x180 (384) y escalas de fila 0x1000.
 * Los parámetros de proyección (centro 256/192 y plano cercano 160) son
 * los que el juego fija una vez en 0x4636DD..0x4636FB.
 *
 * Deja la cámara lista para usar (marco local, O, M, A/B, a_cam y b_cam).
 */
void gex_scene_reference_camera(gex_camera *cam);

void gex_scene_diagnostic_view(gex_emit_matrix *mat, gex_emit_fog *fog);

/*
 * Carga un nivel completo: `.DFX` -> descriptor -> BSP -> emisión ->
 * geometría. En éxito, `scene->geometry` es la geometría lista para
 * `gex_d3d11_mesh_create`.
 *
 * Ante un error devuelve el estado correspondiente y deja la escena
 * liberable con `gex_scene_level_free` (que es idempotente).
 */
gex_scene_status gex_scene_level_load(gex_scene_level *scene, const char *path);

/*
 * Carga un nivel con la camara original del juego:
 *   .DFX -> descriptor -> BSP con la prueba de esfera de la camara ->
 *   emision con la matriz de emision de `cam+0xE0` -> geometria.
 *
 * No hay matriz sintetica ni encuadre automatico: la geometria queda donde
 * la deja el original, con la misma proyeccion (focal, centro y plano
 * cercano) y el mismo split de emisor.
 */
gex_scene_status gex_scene_level_load_camera(gex_scene_level *scene,
                                             const char *path,
                                             const gex_camera *cam);

/*
 * Carga un nivel con la camara original **y las dos trayectorias** que el
 * propio nivel trae en su `.DFX` (MAP7: 26 + 26 registros de 32 B).
 *
 * La trayectoria de posicion se lee del slot 0 (array 0xC034) y la de mira
 * del slot 1 (array 0xC39C), ambas con `gex_dfx_load_camera_path_slot`, que
 * sigue la cadena de punteros documentada (S3_MENU_CAMERA_SEQUENCE.md 4.5).
 * Los registros quedan en la escena (`scene->dfx_path` y
 * `scene->dfx_lookat_path`) y cada nodo se inicializa en el estado original
 * (indice 0, fraccion 0).
 *
 * Si el nivel no tiene esas estructuras, devuelve GEX_SCENE_ERR_PATH: no se
 * inventa una trayectoria.
 */
gex_scene_status gex_scene_level_load_camera_path(gex_scene_level *scene,
                                                  const char *path,
                                                  const gex_camera *cam);

/*
 * Avanza **una actualizacion logica** de las dos trayectorias y actualiza la
 * camara.
 *
 * Hace exactamente lo que hace el original por frame:
 *   1. un paso de `gex_camera_path` en la trayectoria de posicion y otro en
 *      la de mira (nodos independientes, cada uno con su avance);
 *   2. escribe la posicion en `cam` y calcula la orientacion con el look-at
 *      entero (`gex_cam_look_at`, 0x406515 -> 0x40AE51) desde esa posicion
 *      al punto de mira;
 *   3. recalcula las matrices derivadas (O, M, A/B, a_cam/b_cam) con
 *      `gex_cam_update`;
 *   4. vuelve a emitir el nivel con la camara nueva (BSP + emision +
 *      geometria), de modo que la imagen refleje el movimiento.
 *
 * Devuelve 1 si hubo muestra nueva, 0 si la trayectoria de posicion ya
 * termino (el centinela) y -1 si fallo la re-emision. Tras el centinela no
 * se escribe nada mas: la ultima posicion y la ultima orientacion se
 * conservan.
 */
int gex_scene_level_camera_path_step(gex_scene_level *scene, gex_camera *cam);

/*
 * Recorre **toda** la trayectoria en una escena propia y cuenta quantas
 * veces aparece cada indice de textura (14 bits) en la geometria emitida.
 *
 * Motivo: el atlas de texturas tiene un maximo de entradas
 * (`GEX_D3D11_TEXTURE_MAX_ENTRIES`) y el vuelo de MAP7 toca mas texturas
 * distintas que las que caben. Esta funcion permite elegir las mas usadas
 * **sin tocar el modulo de texturas ni el shader**: el llamador decide que
 * indices pide al banco del `.VFX`.
 *
 * `counts` recibe un contador por indice de 14 bits (`counts_len` entradas,
 * 16384 para el rango completo). Devuelve el numero de indices distintos
 * vistos, o 0 si no se pudo recorrer la trayectoria.
 *
 * No toca la escena del llamante: usa la suya propia y la libera.
 */
gex_scene_status gex_scene_camera_path_texture_usage(const char *level_path,
                                                     const gex_camera *cam,
                                                     uint32_t *counts,
                                                     size_t counts_len,
                                                     size_t *distinct);

/* Libera todo lo que la escena haya creado. Idempotente. */
void gex_scene_level_free(gex_scene_level *scene);

const char *gex_scene_status_name(gex_scene_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_SCENE_LEVEL_H */
