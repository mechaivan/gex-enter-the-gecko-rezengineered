/*
 * null_renderer.c - present hook for I-4 (no drawing at all).
 *
 * It exists so the frame loop has a real, successful presentation phase while
 * the port still has no renderer. It must not be mistaken for a graphics
 * backend: there is no device, no surface and no swap here.
 */

#include "gex/render/null_renderer.h"

#include <string.h>

void gex_null_renderer_init(gex_null_renderer *renderer)
{
    if (renderer == NULL) {
        return;
    }
    memset(renderer, 0, sizeof(*renderer));
    renderer->initialized = 1;
}

int gex_null_renderer_present(void *ctx, const gex_frame_tick *tick)
{
    gex_null_renderer *renderer = (gex_null_renderer *)ctx;

    if (renderer == NULL || !renderer->initialized || tick == NULL) {
        return 1;
    }
    renderer->presents++;
    renderer->last_now_us = tick->now_us;
    renderer->last_steps = tick->steps;
    renderer->last_frame_index = tick->frame_index;
    return 0;
}

void gex_null_renderer_shutdown(gex_null_renderer *renderer)
{
    if (renderer == NULL) {
        return;
    }
    renderer->initialized = 0;
}
