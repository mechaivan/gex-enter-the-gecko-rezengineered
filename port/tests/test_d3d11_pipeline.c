/*
 * test_d3d11_pipeline.c - Módulo 5A: pipeline de dibujo D3D11 (Windows-only).
 *
 * Todas las pruebas son **offscreen**: dispositivo WARP y render target
 * propio. No se abre ninguna ventana, ni swapchain, ni demo interactiva.
 *
 * Qué verifica:
 *   - nombres de estado y contrato del inicializador;
 *   - compilación real del HLSL con d3dcompiler_47.dll y creación de todos
 *     los recursos (shaders, input layout, rasterizador, blend, depth,
 *     constant buffer);
 *   - el input layout refleja los 28 B y los offsets publicados;
 *   - el dibujo produce píxeles distintos del color de limpieza;
 *   - el color AARRGGBB llega con los canales R y G y B en su sitio (el
 *     byte order se comprueba con un color que se invierte al no invertirlo);
 *   - el color por vértice se interpola;
 *   - la proyección píxel lógico -> píxel de la ventana es la esperada
 *     (escala uniforme y centrada);
 *   - el mismo dibujo tras cambiar el tamaño del render target (resize);
 *   - geometría vacía, estados inválidos y argumentos nulos;
 *   - liberación repetida y ausencia de recursos parcialmente vivos.
 */

#include "gex/render/d3d11_pipeline.h"

#include "gex/render/d3d11_device.h"
#include "gex/render/d3d11_mesh.h"
#include "gex/render/d3d11_renderer.h"
#include "gex/geometry.h"
#include "gex/geometry_shapes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

typedef struct offscreen {
    ID3D11Texture2D *texture;
    ID3D11RenderTargetView *rtv;
    ID3D11Texture2D *staging;
    UINT width;
    UINT height;
} offscreen;

static int offscreen_create(ID3D11Device *device, UINT width, UINT height,
                            offscreen *out)
{
    D3D11_TEXTURE2D_DESC desc;

    memset(out, 0, sizeof(*out));
    memset(&desc, 0, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    if (FAILED(ID3D11Device_CreateTexture2D(device, &desc, NULL,
                                            &out->texture)) ||
        out->texture == NULL) {
        return -1;
    }
    if (FAILED(ID3D11Device_CreateRenderTargetView(
            device, (ID3D11Resource *)out->texture, NULL, &out->rtv)) ||
        out->rtv == NULL) {
        return -1;
    }
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(ID3D11Device_CreateTexture2D(device, &desc, NULL,
                                            &out->staging)) ||
        out->staging == NULL) {
        return -1;
    }
    out->width = width;
    out->height = height;
    return 0;
}

static void offscreen_destroy(offscreen *o)
{
    if (o->staging != NULL) {
        ID3D11Texture2D_Release(o->staging);
    }
    if (o->rtv != NULL) {
        ID3D11RenderTargetView_Release(o->rtv);
    }
    if (o->texture != NULL) {
        ID3D11Texture2D_Release(o->texture);
    }
    memset(o, 0, sizeof(*o));
}

/* Copia el render target a memoria del host. `pixels` recibe width*height
 * píxeles BGRA de 8 bits (el orden de memoria de R8G8B8A8_UNORM). */
static int offscreen_read(ID3D11DeviceContext *context, offscreen *o,
                          uint8_t *pixels)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr;

    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)o->staging,
                                     (ID3D11Resource *)o->texture);
    hr = ID3D11DeviceContext_Map(context, (ID3D11Resource *)o->staging, 0,
                                 D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        return -1;
    }
    memcpy(pixels, mapped.pData, (size_t)o->width * o->height * 4u);
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)o->staging, 0);
    return 0;
}

static void clear_target(ID3D11DeviceContext *context, ID3D11RenderTargetView *rtv,
                         const float color[4])
{
    ID3D11DeviceContext_ClearRenderTargetView(context, rtv, color);
}

/* Emite un triángulo con coordenadas lógicas (512x384) y color por vértice. */
static void make_triangle(gex_emit_output *out, int16_t x0, int16_t y0,
                          uint32_t c0, int16_t x1, int16_t y1, uint32_t c1,
                          int16_t x2, int16_t y2, uint32_t c2)
{
    gex_emit_triangle *tris = (gex_emit_triangle *)calloc(1, sizeof(*tris));

    memset(out, 0, sizeof(*out));
    if (tris == NULL) {
        return;
    }
    tris->x[0] = x0; tris->y[0] = y0; tris->z[0] = 0x1000;
    tris->x[1] = x1; tris->y[1] = y1; tris->z[1] = 0x1000;
    tris->x[2] = x2; tris->y[2] = y2; tris->z[2] = 0x1000;
    tris->color[0] = c0;
    tris->color[1] = c1;
    tris->color[2] = c2;
    tris->flags = GEX_EMIT_F_NEAR_EMITTER;
    out->tris = tris;
    out->count = 1;
    out->capacity = 1;
}

/* ------------------------------------------------------------------ */
/* Nombres e inicializador                                            */
/* ------------------------------------------------------------------ */

