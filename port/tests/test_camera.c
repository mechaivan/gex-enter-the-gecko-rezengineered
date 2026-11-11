/*
 * test_camera.c - Tests for camera orientation chain and visibility (S-2).
 *
 * Covers: Q12 table values, exact rotation matrices per axis (including
 * negative and wrapped angles), Z->Y->X pre-multiplication order,
 * normal/alternative angle branches, base matrix and forced identity,
 * exact local-frame values with sar 12, A/B wiring and B row 2
 * preservation, a_cam/b_cam offsets, all six semiplane boundaries,
 * unsigned radius, plane test, leaf predicate, and BSP traversal.
 *
 * No dynamic comparison with the original executable: these are
 * deterministic mathematical tests of the ported algorithms.
 */

#include "gex/camera.h"
#include "gex/bsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if(!(cond)){ failures++; \
    printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);} } while(0)

static gex_cam_mat3 mat_identity(void) {
    gex_cam_mat3 m;
    memset(&m, 0, sizeof(m));
    m.w[0] = GEX_CAM_Q12; m.w[4] = GEX_CAM_Q12; m.w[8] = GEX_CAM_Q12;
    return m;
}

static gex_camera cam_default(void) {
    gex_camera c;
    memset(&c, 0, sizeof(c));
    c.focal = 0x140;
    c.field_0x44 = 0x200;
    c.width = 0x180;
    c.far = 0x2EE0;
    return c;
}

/* Same transform the implementation uses: out = (m * v) >> 12. */
static void xform(const gex_cam_mat3 *m, const int16_t v[3], int32_t out[3]) {
    for (int r = 0; r < 3; r++) {
        uint32_t acc = (uint32_t)((int32_t)m->w[3 * r + 0] * (int32_t)v[0]);
        acc += (uint32_t)((int32_t)m->w[3 * r + 1] * (int32_t)v[1]);
        acc += (uint32_t)((int32_t)m->w[3 * r + 2] * (int32_t)v[2]);
        out[r] = (int32_t)acc >> 12;
    }
}

static void mat_diag(int16_t m[9], int16_t x, int16_t y, int16_t z) {
    memset(m, 0, 9 * sizeof(int16_t));
    m[0] = x; m[4] = y; m[8] = z;
}

static void test_tables_init(void) {
    gex_cam_init_tables();
    CHECK(gex_cam_sin_tab[0] == 0);
    CHECK(gex_cam_cos_tab[0] == 4096);
    CHECK(gex_cam_sin_tab[0x400] == 4096);
    CHECK(gex_cam_cos_tab[0x400] == 0);
    CHECK(gex_cam_sin_tab[0x800] == 0);
    CHECK(gex_cam_cos_tab[0x800] == -4096);
    CHECK(gex_cam_sin_tab[0xC00] == -4096);
    CHECK(gex_cam_cos_tab[0xC00] == 0);
    /* 45 degrees: 4096 * sin(pi/4) = 2896.3 -> 2896 */
    CHECK(gex_cam_sin_tab[0x200] == 2896);
    CHECK(gex_cam_cos_tab[0x200] == 2896);
    /* Odd symmetry: sin(2*pi - x) = -sin(x). */
    CHECK(gex_cam_sin_tab[0xFFF] == -gex_cam_sin_tab[1]);
}

/* Exact rotation matrices built directly by the pre-multiply helpers. */
static void test_rotation_exact(void) {
    gex_cam_mat3 m;

    m = mat_identity();
    gex_cam_rot_z(&m, 0x400);  /* +90 degrees */
    CHECK(m.w[0] == 0 && m.w[1] == -4096 && m.w[2] == 0);
    CHECK(m.w[3] == 4096 && m.w[4] == 0 && m.w[5] == 0);
    CHECK(m.w[6] == 0 && m.w[7] == 0 && m.w[8] == 4096);

    m = mat_identity();
    gex_cam_rot_z(&m, (int16_t)-0x400);  /* -90 degrees */
    CHECK(m.w[0] == 0 && m.w[1] == 4096 && m.w[2] == 0);
    CHECK(m.w[3] == -4096 && m.w[4] == 0 && m.w[5] == 0);

    m = mat_identity();
    gex_cam_rot_y(&m, 0x400);
    CHECK(m.w[0] == 0 && m.w[2] == 4096);
    CHECK(m.w[4] == 4096);
    CHECK(m.w[6] == -4096 && m.w[8] == 0);

    m = mat_identity();
    gex_cam_rot_x(&m, 0x400);
    CHECK(m.w[0] == 4096);
    CHECK(m.w[4] == 0 && m.w[5] == -4096);
    CHECK(m.w[7] == 4096 && m.w[8] == 0);

    /* Angle wrap: 0x1400 == 0x400, and 0x800 == -0x800. */
    gex_cam_mat3 a = mat_identity(), b = mat_identity();
    gex_cam_rot_z(&a, 0x1400);
    gex_cam_rot_z(&b, 0x400);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    a = mat_identity(); b = mat_identity();
    gex_cam_rot_z(&a, 0x800);
    gex_cam_rot_z(&b, (int16_t)-0x800);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
}

