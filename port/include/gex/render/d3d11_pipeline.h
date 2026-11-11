/*
 * gex/render/d3d11_pipeline.h - Módulo 5A: pipeline mínimo de dibujo D3D11
 * (Windows-only).
 *
 * Cierra el camino desde la geometría del Módulo 3 hasta la pantalla:
 *
 *   gex_emit_output --gex_geometry_build--> gex_geometry
 *                   --gex_d3d11_mesh_create--> ID3D11Buffer (vértices/índices)
 *                   --gex_d3d11_pipeline_draw--> DrawIndexed
 *
 * El módulo contiene el vertex shader y el pixel shader mínimos (color por
 * vértice, sin texturas), el ID3D11InputLayout que describe los 28 B reales
 * del vértice, los estados de rasterizado/blending/profundidad y el constant
 * buffer con el tamaño del viewport.
 *
 * Los shaders se compilan en ejecución con `d3dcompiler_47.dll` (el
 * compilador HLSL que forma parte de Windows, el mismo criterio que
 * `d3d11.dll` y `dsound.dll`): no hay fxc.exe en el entorno y no se instala
 * ninguna herramienta. El módulo HLSL vive en `src/render/d3d11_pipeline.c`
 * y se documenta en docs/D3D11_FIRST_SCENE_PLAN.md.
 *
 * Windows-only como d3d11_device.h: el código portable no incluye esta
 * cabecera y el Makefile POSIX no compila la fuente.
 */

#ifndef GEX_RENDER_D3D11_PIPELINE_H
#define GEX_RENDER_D3D11_PIPELINE_H

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
/* WIDL C bindings: los ayudantes ID3D11Buffer_Release necesitan esto. */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>

#include <d3d11.h>

#include <stdint.h>

#include "gex/render/d3d11_device.h"
#include "gex/render/d3d11_mesh.h"
#include "gex/render/d3d11_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gex_d3d11_pipeline_status {
    GEX_D3D11_PIPELINE_OK = 0,
    GEX_D3D11_PIPELINE_BAD_ARGUMENT,   /* device/target nulos o tamaños 0 */
    GEX_D3D11_PIPELINE_BAD_STATE,      /* pipeline no inicializado */
    GEX_D3D11_PIPELINE_BAD_DEVICE,     /* dispositivo no inicializado */
    GEX_D3D11_PIPELINE_NO_COMPILER,    /* falta d3dcompiler_47.dll */
    GEX_D3D11_PIPELINE_SHADER_FAILED,  /* el HLSL no compila */
    GEX_D3D11_PIPELINE_CREATE_FAILED,  /* Create* falló */
    GEX_D3D11_PIPELINE_NO_GEOMETRY     /* malla vacía o no inicializada */
} gex_d3d11_pipeline_status;

/*
 * Pipeline de dibujo. Todos los punteros COM son propiedad del módulo:
 * `gex_d3d11_pipeline_shutdown` los libera y deja la estructura a cero.
 */
typedef struct gex_d3d11_pipeline {
    ID3D11Device *device;              /* prestado (R-1) */
    ID3D11DeviceContext *context;      /* prestado (R-1) */
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
    ID3D11RasterizerState *rasterizer;
    ID3D11BlendState *blend;
    ID3D11DepthStencilState *depth_stencil;
    ID3D11Buffer *constant_buffer;     /* viewport + resolución de referencia */
    ID3D11Buffer *tex_constants;       /* tabla de texturas (b1), vacía si no hay */
    const gex_d3d11_texture_set *textures; /* prestado (Módulo 4C) */
    HMODULE       compiler_module;     /* d3dcompiler_47.dll, si se cargó */
    HRESULT       last_hresult;        /* S_OK o el último fallo */
    char          last_error[192];     /* texto del error de compilación */
    int           initialized;          /* 1 = vivo */
    uint64_t      draw_calls;          /* llamadas a DrawIndexed */
    uint64_t      triangles_drawn;     /* triángulos enviados */
} gex_d3d11_pipeline;