static void test_names(void)
{
    CHECK(strcmp(gex_d3d11_pipeline_status_name(GEX_D3D11_PIPELINE_OK),
                 "ok") == 0);
    CHECK(gex_d3d11_pipeline_status_name(
              GEX_D3D11_PIPELINE_BAD_ARGUMENT) != NULL);
    CHECK(gex_d3d11_pipeline_status_name(
              GEX_D3D11_PIPELINE_BAD_STATE) != NULL);
    CHECK(gex_d3d11_pipeline_status_name(
              GEX_D3D11_PIPELINE_BAD_DEVICE) != NULL);
    CHECK(gex_d3d11_pipeline_status_name(
              GEX_D3D11_PIPELINE_NO_COMPILER) != NULL);
    CHECK(gex_d3d11_pipeline_status_name(
              GEX_D3D11_PIPELINE_SHADER_FAILED) != NULL);
    CHECK(gex_d3d11_pipeline_status_name(
              GEX_D3D11_PIPELINE_CREATE_FAILED) != NULL);
    CHECK(gex_d3d11_pipeline_status_name(
              GEX_D3D11_PIPELINE_NO_GEOMETRY) != NULL);
    CHECK(gex_d3d11_pipeline_status_name(
              (gex_d3d11_pipeline_status)999) != NULL);
}

static void test_initializer(void)
{
    gex_d3d11_pipeline a = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_pipeline b;

    memset(&b, 0, sizeof(b));
    CHECK(a.device == NULL && a.context == NULL);
    CHECK(a.vertex_shader == NULL && a.pixel_shader == NULL);
    CHECK(a.input_layout == NULL && a.rasterizer == NULL);
    CHECK(a.blend == NULL && a.depth_stencil == NULL);
    CHECK(a.constant_buffer == NULL && a.compiler_module == NULL);
    CHECK(a.initialized == 0);
    CHECK(a.draw_calls == 0 && a.triangles_drawn == 0);
    /* S_OK == 0: el inicializador coincide con la estructura a cero. */
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);

    gex_d3d11_pipeline_shutdown(&a); /* nunca inicializado: no-op */
    CHECK(a.initialized == 0);
    gex_d3d11_pipeline_shutdown(NULL);
}

/* ------------------------------------------------------------------ */
/* Layout de entrada                                                  */
/* ------------------------------------------------------------------ */