static void test_identity_orientation(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;  /* force identity base */
    gex_cam_mat3 o, m;
    CHECK(gex_cam_build_orientation(&c, &o) == 0);
    /* O = M2 * I. */
    CHECK(o.w[0] == 4096 && o.w[1] == 0 && o.w[2] == 0);
    CHECK(o.w[3] == 0 && o.w[4] == 0 && o.w[5] == -4096);
    CHECK(o.w[6] == 0 && o.w[7] == 4096 && o.w[8] == 0);
    gex_cam_orientation_finalize(&o, &m);
    CHECK(m.w[0] == 4096 && m.w[1] == 0 && m.w[2] == 0);
    CHECK(m.w[3] == 0 && m.w[4] == 0 && m.w[5] == 4096);
    CHECK(m.w[6] == 0 && m.w[7] == -4096 && m.w[8] == 0);
}

/* The original negates the Euler angles before rotating (0x46DE51,
 * 0x46DE67, 0x46DE7D), so a +90 degree Z angle rotates by -90. */
static void test_simple_rot_z(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.euler[2] = 0x400;
    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    int16_t zaxis[3] = { 0, 0, 4096 };
    int32_t out[3];
    xform(&m, zaxis, out);
    CHECK(out[0] == -4096 && out[1] == 0 && out[2] == 0);
}

static void test_simple_rot_y(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.euler[1] = 0x400;
    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    int16_t yaxis[3] = { 0, 4096, 0 };
    int32_t out[3];
    xform(&m, yaxis, out);
    CHECK(out[0] == -4096 && out[1] == 0 && out[2] == 0);
}

static void test_simple_rot_x(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.euler[0] = 0x400;
    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    int16_t xaxis[3] = { 4096, 0, 0 };
    int32_t out[3];
    xform(&m, xaxis, out);
    CHECK(out[0] == 4096 && out[1] == 0 && out[2] == 0);
}

/* build_orientation must call Z, then Y, then X with the NEGATED angles
 * (as the original does at 0x46DE58/0x46DE6E/0x46DE84):
 * R = RotX(-x) * RotY(-y) * RotZ(-z) * base, and M2 is applied to the
 * columns of R. */
static void test_composition_zyx(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.euler[0] = 0x200;
    c.euler[1] = 0x200;
    c.euler[2] = 0x200;

    gex_cam_mat3 r = mat_identity();
    gex_cam_rot_z(&r, (int16_t)-0x200);
    gex_cam_rot_y(&r, (int16_t)-0x200);
    gex_cam_rot_x(&r, (int16_t)-0x200);
    gex_cam_mat3 oexp;
    gex_cam_apply_m2(&oexp, &r);

    gex_cam_mat3 o;
    CHECK(gex_cam_build_orientation(&c, &o) == 0);
    CHECK(memcmp(&o, &oexp, sizeof(o)) == 0);

    /* Golden values of the corrected chain (O = M2 * R). */
    CHECK(o.w[0] == 2047 && o.w[1] == 2047 && o.w[2] == -2896);
    CHECK(o.w[3] == -3494 && o.w[4] == 601 && o.w[5] == -2047);
    CHECK(o.w[6] == -601 && o.w[7] == 3494 && o.w[8] == 2047);

    /* The resulting M must stay (almost) orthonormal. The original
     * truncates every product to Q12, so a few units of drift are
     * expected and are reproduced by the port. */
    gex_cam_mat3 m;
    gex_cam_orientation_finalize(&o, &m);
    int16_t e1[3] = { 4096, 0, 0 };
    int16_t e2[3] = { 0, 4096, 0 };
    int32_t a[3], b[3];
    xform(&m, e1, a);
    xform(&m, e2, b);
    int32_t dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    int32_t na = a[0] * a[0] + a[1] * a[1] + a[2] * a[2];
    int32_t nb = b[0] * b[0] + b[1] * b[1] + b[2] * b[2];
    CHECK(dot > -16384 && dot < 16384);
    CHECK(na > 16000000 && na < 16900000);
    CHECK(nb > 16000000 && nb < 16900000);
}

