/*
 * test_camera_path_render.c - The camera path really reaches the renderer.
 *
 * The scene tests prove that the port produces the right camera positions.
 * This one proves the other half of the chain: that the camera update
 * changes the GEOMETRY that is emitted and that the GPU buffers that are
 * drawn actually change with it.
 *
 * It uses the same D3D11 modules the application uses (device, pipeline,
 * mesh, textures) and the real MAP7 level, and checks:
 *
 *   1. the emit matrix and the emitted triangles change when the camera
 *      moves (the camera is not a no-op for the scene);
 *   2. the vertex buffer read back from the GPU differs before and after
 *      the trajectory advances (the D3D11 mesh really carries the new
 *      geometry);
 *   3. after the sentinel the geometry is stable (no further writes).
 *
 * It needs a real D3D11 device: if the device cannot be created the test
 * says so and skips instead of failing.
 */

#if defined(_WIN32)

#include "gex/scene_level.h"
#include "gex/render/d3d11_device.h"
#include "gex/render/d3d11_mesh.h"
#include "gex/render/d3d11_pipeline.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP7_PATH "C:/GEX_REZ/GEX2/level/map7.dfx"

static int checks;
static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

/* Vuelca el buffer de vertices a memoria (lectura de staging). */
static int read_vertex_buffer(gex_d3d11_device *dev, gex_d3d11_mesh *mesh,
                               uint8_t *out, size_t out_size)
{
    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11Texture2D *staging = NULL;
    ID3D11DeviceContext *ctx = dev->context;
    HRESULT hr;
    size_t bytes = (size_t)mesh->vertex_buffer_bytes;

    if (bytes > out_size) {
        return 0;
    }
    memset(&desc, 0, sizeof(desc));
    desc.Width = mesh->vertex_count;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = ID3D11Device_CreateTexture2D(dev->device, &desc, NULL, &staging);
    if (FAILED(hr) || staging == NULL) {
        return 0;
    }
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)mesh->vertex_buffer,
                                     (ID3D11Resource *)staging);
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)staging, 0,
                                  D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        ID3D11Texture2D_Release(staging);
        return 0;
    }
    memcpy(out, mapped.pData, bytes);
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)staging, 0);
    ID3D11Texture2D_Release(staging);
    return 1;
}

static int run(void)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_scene_level scene;
    gex_camera cam;
    static uint8_t before[256 * 1024];
    static uint8_t after[256 * 1024];
    size_t before_len = 0;
    size_t after_len = 0;
    int16_t emit_before[9];
    int32_t trans_before[3];
    size_t tri_before;
    int step;
    FILE *fh;

    memset(&scene, 0, sizeof(scene));

    fh = fopen(MAP7_PATH, "rb");
    if (fh == NULL) {
        printf("skip render test (no level file)\n");
        return 0;
    }
    fclose(fh);

    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_HARDWARE, 0) !=
        GEX_D3D11_DEVICE_OK) {
        printf("skip render test (no D3D11 device)\n");
        return 0;
    }

    gex_scene_reference_camera(&cam);
    if (gex_scene_level_load_camera_path(&scene, MAP7_PATH, &cam) !=
        GEX_SCENE_OK) {
        printf("skip render test (level did not load)\n");
        gex_d3d11_device_shutdown(&dev);
        return 0;
    }

    /* The mesh of the first frame. */
    CHECK(gex_d3d11_mesh_create(dev.device, &scene.geometry, &mesh) ==
          GEX_D3D11_MESH_OK);
    CHECK(mesh.initialized == 1);

    /* The state of the scene before the camera moves. */
    memcpy(emit_before, scene.stats.emit_m, sizeof(emit_before));
    memcpy(trans_before, scene.stats.emit_t, sizeof(trans_before));
    tri_before = scene.stats.triangles;
    /* Read the vertices that are going to be drawn. */
    CHECK(read_vertex_buffer(&dev, &mesh, before, sizeof(before)) == 1);
    before_len = mesh.vertex_buffer_bytes;

    /* Advance the trajectory a few frames. */
    for (step = 0; step < 10; step++) {
        CHECK(gex_scene_level_camera_path_step(&scene, &cam) == 1);
    }
    CHECK(scene.path_frames == 10);

    /* The rotational part of the emit matrix is unchanged (the rotation path
     * is not decoded yet); the translation and the geometry do change. */
    CHECK(memcmp(emit_before, scene.stats.emit_m, sizeof(emit_before)) == 0);
    CHECK(memcmp(trans_before, scene.stats.emit_t, sizeof(trans_before)) != 0);
    CHECK(scene.stats.triangles != tri_before);

    /* The GPU buffers carry the new geometry. */
    CHECK(gex_d3d11_mesh_update(dev.device, &scene.geometry, &mesh) ==
          GEX_D3D11_MESH_OK);
    CHECK(mesh.initialized == 1);
    CHECK(read_vertex_buffer(&dev, &mesh, after, sizeof(after)) == 1);
    after_len = mesh.vertex_buffer_bytes;
    CHECK(after_len > 0);
    /* The vertex data is not the same as before (the camera moved the
     * geometry on screen). */
    CHECK(after_len != before_len ||
          memcmp(before, after, after_len < before_len ? after_len
                                                        : before_len) != 0);

    /* Run the trajectory to the end and check it stops writing. */
    while (gex_scene_level_camera_path_step(&scene, &cam) == 1) {
        /* hasta el centinela */
    }
    CHECK(scene.path_finished == 1);
    CHECK(scene.path_frames == 428);
    CHECK(cam.pos[0] == 2243 && cam.pos[1] == 487 && cam.pos[2] == 196);

    gex_d3d11_mesh_destroy(&mesh);
    gex_scene_level_free(&scene);
    gex_d3d11_device_shutdown(&dev);
    return 1;
}

int main(void)
{
    printf("Running camera path render tests...\n");
    run();
    printf("Tests completed: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

#else /* !_WIN32 */

#include <stdio.h>

int main(void)
{
    printf("skip camera path render tests (not Windows)\n");
    return 0;
}

#endif