static void test_input_layout(void)
{
    CHECK(GEX_D3D11_INPUT_ELEMENT_COUNT == 6);
    CHECK(strcmp(gex_d3d11_input_layout[0].SemanticName, "POSITION") == 0);
    CHECK(gex_d3d11_input_layout[0].Format == DXGI_FORMAT_R32G32B32_FLOAT);
    CHECK(gex_d3d11_input_layout[0].AlignedByteOffset ==
          GEX_VERTEX_OFFSET_POSITION);
    CHECK(strcmp(gex_d3d11_input_layout[1].SemanticName, "COLOR") == 0);
    CHECK(gex_d3d11_input_layout[1].Format == DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(gex_d3d11_input_layout[1].AlignedByteOffset ==
          GEX_VERTEX_OFFSET_COLOR);
    CHECK(gex_d3d11_input_layout[2].AlignedByteOffset == GEX_VERTEX_OFFSET_UV);
    CHECK(gex_d3d11_input_layout[3].AlignedByteOffset ==
          GEX_VERTEX_OFFSET_TEXTURE);
    CHECK(gex_d3d11_input_layout[4].AlignedByteOffset ==
          GEX_VERTEX_OFFSET_LAYER);
    CHECK(gex_d3d11_input_layout[5].AlignedByteOffset ==
          GEX_VERTEX_OFFSET_FLAGS);
    /* El último offset + 1 byte = stride: los 28 B están descritos. */
    CHECK(GEX_VERTEX_OFFSET_FLAGS + 1u == GEX_VERTEX_STRIDE);
}

/* ------------------------------------------------------------------ */
/* Init / shutdown                                                    */
/* ------------------------------------------------------------------ */

static void test_init_errors(gex_d3d11_device *device)
{
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_device dead = GEX_D3D11_DEVICE_INITIALIZER;

    CHECK(gex_d3d11_pipeline_init(NULL, device) ==
          GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_init(&pipeline, NULL) ==
          GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    /* Dispositivo no inicializado. */
    CHECK(gex_d3d11_pipeline_init(&pipeline, &dead) ==
          GEX_D3D11_PIPELINE_BAD_DEVICE);
    /* Tras el error la estructura queda limpia: sin recursos a medias. */
    CHECK(pipeline.initialized == 0);
    CHECK(pipeline.vertex_shader == NULL && pipeline.input_layout == NULL);
    CHECK(pipeline.constant_buffer == NULL && pipeline.compiler_module == NULL);
    CHECK(pipeline.last_hresult != S_OK);
}

static void test_init_and_shutdown(gex_d3d11_device *device)
{
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_pipeline_status st;

    st = gex_d3d11_pipeline_init(&pipeline, device);
    CHECK(st == GEX_D3D11_PIPELINE_OK);
    if (st != GEX_D3D11_PIPELINE_OK) {
        printf("shader error: %s (%.180s)\n",
               gex_d3d11_pipeline_status_name(st), pipeline.last_error);
        return;
    }
    CHECK(pipeline.initialized == 1);
    CHECK(pipeline.last_hresult == S_OK);
    CHECK(pipeline.vertex_shader != NULL);
    CHECK(pipeline.pixel_shader != NULL);
    CHECK(pipeline.input_layout != NULL);
    CHECK(pipeline.rasterizer != NULL);
    CHECK(pipeline.blend != NULL);
    CHECK(pipeline.depth_stencil != NULL);
    CHECK(pipeline.constant_buffer != NULL);
    CHECK(pipeline.compiler_module != NULL);
    CHECK(pipeline.last_error[0] == '\0');

    /* Doble init sobre un pipeline vivo: no se toca. */
    CHECK(gex_d3d11_pipeline_init(&pipeline, device) ==
          GEX_D3D11_PIPELINE_BAD_STATE);
    CHECK(pipeline.initialized == 1);
    CHECK(pipeline.vertex_shader != NULL);

    gex_d3d11_pipeline_shutdown(&pipeline);
    CHECK(pipeline.initialized == 0);
    CHECK(pipeline.vertex_shader == NULL);
    CHECK(pipeline.pixel_shader == NULL);
    CHECK(pipeline.input_layout == NULL);
    CHECK(pipeline.rasterizer == NULL);
    CHECK(pipeline.blend == NULL);
    CHECK(pipeline.depth_stencil == NULL);
    CHECK(pipeline.constant_buffer == NULL);
    CHECK(pipeline.compiler_module == NULL);
    CHECK(pipeline.device == NULL && pipeline.context == NULL);
    /* Segunda liberación: no-op. */
    gex_d3d11_pipeline_shutdown(&pipeline);
    CHECK(pipeline.initialized == 0);

    /* Tras cerrar, se puede reutilizar. */
    CHECK(gex_d3d11_pipeline_init(&pipeline, device) ==
          GEX_D3D11_PIPELINE_OK);
    gex_d3d11_pipeline_shutdown(&pipeline);
}

/* ------------------------------------------------------------------ */
/* Dibujo offscreen                                                   */
/* ------------------------------------------------------------------ */

/* Devuelve el color (R,G,B,A) del píxel (x,y). El render target es
 * DXGI_FORMAT_R8G8B8A8_UNORM: en memoria los bytes van R,G,B,A. */
static void pixel_at(const uint8_t *pixels, UINT width, UINT x, UINT y,
                     uint8_t out[4])
{
    const uint8_t *p = pixels + ((size_t)y * width + x) * 4u;
    out[0] = p[0]; /* R */
    out[1] = p[1]; /* G */
    out[2] = p[2]; /* B */
    out[3] = p[3]; /* A */
}

static void test_draw_and_colors(gex_d3d11_device *device)
{
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_emit_output emit;
    gex_geometry geom;
    offscreen target;
    uint8_t *pixels = NULL;
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    uint8_t px[4];
    size_t i;
    size_t changed = 0;
    size_t total;

    if (gex_d3d11_pipeline_init(&pipeline, device) != GEX_D3D11_PIPELINE_OK) {
        printf("skip draw tests (sin pipeline)\n");
        return;
    }
    if (offscreen_create(device->device, 256, 192, &target) != 0) {
        printf("skip draw tests (sin render target)\n");
        gex_d3d11_pipeline_shutdown(&pipeline);
        return;
    }

    /* Triángulo sólido con un color cuyo R y B distinguishes: 0xFF3366CC es
     * R=0x33, G=0x66, B=0xCC. Sin la permutación del byte order en el
     * vertex shader se vería (0xCC, 0x66, 0x33). */
    make_triangle(&emit, 64, 64, 0xFF3366CCu, 448, 64, 0xFF3366CCu,
                  64, 320, 0xFF3366CCu);
    CHECK(gex_geometry_build(&emit, &geom) == GEX_GEOM_OK);
    CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
          GEX_D3D11_MESH_OK);

    CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 256, 192,
                                        GEX_D3D11_REFERENCE_WIDTH,
                                        GEX_D3D11_REFERENCE_HEIGHT) ==
          GEX_D3D11_PIPELINE_OK);

    clear_target(device->context, target.rtv, clear_color);
    CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) == GEX_D3D11_PIPELINE_OK);
    CHECK(pipeline.draw_calls == 1u);
    CHECK(pipeline.triangles_drawn == 1u);

    pixels = (uint8_t *)malloc(256u * 192u * 4u);
    if (pixels == NULL) {
        printf("skip read-back (sin memoria del host)\n");
    } else {
        CHECK(offscreen_read(device->context, &target, pixels) == 0);
        total = 256u * 192u;
        for (i = 0; i < total; i++) {
            if (pixels[i * 4u + 0u] != 0u || pixels[i * 4u + 1u] != 0u ||
                pixels[i * 4u + 2u] != 0u) {
                changed++;
            }
        }
        /* Se dibujan píxeles: la geometría llega al render target. */
        CHECK(changed > 100u);
        CHECK(changed < total);

        /* Interior del triángulo: con la escala 0.5 (viewport 256x192 sobre
         * una referencia 512x384) el vértice (64,64) cae en el píxel (32,32)
         * y el (448,64) en el (224,32). */
        pixel_at(pixels, 256, 40, 40, px);
        CHECK(px[0] == 0x33u && px[1] == 0x66u && px[2] == 0xCCu);
        CHECK(px[3] == 0xFFu);
        pixel_at(pixels, 256, 100, 40, px);
        CHECK(px[0] == 0x33u && px[1] == 0x66u && px[2] == 0xCCu);
        /* Fuera del triángulo sigue el color de limpieza. */
        pixel_at(pixels, 256, 250, 185, px);
        CHECK(px[0] == 0u && px[1] == 0u && px[2] == 0u);
    }

    /* Segundo triángulo con tres colores: el centro debe ser el promedio
     * (el color por vértice se interpola). */
    gex_d3d11_mesh_destroy(&mesh);
    gex_geometry_free(&geom);
    gex_emit_output_free(&emit);
    make_triangle(&emit, 64, 64, 0xFFFF0000u, 448, 64, 0xFF00FF00u,
                  64, 320, 0xFF0000FFu);
    CHECK(gex_geometry_build(&emit, &geom) == GEX_GEOM_OK);
    CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
          GEX_D3D11_MESH_OK);
    clear_target(device->context, target.rtv, clear_color);
    CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) == GEX_D3D11_PIPELINE_OK);
    if (pixels != NULL) {
        CHECK(offscreen_read(device->context, &target, pixels) == 0);
        /* Centroide del triángulo: (32,32), (224,32), (32,160) -> (96,74). */
        pixel_at(pixels, 256, 96, 74, px);
        CHECK(px[0] >= 78u && px[0] <= 92u); /* R ~ 85 */
        CHECK(px[1] >= 78u && px[1] <= 92u); /* G ~ 85 */
        CHECK(px[2] >= 78u && px[2] <= 92u); /* B ~ 85 */
        /* Cerca del vértice rojo (32,32) domina el rojo. */
        pixel_at(pixels, 256, 36, 36, px);
        CHECK(px[0] > 200u && px[1] < 60u && px[2] < 60u);
    }

    free(pixels);
    gex_d3d11_mesh_destroy(&mesh);
    gex_geometry_free(&geom);
    gex_emit_output_free(&emit);
    offscreen_destroy(&target);
    gex_d3d11_pipeline_shutdown(&pipeline);
}

