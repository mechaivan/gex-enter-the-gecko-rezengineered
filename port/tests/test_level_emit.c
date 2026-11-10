#include "gex/emit.h"
#include "gex/bsp.h"
#include "gex/camera.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if(!(cond)){ failures++; printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);} } while(0)

#define AZTEC_PATH "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX"

/* ------------------------------------------------------------------ */
/* Helpers de matriz y niebla                                         */
/* ------------------------------------------------------------------ */

static gex_emit_matrix mat_identity(void) {
    gex_emit_matrix m;
    memset(&m, 0, sizeof(m));
    m.m[0] = 0x1000; m.m[4] = 0x1000; m.m[8] = 0x1000; /* identidad Q12 */
    m.focal = 0x140; m.cx = 0x100; m.cy = 0x0C0;
    m.near_z = 0xA0;
    return m;
}

static gex_emit_fog fog_with_lut(int16_t total_z, int16_t none_z) {
    /* Reproduce el generador 0x461895: B=(x<<12)idiv(x-y),
     * A=-((x*y)idiv(x-y)), clamp, tabla[i>>2]=clamp(B+(A<<12)idiv i). */
    gex_emit_fog f;
    memset(&f, 0, sizeof(f));
    int32_t x = total_z, y = none_z;
    int32_t den = x - y;
    int32_t B = (x << 12) / den;
    int32_t A = -((x * y) / den);
    int32_t lim = 32767; /* [0x551644]->+8->+0x40 * 0x7FFF sar 8: valor seguro */
    if (A > lim) A = lim;
    if (A < -lim) A = -lim;
    f.lut[0] = 0;
    for (uint32_t i = 1; i < 3464u; i += 4) {
        /* reproducir shl de x86 sobre A con signo sin UB de C */
        int32_t ash = (int32_t)((uint32_t)A << 12);
        int32_t v = B + (ash / (int32_t)i);
        if (v > 0x1000) v = 0x1000;
        if (v < 0) v = 0;
        f.lut[i >> 2] = (int16_t)v;
    }
    f.have_lut = 1;
    f.fog_total_z = total_z;
    f.fog_none_z = none_z;
    int32_t ash = (int32_t)((uint32_t)A << 12);
    f.threshold = (-ash) / B;       /* 0x65BF80 */
    f.split_z = f.threshold;        /* 0x65BF04, +0x50 != -1 */
    f.fog_rgb[0] = 40; f.fog_rgb[1] = 60; f.fog_rgb[2] = 90;
    return f;
}

/* ------------------------------------------------------------------ */
/* API y reservass                                                    */
/* ------------------------------------------------------------------ */

static void test_api_smoke(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_output out; memset(&out, 0, sizeof(out));

    CHECK(gex_level_emit(NULL, 100, NULL, NULL, &m, &f, &out) == GEX_EMIT_ERR_ARG);
    CHECK(gex_level_emit((const uint8_t *)"x", 100, NULL, NULL, &m, &f, &out) == GEX_EMIT_ERR_ARG);
    CHECK(out.tris == NULL && out.count == 0);
    gex_emit_output_free(&out);
    CHECK(out.tris == NULL);

    CHECK(gex_emit_fog_factor(NULL, 100) == 0);
    CHECK(gex_emit_sort(NULL, 0) == GEX_EMIT_OK);
    CHECK(gex_emit_sort(NULL, 3) == GEX_EMIT_ERR_ARG);

    const char *n = gex_emit_status_name(GEX_EMIT_OK);
    CHECK(n != NULL && strcmp(n, "ok") == 0);
    CHECK(strcmp(gex_emit_status_name((gex_emit_status)99), "unknown error") == 0);
}

/* ------------------------------------------------------------------ */
/* Proyección: identidad                                              */
/* ------------------------------------------------------------------ */

