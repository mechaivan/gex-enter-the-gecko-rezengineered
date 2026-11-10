/*
 * gex/emit.h - Extracción, transformación y clasificación de polígonos BSP
 * (S-2, gex_level_emit).
 *
 * Reproduce la ruta de emisión original de nivel
 * (0x43DC3A -> 0x43DED0 -> 0x401C00 -> 0x4269C2 -> emisores 0x426256 /
 * 0x426588 -> buckets -> 0x469836 -> 0x46558A) reconstruida estáticamente en
 * S2_BSP_TRAVERSAL.md §5 y U1H_RESULTADOS.md §21.
 *
 * Principios del contrato (docs/D3D11_FIRST_SCENE_PLAN.md §11):
 *  - Matriz de emisión propia: cam+0xE0 (9 int16 Q12 row-major +
 *    traslación int32[3]). NO es gex_camera.m (cam+0xE8, visibilidad).
 *  - Fases separadas: collect -> project -> classify -> subdivide -> sort.
 *  - Capacidad dinámica segura; errores explícitos.
 *  - Solo C11/stdint/stddef: nada de D3D11, Win32 ni Glide.
 *
 * Todo lo que reproduce el ejecutable está marcado como tal; las decisiones de
 * seguridad o simplificación del port se documentan junto al código.
 */

#ifndef GEX_EMIT_H
#define GEX_EMIT_H

#include <stddef.h>
#include <stdint.h>

#include "gex/asset/dfx.h"
#include "gex/bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Matriz de emisión: el buffer de 0x20 B apuntado por cam+0xE0.
 * NO es gex_cam_mat3 y NO es la matriz de visibilidad cam+0xE8.
 * ======================================================================== */
typedef struct {
    int16_t m[9];     /* row-major Q12: fila 0 = x de pantalla, 1 = y, 2 = profundidad */
    int32_t t[3];     /* traslación en unidades de profundidad */
    int32_t focal;    /* [0x646850] = 0x140 (320) */
    int32_t cx, cy;   /* [0x646814] = 0x100 (256), [0x646818] = 0xC0 (192) */
    int32_t near_z;   /* [0x6474E8] = 0xA0 (160) */
} gex_emit_matrix;

/* ========================================================================
 * Niebla: LUT de 3464 entradas [0x64FEA8] y umbrales [0x65BF80]/[0x65BF04].
 * ======================================================================== */
typedef struct {
    int16_t lut[3464];   /* [0x64FEA8]; índice z >> 2; relleno por 0x461895 */
    uint8_t fog_rgb[3];  /* +0x40/+0x41/+0x42 */
    int32_t fog_total_z; /* +0x4C: profundidad de niebla total */
    int32_t fog_none_z;  /* +0x4E: profundidad de niebla nula */
    int32_t threshold;   /* [0x65BF80]: niebla por vértice a partir de aquí */
    int32_t split_z;     /* [0x65BF04]: emisor lejano a partir de aquí */
    int have_lut;        /* 0 => usar la fórmula analítica a_q12/b_q12 */
    int32_t a_q12, b_q12;/* [0x647564]<<12 y [0x647570] */
} gex_emit_fog;

/* Factor de niebla para una profundidad z (0 .. 0x1000).
 * Con LUT: índice z >> 2 acotado a [0, 3463] (el original no lo acota).
 * Sin LUT: clamp(b + A / z, 0, 0x1000) con A = a_q12 >> 12. */
int gex_emit_fog_factor(const gex_emit_fog *fog, int32_t z);

/* ========================================================================
 * Triángulo normalizado de salida.
 * ======================================================================== */
#define GEX_EMIT_F_CLIPPED_XY 0x01u  /* xy proyectado fuera de [-0x400,0x3FF] */
#define GEX_EMIT_F_SUBDIVIDED 0x02u  /* proviene de una subdivisión */
#define GEX_EMIT_F_FAR_EMITTER 0x04u /* emisor lejano 0x426588 */
#define GEX_EMIT_F_NEAR_EMITTER 0x08u/* emisor cercano 0x426256 */
#define GEX_EMIT_F_FOG_PASS   0x10u  /* segunda pasada: niebla pura (base blanca) */

typedef struct {
    uint32_t color[3];     /* AARRGGBB por vértice, niebla aplicada si procede */
    int16_t  x[3], y[3];   /* píxeles proyectados (crudos, sin recorte a 10 bits) */
    int32_t  z[3];         /* profundidad de emisión (int32, sin recorte) */
    uint8_t  u[3], v[3];   /* UV de 8 bits */
    uint16_t texture;      /* índice de textura, 14 bits */
    uint8_t  layer;        /* byte[uv+0x0B]: desplazamiento de bucket */
    uint8_t  flags;        /* GEX_EMIT_F_* */
    uint32_t source_poly;  /* índice global del polígono de origen (trazabilidad) */
    uint32_t seq;          /* secuencia de inserción (desempate LIFO en el sort) */
} gex_emit_triangle;

typedef struct {
    gex_emit_triangle *tris;
    size_t             count;
    size_t             capacity;
    size_t             fog_pass_count; /* subconjunto con GEX_EMIT_F_FOG_PASS */
    int                overflow;      /* !=0 si se forzó parada por capacidad */
} gex_emit_output;

