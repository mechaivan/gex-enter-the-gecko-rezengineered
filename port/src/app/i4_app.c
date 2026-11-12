/*
 * i4_app.c - I-4 application core, independent of Win32.
 *
 * The Win32 main creates the window, routes keyboard messages into
 * app.input and calls gex_i4_app_step(). This file never includes windows.h,
 * so the whole integration can be exercised by the automated tests.
 */

#include "gex/app/i4_app.h"
#include "gex/core/frame.h"
#include "gex/input/key_state.h"
#include "gex/render/null_renderer.h"
#if defined(_WIN32)
#include "gex/render/d3d11_renderer.h"
#endif

#include <string.h>

static int app_pre_frame(void *ctx, const gex_frame_tick *tick)
{
    gex_i4_app *app = (gex_i4_app *)ctx;

    (void)tick;
    app->pre_calls++;
    return 0; /* I-4 does not translate input into game actions yet */
}

static int app_update(void *ctx, const gex_frame_tick *tick)
{
    gex_i4_app *app = (gex_i4_app *)ctx;

    (void)tick;
    app->updates++;
    /* Trabajo de juego opcional (S-3: la secuencia de camara). Se invoca una
     * vez por update logico, en el mismo punto del bucle que el update del
     * original. Un valor distinto de 0 aborta el frame. */
    if (app->update_callback != NULL) {
        return app->update_callback(app->update_ctx);
    }
    return 0; /* sin simulacion en I-4 */
}

void gex_i4_app_set_update_callback(gex_i4_app *app,
                                    gex_i4_update_callback callback,
                                    void *ctx)
{
    if (app == NULL) {
        return;
    }
    app->update_callback = callback;
    app->update_ctx = ctx;
}

static int app_present(void *ctx, const gex_frame_tick *tick)
{
    gex_i4_app *app = (gex_i4_app *)ctx;

#if defined(_WIN32)
    if (app->d3d11_enabled) {
        return gex_d3d11_renderer_present(&app->renderer.d3d11_renderer, tick);
    } else {
        return gex_null_renderer_present(&app->renderer.null_renderer, tick);
    }
#else
    return gex_null_renderer_present(&app->renderer.null_renderer, tick);
#endif
}

gex_frame_status gex_i4_app_init(gex_i4_app *app,
                                 const gex_frame_config *cfg,
                                 uint64_t t0_us)
{
    gex_frame_hooks hooks;
    gex_frame_status status;

    if (app == NULL || cfg == NULL) {
        return GEX_FRAME_ERR_ARGUMENT;
    }
    memset(app, 0, sizeof(*app));
    gex_input_init(&app->input);
    
#if defined(_WIN32)
    app->d3d11_enabled = 0; /* start with null renderer */
    gex_null_renderer_init(&app->renderer.null_renderer);
#else
    gex_null_renderer_init(&app->renderer.null_renderer);
#endif

    hooks.pre_frame = app_pre_frame;
    hooks.update = app_update;
    hooks.present = app_present;
    hooks.ctx = app;

    status = gex_frame_init(&app->frame, cfg, &hooks, NULL, t0_us);
    if (status != GEX_FRAME_OK) {
        memset(app, 0, sizeof(*app));
        return status;
    }
    app->initialized = 1;
    return GEX_FRAME_OK;
}

gex_frame_status gex_i4_app_step(gex_i4_app *app, uint64_t now_us)
{
    gex_frame_status status;

    if (app == NULL) {
        return GEX_FRAME_ERR_ARGUMENT;
    }
    if (!app->initialized) {
        return GEX_FRAME_ERR_STATE;
    }

    status = gex_frame_run(&app->frame, now_us);

    if (gex_input_was_pressed(&app->input, GEX_KEY_ESCAPE)) {
        app->quit = 1;
    }
    gex_input_begin_frame(&app->input);
    return status;
}

int gex_i4_app_should_quit(const gex_i4_app *app)
{
    return (app != NULL && app->quit) ? 1 : 0;
}

void gex_i4_app_shutdown(gex_i4_app *app)
{
    if (app == NULL || !app->initialized) {
        return;
    }
    gex_frame_shutdown(&app->frame);
    
#if defined(_WIN32)
    if (app->d3d11_enabled) {
        gex_d3d11_renderer_shutdown(&app->renderer.d3d11_renderer);
    } else {
        gex_null_renderer_shutdown(&app->renderer.null_renderer);
    }
#else
    gex_null_renderer_shutdown(&app->renderer.null_renderer);
#endif
    app->initialized = 0;
}

#if defined(_WIN32)
gex_d3d11_renderer_status gex_i4_app_enable_d3d11(gex_i4_app *app,
                                                 const gex_d3d11_device *device,
                                                 const gex_d3d11_swapchain *swapchain,
                                                 const float clear_color[4])
{
    if (app == NULL || !app->initialized) {
        return GEX_D3D11_RENDERER_BAD_STATE;
    }
    
    if (app->d3d11_enabled) {
        return GEX_D3D11_RENDERER_BAD_STATE;
    }
    
    gex_d3d11_renderer_status status = gex_d3d11_renderer_init(&app->renderer.d3d11_renderer, 
                                                             device, 
                                                             swapchain, 
                                                             clear_color);
    if (status == GEX_D3D11_RENDERER_OK) {
        app->d3d11_enabled = 1;
    }
    
    return status;
}

void gex_i4_app_disable_d3d11(gex_i4_app *app)
{
    if (app == NULL || !app->initialized || !app->d3d11_enabled) {
        return;
    }
    
    gex_d3d11_renderer_shutdown(&app->renderer.d3d11_renderer);
    app->d3d11_enabled = 0;
    
    /* Reinitialize with null renderer */
    gex_null_renderer_init(&app->renderer.null_renderer);
}

int gex_i4_app_is_d3d11_enabled(const gex_i4_app *app)
{
    return (app != NULL && app->initialized && app->d3d11_enabled) ? 1 : 0;
}
#endif