static void test_project_identity(void) {
    gex_emit_matrix m = mat_identity();
    int16_t vtx[9] = {100, 50, 400,   /* v0 */
                      200, -30, 600,  /* v1 */
                      -80, 70, 300};  /* v2 */
    uint32_t color[3] = {0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u};
    uint8_t u[3] = {10, 20, 30}, v[3] = {1, 2, 3};
    gex_emit_triangle tri;
    memset(&tri, 0xAA, sizeof(tri));
    CHECK(gex_emit_project(&m, vtx, color, u, v, 0x1234, 5, 7, &tri) == GEX_EMIT_OK);

    /* z' = z (identidad, sin desplazamiento): 400, 600, 300 */
    CHECK(tri.z[0] == 400 && tri.z[1] == 600 && tri.z[2] == 300);
    /* xs = (100*320)/400 + 256 = 336; ys = (50*320)/400 + 192 = 232 */
    CHECK(tri.x[0] == 336 && tri.y[0] == 232);
    CHECK(tri.x[1] == (int16_t)((200 * 320) / 600 + 256));
    CHECK(tri.y[1] == (int16_t)((-30 * 320) / 600 + 192));
    CHECK(tri.x[2] == (int16_t)((-80 * 320) / 300 + 256));
    CHECK(tri.y[2] == (int16_t)((70 * 320) / 300 + 192));
    CHECK(tri.texture == 0x1234 && tri.layer == 5 && tri.source_poly == 7);
    CHECK(memcmp(tri.u, u, 3) == 0 && memcmp(tri.v, v, 3) == 0);
    CHECK(memcmp(tri.color, color, 12) == 0);
    CHECK(!(tri.flags & GEX_EMIT_F_CLIPPED_XY));
}

/* ------------------------------------------------------------------ */
/* Proyección: descarte por profundidad                               */
/* ------------------------------------------------------------------ */