/* ========================================================================
 * Polígono extraído (fase 1: hojas -> polígonos, sin transformar).
 * ======================================================================== */
typedef struct {
    uint16_t vtx[3];       /* índices de vértice en la tabla (stride 12) */
    uint32_t color[3];     /* dword[vtx+8] crudo */
    uint8_t  u[3], v[3];
    uint16_t texture;      /* word de textura del registro UV, 14 bits */
    uint8_t  layer;        /* byte[uv+0x0B] */
    uint16_t poly_flags;   /* +0x06 del polígono */
    uint32_t source_poly;  /* índice global del polígono de origen */
} gex_emit_poly;

typedef struct {
    gex_emit_poly *polys;
    size_t         count;
    size_t         capacity;
} gex_emit_poly_list;

/* ========================================================================
 * Clasificación (fase 3).
 * ======================================================================== */
typedef enum {
    GEX_EMIT_NEAR = 0, /* 0x426256: cercano o forzado, sin niebla */
    GEX_EMIT_FAR   = 1 /* 0x426588: lejano, con pasada de niebla */
} gex_emit_emitter;

typedef struct {
    int32_t            maxz;
    int                subdivide;   /* maxz < 0x6A4 */
    int                fog_applies; /* maxz >= threshold */
    gex_emit_emitter   emitter;
    uint32_t           fog_color[3];/* color con niebla aplicada por vértice */
} gex_emit_class;

/* ========================================================================
 * Errores.
 * ======================================================================== */
typedef enum {
    GEX_EMIT_OK = 0,
    GEX_EMIT_ERR_ARG,        /* puntero nulo o descriptor incoherente */
    GEX_EMIT_ERR_RANGE,      /* offset/índice fuera de la imagen DFX */
    GEX_EMIT_ERR_INCONSIST,  /* conteos o tablas que no cuadran */
    GEX_EMIT_ERR_NOMEM,      /* fallo de reserva */
    GEX_EMIT_ERR_OVERFLOW    /* capacidad máxima de seguridad alcanzada */
} gex_emit_status;

/* ========================================================================
 * Fases públicas (ninguna llama a otra implícitamente).
 * ======================================================================== */

/* Fase 1: hojas visibles -> polígonos (sin transformar).
 * `leaves` contiene offsets dentro de la imagen; nunca se desreferencian
 * como punteros absolutos. */
/* Fase 1: hojas visibles -> polígonos (sin transformar).
 * `image`/`image_size` son la imagen DFX cargada; los offsets de hoja y de
 * polígono son offsets dentro de ella, nunca punteros absolutos [C]. */
gex_emit_status gex_emit_collect(const uint8_t *image, size_t image_size,
                                 const gex_dfx_descriptor *desc,
                                 const gex_bsp_walk_result *leaves,
                                 gex_emit_poly_list *out);

/* Fase 2: transformar y proyectar UN polígono (0x46A1D0).
 * Devuelve GEX_EMIT_OK y rellena `tri`; si el polígono se clipea por
 * profundidad, tri->flags |= GEX_EMIT_F_CLIPPED_XY y el llamador lo salta. */
gex_emit_status gex_emit_project(const gex_emit_matrix *mat,
                                 const int16_t vtx[9],   /* x0,y0,z0,x1,y1,z1,x2,y2,z2 */
                                 const uint32_t color[3],
                                 const uint8_t u[3], const uint8_t v[3],
                                 uint16_t texture, uint8_t layer,
                                 uint32_t source_poly,
                                 gex_emit_triangle *tri);

/* Fase 3: clasificar (determinante 2D + umbrales) y decidir emisor. */
gex_emit_status gex_emit_classify(const gex_emit_fog *fog,
                                  const gex_emit_triangle *p,
                                  uint16_t poly_flags,
                                  gex_emit_class *cls);

/* Fase 4: subdividir en cuatro triángulos (0x6A4), solo emisor cercano.
 * Recibe los vértices locales del polígono y sus atributos. */
gex_emit_status gex_emit_subdivide(const gex_emit_matrix *mat,
                                   const gex_emit_fog *fog,
                                   int fog_applies,
                                   const int16_t vtx[9],
                                   const uint32_t color[3],
                                   const uint8_t u[3], const uint8_t v[3],
                                   uint16_t texture, uint8_t layer,
                                   uint32_t source_poly,
                                   gex_emit_output *out);

/* Fase 5: ordenar por (maxz >> 2) + 4 * layer, descendente; LIFO dentro
 * de una misma clave (el último emitido se dibuja primero). */
gex_emit_status gex_emit_sort(gex_emit_triangle *tris, size_t count);

/* ========================================================================
 * Orquestador.
 * ======================================================================== */
/* Orquestador. `image`/`image_size`: imagen DFX cargada; `desc`: descriptor
 * de nivel (64 B). */
gex_emit_status gex_level_emit(const uint8_t *image, size_t image_size,
                               const gex_dfx_descriptor *desc,
                               const gex_bsp_walk_result *leaves,
                               const gex_emit_matrix *mat,
                               const gex_emit_fog *fog,
                               gex_emit_output *out);

void gex_emit_output_free(gex_emit_output *out);
void gex_emit_poly_list_free(gex_emit_poly_list *list);
const char *gex_emit_status_name(gex_emit_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_EMIT_H */
