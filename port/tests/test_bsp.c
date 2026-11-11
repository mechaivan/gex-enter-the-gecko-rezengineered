#include "gex/bsp.h"

#ifdef GEX_BSP_TEST_FAULT_INJECT
extern void gex_bsp_set_grow_visited_fail(int which);
extern void gex_bsp_reset_grow_visited_fail(void);
#endif
#include "gex/asset/dfx.h"
#include "gex/camera.h"
#include "ref_map7_frames.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if(!(cond)){ failures++; printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);} } while(0)

static uint8_t *make_two_leaves(size_t *sz) {
    *sz = 3 * GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(*sz, 1);
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;
    *(uint32_t*)(img + 0 + 0x1C) = 2*GEX_BSP_NODE_SIZE;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x12) = GEX_BSP_LEAF_FLAG;
    *(uint16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x12) = GEX_BSP_LEAF_FLAG;
    return img;
}

static void test_two_leaves(void) {
    size_t sz; uint8_t *img = make_two_leaves(&sz);
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_self_cycle(void) {
    /* root at offset 32 (non-zero) so self-reference is representable */
    size_t sz = 2*GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz,1);
    uint32_t root = GEX_BSP_NODE_SIZE;
    /* node at root: internal, child1 = root (self) */
    *(uint16_t*)(img + root + 0x12) = 0x0001;
    *(uint32_t*)(img + root + 0x18) = root;
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, root, NULL, NULL, NULL, &r) == GEX_BSP_ERR_CYCLE);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_multi_cycle(void) {
    size_t sz = 3*GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz,1);
    uint32_t a = GEX_BSP_NODE_SIZE;
    uint32_t b = 2*GEX_BSP_NODE_SIZE;
    /* a -> b -> a */
    *(uint16_t*)(img + a + 0x12) = 0x0001;
    *(uint32_t*)(img + a + 0x18) = b;
    *(uint16_t*)(img + b + 0x12) = 0x0001;
    *(uint32_t*)(img + b + 0x18) = a;
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, a, NULL, NULL, NULL, &r) == GEX_BSP_ERR_CYCLE);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_shared_reference(void) {
    size_t sz = 3*GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz,1);
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;
    *(uint32_t*)(img + 0 + 0x1C) = 2*GEX_BSP_NODE_SIZE;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x12) = 0x0001;
    *(uint32_t*)(img + GEX_BSP_NODE_SIZE + 0x18) = 2*GEX_BSP_NODE_SIZE;
    *(uint16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x12) = GEX_BSP_LEAF_FLAG;
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 1);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_out_of_bounds_child(void) {
    size_t sz = GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz,1);
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = 9999;
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, NULL, &r) == GEX_BSP_ERR_BOUNDS);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_invalid_root(void) {
    size_t sz = GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz,1);
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, sz, NULL, NULL, NULL, &r) == GEX_BSP_ERR_BOUNDS);
    CHECK(gex_bsp_walk(NULL, sz, 0, NULL, NULL, NULL, &r) == GEX_BSP_ERR_ARGUMENT);
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, NULL, NULL) == GEX_BSP_ERR_ARGUMENT);
    free(img);
}

static void test_truncated_image(void) {
    size_t sz = GEX_BSP_NODE_SIZE - 1;
    uint8_t *img = calloc(sz ? sz : 1, 1);
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, NULL, &r) == GEX_BSP_ERR_BOUNDS);
    free(img);
}

static void test_null_image_helpers(void) {
    CHECK(gex_bsp_node_type(NULL, 10, 0) == 0);
    CHECK(gex_bsp_node_is_leaf(NULL, 10, 0) == 0);
}

static int visible_accept(const uint8_t *image, size_t image_size, uint32_t node_offset, void *ctx) {
    (void)image;(void)image_size;(void)ctx;
    return (node_offset != 9999);
}
static int visible_reject(const uint8_t *image, size_t image_size, uint32_t node_offset, void *ctx) {
    (void)image;(void)image_size;(void)node_offset;(void)ctx;
    return 0;
}