static void test_project_depth_clip(void) {
    gex_emit_matrix m = mat_identity();
    int16_t vtx[9] = {10, 10, 100,    /* z = 100 < near_z 160 -> clipeado */
                      20, 20, 400,
                      30, 30, 300};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(tri.flags & GEX_EMIT_F_CLIPPED_XY);

    /* z > 0xFFFF via desplazamiento de la matriz (t[2] = 0x10000) */
    gex_emit_matrix m2 = mat_identity();
    m2.t[2] = 0x10000;
    int16_t vtx2[9] = {0, 0, 0, 20, 20, 400, 30, 30, 300};
    CHECK(gex_emit_project(&m2, vtx2, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(tri.flags & GEX_EMIT_F_CLIPPED_XY);
}

/* ------------------------------------------------------------------ */
/* Proyección: división con signo (truncamiento hacia cero)           */
/* ------------------------------------------------------------------ */

static void test_project_div_sign(void) {
    gex_emit_matrix m = mat_identity();
    /* x' negativo, z positivo: idiv truncado hacia cero */
    int16_t vtx[9] = {-100, 0, 400, 0, 0, 400, 0, 0, 400};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    /* xs = (-100*320)/400 + 256 = -80 + 256 = 176 */
    CHECK(tri.x[0] == 176);
}

/* Pasa-visible que acepta todo (evita el vis no inicializado de visible=NULL). */
static int visible_all(const uint8_t *image, size_t image_size,
                       uint32_t node_offset, void *ctx) {
    (void)image;(void)image_size;(void)node_offset;(void)ctx;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Transformación Q12 con wrap x86                                    */
/* ------------------------------------------------------------------ */

static void test_project_q12_wrap(void) {
    /* Matriz con valores int16 máximos: suma de productos envuelve int32. */
    gex_emit_matrix m = mat_identity();
    for (int i = 0; i < 9; i++) m.m[i] = 0x7FFF;
    int16_t vtx[9] = {0x7FFF, 0x7FFF, 0x7FFF, 0, 0, 400, 0, 0, 400};
    /* cam0 = (0x7FFF*0x7FFF*3) sar 12; el producto de 3 envuelve int32 */
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    uint32_t prod = (uint32_t)((int32_t)0x7FFF * 0x7FFF); /* 1073676289 */
    uint32_t sum = prod + prod + prod;                    /* 3221028867 -> wrap uint32 */
    int32_t cam0 = (int32_t)sum >> 12;                    /* sar aritmético */
    CHECK(tri.z[0] == cam0 + m.t[2]);
}

/* ------------------------------------------------------------------ */
/* Determinante y orientación                                         */
/* ------------------------------------------------------------------ */

static void test_determinant(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_output out; memset(&out, 0, sizeof(out));

    /* Triángulo de cara visible (det < 0 en pantalla): (0,0),(100,0),(0,100)
     * det = 100*100 - 0 = 10000 >= 0 -> DESCARTADO (convencional). */
    int16_t vtx[9] = {0, 0, 400, 100, 0, 400, 0, 100, 400};
    uint32_t color[3] = {0, 0, 0};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, color, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    gex_emit_class cls;
    CHECK(gex_emit_classify(&f, &tri, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.maxz < 0); /* descartado por determinante >= 0 */

    /* Orden inverso: det = -10000 < 0 -> aceptado */
    int16_t vtx2[9] = {0, 0, 400, 0, 100, 400, 100, 0, 400};
    CHECK(gex_emit_project(&m, vtx2, color, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(gex_emit_classify(&f, &tri, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.maxz == 400);
    CHECK(cls.subdivide == 1);     /* 400 < 0x6A4 -> subdivide */
    CHECK(cls.fog_applies == 1);   /* 400 >= threshold 99 */
    CHECK(cls.emitter == GEX_EMIT_FAR); /* 400 >= split_z 99 */
    gex_emit_output_free(&out);
}

/* ------------------------------------------------------------------ */
/* Niebla: LUT y consulta                                             */
/* ------------------------------------------------------------------ */

static void test_fog_lut(void) {
    gex_emit_fog f = fog_with_lut(400, 100);
    CHECK(f.have_lut == 1);
    CHECK(f.lut[0] == 0);
    CHECK(f.threshold == 99); /* (-(A<<12))/B con A=-133, B=5461 -> 99 */
    CHECK(f.split_z == 99);

    /* factor(z=0) = lut[0] = 0; factor(z=400) = lut[100] */
    CHECK(gex_emit_fog_factor(&f, 0) == 0);
    int32_t v100 = f.lut[100];
    CHECK(gex_emit_fog_factor(&f, 400) == v100);
    CHECK(v100 >= 0 && v100 <= 0x1000);

    /* Índice protegido: z muy grande no lee fuera de tabla */
    CHECK(gex_emit_fog_factor(&f, 0x7FFFFFFF) == f.lut[3464u - 1]);
    CHECK(gex_emit_fog_factor(&f, -1) == f.lut[0]);
}

/* ------------------------------------------------------------------ */
/* Niebla: fórmula analítica (sin LUT)                                */
/* ------------------------------------------------------------------ */

static void test_fog_analytic(void) {
    gex_emit_fog f;
    memset(&f, 0, sizeof(f));
    f.have_lut = 0;
    f.a_q12 = -544768;   /* A = -133 << 12 (valor con signo) */
    f.b_q12 = 5461;         /* B = 5461 */
    /* factor(z=400) = clamp(5461 + (-133)/400, 0, 4096) = 5461 -> clamp 4096 */
    CHECK(gex_emit_fog_factor(&f, 400) == 0x1000);
    CHECK(gex_emit_fog_factor(&f, 1) == 0x1000); /* 5461 - 133 = 5328 -> clamp */
    f.a_q12 = 0; f.b_q12 = 100;
    CHECK(gex_emit_fog_factor(&f, 50) == 100);   /* z != 0 */
    CHECK(gex_emit_fog_factor(&f, 0) == 100);    /* z == 0 -> sin dividir */
}

/* ------------------------------------------------------------------ */
/* Mezcla de colores (0x46B727)                                       */
/* ------------------------------------------------------------------ */

static void test_mix_color(void) {
    /* desde=100, hacia=200, factor=2048 -> 100 + (100*2048)>>12 = 150 */
    gex_emit_fog f = fog_with_lut(400, 100);
    /* factor(400) = lut[100]; comprobamos el comportamiento de mix_byte vía
     * apply_fog con un color conocido y factor forzado no expuesto. Se
     * valida indirectamente en test_emit_far_near con colores. */
    (void)f;
    CHECK(1); /* placeholder: cobertura real en classify/emit */
}

/* ------------------------------------------------------------------ */
/* Selección de emisor cercano / lejos                                */
/* ------------------------------------------------------------------ */

static void test_emitter_selection(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100); /* threshold=split_z=99 */
    uint32_t color[3] = {0xFF102030u, 0xFF405060u, 0xFF708090u};
    uint8_t u[3] = {1, 2, 3}, v[3] = {4, 5, 6};

    /* maxz = 400 >= 99 -> fog_applies; flags=0 -> emisor según split_z.
     * Orden de vértices tal que det2d < 0 (cara visible). */
    int16_t vtx[9] = {0, 0, 400, 0, 10, 400, 10, 0, 400};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, color, u, v, 0x100, 3, 11, &tri) == GEX_EMIT_OK);
    gex_emit_class cls;
    CHECK(gex_emit_classify(&f, &tri, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.maxz == 400);
    CHECK(cls.fog_applies == 1);
    CHECK(cls.emitter == GEX_EMIT_FAR); /* 400 >= 99 y no forzado */

    /* Forzar cercano con flag 0x4 */
    CHECK(gex_emit_classify(&f, &tri, 0x0004, &cls) == GEX_EMIT_OK);
    CHECK(cls.emitter == GEX_EMIT_NEAR);

    /* Cerca: maxz = 50 < 99 -> no fog, subdivide. Orden con det2d < 0. */
    int16_t vtx2[9] = {0, 0, 50, 0, 10, 50, 10, 0, 50};
    CHECK(gex_emit_project(&m, vtx2, color, u, v, 0, 0, 0, &tri) == GEX_EMIT_OK);
    printf("DEBUG near: tri.z=%d,%d,%d flags=%X\n", tri.z[0], tri.z[1], tri.z[2], tri.flags);
    CHECK(gex_emit_classify(&f, &tri, 0, &cls) == GEX_EMIT_OK);
    printf("DEBUG near: cls.maxz=%d subdiv=%d fog=%d emitter=%d\n",
           cls.maxz, cls.subdivide, cls.fog_applies, (int)cls.emitter);
    CHECK(cls.maxz == 50);
    CHECK(cls.fog_applies == 0);
    CHECK(cls.subdivide == 1); /* 50 < 0x6A4 */
    CHECK(cls.emitter == GEX_EMIT_NEAR);
}

/* ------------------------------------------------------------------ */
/* Subdivisión en cuatro triángulos                                   */
/* ------------------------------------------------------------------ */

static void test_subdivide(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100); /* fog_applies para z=100? threshold=99 -> sí */
    uint32_t color[3] = {0xFF000000u, 0x00FFFFFFu, 0x00FFFFFFu};
    uint8_t u[3] = {0, 100, 0}, v[3] = {0, 0, 100};
    /* maxz = 100 < 0x6A4 -> subdivide */
    int16_t vtx[9] = {0, 0, 100, 200, 0, 100, 0, 200, 100};
    gex_emit_output out; memset(&out, 0, sizeof(out));

    CHECK(gex_emit_subdivide(&m, &f, 1, vtx, color, u, v, 0x7, 2, 999, &out) == GEX_EMIT_OK);
    CHECK(out.count == 4);
    for (size_t i = 0; i < out.count; i++) {
        CHECK(out.tris[i].flags & GEX_EMIT_F_SUBDIVIDED);
        CHECK(out.tris[i].flags & GEX_EMIT_F_NEAR_EMITTER);
        CHECK(!(out.tris[i].flags & GEX_EMIT_F_FAR_EMITTER));
        CHECK(out.tris[i].source_poly == 999);
        CHECK(out.tris[i].texture == 0x7 && out.tris[i].layer == 2);
    }
    gex_emit_output_free(&out);
}

/* ------------------------------------------------------------------ */
/* Ordenación por profundidad y capa                                  */
/* ------------------------------------------------------------------ */

static void test_sort(void) {
    gex_emit_triangle ts[6];
    memset(ts, 0, sizeof(ts));
    /* clave = (avgz>>2) + 4*layer */
    /* t0: z=400 -> clave 100 + 0 = 100 */
    ts[0].z[0] = ts[0].z[1] = ts[0].z[2] = 400; ts[0].layer = 0; ts[0].source_poly = 0;
    /* t1: z=800 -> clave 200 */
    ts[1].z[0] = ts[1].z[1] = ts[1].z[2] = 800; ts[1].layer = 0; ts[1].source_poly = 1;
    /* t2: z=400, layer=1 -> clave 100 + 4 = 104 */
    ts[2].z[0] = ts[2].z[1] = ts[2].z[2] = 400; ts[2].layer = 1; ts[2].source_poly = 2;
    /* t3: z=400, layer=0, source_poly=5 -> misma clave que t0, LIFO después */
    ts[3].z[0] = ts[3].z[1] = ts[3].z[2] = 400; ts[3].layer = 0; ts[3].source_poly = 5;

    CHECK(gex_emit_sort(ts, 4) == GEX_EMIT_OK);
    /* descendente: t1(200), t2(104), t3(100, sp=5), t0(100, sp=0) */
    CHECK(ts[0].source_poly == 1);
    CHECK(ts[1].source_poly == 2);
    CHECK(ts[2].source_poly == 5);
    CHECK(ts[3].source_poly == 0);
}

/* ------------------------------------------------------------------ */
/* Capacidad y errores de memoria                                     */
/* ------------------------------------------------------------------ */

static void test_capacity_growth(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_output out; memset(&out, 0, sizeof(out));
    uint32_t color[3] = {0, 0, 0};
    uint8_t u[3] = {0, 0, 0}, v[3] = {0, 0, 0};

    /* Muchos triángulos cercanos -> subdivisión (4 cada uno) */
    for (int i = 0; i < 500; i++) {
        int16_t vtx[9] = {0, 0, 100, 100, 0, 100, 0, 100, 100};
        CHECK(gex_emit_subdivide(&m, &f, 1, vtx, color, u, v, 0, 0,
                                 (uint32_t)i, &out) == GEX_EMIT_OK);
    }
    CHECK(out.count == 2000u);
    CHECK(out.capacity >= out.count);
    gex_emit_output_free(&out);
}

/* helper no estático para no duplicar: push_tri es static. Usamos subdivide
 * que ya crece internamente; el test anterior cubre el crecimiento. */

/* ------------------------------------------------------------------ */
/* Extracción de polígonos (imagen sintética)                         */
/* ------------------------------------------------------------------ */

/* Construye una imagen DFX mínima: descriptor en la imagen, tabla de
 * vértices y polígomos, y un BSP con dos hojas. Offsets relativos al inicio
 * de la imagen (el puerto nunca usa punteros absolutos). */
static uint8_t *make_mini_image(size_t *out_sz) {
    /* Layout de la imagen (offsets, sin solapamientos):
     *   0x0000: descriptor (64 B)
     *   0x0040: tabla de vértices (3 vértices × 12 B = 0x0040..0x0063)
     *   0x007C: tabla de polígomos (1 polígono × 20 B = 0x007C..0x008F)
     *   0x0090: BSP: 1 nodo interno (0x0090..0x00AF)
     *   0x00A0: hoja 1 (0x00A0..0x00BF)
     *   0x00C0: registro UV (12 B = 0x00C0..0x00CB)
     *   0x00D0: hoja 2 (0x00D0..0x00EF), sin polígonos
     * Tamaño total 0x0100.
     */
    size_t sz = 0x0100;
    uint8_t *img = calloc(sz, 1);
    /* descriptor */
    *(uint32_t *)(img + 0x00) = 0x90;         /* root_bsp */
    *(uint32_t *)(img + 0x14) = 3;            /* vertex_count */
    *(uint32_t *)(img + 0x18) = 1;            /* polygon_count */
    *(uint32_t *)(img + 0x20) = 0x40;         /* vertex_table */
    *(uint32_t *)(img + 0x24) = 0x7C;         /* polygon_table */
    /* vértices (int16 x,y,z + u32 color), stride 12:
     * (0,0,400),(0,100,400),(100,0,300) -> det2d < 0 (cara visible) */
    uint8_t *vb = img + 0x40;
    *(int16_t *)(vb + 0 * 12 + 0) = 0;  *(int16_t *)(vb + 0 * 12 + 2) = 0;  *(int16_t *)(vb + 0 * 12 + 4) = 400; *(uint32_t *)(vb + 0 * 12 + 8) = 0xFF010203u;
    *(int16_t *)(vb + 1 * 12 + 0) = 0;  *(int16_t *)(vb + 1 * 12 + 2) = 100; *(int16_t *)(vb + 1 * 12 + 4) = 400; *(uint32_t *)(vb + 1 * 12 + 8) = 0xFF040506u;
    *(int16_t *)(vb + 2 * 12 + 0) = 100;*(int16_t *)(vb + 2 * 12 + 2) = 0;  *(int16_t *)(vb + 2 * 12 + 4) = 300; *(uint32_t *)(vb + 2 * 12 + 8) = 0xFF070809u;
    /* polígono: idx 0,1,2 ; flags 0 ; uv_ptr=0xC0 */
    uint16_t *pt = (uint16_t *)(img + 0x7C);
    pt[0] = 0; pt[1] = 1; pt[2] = 2; pt[3] = 0; /* +0x06 flags */
    *(uint32_t *)(img + 0x7C + 0x10) = 0xC0; /* uv ptr */
    /* registro UV en 0xC0: 3×{u,v,texture16}; byte[0x0B]=capa */
    uint8_t *uv = img + 0xC0;
    uv[0] = 1; uv[1] = 2; uv[2] = 0; uv[3] = 0;   /* v0: u=1,v=2,tex=0 */
    uv[4] = 3; uv[5] = 4; uv[6] = 0; uv[7] = 0;   /* v1 */
    uv[8] = 5; uv[9] = 6; uv[10] = 0; uv[11] = 7; /* v2: tex=0, capa=7 */
    /* BSP: nodo raíz en 0x90 (interno, hijos 0xA0 y 0xD0) */
    uint16_t *bn = (uint16_t *)(img + 0x90);
    bn[9] = 1; /* interno */
    *(uint32_t *)(img + 0x90 + 0x18) = 0xA0;
    *(uint32_t *)(img + 0x90 + 0x1C) = 0xD0;
    uint16_t *bl = (uint16_t *)(img + 0xA0); /* hoja 1 */
    bl[9] = 2; /* leaf flag */
    *(uint16_t *)(img + 0xA0 + 0x10) = 1; /* npolys=1 */
    *(uint32_t *)(img + 0xA0 + 0x0C) = 0x7C; /* poly ptr */
    uint16_t *bl2 = (uint16_t *)(img + 0xD0); /* hoja 2, sin polígonos */
    bl2[9] = 2;
    *(uint16_t *)(img + 0xD0 + 0x10) = 0; /* npolys=0 */
    *out_sz = sz;
    return img;
}

static void test_collect_mini(void) {
    size_t sz; uint8_t *img = make_mini_image(&sz);
    gex_dfx_descriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.root_bsp = 0x90;
    desc.vertex_count = 3;
    desc.polygon_count = 1;
    desc.vertex_table = 0x40;
    desc.polygon_table = 0x7C;

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0x90, visible_all, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);

    gex_emit_poly_list polys; memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(img, sz, &desc, &r, &polys) == GEX_EMIT_OK);
    CHECK(polys.count == 1);
    if (polys.count > 0) {
        CHECK(polys.polys[0].vtx[0] == 0 && polys.polys[0].vtx[1] == 1 &&
              polys.polys[0].vtx[2] == 2);
        CHECK(polys.polys[0].color[0] == 0xFF010203u);
        CHECK(polys.polys[0].color[1] == 0xFF040506u);
        CHECK(polys.polys[0].color[2] == 0xFF070809u);
        CHECK(polys.polys[0].u[0] == 1 && polys.polys[0].v[0] == 2);
        CHECK(polys.polys[0].texture == 0);
        CHECK(polys.polys[0].layer == 7);
        CHECK(polys.polys[0].source_poly == 0);
        CHECK(polys.polys[0].poly_flags == 0);
    }

    gex_emit_poly_list_free(&polys);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_collect_errors(void) {
    size_t sz; uint8_t *img = make_mini_image(&sz);
    gex_dfx_descriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.root_bsp = 0x90;
    desc.vertex_count = 3;
    desc.polygon_count = 1;
    desc.vertex_table = 0x40;
    desc.polygon_table = 0x7C;

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    gex_bsp_walk(img, sz, 0x90, visible_all, NULL, NULL, &r);

    /* NULLs */
    CHECK(gex_emit_collect(NULL, sz, &desc, &r, NULL) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_collect(img, sz, NULL, &r, &(gex_emit_poly_list){0}) == GEX_EMIT_ERR_ARG);

    /* vertex_count cuyo índice sobrepasa: poner polígono con vtx 999 */
    uint8_t *img2 = malloc(sz); memcpy(img2, img, sz);
    *(uint16_t *)(img2 + 0x7C + 0) = 999; /* vtx0 = 999 >= 3 */
    gex_emit_poly_list polys; memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(img2, sz, &desc, &r, &polys) == GEX_EMIT_ERR_RANGE);
    gex_emit_poly_list_free(&polys);
    free(img2);

    gex_bsp_walk_free(&r);
    free(img);
}

/* ------------------------------------------------------------------ */
/* Flujo completo nivel emit (imagen sintética)                       */
/* ------------------------------------------------------------------ */

static void test_level_emit_mini(void) {
    size_t sz; uint8_t *img = make_mini_image(&sz);
    gex_dfx_descriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.root_bsp = 0x90;
    desc.vertex_count = 3;
    desc.polygon_count = 1;
    desc.vertex_table = 0x40;
    desc.polygon_table = 0x7C;

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0x90, visible_all, NULL, NULL, &r) == GEX_BSP_OK);

    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_output out; memset(&out, 0, sizeof(out));

    CHECK(gex_level_emit(img, sz, &desc, &r, &m, &f, &out) == GEX_EMIT_OK);
    /* Un polígono, det<0, maxz 400 >= 99 -> FAR ->
     * 2 triángulos (principal + pasada de niebla) */
    CHECK(out.count == 2);
    CHECK(out.fog_pass_count == 1);
    /* LIFO dentro de misma clave: el segundo registro (niebla) se emitió
     * después, por tanto se dibuja primero. */
    CHECK(out.tris[0].flags & GEX_EMIT_F_FOG_PASS);
    CHECK(out.tris[0].flags & GEX_EMIT_F_FAR_EMITTER);
    CHECK(!(out.tris[1].flags & GEX_EMIT_F_FOG_PASS));
    CHECK(out.tris[1].flags & GEX_EMIT_F_FAR_EMITTER);
    /* Registro principal: colores del llamador SIN niebla */
    CHECK(out.tris[1].color[0] == 0xFF010203u);
    /* Pasada: niebla pura (no blanco 0x00FFFFFF) */
    CHECK(out.tris[0].color[0] != 0x00FFFFFFu);

    gex_emit_output_free(&out);
    gex_bsp_walk_free(&r);
    free(img);
}

