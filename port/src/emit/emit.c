/*
 * gex/emit.c - Implementación de gex_level_emit (S-2).
 *
 * Reproduce la ruta de emisión de polígonos BSP del nivel reconstruida
 * estáticamente:
 *   0x43DC3A -> 0x43DED0 -> 0x401C00 -> 0x4269C2 -> 0x426256 / 0x426588 ->
 *   buckets -> 0x469836 -> 0x46558A.
 *
 * Evidencia: S2_BSP_TRAVERSAL.md §5, S2_CAMERA_VISIBILITY.md §13,
 * U1H_RESULTADOS.md §21, U1_CONSOLIDACION.md §4.4.
 *
 * Notas de fidelidad al original:
 *   [A] demostrado en el desensamblado.
 *   [B] estructura demostrada, semántica no verificada dinámicamente.
 *   [C] decisión de seguridad / simplificación del port.
 *
 * Todo es estático: sin traza de frames reales del ejecutable, sin D3D11,
 * sin Win32, sin Glide. C11 puro.
 */

#include "gex/emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Constantes del original                                            */
/* ------------------------------------------------------------------ */
#define GEX_EMIT_NEAR_Z     0xA0   /* [0x6474E8] */
#define GEX_EMIT_FOCAL      0x140  /* [0x646850] */
#define GEX_EMIT_CX         0x100  /* [0x646814] */
#define GEX_EMIT_CY         0x0C0  /* [0x646818] */
#define GEX_EMIT_SUBDIV_Z   0x6A4  /* 1700: umbral de subdivisión */
#define GEX_EMIT_LUT_SIZE   3464u  /* 0x3620 entradas */
#define GEX_EMIT_FACTOR_MAX 0x1000
#define GEX_EMIT_XY_MIN     (-0x400)
#define GEX_EMIT_XY_MAX     0x3FF

/* ------------------------------------------------------------------ */
/* Aritmética de 32 bits con wrap x86                                 */
/* ------------------------------------------------------------------ */

/* Producto punto de una fila de la matriz Q12 con un vértice int16.
 * Nueve imul de 32 bits con operandos movsx(int16) + suma de 32 bits;
 * wrap modular sin comprobación de desbordamiento [A]. */
/* Acumulador uint32_t para reproducir el wrap de 32 bits del x86 sin UB de C
 * cuando la suma de los tres productos desborda int32. */
static int32_t dot_row(const int16_t m[3], const int16_t v[3]) {
    uint32_t acc = (uint32_t)((int32_t)m[0] * (int32_t)v[0]) +
                   (uint32_t)((int32_t)m[1] * (int32_t)v[1]) +
                   (uint32_t)((int32_t)m[2] * (int32_t)v[2]);
    return (int32_t)acc;
}

/* sar 12 aritmético (comportamiento x86; en clang/gcc, >> sobre int32_t
 * es sar). */
static int32_t sar12(int32_t v) {
    return v >> 12;
}

/* idiv de 32 bits con truncamiento hacia cero (semántica C99 / x86 idiv).
 * b == 0 devuelve 0: el original no divide si z' == 0 [C]. */
static int32_t idiv32(int32_t a, int32_t b) {
    if (b == 0) return 0;
    return a / b;
}

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t clamp_byte(int32_t v) {
    return (uint8_t)clamp_i32(v, 0, 255);
}

/* ------------------------------------------------------------------ */
/* Fase 2: proyección de un vértice (0x46A1D0)                        */
/* ------------------------------------------------------------------ */

/* Transforma y proyecta UN vértice con la aritmética exacta de 0x46A1D0.
 * Devuelve 0 si pasa el descarte de profundidad; -1 si se clipea
 * (z < near_z || z > 0xFFFF: [0x647578] |= 0x40000) [A]. */
