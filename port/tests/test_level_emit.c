#include "gex/emit.h"
#include "ref_emit_frames.h"
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
/* Aritmética de referencia (misma semántica que emit.c)              */
/* ------------------------------------------------------------------ */

static int32_t ref_sar32(int32_t v, unsigned n) {
    if (n == 0) return v;
    if (v >= 0) return (int32_t)((uint32_t)v >> n);
    return (int32_t)(~((~(uint32_t)v) >> n));
}

static int32_t ref_i32(uint32_t bits) {
    int32_t v;
    memcpy(&v, &bits, 4);
    return v;
}

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
    /* Generador 0x461895, ahora público en el propio módulo de emisión
     * (gex_emit_fog_generate): una sola implementación para la prueba, la
     * aplicación y cualquier otro consumidor. */
    gex_emit_fog f;
    static const uint8_t rgb[3] = {40, 60, 90};
    gex_emit_fog_generate(&f, total_z, none_z, rgb);
    return f;
}

/* Triángulo directo con det2d < 0 y bucket válido. */
static gex_emit_triangle tri_at(int32_t z, uint32_t c0, uint32_t c1,
                                uint32_t c2) {
    gex_emit_triangle t;
    memset(&t, 0, sizeof(t));
    t.x[0] = 0;   t.x[1] = 0;   t.x[2] = 100;
    t.y[0] = 0;   t.y[1] = 100; t.y[2] = 0;
    t.z[0] = z;   t.z[1] = z;   t.z[2] = z;
    t.color[0] = c0; t.color[1] = c1; t.color[2] = c2;
    return t;
}

/* ------------------------------------------------------------------ */
/* API y reservas                                                     */
/* ------------------------------------------------------------------ */