/* Alternative angle branch and a non-identity base matrix. */
static void test_alt_and_base(void) {
    gex_camera c = cam_default();
    gex_cam_mat3 base = mat_identity();
    c.base = &base;
    c.euler[2] = 0x400;         /* normal: 90 degrees */
    c.euler_alt[2] = 0x200;     /* alternative: 45 degrees */

    gex_cam_mat3 normal, alt;
    CHECK(gex_cam_build_orientation(&c, &normal) == 0);
    c.use_alt_angles = 1;
    CHECK(gex_cam_build_orientation(&c, &alt) == 0);
    CHECK(memcmp(&normal, &alt, sizeof(normal)) != 0);

    /* The alternative branch negates its angles as well (0x46DED4,
     * 0x46DEEC, 0x46DF06). */
    gex_cam_mat3 r = mat_identity();
    gex_cam_rot_z(&r, (int16_t)-0x200);
    gex_cam_mat3 oexp;
    gex_cam_apply_m2(&oexp, &r);
    CHECK(memcmp(&alt, &oexp, sizeof(alt)) == 0);
}

static void test_q12_ops(void) {
    CHECK(((int32_t)(-8193) >> 12) == -3);
    CHECK(((int32_t)(-8192) >> 12) == -2);
    CHECK(((int32_t)(8192) >> 12) == 2);
}

/* Exact local frame for w = 0x180 (384), f = 0x140 (320):
 *   a = 256, b = 192
 * raw (after sar 12): (0,40,24), (30,0,24), (-30,0,24), (0,-40,24)
 * normalized:          (0,3523,2114), (3321,0,2656),
 *                      (-3321,0,2656), (0,-3523,2114) */
static void test_local_frame_exact(void) {
    gex_camera c = cam_default();
    gex_cam_build_local_frame(&c);

    CHECK(c.local[0][0] == 0 && c.local[0][1] == 0 &&
          c.local[0][2] == 4096);

    CHECK(c.local[1][0] == 0 && c.local[1][1] == 3523 &&
          c.local[1][2] == 2114);
    CHECK(c.local[2][0] == 3321 && c.local[2][1] == 0 &&
          c.local[2][2] == 2656);
    CHECK(c.local[3][0] == -3321 && c.local[3][1] == 0 &&
          c.local[3][2] == 2656);
    CHECK(c.local[4][0] == 0 && c.local[4][1] == -3523 &&
          c.local[4][2] == 2114);
}

/* A/B wiring: A rows from local[0..2], B rows 0-1 from local[3..4],
 * B row 2 preserved from the caller, a_cam = A*pos, b_cam = B*pos. */
static void test_build_visibility_matrices(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.pos[0] = 1000; c.pos[1] = 2000; c.pos[2] = 3000;

    int16_t local[5][3] = {
        { 100, 200, 300 },
        { 200, 300, 100 },
        { 300, 100, 200 },
        { -100, 200, 300 },
        { 100, -200, 300 },
    };
    memcpy(c.local, local, sizeof(local));
    c.mat_b[6] = 7; c.mat_b[7] = 8; c.mat_b[8] = 9;

    gex_cam_build_visibility_matrices(&c);

    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    CHECK(memcmp(&c.m, &m, sizeof(m)) == 0);

    int32_t t[3];
    for (int row = 0; row < 3; row++) {
        xform(&m, local[row], t);
        CHECK(c.mat_a[3 * row + 0] == (int16_t)t[0]);
        CHECK(c.mat_a[3 * row + 1] == (int16_t)t[1]);
        CHECK(c.mat_a[3 * row + 2] == (int16_t)t[2]);
    }
    xform(&m, local[3], t);
    CHECK(c.mat_b[0] == (int16_t)t[0]);
    CHECK(c.mat_b[1] == (int16_t)t[1]);
    CHECK(c.mat_b[2] == (int16_t)t[2]);
    xform(&m, local[4], t);
    CHECK(c.mat_b[3] == (int16_t)t[0]);
    CHECK(c.mat_b[4] == (int16_t)t[1]);
    CHECK(c.mat_b[5] == (int16_t)t[2]);

    /* B row 2 is read, never written. */
    CHECK(c.mat_b[6] == 7 && c.mat_b[7] == 8 && c.mat_b[8] == 9);

    /* a_cam = A * pos (not M * pos). */
    gex_cam_mat3 a_mat;
    memcpy(a_mat.w, c.mat_a, sizeof(a_mat.w));
    xform(&a_mat, c.pos, t);
    CHECK(c.a_cam[0] == t[0] && c.a_cam[1] == t[1] && c.a_cam[2] == t[2]);
    CHECK(c.a_cam[0] == 24 && c.a_cam[1] == -123 && c.a_cam[2] == 97);

    /* b_cam = B * pos, including the preserved row 2. */
    gex_cam_mat3 b_mat;
    memcpy(b_mat.w, c.mat_b, sizeof(b_mat.w));
    xform(&b_mat, c.pos, t);
    CHECK(c.b_cam[0] == t[0] && c.b_cam[1] == t[1] && c.b_cam[2] == t[2]);
    CHECK(c.b_cam[2] == 12);

    /* a_cam must not equal M * pos (would indicate the old bug). */
    int32_t mpos[3];
    xform(&m, c.pos, mpos);
    CHECK(c.a_cam[0] != mpos[0] || c.a_cam[1] != mpos[1] ||
          c.a_cam[2] != mpos[2]);
}

