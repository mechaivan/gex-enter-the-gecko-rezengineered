/*
 * gex/app/i4_app.h - the platform-independent I-4 application core.
 *
 * It wires together the pieces that I-4 adds and keeps them testable without
 * a window:
 *
 *   gex_input          keyboard levels and edges
 *   gex_null_renderer  the present phase (draws nothing)
 *   gex_frame          the I-3 frame loop, with three bound hooks
 *
 * The Win32 entry point owns the window, pumps messages into `app.input` and
 * calls gex_i4_app_step() with samples from the real clock. Tests do exactly
 * the same with a scripted clock.
 *
 * The three hooks are explicit stubs, not game logic:
 *   pre_frame  records the call; I-4 has no input-to-game translation yet
 *   update     records the call; there is no simulation
 *   present    forwards to the null renderer
 */

#ifndef GEX_APP_I4_APP_H
#define GEX_APP_I4_APP_H

#include <stdint.h>

#include "gex/core/frame.h"
#include "gex/input/key_state.h"
#include "gex/render/null_renderer.h"

typedef struct gex_i4_app {
    gex_frame frame;
    gex_input input;
    gex_null_renderer renderer;
    uint64_t pre_calls; /* pre_frame hook invocations */
    uint64_t updates;   /* update hook invocations */
    int quit;           /* set by Escape, or by the platform */
    int initialized;
} gex_i4_app;

/* Validates the frame config through gex_frame_init; audio is not attached. */
gex_frame_status gex_i4_app_init(gex_i4_app *app,
                                 const gex_frame_config *cfg,
                                 uint64_t t0_us);

/*
 * One iteration: run the frame with `now_us`, then interpret Escape and clear
 * the input edges. Returns the gex_frame_status of the iteration unchanged.
 * On a frame error the input edges are still cleared, so a failing frame never
 * leaves a stale press for the next one.
 */
gex_frame_status gex_i4_app_step(gex_i4_app *app, uint64_t now_us);

int gex_i4_app_should_quit(const gex_i4_app *app);

/* Idempotent: shuts the frame down and marks the app uninitialised. */
void gex_i4_app_shutdown(gex_i4_app *app);

#endif /* GEX_APP_I4_APP_H */
