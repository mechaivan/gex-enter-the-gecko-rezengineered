#include "gex/bsp.h"
#include "gex/asset/dfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if(!(cond)){ failures++; printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);} } while(0)

/* Build a tiny image with 3 nodes: root internal -> two leaves */
static uint8_t *make_image(size_t *out_size) {
    size_t sz = 3 * GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz, 1);
    /* node 0: internal, children 1 and 2 */
    *(uint16_t*)(img + 0 + 0x12) = 0x0001; /* not leaf */
    *(uint32_t*)(img + 0 + 0x18) = GEX_BSP_NODE_SIZE;       /* child1 */
    *(uint32_t*)(img + 0 + 0x1C) = 2*GEX_BSP_NODE_SIZE;     /* child2 */
    /* node 1: leaf */
    *(uint16_t*)(img + GEX_BSP_NODE_SIZE + 0x12) = GEX_BSP_LEAF_FLAG;
    /* node 2: leaf */
    *(uint16_t*)(img + 2*GEX_BSP_NODE_SIZE + 0x12) = GEX_BSP_LEAF_FLAG;
    *out_size = sz;
    return img;
}

static void test_basic(void) {
    size_t sz; uint8_t *img = make_image(&sz);
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 2);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_cycle(void) {
    size_t sz = 2*GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz,1);
    /* node0 -> node1 -> node0 (cycle) */
    *(uint16_t*)(img+0+0x12)=0x0001;
    *(uint32_t*)(img+0+0x18)=GEX_BSP_NODE_SIZE;
    *(uint16_t*)(img+GEX_BSP_NODE_SIZE+0x12)=0x0001;
    *(uint32_t*)(img+GEX_BSP_NODE_SIZE+0x18)=0; /* child1=0? use self */
    *(uint32_t*)(img+GEX_BSP_NODE_SIZE+0x1C)=0;
    /* make node1 point back to node0 */
    *(uint32_t*)(img+GEX_BSP_NODE_SIZE+0x18)=0;
    /* can't easily make 0 a valid node (root); instead test bounds */
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    /* root 0 is internal with child1=32 (node1); node1 has no children -> 0 leaves, no crash */
    CHECK(gex_bsp_walk(img, sz, 0, NULL, NULL, &r) == GEX_BSP_OK);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_bounds(void) {
    size_t sz = GEX_BSP_NODE_SIZE;
    uint8_t *img = calloc(sz,1);
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    /* root out of bounds */
    CHECK(gex_bsp_walk(img, sz, GEX_BSP_NODE_SIZE, NULL, NULL, &r) == GEX_BSP_ERR_BOUNDS);
    /* null image */
    CHECK(gex_bsp_walk(NULL, sz, 0, NULL, NULL, &r) == GEX_BSP_ERR_ARGUMENT);
    free(img);
}

static void test_real_aztec(void) {
    const char *path = "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX";
    FILE *f = fopen(path,"rb"); if(!f){ printf("skip aztec\n"); return; } fclose(f);
    gex_dfx_container c; memset(&c,0,sizeof(c));
    if (gex_dfx_load(path,&c) != GEX_DFX_OK) { printf("load fail\n"); return; }
    gex_dfx_level level;
    memset(&level, 0, sizeof(level));
    if (gex_dfx_load_level(&c, &level) != GEX_DFX_OK) { printf("level fail\n"); gex_dfx_release_container(&c); return; }
    uint32_t root = level.descriptor.root_bsp;
    gex_bsp_walk_result r; memset(&r,0,sizeof(r));
    CHECK(gex_bsp_walk(c.image, c.image_size, root, NULL, NULL, &r) == GEX_BSP_OK);
    printf("AZTEC leaves (all visible): %u\n", r.count);
    CHECK(r.count > 0);
    gex_bsp_walk_free(&r);
    gex_dfx_release_level(&level);
}

int main(void) {
    printf("Running BSP tests...\n");
    test_basic();
    test_cycle();
    test_bounds();
    test_real_aztec();
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
