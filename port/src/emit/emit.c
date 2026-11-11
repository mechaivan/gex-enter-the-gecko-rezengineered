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

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Constantes del original                                            */
/* ------------------------------------------------------------------ */
#define GEX_EMIT_SUBDIV_Z   0x6A4   /* 1700: umbral de subdivisión (0x426D09) */
#define GEX_EMIT_LUT_SIZE   3464u   /* 0x3620 entradas de [0x64FEA8] */
#define GEX_EMIT_FACTOR_MAX 0x1000
#define GEX_EMIT_POLY_HIDDEN 0x8000u /* poly+0x06 & 0x8000: oculto */

/* ------------------------------------------------------------------ */
/* Aritmética de 32 bits con wrap x86                                 */
/* ------------------------------------------------------------------ */

/* El ejecutable es i386: toda la aritmética de 32 bits envuelve de forma
 * modular y los desplazamientos a la derecha son aritméticos. Estas rutinas
 * reproducen ese comportamiento sin desbordamiento con signo (UB en C) ni
 * desplazamientos de valor negativo (implementation-defined). */

static int32_t i32_from_bits(uint32_t bits) {
    int32_t v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

static uint32_t bits_from_i32(int32_t v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return bits;
}

static int32_t add_wrap(int32_t a, int32_t b) {
    return i32_from_bits(bits_from_i32(a) + bits_from_i32(b));
}

static int32_t sub_wrap(int32_t a, int32_t b) {
    return i32_from_bits(bits_from_i32(a) - bits_from_i32(b));
}

static int32_t mul_wrap(int32_t a, int32_t b) {
    return i32_from_bits(bits_from_i32(a) * bits_from_i32(b));
}

static int32_t neg_wrap(int32_t a) {
    return i32_from_bits(0u - bits_from_i32(a));
}

/* sar de 32 bits con relleno de signo, definido en C para cualquier n. */
static int32_t sar32(int32_t v, unsigned n) {
    if (n == 0) return v;
    if (n >= 32u) return (v < 0) ? -1 : 0;
    if (v >= 0) return (int32_t)(bits_from_i32(v) >> n);
    return i32_from_bits(~(bits_from_i32(~v) >> n));
}

/* idiv de 32 bits con truncamiento hacia cero (C99 y x86 idiv).
 * b == 0 devuelve 0 [C]: el original no divide si z' == 0.
 * INT32_MIN / -1 provocaría #DE en x86; el port devuelve el valor envuelto
 * (0x80000000) y lo documenta como decisión de seguridad [C]. */
static int32_t idiv32(int32_t a, int32_t b) {
    if (b == 0) return 0;
    if (b == -1) return neg_wrap(a);
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

/* Producto punto de una fila Q12 (tres imul + dos add de 32 bits) [A]. */
static int32_t dot_row(const int16_t m[3], const int16_t v[3]) {
    int32_t acc = 0;
    acc = add_wrap(acc, mul_wrap((int32_t)m[0], (int32_t)v[0]));
    acc = add_wrap(acc, mul_wrap((int32_t)m[1], (int32_t)v[1]));
    acc = add_wrap(acc, mul_wrap((int32_t)m[2], (int32_t)v[2]));
    return acc;
}

/* ------------------------------------------------------------------ */
/* Niebla                                                             */
/* ------------------------------------------------------------------ */

/*
 * Generador de la LUT de niebla (0x461895).
 *
 * Reproduce el relleno de los 3464 indices de [0x64FEA8]:
 *   B = (total_z << 12) idiv (total_z - none_z)
 *   A = -((total_z * none_z) idiv (total_z - none_z))
 *   lut[i >> 2] = clamp(B + ((A << 12) idiv i), 0, 0x1000)
 *
 * El bucle recorre i en pasos de 4 hasta 0x3620 (los 3464 indices) y el
 * desplazamiento de A usa el `shl` de 32 bits con signo de x86, con la
 * division truncada. Se publica para que la aplicacion y las pruebas usen la
 * misma niebla sin duplicar el generador.
 */
void gex_emit_fog_generate(gex_emit_fog *fog, int16_t total_z,
                           int16_t none_z, const uint8_t fog_rgb[3]) {
    int32_t x, y, den, B, A, lim;

    if (fog == NULL) return;
    memset(fog, 0, sizeof(*fog));

    x = total_z;
    y = none_z;
    den = x - y;
    if (fog_rgb != NULL) {
        fog->fog_rgb[0] = fog_rgb[0];
        fog->fog_rgb[1] = fog_rgb[1];
        fog->fog_rgb[2] = fog_rgb[2];
    }
    fog->fog_total_z = total_z;
    fog->fog_none_z = none_z;
    if (den == 0) {
        fog->have_lut = 0;
        return;
    }

    B = (x << 12) / den;
    A = -((x * y) / den);
    lim = 32767;
    if (A > lim) A = lim;
    if (A < -lim) A = -lim;

    fog->lut[0] = 0;
    for (uint32_t i = 1; i < 0x3620u; i += 4) {
        /* shl de x86 sobre A con signo, sin UB de C */
        int32_t ash = (int32_t)((uint32_t)A << 12);
        int32_t val = B + (ash / (int32_t)i);
        if (val > 0x1000) val = 0x1000;
        if (val < 0) val = 0;
        fog->lut[i >> 2] = (int16_t)val;
    }

    fog->have_lut = 1;
    {
        int32_t ash = (int32_t)((uint32_t)A << 12);
        fog->threshold = B != 0 ? (-ash) / B : 0;
        /* A y B en la forma que espera la ruta analítica (0x46A150):
         * b_q12 = B y a_q12 = A escalado por 4096, de modo que
         * clamp(b_q12 + a_q12 / z) reproduce el mismo factor que la LUT. */
        fog->b_q12 = B;
        fog->a_q12 = ash;
    }
    fog->split_z = fog->threshold;
}

int gex_emit_fog_factor(const gex_emit_fog *fog, int32_t z) {
    if (fog == NULL) return 0;

    if (fog->have_lut) {
        /* LUT [0x64FEA8], índice z >> 2. El puerto acota el índice a
         * [0, 3463]; el original no lo hace (lectura fuera de tabla para
         * z >= 0x3620) [C]. */
        int32_t idx = sar32(z, 2);
        if (idx < 0) idx = 0;
        if (idx > (int32_t)(GEX_EMIT_LUT_SIZE - 1)) {
            idx = (int32_t)(GEX_EMIT_LUT_SIZE - 1);
        }
        return (int)fog->lut[idx];
    }

    /* Fórmula analítica (0x46A150): clamp(B + (A<<12) idiv z, 0, 0x1000),
     * con A<<12 en a_q12 [A]. División truncada; z == 0 no divide [C]. */
    int32_t f = add_wrap(fog->b_q12, idiv32(fog->a_q12, z));
    return clamp_i32(f, 0, GEX_EMIT_FACTOR_MAX);
}

/* Mezcla un byte hacia el color de niebla (0x46B727):
 * out = desde + (((hacia - desde) * factor) sar 12), clamp 0..255 [A]. */
static uint8_t mix_byte(uint8_t desde, uint8_t hacia, int32_t factor) {
    int32_t d = sub_wrap((int32_t)hacia, (int32_t)desde);
    int32_t t = sar32(mul_wrap(d, factor), 12);
    return clamp_byte(add_wrap((int32_t)desde, t));
}

/* Aplica niebla a un color AARRGGBB: canales bajos 0..2 (B,G,R en memoria),
 * alfa intacto. El original mezcla byte a byte sobre [0x6474C4] [A]. */
static uint32_t apply_fog(uint32_t color, const uint8_t fog_rgb[3],
                          int32_t factor) {
    uint32_t b = mix_byte((uint8_t)(color & 0xFFu), fog_rgb[0], factor);
    uint32_t g = mix_byte((uint8_t)((color >> 8) & 0xFFu), fog_rgb[1], factor);
    uint32_t r = mix_byte((uint8_t)((color >> 16) & 0xFFu), fog_rgb[2], factor);
    return (color & 0xFF000000u) | (r << 16) | (g << 8) | b;
}

/* Pasada de niebla del emisor lejano (registro 0x24 de 0x426588): base
 * 0x00FFFFFF y alfa 0x00 [A]. */
static uint32_t fog_pass_color(const uint8_t fog_rgb[3], int32_t factor) {
    uint32_t b = mix_byte(255u, fog_rgb[0], factor);
    uint32_t g = mix_byte(255u, fog_rgb[1], factor);
    uint32_t r = mix_byte(255u, fog_rgb[2], factor);
    return (r << 16) | (g << 8) | b;
}

/* ------------------------------------------------------------------ */
/* Reservas seguras                                                   */
/* ------------------------------------------------------------------ */

/* Devuelve 0 si reserva, -1 sin memoria, -2 si la capacidad desborda
 * size_t. Nunca calcula bytes sin comprobar la multiplicación. */
static int grow_tris(gex_emit_output *out, size_t need) {
    if (out->capacity >= need) return 0;
    if (need > SIZE_MAX / sizeof(gex_emit_triangle)) return -2;

    size_t new_cap = out->capacity ? out->capacity : 256u;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2u) {
            new_cap = need;
            break;
        }
        new_cap *= 2u;
    }
    if (new_cap > SIZE_MAX / sizeof(gex_emit_triangle)) return -2;

    gex_emit_triangle *tmp =
        (gex_emit_triangle *)realloc(out->tris, new_cap * sizeof(*tmp));
    if (tmp == NULL) return -1;
    out->tris = tmp;
    out->capacity = new_cap;
    return 0;
}