/* Six semiplanes with A = B = identity. */
static void test_visibility_a_planes(void) {
    gex_camera c = cam_default();
    mat_diag(c.mat_a, 4096, 4096, 4096);
    mat_diag(c.mat_b, 4096, 4096, 4096);
    c.a_cam[0] = c.a_cam[1] = c.a_cam[2] = 0;
    c.b_cam[0] = c.b_cam[1] = c.b_cam[2] = 0;
    c.far = 1000;
    c.pos[0] = c.pos[1] = c.pos[2] = 0;

    int16_t p[3];

    p[0] = 0; p[1] = 0; p[2] = 0;
    CHECK(gex_cam_sphere_visible(&c, p, 10));      /* inside */
    p[0] = -10; p[1] = 0; p[2] = 0;
    CHECK(!gex_cam_sphere_visible(&c, p, 10));     /* near <= -r */
    p[0] = -9; p[1] = 0; p[2] = 0;
    CHECK(gex_cam_sphere_visible(&c, p, 10));      /* near > -r */
    p[0] = 1010; p[1] = 0; p[2] = 0;
    CHECK(!gex_cam_sphere_visible(&c, p, 10));     /* far >= far+r */
    p[0] = 1009; p[1] = 0; p[2] = 0;
    CHECK(gex_cam_sphere_visible(&c, p, 10));

    p[0] = 0; p[1] = -10; p[2] = 0;
    CHECK(!gex_cam_sphere_visible(&c, p, 10));     /* A row 1 */
    p[0] = 0; p[1] = -9; p[2] = 0;
    CHECK(gex_cam_sphere_visible(&c, p, 10));
    p[0] = 0; p[1] = 0; p[2] = -10;
    CHECK(!gex_cam_sphere_visible(&c, p, 10));     /* A row 2 */
    p[0] = 0; p[1] = 0; p[2] = -9;
    CHECK(gex_cam_sphere_visible(&c, p, 10));
}

/* B must be used for T_B. B row 0 is negated and row 1 is negated, so
 * a test that accidentally used A would accept these centres. */
static void test_visibility_b_planes(void) {
    gex_camera c = cam_default();
    mat_diag(c.mat_a, 4096, 4096, 4096);
    mat_diag(c.mat_b, -4096, -4096, 4096);
    c.a_cam[0] = c.a_cam[1] = c.a_cam[2] = 0;
    c.b_cam[0] = c.b_cam[1] = c.b_cam[2] = 0;
    c.far = 1000;

    int16_t p[3];
    p[0] = 11; p[1] = 0; p[2] = 0;   /* A.x = 11 (ok), B.x = -11 <= -r */
    CHECK(!gex_cam_sphere_visible(&c, p, 10));
    p[0] = 9; p[1] = 0; p[2] = 0;    /* B.x = -9 > -r */
    CHECK(gex_cam_sphere_visible(&c, p, 10));
    p[0] = 0; p[1] = 10; p[2] = 0;   /* A.y = 10, B.y = -10 <= -r */
    CHECK(!gex_cam_sphere_visible(&c, p, 10));
    p[0] = 0; p[1] = 9; p[2] = 0;    /* B.y = -9 > -r */
    CHECK(gex_cam_sphere_visible(&c, p, 10));
}

/* a_cam / b_cam offsets are the plane origins. */
static void test_visibility_cam_offsets(void) {
    gex_camera c = cam_default();
    mat_diag(c.mat_a, 4096, 4096, 4096);
    mat_diag(c.mat_b, 4096, 4096, 4096);
    c.far = 1000;

    int16_t p[3] = { 0, 0, 100 };

    memset(c.a_cam, 0, sizeof(c.a_cam));
    memset(c.b_cam, 0, sizeof(c.b_cam));
    CHECK(gex_cam_sphere_visible(&c, p, 10));

    c.a_cam[2] = 105;               /* d0 = 100 - 105 = -5 > -r */
    CHECK(gex_cam_sphere_visible(&c, p, 10));
    c.a_cam[2] = 110;               /* d0 = -10 <= -r */
    CHECK(!gex_cam_sphere_visible(&c, p, 10));

    memset(c.a_cam, 0, sizeof(c.a_cam));
    c.b_cam[1] = 10;                /* T_B.y - B_cam.y = -10 <= -r */
    CHECK(!gex_cam_sphere_visible(&c, p, 10));
    c.b_cam[1] = 9;
    CHECK(gex_cam_sphere_visible(&c, p, 10));

    memset(c.b_cam, 0, sizeof(c.b_cam));
    c.a_cam[1] = 10;                /* A row 1 origin */
    CHECK(!gex_cam_sphere_visible(&c, p, 10));
}