static void test_api_smoke(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_output out; memset(&out, 0, sizeof(out));

    CHECK(gex_level_emit(NULL, 100, NULL, NULL, &m, &f, 0, &out) == GEX_EMIT_ERR_ARG);
    CHECK(gex_level_emit((const uint8_t *)"x", 100, NULL, NULL, &m, &f, 0, &out) == GEX_EMIT_ERR_ARG);
    CHECK(gex_level_emit((const uint8_t *)"x", 100, NULL, NULL, &m, &f, 0, NULL) == GEX_EMIT_ERR_ARG);
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
    CHECK(!(tri.flags & GEX_EMIT_F_DEPTH_CLIPPED));

    /* color/u/v NULL se aceptan y quedan a cero */
    memset(&tri, 0xAA, sizeof(tri));
    CHECK(gex_emit_project(&m, vtx, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(tri.color[0] == 0 && tri.color[1] == 0 && tri.color[2] == 0);
    CHECK(tri.u[0] == 0 && tri.v[0] == 0);
}

/* ------------------------------------------------------------------ */
/* Proyección: descarte por profundidad (defecto 1: x/y definidos)    */
/* ------------------------------------------------------------------ */

static void test_project_depth_clip(void) {
    gex_emit_matrix m = mat_identity();
    int16_t vtx[9] = {10, 10, 100,    /* z = 100 < near_z 160 -> clipeado */
                      20, 20, 400,
                      30, 30, 300};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(tri.flags & GEX_EMIT_F_DEPTH_CLIPPED);
    /* El original abandona el polígono; el port publica x/y = 0 [C]. */
    for (int j = 0; j < 3; j++) {
        CHECK(tri.x[j] == 0 && tri.y[j] == 0);
    }
    /* z sí queda definido para todos (aritmética determinista) */
    CHECK(tri.z[0] == 100 && tri.z[1] == 400 && tri.z[2] == 300);

    /* z > 0xFFFF via desplazamiento de la matriz (t[2] = 0x10000) */
    gex_emit_matrix m2 = mat_identity();
    m2.t[2] = 0x10000;
    int16_t vtx2[9] = {0, 0, 0, 20, 20, 400, 30, 30, 300};
    CHECK(gex_emit_project(&m2, vtx2, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(tri.flags & GEX_EMIT_F_DEPTH_CLIPPED);
    CHECK(tri.z[0] == 0x10000);
}

/* ------------------------------------------------------------------ */
/* Proyección: división con signo y clamp de x/y                      */
/* ------------------------------------------------------------------ */

static void test_project_div_sign(void) {
    gex_emit_matrix m = mat_identity();
    /* x' negativo, z positivo: idiv truncado hacia cero */
    int16_t vtx[9] = {-100, 0, 400, 0, 0, 400, 0, 0, 400};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    /* xs = (-100*320)/400 + 256 = -80 + 256 = 176 */
    CHECK(tri.x[0] == 176);

    /* Clamp final [-0x400, 0x3FF] */
    int16_t vtx2[9] = {10000, -10000, 400, -10000, 10000, 400, 0, 0, 400};
    CHECK(gex_emit_project(&m, vtx2, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(tri.x[0] == 0x3FF && tri.y[0] == -0x400);
    CHECK(tri.x[1] == -0x400 && tri.y[1] == 0x3FF);
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
    /* cam0 = sar12((0x7FFF*0x7FFF) * 3) con wrap modular de 32 bits */
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    uint32_t prod = (uint32_t)((int32_t)0x7FFF * 0x7FFF); /* 1073676289 */
    uint32_t sum = prod + prod + prod;                    /* 3221028867 */
    int32_t cam0 = ref_sar32(ref_i32(sum), 12);
    CHECK(tri.z[0] == cam0 + m.t[2]);
    CHECK(cam0 < 0); /* el wrap produce un valor negativo */
    CHECK(tri.flags & GEX_EMIT_F_DEPTH_CLIPPED); /* z fuera de [near,0xFFFF] */

    /* t[2] compensa el wrap: z' vuelve a un valor válido y x se proyecta */
    gex_emit_matrix m2 = m;
    m2.t[2] = -cam0 + 1000;
    int16_t vtx2[9] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
                       0x7FFF, 0x7FFF, 0x7FFF};
    CHECK(gex_emit_project(&m2, vtx2, NULL, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(tri.z[0] == 1000);
    CHECK(!(tri.flags & GEX_EMIT_F_DEPTH_CLIPPED));
    CHECK(tri.x[0] == -0x400); /* (cam0*320)/1000 + 256, clampado */
}

/* ------------------------------------------------------------------ */
/* Determinante, gate y prioridad de subdivisión                      */
/* ------------------------------------------------------------------ */

static void test_determinant(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);

    /* zmin = 2000 >= 0x6A4: el gate se aplica (la subdivisión no interviene).
     * (0,0),(100,0),(0,100): det = +10000 >= 0 -> DESCARTADO */
    int16_t vtx[9] = {0, 0, 2000, 100, 0, 2000, 0, 100, 2000};
    uint32_t color[3] = {0, 0, 0};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, color, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    gex_emit_class cls;
    CHECK(gex_emit_classify(&f, &tri, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin < 0); /* descartado por determinante >= 0 */
    CHECK(cls.bucket == -1);

    /* Orden inverso: det = -10000 < 0 -> pasa; bucket 500, emisor lejano. */
    int16_t vtx2[9] = {0, 0, 2000, 0, 100, 2000, 100, 0, 2000};
    CHECK(gex_emit_project(&m, vtx2, color, NULL, NULL, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(gex_emit_classify(&f, &tri, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == 2000);
    CHECK(cls.subdivide == 0);
    CHECK(cls.bucket == 500);
    CHECK(cls.emitter == GEX_EMIT_FAR);
}

static void test_classify_subdivide_priority(void) {
    /* Aunque el gate rechazaría (estado, oculto, capa negativa, det), la
     * subdivisión se decide primero (0x426D09 -> 0x426E71). */
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_triangle t = tri_at(100, 0, 0, 0);
    t.layer = -128;
    t.x[0] = 100; t.x[1] = 0; t.x[2] = 0; /* det > 0 */
    gex_emit_class cls;
    CHECK(gex_emit_classify(&f, &t, 0x8000u, 0x20000u, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == 100);
    CHECK(cls.subdivide == 1);
    CHECK(cls.emitter == GEX_EMIT_NEAR);
    CHECK(cls.fog_applies == 0);
}

static void test_gate(void) {
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_class cls;

    /* Caso base: z = 0x800 -> bucket 512 */
    gex_emit_triangle t = tri_at(0x800, 0, 0, 0);
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == 0x800 && cls.bucket == 0x200);
    CHECK(cls.subdivide == 0);

    /* 1. [0x647578] & 0x60000 */
    CHECK(gex_emit_classify(&f, &t, 0, 0x20000u, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1);
    CHECK(gex_emit_classify(&f, &t, 0, 0x40000u, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1);

    /* 2. vértice clipeado por profundidad */
    gex_emit_triangle tc = t;
    tc.flags |= GEX_EMIT_F_DEPTH_CLIPPED;
    CHECK(gex_emit_classify(&f, &tc, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1);

    /* 3. det2d >= 0 */
    gex_emit_triangle td = t;
    td.x[0] = 100; td.x[1] = 0; td.x[2] = 0;
    CHECK(gex_emit_classify(&f, &td, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1);

    /* 4. bucket: límite superior 0xC00 y capa negativa */
    gex_emit_triangle tb = tri_at(0x3000, 0, 0, 0); /* 0xC00 */
    CHECK(gex_emit_classify(&f, &tb, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1);
    tb = tri_at(0x2FFC, 0, 0, 0); /* 0xBFF: pasa */
    CHECK(gex_emit_classify(&f, &tb, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.bucket == 0xBFF);
    gex_emit_triangle tn = tri_at(0x6A4, 0, 0, 0);
    tn.layer = -128; /* 425 - 512 = -87 */
    CHECK(gex_emit_classify(&f, &tn, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1);

    /* 5. poly+0x06 & 0x8000 */
    CHECK(gex_emit_classify(&f, &t, 0x8000u, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1);

    /* 6. fuera de pantalla: todos al mismo lado */
    gex_emit_triangle to;
    memset(&to, 0, sizeof(to));
    to.z[0] = to.z[1] = to.z[2] = 0x800;
    to.x[0] = -100; to.x[1] = -1; to.x[2] = -1;
    to.y[0] = 0;    to.y[1] = 100; to.y[2] = 0; /* det = -9900 */
    CHECK(gex_emit_classify(&f, &to, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1); /* todos x < 0 */

    to.x[0] = 0x300; to.x[1] = 0x201; to.x[2] = 0x201;
    to.y[0] = 0;     to.y[1] = 0;     to.y[2] = 100; /* det = -25600 */
    CHECK(gex_emit_classify(&f, &to, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1); /* todos x > 0x200 */

    to.x[0] = 0; to.x[1] = 100; to.x[2] = 0;
    to.y[0] = -1; to.y[1] = -1; to.y[2] = -100; /* det = -9900 */
    CHECK(gex_emit_classify(&f, &to, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1); /* todos y < 0 */

    to.x[0] = 0; to.x[1] = 100; to.x[2] = 0;
    to.y[0] = 0x181; to.y[1] = 0x181; to.y[2] = 0x300; /* det = -100 */
    CHECK(gex_emit_classify(&f, &to, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == -1); /* todos y > 0x180 */

    /* Frontera: exactamente 0/0x200/0x180 no descarta */
    to.x[0] = 0; to.x[1] = 0; to.x[2] = 0x200;
    to.y[0] = 0; to.y[1] = 0x180; to.y[2] = 0;
    CHECK(gex_emit_classify(&f, &to, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == 0x800);

    /* Determinante con wrap de 32 bits (defecto 2): coordenadas extremas.
     * a = -0xFFFF, b = -0xFFFF, c = d = 0 -> a*b = 0xFFFF0001 -> -0x1FFFF.
     * Sin wrap modular el producto sería positivo y se descartaría. */
    gex_emit_triangle tw;
    memset(&tw, 0, sizeof(tw));
    tw.z[0] = tw.z[1] = tw.z[2] = 0x800;
    tw.x[0] = 0x7FFF; tw.x[1] = -0x8000; tw.x[2] = 0x7FFF;
    tw.y[0] = 0x7FFF; tw.y[1] = 0x7FFF; tw.y[2] = -0x8000;
    CHECK(gex_emit_classify(&f, &tw, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == 0x800); /* det envuelto < 0: pasa el gate */
    CHECK(cls.bucket == 0x200);
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
    f.a_q12 = -544768;   /* (A<<12) = -133<<12 */
    f.b_q12 = 5461;      /* B = 5461 */
    /* factor(400) = 5461 + (-544768)/400 = 5461 - 1361 = 4100 -> 4096 */
    CHECK(gex_emit_fog_factor(&f, 400) == 0x1000);
    CHECK(gex_emit_fog_factor(&f, 1) == 0); /* 5461 - 544768 -> clamp 0 */
    f.a_q12 = 0; f.b_q12 = 100;
    CHECK(gex_emit_fog_factor(&f, 50) == 100);   /* z != 0 */
    CHECK(gex_emit_fog_factor(&f, 0) == 100);    /* z == 0 -> sin dividir */
    /* División truncada hacia cero y clamp inferior */
    f.a_q12 = 1000; f.b_q12 = 0;
    CHECK(gex_emit_fog_factor(&f, 300) == 3);
    f.a_q12 = -1000;
    CHECK(gex_emit_fog_factor(&f, 300) == 0);
}

/* El generador público 0x461895 y la coherencia entre la ruta LUT y la
 * ruta analítica (0x46A150). */
static void test_fog_generator(void) {
    gex_emit_fog f, g;
    static const uint8_t rgb[3] = {40, 60, 90};

    gex_emit_fog_generate(&f, 5000, 3000, rgb);
    CHECK(f.have_lut == 1);
    CHECK(f.fog_total_z == 5000);
    CHECK(f.fog_none_z == 3000);
    CHECK(f.threshold == 3000);
    CHECK(f.split_z == f.threshold);
    CHECK(f.b_q12 > 0);
    CHECK(f.a_q12 < 0);
    CHECK(f.fog_rgb[0] == 40 && f.fog_rgb[1] == 60 && f.fog_rgb[2] == 90);
    CHECK(f.lut[0] == 0);

    /* Sin niebla por debajo de none_z y niebla total desde total_z. */
    CHECK(gex_emit_fog_factor(&f, 2000) == 0);
    CHECK(gex_emit_fog_factor(&f, 5000) == 0x1000);

    /* La ruta analítica con los mismos A y B da el mismo factor que la LUT
     * (la tabla usa el indice z>>2 con i = 4k+1, asi que hay una diferencia
     * minima de redondeo). */
    memset(&g, 0, sizeof(g));
    g.have_lut = 0;
    g.a_q12 = f.a_q12;
    g.b_q12 = f.b_q12;
    for (int z = 500; z <= 9000; z += 250) {
        int32_t lut = gex_emit_fog_factor(&f, z);
        int32_t ana = gex_emit_fog_factor(&g, z);
        int32_t diff = lut - ana;
        if (diff < 0) diff = -diff;
        CHECK(diff <= 8);
    }
}

/* ------------------------------------------------------------------ */
/* Mezcla de colores real (0x46B727) vía classify con LUT controlada  */
/* ------------------------------------------------------------------ */

static void test_mix_color(void) {
    gex_emit_fog f;
    memset(&f, 0, sizeof(f));
    f.have_lut = 1;
    f.threshold = 0;            /* siempre aplica niebla */
    f.split_z = 0x7FFFFFFF;     /* emisor cercano: sin pasada extra */
    f.fog_rgb[0] = 200; f.fog_rgb[1] = 100; f.fog_rgb[2] = 50;
    /* z = 0x800 -> índice 0x200 */
    gex_emit_triangle t = tri_at(0x800, 0xAA112233u, 0, 0);
    gex_emit_class cls;

    /* factor 0: color intacto */
    f.lut[0x200] = 0;
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.fog_color[0] == 0xAA112233u);

    /* factor 0x1000: extremo exacto; alfa se conserva */
    f.lut[0x200] = 0x1000;
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.fog_color[0] == 0xAA3264C8u); /* r=50,g=100,b=200, a=AA */

    /* factor 0x800: (149*2048)>>12 = 74 (truncado), etc. */
    f.lut[0x200] = 0x800;
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.fog_color[0] == 0xAA21437Du); /* b=125,g=67,r=33 */

    /* delta negativo: 200 -> 100 con factor 0x800 = 150 */
    f.fog_rgb[0] = 100; f.fog_rgb[1] = 34; f.fog_rgb[2] = 17;
    t.color[0] = 0xAA1122C8u; /* b=200,g=34,r=17 */
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.fog_color[0] == 0xAA112296u); /* b=150, g=34, r=17 */

    /* saturación superior: 10 + (190*8192)>>12 = 390 -> 255 */
    f.fog_rgb[0] = 200;
    t.color[0] = 0xFF11220Au;
    f.lut[0x200] = 0x2000;
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.fog_color[0] == 0xFF1122FFu);

    /* saturación inferior: 200 + (-190*2) = -180 -> 0 */
    f.fog_rgb[0] = 10;
    t.color[0] = 0x001122C8u;
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.fog_color[0] == 0x00112200u);

    /* factor 0x1000 con desde == hacia: sin cambio */
    f.fog_rgb[0] = 0x33; f.fog_rgb[1] = 0x22; f.fog_rgb[2] = 0x11;
    t.color[0] = 0xAA112233u;
    f.lut[0x200] = 0x1000;
    CHECK(gex_emit_classify(&f, &t, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.fog_color[0] == 0xAA112233u);
}

/* ------------------------------------------------------------------ */
/* Selección de emisor cercano / lejos                                */
/* ------------------------------------------------------------------ */

static void test_emitter_selection(void) {
    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100); /* threshold=split_z=99 */
    uint32_t color[3] = {0xFF102030u, 0xFF405060u, 0xFF708090u};
    uint8_t u[3] = {1, 2, 3}, v[3] = {4, 5, 6};

    /* zmin = 2000 >= 0x6A4: no subdivide; 2000 >= 99 -> FAR.
     * Orden de vértices tal que det2d < 0 (cara visible). */
    int16_t vtx[9] = {0, 0, 2000, 0, 10, 2000, 10, 0, 2000};
    gex_emit_triangle tri;
    CHECK(gex_emit_project(&m, vtx, color, u, v, 0x100, 3, 11, &tri) == GEX_EMIT_OK);
    gex_emit_class cls;
    CHECK(gex_emit_classify(&f, &tri, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == 2000);
    CHECK(cls.subdivide == 0);
    CHECK(cls.fog_applies == 1);
    CHECK(cls.emitter == GEX_EMIT_FAR);
    CHECK(cls.bucket == 500 + 12);

    /* Forzar cercano con flag 0x4 */
    CHECK(gex_emit_classify(&f, &tri, 0x0004, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.emitter == GEX_EMIT_NEAR);
    CHECK(cls.fog_applies == 1);

    /* Subdivisión (zmin < 0x6A4) manda sobre el emisor lejano */
    int16_t vtx2[9] = {0, 0, 400, 0, 10, 400, 10, 0, 400};
    CHECK(gex_emit_project(&m, vtx2, color, u, v, 0, 0, 0, &tri) == GEX_EMIT_OK);
    CHECK(gex_emit_classify(&f, &tri, 0, 0, &cls) == GEX_EMIT_OK);
    CHECK(cls.zmin == 400);
    CHECK(cls.subdivide == 1);
    CHECK(cls.fog_applies == 0);
    CHECK(cls.emitter == GEX_EMIT_NEAR);
}

/* ------------------------------------------------------------------ */
/* Subdivisión: 4 principales + slivers (orden y datos)               */
/* ------------------------------------------------------------------ */

static void test_subdivide(void) {
    gex_emit_matrix m = mat_identity();
    m.near_z = 0x50; /* z = 100 válido para proyectar */
    m.cy = 0;        /* geometría centrada en el origen de pantalla */
    uint32_t color[3] = {0xFD010203u, 0x11223344u, 0x55667788u};
    uint8_t u[3] = {10, 20, 30}, v[3] = {40, 50, 60};
    /* v0=(-600,-600), v1=(600,600), v2=(0,-600); z=100.
     * El clamp de x/y a [-0x400,0x3FF] rompe la colinealidad del sliver
     * (v0,v1,m01): pasan los 4 principales y ese sliver -> 5 triángulos. */
    int16_t vtx[9] = {-600, -600, 100, 600, 600, 100, 0, -600, 100};
    gex_emit_output out; memset(&out, 0, sizeof(out));

    CHECK(gex_emit_subdivide(&m, vtx, color, u, v, 0x7, 2, 0, 0, 999,
                             &out) == GEX_EMIT_OK);
    CHECK(out.count == 5);
    CHECK(out.fog_pass_count == 0);
    if (out.count == 5) {
        const int16_t ex[5][3] = {
            {-1024, 256, -704},
            {-704, 1023, 256},
            {256, 1023, -704},
            {256, 1023, 1023},
            {-1024, 1023, 256},
        };
        const int16_t ey[5][3] = {
            {-1024, 0, -1024},
            {-1024, 0, -1024},
            {0, 0, -1024},
            {0, 1023, 0},
            {-1024, 1023, 0},
        };
        for (size_t i = 0; i < out.count; i++) {
            CHECK(out.tris[i].flags & GEX_EMIT_F_SUBDIVIDED);
            CHECK(out.tris[i].flags & GEX_EMIT_F_NEAR_EMITTER);
            CHECK(!(out.tris[i].flags & GEX_EMIT_F_FAR_EMITTER));
            CHECK(!(out.tris[i].flags & GEX_EMIT_F_FOG_PASS));
            CHECK(out.tris[i].source_poly == 999);
            CHECK(out.tris[i].texture == 0x7 && out.tris[i].layer == 2);
            for (int j = 0; j < 3; j++) {
                CHECK(out.tris[i].x[j] == ex[i][j]);
                CHECK(out.tris[i].y[j] == ey[i][j]);
            }
        }
        /* Colores: c01, c20, c12 con truncamiento y alfa del primer color */
        CHECK(out.tris[0].color[0] == color[0]);
        CHECK(out.tris[0].color[1] == 0xFD111A23u);
        CHECK(out.tris[0].color[2] == 0x55333C45u);
        CHECK(out.tris[1].color[0] == 0x55333C45u);
        CHECK(out.tris[1].color[1] == 0x11445566u);
        CHECK(out.tris[1].color[2] == color[2]);
        /* UV medias */
        CHECK(out.tris[0].u[0] == 10 && out.tris[0].u[1] == 15 &&
              out.tris[0].u[2] == 20);
        CHECK(out.tris[0].v[0] == 40 && out.tris[0].v[1] == 45 &&
              out.tris[0].v[2] == 50);
        CHECK(out.tris[1].u[0] == 20 && out.tris[1].u[1] == 25);
        CHECK(out.tris[1].v[0] == 50 && out.tris[1].v[1] == 55);
    }
    gex_emit_output_free(&out);

    /* Alfa con bit 1 puesto: sin slivers -> 4 */
    memset(&out, 0, sizeof(out));
    uint32_t color4[3] = {0xFF010203u, color[1], color[2]};
    CHECK(gex_emit_subdivide(&m, vtx, color4, u, v, 0x7, 2, 0, 0, 1,
                             &out) == GEX_EMIT_OK);
    CHECK(out.count == 4);
    gex_emit_output_free(&out);

    /* Un principal rechazado por el gate aborta conservando lo emitido:
     * estado de recorte global -> el primer subtriángulo ya no pasa. */
    memset(&out, 0, sizeof(out));
    CHECK(gex_emit_subdivide(&m, vtx, color4, u, v, 0x7, 2, 0,
                             GEX_EMIT_STATE_CLIP_MASK, 1, &out) == GEX_EMIT_OK);
    CHECK(out.count == 0);
    gex_emit_output_free(&out);
}

/* ------------------------------------------------------------------ */
/* Ordenación por bucket (zmin>>2 + 4*layer) y LIFO                   */
/* ------------------------------------------------------------------ */

static void test_sort(void) {
    gex_emit_triangle ts[6];
    memset(ts, 0, sizeof(ts));
    /* t0: z=400, capa 0 -> clave 100 */
    ts[0].z[0] = ts[0].z[1] = ts[0].z[2] = 400; ts[0].layer = 0;
    ts[0].seq = 0; ts[0].source_poly = 0;
    /* t1: z=800 -> 200 */
    ts[1].z[0] = ts[1].z[1] = ts[1].z[2] = 800; ts[1].layer = 0;
    ts[1].seq = 1; ts[1].source_poly = 1;
    /* t2: z=400, capa 1 -> 104 */
    ts[2].z[0] = ts[2].z[1] = ts[2].z[2] = 400; ts[2].layer = 1;
    ts[2].seq = 2; ts[2].source_poly = 2;
    /* t3: z=400, capa -1 -> 96 */
    ts[3].z[0] = ts[3].z[1] = ts[3].z[2] = 400; ts[3].layer = -1;
    ts[3].seq = 3; ts[3].source_poly = 3;
    /* t4: z=400, capa 0, mismo bucket que t0 -> LIFO (seq 4) */
    ts[4].z[0] = ts[4].z[1] = ts[4].z[2] = 400; ts[4].layer = 0;
    ts[4].seq = 4; ts[4].source_poly = 4;
    /* t5: z=400, capa -128 -> 100 - 512 = -412 */
    ts[5].z[0] = ts[5].z[1] = ts[5].z[2] = 400; ts[5].layer = -128;
    ts[5].seq = 5; ts[5].source_poly = 5;

    CHECK(gex_emit_sort(ts, 6) == GEX_EMIT_OK);
    CHECK(ts[0].source_poly == 1); /* 200 */
    CHECK(ts[1].source_poly == 2); /* 104 */
    CHECK(ts[2].source_poly == 4); /* 100, seq 4 */
    CHECK(ts[3].source_poly == 0); /* 100, seq 0 */
    CHECK(ts[4].source_poly == 3); /* 96 */
    CHECK(ts[5].source_poly == 5); /* -412 */
}

/* ------------------------------------------------------------------ */
/* Capacidad y crecimiento                                            */
/* ------------------------------------------------------------------ */

static void test_capacity_growth(void) {
    gex_emit_matrix m = mat_identity();
    m.near_z = 0x50;
    m.cy = 0;
    gex_emit_output out; memset(&out, 0, sizeof(out));
    uint32_t color[3] = {0xFD000000u, 0x11223344u, 0x55667788u};
    uint8_t u[3] = {0, 1, 2}, v[3] = {3, 4, 5};

    /* 500 subdivisiones x 5 triángulos = 2500 */
    for (int i = 0; i < 500; i++) {
        int16_t vtx[9] = {-600, -600, 100, 600, 600, 100, 0, -600, 100};
        CHECK(gex_emit_subdivide(&m, vtx, color, u, v, 0, 0, 0, 0,
                                 (uint32_t)i, &out) == GEX_EMIT_OK);
    }
    CHECK(out.count == 2500u);
    CHECK(out.capacity >= out.count);
    CHECK(out.overflow == 0);
    gex_emit_output_free(&out);
}

/* ------------------------------------------------------------------ */
/* Extracción de polígonos (imagen sintética sin solapamientos)       */
/* ------------------------------------------------------------------ */

/* Layout:
 *   0x000 descriptor (64 B)
 *   0x040 tabla de vértices (3 x 12 B)      -> 0x040..0x063
 *   0x070 tabla de polígonos (1 x 20 B)     -> 0x070..0x083
 *   0x090 registro UV (12 B)                -> 0x090..0x09B
 *   0x0A0 nodo raíz BSP (32 B)              -> 0x0A0..0x0BF
 *   0x0C0 hoja 1 (32 B)                     -> 0x0C0..0x0DF
 *   0x0E0 hoja 2 (32 B, sin polígonos)      -> 0x0E0..0x0FF
 * Tamaño total 0x100. */
static uint8_t *make_mini_image(size_t *out_sz) {
    size_t sz = 0x0100;
    uint8_t *img = calloc(sz, 1);
    /* descriptor */
    *(uint32_t *)(img + 0x00) = 0xA0;         /* root_bsp */
    *(uint32_t *)(img + 0x14) = 3;            /* vertex_count */
    *(uint32_t *)(img + 0x18) = 1;            /* polygon_count */
    *(uint32_t *)(img + 0x20) = 0x40;         /* vertex_table */
    *(uint32_t *)(img + 0x24) = 0x70;         /* polygon_table */
    /* vértices (int16 x,y,z + u32 color), stride 12:
     * (0,0,400),(0,100,400),(100,0,300) -> det2d < 0 (cara visible) */
    uint8_t *vb = img + 0x40;
    *(int16_t *)(vb + 0 * 12 + 0) = 0;  *(int16_t *)(vb + 0 * 12 + 2) = 0;  *(int16_t *)(vb + 0 * 12 + 4) = 400; *(uint32_t *)(vb + 0 * 12 + 8) = 0xFF010203u;
    *(int16_t *)(vb + 1 * 12 + 0) = 0;  *(int16_t *)(vb + 1 * 12 + 2) = 100; *(int16_t *)(vb + 1 * 12 + 4) = 400; *(uint32_t *)(vb + 1 * 12 + 8) = 0xFF040506u;
    *(int16_t *)(vb + 2 * 12 + 0) = 100;*(int16_t *)(vb + 2 * 12 + 2) = 0;  *(int16_t *)(vb + 2 * 12 + 4) = 300; *(uint32_t *)(vb + 2 * 12 + 8) = 0xFF070809u;
    /* polígono: idx 0,1,2 ; flags 0 ; uv_ptr=0x90 */
    uint16_t *pt = (uint16_t *)(img + 0x70);
    pt[0] = 0; pt[1] = 1; pt[2] = 2; pt[3] = 0; /* +0x06 flags */
    *(uint32_t *)(img + 0x70 + 0x10) = 0x90;    /* uv ptr */
    /* registro UV en 0x90: 3 x {u,v,texture16}; byte[0x0B]=capa */
    uint8_t *uv = img + 0x90;
    uv[0] = 1; uv[1] = 2; uv[2] = 0x23; uv[3] = 0x01; /* v0: u=1,v=2,tex=0x123 */
    uv[4] = 3; uv[5] = 4; uv[6] = 0; uv[7] = 0;       /* v1 */
    uv[8] = 5; uv[9] = 6; uv[10] = 0; uv[11] = 7;     /* v2: capa=7 */
    /* BSP: nodo raíz en 0xA0 (interno, hijos 0xC0 y 0xE0) */
    *(uint16_t *)(img + 0xA0 + 0x12) = 1; /* interno */
    *(uint32_t *)(img + 0xA0 + 0x18) = 0xC0;
    *(uint32_t *)(img + 0xA0 + 0x1C) = 0xE0;
    /* hoja 1 en 0xC0 */
    *(uint16_t *)(img + 0xC0 + 0x12) = 2; /* leaf flag */
    *(uint16_t *)(img + 0xC0 + 0x10) = 1; /* npolys=1 */
    *(uint32_t *)(img + 0xC0 + 0x0C) = 0x70;
    /* hoja 2 en 0xE0, sin polígonos */
    *(uint16_t *)(img + 0xE0 + 0x12) = 2;
    *(uint16_t *)(img + 0xE0 + 0x10) = 0;
    *out_sz = sz;
    return img;
}

static void fill_desc(gex_dfx_descriptor *d) {
    memset(d, 0, sizeof(*d));
    d->root_bsp = 0xA0;
    d->vertex_count = 3;
    d->polygon_count = 1;
    d->vertex_table = 0x40;
    d->polygon_table = 0x70;
}

static void test_collect_mini(void) {
    size_t sz; uint8_t *img = make_mini_image(&sz);
    gex_dfx_descriptor desc; fill_desc(&desc);

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0xA0, visible_all, NULL, NULL, &r) == GEX_BSP_OK);
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
        CHECK(polys.polys[0].texture == 0x123);
        CHECK(polys.polys[0].layer == 7);
        CHECK(polys.polys[0].source_poly == 0);
        CHECK(polys.polys[0].poly_flags == 0);
    }

    gex_emit_poly_list_free(&polys);
    gex_bsp_walk_free(&r);
    free(img);
}

/* Defecto 3: offsets y sumas que desbordan 32 bits nunca leen. */
static void test_collect_offsets(void) {
    size_t sz; uint8_t *img = make_mini_image(&sz);
    gex_dfx_descriptor desc; fill_desc(&desc);

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0xA0, visible_all, NULL, NULL, &r) == GEX_BSP_OK);

    uint8_t *copy = malloc(sz);
    gex_emit_poly_list polys;

    /* NULLs */
    CHECK(gex_emit_collect(NULL, sz, &desc, &r, &polys) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_collect(img, sz, NULL, &r, &polys) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_collect(img, sz, &desc, NULL, &polys) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_collect(img, sz, &desc, &r, NULL) == GEX_EMIT_ERR_ARG);

    /* count > 0 con lista nula */
    {
        gex_bsp_walk_result bad = r;
        bad.leaves = NULL;
        memset(&polys, 0, sizeof(polys));
        CHECK(gex_emit_collect(img, sz, &desc, &bad, &polys) == GEX_EMIT_ERR_ARG);
        gex_emit_poly_list_free(&polys);
    }

    /* Hoja fuera de la imagen (offset con wrap potencial) */
    {
        gex_bsp_walk_result bad = r;
        uint32_t fake[1] = {0xFFFFFFF0u};
        bad.leaves = fake;
        bad.count = 1;
        memset(&polys, 0, sizeof(polys));
        CHECK(gex_emit_collect(img, sz, &desc, &bad, &polys) == GEX_EMIT_ERR_RANGE);
        gex_emit_poly_list_free(&polys);
        fake[0] = 0xFFFFFFFFu;
        CHECK(gex_emit_collect(img, sz, &desc, &bad, &polys) == GEX_EMIT_ERR_RANGE);
        gex_emit_poly_list_free(&polys);
    }

    /* Puntero de polígonos con wrap */
    memcpy(copy, img, sz);
    *(uint32_t *)(copy + 0xC0 + 0x0C) = 0xFFFFFFF0u;
    memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(copy, sz, &desc, &r, &polys) == GEX_EMIT_ERR_RANGE);
    gex_emit_poly_list_free(&polys);

    /* Puntero UV con wrap */
    memcpy(copy, img, sz);
    *(uint32_t *)(copy + 0x70 + 0x10) = 0xFFFFFFFFu;
    memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(copy, sz, &desc, &r, &polys) == GEX_EMIT_ERR_RANGE);
    gex_emit_poly_list_free(&polys);

    /* Tabla de vértices del descriptor con wrap */
    {
        gex_dfx_descriptor bad = desc;
        bad.vertex_table = 0xFFFFFFF0u;
        memset(&polys, 0, sizeof(polys));
        CHECK(gex_emit_collect(img, sz, &bad, &r, &polys) == GEX_EMIT_ERR_RANGE);
        gex_emit_poly_list_free(&polys);
        bad = desc;
        bad.vertex_count = 0xFFFFFFFFu;
        /* En 64 bits el producto no desborda size_t: se detecta como rango. */
        CHECK(gex_emit_collect(img, sz, &bad, &r, &polys) == GEX_EMIT_ERR_RANGE);
        gex_emit_poly_list_free(&polys);
        bad = desc;
        bad.polygon_table = 0xFFFFFFF0u;
        CHECK(gex_emit_collect(img, sz, &bad, &r, &polys) == GEX_EMIT_ERR_RANGE);
        gex_emit_poly_list_free(&polys);
    }

    /* np muy grande: la tabla de polígonos no cabe */
    memcpy(copy, img, sz);
    *(uint16_t *)(copy + 0xC0 + 0x10) = 0x7FFF;
    memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(copy, sz, &desc, &r, &polys) == GEX_EMIT_ERR_RANGE);
    gex_emit_poly_list_free(&polys);

    /* np negativo (int16) no itera */
    memcpy(copy, img, sz);
    *(uint16_t *)(copy + 0xC0 + 0x10) = 0xFFFF;
    memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(copy, sz, &desc, &r, &polys) == GEX_EMIT_OK);
    CHECK(polys.count == 0);
    gex_emit_poly_list_free(&polys);

    /* Índice de vértice fuera de la tabla */
    memcpy(copy, img, sz);
    *(uint16_t *)(copy + 0x70 + 0) = 999;
    memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(copy, sz, &desc, &r, &polys) == GEX_EMIT_ERR_RANGE);
    gex_emit_poly_list_free(&polys);

    /* Imagen vacía */
    memset(&polys, 0, sizeof(polys));
    CHECK(gex_emit_collect(img, 0, &desc, &r, &polys) == GEX_EMIT_ERR_RANGE);
    gex_emit_poly_list_free(&polys);

    free(copy);
    gex_bsp_walk_free(&r);
    free(img);
}

/* ------------------------------------------------------------------ */
/* Flujo completo nivel emit (imagen sintética)                       */
/* ------------------------------------------------------------------ */

static void test_level_emit_mini(void) {
    size_t sz; uint8_t *img = make_mini_image(&sz);
    gex_dfx_descriptor desc; fill_desc(&desc);

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0xA0, visible_all, NULL, NULL, &r) == GEX_BSP_OK);

    gex_emit_matrix m = mat_identity();
    gex_emit_fog f = fog_with_lut(400, 100);
    gex_emit_output out; memset(&out, 0, sizeof(out));

    CHECK(gex_level_emit(img, sz, &desc, &r, &m, &f, 0, &out) == GEX_EMIT_OK);
    /* zmin 400 < 0x6A4 -> subdivisión en 4; alfa 0xFF -> sin slivers.
     * La subdivisión no aplica niebla: 0 pasadas. */
    CHECK(out.count == 4);
    CHECK(out.fog_pass_count == 0);
    for (size_t i = 0; i < out.count; i++) {
        CHECK(out.tris[i].flags & GEX_EMIT_F_SUBDIVIDED);
        CHECK(out.tris[i].flags & GEX_EMIT_F_NEAR_EMITTER);
        CHECK(!(out.tris[i].flags & GEX_EMIT_F_FAR_EMITTER));
        CHECK(out.tris[i].layer == 7);
        CHECK(out.tris[i].texture == 0x123);
    }
    /* Orden: clave (zmin>>2)+4*layer descendente; LIFO (seq descendente)
     * dentro de la misma clave. Los subtriángulos tienen zmin 400 o 350. */
    for (size_t i = 0; i + 1 < out.count; i++) {
        const gex_emit_triangle *a = &out.tris[i];
        const gex_emit_triangle *b = &out.tris[i + 1];
        int32_t ma = a->z[0] > a->z[1] ? a->z[0] : a->z[1];
        if (a->z[2] > ma) ma = a->z[2];
        int32_t mb = b->z[0] > b->z[1] ? b->z[0] : b->z[1];
        if (b->z[2] > mb) mb = b->z[2];
        int32_t ka = (ma >> 2) + 4 * (int32_t)a->layer;
        int32_t kb = (mb >> 2) + 4 * (int32_t)b->layer;
        CHECK(ka >= kb);
        if (ka == kb) CHECK(a->seq > b->seq);
    }

    gex_emit_output_free(&out);
    gex_bsp_walk_free(&r);
    free(img);
}

/* ------------------------------------------------------------------ */
/* AZTEC16 real: validación estructural y determinismo                */
/* ------------------------------------------------------------------ */

static uint32_t hash_tris(const gex_emit_output *out) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < out->count; i++) {
        const gex_emit_triangle *t = &out->tris[i];
        for (int j = 0; j < 3; j++) {
            h = (h ^ t->color[j]) * 16777619u;
            h = (h ^ (uint32_t)(uint16_t)t->x[j]) * 16777619u;
            h = (h ^ (uint32_t)(uint16_t)t->y[j]) * 16777619u;
            h = (h ^ (uint32_t)t->z[j]) * 16777619u;
            h = (h ^ t->u[j]) * 16777619u;
            h = (h ^ t->v[j]) * 16777619u;
        }
        h = (h ^ (uint32_t)t->texture) * 16777619u;
        h = (h ^ (uint32_t)t->layer) * 16777619u;
        h = (h ^ t->flags) * 16777619u;
        h = (h ^ t->source_poly) * 16777619u;
        h = (h ^ t->seq) * 16777619u;
    }
    return h;
}

static void check_structural(const gex_emit_output *out, uint32_t poly_count) {
    size_t fog = 0;
    int32_t prev_key = 0x7FFFFFFF;
    uint32_t prev_seq = 0;
    int first = 1;
    for (size_t i = 0; i < out->count; i++) {
        const gex_emit_triangle *t = &out->tris[i];
        int near = (t->flags & GEX_EMIT_F_NEAR_EMITTER) != 0;
        int far = (t->flags & GEX_EMIT_F_FAR_EMITTER) != 0;
        CHECK(near != far); /* exactamente un emisor */
        if (t->flags & GEX_EMIT_F_FOG_PASS) {
            CHECK(far);
            fog++;
        }
        CHECK(t->texture <= 0x3FFFu);
        CHECK(t->source_poly < poly_count);
        for (int j = 0; j < 3; j++) {
            CHECK(t->x[j] >= GEX_EMIT_XY_MIN && t->x[j] <= GEX_EMIT_XY_MAX);
            CHECK(t->y[j] >= GEX_EMIT_XY_MIN && t->y[j] <= GEX_EMIT_XY_MAX);
        }
        /* La clave de bucket usa la profundidad MINIMA (0x426303). */
        int32_t mz = t->z[0];
        if (t->z[1] < mz) mz = t->z[1];
        if (t->z[2] < mz) mz = t->z[2];
        int32_t key = (int32_t)((uint32_t)mz >> 2) + 4 * (int32_t)t->layer;
        if (!first) {
            CHECK(prev_key >= key);
            if (prev_key == key) CHECK(prev_seq > t->seq);
        }
        prev_key = key;
        prev_seq = t->seq;
        first = 0;
    }
    CHECK(fog == out->fog_pass_count);
}

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
    CHECK(r.count == 815u);

    /* Matriz de cámara sintética: escala 0.25 y traslación +0x1000 mantienen
     * la profundidad dentro del rango de buckets [0, 0xC00) y la geometría
     * real en pantalla. No pretende reproducir la cámara del juego. */
    gex_emit_matrix m = mat_identity();
    m.m[0] = 0x0400;
    m.m[4] = 0x0400;
    m.m[8] = 0x0400;
    m.t[2] = 0x1000;
    gex_emit_fog f = fog_with_lut(400, 100);

    /* Ruta cercana (split_z inalcanzable) */
    gex_emit_fog fnear = f;
    fnear.split_z = 0x7FFFFFFF;
    gex_emit_output near_out; memset(&near_out, 0, sizeof(near_out));
    CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &r, &m,
                         &fnear, 0, &near_out) == GEX_EMIT_OK);
    CHECK(near_out.count > 0);
    CHECK(near_out.fog_pass_count == 0);
    check_structural(&near_out, lv.descriptor.polygon_count);

    /* Ruta lejana (split_z = 0): registro principal + pasada de niebla */
    gex_emit_fog ffar = f;
    ffar.split_z = 0;
    gex_emit_output far_out; memset(&far_out, 0, sizeof(far_out));
    CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &r, &m,
                         &ffar, 0, &far_out) == GEX_EMIT_OK);
    CHECK(far_out.count > 0);
    CHECK(far_out.fog_pass_count > 0);
    check_structural(&far_out, lv.descriptor.polygon_count);

    /* Estado de recorte global: todo descartado (incluidos subdivididos) */
    gex_emit_output clip_out; memset(&clip_out, 0, sizeof(clip_out));
    CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &r, &m, &f,
                         GEX_EMIT_STATE_CLIP_MASK, &clip_out) == GEX_EMIT_OK);
    CHECK(clip_out.count == 0);

    /* Determinismo bit a bit entre dos ejecuciones idénticas */
    gex_emit_output rep; memset(&rep, 0, sizeof(rep));
    CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &r, &m,
                         &ffar, 0, &rep) == GEX_EMIT_OK);
    CHECK(rep.count == far_out.count);
    CHECK(rep.fog_pass_count == far_out.fog_pass_count);
    CHECK(hash_tris(&rep) == hash_tris(&far_out));

    printf("AZTEC16: leaves=%u near=%u (fog=%u) far=%u (fog=%u)\n",
           r.count, (uint32_t)near_out.count, (uint32_t)near_out.fog_pass_count,
           (uint32_t)far_out.count, (uint32_t)far_out.fog_pass_count);

    gex_emit_output_free(&rep);
    gex_emit_output_free(&clip_out);
    gex_emit_output_free(&far_out);
    gex_emit_output_free(&near_out);
    gex_bsp_walk_free(&r);
    gex_dfx_release_level(&lv);
    gex_dfx_release_container(&c);
}

