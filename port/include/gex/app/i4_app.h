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
#if defined(_WIN32)
#include "gex/render/d3d11_renderer.h"
#endif

/* Union to hold either null or D3D11 renderer */
typedef union {
    gex_null_renderer null_renderer;
    gex_d3d11_renderer d3d11_renderer;
} gex_renderer_union;

/*
 * Callback opcional de trabajo de juego, invocado **una vez por update
 * lógico** (0x0043E1BA), en el mismo sitio y con la misma cadencia que el
 * update del original.
 *
 * Lo usa la secuencia de cámara del menú (S-3): un paso de trayectoria por
 * update lógico, como hace el juego original (0x411B94 -> 0x53E98C avanza el
 * nodo una vez por update).
 *
 * Devuelve 0 para seguir; un valor distinto de 0 aborta el frame (la misma
 * política que los demas hooks del bucle).
 */
typedef int (*gex_i4_update_callback)(void *ctx);

struct gex_i4_app;

/*
 * Registra el callback de update. NULL lo quita. No es_threadsafe ni
 * reentrante: se llama desde el bucle, en el hilo que lo ejecuta.
 */
void gex_i4_app_set_update_callback(struct gex_i4_app *app,
                                    gex_i4_update_callback callback,
                                    void *ctx);
typedef struct gex_i4_app {
    gex_frame frame;
    gex_input input;
    gex_renderer_union renderer;
    uint64_t pre_calls; /* pre_frame hook invocations */
    uint64_t updates;   /* update hook invocations */
    int quit;           /* set by Escape, or by the platform */
    int initialized;
#if defined(_WIN32)
    int d3d11_enabled;  /* whether D3D11 renderer is active */
#endif
    /* Callback opcional de trabajo de juego por update lógico (S-3). */
    gex_i4_update_callback update_callback;
    void *update_ctx;
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

#if defined(_WIN32)
/* Enable D3D11 rendering with the specified device and swapchain */
gex_d3d11_renderer_status gex_i4_app_enable_d3d11(gex_i4_app *app,
                                                 const gex_d3d11_device *device,
                                                 const gex_d3d11_swapchain *swapchain,
                                                 const float clear_color[4]);

/* Disable D3D11 rendering and switch back to null renderer */
void gex_i4_app_disable_d3d11(gex_i4_app *app);

/* Check if D3D11 rendering is enabled */
int gex_i4_app_is_d3d11_enabled(const gex_i4_app *app);
#endif

#endif /* GEX_APP_I4_APP_H */
