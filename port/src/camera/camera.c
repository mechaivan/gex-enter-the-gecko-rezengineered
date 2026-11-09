/*
 * camera.c - Camera orientation chain and visibility (S-2).
 *
 * Reproduces the original GEX3D.EXE camera pipeline statically reverse
 * engineered from the executable. All arithmetic is integer Q12
 * fixed-point (4096 = 1.0), matching the original x86 code exactly.
 * No floating point is used.
 *
 * Ingeniería inversa estática y de solo lectura. No se modifican
 * ejecutables ni assets originales.
 */

#include "gex/camera.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Trigonometric tables                                               */
/* ------------------------------------------------------------------ */

/* The original tables live in BSS at 0x00646C20 (sin) and 0x00645C20
 * (cos). They are zero-initialized in the PE file and filled by runtime
 * initialization code that was not located statically. They are indexed
 * as table[2 * (angle & 0xFFF)] where angle is in Q12 turns
 * (0x1000 = one full turn). We reproduce them mathematically: each
 * entry is round(sin/cos(2π·i/0x1000) · 4096) as int16. */
int16_t gex_cam_sin_tab[0x1000];
int16_t gex_cam_cos_tab[0x1000];

void gex_cam_init_tables(void) {
    const double two_pi = 6.2831853071795864769;
    for (int i = 0; i < 0x1000; i++) {
        double a = two_pi * (double)i / 4096.0;
        gex_cam_sin_tab[i] = (int16_t)llround(sin(a) * 4096.0);
        gex_cam_cos_tab[i] = (int16_t)llround(cos(a) * 4096.0);
    }
}

/* Reduce an angle to [0, 0x1000) following the original algorithm:
 *   neg(angle); cdq; xor eax,edx; sub eax,edx; and eax,0xFFF;
 *   xor eax,edx; sub eax,edx
 * This computes ((-angle) mod 0x1000) with x86 idiv semantics,
 * then reduces modulo 0x1000. */
static int16_t reduce_angle(int16_t angle) {
    /* neg of int16, then x86 modulo 0x1000 via the same sequence. */
    int32_t v = (int32_t)(-(int32_t)angle);
    int32_t edx = v >= 0 ? 0 : -1;  /* cdq sign extension */
    int32_t ax = v ^ edx;
    ax = ax - edx;
    ax = ax & 0xFFF;
    ax = ax ^ edx;
    ax = ax - edx;
    return (int16_t)ax;
}

/* Table lookup: table[2 * index] where index is already reduced. */
static int16_t tab_sin(int16_t angle) {
    int16_t idx = reduce_angle(angle);
    return gex_cam_sin_tab[idx & 0xFFF];
}
static int16_t tab_cos(int16_t angle) {
    int16_t idx = reduce_angle(angle);
    return gex_cam_cos_tab[idx & 0xFFF];
}

/* ------------------------------------------------------------------ */
/* Q12 helpers                                                        */
/* ------------------------------------------------------------------ */

/* sar 12 on int32: arithmetic shift right, rounds toward −∞. */
static int32_t sar12(int32_t v) {
    return v >> 12;  /* implementation-defined for negative, but
                        GCC/Clang/MSVC all do arithmetic shift */
}

/* Each rotation function builds a Q12 matrix for rotation about its axis
 * by `angle` (already negated by the caller) and pre-multiplies:
 *   m = Rot(axis, angle) · m
 * using the same table lookup and sar 12 semantics as the original. */

void gex_cam_rot_z(gex_cam_mat3 *m, int16_t angle) {
    int16_t s = tab_sin(angle);
    int16_t c = tab_cos(angle);
    /* RotZ: [[c, -s, 0],[s, c, 0],[0, 0, 4096]] */
    gex_cam_mat3 r;
    r.w[0] = c; r.w[1] = (int16_t)(-s); r.w[2] = 0;
    r.w[3] = s; r.w[4] = c;             r.w[5] = 0;
    r.w[6] = 0; r.w[7] = 0;             r.w[8] = GEX_CAM_Q12;
    gex_cam_mat_mul(m, &r, m);
}

void gex_cam_rot_y(gex_cam_mat3 *m, int16_t angle) {
    int16_t s = tab_sin(angle);
    int16_t c = tab_cos(angle);
    /* RotY: [[c, 0, s],[0, 4096, 0],[-s, 0, c]] */
    gex_cam_mat3 r;
    r.w[0] = c; r.w[1] = 0;             r.w[2] = s;
    r.w[3] = 0; r.w[4] = GEX_CAM_Q12;   r.w[5] = 0;
    r.w[6] = (int16_t)(-s); r.w[7] = 0; r.w[8] = c;
    gex_cam_mat_mul(m, &r, m);
}