static int project_vertex(const gex_emit_matrix *mat,
                          int16_t x, int16_t y, int16_t z,
                          int32_t *xs, int32_t *ys, int32_t *zp) {
    int32_t cam[3];

    cam[0] = sar12(dot_row(&mat->m[0], (int16_t[3]){x, y, z})) + mat->t[0];
    cam[1] = sar12(dot_row(&mat->m[3], (int16_t[3]){x, y, z})) + mat->t[1];
    cam[2] = sar12(dot_row(&mat->m[6], (int16_t[3]){x, y, z})) + mat->t[2];

    *zp = cam[2];
    if (cam[2] < mat->near_z || cam[2] > 0xFFFF) {
        return -1;
    }

    if (cam[2] != 0) {
        *xs = idiv32((int32_t)cam[0] * mat->focal, cam[2]) + mat->cx;
        *ys = idiv32((int32_t)cam[1] * mat->focal, cam[2]) + mat->cy;
    } else {
        *xs = 0; /* z' == 0: el original conserva el valor previo; no alcanzable
                    con near_z = 0xA0 > 0 [C] */
        *ys = 0;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Niebla                                                             */
/* ------------------------------------------------------------------ */

int gex_emit_fog_factor(const gex_emit_fog *fog, int32_t z) {
    if (fog == NULL) return 0;

    if (fog->have_lut) {
        /* LUT [0x64FEA8], índice z >> 2. El puerto acota el índice a
         * [0, 3463]; el original no lo hace (lectura fuera de tabla para
         * z >= 0x3620) [C]. */
        int32_t idx = z >> 2;
        if (idx < 0) idx = 0;
        if (idx > (int32_t)(GEX_EMIT_LUT_SIZE - 1)) idx = (int32_t)(GEX_EMIT_LUT_SIZE - 1);
        return (int)fog->lut[idx];
    }

    /* Fórmula analítica: clamp((A idiv z) + B, 0, 0x1000),
     * A = [0x647564] = a_q12 >> 12, B = [0x647570] = b_q12 [A]. */
    int32_t A = fog->a_q12 >> 12;
    int32_t f = fog->b_q12 + idiv32(A, z);
    return clamp_i32(f, 0, GEX_EMIT_FACTOR_MAX);
}

/* Mezcla un byte hacia el color de niebla (0x46B727):
 * out = desde + (((hacia - desde) * factor) sar 12), clamp 0..255 [A]. */
static uint8_t mix_byte(uint8_t desde, uint8_t hacia, int factor) {
    int32_t d = (int32_t)hacia - (int32_t)desde;
    int32_t t = (d * factor) >> 12; /* imul 32 bits + sar 12 aritmético */
    return clamp_byte((int32_t)desde + t);
}

/* Aplica niebla a un color (bytes 0..2 = B,G,R en memoria; byte 3 intacto),
 * factor por vértice. */
static uint32_t apply_fog(uint32_t color, const uint8_t fog_rgb[3], int factor) {
    uint8_t *b = (uint8_t *)&color;
    b[0] = mix_byte(b[0], fog_rgb[0], factor);
    b[1] = mix_byte(b[1], fog_rgb[1], factor);
    b[2] = mix_byte(b[2], fog_rgb[2], factor);
    return color;
}

/* Pasada de niebla del emisor lejano: base blanca 0x00FFFFFF [A]. */
static uint32_t fog_pass_color(const uint8_t fog_rgb[3], int factor) {
    return ((uint32_t)mix_byte(255u, fog_rgb[0], factor)) |
           (((uint32_t)mix_byte(255u, fog_rgb[1], factor)) << 8) |
           (((uint32_t)mix_byte(255u, fog_rgb[2], factor)) << 16) |
           0xFF000000u;
}

/* ------------------------------------------------------------------ */
/* Reservas seguras                                                   */
/* ------------------------------------------------------------------ */

static int grow_tris(gex_emit_output *out, size_t need) {
    if (out->capacity >= need) return 0;
    size_t new_cap = out->capacity ? out->capacity : 256;
    while (new_cap < need) {
        if (new_cap > (SIZE_MAX / 2)) return -1;
        new_cap *= 2;
    }
    gex_emit_triangle *tmp = (gex_emit_triangle *)realloc(out->tris,
                                                          new_cap * sizeof(*tmp));
    if (tmp == NULL) return -1;
    out->tris = tmp;
    out->capacity = new_cap;
    return 0;
}

static int push_tri(gex_emit_output *out, const gex_emit_triangle *tri) {
    if (out->count == out->capacity && grow_tris(out, out->count + 1) != 0) {
        out->overflow = 1;
        return -1;
    }
    out->tris[out->count] = *tri;
    out->tris[out->count].seq = (uint32_t)out->count; /* secuencia de inserción */
    out->count++;
    if (tri->flags & GEX_EMIT_F_FOG_PASS) out->fog_pass_count++;
    return 0;
}

static void init_output(gex_emit_output *out) {
    memset(out, 0, sizeof(*out));
}

static int grow_polys(gex_emit_poly_list *list, size_t need) {
    if (list->capacity >= need) return 0;
    size_t new_cap = list->capacity ? list->capacity : 256;
    while (new_cap < need) {
        if (new_cap > (SIZE_MAX / 2)) return -1;
        new_cap *= 2;
    }
    gex_emit_poly *tmp = (gex_emit_poly *)realloc(list->polys,
                                                  new_cap * sizeof(*tmp));
    if (tmp == NULL) return -1;
    list->polys = tmp;
    list->capacity = new_cap;
    return 0;
}

/* Lectura segura dentro de la imagen: offsets, nunca punteros absolutos [C]. */
static int read_u16(const uint8_t *image, size_t image_size,
                    uint32_t off, uint16_t *out) {
    if (off + 2 > image_size) return -1;
    *out = (uint16_t)(image[off] | (uint16_t)(image[off + 1] << 8));
    return 0;
}

static int read_u32(const uint8_t *image, size_t image_size,
                    uint32_t off, uint32_t *out) {
    if (off + 4 > image_size) return -1;
    *out = (uint32_t)image[off] | ((uint32_t)image[off + 1] << 8) |
           ((uint32_t)image[off + 2] << 16) | ((uint32_t)image[off + 3] << 24);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Fase 1: collect                                                    */
/* ------------------------------------------------------------------ */

/* Hoja -> polígonos, según 0x4269C2 (42359-42388):
 *   word[leaf+0x12] & 2  -> prueba de hoja
 *   (int16)word[leaf+0x10] -> npolys (con signo; negativo no itera)
 *   leaf+0x0C -> tabla de polígomos (0x14 B cada uno) [A] */
gex_emit_status gex_emit_collect(const uint8_t *image, size_t image_size,
                                 const gex_dfx_descriptor *desc,
                                 const gex_bsp_walk_result *leaves,
                                 gex_emit_poly_list *out) {
    if (image == NULL || desc == NULL || leaves == NULL || out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    memset(out, 0, sizeof(*out));

    if ((size_t)desc->vertex_table + (size_t)desc->vertex_count * 12u > image_size ||
        (size_t)desc->polygon_table + (size_t)desc->polygon_count * 20u > image_size) {
        return GEX_EMIT_ERR_RANGE;
    }

    /* Cuenta previa para una sola reserva. */
    size_t total = 0;
    for (uint32_t i = 0; i < leaves->count; i++) {
        uint32_t loff = leaves->leaves[i];
        if (loff + 0x14 > image_size) return GEX_EMIT_ERR_RANGE;
        uint16_t leaf_type = 0;
        if (read_u16(image, image_size, loff + 0x12, &leaf_type) != 0)
            return GEX_EMIT_ERR_RANGE;
        if (!(leaf_type & 0x0002u)) continue; /* prueba de hoja [A] */

        uint16_t np_raw = 0;
        if (read_u16(image, image_size, loff + 0x10, &np_raw) != 0)
            return GEX_EMIT_ERR_RANGE;
        int32_t np = (int16_t)np_raw; /* con signo [A] */
        if (np <= 0) continue;
        total += (size_t)np;
    }

    if (total > 0 && grow_polys(out, total) != 0) return GEX_EMIT_ERR_NOMEM;

    uint32_t poly_index = 0;
    for (uint32_t i = 0; i < leaves->count; i++) {
        uint32_t loff = leaves->leaves[i];
        uint16_t leaf_type = 0;
        read_u16(image, image_size, loff + 0x12, &leaf_type);
        if (!(leaf_type & 0x0002u)) continue;

        uint16_t np_raw = 0;
        read_u16(image, image_size, loff + 0x10, &np_raw);
        int32_t np = (int16_t)np_raw;
        if (np <= 0) continue;

        uint32_t poly_off = 0;
        read_u32(image, image_size, loff + 0x0C, &poly_off);
        if (poly_off + (size_t)np * 20u > image_size) return GEX_EMIT_ERR_RANGE;

        for (int32_t k = 0; k < np; k++) {
            uint32_t poff = poly_off + (size_t)k * 20u;
            if (out->count == out->capacity &&
                grow_polys(out, out->count + 1) != 0) {
                return GEX_EMIT_ERR_NOMEM;
            }
            gex_emit_poly *p = &out->polys[out->count++];
            memset(p, 0, sizeof(*p));
            p->source_poly = poly_index++;

            /* Índices de vértice (u16 en poly+0,+2,+4), stride 12 [A] */
            for (int j = 0; j < 3; j++) {
                uint16_t idx = 0;
                if (read_u16(image, image_size, poff + 2u * j, &idx) != 0)
                    return GEX_EMIT_ERR_RANGE;
                p->vtx[j] = idx;
                if (idx >= desc->vertex_count) return GEX_EMIT_ERR_RANGE;
            }

            /* Flags del polígono (u16 en +0x06) [A] */
            uint16_t pf = 0;
            read_u16(image, image_size, poff + 6u, &pf);
            p->poly_flags = pf;

            /* Registro UV: ptr en poly+0x10; 12 B: 3×{u8 u, u8 v, u16 textura};
             * byte[+0x0B] = capa [A] */
            uint32_t uv_off = 0;
            read_u32(image, image_size, poff + 0x10u, &uv_off);
            if (uv_off + 12u > image_size) return GEX_EMIT_ERR_RANGE;
            const uint8_t *uv = image + uv_off;
            for (int j = 0; j < 3; j++) {
                p->u[j] = uv[4u * j];
                p->v[j] = uv[4u * j + 1u];
            }
            uint16_t tw = 0;
            read_u16(image, image_size, uv_off + 2u, &tw);
            p->texture = (uint16_t)(tw & 0x3FFFu); /* índice 14 bits [A] */
            p->layer = uv[0x0Bu]; /* desplazamiento de bucket [A] */

            /* Colores del vértice: dword[vtx+8] [A] */
            for (int j = 0; j < 3; j++) {
                uint32_t voff = desc->vertex_table + (size_t)p->vtx[j] * 12u + 8u;
                read_u32(image, image_size, voff, &p->color[j]);
            }
        }
    }
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Fase 2: project                                                    */
/* ------------------------------------------------------------------ */

gex_emit_status gex_emit_project(const gex_emit_matrix *mat,
                                 const int16_t vtx[9],   /* x0,y0,z0,x1,y1,z1,x2,y2,z2 */
                                 const uint32_t color[3],
                                 const uint8_t u[3], const uint8_t v[3],
                                 uint16_t texture, uint8_t layer,
                                 uint32_t source_poly,
                                 gex_emit_triangle *tri) {
    if (mat == NULL || vtx == NULL || tri == NULL) return GEX_EMIT_ERR_ARG;
    memset(tri, 0, sizeof(*tri));

    int32_t xs[3], ys[3], zp[3];
    for (int j = 0; j < 3; j++) {
        if (project_vertex(mat, vtx[3 * j], vtx[3 * j + 1], vtx[3 * j + 2],
                           &xs[j], &ys[j], &zp[j]) != 0) {
            tri->flags |= GEX_EMIT_F_CLIPPED_XY; /* [A]: polígono clipeado */
        }
    }

    /* Copia valores crudos sin recortar a 10 bits; el renderizador decide el
     * recorte final [C]. Almacenados en int16 (clamp al rango de almacenaje). */
    for (int j = 0; j < 3; j++) {
        tri->x[j] = (int16_t)clamp_i32(xs[j], -32768, 32767);
        tri->y[j] = (int16_t)clamp_i32(ys[j], -32768, 32767);
        tri->z[j] = zp[j];
        if (xs[j] < GEX_EMIT_XY_MIN || xs[j] > GEX_EMIT_XY_MAX ||
            ys[j] < GEX_EMIT_XY_MIN || ys[j] > GEX_EMIT_XY_MAX) {
            tri->flags |= GEX_EMIT_F_CLIPPED_XY;
        }
    }
    if (color) memcpy(tri->color, color, sizeof(tri->color));
    if (u)     memcpy(tri->u, u, sizeof(tri->u));
    if (v)     memcpy(tri->v, v, sizeof(tri->v));
    tri->texture = texture;
    tri->layer = layer;
    tri->source_poly = source_poly;
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Fase 3: classify                                                   */
/* ------------------------------------------------------------------ */

/* Determinante 2D (0x46B6B5): det = (x1-x0)(y2-y0) - (x2-x0)(y1-y0),
 * fórmula convencional [A]. Descarta si det >= 0. */
static int32_t det2d(const int16_t x[3], const int16_t y[3]) {
    return (int32_t)(x[1] - x[0]) * (int32_t)(y[2] - y[0]) -
           (int32_t)(x[2] - x[0]) * (int32_t)(y[1] - y[0]);
}

gex_emit_status gex_emit_classify(const gex_emit_fog *fog,
                                  const gex_emit_triangle *p,
                                  uint16_t poly_flags,
                                  gex_emit_class *cls) {
    if (fog == NULL || p == NULL || cls == NULL) return GEX_EMIT_ERR_ARG;
    memset(cls, 0, sizeof(*cls));

    cls->maxz = p->z[0];
    if (p->z[1] > cls->maxz) cls->maxz = p->z[1];
    if (p->z[2] > cls->maxz) cls->maxz = p->z[2];

    /* Back-face culling 2D posterior a proyección [A]: */
    if (det2d(p->x, p->y) >= 0) {
        cls->maxz = -1; /* marca de descarte */
        return GEX_EMIT_OK;
    }

    cls->subdivide = (cls->maxz < GEX_EMIT_SUBDIV_Z) ? 1 : 0;
    cls->fog_applies = (cls->maxz >= fog->threshold) ? 1 : 0;

    int force_near = (poly_flags & 0x0004u) != 0; /* [A]: fuerza 0x426256 */
    cls->emitter = (cls->maxz >= fog->split_z && !force_near)
                       ? GEX_EMIT_FAR
                       : GEX_EMIT_NEAR;

    /* Colores: si niebla aplicada, mezclar cada vértice con su propio factor
     * z (el original aplica tabla[z>>2] por vértice) [A]. */
    for (int j = 0; j < 3; j++) {
        int f = gex_emit_fog_factor(fog, p->z[j]);
        cls->fog_color[j] = cls->fog_applies
                                ? apply_fog(p->color[j], fog->fog_rgb, f)
                                : p->color[j];
    }
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Fase 4: subdivide (solo emisor cercano)                            */
/* ------------------------------------------------------------------ */

/* Punto medio con truncamiento hacia cero (cdq; sar 1) [A]. */
static int16_t mid_i16(int16_t a, int16_t b) {
    int32_t s = (int32_t)a + (int32_t)b;
    if (s < 0) s = -((-s) >> 1);
    else s = s >> 1;
    return (int16_t)s;
}

/* Media por byte de dos colores dword (el original promedia cada byte) [A]. */
static uint32_t mid_color(uint32_t a, uint32_t b) {
    uint8_t pa[4], pb[4], out[4];
    memcpy(pa, &a, 4);
    memcpy(pb, &b, 4);
    for (int j = 0; j < 3; j++) {
        out[j] = (uint8_t)(((int32_t)pa[j] + (int32_t)pb[j]) >> 1);
    }
    out[3] = pa[3];
    uint32_t r;
    memcpy(&r, out, 4);
    return r;
}

/* Emite un triángulo ya proyectado, aplicando niebla si corresponde. */
/* Subdivide un polígono cercano en cuatro triángulos del punto medio y
 * los emite con el emisor cercano (nunca el lejano) [A]. El orden de
 * vértices de cada subtriángulo es el estándar del punto medio; el orden
 * exacto del original no se pudo verificar bit a bit [B]. */
gex_emit_status gex_emit_subdivide(const gex_emit_matrix *mat,
                                   const gex_emit_fog *fog,
                                   int fog_applies,
                                   const int16_t vtx[9],
                                   const uint32_t color[3],
                                   const uint8_t u[3], const uint8_t v[3],
                                   uint16_t texture, uint8_t layer,
                                   uint32_t source_poly,
                                   gex_emit_output *out) {
    if (mat == NULL || vtx == NULL || color == NULL || u == NULL || v == NULL ||
        out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }

    int16_t m[3][3]; /* m01, m02, m12 */
    for (int j = 0; j < 3; j++) {
        m[0][j] = mid_i16(vtx[3 * 0 + j], vtx[3 * 1 + j]);
        m[1][j] = mid_i16(vtx[3 * 0 + j], vtx[3 * 2 + j]);
        m[2][j] = mid_i16(vtx[3 * 1 + j], vtx[3 * 2 + j]);
    }

    uint32_t mc[3];
    mc[0] = mid_color(color[0], color[1]);
    mc[1] = mid_color(color[0], color[2]);
    mc[2] = mid_color(color[1], color[2]);

    uint8_t mu[3], mv[3];
    for (int j = 0; j < 3; j++) {
        mu[0] = (uint8_t)(((int32_t)u[0] + u[1]) >> 1);
        mu[1] = (uint8_t)(((int32_t)u[0] + u[2]) >> 1);
        mu[2] = (uint8_t)(((int32_t)u[1] + u[2]) >> 1);
        mv[0] = (uint8_t)(((int32_t)v[0] + v[1]) >> 1);
        mv[1] = (uint8_t)(((int32_t)v[0] + v[2]) >> 1);
        mv[2] = (uint8_t)(((int32_t)v[1] + v[2]) >> 1);
    }

    int16_t t0[9] = {vtx[0], vtx[1], vtx[2], m[0][0], m[0][1], m[0][2],
                     m[1][0], m[1][1], m[1][2]};
    int16_t t1[9] = {vtx[3], vtx[4], vtx[5], m[2][0], m[2][1], m[2][2],
                     m[0][0], m[0][1], m[0][2]};
    int16_t t2[9] = {vtx[6], vtx[7], vtx[8], m[1][0], m[1][1], m[1][2],
                     m[2][0], m[2][1], m[2][2]};
    int16_t t3[9] = {m[0][0], m[0][1], m[0][2], m[2][0], m[2][1], m[2][2],
                     m[1][0], m[1][1], m[1][2]};

    int16_t *ts[4] = {t0, t1, t2, t3};
    uint32_t cc[4][3] = {{color[0], mc[0], mc[1]},
                         {color[1], mc[2], mc[0]},
                         {color[2], mc[1], mc[2]},
                         {mc[0], mc[2], mc[1]}};
    uint8_t uu[4][3] = {{u[0], mu[0], mu[1]},
                        {u[1], mu[2], mu[0]},
                        {u[2], mu[1], mu[2]},
                        {mu[0], mu[2], mu[1]}};
    uint8_t vv[4][3] = {{v[0], mv[0], mv[1]},
                        {v[1], mv[2], mv[0]},
                        {v[2], mv[1], mv[2]},
                        {mv[0], mv[2], mv[1]}};

    gex_emit_status st = GEX_EMIT_OK;
    for (int t = 0; t < 4 && st == GEX_EMIT_OK; t++) {
        gex_emit_triangle tri;
        st = gex_emit_project(mat, ts[t], cc[t], uu[t], vv[t], texture, layer,
                              source_poly, &tri);
        if (st != GEX_EMIT_OK) break;
        tri.flags |= GEX_EMIT_F_SUBDIVIDED | GEX_EMIT_F_NEAR_EMITTER;
        for (int j = 0; j < 3 && st == GEX_EMIT_OK; j++) {
            if (fog_applies) {
                int f = gex_emit_fog_factor(fog, tri.z[j]);
                tri.color[j] = apply_fog(tri.color[j], fog->fog_rgb, f);
            }
        }
        if (st == GEX_EMIT_OK) {
            st = push_tri(out, &tri) == 0 ? GEX_EMIT_OK : GEX_EMIT_ERR_NOMEM;
        }
    }
    return st;
}

/* ------------------------------------------------------------------ */
/* Fase 5: sort                                                       */
/* ------------------------------------------------------------------ */

static int tri_compare(const void *pa, const void *pb) {
    const gex_emit_triangle *a = (const gex_emit_triangle *)pa;
    const gex_emit_triangle *b = (const gex_emit_triangle *)pb;
    int32_t ka = (int32_t)((a->z[0] + a->z[1] + a->z[2]) / 3) >> 2;
    int32_t kb = (int32_t)((b->z[0] + b->z[1] + b->z[2]) / 3) >> 2;
    ka += 4 * (int32_t)a->layer;
    kb += 4 * (int32_t)b->layer;
    if (ka != kb) return kb - ka;       /* descendente: lejano primero [A] */
    if (a->source_poly != b->source_poly)
        return (int)b->source_poly - (int)a->source_poly; /* LIFO [A] */
    return (int)b->seq - (int)a->seq;   /* LIFO: último emitido primero [A] */
}

gex_emit_status gex_emit_sort(gex_emit_triangle *tris, size_t count) {
    if (tris == NULL && count > 0) return GEX_EMIT_ERR_ARG;
    if (count > 1) qsort(tris, count, sizeof(*tris), tri_compare);
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Orquestador                                                        */
/* ------------------------------------------------------------------ */

gex_emit_status gex_level_emit(const uint8_t *image, size_t image_size,
                               const gex_dfx_descriptor *desc,
                               const gex_bsp_walk_result *leaves,
                               const gex_emit_matrix *mat,
                               const gex_emit_fog *fog,
                               gex_emit_output *out) {
    if (image == NULL || desc == NULL || leaves == NULL || mat == NULL ||
        fog == NULL || out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    init_output(out);

    gex_emit_poly_list polys;
    memset(&polys, 0, sizeof(polys));
    gex_emit_status st = gex_emit_collect(image, image_size, desc, leaves, &polys);
    if (st != GEX_EMIT_OK) {
        gex_emit_poly_list_free(&polys);
        return st;
    }

    for (size_t i = 0; i < polys.count && st == GEX_EMIT_OK; i++) {
        gex_emit_poly *p = &polys.polys[i];
            /* Vértices locales (int16 x/y/z, stride 12) [A] */
        int16_t vtx[9];
        int bad = 0;
        for (int j = 0; j < 3; j++) {
            uint32_t voff = desc->vertex_table + (size_t)p->vtx[j] * 12u;
            uint16_t t0 = 0, t1 = 0, t2 = 0;
            if (read_u16(image, image_size, voff, &t0) != 0 ||
                read_u16(image, image_size, voff + 2, &t1) != 0 ||
                read_u16(image, image_size, voff + 4, &t2) != 0) {
                bad = 1;
                break;
            }
            vtx[3 * j] = (int16_t)t0;
            vtx[3 * j + 1] = (int16_t)t1;
            vtx[3 * j + 2] = (int16_t)t2;
        }
        if (bad) { st = GEX_EMIT_ERR_RANGE; break; }

        /* Proyectar */
        gex_emit_triangle tri;
        st = gex_emit_project(mat, vtx, p->color, p->u, p->v, p->texture,
                              p->layer, p->source_poly, &tri);
        if (st != GEX_EMIT_OK) break;
        if (tri.flags & GEX_EMIT_F_CLIPPED_XY) continue; /* polígono clipeado [A] */

        /* Clasificar */
        gex_emit_class cls;
        st = gex_emit_classify(fog, &tri, p->poly_flags, &cls);
        if (st != GEX_EMIT_OK) break;
        if (cls.maxz < 0) continue; /* descartado por determinante 2D [A] */

        /* Colores de salida:
         * NEAR (0x426256): color con niebla si aplica (rama con niebla).
         * FAR (0x426588): registro principal SIN niebla + segundo registro de
         * pasada de niebla (base blanca). */
        if (cls.subdivide && cls.emitter == GEX_EMIT_NEAR) {
            /* Subdivisión en cuatro, solo emisor cercano [A]. */
            st = gex_emit_subdivide(mat, fog, cls.fog_applies, vtx, p->color,
                                    p->u, p->v, p->texture, p->layer,
                                    p->source_poly, out);
        } else if (cls.emitter == GEX_EMIT_FAR) {
            tri.flags |= GEX_EMIT_F_FAR_EMITTER;
            /* registro principal: colores del llamador intactos (ya en tri.color) */
            if (push_tri(out, &tri) != 0) { st = GEX_EMIT_ERR_NOMEM; break; }
            /* segundo registro: pasada de niebla (base blanca) */
            gex_emit_triangle fogtri = tri;
            fogtri.flags = GEX_EMIT_F_FOG_PASS | GEX_EMIT_F_FAR_EMITTER;
            fogtri.flags &= ~GEX_EMIT_F_NEAR_EMITTER;
            for (int j = 0; j < 3; j++) {
                int f = gex_emit_fog_factor(fog, tri.z[j]);
                fogtri.color[j] = fog_pass_color(fog->fog_rgb, f);
            }
            if (push_tri(out, &fogtri) != 0) { st = GEX_EMIT_ERR_NOMEM; break; }
        } else {
            tri.flags |= GEX_EMIT_F_NEAR_EMITTER;
            if (cls.fog_applies) {
                memcpy(tri.color, cls.fog_color, sizeof(tri.color));
            }
            if (push_tri(out, &tri) != 0) { st = GEX_EMIT_ERR_NOMEM; break; }
        }
    }

    if (st == GEX_EMIT_OK) {
        st = gex_emit_sort(out->tris, out->count);
    }

    gex_emit_poly_list_free(&polys);
    return st;
}

void gex_emit_output_free(gex_emit_output *out) {
    if (out == NULL) return;
    free(out->tris);
    memset(out, 0, sizeof(*out));
}

void gex_emit_poly_list_free(gex_emit_poly_list *list) {
    if (list == NULL) return;
    free(list->polys);
    memset(list, 0, sizeof(*list));
}

const char *gex_emit_status_name(gex_emit_status status) {
    switch (status) {
    case GEX_EMIT_OK: return "ok";
    case GEX_EMIT_ERR_ARG: return "invalid argument";
    case GEX_EMIT_ERR_RANGE: return "offset/index out of image";
    case GEX_EMIT_ERR_INCONSIST: return "inconsistent descriptor";
    case GEX_EMIT_ERR_NOMEM: return "memory allocation failed";
    case GEX_EMIT_ERR_OVERFLOW: return "capacity overflow";
    default: return "unknown error";
    }
}