static void test_visibility_unsigned_radius(void) {
    gex_camera c = cam_default();
    mat_diag(c.mat_a, 4096, 4096, 4096);
    mat_diag(c.mat_b, 4096, 4096, 4096);
    c.a_cam[0] = c.a_cam[1] = c.a_cam[2] = 0;
    c.b_cam[0] = c.b_cam[1] = c.b_cam[2] = 0;
    c.far = 100;

    int16_t p[3] = { 0, 0, 0 };
    /* Radius 0xFFFF is 65535, not -1. */
    CHECK(gex_cam_sphere_visible(&c, p, 0xFFFF));
    /* With a wrapped radius of -1 the far limit would be 99 and this
     * centre (3000) would be rejected. */
    p[0] = 3000;
    CHECK(gex_cam_sphere_visible(&c, p, 0xFFFF));
}

static void test_plane_distance(void) {
    gex_camera c = cam_default();
    c.pos[0] = 4096;
    int16_t normal[3] = { 4096, 0, 0 };
    int32_t d = gex_cam_plane_distance(&c, normal, 0);
    CHECK(d == 4096);
}

/* ------------------------------------------------------------------ */
/* Explicit x86 reference model                                       */
/* ------------------------------------------------------------------ */

/* The reference model below does the arithmetic the way the original
 * instructions do it: 32-bit registers, wrap-around, `sar 12`, and a
 * bit-copy instead of any implementation-defined signed conversion. It is
 * written independently of the implementation under test so the two can
 * be compared. */