static void test_visibility_predicate(void) {
    size_t sz; uint8_t *img = make_two_leaves(&sz);
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, visible_reject, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 0);
    gex_bsp_walk_free(&r);

    memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, visible_accept, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_leaf_cap(void) {
    /* Build a comb: root internal with 0x800 leaves */
    size_t nleaves = GEX_BSP_LIST_CAP;
    size_t nnodes = nleaves + 1;
    size_t sz = nnodes * GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz, 1);
    /* root at 0: internal, child1 = node1, child2 = 0 */
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;
    /* comb: node i (i>=1) is leaf if i==nnodes-1? Instead make each internal node have leaf+next */
    for (size_t i = 1; i < nnodes; i++) {
        uint32_t off = (uint32_t)(i * GEX_BSP_NODE_SIZE);
        if (i == nnodes - 1) {
            *(uint16_t*)(img + off + 0x12) = GEX_BSP_LEAF_FLAG;
        } else {
            *(uint16_t*)(img + off + 0x12) = 0x0001;
            /* leaf child */
            /* We don't have that many nodes allocated. Instead: node i has child1 = a leaf at... */
        }
    }
    /* Simpler: each internal node i has child1 = leaf node, child2 = next internal.
       Allocate nnodes*2 nodes. */
    free(img);
    size_t total = (nnodes*2) * GEX_BSP_NODE_SIZE;
    img = calloc(total, 1);
    /* root 0 -> internal 1 */
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE; /* node1 */
    for (size_t i = 0; i < nleaves; i++) {
        uint32_t in = (uint32_t)((1 + i) * GEX_BSP_NODE_SIZE);
        uint32_t leaf = (uint32_t)((1 + nleaves + i) * GEX_BSP_NODE_SIZE);
        if (i + 1 < nleaves) {
            *(uint16_t*)(img + in + 0x12) = 0x0001;
            *(uint32_t*)(img + in + 0x18) = leaf;          /* leaf */
            *(uint32_t*)(img + in + 0x1C) = (uint32_t)((2 + i) * GEX_BSP_NODE_SIZE); /* next internal */
        } else {
            *(uint16_t*)(img + in + 0x12) = 0x0001;
            *(uint32_t*)(img + in + 0x18) = leaf;
        }
        *(uint16_t*)(img + leaf + 0x12) = GEX_BSP_LEAF_FLAG;
    }
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, total, 0, NULL, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == GEX_BSP_LIST_CAP);
    gex_bsp_walk_free(&r);

    /* one more leaf -> overflow */
    /* add an extra leaf attached to last internal node */
    /* Instead: build nleaves+1 leaves */
    free(img);
    size_t n2 = GEX_BSP_LIST_CAP + 1;
    size_t total2 = (1 + n2 + n2) * GEX_BSP_NODE_SIZE;
    img = calloc(total2, 1);
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;
    for (size_t i = 0; i < n2; i++) {
        uint32_t in = (uint32_t)((1 + i) * GEX_BSP_NODE_SIZE);
        uint32_t leaf = (uint32_t)((1 + n2 + i) * GEX_BSP_NODE_SIZE);
        *(uint16_t*)(img + in + 0x12) = 0x0001;
        *(uint32_t*)(img + in + 0x18) = leaf;
        if (i + 1 < n2) *(uint32_t*)(img + in + 0x1C) = (uint32_t)((2 + i) * GEX_BSP_NODE_SIZE);
        *(uint16_t*)(img + leaf + 0x12) = GEX_BSP_LEAF_FLAG;
    }
    memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, total2, 0, NULL, NULL, NULL, &r) == GEX_BSP_ERR_LEAF_CAP);
    CHECK(r.leaves == NULL && r.count == 0);
    gex_bsp_walk_free(&r);
    free(img);
}


