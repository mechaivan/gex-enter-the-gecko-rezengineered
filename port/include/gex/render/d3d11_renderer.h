/*
 * gex/render/d3d11_renderer.h - the R-3 Direct3D 11 renderer (Windows-only).
 *
 * This header defines the R-3 renderer that provides the first visible graphics
 * output of the port. It creates a render target view from the backbuffer of
 * R-2, clears it with a configurable color, and presents the result via
 * IDXGISwapChain::Present.
 *
 * The renderer integrates with the I-4 application by replacing the null renderer,
 * while maintaining the existing contracts of R-1 (device) and R-2 (swapchain).
 *
 * Windows-only, like d3d11_device.h and d3d11_swapchain.h: portable code must
 * not include this header and the POSIX Makefile does not compile the source.
 */

#ifndef GEX_RENDER_D3D11_RENDERER_H
#define GEX_RENDER_D3D11_RENDERER_H

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
/* WIDL C bindings: the ID3D11Device_/IDXGISwapChain_ helpers need this. */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>

#include <d3d11.h>

#include <stdint.h>

#include "gex/render/d3d11_device.h"
#include "gex/render/d3d11_swapchain.h"
#include "gex/render/d3d11_pipeline.h"
#include "gex/core/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gex_d3d11_renderer_status {
    GEX_D3D11_RENDERER_OK = 0,
    GEX_D3D11_RENDERER_BAD_ARGUMENT,  /* NULL renderer/device/swapchain */
    GEX_D3D11_RENDERER_BAD_STATE,     /* already initialized */
    GEX_D3D11_RENDERER_BAD_DEVICE,    /* uninitialized or invalid device */
    GEX_D3D11_RENDERER_BAD_SWAPCHAIN,  /* uninitialized or invalid swapchain */
    GEX_D3D11_RENDERER_RTV_CREATE_FAILED  /* failed to create render target view */
} gex_d3d11_renderer_status;

typedef struct gex_d3d11_renderer {
    ID3D11Device *device;            /* borrowed from R-1 */
    ID3D11DeviceContext *context;    /* borrowed from R-1 */
    IDXGISwapChain *swapchain;       /* borrowed from R-2 */
    ID3D11Texture2D *backbuffer;    /* borrowed from R-2 */
    ID3D11RenderTargetView *rtv;    /* owned by this renderer */
    float clear_color[4];            /* RGBA color for clearing */
    HRESULT last_hresult;           /* S_OK or the last error */
    int initialized;                 /* 1 = live; the only field init reads
                                       * before it owns the struct */
    uint64_t presents;               /* presentation count */
    uint64_t last_now_us;           /* last presentation time */
    uint32_t last_steps;            /* last frame steps */
    uint32_t last_frame_index;      /* last frame index */
    UINT sync_interval;             /* VSync sync interval (0 = immediate, 1 = 1 refresh) */
    int enable_debug_layer;          /* whether debug layer is enabled */
    HRESULT device_removed_reason;   /* reason for device removal, S_OK if none */
    uint64_t frame_time_us;         /* time between frames for performance monitoring */
    /* Geometría opcional (Módulo 5A). Son punteros prestados: el renderer
     * no los libera. Si pipeline o mesh son NULL, o la malla no está
     * inicializada, el renderer se comporta exactamente como antes: limpia y
     * presenta. */
    gex_d3d11_pipeline *pipeline;    /* pipeline de dibujo (prestado) */
    gex_d3d11_mesh *mesh;            /* malla a dibujar (prestado) */
    UINT geometry_reference_width;   /* resolución lógica del espacio dibujado */
    UINT geometry_reference_height;
    uint64_t draws;                  /* llamadas a DrawIndexed */
    uint64_t draw_errors;            /* dibujos fallidos */
    /* Diagnóstico del último dibujo fallido (Módulo 5A). */
    gex_d3d11_pipeline_status last_draw_status;
    HRESULT last_draw_hresult;
} gex_d3d11_renderer;

/*
 * PRE-CONDITION: a gex_d3d11_renderer must be initialized with
 * GEX_D3D11_RENDERER_INITIALIZER (or zeroed) before the first init.
 *
 *   gex_d3d11_renderer renderer = GEX_D3D11_RENDERER_INITIALIZER;
 *   if (gex_d3d11_renderer_init(&renderer, &device, &swapchain, clear_color) ==
 *       GEX_D3D11_RENDERER_OK) { ... gex_d3d11_renderer_shutdown(&renderer); }
 *
 * shutdown leaves the struct exactly in the initializer state, so it can be
 * reused for another init (and a second shutdown is a no-op).
 */
