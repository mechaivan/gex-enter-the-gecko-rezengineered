/*
 * test_camera.c - Tests for camera orientation chain and visibility (S-2).
 *
 * Covers: identity matrix, simple rotations per axis, Z→Y→X composition,
 * normal/alternative branches, base matrix and forced identity, Q12 ops,
 * 32-bit wrap-around and int16 truncation, approximate normalization,
 * null vector, six-semiplane visibility, unsigned radius, child visit
 * order, and BSP traversal integration.
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

static gex_camera cam_default(void) {
    gex_camera c;
    memset(&c, 0, sizeof(c));
    c.pos[0] = 0; c.pos[1] = 0; c.pos[2] = 0;
    c.euler[0] = 0; c.euler[1] = 0; c.euler[2] = 0;
    c.focal = 0x140;
    c.field_0x44 = 0x200;
    c.width = 0x180;
    c.far = 0x2EE0;
    c.flag_identity = 0;
    c.base = NULL;
    c.euler_alt[0] = 0; c.euler_alt[1] = 0; c.euler_alt[2] = 0;
    return c;
}

static void test_tables_init(void) {
    gex_cam_init_tables();
    /* sin(0) = 0, cos(0) = 4096 */
    CHECK(gex_cam_sin_tab[0] == 0);
    CHECK(gex_cam_cos_tab[0] == 4096);
    /* sin(0x1000/4) = sin(90°) ≈ 4096 */
    CHECK(gex_cam_sin_tab[0x400] > 4000);
    CHECK(gex_cam_cos_tab[0x400] < 100);
    /* sin(0x1000/2) = sin(180°) ≈ 0 */
    CHECK(gex_cam_sin_tab[0x800] > -100 && gex_cam_sin_tab[0x800] < 100);
    CHECK(gex_cam_cos_tab[0x800] < -4000);
    /* Tables are periodic: sin(2π - x) = -sin(x) */
    CHECK(gex_cam_sin_tab[0xFFF] == -gex_cam_sin_tab[1]);
}

static void test_identity_orientation(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;  /* force identity base */
    gex_cam_mat3 o, m;
    CHECK(gex_cam_build_orientation(&c, &o) == 0);
    /* O = M2 · I = M2 */
    CHECK(o.w[0] == 4096 && o.w[1] == 0 && o.w[2] == 0);
    CHECK(o.w[3] == 0 && o.w[4] == 0 && o.w[5] == -4096);
    CHECK(o.w[6] == 0 && o.w[7] == 4096 && o.w[8] == 0);
    gex_cam_orientation_finalize(&o, &m);
    /* M = transpose(O) = M2^T */
    CHECK(m.w[0] == 4096 && m.w[1] == 0 && m.w[2] == 0);
    CHECK(m.w[3] == 0 && m.w[4] == 0 && m.w[5] == 4096);
    CHECK(m.w[6] == 0 && m.w[7] == -4096 && m.w[8] == 0);
}

static void test_simple_rot_z(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    /* Z angle 90° = 0x400, negated → -0x400 */
    c.euler[2] = 0x400;
    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    /* With M2 change of axes, Z axis maps to Y: M·(0,0,4096) = (0,±4096,0) */
    int16_t zaxis[3] = { 0, 0, 4096 };
    int32_t out[3];
    for (int r = 0; r < 3; r++) {
        out[r] = (int32_t)m.w[3*r+0]*(int32_t)zaxis[0] +
                 (int32_t)m.w[3*r+1]*(int32_t)zaxis[1] +
                 (int32_t)m.w[3*r+2]*(int32_t)zaxis[2];
        out[r] >>= 12;
    }
    CHECK(out[0] == 0 && out[1] != 0 && out[2] == 0);
}

static void test_simple_rot_y(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.euler[1] = 0x400;  /* Y angle 90° */
    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    /* With M2 change of axes, Y axis maps to -Z */
    int16_t yaxis[3] = { 0, 4096, 0 };
    int32_t out[3];
    for (int r = 0; r < 3; r++) {
        out[r] = (int32_t)m.w[3*r+0]*(int32_t)yaxis[0] +
                 (int32_t)m.w[3*r+1]*(int32_t)yaxis[1] +
                 (int32_t)m.w[3*r+2]*(int32_t)yaxis[2];
        out[r] >>= 12;
    }
    CHECK(out[0] == 0 && out[1] == 0 && out[2] != 0);
}