static int32_t ref_bits_i32(uint32_t u) {
    int32_t v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

static int16_t ref_bits_i16(uint16_t u) {
    int16_t v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

static int32_t ref_sar12(uint32_t bits) {
    uint32_t s = bits >> 12;
    if (bits & 0x80000000u) s |= 0xFFF00000u;
    return ref_bits_i32(s);
}

static int32_t ref_add32(int32_t a, int32_t b) {
    return ref_bits_i32((uint32_t)a + (uint32_t)b);
}

static int32_t ref_sub32(int32_t a, int32_t b) {
    return ref_bits_i32((uint32_t)a - (uint32_t)b);
}

/* Model of 0x00401C00: three imul + two add, sar 12, sub distance. */
static int32_t ref_plane_distance(const gex_camera *cam,
                                  const int16_t normal[3], int32_t distance) {
    uint32_t acc = 0;
    for (int i = 0; i < 3; i++) {
        acc += (uint32_t)((int32_t)cam->pos[i] * (int32_t)normal[i]);
    }
    return ref_sub32(ref_sar12(acc), distance);
}

static void ref_xform(const gex_cam_mat3 *m, const int16_t v[3],
                      int32_t out[3]) {
    for (int r = 0; r < 3; r++) {
        uint32_t acc = (uint32_t)((int32_t)m->w[3 * r + 0] * (int32_t)v[0]);
        acc += (uint32_t)((int32_t)m->w[3 * r + 1] * (int32_t)v[1]);
        acc += (uint32_t)((int32_t)m->w[3 * r + 2] * (int32_t)v[2]);
        out[r] = ref_sar12(acc);
    }
}

/* Model of the six-semiplane test in 0x00401C00. */
static int ref_sphere_visible(const gex_camera *cam,
                              const int16_t c[3], uint16_t radius) {
    int32_t r = (int32_t)radius;
    gex_cam_mat3 a_mat, b_mat;
    memcpy(a_mat.w, cam->mat_a, sizeof(a_mat.w));
    memcpy(b_mat.w, cam->mat_b, sizeof(b_mat.w));

    int32_t T_A[3], T_B[3];
    ref_xform(&a_mat, c, T_A);
    ref_xform(&b_mat, c, T_B);

    int32_t neg_r = ref_sub32(0, r);
    int32_t far_plus_r = ref_add32(cam->far, r);
    int32_t d0 = ref_sub32(T_A[0], cam->a_cam[0]);
    if (d0 <= neg_r) return 0;
    if (d0 >= far_plus_r) return 0;
    if (ref_sub32(T_A[1], cam->a_cam[1]) <= neg_r) return 0;
    if (ref_sub32(T_A[2], cam->a_cam[2]) <= neg_r) return 0;
    if (ref_sub32(T_B[0], cam->b_cam[0]) <= neg_r) return 0;
    if (ref_sub32(T_B[1], cam->b_cam[1]) <= neg_r) return 0;
    return 1;
}

/* Deterministic pseudo-random source with a bias toward the int16 edges,
 * so the wrap paths are reached often. */
static uint32_t lcg_next(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static int16_t lcg_i16(uint32_t *s) {
    uint32_t r = lcg_next(s);
    switch (r & 7u) {
    case 0: return INT16_MIN;
    case 1: return INT16_MAX;
    case 2: return ref_bits_i16((uint16_t)(r >> 8));
    default: return ref_bits_i16((uint16_t)(r >> 16));
    }
}

static void test_plane_distance_wrap(void) {
    gex_camera c = cam_default();

    /* Sum of the three products above INT32_MAX:
     *   3 * 32767^2 = 3 221 028 867
     *   wraps to -1 073 938 429; sar 12 -> -262 192. */
    int16_t npos[3] = { 32767, 32767, 32767 };
    c.pos[0] = 32767; c.pos[1] = 32767; c.pos[2] = 32767;
    CHECK(gex_cam_plane_distance(&c, npos, 0) == -262192);
    CHECK(gex_cam_plane_distance(&c, npos, 0) ==
          ref_plane_distance(&c, npos, 0));

    /* Sum below INT32_MIN:
     *   3 * (-32768 * 32767) = -3 221 127 168
     *   wraps to 1 073 840 128; sar 12 -> 262 168. */
    c.pos[0] = -32768; c.pos[1] = -32768; c.pos[2] = -32768;
    CHECK(gex_cam_plane_distance(&c, npos, 0) == 262168);
    CHECK(gex_cam_plane_distance(&c, npos, 0) ==
          ref_plane_distance(&c, npos, 0));

    /* Negative value before sar 12: 100 * -4096 = -409 600 -> -100. */
    int16_t nneg[3] = { -4096, 0, 0 };
    c.pos[0] = 100; c.pos[1] = 0; c.pos[2] = 0;
    CHECK(gex_cam_plane_distance(&c, nneg, 0) == -100);
    CHECK(gex_cam_plane_distance(&c, nneg, 0) ==
          ref_plane_distance(&c, nneg, 0));

    /* distance subtraction wraps in 32 bits:
     *   0 - INT32_MIN = INT32_MIN
     *   5 - INT32_MAX = -2 147 483 642
     *   -5 - INT32_MAX = 2 147 483 644  (wrapped) */
    int16_t nzero[3] = { 0, 0, 0 };
    CHECK(gex_cam_plane_distance(&c, nzero, INT32_MIN) == INT32_MIN);
    int16_t n4096[3] = { 4096, 0, 0 };
    c.pos[0] = 5;                         /* d = (5 * 4096) >> 12 = 5 */
    CHECK(gex_cam_plane_distance(&c, n4096, INT32_MAX) == -2147483642);
    c.pos[0] = -5;                        /* d = -5 */
    CHECK(gex_cam_plane_distance(&c, n4096, INT32_MAX) == 2147483644);
    CHECK(gex_cam_plane_distance(&c, n4096, INT32_MAX) ==
          ref_plane_distance(&c, n4096, INT32_MAX));
}

/* 512 deterministic configurations against the reference model. */
static void test_plane_distance_reference(void) {
    gex_camera c = cam_default();
    uint32_t s = 0x9E3779B9u;
    for (int i = 0; i < 512; i++) {
        int16_t n[3];
        int32_t dist = ref_bits_i32(lcg_next(&s));
        for (int k = 0; k < 3; k++) {
            c.pos[k] = lcg_i16(&s);
            n[k] = lcg_i16(&s);
        }
        CHECK(gex_cam_plane_distance(&c, n, dist) ==
              ref_plane_distance(&c, n, dist));
    }
}

/* Extreme a_cam / far values make the signed differences overflow; the
 * result must still be the wrapped 32-bit one. */
static void test_visibility_wrap(void) {
    gex_camera c = cam_default();
    mat_diag(c.mat_a, 4096, 4096, 4096);
    mat_diag(c.mat_b, 4096, 4096, 4096);
    memset(c.a_cam, 0, sizeof(c.a_cam));
    memset(c.b_cam, 0, sizeof(c.b_cam));
    c.far = 1000;

    int16_t p[3] = { 0, 0, 0 };

    /* T_A.x - INT32_MAX = -INT32_MAX, rejected by the near plane. */
    c.a_cam[0] = INT32_MAX;
    CHECK(!gex_cam_sphere_visible(&c, p, 10));
    CHECK(gex_cam_sphere_visible(&c, p, 10) ==
          ref_sphere_visible(&c, p, 10));

    /* T_A.y - INT32_MIN wraps to INT32_MIN + T_A.y: huge positive, so the
     * lateral plane accepts it. This is the value plain signed arithmetic
     * cannot represent. Both A row 1 and B row 1 use the offset, so both
     * origins are set to INT32_MIN. */
    memset(c.a_cam, 0, sizeof(c.a_cam));
    memset(c.b_cam, 0, sizeof(c.b_cam));
    c.a_cam[1] = INT32_MIN;
    c.b_cam[1] = INT32_MIN;
    int16_t py[3] = { 0, -4096, 0 };
    CHECK(gex_cam_sphere_visible(&c, py, 10) == 1);
    CHECK(gex_cam_sphere_visible(&c, py, 10) ==
          ref_sphere_visible(&c, py, 10));

    /* far + radius wraps when far is INT32_MAX: 0 >= (INT32_MIN + 9) is
     * true, so the far limit rejects the point. */
    memset(c.a_cam, 0, sizeof(c.a_cam));
    c.far = INT32_MAX;
    CHECK(!gex_cam_sphere_visible(&c, p, 10));
    CHECK(gex_cam_sphere_visible(&c, p, 10) ==
          ref_sphere_visible(&c, p, 10));
    c.far = 1000;
}

/* 256 deterministic camera configurations against the reference model,
 * with at least one accepted and one rejected case. */
static void test_visibility_reference(void) {
    uint32_t s = 0x51ED2701u;
    int seen_visible = 0, seen_rejected = 0;
    for (int i = 0; i < 256; i++) {
        gex_camera c = cam_default();
        int16_t center[3];
        for (int k = 0; k < 9; k++) c.mat_a[k] = lcg_i16(&s);
        for (int k = 0; k < 9; k++) c.mat_b[k] = lcg_i16(&s);
        for (int k = 0; k < 3; k++) {
            center[k] = lcg_i16(&s);
            c.a_cam[k] = ref_bits_i32(lcg_next(&s));
            c.b_cam[k] = ref_bits_i32(lcg_next(&s));
        }
        c.far = ref_bits_i32(lcg_next(&s));
        uint16_t radius = (uint16_t)(lcg_next(&s) >> 16);

        int got = gex_cam_sphere_visible(&c, center, radius);
        int want = ref_sphere_visible(&c, center, radius);
        CHECK(got == want);
        if (got) seen_visible++; else seen_rejected++;
    }
    CHECK(seen_visible > 0);
    CHECK(seen_rejected > 0);
}

static void test_leaf_predicate(void) {
    CHECK(gex_cam_node_is_leaf(0x0000) == 1);
    CHECK(gex_cam_node_is_leaf(0x0001) == 0);
    CHECK(gex_cam_node_is_leaf(0x0002) == 1);
    CHECK(gex_cam_node_is_leaf(0x0003) == 0);
}

static int visible_fn_all(const uint8_t *image, size_t image_size,
                          uint32_t node_offset, void *ctx) {
    (void)image;(void)image_size;(void)node_offset;(void)ctx;
    return 1;
}

static void test_bsp_child_order_camera(void) {
    size_t sz = 3 * GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz, 1);
    /* Centres at the camera origin keep every node inside the six
     * semiplanes, so the test isolates the child visit order. */
    *(uint16_t*)(img + 0 + 0x06) = 100;
    *(uint16_t*)(img + 0 + 0x0C) = 4096;
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x14) = 0;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;
    *(uint32_t*)(img + 0 + 0x1C) = 2*GEX_BSP_NODE_SIZE;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x06) = 100;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x12) = 0x0002;
    *(uint16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x06) = 100;
    *(uint16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x12) = 0x0002;

    gex_camera c = cam_default();
    c.flag_identity = 1;
    gex_cam_update(&c);

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, visible_fn_all, &c, &c, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);
    CHECK(r.leaves[0] == GEX_BSP_NODE_SIZE);
    CHECK(r.leaves[1] == 2*GEX_BSP_NODE_SIZE);
    gex_bsp_walk_free(&r);
    free(img);
}

