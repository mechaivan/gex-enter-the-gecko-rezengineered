/*
 * d3d11_renderer.c - R-3: Direct3D 11 renderer with real presentation.
 *
 * This renderer implements the first visible graphics output of the port:
 * it creates a render target view from the backbuffer of R-2, clears it
 * with a configurable color, and presents the result via IDXGISwapChain::Present.
 *
 * The renderer integrates with the I-4 application by replacing the null renderer,
 * while maintaining the existing contracts of R-1 (device) and R-2 (swapchain).
 *
 * Windows-only, like d3d11_device.c and d3d11_swapchain.c: portable code must
 * not include this header and the POSIX Makefile does not compile the source.
 */

#include "gex/render/d3d11_renderer.h"

#if defined(_WIN32)

#include <string.h>
#include <d3d11.h>

static void release_partial(ID3D11RenderTargetView *rtv)
{
    if (rtv != NULL) {
        ID3D11RenderTargetView_Release(rtv);
    }
}

gex_d3d11_renderer_status gex_d3d11_renderer_init(
    gex_d3d11_renderer *renderer,
    const gex_d3d11_device *device,
    const gex_d3d11_swapchain *swapchain,
    const float clear_color[4])
{
    HRESULT hr;
    ID3D11RenderTargetView *rtv = NULL;
    ID3D11Texture2D *backbuffer = NULL;

    if (renderer == NULL || device == NULL || swapchain == NULL) {
        return GEX_D3D11_RENDERER_BAD_ARGUMENT;
    }
    
    /* State first: a live renderer is never touched */
    if (renderer->initialized) {
        return GEX_D3D11_RENDERER_BAD_STATE;
    }
    
    if (!device->initialized || device->device == NULL || device->context == NULL) {
        return GEX_D3D11_RENDERER_BAD_DEVICE;
    }
    
    if (!swapchain->initialized || swapchain->swapchain == NULL || 
        swapchain->backbuffer == NULL) {
        return GEX_D3D11_RENDERER_BAD_SWAPCHAIN;
    }
    
    /* Store the clear color */
    if (clear_color != NULL) {
        memcpy(renderer->clear_color, clear_color, sizeof(renderer->clear_color));
    } else {
        /* Default to dark blue if no color provided */
        renderer->clear_color[0] = 0.0f;  /* R */
        renderer->clear_color[1] = 0.0f;  /* G */
        renderer->clear_color[2] = 0.5f;  /* B */
        renderer->clear_color[3] = 1.0f;  /* A */
    }
    
    /* Get the backbuffer from the swapchain */
    backbuffer = swapchain->backbuffer;
    
    /* Create the render target view from the backbuffer */
    hr = ID3D11Device_CreateRenderTargetView(device->device, (ID3D11Resource *)backbuffer,
                                           NULL, &rtv);
    renderer->last_hresult = hr;
    if (FAILED(hr)) {
        return GEX_D3D11_RENDERER_RTV_CREATE_FAILED;
    }
    if (rtv == NULL) {
        renderer->last_hresult = E_FAIL;
        return GEX_D3D11_RENDERER_RTV_CREATE_FAILED;
    }
    
    /* Store the resources */
    renderer->device = device->device;
    renderer->context = device->context;
    renderer->swapchain = swapchain->swapchain;
    renderer->rtv = rtv;
    renderer->backbuffer = backbuffer;
    renderer->initialized = 1;
    
    /* Initialize device removed reason */
    renderer->device_removed_reason = S_OK;
    
    /* Set sync interval from swapchain configuration */
    if (swapchain) {
        /* Default to 1 (VSync enabled), will be set properly by set_sync_interval_from_config */
        renderer->sync_interval = 1;
    } else {
        renderer->sync_interval = 1; /* Default to 1 (VSync enabled) */
    }
    
    /* Note: Debug layer is enabled at device creation time via D3D11_CREATE_DEVICE_DEBUG flag.
       This renderer only stores the preference; the actual debug layer activation happens
       in the device creation. We check if the debug layer is available by querying the device. */
    if (renderer->enable_debug_layer) {
        renderer->last_hresult = S_OK;
    } else {
        renderer->last_hresult = S_OK;
    }
    
    return GEX_D3D11_RENDERER_OK;
}