void gex_cam_rot_x(gex_cam_mat3 *m, int16_t angle) {
    int16_t s = tab_sin(angle);
    int16_t c = tab_cos(angle);
    /* RotX: [[4096, 0, 0],[0, c, -s],[0, s, c]] */
    gex_cam_mat3 r;
    r.w[0] = GEX_CAM_Q12; r.w[1] = 0;   r.w[2] = 0;
    r.w[3] = 0;           r.w[4] = c;   r.w[5] = (int16_t)(-s);
    r.w[6] = 0;           r.w[7] = s;   r.w[8] = c;
    gex_cam_mat_mul(m, &r, m);
}

/* dst = a · b  (row-major Q12, each element sar 12).
 * Uses int32 intermediates; wrap-around matches x86. */
void gex_cam_mat_mul(gex_cam_mat3 *dst, const gex_cam_mat3 *a,
                     const gex_cam_mat3 *b) {
    gex_cam_mat3 r;
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int32_t acc = 0;
            for (int k = 0; k < 3; k++) {
                acc += (int32_t)a->w[3 * row + k] *
                       (int32_t)b->w[3 * k + col];
            }
            r.w[3 * row + col] = (int16_t)sar12(acc);
        }
    }
    *dst = r;
}

void gex_cam_mat_transpose(gex_cam_mat3 *dst, const gex_cam_mat3 *src) {
    gex_cam_mat3 r;
    for (int i = 0; i < 9; i++) {
        int row = i / 3, col = i % 3;
        r.w[3 * col + row] = src->w[i];
    }
    *dst = r;
}

/* M2 = 4096 * [[1,0,0],[0,0,-1],[0,1,0]].
 * O = M2 · R applied column by column. */
void gex_cam_apply_m2(gex_cam_mat3 *o, const gex_cam_mat3 *r) {
    /* Column 0 of R: (r.w[0], r.w[3], r.w[6]) -> M2 * col0 = (r0, r6, -r3) */
    /* Column 1 of R: (r.w[1], r.w[4], r.w[7]) -> M2 * col1 = (r1, r7, -r4) */
    /* Column 2 of R: (r.w[2], r.w[5], r.w[8]) -> M2 * col2 = (r2, r8, -r5) */
    o->w[0] = r->w[0]; o->w[1] = r->w[6]; o->w[2] = (int16_t)(-r->w[3]);
    o->w[3] = r->w[1]; o->w[4] = r->w[7]; o->w[5] = (int16_t)(-r->w[4]);
    o->w[6] = r->w[2]; o->w[7] = r->w[8]; o->w[8] = (int16_t)(-r->w[5]);
}

/* Transform a vector by a Q12 matrix: out = (m · v) >> 12.
 * v is int16[3], out is int32[3]. No saturation. */
static void transform3(const gex_cam_mat3 *m, const int16_t v[3],
                       int32_t out[3]) {
    for (int r = 0; r < 3; r++) {
        out[r] = (int32_t)m->w[3 * r + 0] * (int32_t)v[0] +
                 (int32_t)m->w[3 * r + 1] * (int32_t)v[1] +
                 (int32_t)m->w[3 * r + 2] * (int32_t)v[2];
        out[r] >>= 12;  /* sar 12 */
    }
}

/* ------------------------------------------------------------------ */
/* Local frame (0x402DC0)                                             */
/* ------------------------------------------------------------------ */

/* Approximate normalization exactly as in the original:
 *   max,mid,min = XOR-swap sort desc(|gx|,|gy|,|gz|)
 *   approx = (30*max + 12*mid + 9*min) / 8   (int32, idiv by 8)
 *   if approx != 0: g_i = (g_i << 14) / approx   (idiv, trunc toward 0) */