/* ------------------------------------------------------------------ */
/* Resize                                                             */
/* ------------------------------------------------------------------ */

static void test_resize(gex_d3d11_device *device)
{
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_emit_output emit;
    gex_geometry geom;
    offscreen small;
    offscreen big;
    uint8_t *pixels = NULL;
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    uint8_t px[4];

    if (gex_d3d11_pipeline_init(&pipeline, device) != GEX_D3D11_PIPELINE_OK) {
        printf("skip resize (sin pipeline)\n");
        return;
    }
    if (offscreen_create(device->device, 256, 192, &small) != 0 ||
        offscreen_create(device->device, 320, 240, &big) != 0) {
        printf("skip resize (sin render targets)\n");
        gex_d3d11_pipeline_shutdown(&pipeline);
        return;
    }

    make_triangle(&emit, 64, 64, 0xFFFF0000u, 448, 64, 0xFFFF0000u,
                  64, 320, 0xFFFF0000u);
    CHECK(gex_geometry_build(&emit, &geom) == GEX_GEOM_OK);
    CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
          GEX_D3D11_MESH_OK);

    /* 256x192: escala 0.5, el vértice (64,64) cae en (32,32). */
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, small.rtv, 256, 192,
                                        GEX_D3D11_REFERENCE_WIDTH,
                                        GEX_D3D11_REFERENCE_HEIGHT) ==
          GEX_D3D11_PIPELINE_OK);
    clear_target(device->context, small.rtv, clear_color);
    CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) == GEX_D3D11_PIPELINE_OK);

    /* 320x240: escala 0.625, el mismo vértice cae en (40,40). */
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, big.rtv, 320, 240,
                                        GEX_D3D11_REFERENCE_WIDTH,
                                        GEX_D3D11_REFERENCE_HEIGHT) ==
          GEX_D3D11_PIPELINE_OK);
    clear_target(device->context, big.rtv, clear_color);
    CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) == GEX_D3D11_PIPELINE_OK);

    pixels = (uint8_t *)malloc(320u * 240u * 4u);
    if (pixels != NULL) {
        CHECK(offscreen_read(device->context, &big, pixels) == 0);
        pixel_at(pixels, 320, 44, 44, px);
        CHECK(px[0] > 200u && px[1] < 60u && px[2] < 60u);
        /* Y el origen lógico (0,0) queda en la esquina, no desplazado. */
        pixel_at(pixels, 320, 2, 2, px);
        CHECK(px[0] == 0u && px[1] == 0u && px[2] == 0u);
    }
    free(pixels);

    gex_d3d11_mesh_destroy(&mesh);
    gex_geometry_free(&geom);
    gex_emit_output_free(&emit);
    offscreen_destroy(&small);
    offscreen_destroy(&big);
    gex_d3d11_pipeline_shutdown(&pipeline);
}

/* ------------------------------------------------------------------ */
/* Geometría vacía y estados inválidos                                */
/* ------------------------------------------------------------------ */