/* ---------------------------------------------------------------------------
 * Dynamic reference: state of the retail camera captured from the running
 * GEX3D.EXE (AZTEC16 forced as LEVEL\MAP7.DFX, frame 1 of the 3D route,
 * 2026-11-11). The values were read out of the live process with hardware
 * breakpoints, with no code patching:
 *
 *   camera object   0x0065CD80
 *     +0x00 position        (0x0925, 0x0FE7, 0x0C02)
 *     +0x30 Euler angles    (0xFE73, 0x0000, 0x0AF2)
 *     +0x40 focal 0x0140, +0x44 0x0200, +0x48 width 0x0180, +0x5C far 0x2134
 *     +0x60 local frame     (0,0,4096) (0,3523,2114) (3321,0,2656)
 *                          (-3321,0,2656) (0,-3523,2114)
 *     +0x88 matrix A       3074 -1352 -2343 / -258 113 -4099 / 657 -3917 -1520
 *     +0xA8 matrix B       3329 2162 -1520 / 3430 -1509 1679 / 0 0 0
 *     +0xC8 a_cam          -1346 -3112 -4659
 *     +0xD4 b_cam          2910 1720 0
 *     +0xE8 matrix M       -1648 -2144 3074 / -3749 943 -1352 / 0 -3359 -2343
 *     +0xF0 base matrix    identity (4096 at 0, 4 and 8)
 *
 * The port must reproduce every one of those values exactly.
 * --------------------------------------------------------------------- */