#define GEX_D3D11_RENDERER_INITIALIZER                                          \
    {                                                                         \
        .device = NULL, .context = NULL, .swapchain = NULL,                    \
        .backbuffer = NULL, .rtv = NULL, .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}, \
        .last_hresult = S_OK, .initialized = 0,                                \
        .presents = 0, .last_now_us = 0, .last_steps = 0, .last_frame_index = 0, \
        .sync_interval = 1, .enable_debug_layer = 0, .device_removed_reason = S_OK, \
        .frame_time_us = 0,                                                    \
        .pipeline = NULL, .mesh = NULL,                                        \
        .geometry_reference_width = 0, .geometry_reference_height = 0,         \
        .draws = 0, .draw_errors = 0,                                          \
        .last_draw_status = GEX_D3D11_PIPELINE_OK, .last_draw_hresult = S_OK    \
    }

/*
 * Initialize the renderer with the device from R-1 and swapchain from R-2.
 * The clear_color parameter can be NULL to use the default dark blue.
 */
gex_d3d11_renderer_status gex_d3d11_renderer_init(
    gex_d3d11_renderer *renderer,
    const gex_d3d11_device *device,
    const gex_d3d11_swapchain *swapchain,
    const float clear_color[4]);

gex_d3d11_renderer_status gex_d3d11_renderer_set_sync_interval(
    gex_d3d11_renderer *renderer,
    UINT sync_interval);

/*
 * Set the VSync sync interval from swapchain configuration.
 * This function uses vsync_retraces as the source for sync_interval.
 */
gex_d3d11_renderer_status gex_d3d11_renderer_set_sync_interval_from_config(
    gex_d3d11_renderer *renderer,
    UINT vsync_retraces);

/*
 * Resize the renderer after the swapchain has been resized. This releases the
 * old render target view and creates a new one from the updated backbuffer.
 */
gex_d3d11_renderer_status gex_d3d11_renderer_resize(
    gex_d3d11_renderer *renderer,
    const gex_d3d11_swapchain *swapchain);

/*
 * Present hook compatible with gex_renderer. Clears the render target with
 * the configured color, draws the optional geometry (Módulo 5A) and presents
 * the backbuffer.
 */
int gex_d3d11_renderer_present(void *ctx, const gex_frame_tick *tick);

/*
 * Registra la geometría opcional que se dibuja antes de cada Present
 * (Módulo 5A). Los punteros son **prestados**: el renderer no los libera,
 * así que deben seguir vivos hasta que se llame otra vez con NULL o hasta
 * cerrar el renderer.
 *
 *   renderer == NULL o no inicializado -> BAD_ARGUMENT / BAD_STATE
 *   pipeline == NULL y mesh == NULL    -> quita la geometría (vuelve al
 *                                          comportamiento de solo limpiar)
 *   ref_width/ref_height == 0          -> usa 512x384 (el espacio lógico del
 *                                          original)
 */
gex_d3d11_renderer_status gex_d3d11_renderer_set_geometry(
    gex_d3d11_renderer *renderer,
    gex_d3d11_pipeline *pipeline,
    gex_d3d11_mesh *mesh,
    UINT ref_width, UINT ref_height);

/*
 * Release all resources and zero the struct. The device and swapchain are NOT
 * touched: they are released by their respective modules. Idempotent.
 */
void gex_d3d11_renderer_shutdown(gex_d3d11_renderer *renderer);





/*
 * Enable or disable the debug layer for this renderer.
 * When enabled, debug messages will be output to the debug console.
 * Returns 0 on success, non-zero on failure.
 */
int gex_d3d11_renderer_set_debug_layer(gex_d3d11_renderer *renderer, int enable);

/*
 * Check if the device has been removed and get the reason.
 * Returns S_OK if the device is still valid, or the error code if removed.
 */
HRESULT gex_d3d11_renderer_get_device_removed_reason(gex_d3d11_renderer *renderer);

/*
 * Get the time in microseconds between the last two frames.
 * Returns 0 if no frames have been presented yet.
 */
uint64_t gex_d3d11_renderer_get_frame_time_us(gex_d3d11_renderer *renderer);

const char *gex_d3d11_renderer_status_name(gex_d3d11_renderer_status status);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* GEX_RENDER_D3D11_RENDERER_H */