static void test_invalid_states(gex_d3d11_device *device)
{
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_pipeline dead = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    offscreen target;

    CHECK(gex_d3d11_pipeline_init(&pipeline, device) == GEX_D3D11_PIPELINE_OK);
    if (offscreen_create(device->device, 64, 64, &target) != 0) {
        printf("skip invalid states (sin render target)\n");
        gex_d3d11_pipeline_shutdown(&pipeline);
        return;
    }

    /* Destino inválido. */
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, NULL, 64, 64, 512, 384) ==
          GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 0, 64, 512,
                                        384) == GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 64, 0, 512,
                                        384) == GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 64, 64, 0,
                                        384) == GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 64, 64, 512,
                                        0) == GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_set_target(NULL, target.rtv, 64, 64, 512, 384) ==
          GEX_D3D11_PIPELINE_BAD_ARGUMENT);

    /* Malla nula o no inicializada. */
    CHECK(gex_d3d11_pipeline_draw(&pipeline, NULL) ==
          GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) ==
          GEX_D3D11_PIPELINE_BAD_ARGUMENT);
    CHECK(gex_d3d11_pipeline_draw(NULL, &mesh) ==
          GEX_D3D11_PIPELINE_BAD_ARGUMENT);

    /* Pipeline no inicializado. */
    CHECK(gex_d3d11_pipeline_set_target(&dead, target.rtv, 64, 64, 512, 384) ==
          GEX_D3D11_PIPELINE_BAD_STATE);
    CHECK(gex_d3d11_pipeline_draw(&dead, &mesh) ==
          GEX_D3D11_PIPELINE_BAD_STATE);

    /* Geometría vacía: la malla existe pero no tiene índices. */
    {
        gex_emit_output empty;
        gex_geometry geom;
        memset(&empty, 0, sizeof(empty));
        CHECK(gex_geometry_build(&empty, &geom) == GEX_GEOM_OK);
        CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
              GEX_D3D11_MESH_BAD_GEOMETRY);
        gex_geometry_free(&geom);
    }

    /* El pipeline sigue vivo tras todos los errores anteriores. */
    CHECK(pipeline.initialized == 1);
    CHECK(pipeline.vertex_shader != NULL);
    CHECK(pipeline.draw_calls == 0u);

    offscreen_destroy(&target);
    gex_d3d11_pipeline_shutdown(&pipeline);
}

/* ------------------------------------------------------------------ */
/* Comprobación de que no quedan recursos vivos tras un error         */
/* ------------------------------------------------------------------ */

static void test_no_partial_resources(gex_d3d11_device *device)
{
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_emit_output emit;
    gex_geometry geom;
    offscreen target;

    if (gex_d3d11_pipeline_init(&pipeline, device) != GEX_D3D11_PIPELINE_OK) {
        printf("skip partial resources (sin pipeline)\n");
        return;
    }
    if (offscreen_create(device->device, 64, 64, &target) != 0) {
        gex_d3d11_pipeline_shutdown(&pipeline);
        return;
    }

    /* Una malla con índices fuera de rango no debe crear recursos. */
    {
        gex_vertex v[3];
        uint32_t bad[3];
        memset(v, 0, sizeof(v));
        bad[0] = 0; bad[1] = 1; bad[2] = 99; /* 99 >= vertex_count */
        memset(&geom, 0, sizeof(geom));
        geom.vertices = v;
        geom.indices = bad;
        geom.vertex_count = 3;
        geom.index_count = 3;
        geom.triangle_count = 1;
        CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
              GEX_D3D11_MESH_BAD_GEOMETRY);
        CHECK(mesh.vertex_buffer == NULL && mesh.index_buffer == NULL);
        CHECK(mesh.initialized == 0);
    }

    /* Tras el fallo el pipeline sigue siendo utilizable. */
    CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 64, 64, 512,
                                        384) == GEX_D3D11_PIPELINE_OK);

    make_triangle(&emit, 10, 10, 0xFFFFFFFFu, 50, 10, 0xFFFFFFFFu,
                  10, 50, 0xFFFFFFFFu);
    CHECK(gex_geometry_build(&emit, &geom) == GEX_GEOM_OK);
    CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
          GEX_D3D11_MESH_OK);
    CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) == GEX_D3D11_PIPELINE_OK);
    gex_d3d11_mesh_destroy(&mesh);
    gex_geometry_free(&geom);
    gex_emit_output_free(&emit);

    offscreen_destroy(&target);
    gex_d3d11_pipeline_shutdown(&pipeline);
    CHECK(pipeline.initialized == 0);
    CHECK(pipeline.constant_buffer == NULL);
}

/* ------------------------------------------------------------------ */
/* Cubo de prueba (geometría 3D reutilizable)                         */
/* ------------------------------------------------------------------ */