static gex_emit_status push_tri(gex_emit_output *out,
                                const gex_emit_triangle *tri) {
    if (out->count == out->capacity) {
        int r = grow_tris(out, out->count + 1u);
        if (r != 0) {
            out->overflow = 1;
            return (r == -2) ? GEX_EMIT_ERR_OVERFLOW : GEX_EMIT_ERR_NOMEM;
        }
    }
    out->tris[out->count] = *tri;
    out->tris[out->count].seq = (uint32_t)out->count; /* inserción LIFO */
    out->count++;
    if (tri->flags & GEX_EMIT_F_FOG_PASS) out->fog_pass_count++;
    return GEX_EMIT_OK;
}

static int grow_polys(gex_emit_poly_list *list, size_t need) {
    if (list->capacity >= need) return 0;
    if (need > SIZE_MAX / sizeof(gex_emit_poly)) return -2;

    size_t new_cap = list->capacity ? list->capacity : 256u;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2u) {
            new_cap = need;
            break;
        }
        new_cap *= 2u;
    }
    if (new_cap > SIZE_MAX / sizeof(gex_emit_poly)) return -2;

    gex_emit_poly *tmp =
        (gex_emit_poly *)realloc(list->polys, new_cap * sizeof(*tmp));
    if (tmp == NULL) return -1;
    list->polys = tmp;
    list->capacity = new_cap;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Lecturas comprobadas dentro de la imagen                           */
/* ------------------------------------------------------------------ */

/* Todas las sumas se hacen en size_t y se comprueban antes de leer: un
 * offset 0xFFFFFFFF nunca envuelve a un valor pequeño [C]. */