static void test_fault_inject_first(void) {
    /* Build a tree with >64 leaves to force grow_visited */
    size_t nleaves = 65;
    size_t total = (1 + nleaves + nleaves) * GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(total, 1);
    /* root internal -> node1 */
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;
    for (size_t i = 0; i < nleaves; i++) {
        uint32_t in = (uint32_t)((1 + i) * GEX_BSP_NODE_SIZE);
        uint32_t leaf = (uint32_t)((1 + nleaves + i) * GEX_BSP_NODE_SIZE);
        *(uint16_t*)(img + in + 0x12) = 0x0001;
        *(uint32_t*)(img + in + 0x18) = leaf;
        if (i + 1 < nleaves) *(uint32_t*)(img + in + 0x1C) = (uint32_t)((2 + i) * GEX_BSP_NODE_SIZE);
        *(uint16_t*)(img + leaf + 0x12) = GEX_BSP_LEAF_FLAG;
    }

    gex_bsp_reset_grow_visited_fail();
    gex_bsp_set_grow_visited_fail(1);
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, total, 0, NULL, NULL, NULL, &r) == GEX_BSP_ERR_MEMORY);
    CHECK(r.leaves == NULL && r.count == 0);
    gex_bsp_walk_free(&r);

    gex_bsp_reset_grow_visited_fail();
    gex_bsp_set_grow_visited_fail(2);
    memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, total, 0, NULL, NULL, NULL, &r) == GEX_BSP_ERR_MEMORY);
    CHECK(r.leaves == NULL && r.count == 0);
    gex_bsp_walk_free(&r);

    gex_bsp_reset_grow_visited_fail();
    memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, total, 0, NULL, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == nleaves);
    gex_bsp_walk_free(&r);

    free(img);
}

static void test_child_order_distance_bits(void) {
    size_t sz = 3 * GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz, 1);
    *(uint16_t*)(img + 0 + 0x06) = 100;          /* radius */
    *(uint16_t*)(img + 0 + 0x0C) = 4096;         /* normal +X */
    *(uint16_t*)(img + 0 + 0x12) = 0x0001;
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;
    *(uint32_t*)(img + 0 + 0x1C) = 2 * GEX_BSP_NODE_SIZE;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x06) = 100;
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x12) = 0x0002;
    *(uint16_t*)(img + 2 * GEX_BSP_NODE_SIZE + 0x06) = 100;
    *(uint16_t*)(img + 2 * GEX_BSP_NODE_SIZE + 0x12) = 0x0002;

    gex_camera cam;
    memset(&cam, 0, sizeof(cam));
    cam.width = 0x180;
    cam.focal = 0x140;
    cam.far = 1000;
    cam.flag_identity = 1;
    gex_cam_update(&cam);

    gex_bsp_walk_result r;
    memset(&r, 0, sizeof(r));

    /* distance = 0 -> d = 0 >= 0 -> lo child first. */
    *(uint32_t*)(img + 0 + 0x14) = 0;
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, &cam, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);
    CHECK(r.leaves[0] == GEX_BSP_NODE_SIZE);
    CHECK(r.leaves[1] == 2 * GEX_BSP_NODE_SIZE);
    gex_bsp_walk_free(&r);

    /* distance bits 0x80000000 -> d = INT32_MIN (< 0) -> hi child first.
     * The field is read as an unsigned dword and reinterpreted, so this
     * value is guaranteed regardless of the compiler. */
    memset(&r, 0, sizeof(r));
    *(uint32_t*)(img + 0 + 0x14) = 0x80000000u;
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, &cam, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);
    CHECK(r.leaves[0] == 2 * GEX_BSP_NODE_SIZE);
    CHECK(r.leaves[1] == GEX_BSP_NODE_SIZE);
    gex_bsp_walk_free(&r);

    free(img);
}

static void test_real_aztec(void) {
    const char *path = "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX";
    FILE *f = fopen(path,"rb"); if(!f){ printf("skip aztec\n"); return; } fclose(f);
    gex_dfx_container c; memset(&c,0,sizeof(c));
    if (gex_dfx_load(path,&c) != GEX_DFX_OK) { printf("load fail\n"); return; }
    gex_dfx_level level; memset(&level,0,sizeof(level));
    if (gex_dfx_load_level(&c,&level) != GEX_DFX_OK) { printf("level fail\n"); gex_dfx_release_container(&c); return; }
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(c.image, c.image_size, level.descriptor.root_bsp, NULL, NULL, NULL, &r) == GEX_BSP_OK);
    printf("AZTEC leaves (all visible): %u\n", r.count);
    CHECK(r.count == 815u);
    gex_bsp_walk_free(&r);
    gex_dfx_release_level(&level);
    gex_dfx_release_container(&c);
}