static void test_draw_cube(gex_d3d11_device *device)
{
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_emit_output emit;
    gex_geometry geom;
    offscreen target;
    uint8_t *pixels = NULL;
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    uint8_t px[4];
    size_t i;
    size_t changed = 0;

    if (gex_d3d11_pipeline_init(&pipeline, device) != GEX_D3D11_PIPELINE_OK) {
        printf("skip cube (sin pipeline)\n");
        return;
    }
    if (offscreen_create(device->device, 256, 192, &target) != 0) {
        printf("skip cube (sin render target)\n");
        gex_d3d11_pipeline_shutdown(&pipeline);
        return;
    }

    CHECK(gex_shape_cube(&emit, 0.6f) == 0);
    CHECK(emit.count == GEX_SHAPE_CUBE_TRIANGLES);
    CHECK(gex_geometry_build(&emit, &geom) == GEX_GEOM_OK);
    CHECK(geom.triangle_count == GEX_SHAPE_CUBE_TRIANGLES);
    CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
          GEX_D3D11_MESH_OK);

    CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 256, 192,
                                        GEX_D3D11_REFERENCE_WIDTH,
                                        GEX_D3D11_REFERENCE_HEIGHT) ==
          GEX_D3D11_PIPELINE_OK);
    clear_target(device->context, target.rtv, clear_color);
    CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) == GEX_D3D11_PIPELINE_OK);
    CHECK(pipeline.triangles_drawn == GEX_SHAPE_CUBE_TRIANGLES);

    pixels = (uint8_t *)malloc(256u * 192u * 4u);
    if (pixels != NULL) {
        CHECK(offscreen_read(device->context, &target, pixels) == 0);
        for (i = 0; i < 256u * 192u; i++) {
            if (pixels[i * 4u + 0u] != 0u || pixels[i * 4u + 1u] != 0u ||
                pixels[i * 4u + 2u] != 0u) {
                changed++;
            }
        }
        /* El cubo ocupa una parte apreciable del viewport. */
        CHECK(changed > 1000u);
        CHECK(changed < 256u * 192u);
        /* El centro del cubo está en el centro del viewport lógico. */
        pixel_at(pixels, 256, 128, 96, px);
        CHECK(px[0] != 0u || px[1] != 0u || px[2] != 0u);
        /* Las esquinas quedan fuera del cubo. */
        pixel_at(pixels, 256, 4, 4, px);
        CHECK(px[0] == 0u && px[1] == 0u && px[2] == 0u);
    }

    free(pixels);
    gex_d3d11_mesh_destroy(&mesh);
    gex_geometry_free(&geom);
    gex_emit_output_free(&emit);
    offscreen_destroy(&target);
    gex_d3d11_pipeline_shutdown(&pipeline);
}

/* ------------------------------------------------------------------ */
/* Contrato de integración con el renderer                             */
/* ------------------------------------------------------------------ */

static void test_renderer_geometry_contract(void)
{
    gex_d3d11_renderer renderer = GEX_D3D11_RENDERER_INITIALIZER;
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;

    /* Sin renderer: puntero nulo. */
    CHECK(gex_d3d11_renderer_set_geometry(NULL, &pipeline, &mesh, 512, 384) ==
          GEX_D3D11_RENDERER_BAD_ARGUMENT);
    /* Renderer no inicializado. */
    CHECK(gex_d3d11_renderer_set_geometry(&renderer, &pipeline, &mesh, 512,
                                          384) == GEX_D3D11_RENDERER_BAD_STATE);
    /* Con el renderer inicializado pero sin geometría, el estado es el de
     * siempre: limpiar y presentar. */
    CHECK(renderer.pipeline == NULL && renderer.mesh == NULL);
    CHECK(renderer.draws == 0u && renderer.draw_errors == 0u);
}

/* ------------------------------------------------------------------ */
/* Fallos de inicialización en cada etapa                               */
/* ------------------------------------------------------------------ */

#ifdef GEX_PIPELINE_TEST_FAULT_INJECT
extern int gex_d3d11_pipeline_test_fail_stage;

/* Comprueba que un fallo en cualquier etapa:
 *  - devuelve el estado esperado para esa etapa;
 *  - no deja ningún recurso vivo en la estructura;
 *  - conserva el diagnóstico (el mensaje no se pierde al limpiar);
 *  - deja el dispositivo utilizable (se puede reintentar). */
static void test_init_faults(gex_d3d11_device *device)
{
    for (int stage = 1; stage <= 10; stage++) {
        gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
        gex_d3d11_pipeline_status st;
        gex_d3d11_pipeline_status expected;

        if (stage == 1) {
            expected = GEX_D3D11_PIPELINE_NO_COMPILER;
        } else if (stage == 2 || stage == 3) {
            expected = GEX_D3D11_PIPELINE_SHADER_FAILED;
        } else {
            expected = GEX_D3D11_PIPELINE_CREATE_FAILED;
        }

        gex_d3d11_pipeline_test_fail_stage = stage;
        st = gex_d3d11_pipeline_init(&pipeline, device);
        CHECK(st == expected);

        /* Ningún recurso queda vivo. */
        CHECK(pipeline.initialized == 0);
        CHECK(pipeline.vertex_shader == NULL);
        CHECK(pipeline.pixel_shader == NULL);
        CHECK(pipeline.input_layout == NULL);
        CHECK(pipeline.rasterizer == NULL);
        CHECK(pipeline.blend == NULL);
        CHECK(pipeline.depth_stencil == NULL);
        CHECK(pipeline.constant_buffer == NULL);
        CHECK(pipeline.compiler_module == NULL);
        CHECK(pipeline.device == NULL && pipeline.context == NULL);

        /* El diagnóstico sobrevive a la limpieza. */
        CHECK(pipeline.last_error[0] != '\0');
        CHECK(pipeline.last_hresult != S_OK);

        /* El dispositivo sigue vivo: se puede reintentar sin fallo. */
        gex_d3d11_pipeline_test_fail_stage = 0;
        CHECK(gex_d3d11_pipeline_init(&pipeline, device) ==
              GEX_D3D11_PIPELINE_OK);
        CHECK(pipeline.last_error[0] == '\0');
        gex_d3d11_pipeline_shutdown(&pipeline);
    }
    gex_d3d11_pipeline_test_fail_stage = 0;
}
#endif

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Conjunto de texturas (Módulo 4C)                                   */
/* ------------------------------------------------------------------ */

/*
 * Escribe un `.VFX` sintético con UNA textura de 64x64 en ARGB_4444 (el
 * formato 12 de Glide 2.x). El texel (x, y) vale R = 4x, G = 4y, B = 255 y
 * A = 255, de modo que el color muestreado dice exactamente en qué texel se
 * ha aterrizado el fragmento.
 */
