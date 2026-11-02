/*
 * gex/render/null_renderer.h - the I-4 renderer that draws nothing.
 *
 * This is deliberately not a renderer stub waiting to be filled in later:
 * it is the declared I-4 behaviour. The present hook is connected to the
 * frame loop and returns success, but no pixel, buffer, Glide call or swap
 * exists behind it. When a real renderer arrives it replaces this module at
 * the `present` hook; the frame loop does not change.
 */

#ifndef GEX_RENDER_NULL_RENDERER_H
#define GEX_RENDER_NULL_RENDERER_H

#include <stdint.h>

#include "gex/core/frame.h"

typedef struct gex_null_renderer {
    uint64_t presents;     /* present callbacks that completed */
    uint64_t last_now_us;  /* clock sample of the last present */
    uint64_t last_steps;   /* logical steps of the last presented frame */
    uint64_t last_frame_index;
    int initialized;
} gex_null_renderer;

void gex_null_renderer_init(gex_null_renderer *renderer);

/* gex_frame_callback: records the tick and returns 0 (success). */
int gex_null_renderer_present(void *ctx, const gex_frame_tick *tick);

/* Idempotent. Counters stay readable until the next init. */
void gex_null_renderer_shutdown(gex_null_renderer *renderer);

#endif /* GEX_RENDER_NULL_RENDERER_H */