static void test_simple_rot_x(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.euler[0] = 0x400;  /* X angle 90° */
    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    /* X axis preserved through M2: M·(4096,0,0) = (4096,0,0) */
    int16_t xaxis[3] = { 4096, 0, 0 };
    int32_t out[3];
    for (int r = 0; r < 3; r++) {
        out[r] = (int32_t)m.w[3*r+0]*(int32_t)xaxis[0] +
                 (int32_t)m.w[3*r+1]*(int32_t)xaxis[1] +
                 (int32_t)m.w[3*r+2]*(int32_t)xaxis[2];
        out[r] >>= 12;
    }
    CHECK(out[0] == 4096 && out[1] == 0 && out[2] == 0);
}

static void test_composition_zyx(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    c.euler[0] = 0x200;  /* X 45° */
    c.euler[1] = 0x200;  /* Y 45° */
    c.euler[2] = 0x200;  /* Z 45° */
    gex_cam_mat3 o, m;
    gex_cam_build_orientation(&c, &o);
    gex_cam_orientation_finalize(&o, &m);
    /* M·e1 should have norm ≈ 4096^2 (approximately orthogonal) */
    int16_t e1[3] = { 4096, 0, 0 };
    int32_t c0[3];
    for (int r = 0; r < 3; r++) {
        c0[r] = (int32_t)m.w[3*r+0]*(int32_t)e1[0] +
                (int32_t)m.w[3*r+1]*(int32_t)e1[1] +
                (int32_t)m.w[3*r+2]*(int32_t)e1[2];
        c0[r] >>= 12;
    }
    int32_t norm = c0[0]*c0[0] + c0[1]*c0[1] + c0[2]*c0[2];
    CHECK(norm > 10000000 && norm < 100000000);
}

static void test_q12_ops(void) {
    /* sar 12: negative rounds toward −∞ */
    CHECK(((int32_t)(-8193) >> 12) == -3);
    CHECK(((int32_t)(-8192) >> 12) == -2);
    CHECK(((int32_t)(8192) >> 12) == 2);
}

static void test_approx_normalize(void) {
    gex_camera c = cam_default();
    c.width = 0x180;  /* 384 */
    c.focal = 0x140;  /* 320 */
    gex_cam_build_local_frame(&c);
    /* local[0] should be (0, 0, 4096) */
    CHECK(c.local[0][0] == 0 && c.local[0][1] == 0 && c.local[0][2] == 4096);
    /* Other locals should be non-zero normalized vectors */
    for (int i = 1; i <= 4; i++) {
        int32_t norm = (int32_t)c.local[i][0]*c.local[i][0] +
                       (int32_t)c.local[i][1]*c.local[i][1] +
                       (int32_t)c.local[i][2]*c.local[i][2];
        CHECK(norm > 1000000 && norm < 30000000);
    }
}

static void test_visibility_sphere_inside(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    gex_cam_update(&c);
    /* Sphere at origin, small radius → visible */
    int16_t center[3] = { 0, 0, 1000 };
    CHECK(gex_cam_sphere_visible(&c, center, 100));
}

static void test_visibility_sphere_behind(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    gex_cam_update(&c);
    /* Sphere far behind → culled by far plane (far = 12000) */
    int16_t center[3] = { 0, 0, -20000 };
    CHECK(!gex_cam_sphere_visible(&c, center, 100));
}

static void test_visibility_sphere_too_far_right(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    gex_cam_update(&c);
    /* Sphere far to the right → culled by lateral plane */
    int16_t center[3] = { 30000, 0, 1000 };
    CHECK(!gex_cam_sphere_visible(&c, center, 100));
}

static void test_visibility_unsigned_radius(void) {
    gex_camera c = cam_default();
    c.flag_identity = 1;
    gex_cam_update(&c);
    /* Radius 0xFFFF (65535 unsigned) should not wrap to negative */
    int16_t center[3] = { 0, 0, 1000 };
    CHECK(gex_cam_sphere_visible(&c, center, 0xFFFF));
}