/* ------------------------------------------------------------------ */


/* ---------------------------------------------------------------------------
 * Referencia dinamica del original (2026-11-11).
 *
 * Contexto de emision, niebla y profundidades capturados del GEX3D.EXE en
 * ejecucion con el nivel `map7.dfx` (el que carga el juego). Se exige:
 *  - que la proyeccion del port reproduzca bit a bit las profundidades
 *    capturadas ([0x647430]/[0x64742C]/[0x6474A0]);
 *  - que la profundidad de referencia sea la MINIMA de las tres (0x426D03),
 *    que es el valor con el que el original decide la subdivision, la niebla,
 *    el emisor y el bucket;
 *  - el numero de triangulos emitidos (valor dorado del port; el original
 *    dibuja mas, la diferencia esta en la rama de subdivision, ver
 *    S2_BSP_TRAVERSAL.md).
 * ------------------------------------------------------------------------- */
#define MAP7_PATH "C:/GEX_REZ/GEX2/level/map7.dfx"

static void test_map7_reference_emit(void) {
    static const uint32_t GOLDEN_TRIANGLES[REF_EMIT_COUNT] = {
        1123u, 1128u, 1143u, 1169u, 1154u, 1157u
    };
    gex_dfx_container c; gex_dfx_level lv;
    FILE *f = fopen(MAP7_PATH, "rb");
    if (!f) { printf("skip map7 reference (no file)\n"); return; }
    fclose(f);

    memset(&c, 0, sizeof(c)); memset(&lv, 0, sizeof(lv));
    if (gex_dfx_load(MAP7_PATH, &c) != GEX_DFX_OK) { printf("skip map7 (load)\n"); return; }
    if (gex_dfx_load_level(&c, &lv) != GEX_DFX_OK) {
        printf("skip map7 (level)\n");
        gex_dfx_release_container(&c);
        return;
    }

    for (int i = 0; i < REF_EMIT_COUNT; i++) {
        const int32_t *p = REF_EMIT_PARAMS[i];
        gex_emit_matrix m; gex_emit_fog fog; gex_emit_output out;
        gex_bsp_walk_result leaves;
        int checked = 0;

        memset(&m, 0, sizeof(m));
        memcpy(m.m, REF_EMIT_M[i], sizeof(m.m));
        m.t[0] = REF_EMIT_T[i][0]; m.t[1] = REF_EMIT_T[i][1]; m.t[2] = REF_EMIT_T[i][2];
        m.focal = p[0]; m.cx = p[1]; m.cy = p[2]; m.near_z = p[3];

        memset(&fog, 0, sizeof(fog));
        memcpy(fog.lut, REF_FOG_LUT, sizeof(fog.lut));
        fog.have_lut = 1;
        fog.fog_rgb[0] = (uint8_t)p[8];
        fog.fog_rgb[1] = (uint8_t)p[9];
        fog.fog_rgb[2] = (uint8_t)p[10];
        fog.threshold = p[4]; fog.split_z = p[5];
        fog.a_q12 = p[6]; fog.b_q12 = p[7];

        memset(&leaves, 0, sizeof(leaves));
        leaves.leaves = (uint32_t *)REF_EMIT_LEAVES[i];
        leaves.count = p[11];

        memset(&out, 0, sizeof(out));
        CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &leaves,
                             &m, &fog, 0, &out) == GEX_EMIT_OK);

        /* Las profundidades proyectadas deben coincidir bit a bit con las
         * capturadas del original (solo el frame 0 tiene profundidad
         * capturada: el limite de volcado del trazador solo alcanzo el
         * primer frame). */
        if (i == 0) {
            gex_emit_poly_list polys;
            memset(&polys, 0, sizeof(polys));
            CHECK(gex_emit_collect(c.image, c.image_size, &lv.descriptor,
                                   &leaves, &polys) == GEX_EMIT_OK);
            for (uint32_t k = 0; k < polys.count && k < REF_DEPTH_COUNT; k++) {
                const gex_emit_poly *pp = &polys.polys[k];
                int16_t vtx[9];
                gex_emit_triangle tri;
                int ok = 1;
                for (int j = 0; j < 3; j++) {
                    size_t voff = (size_t)lv.descriptor.vertex_table +
                                  (size_t)pp->vtx[j] * GEX_VERTEX_SIZE;
                    if (voff + 6u > c.image_size) { ok = 0; break; }
                    const uint8_t *vp = c.image + voff;
                    int16_t t0, t1, t2;
                    memcpy(&t0, vp, 2);
                    memcpy(&t1, vp + 2, 2);
                    memcpy(&t2, vp + 4, 2);
                    vtx[3 * j] = t0; vtx[3 * j + 1] = t1; vtx[3 * j + 2] = t2;
                }
                if (!ok) continue;
                if (gex_emit_project(&m, vtx, pp->color, pp->u, pp->v,
                                     pp->texture, pp->layer, pp->source_poly,
                                     &tri) != GEX_EMIT_OK) {
                    continue;
                }
                CHECK(tri.z[0] == REF_DEPTHS[k * 3 + 0]);
                CHECK(tri.z[1] == REF_DEPTHS[k * 3 + 1]);
                CHECK(tri.z[2] == REF_DEPTHS[k * 3 + 2]);
                checked++;
            }
            CHECK(checked == REF_DEPTH_COUNT);
            gex_emit_poly_list_free(&polys);
        }

        CHECK(out.count == GOLDEN_TRIANGLES[i]);
        printf("map7 frame %d: %u triangulos (original %u dibujos), %d profundidades comprobadas\n",
               i, (unsigned)out.count, REF_EMIT_DRAWS[i], checked);
        gex_emit_output_free(&out);
    }
    gex_dfx_release_level(&lv);
    gex_dfx_release_container(&c);
}

int main(void) {
    test_api_smoke();
    test_project_identity();
    test_project_depth_clip();
    test_project_div_sign();
    test_project_q12_wrap();
    test_determinant();
    test_classify_subdivide_priority();
    test_gate();
    test_fog_lut();
    test_fog_analytic();
    test_fog_generator();
    test_mix_color();
    test_emitter_selection();
    test_subdivide();
    test_sort();
    test_capacity_growth();
    test_collect_mini();
    test_collect_offsets();
    test_level_emit_mini();
    test_aztec_real();
    test_map7_reference_emit();

    printf("test_level_emit: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