static int range_ok(size_t off, size_t len, size_t size) {
    return off <= size && len <= size - off;
}

static int read_u16(const uint8_t *image, size_t image_size,
                    size_t off, uint16_t *out) {
    if (!range_ok(off, 2u, image_size)) return -1;
    *out = (uint16_t)((uint16_t)image[off] |
                      (uint16_t)((uint16_t)image[off + 1u] << 8));
    return 0;
}

static int read_u32(const uint8_t *image, size_t image_size,
                    size_t off, uint32_t *out) {
    if (!range_ok(off, 4u, image_size)) return -1;
    *out = (uint32_t)image[off] |
           ((uint32_t)image[off + 1u] << 8) |
           ((uint32_t)image[off + 2u] << 16) |
           ((uint32_t)image[off + 3u] << 24);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Fase 1: collect                                                    */
/* ------------------------------------------------------------------ */

/* Hoja -> polígonos, según 0x4269C2:
 *   word[leaf+0x12] & 2   -> prueba de hoja
 *   (int16)word[leaf+0x10] -> npolys (con signo; <= 0 no itera)
 *   dword[leaf+0x0C]      -> offset de la tabla de polígonos (0x14 B cada uno)
 * [A] para la estructura; todos los accesos son offsets comprobados [C]. */
gex_emit_status gex_emit_collect(const uint8_t *image, size_t image_size,
                                 const gex_dfx_descriptor *desc,
                                 const gex_bsp_walk_result *leaves,
                                 gex_emit_poly_list *out) {
    if (image == NULL || desc == NULL || leaves == NULL || out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    if (leaves->count > 0 && leaves->leaves == NULL) return GEX_EMIT_ERR_ARG;
    memset(out, 0, sizeof(*out));

    /* Tablas del descriptor: comprobadas una vez, con multiplicación segura. */
    if (desc->vertex_count > 0) {
        if ((size_t)desc->vertex_count > SIZE_MAX / GEX_VERTEX_SIZE) {
            return GEX_EMIT_ERR_OVERFLOW;
        }
        if (!range_ok((size_t)desc->vertex_table,
                      (size_t)desc->vertex_count * GEX_VERTEX_SIZE,
                      image_size)) {
            return GEX_EMIT_ERR_RANGE;
        }
    }
    if (desc->polygon_count > 0) {
        if ((size_t)desc->polygon_count > SIZE_MAX / GEX_POLYGON_SIZE) {
            return GEX_EMIT_ERR_OVERFLOW;
        }
        if (!range_ok((size_t)desc->polygon_table,
                      (size_t)desc->polygon_count * GEX_POLYGON_SIZE,
                      image_size)) {
            return GEX_EMIT_ERR_RANGE;
        }
    }

    /* Primera pasada: contar para reservar una sola vez. */
    size_t total = 0;
    for (uint32_t i = 0; i < leaves->count; i++) {
        size_t loff = (size_t)leaves->leaves[i];
        uint16_t leaf_type = 0;
        if (!range_ok(loff, 0x14u, image_size)) return GEX_EMIT_ERR_RANGE;
        if (read_u16(image, image_size, loff + 0x12u, &leaf_type) != 0) {
            return GEX_EMIT_ERR_RANGE;
        }
        if ((leaf_type & GEX_BSP_LEAF_FLAG) == 0) continue;

        uint16_t np_raw = 0;
        if (read_u16(image, image_size, loff + 0x10u, &np_raw) != 0) {
            return GEX_EMIT_ERR_RANGE;
        }
        int32_t np = (int16_t)np_raw; /* con signo [A] */
        if (np <= 0) continue;

        if ((size_t)np > SIZE_MAX - total) return GEX_EMIT_ERR_OVERFLOW;
        total += (size_t)np;
    }
    if (total > SIZE_MAX / sizeof(gex_emit_poly)) {
        return GEX_EMIT_ERR_OVERFLOW;
    }
    if (total > 0 && grow_polys(out, total) != 0) return GEX_EMIT_ERR_NOMEM;

    /* Segunda pasada: extraer. */
    uint32_t poly_index = 0;
    for (uint32_t i = 0; i < leaves->count; i++) {
        size_t loff = (size_t)leaves->leaves[i];
        uint16_t leaf_type = 0;
        read_u16(image, image_size, loff + 0x12u, &leaf_type);
        if ((leaf_type & GEX_BSP_LEAF_FLAG) == 0) continue;

        uint16_t np_raw = 0;
        read_u16(image, image_size, loff + 0x10u, &np_raw);
        int32_t np = (int16_t)np_raw;
        if (np <= 0) continue;

        uint32_t poly_off = 0;
        if (read_u32(image, image_size, loff + 0x0Cu, &poly_off) != 0) {
            return GEX_EMIT_ERR_RANGE;
        }
        size_t poly_bytes = (size_t)np * GEX_POLYGON_SIZE;
        if (!range_ok((size_t)poly_off, poly_bytes, image_size)) {
            return GEX_EMIT_ERR_RANGE;
        }

        for (int32_t k = 0; k < np; k++) {
            size_t poff = (size_t)poly_off + (size_t)k * GEX_POLYGON_SIZE;
            if (out->count == out->capacity &&
                grow_polys(out, out->count + 1u) != 0) {
                return GEX_EMIT_ERR_NOMEM;
            }
            gex_emit_poly *p = &out->polys[out->count++];
            memset(p, 0, sizeof(*p));
            p->source_poly = poly_index++;

            /* Índices de vértice (u16 en poly+0,+2,+4), stride 12 [A]. */
            for (int j = 0; j < 3; j++) {
                uint16_t idx = 0;
                if (read_u16(image, image_size, poff + 2u * (size_t)j,
                             &idx) != 0) {
                    return GEX_EMIT_ERR_RANGE;
                }
                p->vtx[j] = idx;
                if (idx >= desc->vertex_count) return GEX_EMIT_ERR_RANGE;
            }

            /* Flags del polígono (u16 en +0x06) [A]. */
            uint16_t pf = 0;
            if (read_u16(image, image_size, poff + 6u, &pf) != 0) {
                return GEX_EMIT_ERR_RANGE;
            }
            p->poly_flags = pf;

            /* Registro UV: dword en poly+0x10; 12 B: 3 x {u8 u, u8 v,
             * u16 textura}; byte[+0x0B] = capa (int8) [A]. */
            uint32_t uv_off = 0;
            if (read_u32(image, image_size, poff + 0x10u, &uv_off) != 0) {
                return GEX_EMIT_ERR_RANGE;
            }
            if (!range_ok((size_t)uv_off, 12u, image_size)) {
                return GEX_EMIT_ERR_RANGE;
            }
            const uint8_t *uv = image + (size_t)uv_off;
            for (int j = 0; j < 3; j++) {
                p->u[j] = uv[4u * (size_t)j];
                p->v[j] = uv[4u * (size_t)j + 1u];
            }
            uint16_t tw = 0;
            read_u16(image, image_size, (size_t)uv_off + 6u, &tw);
            /* El indice de textura sale de la mitad alta del SEGUNDO
             * dword del registro UV (+6): el comando escribe el dword
             * de UV en +0x1C y 0x46558A toma word[+0x1E] & 0x3FFF [A]. */
            p->texture = (uint16_t)(tw & 0x3FFFu); /* índice 14 bits [A] */
            p->layer = (int8_t)uv[0x0Bu];          /* movsx: con signo [A] */

            /* Colores del vértice: dword[vtx+8] [A]. */
            for (int j = 0; j < 3; j++) {
                size_t voff = (size_t)desc->vertex_table +
                              (size_t)p->vtx[j] * GEX_VERTEX_SIZE + 8u;
                if (read_u32(image, image_size, voff, &p->color[j]) != 0) {
                    return GEX_EMIT_ERR_RANGE;
                }
            }
        }
    }
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Fase 2: proyección de un vértice (0x46A1D0)                        */
/* ------------------------------------------------------------------ */

/* Transforma y proyecta UN vértice con la aritmética exacta de 0x46A1D0.
 * Devuelve 0 si pasa el descarte de profundidad; -1 si se clipea
 * (z < near_z || z > 0xFFFF -> [0x647578] |= 0x40000) [A]. */
static int project_vertex(const gex_emit_matrix *mat,
                          int16_t x, int16_t y, int16_t z,
                          int32_t *xs, int32_t *ys, int32_t *zp) {
    int16_t v[3] = {x, y, z};
    int32_t cam0 = add_wrap(sar32(dot_row(&mat->m[0], v), 12), mat->t[0]);
    int32_t cam1 = add_wrap(sar32(dot_row(&mat->m[3], v), 12), mat->t[1]);
    int32_t cam2 = add_wrap(sar32(dot_row(&mat->m[6], v), 12), mat->t[2]);

    *zp = cam2;
    if (cam2 < mat->near_z || cam2 > 0xFFFF) return -1;

    if (cam2 != 0) {
        *xs = add_wrap(idiv32(mul_wrap(cam0, mat->focal), cam2), mat->cx);
        *ys = add_wrap(idiv32(mul_wrap(cam1, mat->focal), cam2), mat->cy);
    } else {
        /* z' == 0: el original conserva los globales previos; el port fija
         * 0 porque con near_z > 0 este caso está clipeado [C]. */
        *xs = 0;
        *ys = 0;
    }
    return 0;
}

gex_emit_status gex_emit_project(const gex_emit_matrix *mat,
                                 const int16_t vtx[9],
                                 const uint32_t color[3],
                                 const uint8_t u[3], const uint8_t v[3],
                                 uint16_t texture, int8_t layer,
                                 uint32_t source_poly,
                                 gex_emit_triangle *tri) {
    if (mat == NULL || vtx == NULL || tri == NULL) return GEX_EMIT_ERR_ARG;
    memset(tri, 0, sizeof(*tri));

    int32_t xs[3] = {0, 0, 0}, ys[3] = {0, 0, 0}, zp[3] = {0, 0, 0};
    int clipped = 0;
    for (int j = 0; j < 3; j++) {
        if (project_vertex(mat, vtx[3 * j], vtx[3 * j + 1], vtx[3 * j + 2],
                           &xs[j], &ys[j], &zp[j]) != 0) {
            clipped = 1; /* 0x46A1D0 abandona el polígono entero [A] */
        }
    }

    for (int j = 0; j < 3; j++) {
        tri->z[j] = zp[j];
        if (clipped) {
            /* El original deja los globales del polígono anterior; el port
             * publica 0 y marca el flag para que el llamador lo descarte [C]. */
            tri->x[j] = 0;
            tri->y[j] = 0;
        } else {
            /* Clamp final de 0x46A1D0: x/y a [-0x400, 0x3FF] [A]. */
            tri->x[j] = (int16_t)clamp_i32(xs[j], GEX_EMIT_XY_MIN,
                                           GEX_EMIT_XY_MAX);
            tri->y[j] = (int16_t)clamp_i32(ys[j], GEX_EMIT_XY_MIN,
                                           GEX_EMIT_XY_MAX);
        }
    }
    if (clipped) tri->flags |= GEX_EMIT_F_DEPTH_CLIPPED;

    if (color != NULL) memcpy(tri->color, color, sizeof(tri->color));
    if (u != NULL) memcpy(tri->u, u, sizeof(tri->u));
    if (v != NULL) memcpy(tri->v, v, sizeof(tri->v));
    tri->texture = texture;
    tri->layer = layer;
    tri->source_poly = source_poly;
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Gate de los emisores (0x426256 / 0x426588)                         */
/* ------------------------------------------------------------------ */

/* Determinante 2D de 0x46B6B5 sobre las coordenadas empaquetadas
 * (y<<16)|x: (x1-x0)(y2-y0) - (x2-x0)(y1-y0), con imul/sub de 32 bits [A].
 * Se descarta si det >= 0. */
static int32_t det2d(const int16_t x[3], const int16_t y[3]) {
    int32_t a = sub_wrap((int32_t)x[1], (int32_t)x[0]);
    int32_t b = sub_wrap((int32_t)y[2], (int32_t)y[0]);
    int32_t c = sub_wrap((int32_t)x[2], (int32_t)x[0]);
    int32_t d = sub_wrap((int32_t)y[1], (int32_t)y[0]);
    return sub_wrap(mul_wrap(a, b), mul_wrap(c, d));
}

/* Profundidad de referencia del original: el MAXIMO de las tres
 * profundidades proyectadas.
 *
 * 0x426CC1..0x426D06 (y el mismo patron en 0x4262BE..0x426300 del emisor
 * cercano y en 0x4278A6..0x4278E5) calcula:
 *
 *     if (A <= B)  r = (B <= C) ? C : B;   -> max(B, C)
 *     else         r = (A <= C) ? C : A;   -> max(A, C)
 *
 * Como A > B en la segunda rama y B >= A en la primera, el resultado es
 * max(A, B, C) con A = [0x647430], B = [0x64742C] y C = [0x6474A0] (las tres
 * profundidades que escribe 0x46A1D0).
 *
 * Ese valor decide la subdivision (0x426D09, umbral 0x6A4), la niebla
 * (0x426D2F), la eleccion de emisor (0x426DE8) y la clave de bucket
 * (0x426303 en 0x426256).
 *
 * Evidencia dinamica (map7.dfx, 2026-11-11): de los 55 poligonos del frame
 * de referencia cuya profundidad MINIMA es < 0x6A4, el original subdivide
 * exactamente los 27 cuya profundidad MAXIMA es < 0x6A4 (55/55 aciertos con
 * el maximo, 27/55 con el minimo). Ademas, con el maximo la secuencia de
 * dibujo del port coincide triángulo a triángulo con la del original
 * (1123/1123); con el minimo solo 14/1123. */
static int32_t max_depth(const gex_emit_triangle *tri) {
    int32_t mz = tri->z[0];
    if (tri->z[1] > mz) mz = tri->z[1];
    if (tri->z[2] > mz) mz = tri->z[2];
    return mz;
}

/* Gate completo compartido por el emisor cercano y el lejano:
 *   1. [0x647578] & 0x60000 (recorte global) -> descarta
 *   2. vértice clipeado por profundidad (0x40000) -> descarta
 *   3. det2d >= 0 (back-face) -> descarta
 *   4. bucket (maxz>>2) + 4*layer fuera de [0, 0xC00) -> descarta
 *   5. poly+0x06 & 0x8000 (oculto) -> descarta
 *   6. todos x<0, x>0x200, y<0 o y>0x180 -> descarta
 * [A] reconstruido de 0x426256/0x426588 y 0x46B6B5. */
static int emit_gate_body(const gex_emit_triangle *tri, uint16_t poly_flags,
                           uint32_t state_flags, int8_t layer,
                           int32_t *maxz_out, int32_t *bucket_out) {
    if ((state_flags & GEX_EMIT_STATE_CLIP_MASK) != 0u) return 0;
    if (tri->flags & GEX_EMIT_F_DEPTH_CLIPPED) return 0;

    int32_t mz = max_depth(tri);
    int32_t bucket = add_wrap(sar32(mz, 2), (int32_t)layer * 4);
    if (bucket < 0 || bucket >= (int32_t)GEX_EMIT_BUCKET_COUNT) return 0;

    if ((poly_flags & GEX_EMIT_POLY_HIDDEN) != 0u) return 0;

    int all_left = tri->x[0] < 0 && tri->x[1] < 0 && tri->x[2] < 0;
    int all_right = tri->x[0] > GEX_EMIT_SCREEN_MAX_X &&
                    tri->x[1] > GEX_EMIT_SCREEN_MAX_X &&
                    tri->x[2] > GEX_EMIT_SCREEN_MAX_X;
    int all_up = tri->y[0] < 0 && tri->y[1] < 0 && tri->y[2] < 0;
    int all_down = tri->y[0] > GEX_EMIT_SCREEN_MAX_Y &&
                   tri->y[1] > GEX_EMIT_SCREEN_MAX_Y &&
                   tri->y[2] > GEX_EMIT_SCREEN_MAX_Y;
    if (all_left || all_right || all_up || all_down) return 0;

    if (maxz_out != NULL) *maxz_out = mz;
    if (bucket_out != NULL) *bucket_out = bucket;
    return 1;
}

/* det2d (0x46B6B5 -> [0x6468E0]): (X1-X0)(Y2-Y0) - (X2-X0)(Y1-Y0).
 *
 * El original solo ABORTA la subdivision cuando este valor es >= 0 (cara
 * trasera): es la unica situacion en la que el emisor cercano devuelve 0
 * (0x42629A). El resto de rechazos (recorte, bucket fuera de rango, oculto,
 * fuera de pantalla) devuelven 1 y la subdivision continua con el siguiente
 * triangulo [A]. */
static int emit_backfacing(const gex_emit_triangle *tri) {
    return det2d(tri->x, tri->y) >= 0;
}

static int emit_gate(const gex_emit_triangle *tri, uint16_t poly_flags,
                     uint32_t state_flags, int8_t layer,
                     int32_t *maxz_out, int32_t *bucket_out) {
    if (emit_backfacing(tri)) return 0;
    return emit_gate_body(tri, poly_flags, state_flags, layer,
                          maxz_out, bucket_out);
}

/* ------------------------------------------------------------------ */
/* Fase 3: classify                                                   */
/* ------------------------------------------------------------------ */

gex_emit_status gex_emit_classify(const gex_emit_fog *fog,
                                  const gex_emit_triangle *p,
                                  uint16_t poly_flags,
                                  uint32_t state_flags,
                                  gex_emit_class *cls) {
    if (fog == NULL || p == NULL || cls == NULL) return GEX_EMIT_ERR_ARG;
    memset(cls, 0, sizeof(*cls));

    int32_t maxz = max_depth(p);
    cls->maxz = maxz;

    /* 0x426D09: la subdivisión tiene prioridad absoluta sobre det2d, bucket,
     * niebla y selección de emisor; si maxz < 0x6A4 se salta a 0x426E71 [A]. */
    if (maxz < GEX_EMIT_SUBDIV_Z) {
        cls->subdivide = 1;
        cls->emitter = GEX_EMIT_NEAR;
        cls->fog_applies = 0;
        cls->bucket = -1;
        for (int j = 0; j < 3; j++) cls->fog_color[j] = p->color[j];
        return GEX_EMIT_OK;
    }

    int32_t gate_maxz = 0, bucket = -1;
    if (!emit_gate(p, poly_flags, state_flags, p->layer,
                   &gate_maxz, &bucket)) {
        cls->maxz = -1; /* marca de descarte */
        cls->bucket = -1;
        cls->emitter = GEX_EMIT_NEAR;
        for (int j = 0; j < 3; j++) cls->fog_color[j] = p->color[j];
        return GEX_EMIT_OK;
    }
    cls->maxz = gate_maxz;
    cls->bucket = bucket;

    /* Niebla por vértice si maxz >= [0x65BF80] [A]. */
    cls->fog_applies = (gate_maxz >= fog->threshold) ? 1 : 0;

    /* Emisor lejano si maxz >= [0x65BF04] y el polígono no fuerza cercano
     * (poly+0x06 & 4) [A]. */
    cls->emitter = (gate_maxz >= fog->split_z &&
                    (poly_flags & 0x0004u) == 0u)
                       ? GEX_EMIT_FAR
                       : GEX_EMIT_NEAR;

    for (int j = 0; j < 3; j++) {
        if (cls->fog_applies) {
            int32_t f = gex_emit_fog_factor(fog, p->z[j]);
            cls->fog_color[j] = apply_fog(p->color[j], fog->fog_rgb, f);
        } else {
            cls->fog_color[j] = p->color[j];
        }
    }
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Fase 4: subdivide (solo emisor cercano)                            */
/* ------------------------------------------------------------------ */

/* Punto medio con truncamiento hacia cero: (a+b) con cdq/sar 1 [A]. */
static int16_t mid_i16(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a + (int32_t)b) / 2);
}

static uint8_t mid_byte(uint8_t a, uint8_t b) {
    return (uint8_t)(((int32_t)a + (int32_t)b) / 2);
}

/* Media por byte de dos colores; el alfa es el del primer color [A]. */
static uint32_t mid_color(uint32_t a, uint32_t b) {
    uint32_t b0 = mid_byte((uint8_t)(a & 0xFFu), (uint8_t)(b & 0xFFu));
    uint32_t b1 = mid_byte((uint8_t)((a >> 8) & 0xFFu),
                           (uint8_t)((b >> 8) & 0xFFu));
    uint32_t b2 = mid_byte((uint8_t)((a >> 16) & 0xFFu),
                           (uint8_t)((b >> 16) & 0xFFu));
    return (a & 0xFF000000u) | (b2 << 16) | (b1 << 8) | b0;
}

/* Proyecta un subtriángulo, aplica el gate del emisor cercano y lo inserta.
 * Devuelve OK siempre que no haya error de memoria; `emitted` indica si pasó
 * el gate. */
static gex_emit_status sub_emit(const gex_emit_matrix *mat,
                                const int16_t vs[9],
                                const uint32_t cs[3],
                                const uint8_t us[3], const uint8_t vv[3],
                                uint16_t texture, int8_t layer,
                                uint16_t poly_flags, uint32_t state_flags,
                                uint32_t source_poly,
                                gex_emit_output *out, int *emitted,
                                int *fatal) {
    gex_emit_triangle tri;
    gex_emit_status st = gex_emit_project(mat, vs, cs, us, vv, texture, layer,
                                          source_poly, &tri);
    if (st != GEX_EMIT_OK) return st;

    *emitted = 0;
    if (emit_backfacing(&tri)) {
        *fatal = 1;
        return GEX_EMIT_OK;
    }
    *fatal = 0;
    if (!emit_gate_body(&tri, poly_flags, state_flags, layer, NULL, NULL)) {
        return GEX_EMIT_OK;
    }

    tri.flags |= GEX_EMIT_F_SUBDIVIDED | GEX_EMIT_F_NEAR_EMITTER;
    st = push_tri(out, &tri);
    if (st != GEX_EMIT_OK) return st;
    *emitted = 1;
    out->subtriangle_emitted++;
    return GEX_EMIT_OK;
}

/* 0x426E71-0x427816: cuatro triángulos principales en el orden exacto del
 * original, más hasta tres slivers degenerados si (color0 >> 24) & 2 == 0.
 * Solo emisor cercano (0x426256), sin niebla [A]. Si un principal no pasa el
 * gate se aborta conservando lo ya emitido; los slivers no abortan. */
gex_emit_status gex_emit_subdivide(const gex_emit_matrix *mat,
                                   const int16_t vtx[9],
                                   const uint32_t color[3],
                                   const uint8_t u[3], const uint8_t v[3],
                                   uint16_t texture, int8_t layer,
                                   uint16_t poly_flags,
                                   uint32_t state_flags,
                                   uint32_t source_poly,
                                   gex_emit_output *out) {
    if (mat == NULL || vtx == NULL || color == NULL || u == NULL ||
        v == NULL || out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }

    const int16_t *v0 = &vtx[0];
    const int16_t *v1 = &vtx[3];
    const int16_t *v2 = &vtx[6];

    out->subdivide_count++;

    int16_t m01[3], m20[3], m12[3];
    for (int j = 0; j < 3; j++) {
        m01[j] = mid_i16(v0[j], v1[j]);
        m20[j] = mid_i16(v2[j], v0[j]);
        m12[j] = mid_i16(v1[j], v2[j]);
    }

    uint32_t c01 = mid_color(color[0], color[1]);
    uint32_t c20 = mid_color(color[2], color[0]);
    uint32_t c12 = mid_color(color[1], color[2]);

    uint8_t u01 = mid_byte(u[0], u[1]), v01 = mid_byte(v[0], v[1]);
    uint8_t u20 = mid_byte(u[2], u[0]), v20 = mid_byte(v[2], v[0]);
    uint8_t u12 = mid_byte(u[1], u[2]), v12 = mid_byte(v[1], v[2]);

    /* Orden exacto de los cuatro principales (0x427294..0x427580) [A]:
     *   (v0,m01,m20), (m20,m12,v2), (m01,m12,m20), (m01,v1,m12) */
    struct {
        int16_t vs[9];
        uint32_t cs[3];
        uint8_t us[3];
        uint8_t vv[3];
    } main4[4] = {
        {{v0[0], v0[1], v0[2], m01[0], m01[1], m01[2],
          m20[0], m20[1], m20[2]},
         {color[0], c01, c20}, {u[0], u01, u20}, {v[0], v01, v20}},
        {{m20[0], m20[1], m20[2], m12[0], m12[1], m12[2],
          v2[0], v2[1], v2[2]},
         {c20, c12, color[2]}, {u20, u12, u[2]}, {v20, v12, v[2]}},
        {{m01[0], m01[1], m01[2], m12[0], m12[1], m12[2],
          m20[0], m20[1], m20[2]},
         {c01, c12, c20}, {u01, u12, u20}, {v01, v12, v20}},
        {{m01[0], m01[1], m01[2], v1[0], v1[1], v1[2],
          m12[0], m12[1], m12[2]},
         {c01, color[1], c12}, {u01, u[1], u12}, {v01, v[1], v12}},
    };

    for (int t = 0; t < 4; t++) {
        int emitted = 0, fatal = 0;
        out->subtriangle_count++;
        gex_emit_status st = sub_emit(mat, main4[t].vs, main4[t].cs,
                                      main4[t].us, main4[t].vv, texture,
                                      layer, poly_flags, state_flags,
                                      source_poly, out, &emitted, &fatal);
        if (st != GEX_EMIT_OK) return st;
        /* 0x4272D4 / 0x4273AC / 0x4274A1: solo det2d >= 0 aborta la
         * subdivision, y solo en los tres primeros principales. El cuarto
         * (0x427571) y los slivers no abortan [A]. */
        if (fatal && t < 3) {
            out->subdivide_aborted++;
            return GEX_EMIT_OK;
        }
    }

    /* Slivers: solo si (color0 >> 24) & 2 == 0 [A]. Orden exacto:
     * (v0,v1,m01), (m12,v1,v2), (v0,m20,v2). Sus fallos se ignoran. */
    if (((color[0] >> 24) & 0x2u) != 0u) return GEX_EMIT_OK;

    struct {
        int16_t vs[9];
        uint32_t cs[3];
        uint8_t us[3];
        uint8_t vv[3];
    } slivers[3] = {
        {{v0[0], v0[1], v0[2], v1[0], v1[1], v1[2],
          m01[0], m01[1], m01[2]},
         {color[0], color[1], c01}, {u[0], u[1], u01}, {v[0], v[1], v01}},
        {{m12[0], m12[1], m12[2], v1[0], v1[1], v1[2],
          v2[0], v2[1], v2[2]},
         {c12, color[1], color[2]}, {u12, u[1], u[2]}, {v12, v[1], v[2]}},
        {{v0[0], v0[1], v0[2], m20[0], m20[1], m20[2],
          v2[0], v2[1], v2[2]},
         {color[0], c20, color[2]}, {u[0], u20, u[2]}, {v[0], v20, v[2]}},
    };

    for (int t = 0; t < 3; t++) {
        int emitted = 0, fatal = 0;
        out->subtriangle_count++;
        gex_emit_status st = sub_emit(mat, slivers[t].vs, slivers[t].cs,
                                      slivers[t].us, slivers[t].vv, texture,
                                      layer, poly_flags, state_flags,
                                      source_poly, out, &emitted, &fatal);
        if (st != GEX_EMIT_OK) return st;
        (void)emitted;
        (void)fatal; /* los slivers nunca abortan la subdivision */
    }
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Fase 5: sort                                                       */
/* ------------------------------------------------------------------ */

/* Clave de bucket = (maxz sar 2) + 4*layer; descendente. Dentro de la misma
 * clave, LIFO: el último insertado se dibuja primero (seq descendente). */
static int tri_compare(const void *pa, const void *pb) {
    const gex_emit_triangle *a = (const gex_emit_triangle *)pa;
    const gex_emit_triangle *b = (const gex_emit_triangle *)pb;

    int32_t ka = add_wrap(sar32(max_depth(a), 2), (int32_t)a->layer * 4);
    int32_t kb = add_wrap(sar32(max_depth(b), 2), (int32_t)b->layer * 4);
    if (ka != kb) return (ka < kb) ? 1 : -1; /* descendente */
    if (a->seq != b->seq) return (a->seq < b->seq) ? 1 : -1; /* LIFO */
    return 0;
}

gex_emit_status gex_emit_sort(gex_emit_triangle *tris, size_t count) {
    if (tris == NULL && count > 0) return GEX_EMIT_ERR_ARG;
    if (count > 1) qsort(tris, count, sizeof(*tris), tri_compare);
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Orquestador                                                        */
/* ------------------------------------------------------------------ */

/* Lee los tres vértices de un polígono ya extraído (stride 12) [A]. */
static gex_emit_status load_vertices(const uint8_t *image, size_t image_size,
                                     const gex_dfx_descriptor *desc,
                                     const gex_emit_poly *p,
                                     int16_t vtx[9]) {
    for (int j = 0; j < 3; j++) {
        size_t voff = (size_t)desc->vertex_table +
                      (size_t)p->vtx[j] * GEX_VERTEX_SIZE;
        uint16_t t0 = 0, t1 = 0, t2 = 0;
        if (read_u16(image, image_size, voff, &t0) != 0 ||
            read_u16(image, image_size, voff + 2u, &t1) != 0 ||
            read_u16(image, image_size, voff + 4u, &t2) != 0) {
            return GEX_EMIT_ERR_RANGE;
        }
        vtx[3 * j] = (int16_t)t0;
        vtx[3 * j + 1] = (int16_t)t1;
        vtx[3 * j + 2] = (int16_t)t2;
    }
    return GEX_EMIT_OK;
}

gex_emit_status gex_level_emit(const uint8_t *image, size_t image_size,
                               const gex_dfx_descriptor *desc,
                               const gex_bsp_walk_result *leaves,
                               const gex_emit_matrix *mat,
                               const gex_emit_fog *fog,
                               uint32_t state_flags,
                               gex_emit_output *out) {
    if (image == NULL || desc == NULL || leaves == NULL || mat == NULL ||
        fog == NULL || out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    memset(out, 0, sizeof(*out));

    gex_emit_poly_list polys;
    memset(&polys, 0, sizeof(polys));
    gex_emit_status st =
        gex_emit_collect(image, image_size, desc, leaves, &polys);
    if (st != GEX_EMIT_OK) {
        gex_emit_poly_list_free(&polys);
        return st;
    }

    for (size_t i = 0; i < polys.count && st == GEX_EMIT_OK; i++) {
        const gex_emit_poly *p = &polys.polys[i];
        int16_t vtx[9];
        st = load_vertices(image, image_size, desc, p, vtx);
        if (st != GEX_EMIT_OK) break;

        gex_emit_triangle tri;
        st = gex_emit_project(mat, vtx, p->color, p->u, p->v, p->texture,
                              p->layer, p->source_poly, &tri);
        if (st != GEX_EMIT_OK) break;
        if (tri.flags & GEX_EMIT_F_DEPTH_CLIPPED) continue; /* 0x40000 [A] */

        gex_emit_class cls;
        st = gex_emit_classify(fog, &tri, p->poly_flags, state_flags, &cls);
        if (st != GEX_EMIT_OK) break;
        if (cls.maxz < 0) continue; /* gate [A] */

        if (cls.subdivide) {
            /* La subdivisión ya proyecta y aplica el gate por subtriángulo. */
            st = gex_emit_subdivide(mat, vtx, p->color, p->u, p->v,
                                    p->texture, p->layer, p->poly_flags,
                                    state_flags, p->source_poly, out);
        } else if (cls.emitter == GEX_EMIT_FAR) {
            /* Registro principal 0x34 con los colores del llamador (ya
             * mezclados si maxz >= umbral) [A]. */
            tri.flags |= GEX_EMIT_F_FAR_EMITTER;
            memcpy(tri.color, cls.fog_color, sizeof(tri.color));
            st = push_tri(out, &tri);
            if (st != GEX_EMIT_OK) break;

            /* Registro de niebla 0x24: base 0x00FFFFFF, factor por vértice,
             * sin UV [A]. */
            gex_emit_triangle fogtri = tri;
            fogtri.flags = GEX_EMIT_F_FOG_PASS | GEX_EMIT_F_FAR_EMITTER;
            for (int j = 0; j < 3; j++) {
                int32_t f = gex_emit_fog_factor(fog, tri.z[j]);
                fogtri.color[j] = fog_pass_color(fog->fog_rgb, f);
            }
            st = push_tri(out, &fogtri);
        } else {
            /* Emisor cercano 0x426256: un único registro con los colores. */
            tri.flags |= GEX_EMIT_F_NEAR_EMITTER;
            memcpy(tri.color, cls.fog_color, sizeof(tri.color));
            st = push_tri(out, &tri);
        }
    }

    if (st == GEX_EMIT_OK) st = gex_emit_sort(out->tris, out->count);

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