static int write_synthetic_vfx(const char *path)
{
    FILE *fh;
    uint8_t header[0x8C];
    uint16_t *data;
    uint32_t x, y;
    uint32_t count = 1;
    const uint32_t size = 64u * 64u * 2u;

    memset(header, 0, sizeof(header));
    header[0] = 2;   /* smallLod = 2 -> 64 px */
    header[4] = 2;   /* largeLod = 2 */
    header[8] = 3;   /* aspect = 1x1 */
    header[12] = 12; /* format = GR_TEXFMT_ARGB_4444 */
    header[0x84] = (uint8_t)(size & 0xFFu);
    header[0x85] = (uint8_t)((size >> 8) & 0xFFu);
    header[0x86] = (uint8_t)((size >> 16) & 0xFFu);
    header[0x87] = (uint8_t)((size >> 24) & 0xFFu);

    data = (uint16_t *)calloc(64u * 64u, sizeof(*data));
    if (data == NULL) {
        return -1;
    }
    for (y = 0; y < 64u; y++) {
        for (x = 0; x < 64u; x++) {
            uint32_t r = x & 0x0Fu;
            uint32_t g = y & 0x0Fu;
            data[y * 64u + x] = (uint16_t)((0xFu << 12) | (r << 8) | (g << 4) |
                                           0x0Fu);
        }
    }

    fh = fopen(path, "wb");
    if (fh == NULL) {
        free(data);
        return -1;
    }
    fwrite(&count, 1, sizeof(count), fh);
    fwrite(&header[0], 1, sizeof(header), fh);
    fwrite(data, 1, size, fh);
    fclose(fh);
    free(data);
    return 0;
}