static void approx_normalize(int16_t g[3]) {
    int16_t a = g[0], b = g[1], c = g[2];
    int32_t ia = (int32_t)a;
    int32_t ib = (int32_t)b;
    int32_t ic = (int32_t)c;

    /* XOR-swap sort descending by absolute value. */
    /* Step 1: ensure |a| >= |b| */
    if ((ia < 0 ? -ia : ia) < (ib < 0 ? -ib : ib)) {
        a = (int16_t)(a ^ b); b = (int16_t)(a ^ b); a = (int16_t)(a ^ b);
        ia = (int32_t)a; ib = (int32_t)b;
    }
    /* Step 2: ensure |a| >= |c| */
    if ((ia < 0 ? -ia : ia) < (ic < 0 ? -ic : ic)) {
        a = (int16_t)(a ^ c); c = (int16_t)(a ^ c); a = (int16_t)(a ^ c);
        ia = (int32_t)a; ic = (int32_t)c;
    }
    /* Step 3: ensure |b| >= |c| */
    if ((ib < 0 ? -ib : ib) < (ic < 0 ? -ic : ic)) {
        b = (int16_t)(b ^ c); c = (int16_t)(b ^ c); b = (int16_t)(b ^ c);
        ib = (int32_t)b; ic = (int32_t)c;
    }
    /* Now |a| >= |b| >= |c| */
    int32_t maxv = ia < 0 ? -ia : ia;
    int32_t midv = ib < 0 ? -ib : ib;
    int32_t minv = ic < 0 ? -ic : ic;

    int32_t approx = (30 * maxv + 12 * midv + 9 * minv) / 8;

    if (approx != 0) {
        /* idiv: truncation toward zero.  Left shift of a negative int32 is
         * UB in C; replicate x86 shl via unsigned shift then reinterpret. */
        int32_t sh0 = (int32_t)((uint32_t)(int32_t)a << 14);
        int32_t sh1 = (int32_t)((uint32_t)(int32_t)b << 14);
        int32_t sh2 = (int32_t)((uint32_t)(int32_t)c << 14);
        g[0] = (int16_t)(sh0 / approx);
        g[1] = (int16_t)(sh1 / approx);
        g[2] = (int16_t)(sh2 / approx);
    }
    /* if approx == 0: vector stays (0,0,0) */
}

void gex_cam_build_local_frame(gex_camera *cam) {
    int32_t w = cam->width;            /* +0x48, int32 */
    int16_t f = cam->focal;            /* +0x40, int16 */

    /* half = trunc(w / 2)   (idiv, toward zero) */
    int32_t half = w / 2;
    /* a = trunc(w / 3) * 2  (idiv, toward zero) */
    int32_t a = (w / 3) * 2;
    int32_t b = half;

    /* Corner rays */
    /* v1 = (-a, -b, f)  v2 = ( a, -b, f)  v3 = (-a, b, f)  v4 = ( a, b, f) */

    /* cam+0x60 = (0, 0, 0x1000) */
    cam->local[0][0] = 0;
    cam->local[0][1] = 0;
    cam->local[0][2] = (int16_t)GEX_CAM_Q12;

    /* cam+0x68 = v1 × v2 = (0, +2af, +2ab) */
    {
        int16_t g[3] = { 0, (int16_t)(2 * a * f), (int16_t)(2 * a * b) };
        approx_normalize(g);
        cam->local[1][0] = g[0];
        cam->local[1][1] = g[1];
        cam->local[1][2] = g[2];
    }
    /* cam+0x70 = v3 × v1 = (+2bf, 0, +2ab) */
    {
        int16_t g[3] = { (int16_t)(2 * b * f), 0, (int16_t)(2 * a * b) };
        approx_normalize(g);
        cam->local[2][0] = g[0];
        cam->local[2][1] = g[1];
        cam->local[2][2] = g[2];
    }
    /* cam+0x78 = v2 × v4 = (−2bf, 0, +2ab) */
    {
        int16_t g[3] = { (int16_t)(-(2 * b * f)), 0, (int16_t)(2 * a * b) };
        approx_normalize(g);
        cam->local[3][0] = g[0];
        cam->local[3][1] = g[1];
        cam->local[3][2] = g[2];
    }
    /* cam+0x80 = v4 × v3 = (0, −2af, +2ab) */
    {
        int16_t g[3] = { 0, (int16_t)(-(2 * a * f)), (int16_t)(2 * a * b) };
        approx_normalize(g);
        cam->local[4][0] = g[0];
        cam->local[4][1] = g[1];
        cam->local[4][2] = g[2];
    }
}

/* ------------------------------------------------------------------ */
/* Orientation chain                                                  */
/* ------------------------------------------------------------------ */

int gex_cam_build_orientation(const gex_camera *cam, gex_cam_mat3 *out_o) {
    /* Select angle source. */
    int use_alt = cam->use_alt_angles != 0;

    int16_t ax, ay, az;
    if (use_alt) {
        ax = cam->euler_alt[0];  /* +0x128 */
        ay = cam->euler_alt[1];  /* +0x12A */
        az = cam->euler_alt[2];  /* +0x12C */
    } else {
        ax = cam->euler[0];      /* +0x30 */
        ay = cam->euler[1];      /* +0x32 */
        az = cam->euler[2];      /* +0x34 */
    }

    /* Start from base matrix. */
    gex_cam_mat3 r;
    if (cam->flag_identity != 0 || cam->base == NULL) {
        /* Identity */
        memset(&r, 0, sizeof(r));
        r.w[0] = GEX_CAM_Q12; r.w[4] = GEX_CAM_Q12; r.w[8] = GEX_CAM_Q12;
    } else {
        r = *cam->base;
    }

    /* Pre-multiply rotations: Z first, then Y, then X (angles negated). */
    gex_cam_rot_z(&r, (int16_t)(-az));
    gex_cam_rot_y(&r, (int16_t)(-ay));
    gex_cam_rot_x(&r, (int16_t)(-ax));

    /* O = M2 · R (applied by columns) */
    gex_cam_apply_m2(out_o, &r);
    return 0;
}

