/*
 * i4_app.c - I-4 application core, independent of Win32.
 *
 * The Win32 main creates the window, routes keyboard messages into
 * app.input and calls gex_i4_app_step(). This file never includes windows.h,
 * so the whole integration can be exercised by the automated tests.
 */

#include "gex/app/i4_app.h"

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
    return 0; /* no simulation in I-4 */
}

static int app_present(void *ctx, const gex_frame_tick *tick)
{
    gex_i4_app *app = (gex_i4_app *)ctx;

    return gex_null_renderer_present(&app->renderer, tick);
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
    gex_null_renderer_init(&app->renderer);

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
    gex_null_renderer_shutdown(&app->renderer);
    app->initialized = 0;
}
