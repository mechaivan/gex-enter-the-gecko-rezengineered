/*
 * gex/core/frame.h - the native frame loop (I-3).
 *
 * One pass of this loop reproduces the shape measured in
 * docs/FRAME_STEP_TRACE.md section 3.1 for the original frame step
 * 0x0043EA36, minus the rendering that does not exist yet:
 *
 *   pre-frame         0x0043F80D  input and frame preparation
 *   update (N times)  0x0043E1BA  the per-frame update
 *   audio scheduler   0x005380B3  called once per presented frame
 *   present           0x0043DC3A render + 0x0054053E drain + 0x005404F6 swap
 *
 * The trailing leaf 0x0046300C is NOT modelled: its semantics are still
 * unidentified (docs/FRAME_STEP_TRACE.md section 3.1), and this port does not
 * invent behaviour for it. When I-4 has the evidence it becomes a fourth
 * hook after present, not a reinterpretation of this one.
 *
 * What is real here and what is not:
 *   - real: the clock, the accumulator, the callback order, the audio
 *     scheduler integration, the error/pause/shutdown policy, the counters;
 *   - not real: pre_frame, update and present are injected callbacks. In I-3
 *     the tests and framedemo supply recording stubs. Nothing of the game
 *     logic or the renderer runs. This is a loop, not a playable build.
 *
 * Time units and cadence (read this before changing a constant):
 *   - The original's PRESENTATION period is 40 ms (25 FPS, p50 measured over
 *     1 723 frames). That is a property of the blocking swap, not of the
 *     simulation.
 *   - The update's own timestamp is computed as
 *     (GetTickCount() - level_start) / 16.666, i.e. its quantum is 60 Hz
 *     (docs/FRAME_STEP_TRACE.md section 3.3). Those are two different clocks
 *     and this loop keeps them apart: step_us is the LOGICAL rate, the
 *     presentation rate is whatever cadence the caller runs gex_frame_run at.
 *   - Therefore the decoupled preset uses 16667 us (60 Hz), the granularity
 *     the update already speaks, and NOT 40000 us. The original preset keeps
 *     40000 us only because original_mode ignores step_us entirely (exactly
 *     one update per present), so it is the 1:1 compatibility witness.
 *   - The final value of STEP is still pending E-2 (M13 proposal section 8).
 *     The presets are a documented choice, not a recovered constant.
 */

#ifndef GEX_CORE_FRAME_H
#define GEX_CORE_FRAME_H

#include <stdint.h>

#include "gex/audio/sched.h"
#include "gex/core/loop_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 25 Hz: the measured presentation period of the original swap. */
#define GEX_FRAME_STEP_ORIGINAL_US 40000u
/* 60 Hz: the quantum of the update's own timestamp (constant 16.666). */
#define GEX_FRAME_STEP_60HZ_US 16667u
/* M13 proposal section 4: anti-spiral clamp and catch-up bound. */
#define GEX_FRAME_MAX_DT_US 250000u
#define GEX_FRAME_MAX_STEPS 5

typedef enum gex_frame_status {
    GEX_FRAME_OK = 0,
    /* Recovered: the clock sample went backwards. The sample was ignored and
     * the frame was still presented (same policy as the core). */
    GEX_FRAME_WARN_CLOCK = 1,
    GEX_FRAME_ERR_ARGUMENT = -1,
    GEX_FRAME_ERR_STATE = -2,     /* not initialised, or already shut down */
    GEX_FRAME_ERR_PRE_FRAME = -3, /* pre_frame callback returned nonzero */
    GEX_FRAME_ERR_UPDATE = -4,    /* update callback returned nonzero */
    GEX_FRAME_ERR_AUDIO = -5,     /* scheduler returned a non-OK status */
    GEX_FRAME_ERR_PRESENT = -6    /* present callback returned nonzero */
} gex_frame_status;

/*
 * Per-iteration context handed to every callback.
 *
 *   now_us      the clock sample of this iteration
 *   step_us     the configured logical step
 *   frame_index frames presented before this iteration (0-based)
 *   steps       steps executed so far in this iteration; for pre_frame it is
 *               0, for update it is the 1-based step number, and for present
 *               it is the total number of steps this frame consumed
 */
typedef struct gex_frame_tick {
    uint64_t now_us;
    uint64_t step_us;
    uint64_t frame_index;
    uint32_t steps;
} gex_frame_tick;

/* 0 = ok, nonzero = error. A callback that returns nonzero aborts the frame:
 * no later phase runs (see the error policy below). */
typedef int (*gex_frame_callback)(void *ctx, const gex_frame_tick *tick);