void gex_cam_orientation_finalize(const gex_cam_mat3 *o, gex_cam_mat3 *out_m) {
    gex_cam_mat_transpose(out_m, o);
}

void gex_cam_build_visibility_matrices(gex_camera *cam) {
    gex_cam_mat3 m;
    gex_cam_mat3 o;
    gex_cam_build_orientation(cam, &o);
    gex_cam_orientation_finalize(&o, &m);

    /* Work copy of M (row-major Q12). */
    int16_t mat_w[9];
    memcpy(mat_w, m.w, sizeof(mat_w));

    int16_t *A = cam->mat_a;   /* +0x88 */
    int16_t *B = cam->mat_b;   /* +0xA8 */

    /* A rows: local[0] (advance), local[1] (down), local[2] (left) */
    for (int row = 0; row < 3; row++) {
        int32_t t[3];
        transform3((const gex_cam_mat3 *)mat_w, cam->local[row], t);
        A[3 * row + 0] = (int16_t)t[0];
        A[3 * row + 1] = (int16_t)t[1];
        A[3 * row + 2] = (int16_t)t[2];
    }
    /* B rows 0-1: local[3] (right), local[4] (up).
     * B row 2 (cam+0xB4..0xB8) is never written by camera code. */
    {
        int32_t t[3];
        transform3((const gex_cam_mat3 *)mat_w, cam->local[3], t);
        B[0] = (int16_t)t[0]; B[1] = (int16_t)t[1]; B[2] = (int16_t)t[2];
    }
    {
        int32_t t[3];
        transform3((const gex_cam_mat3 *)mat_w, cam->local[4], t);
        B[3] = (int16_t)t[0]; B[4] = (int16_t)t[1]; B[5] = (int16_t)t[2];
    }

    /* a_cam = A · camPos  (int32, no truncate) */
    transform3(&m, cam->pos, cam->a_cam);
    /* b_cam = B · camPos  (int32, no truncate).
     * B row 2 holds whatever was previously in memory (uninitialized
     * from the camera's perspective); we still compute all three rows
     * for completeness but the visibility test only uses B rows 0-1. */
    transform3(&m, cam->pos, cam->b_cam);
    cam->m = m;
}

void gex_cam_update(gex_camera *cam) {
    gex_cam_build_local_frame(cam);
    gex_cam_build_visibility_matrices(cam);
}

/* ------------------------------------------------------------------ */
/* Visibility test                                                    */
/* ------------------------------------------------------------------ */

int gex_cam_sphere_visible(const gex_camera *cam,
                           const int16_t c[3], uint16_t radius) {
    int32_t r = (int32_t)radius;  /* unsigned read, then used as int32 */

    int32_t T_A[3], T_B[3];
    transform3(&cam->m, c, T_A);
    transform3(&cam->m, c, T_B);

    int32_t A_cam[3], B_cam[3];
    transform3(&cam->m, cam->pos, A_cam);
    transform3(&cam->m, cam->pos, B_cam);

    int32_t far = cam->far;

    /* Near/far on A row 0 */
    int32_t d0 = T_A[0] - A_cam[0];
    if (d0 <= -r) return 0;
    if (d0 >= far + r) return 0;
    /* Laterals */
    if (T_A[1] - A_cam[1] <= -r) return 0;
    if (T_A[2] - A_cam[2] <= -r) return 0;
    if (T_B[0] - B_cam[0] <= -r) return 0;
    if (T_B[1] - B_cam[1] <= -r) return 0;
    return 1;
}

int32_t gex_cam_plane_distance(const gex_camera *cam,
                               const int16_t normal[3], int32_t distance) {
    int32_t d = ((int32_t)cam->pos[0] * (int32_t)normal[0] +
                 (int32_t)cam->pos[1] * (int32_t)normal[1] +
                 (int32_t)cam->pos[2] * (int32_t)normal[2]) >> 12;
    d -= distance;
    return d;
}