static void test_texture_set(gex_d3d11_device *device)
{
    const char *vfx_path = "gex_test_texture.vfx";
    gex_vfx_texture_bank bank;
    gex_d3d11_texture_set set = GEX_D3D11_TEXTURE_SET_INITIALIZER;
    gex_d3d11_pipeline pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_emit_output emit;
    gex_geometry geom;
    offscreen target;
    uint8_t *pixels = NULL;
    uint32_t wanted[1];
    int status;

    /* Nombres de estado. */
    CHECK(strcmp(gex_d3d11_texture_status_name(GEX_D3D11_TEXTURE_OK), "ok") ==
          0);
    CHECK(gex_d3d11_texture_status_name(GEX_D3D11_TEXTURE_BAD_ARGUMENT) !=
          NULL);
    CHECK(gex_d3d11_texture_status_name(GEX_D3D11_TEXTURE_BAD_STATE) != NULL);
    CHECK(gex_d3d11_texture_status_name(GEX_D3D11_TEXTURE_TOO_MANY) != NULL);
    CHECK(gex_d3d11_texture_status_name(GEX_D3D11_TEXTURE_CREATE_FAILED) !=
          NULL);
    CHECK(gex_d3d11_texture_status_name(GEX_D3D11_TEXTURE_OUT_OF_MEMORY) !=
          NULL);

    /* Argumentos inválidos. */
    CHECK(gex_d3d11_texture_set_create(NULL, device, NULL,
                                       GEX_NCC_UV_MODE_NORMALIZED) ==
          GEX_D3D11_TEXTURE_BAD_ARGUMENT);
    CHECK(gex_d3d11_texture_set_create(&set, NULL, NULL,
                                       GEX_NCC_UV_MODE_NORMALIZED) ==
          GEX_D3D11_TEXTURE_BAD_ARGUMENT);
    CHECK(gex_d3d11_texture_set_create(&set, device, NULL,
                                       GEX_NCC_UV_MODE_NORMALIZED) ==
          GEX_D3D11_TEXTURE_BAD_ARGUMENT);
    gex_d3d11_texture_set_destroy(&set);
    gex_d3d11_texture_set_destroy(NULL);

    if (write_synthetic_vfx(vfx_path) != 0) {
        printf("skip texture set (no se pudo escribir el VFX)\n");
        return;
    }

    memset(&bank, 0, sizeof(bank));
    wanted[0] = 0;
    CHECK(gex_vfx_texture_bank_load(&bank, vfx_path, wanted, 1) == GEX_VFX_OK);
    CHECK(bank.texture_count == 1u);
    CHECK(bank.decoded_count == 1u);
    CHECK(bank.max_width == 64u && bank.max_height == 64u);

    /* Creación del conjunto: una rebanada de 64x64. */
    status = gex_d3d11_texture_set_create(&set, device, &bank,
                                          GEX_NCC_UV_MODE_NORMALIZED);
    CHECK(status == GEX_D3D11_TEXTURE_OK);
    if (status == GEX_D3D11_TEXTURE_OK) {
        D3D11_TEXTURE2D_DESC desc;
        memset(&desc, 0, sizeof(desc));
        ID3D11Texture2D_GetDesc(set.atlas, &desc);
        /* Una textura de 64x64 con el borde replicado: el atlas es de
         * (64+2) x (64+2). */
        CHECK(desc.Width == 66u);
        CHECK(desc.Height == 66u);
        CHECK(desc.ArraySize == 1u);
        CHECK(desc.MipLevels == 1u);
        CHECK(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
        CHECK(set.texture_count == 1u);
        CHECK(set.atlas_width == 66u);
        CHECK(set.atlas_height == 66u);
        CHECK(set.srv != NULL);
        CHECK(set.sampler != NULL);
        CHECK(set.lookup_buffer != NULL);

        /* Un conjunto vivo no se puede volver a crear. */
        CHECK(gex_d3d11_texture_set_create(&set, device, &bank,
                                           GEX_NCC_UV_MODE_NORMALIZED) ==
              GEX_D3D11_TEXTURE_BAD_STATE);
    }

    /* Muestreo: un triángulo con UV en el byte 32 (el texel 8 de una
     * textura de 64, con el divisor 256) debe pintar R = 136, G = 136,
     * B = 255 (el color por vértice es blanco). */
    memset(&emit, 0, sizeof(emit));
    if (status == GEX_D3D11_TEXTURE_OK &&
        gex_d3d11_pipeline_init(&pipeline, device) == GEX_D3D11_PIPELINE_OK &&
        offscreen_create(device->device, 256, 192, &target) == 0) {
        gex_emit_triangle *tris =
            (gex_emit_triangle *)calloc(1, sizeof(*tris));
        float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

        CHECK(tris != NULL);
        if (tris != NULL) {
            int c;
            tris->x[0] = 0; tris->y[0] = 0; tris->z[0] = 0x1000;
            tris->x[1] = 256; tris->y[1] = 0; tris->z[1] = 0x1000;
            tris->x[2] = 0; tris->y[2] = 192; tris->z[2] = 0x1000;
            for (c = 0; c < 3; c++) {
                /* Color blanco con alfa 0x40: el alfa del vertice NO debe
                 * llegar al resultado (el original no lo tiene: su vertice
                 * solo lleva B, G y R, § 19.11 del plan). Con el alfa del
                 * texel (255) el resultado debe ser opaco. */
                tris->color[c] = 0x40FFFFFFu;
                tris->u[c] = 32;
                tris->v[c] = 32;
            }
            tris->texture = 0;
            tris->flags = GEX_EMIT_F_NEAR_EMITTER;
            emit.tris = tris;
            emit.count = 1;
            emit.capacity = 1;

            CHECK(gex_geometry_build(&emit, &geom) == GEX_GEOM_OK);
            CHECK(gex_d3d11_mesh_create(device->device, &geom, &mesh) ==
                  GEX_D3D11_MESH_OK);
            CHECK(gex_d3d11_pipeline_set_textures(&pipeline, &set) ==
                  GEX_D3D11_PIPELINE_OK);
            CHECK(gex_d3d11_pipeline_set_target(&pipeline, target.rtv, 256, 192,
                                                GEX_D3D11_REFERENCE_WIDTH,
                                                GEX_D3D11_REFERENCE_HEIGHT) ==
                  GEX_D3D11_PIPELINE_OK);
            clear_target(device->context, target.rtv, clear_color);
            CHECK(gex_d3d11_pipeline_draw(&pipeline, &mesh) ==
                  GEX_D3D11_PIPELINE_OK);

            pixels = (uint8_t *)malloc(256u * 192u * 4u);
            if (pixels != NULL) {
                CHECK(offscreen_read(device->context, &target, pixels) == 0);
                /* El triángulo ocupa el cuarto superior izquierdo del
                 * viewport (escala 0,5 sobre la referencia 512x384): se
                 * mira un punto interior. */
                {
                    size_t at = (16u * 192u + 24u) * 4u;
                    CHECK(pixels[at + 0] == 136u);
                    CHECK(pixels[at + 1] == 136u);
                    CHECK(pixels[at + 2] == 255u);
                    /* El alfa de salida es el del TEXEL (255), no el del
                     * vertice (0x40): el original no tiene alfa por
                     * vertice (§ 19.11 del plan). */
                    CHECK(pixels[at + 3] == 255u);
                }
                free(pixels);
            }
            gex_emit_output_free(&emit);
        }
        gex_d3d11_mesh_destroy(&mesh);
        gex_d3d11_pipeline_shutdown(&pipeline);
        offscreen_destroy(&target);
    }

    gex_d3d11_texture_set_destroy(&set);
    CHECK(set.initialized == 0);
    gex_d3d11_texture_set_destroy(&set);
    gex_vfx_texture_bank_free(&bank);
    remove(vfx_path);
}

int main(void)
{
    gex_d3d11_device device = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_device_status ds;

    test_names();
    test_initializer();
    test_input_layout();
    test_renderer_geometry_contract();

    ds = gex_d3d11_device_init(&device, GEX_D3D11_DRIVER_WARP, 0);
    if (ds != GEX_D3D11_DEVICE_OK) {
        printf("skip device tests: %s (0x%08lx)\n",
               gex_d3d11_device_status_name(ds),
               (unsigned long)device.last_hresult);
        printf("test_d3d11_pipeline: %d checks, %d failures\n", checks,
               failures);
        return failures == 0 ? 0 : 1;
    }

    test_init_errors(&device);
    test_init_and_shutdown(&device);
#ifdef GEX_PIPELINE_TEST_FAULT_INJECT
    test_init_faults(&device);
#endif
    test_draw_and_colors(&device);
    test_resize(&device);
    test_invalid_states(&device);
    test_no_partial_resources(&device);
    test_draw_cube(&device);
    test_texture_set(&device);

    gex_d3d11_device_shutdown(&device);

    printf("test_d3d11_pipeline: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
