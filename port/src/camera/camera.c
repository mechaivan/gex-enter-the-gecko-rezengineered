/*
 * camera.c - Camera orientation chain and visibility (S-2).
 *
 * Reproduces the original GEX3D.EXE camera pipeline statically reverse
 * engineered from the executable. The arithmetic is integer Q12
 * fixed-point (4096 = 1.0) with the same operation order, sign handling
 * and `sar 12` steps as the original x86 code.
 *
 * The original sine/cosine tables live in BSS at 0x00646C20 (sin) and
 * 0x00645C20 (cos). Their point values were not recovered statically, so
 * the port generates them mathematically in Q12. That generation was
 * VERIFIED against the running original on 2026-11-11: the retail table
 * truncates instead of rounding, and reproduces trunc(4096*sin(2*pi*i/4096))
 * on all 1024 live entries (see S2_CAMERA_VISIBILITY.md).
 *
 * Ingeniería inversa estática y de solo lectura. No se modifican
 * ejecutables ni assets originales.
 */

#include "gex/camera.h"

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Trigonometric tables                                               */
/* ------------------------------------------------------------------ */

/* Full-turn Q12 tables, indexed by angle & 0xFFF. The original keeps a
 * half-turn sine table plus quadrant reduction; the port stores the
 * mathematically generated values for the whole turn and applies the
 * same quadrant identities on the first half (see tab_sin/tab_cos). */
int16_t gex_cam_sin_tab[0x1000];
int16_t gex_cam_cos_tab[0x1000];

static int g_tables_ready = 0;

/* Retail table values are TRUNCATED, not rounded: the table dumped from
 * the running original (0x00646C20, half-turn sine) matches
 * trunc(4096*sin(2*pi*i/4096)) on all 1024 live entries and differs from
 * the rounded values on 502 of them. The port therefore truncates. */
void gex_cam_init_tables(void) {
    const double two_pi = 6.2831853071795864769;
    for (int i = 0; i < 0x1000; i++) {
        double a = two_pi * (double)i / 4096.0;
        gex_cam_sin_tab[i] = (int16_t)(sin(a) * 4096.0);
        gex_cam_cos_tab[i] = (int16_t)(cos(a) * 4096.0);
    }
    g_tables_ready = 1;
}

/* Reduce an angle to [0, 0x1000) using the same idiv-by-0x1000
 * remainder sequence as the original:
 *   cdq; xor eax,edx; sub eax,edx; and eax,0xFFF;
 *   xor eax,edx; sub eax,edx
 * The sequence yields the signed remainder (C truncated semantics); the
 * final mask folds it into the non-negative range.
 */
static int32_t norm_angle(int16_t angle) {
    int32_t v = (int32_t)angle;      /* movsx */
    /* cdq: all-ones for negatives. Written as a comparison because >>
     * of a negative value is implementation-defined in C. */
    int32_t edx = (v < 0) ? -1 : 0;
    int32_t ax = v ^ edx;
    ax -= edx;
    ax &= 0xFFF;
    ax ^= edx;
    ax -= edx;
    return ax & 0xFFF;
}

/* sin(theta) from the half-turn sine table (indices <= 0x400).
 * Quadrants as in the original:
 *   [0x000,0x400)  +S[a]
 *   [0x400,0x800)  +S[0x800-a]
 *   [0x800,0xC00)  -S[a-0x800]
 *   [0xC00,0x1000) -S[0x1000-a]
 * where S[i] = round(4096*sin(2*pi*i/0x1000)).
 */
static int16_t tab_sin(int16_t angle) {
    int32_t a = norm_angle(angle);
    if (a < 0x400) return gex_cam_sin_tab[a];
    if (a < 0x800) return gex_cam_sin_tab[0x800 - a];
    if (a < 0xC00) return (int16_t)(-gex_cam_sin_tab[a - 0x800]);
    return (int16_t)(-gex_cam_sin_tab[0x1000 - a]);
}

/* cos(theta) = sin(theta + 0x400), expressed on the same half-turn
 * table (this is the identity the original single-table lookup relies
 * on). Each band stays within indices 0..0x400:
 *   [0x000,0x400)  +S[0x400-a]
 *   [0x400,0x800)  -S[a-0x400]
 *   [0x800,0xC00)  -S[0xC00-a]
 *   [0xC00,0x1000) +S[a-0xC00]
 */
static int16_t tab_cos(int16_t angle) {
    int32_t a = norm_angle(angle);
    if (a < 0x400) return gex_cam_sin_tab[0x400 - a];
    if (a < 0x800) return (int16_t)(-gex_cam_sin_tab[a - 0x400]);
    if (a < 0xC00) return (int16_t)(-gex_cam_sin_tab[0xC00 - a]);
    return gex_cam_sin_tab[a - 0xC00];
}