#define GEX_D3D11_PIPELINE_INITIALIZER                                    \
    {                                                                      \
        .device = NULL, .context = NULL, .vertex_shader = NULL,            \
        .pixel_shader = NULL, .input_layout = NULL, .rasterizer = NULL,    \
        .blend = NULL, .depth_stencil = NULL, .constant_buffer = NULL,     \
        .tex_constants = NULL, .textures = NULL, .compiler_module = NULL,   \
        .last_hresult = S_OK, .last_error = {0},                           \
        .initialized = 0, .draw_calls = 0, .triangles_drawn = 0             \
    }

/*
 * Descripción de entrada del vértice: refleja exactamente los 28 B de
 * `gex_vertex` (port/include/gex/geometry.h). Los offsets vienen de las
 * constantes públicas del vértice, no de números escritos a mano.
 *
 * El color se declara como DXGI_FORMAT_R8G8B8A8_UNORM porque D3D11 no
 * admite B8G8R8A8_UNORM en un vertex buffer. El vértice guarda el color
 * empaquetado AARRGGBB en un uint32 (little-endian: BB, GG, RR, AA en
 * memoria), así que el shader recibe los canales R,G,B,A en ese orden y el
 * vertex shader los reordena a (R,G,B,A) antes de pasarlos al pixel shader.
 */
#define GEX_D3D11_INPUT_ELEMENT_COUNT 6

extern const D3D11_INPUT_ELEMENT_DESC
    gex_d3d11_input_layout[GEX_D3D11_INPUT_ELEMENT_COUNT];

/* Resolución lógica de referencia: el espacio de pantalla del original. */
#define GEX_D3D11_REFERENCE_WIDTH  512u
#define GEX_D3D11_REFERENCE_HEIGHT 384u

/* Divisor de profundidad: la emisión usa z en [near_z, 0xFFFF]. */
#define GEX_D3D11_DEPTH_DIVISOR 65535.0f

/*
 * Compila los shaders y crea los estados. Requiere un dispositivo vivo.
 * No abre ninguna ventana ni swapchain.
 */
gex_d3d11_pipeline_status gex_d3d11_pipeline_init(gex_d3d11_pipeline *pipeline,
                                                   const gex_d3d11_device *device);

/*
 * Fija el destino de dibujo: render target, viewport y constant buffer.
 *
 *   rvt == NULL o width/height/ref_width/ref_height == 0 -> BAD_ARGUMENT
 *   pipeline no inicializado                          -> BAD_STATE
 *
 * `ref_width`/`ref_height` son la resolución lógica del espacio proyectado
 * (512x384 en el original). El vertex shader centra esa resolución en el
 * viewport y la escala sin deformarla (scale = min(vw/rw, vh/rh)).
 */
gex_d3d11_pipeline_status gex_d3d11_pipeline_set_target(
    gex_d3d11_pipeline *pipeline,
    ID3D11RenderTargetView *rtv,
    UINT width, UINT height,
    UINT ref_width, UINT ref_height);

/*
 * Fija el conjunto de texturas del Módulo 4C (prestado: el pipeline no lo
 * libera). Con `set == NULL` el pixel shader no encuentra ninguna entrada en
 * la tabla y dibuja el color por vértice, que es el camino del cubo de
 * prueba.
 *
 *   pipeline no inicializado -> BAD_STATE
 */
gex_d3d11_pipeline_status gex_d3d11_pipeline_set_textures(
    gex_d3d11_pipeline *pipeline, const gex_d3d11_texture_set *set);

/*
 * Dibuja una malla con DrawIndexed, en el orden de su index buffer (que es
 * el orden de emisión). No toca el render target: el llamador limpia antes.
 *
 *   mesh == NULL o mesh no inicializado -> BAD_ARGUMENT / NO_GEOMETRY
 *   malla vacía (index_count == 0)       -> NO_GEOMETRY (no se dibuja nada)
 */
gex_d3d11_pipeline_status gex_d3d11_pipeline_draw(gex_d3d11_pipeline *pipeline,
                                                   const gex_d3d11_mesh *mesh);

/* Libera todos los recursos y deja la estructura a cero. Idempotente. */
void gex_d3d11_pipeline_shutdown(gex_d3d11_pipeline *pipeline);

const char *gex_d3d11_pipeline_status_name(gex_d3d11_pipeline_status status);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* GEX_RENDER_D3D11_PIPELINE_H */