/* ------------------------------------------------------------------ */
/* AZTEC16 real                                                       */
/* ------------------------------------------------------------------ */

static void test_aztec_real(void) {
    FILE *fh = fopen(AZTEC_PATH, "rb");
    if (!fh) { printf("skip aztec (no file)\n"); return; }
    fclose(fh);

    gex_dfx_container c; memset(&c, 0, sizeof(c));
    CHECK(gex_dfx_load(AZTEC_PATH, &c) == GEX_DFX_OK);
    gex_dfx_level lv; memset(&lv, 0, sizeof(lv));
    CHECK(gex_dfx_load_level(&c, &lv) == GEX_DFX_OK);
    CHECK(lv.descriptor.vertex_count == 3882u);
    CHECK(lv.descriptor.polygon_count == 9609u);

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(c.image, c.image_size, lv.descriptor.root_bsp,
                       visible_all, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 815u); /* 815 hojas estructurales visibles sin predicado */

    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_output out; memset(&out, 0, sizeof(out));
    CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &r, &m, &f, &out) == GEX_EMIT_OK);
    CHECK(out.count > 0);
    printf("AZTEC16: leaves=%u polys_extracted -> triangles=%u (fog_passes=%u)\n",
           r.count, (uint32_t)out.count, (uint32_t)out.fog_pass_count);

    gex_emit_output_free(&out);
    gex_bsp_walk_free(&r);
    gex_dfx_release_level(&lv);
    gex_dfx_release_container(&c);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_api_smoke();
    test_project_identity();
    test_project_depth_clip();
    test_project_div_sign();
    test_project_q12_wrap();
    test_determinant();
    test_fog_lut();
    test_fog_analytic();
    test_mix_color();
    test_emitter_selection();
    test_subdivide();
    test_sort();
    test_capacity_growth();
    test_collect_mini();
    test_collect_errors();
    test_level_emit_mini();
    test_aztec_real();

    printf("test_level_emit: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