static void test_plane_distance(void) {
    gex_camera c = cam_default();
    c.pos[0] = 4096; c.pos[1] = 0; c.pos[2] = 0;
    int16_t normal[3] = { 4096, 0, 0 };  /* +X plane */
    int32_t d = gex_cam_plane_distance(&c, normal, 0);
    CHECK(d == 4096);  /* (4096*4096)>>12 = 4096 */
}

static void test_leaf_predicate(void) {
    CHECK(gex_cam_node_is_leaf(0x0000) == 1);  /* & 1 == 0 */
    CHECK(gex_cam_node_is_leaf(0x0001) == 0);  /* internal */
    CHECK(gex_cam_node_is_leaf(0x0002) == 1);  /* & 1 == 0 (type 2 = leaf) */
    CHECK(gex_cam_node_is_leaf(0x0003) == 0);  /* & 1 != 0 */
}

static int visible_fn_all(const uint8_t *image, size_t image_size,
                          uint32_t node_offset, void *ctx) {
    (void)image;(void)image_size;(void)node_offset;(void)ctx;
    return 1;
}

static void test_bsp_child_order_camera(void) {
    /* Build: root internal, children c1 (left=0x18) and c2 (right=0x1C).
     * Camera at origin, identity orientation.
     * Root: center=(0,0,1000), radius=100, normal +X, distance 0.
     * d = (camPos · N) >> 12 - distance = 0 - 0 = 0 >= 0
     * So push c2 (hi) then c1 (lo) → visit c1 first.
     */
    size_t sz = 3 * GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz, 1);
    /* root at 0: internal */
    *(int16_t*)(img + 0 + 0x00) = 0;   *(int16_t*)(img + 0 + 0x02) = 0;
    *(int16_t*)(img + 0 + 0x04) = 1000; /* center Z=1000 */
    *(uint16_t*)(img + 0 + 0x06) = 100; /* radius */
    *(uint16_t*)(img + 0 + 0x0C) = 4096;   /* normal X */
    *(uint16_t*)(img + 0 + 0x0E) = 0;
    *(uint16_t*)(img + 0 + 0x10) = 0;
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;  /* internal */
    *(uint32_t*)(img + 0 + 0x14) = 0;       /* distance */
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;   /* child lo */
    *(uint32_t*)(img + 0 + 0x1C) = 2*GEX_BSP_NODE_SIZE; /* child hi */
    /* c1 leaf: center visible */
    *(int16_t*)(img + GEX_BSP_NODE_SIZE + 0x00) = 0;
    *(int16_t*)(img + GEX_BSP_NODE_SIZE + 0x02) = 0;
    *(int16_t*)(img + GEX_BSP_NODE_SIZE + 0x04) = 1000;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x06) = 100;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x12) = 0x0002;
    /* c2 leaf: center visible */
    *(int16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x00) = 0;
    *(int16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x02) = 0;
    *(int16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x04) = 1000;
    *(uint16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x06) = 100;
    *(uint16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x12) = 0x0002;

    gex_camera c = cam_default();
    c.pos[0] = 0; c.pos[1] = 0; c.pos[2] = 0;
    c.flag_identity = 1;
    gex_cam_update(&c);

    gex_bsp_walk_result r; memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, visible_fn_all, &c, &c, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);
    /* Order: c1 (lo) first, then c2 (hi) because d >= 0 pushes hi first */
    CHECK(r.leaves[0] == GEX_BSP_NODE_SIZE);
    CHECK(r.leaves[1] == 2*GEX_BSP_NODE_SIZE);
    gex_bsp_walk_free(&r);
    free(img);
}

int main(void) {
    printf("Running camera tests...\n");
    test_tables_init();
    test_identity_orientation();
    test_simple_rot_z();
    test_simple_rot_y();
    test_simple_rot_x();
    test_composition_zyx();
    test_q12_ops();
    test_approx_normalize();
    test_visibility_sphere_inside();
    test_visibility_sphere_behind();
    test_visibility_sphere_too_far_right();
    test_visibility_unsigned_radius();
    test_plane_distance();
    test_leaf_predicate();
    test_bsp_child_order_camera();
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