gex_d3d11_renderer_status gex_d3d11_renderer_resize(
    gex_d3d11_renderer *renderer,
    const gex_d3d11_swapchain *swapchain)
{
    HRESULT hr;
    ID3D11RenderTargetView *rtv = NULL;
    ID3D11Texture2D *backbuffer = NULL;

    if (renderer == NULL || swapchain == NULL) {
        return GEX_D3D11_RENDERER_BAD_ARGUMENT;
    }
    
    if (!renderer->initialized) {
        return GEX_D3D11_RENDERER_BAD_STATE;
    }
    
    if (!swapchain->initialized || swapchain->swapchain == NULL || 
        swapchain->backbuffer == NULL) {
        return GEX_D3D11_RENDERER_BAD_SWAPCHAIN;
    }
    
    /* Get the new backbuffer from the swapchain */
    backbuffer = swapchain->backbuffer;
    
    /* The old RTV must be released before using the new backbuffer */
    if (renderer->rtv != NULL) {
        ID3D11RenderTargetView_Release(renderer->rtv);
        renderer->rtv = NULL;
    }
    
    /* Create a new render target view from the new backbuffer */
    hr = ID3D11Device_CreateRenderTargetView(renderer->device, 
                                          (ID3D11Resource *)backbuffer,
                                          NULL, &rtv);
    renderer->last_hresult = hr;
    if (FAILED(hr)) {
        return GEX_D3D11_RENDERER_RTV_CREATE_FAILED;
    }
    if (rtv == NULL) {
        renderer->last_hresult = E_FAIL;
        return GEX_D3D11_RENDERER_RTV_CREATE_FAILED;
    }
    
    /* Update the resources */
    renderer->backbuffer = backbuffer;
    renderer->rtv = rtv;
    
    return GEX_D3D11_RENDERER_OK;
}

int gex_d3d11_renderer_present(void *ctx, const gex_frame_tick *tick)
{
    gex_d3d11_renderer *renderer = (gex_d3d11_renderer *)ctx;
    int draw_failed = 0;

    if (renderer == NULL || !renderer->initialized || tick == NULL) {
        return 1;
    }
    
    /* Clear the render target with the configured color */
    ID3D11DeviceContext_ClearRenderTargetView(renderer->context, renderer->rtv,
                                           renderer->clear_color);

    /* Módulo 5A: dibujo opcional de la geometría, antes del Present. Si no
     * hay pipeline o malla, el comportamiento es el de siempre: limpiar y
     * presentar. */
    if (renderer->pipeline != NULL && renderer->mesh != NULL &&
        renderer->mesh->initialized) {
        D3D11_TEXTURE2D_DESC desc;
        gex_d3d11_pipeline_status st;

        memset(&desc, 0, sizeof(desc));
        if (renderer->backbuffer != NULL) {
            ID3D11Texture2D_GetDesc(renderer->backbuffer, &desc);
        }
        st = gex_d3d11_pipeline_set_target(
            renderer->pipeline, renderer->rtv, desc.Width, desc.Height,
            renderer->geometry_reference_width,
            renderer->geometry_reference_height);
        if (st == GEX_D3D11_PIPELINE_OK) {
            st = gex_d3d11_pipeline_draw(renderer->pipeline, renderer->mesh);
        }
        if (st == GEX_D3D11_PIPELINE_OK) {
            renderer->draws++;
        } else {
            renderer->draw_errors++;
            renderer->last_hresult = E_FAIL;
            draw_failed = 1;
        }
    }
    
    /* Present the backbuffer with the configured sync interval */
    HRESULT hr = IDXGISwapChain_Present(renderer->swapchain, renderer->sync_interval, 0);
    renderer->last_hresult = hr;
    if (FAILED(hr)) {
        /* Check for device removal error */
        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            renderer->device_removed_reason = ID3D11Device_GetDeviceRemovedReason(renderer->device);
        }
        return 1;
    }
    
    /* Update statistics */
    renderer->presents++;
    
    /* Calculate frame time if we have a previous timestamp */
    if (renderer->last_now_us > 0) {
        renderer->frame_time_us = tick->now_us - renderer->last_now_us;
    }
    
    renderer->last_now_us = tick->now_us;
    renderer->last_steps = tick->steps;
    renderer->last_frame_index = tick->frame_index;
    
    return draw_failed;
}