/* ------------------------------------------------------------------ */
/* Q12 helpers                                                        */
/* ------------------------------------------------------------------ */

/* Bit-level reinterpretation helpers.
 *
 * Every 32-bit value in the original lives in a register, so "signed" and
 * "unsigned" are only a matter of how the next instruction reads the bits.
 * Going through memcpy keeps the port free of both signed overflow and
 * implementation-defined integer conversions: the bits are copied
 * verbatim. (uint32_t and int32_t have the same size on every supported
 * target; the memcpy is the standard bit-cast idiom.) */
static int32_t bits_i32(uint32_t u) {
    int32_t v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

static int16_t bits_i16(uint16_t u) {
    int16_t v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

/* Truncate an int32 to int16 the way a `mov word ptr` store does: keep the
 * low 16 bits, no saturation. */
static int16_t low_i16(int32_t v) {
    return bits_i16((uint16_t)((uint32_t)v & 0xFFFFu));
}

/* 16-bit `neg` with wrap (e.g. -(-32768) stays -32768). */
static int16_t neg_i16(int16_t v) {
    return low_i16(-(int32_t)v);
}

/* add/sub with x86 32-bit wrap-around. */
static int32_t add_wrap(int32_t a, int32_t b) {
    return bits_i32((uint32_t)a + (uint32_t)b);
}

static int32_t sub_wrap(int32_t a, int32_t b) {
    return bits_i32((uint32_t)a - (uint32_t)b);
}

/* sar 12 on a 32-bit register: arithmetic shift right, rounding toward
 * -inf. Implemented on the unsigned bits so the result does not depend on
 * the implementation-defined behaviour of >> on negative ints. */
static int32_t sar12(uint32_t bits) {
    uint32_t s = bits >> 12;
    if (bits & 0x80000000u) s |= 0xFFF00000u;
    return bits_i32(s);
}

/* Each rotation function builds a Q12 matrix for a rotation about its
 * axis by `angle` (raw Q12 turns, not pre-negated) and pre-multiplies:
 *   m = Rot(axis, angle) * m
 * using the same table lookup and sar 12 semantics as the original. */

void gex_cam_rot_z(gex_cam_mat3 *m, int16_t angle) {
    int16_t s = tab_sin(angle);
    int16_t c = tab_cos(angle);
    /* RotZ: [[c, -s, 0],[s, c, 0],[0, 0, 4096]] */
    gex_cam_mat3 r;
    r.w[0] = c;                       r.w[1] = neg_i16(s);    r.w[2] = 0;
    r.w[3] = s;                       r.w[4] = c;             r.w[5] = 0;
    r.w[6] = 0;                       r.w[7] = 0;             r.w[8] = GEX_CAM_Q12;
    gex_cam_mat_mul(m, &r, m);
}

void gex_cam_rot_y(gex_cam_mat3 *m, int16_t angle) {
    int16_t s = tab_sin(angle);
    int16_t c = tab_cos(angle);
    /* RotY: [[c, 0, s],[0, 4096, 0],[-s, 0, c]] */
    gex_cam_mat3 r;
    r.w[0] = c;                       r.w[1] = 0;             r.w[2] = s;
    r.w[3] = 0;                       r.w[4] = GEX_CAM_Q12;   r.w[5] = 0;
    r.w[6] = neg_i16(s);              r.w[7] = 0;             r.w[8] = c;
    gex_cam_mat_mul(m, &r, m);
}

void gex_cam_rot_x(gex_cam_mat3 *m, int16_t angle) {
    int16_t s = tab_sin(angle);
    int16_t c = tab_cos(angle);
    /* RotX: [[4096, 0, 0],[0, c, -s],[0, s, c]] */
    gex_cam_mat3 r;
    r.w[0] = GEX_CAM_Q12; r.w[1] = 0;  r.w[2] = 0;
    r.w[3] = 0;           r.w[4] = c;  r.w[5] = neg_i16(s);
    r.w[6] = 0;           r.w[7] = s;  r.w[8] = c;
    gex_cam_mat_mul(m, &r, m);
}

/* dst = a * b  (row-major Q12, each element sar 12).
 * 32-bit wrap-around matches x86; the accumulator is unsigned so the
 * intermediate additions are well defined. */
void gex_cam_mat_mul(gex_cam_mat3 *dst, const gex_cam_mat3 *a,
                     const gex_cam_mat3 *b) {
    gex_cam_mat3 r;
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            uint32_t acc = 0;
            for (int k = 0; k < 3; k++) {
                acc += (uint32_t)((int32_t)a->w[3 * row + k] *
                                  (int32_t)b->w[3 * k + col]);
            }
            r.w[3 * row + col] = low_i16(sar12(acc));
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
 *
 * The original's working matrix W (0x46DD40) starts as the base matrix
 * and the rotators post-multiply it, so W ends up as the transpose of
 * the port's rotation product R. 0x46E038..0x46E11D copies one ROW of W
 * into the scratch vector at 0x006474E0, runs it through the
 * matrix/vector helper 0x004698B0 with M2 at 0x00646BE0, and writes the
 * three results into the destination COLUMN. With W = transpose(R) that
 * is O = M2 * R, i.e. M2 is applied to the COLUMNS of R:
 *
 *   O[r][c] = (M2 * column_c(R))[r] = (R[0][c], -R[2][c], R[1][c])
 *
 * The earlier port code applied M2 to the rows instead, which is a
 * different matrix: it produced a visibly wrong M (see
 * S2_CAMERA_VISIBILITY.md). */
void gex_cam_apply_m2(gex_cam_mat3 *o, const gex_cam_mat3 *r) {
    /* O[0][c] = R[0][c] */
    o->w[0] = r->w[0];  o->w[1] = r->w[1];  o->w[2] = r->w[2];
    /* O[1][c] = -R[2][c] */
    o->w[3] = neg_i16(r->w[6]);  o->w[4] = neg_i16(r->w[7]);  o->w[5] = neg_i16(r->w[8]);
    /* O[2][c] = R[1][c] */
    o->w[6] = r->w[3];  o->w[7] = r->w[4];  o->w[8] = r->w[5];
}

/* Transform a vector by a Q12 matrix: out = (m * v) >> 12.
 * v is int16[3], out is int32[3]. The products are exact in int32
 * (|a*b| <= 32768*32767); the three-term sum is accumulated modulo 2^32
 * and the shift is applied to the resulting register, exactly like the
 * original imul/add/sar sequence (which can overflow the signed range). */
static void transform3(const gex_cam_mat3 *m, const int16_t v[3],
                       int32_t out[3]) {
    for (int r = 0; r < 3; r++) {
        uint32_t acc = (uint32_t)((int32_t)m->w[3 * r + 0] * (int32_t)v[0]);
        acc += (uint32_t)((int32_t)m->w[3 * r + 1] * (int32_t)v[1]);
        acc += (uint32_t)((int32_t)m->w[3 * r + 2] * (int32_t)v[2]);
        out[r] = sar12(acc);
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
    const int32_t g0 = (int32_t)g[0];
    const int32_t g1 = (int32_t)g[1];
    const int32_t g2 = (int32_t)g[2];
    int32_t av0 = g0 < 0 ? -g0 : g0;
    int32_t av1 = g1 < 0 ? -g1 : g1;
    int32_t av2 = g2 < 0 ? -g2 : g2;

    /* XOR-swap sort descending by absolute value. The signed components
     * keep their original positions; only the three magnitudes used to
     * build `approx` are reordered, exactly as the original does. */
    if (av0 < av1) { int32_t t = av0; av0 = av1; av1 = t; }
    if (av0 < av2) { int32_t t = av0; av0 = av2; av2 = t; }
    if (av1 < av2) { int32_t t = av1; av1 = av2; av2 = t; }

    int32_t maxv = av0;
    int32_t midv = av1;
    int32_t minv = av2;

    int32_t approx = (30 * maxv + 12 * midv + 9 * minv) / 8;

    if (approx != 0) {
        /* idiv: truncation toward zero. The `shl` of a negative value is
         * done on the unsigned bits and reinterpreted, as x86 does. */
        int32_t sh0 = bits_i32((uint32_t)g0 << 14);
        int32_t sh1 = bits_i32((uint32_t)g1 << 14);
        int32_t sh2 = bits_i32((uint32_t)g2 << 14);
        g[0] = low_i16(sh0 / approx);
        g[1] = low_i16(sh1 / approx);
        g[2] = low_i16(sh2 / approx);
    }
    /* if approx == 0: vector stays (0,0,0) */
}

/* Cross product of two int16 corner rays, then sar 12 and truncation to
 * int16, exactly like the original (two imul, sub, sar 0xC, word store). */
static void cross_shift(const int16_t u[3], const int16_t v[3], int16_t out[3]) {
    uint32_t g0 = (uint32_t)((int32_t)u[1] * (int32_t)v[2]) -
                  (uint32_t)((int32_t)u[2] * (int32_t)v[1]);
    uint32_t g1 = (uint32_t)((int32_t)u[2] * (int32_t)v[0]) -
                  (uint32_t)((int32_t)u[0] * (int32_t)v[2]);
    uint32_t g2 = (uint32_t)((int32_t)u[0] * (int32_t)v[1]) -
                  (uint32_t)((int32_t)u[1] * (int32_t)v[0]);
    out[0] = low_i16(sar12(g0));
    out[1] = low_i16(sar12(g1));
    out[2] = low_i16(sar12(g2));
}

void gex_cam_build_local_frame(gex_camera *cam) {
    int32_t w = cam->width;            /* +0x48, int32 */
    int16_t f = cam->focal;            /* +0x40, int16 */

    /* half = trunc(w / 2)   (idiv, toward zero) */
    int16_t b = low_i16(w / 2);
    /* a = trunc(w / 3) * 2  (idiv, toward zero) */
    int16_t a = low_i16((w / 3) * 2);

    /* Corner rays:
     *   v1 = (-a, -b, f)   v2 = ( a, -b, f)
     *   v3 = (-a,  b, f)   v4 = ( a,  b, f) */
    const int16_t v1[3] = { neg_i16(a), neg_i16(b), f };
    const int16_t v2[3] = { a,          neg_i16(b), f };
    const int16_t v3[3] = { neg_i16(a), b,          f };
    const int16_t v4[3] = { a,          b,          f };

    /* cam+0x60 = (0, 0, 0x1000): advance axis. */
    cam->local[0][0] = 0;
    cam->local[0][1] = 0;
    cam->local[0][2] = GEX_CAM_Q12;

    /* cam+0x68 = v1 x v2 = (0, +2af, +2ab) / 4096 */
    cross_shift(v1, v2, cam->local[1]);
    approx_normalize(cam->local[1]);

    /* cam+0x70 = v3 x v1 = (+2bf, 0, +2ab) / 4096 */
    cross_shift(v3, v1, cam->local[2]);
    approx_normalize(cam->local[2]);

    /* cam+0x78 = v2 x v4 = (-2bf, 0, +2ab) / 4096 */
    cross_shift(v2, v4, cam->local[3]);
    approx_normalize(cam->local[3]);

    /* cam+0x80 = v4 x v3 = (0, -2af, +2ab) / 4096 */
    cross_shift(v4, v3, cam->local[4]);
    approx_normalize(cam->local[4]);
}

/* ------------------------------------------------------------------ */
/* Orientation chain                                                  */
/* ------------------------------------------------------------------ */

int gex_cam_build_orientation(const gex_camera *cam, gex_cam_mat3 *out_o) {
    if (!g_tables_ready) {
        /* Port convenience: the original tables are filled before use at
         * runtime. Keep the same guarantee without a hard dependency on
         * the caller. */
        gex_cam_init_tables();
    }

    /* Select angle source.
     *
     * The original negates the Euler angles before handing them to the
     * rotation helpers: 0x46DE51/0x46DE67/0x46DE7D do
     * `movswl 0x34(%edx), %eax; negl %eax` (and the same for +0x32 and
     * +0x30) before calling the Z, Y and X rotators, and the alternative
     * branch at 0x46DECA/0x46DEE3/0x46DEFC negates +0x12C/+0x12A/+0x128
     * the same way. The rotators then index the sine/cosine tables with
     * the negated angle, so the net effect is a rotation by -euler.
     * Captured from the running original (frame 1 of AZTEC16): with
     * euler = (-397, 0, 2802) the resulting M is
     * {{-1648,-2144,3074},{-3749,943,-1352},{0,-3359,-2343}}, which the
     * port reproduces bit for bit only with the negated angles. */
    int use_alt = cam->use_alt_angles != 0;

    int16_t ax, ay, az;
    if (use_alt) {
        ax = neg_i16(cam->euler_alt[0]);  /* +0x128 */
        ay = neg_i16(cam->euler_alt[1]);  /* +0x12A */
        az = neg_i16(cam->euler_alt[2]);  /* +0x12C */
    } else {
        ax = neg_i16(cam->euler[0]);      /* +0x30 */
        ay = neg_i16(cam->euler[1]);      /* +0x32 */
        az = neg_i16(cam->euler[2]);      /* +0x34 */
    }

    /* Start from base matrix. */
    gex_cam_mat3 r;
    if (cam->flag_identity != 0 || cam->base == NULL) {
        /* Identity.  The original dereferences a NULL base; the port
         * guards that case as a documented safety decision. */
        memset(&r, 0, sizeof(r));
        r.w[0] = GEX_CAM_Q12; r.w[4] = GEX_CAM_Q12; r.w[8] = GEX_CAM_Q12;
    } else {
        r = *cam->base;
    }

    /* Pre-multiply rotations: R = RotX(x) * RotY(y) * RotZ(z) * base.
     * The original builder calls Z, then Y, then X with the raw angles. */
    gex_cam_rot_z(&r, az);
    gex_cam_rot_y(&r, ay);
    gex_cam_rot_x(&r, ax);

    /* O = M2 * R (applied by columns) */
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

    int16_t *A = cam->mat_a;   /* +0x88 */
    int16_t *B = cam->mat_b;   /* +0xA8 */

    /* A rows: local[0] (advance), local[1] (down), local[2] (left). */
    for (int row = 0; row < 3; row++) {
        int32_t t[3];
        transform3(&m, cam->local[row], t);
        A[3 * row + 0] = low_i16(t[0]);
        A[3 * row + 1] = low_i16(t[1]);
        A[3 * row + 2] = low_i16(t[2]);
    }
    /* B rows 0-1: local[3] (right), local[4] (up).
     * B row 2 (+0xB4..0xB8) is NOT written here: the original reads it
     * but the writer was not located. The port preserves whatever the
     * caller left in mat_b[6..8] and documents that as a port decision. */
    {
        int32_t t[3];
        transform3(&m, cam->local[3], t);
        B[0] = low_i16(t[0]); B[1] = low_i16(t[1]); B[2] = low_i16(t[2]);
    }
    {
        int32_t t[3];
        transform3(&m, cam->local[4], t);
        B[3] = low_i16(t[0]); B[4] = low_i16(t[1]); B[5] = low_i16(t[2]);
    }

    /* a_cam = A * camPos; b_cam = B * camPos (all nine B entries,
     * including the preserved row 2). Both stay int32. */
    {
        gex_cam_mat3 a_mat;
        memcpy(a_mat.w, A, sizeof(a_mat.w));
        transform3(&a_mat, cam->pos, cam->a_cam);
    }
    {
        gex_cam_mat3 b_mat;
        memcpy(b_mat.w, B, sizeof(b_mat.w));
        transform3(&b_mat, cam->pos, cam->b_cam);
    }
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

    gex_cam_mat3 a_mat, b_mat;
    memcpy(a_mat.w, cam->mat_a, sizeof(a_mat.w));
    memcpy(b_mat.w, cam->mat_b, sizeof(b_mat.w));

    int32_t T_A[3], T_B[3];
    transform3(&a_mat, c, T_A);
    transform3(&b_mat, c, T_B);

    /* x86 `neg`/`add`/`sub` wrap at 32 bits. a_cam/b_cam are caller data
     * and `far` comes from the camera, so none of these differences may
     * rely on signed overflow not happening. */
    const int32_t neg_r = sub_wrap(0, r);
    const int32_t far_plus_r = add_wrap(cam->far, r);

    int32_t d0 = sub_wrap(T_A[0], cam->a_cam[0]);
    if (d0 <= neg_r) return 0;
    if (d0 >= far_plus_r) return 0;
    /* Lateral semiplanos: A rows 1-2, B rows 0-1. */
    if (sub_wrap(T_A[1], cam->a_cam[1]) <= neg_r) return 0;
    if (sub_wrap(T_A[2], cam->a_cam[2]) <= neg_r) return 0;
    if (sub_wrap(T_B[0], cam->b_cam[0]) <= neg_r) return 0;
    if (sub_wrap(T_B[1], cam->b_cam[1]) <= neg_r) return 0;
    return 1;
}

/* Plane test for child ordering (0x00401C00):
 *
 *   acc = pos.x*N.x + pos.y*N.y + pos.z*N.z     (three imul + add, 32-bit)
 *   d   = sar(acc, 12)                          (arithmetic shift)
 *   d   = d - node_distance                     (32-bit sub)
 *
 * Each product is exact in int32 (|pos|,|N| <= 32768), but the three-term
 * sum overflows int32 for large coordinates: the original register wraps.
 * The accumulator is therefore uint32_t, the shift is applied to the
 * register bits and the subtraction wraps too. No signed overflow and no
 * implementation-defined conversion is involved. */
int32_t gex_cam_plane_distance(const gex_camera *cam,
                               const int16_t normal[3], int32_t distance) {
    uint32_t acc = 0;
    for (int i = 0; i < 3; i++) {
        acc += (uint32_t)((int32_t)cam->pos[i] * (int32_t)normal[i]);
    }
    int32_t d = sar12(acc);
    return sub_wrap(d, distance);
}