static void test_dynamic_reference_camera(void) {
    static const int16_t kPos[3]   = { 0x0925, 0x0FE7, 0x0C02 };
    static const int16_t kEuler[3] = { 0xFE73, 0x0000, 0x0AF2 };
    static const int16_t kLocal[5][3] = {
        { 0, 0, 4096 }, { 0, 3523, 2114 }, { 3321, 0, 2656 },
        { -3321, 0, 2656 }, { 0, -3523, 2114 }
    };
    static const int16_t kA[9] = { 3074, -1352, -2343, -258, 113, -4099,
                                   657, -3917, -1520 };
    static const int16_t kB[9] = { 3329, 2162, -1520, 3430, -1509, 1679,
                                   0, 0, 0 };
    static const int16_t kM[9] = { -1648, -2144, 3074, -3749, 943, -1352,
                                   0, -3359, -2343 };
    static const int32_t kACam[3] = { -1346, -3112, -4659 };
    static const int32_t kBCam[3] = { 2910, 1720, 0 };

    gex_camera c = cam_default();
    gex_cam_mat3 base = mat_identity();
    int i;

    memcpy(c.pos, kPos, sizeof(kPos));
    memcpy(c.euler, kEuler, sizeof(kEuler));
    c.focal = 0x140;
    c.field_0x44 = 0x200;
    c.width = 0x180;
    c.far = 0x2134;
    c.base = &base;

    gex_cam_update(&c);

    for (i = 0; i < 5; i++) {
        CHECK(c.local[i][0] == kLocal[i][0]);
        CHECK(c.local[i][1] == kLocal[i][1]);
        CHECK(c.local[i][2] == kLocal[i][2]);
    }
    for (i = 0; i < 9; i++) {
        CHECK(c.m.w[i] == kM[i]);
        CHECK(c.mat_a[i] == kA[i]);
        CHECK(c.mat_b[i] == kB[i]);
    }
    for (i = 0; i < 3; i++) {
        CHECK(c.a_cam[i] == kACam[i]);
        CHECK(c.b_cam[i] == kBCam[i]);
    }
}

/* The retail sine table (0x00646C20) truncates instead of rounding: the
 * values dumped from the running game match trunc(4096*sin(2*pi*i/4096))
 * on every live entry (0..0x400) and differ from the rounded ones on 502
 * of the 1024. */
static void test_table_truncation(void) {
    gex_cam_init_tables();
    CHECK(gex_cam_sin_tab[2] == 12);      /* 12.566 -> 12 (trunc), 13 (round) */
    CHECK(gex_cam_sin_tab[3] == 18);      /* 18.849 -> 18 (trunc), 19 (round) */
    CHECK(gex_cam_sin_tab[6] == 37);      /* 37.699 -> 37 (trunc), 38 (round) */
    CHECK(gex_cam_cos_tab[2] == 4095);    /* 4095.98 -> 4095 (trunc), 4096 (round) */
    CHECK(gex_cam_cos_tab[100] == 4047);  /* 4047.7 -> 4047 (trunc), 4048 (round) */
    CHECK(gex_cam_sin_tab[100] == 625);   /* 625.9 -> 625 (trunc), 626 (round) */
}

int main(void) {
    printf("Running camera tests...\n");
    test_tables_init();
    test_table_truncation();
    test_rotation_exact();
    test_identity_orientation();
    test_simple_rot_z();
    test_simple_rot_y();
    test_simple_rot_x();
    test_composition_zyx();
    test_alt_and_base();
    test_q12_ops();
    test_local_frame_exact();
    test_build_visibility_matrices();
    test_dynamic_reference_camera();
    test_visibility_a_planes();
    test_visibility_b_planes();
    test_visibility_cam_offsets();
    test_visibility_unsigned_radius();
    test_plane_distance();
    test_plane_distance_wrap();
    test_plane_distance_reference();
    test_visibility_wrap();
    test_visibility_reference();
    test_leaf_predicate();
    test_bsp_child_order_camera();
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