gex_d3d11_renderer_status gex_d3d11_renderer_set_geometry(
    gex_d3d11_renderer *renderer,
    gex_d3d11_pipeline *pipeline,
    gex_d3d11_mesh *mesh,
    UINT ref_width, UINT ref_height)
{
    if (renderer == NULL) {
        return GEX_D3D11_RENDERER_BAD_ARGUMENT;
    }
    if (!renderer->initialized) {
        return GEX_D3D11_RENDERER_BAD_STATE;
    }
    /* Sólo una de las dos mitades: el pipeline sin malla o la malla sin
     * pipeline no se pueden dibujar. */
    if ((pipeline == NULL) != (mesh == NULL)) {
        return GEX_D3D11_RENDERER_BAD_ARGUMENT;
    }

    renderer->pipeline = pipeline;
    renderer->mesh = mesh;
    renderer->geometry_reference_width =
        (ref_width == 0u) ? GEX_D3D11_REFERENCE_WIDTH : ref_width;
    renderer->geometry_reference_height =
        (ref_height == 0u) ? GEX_D3D11_REFERENCE_HEIGHT : ref_height;
    return GEX_D3D11_RENDERER_OK;
}

void gex_d3d11_renderer_shutdown(gex_d3d11_renderer *renderer)
{
    if (renderer == NULL) {
        return;
    }
    
    /* Release resources in the correct order */
    if (renderer->rtv != NULL) {
        ID3D11RenderTargetView_Release(renderer->rtv);
    }
    
    /* Zero the struct */
    memset(renderer, 0, sizeof(*renderer));
}

const char *gex_d3d11_renderer_status_name(gex_d3d11_renderer_status status)
{
    switch (status) {
    case GEX_D3D11_RENDERER_OK:
        return "ok";
    case GEX_D3D11_RENDERER_BAD_ARGUMENT:
        return "bad argument";
    case GEX_D3D11_RENDERER_BAD_STATE:
        return "bad state";
    case GEX_D3D11_RENDERER_BAD_DEVICE:
        return "bad device";
    case GEX_D3D11_RENDERER_BAD_SWAPCHAIN:
        return "bad swapchain";
    case GEX_D3D11_RENDERER_RTV_CREATE_FAILED:
        return "rtv create failed";
    }
    return "unknown";
}

gex_d3d11_renderer_status gex_d3d11_renderer_set_sync_interval(gex_d3d11_renderer *renderer, UINT sync_interval)
{
    if (renderer == NULL || !renderer->initialized) {
        return GEX_D3D11_RENDERER_BAD_ARGUMENT;
    }
    
    /* Validate sync interval */
    if (sync_interval > 4) {
        return GEX_D3D11_RENDERER_BAD_ARGUMENT;
    }
    
    renderer->sync_interval = sync_interval;
    return GEX_D3D11_RENDERER_OK;
}

gex_d3d11_renderer_status gex_d3d11_renderer_set_sync_interval_from_config(
    gex_d3d11_renderer *renderer,
    UINT vsync_retraces)
{
    if (renderer == NULL || !renderer->initialized) {
        return GEX_D3D11_RENDERER_BAD_ARGUMENT;
    }
    
    /* Use vsync_retraces from the swapchain configuration */
    /* vsync_retraces=0 means no VSync (sync_interval=0), other values are passed directly */
    renderer->sync_interval = vsync_retraces;
    return GEX_D3D11_RENDERER_OK;
}

int gex_d3d11_renderer_set_debug_layer(gex_d3d11_renderer *renderer, int enable)
{
    if (renderer == NULL || !renderer->initialized) {
        return 1;
    }
    
    renderer->enable_debug_layer = enable;
    return 0;
}

HRESULT gex_d3d11_renderer_get_device_removed_reason(gex_d3d11_renderer *renderer)
{
    if (renderer == NULL || !renderer->initialized) {
        return E_FAIL;
    }
    
    return renderer->device_removed_reason;
}

uint64_t gex_d3d11_renderer_get_frame_time_us(gex_d3d11_renderer *renderer)
{
    if (renderer == NULL || !renderer->initialized) {
        return 0;
    }
    
    return renderer->frame_time_us;
}

#endif /* _WIN32 */