/* ---------------------------------------------------------------------------
 * Referencia dinamica del original (2026-11-11).
 *
 * El GEX3D.EXE en ejecucion carga el nivel `map7.dfx` (no AZTEC16) desde
 * `C:\GEX_REZ\GEX2\level\map7.dfx`. Con el breakpoint de hardware en
 * 0x00401C00 se capturaron, en 18 frames consecutivos, la camara (posicion y
 * angulos Euler) y la lista COMPLETA de hojas visibles que el original deja
 * en su buffer de salida. Los punteros de esa lista se convirtieron a offsets
 * dentro de la imagen del nivel restando la base del buffer: es el mismo
 * espacio que recorre el port.
 *
 * El port debe reproducir el conteo y el orden exacto de las hojas.
 * --------------------------------------------------------------------- */
#define MAP7_PATH "C:/GEX_REZ/GEX2/level/map7.dfx"

static int visible_all_ref(const uint8_t *image, size_t image_size,
                           uint32_t node_offset, void *ctx) {
    (void)image; (void)image_size; (void)node_offset; (void)ctx;
    return 1;
}

static void test_map7_reference(void) {
    FILE *f = fopen(MAP7_PATH, "rb");
    if (!f) { printf("skip map7 reference (no file)\n"); return; }
    fclose(f);

    gex_dfx_container c; memset(&c, 0, sizeof(c));
    if (gex_dfx_load(MAP7_PATH, &c) != GEX_DFX_OK) { printf("skip map7 (load)\n"); return; }
    gex_dfx_level level; memset(&level, 0, sizeof(level));
    if (gex_dfx_load_level(&c, &level) != GEX_DFX_OK) {
        printf("skip map7 (level)\n");
        gex_dfx_release_container(&c);
        return;
    }

    gex_cam_mat3 base;
    memset(&base, 0, sizeof(base));
    base.w[0] = base.w[4] = base.w[8] = 4096;   /* base del original (identidad) */

    for (int i = 0; i < REF_FRAME_COUNT; i++) {
        gex_camera cam;
        gex_bsp_walk_result r;
        memset(&cam, 0, sizeof(cam));
        memcpy(cam.pos, REF_FRAMES[i].pos, sizeof(cam.pos));
        memcpy(cam.euler, REF_FRAMES[i].euler, sizeof(cam.euler));
        cam.focal = 0x140;
        cam.field_0x44 = 0x200;
        cam.width = 0x180;
        cam.far = 0x2134;
        cam.base = &base;
        gex_cam_update(&cam);

        memset(&r, 0, sizeof(r));
        CHECK(gex_bsp_walk(c.image, c.image_size, level.descriptor.root_bsp,
                           visible_all_ref, NULL, &cam, &r) == GEX_BSP_OK);
        CHECK(r.count == REF_FRAMES[i].count);
        for (int j = 0; j < 16 && j < (int)r.count; j++) {
            CHECK(r.leaves[j] == REF_FRAMES[i].first[j]);
        }
        /* Frame 0: lista completa (307 hojas) */
        if (i == 0) {
            CHECK(r.count == REF_FULL_FRAME0_COUNT);
            for (uint32_t j = 0; j < r.count && j < REF_FULL_FRAME0_COUNT; j++) {
                CHECK(r.leaves[j] == REF_FULL_0[j]);
            }
        }
        gex_bsp_walk_free(&r);
    }
    printf("map7 reference: %d frames reproducidos\n", REF_FRAME_COUNT);

    gex_dfx_release_level(&level);
    gex_dfx_release_container(&c);
}

int main(void) {
    printf("Running BSP tests...\n");
    test_two_leaves();
    test_self_cycle();
    test_multi_cycle();
    test_shared_reference();
    test_out_of_bounds_child();
    test_invalid_root();
    test_truncated_image();
    test_null_image_helpers();
    test_visibility_predicate();
    test_leaf_cap();
    test_fault_inject_first();
    test_child_order_distance_bits();
    test_real_aztec();
    test_map7_reference();
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