typedef struct gex_frame_hooks {
    gex_frame_callback pre_frame; /* 0x0043F80D; optional */
    gex_frame_callback update;    /* 0x0043E1BA; optional */
    gex_frame_callback present;   /* 0x0043DC3A + drain + swap; optional */
    void *ctx;                    /* passed verbatim to all three */
} gex_frame_hooks;

/* The frame config IS the accumulator config: one type, no second copy of
 * step_us/max_dt_us/max_steps that could drift from the core. */
typedef gex_loop_config gex_frame_config;

/* 25 FPS presentation, exactly one update per presented frame, no carry.
 * This is the witness that the scaffolding does not change the original
 * serial behaviour (M13 ORG-1). */
gex_frame_config gex_frame_config_original(void);

/* Free presentation cadence, 60 Hz logical steps, catch-up up to 5 steps and
 * a 250 ms clamp. Not a claim about what the final port will ship. */
gex_frame_config gex_frame_config_decoupled(void);

typedef struct gex_frame {
    gex_frame_config cfg;
    gex_loop_core core; /* the M-13 accumulator; the only clock owner */
    gex_audio_sched *audio; /* optional; borrowed, never owned */
    gex_frame_hooks hooks;
    gex_frame_tick tick;
    gex_frame_status last_status;
    gex_audio_status last_audio; /* last status the scheduler returned */
    /* counters (never reset except by gex_frame_init) */
    uint64_t frames;        /* frames presented */
    uint64_t steps;         /* logical updates entered (a failing update
                             * counts as entered; it did run) */
    uint64_t pre_frames;    /* pre_frame callbacks made */
    uint64_t audio_ticks;   /* scheduler calls made (0 when audio is NULL) */
    uint64_t aborted;       /* frames stopped by a callback/audio error */
    uint64_t clock_backwards;
    /* state */
    int initialized;
    int paused;
    /* per-iteration scratch, not part of the contract */
    uint32_t last_steps;
    uint32_t iter_steps;
    int iter_error;
    gex_frame_status iter_status;
} gex_frame;

/*
 * Lifecycle. init validates the config through gex_loop_core_init and zeroes
 * every counter; `audio` may be NULL, in which case no scheduler is ticked.
 * A frame that was never initialised, or was shut down, refuses to run.
 * init is also the safe way to restart a shut-down frame.
 */
gex_frame_status gex_frame_init(gex_frame *f, const gex_frame_config *cfg,
                                const gex_frame_hooks *hooks,
                                gex_audio_sched *audio, uint64_t t0_us);

/*
 * One loop pass at clock sample `now_us`:
 *
 *   pre_frame -> [update x N] -> audio tick (once) -> present
 *
 * The audio tick is INSIDE the once-per-frame present phase, so a catch-up
 * iteration that runs three updates still pumps audio exactly once. That is
 * the original contract: the frame loop calls 0x005380B3 once per pass
 * (0x00463205, 0x0053A5FB, 0x0053FCB7), not once per update.
 *
 * Error policy (all of it observable, none of it silent):
 *   - a pre_frame error skips update, audio and present;
 *   - an update error skips the remaining updates, audio and present;
 *   - an audio error skips present and is reported as GEX_FRAME_ERR_AUDIO
 *     with the exact scheduler status in `f->last_audio`;
 *   - a present error is reported after the audio tick already happened.
 *   - after any error the accumulator is emptied and the clock re-anchored
 *     at `now_us`, so the failed step is never replayed by the next run.
 *     The caller still owns the decision to close the audio stream: this
 *     loop never closes, stops or reopens anything it does not own.
 *
 * Returns GEX_FRAME_OK, GEX_FRAME_WARN_CLOCK (recovered), or a negative
 * GEX_FRAME_ERR_*. While paused, run does nothing and returns OK.
 */
gex_frame_status gex_frame_run(gex_frame *f, uint64_t now_us);

/*
 * Pause suspends the audio scheduler (freezing a device cursor when the sink
 * supports it) and makes run a no-op. resume re-arms both and re-anchors the
 * clock at `now_us`, so the pause duration is not turned into catch-up steps.
 * Both are idempotent; pause/resume on an uninitialised frame do nothing.
 */
void gex_frame_pause(gex_frame *f);
gex_frame_status gex_frame_resume(gex_frame *f, uint64_t now_us);

/*
 * Release the frame's references (hooks and scheduler pointer) and mark it
 * shut down. Idempotent. The audio stream and scheduler belong to the caller
 * and are not touched; counters stay readable until the next init.
 */
void gex_frame_shutdown(gex_frame *f);

const char *gex_frame_status_name(gex_frame_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_CORE_FRAME_H */
