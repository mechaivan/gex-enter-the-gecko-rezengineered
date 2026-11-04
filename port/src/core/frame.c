/*
 * frame.c - the native frame loop (I-3).
 *
 * The loop mathematics are NOT reimplemented here: gex_loop_core_iterate()
 * owns the accumulator and calls our two trampolines, which is what keeps the
 * frame loop and the tools/m13-core harness on the same code. This file only
 * adds the phase order, the audio tick, the error policy and the lifecycle.
 */

#include "gex/core/frame.h"

#include <stddef.h>
#include <string.h>

gex_frame_config gex_frame_config_original(void)
{
    gex_frame_config cfg;
    cfg.step_us = GEX_FRAME_STEP_ORIGINAL_US;
    cfg.max_dt_us = GEX_FRAME_MAX_DT_US;
    cfg.max_steps = GEX_FRAME_MAX_STEPS;
    cfg.original_mode = 1;
    return cfg;
}

gex_frame_config gex_frame_config_decoupled(void)
{
    gex_frame_config cfg;
    cfg.step_us = GEX_FRAME_STEP_60HZ_US;
    cfg.max_dt_us = GEX_FRAME_MAX_DT_US;
    cfg.max_steps = GEX_FRAME_MAX_STEPS;
    cfg.original_mode = 0;
    return cfg;
}

/* sim_step trampoline: one logical update (0x0043E1BA). */
static void frame_update(void *ctx)
{
    gex_frame *f = (gex_frame *)ctx;

    if (f->iter_error) {
        return; /* a failed frame does not keep updating */
    }
    f->iter_steps++;
    f->tick.steps = f->iter_steps;
    if (f->hooks.update != NULL &&
        f->hooks.update(f->hooks.ctx, &f->tick) != 0) {
        f->iter_error = 1;
        f->iter_status = GEX_FRAME_ERR_UPDATE;
    }
}

/* present trampoline: audio tick then presentation, once per frame. */
static void frame_present(void *ctx)
{
    gex_frame *f = (gex_frame *)ctx;

    if (f->iter_error) {
        return; /* the frame never reached its presentation phase */
    }

    if (f->audio != NULL) {
        const gex_audio_status status = gex_audio_sched_tick(f->audio);
        f->audio_ticks++;
        f->last_audio = status;
        if (status != GEX_AUDIO_OK) {
            f->iter_error = 1;
            f->iter_status = GEX_FRAME_ERR_AUDIO;
            return;
        }
    }

    f->tick.steps = f->iter_steps;
    if (f->hooks.present != NULL &&
        f->hooks.present(f->hooks.ctx, &f->tick) != 0) {
        f->iter_error = 1;
        f->iter_status = GEX_FRAME_ERR_PRESENT;
        return;
    }
    f->frames++;
}

gex_frame_status gex_frame_init(gex_frame *f, const gex_frame_config *cfg,
                                const gex_frame_hooks *hooks,
                                gex_audio_sched *audio, uint64_t t0_us)
{
    if (f == NULL || cfg == NULL) {
        return GEX_FRAME_ERR_ARGUMENT;
    }
    memset(f, 0, sizeof(*f));
    if (gex_loop_core_init(&f->core, cfg, t0_us) != 0) {
        memset(f, 0, sizeof(*f));
        return GEX_FRAME_ERR_ARGUMENT;
    }
    f->cfg = *cfg;
    if (hooks != NULL) {
        f->hooks = *hooks;
    }
    f->audio = audio;
    f->last_status = GEX_FRAME_OK;
    f->last_audio = GEX_AUDIO_OK;
    f->initialized = 1;
    return GEX_FRAME_OK;
}

gex_frame_status gex_frame_run(gex_frame *f, uint64_t now_us)
{
    if (f == NULL) {
        return GEX_FRAME_ERR_ARGUMENT;
    }
    if (!f->initialized) {
        return GEX_FRAME_ERR_STATE;
    }
    if (f->paused) {
        return GEX_FRAME_OK;
    }

    f->tick.now_us = now_us;
    f->tick.step_us = f->cfg.step_us;
    f->tick.frame_index = f->frames;
    f->tick.steps = 0;
    f->last_steps = 0;
    f->iter_steps = 0;
    f->iter_error = 0;
    f->iter_status = GEX_FRAME_OK;

    if (f->hooks.pre_frame != NULL) {
        f->pre_frames++;
        if (f->hooks.pre_frame(f->hooks.ctx, &f->tick) != 0) {
            f->aborted++;
            f->last_status = GEX_FRAME_ERR_PRE_FRAME;
            /* Clean accumulator and re-anchor clock like other errors. */
            f->core.acc_us = 0;
            f->core.t_prev_us = now_us;
            return f->last_status;
        }
    }

    const int n = gex_loop_core_iterate(&f->core, now_us, frame_update, f,
                                        frame_present, f);

    if (f->iter_error) {
        /* The steps before the failure did run: count them, then drop the
         * unconsumed remainder so the next run never replays this frame. */
        f->steps += f->iter_steps;
        f->last_steps = f->iter_steps;
        f->core.acc_us = 0;
        f->core.t_prev_us = now_us;
        f->aborted++;
        f->last_status = f->iter_status;
        return f->last_status;
    }

    if (n < 0) {
        /* Backwards clock: the core ignored the sample and still presented. */
        f->clock_backwards++;
        f->last_status = GEX_FRAME_WARN_CLOCK;
        return f->last_status;
    }

    f->last_steps = (uint32_t)n;
    f->steps += (uint64_t)n;
    f->last_status = GEX_FRAME_OK;
    return GEX_FRAME_OK;
}

void gex_frame_pause(gex_frame *f)
{
    if (f == NULL || !f->initialized || f->paused) {
        return;
    }
    f->paused = 1;
    gex_audio_sched_pause(f->audio);
}

gex_frame_status gex_frame_resume(gex_frame *f, uint64_t now_us)
{
    if (f == NULL) {
        return GEX_FRAME_ERR_ARGUMENT;
    }
    if (!f->initialized) {
        return GEX_FRAME_ERR_STATE;
    }
    if (!f->paused) {
        return GEX_FRAME_OK; /* idempotent; does not touch the accumulator */
    }
    f->paused = 0;
    gex_loop_core_rebase(&f->core, now_us);
    gex_audio_sched_resume(f->audio);
    return GEX_FRAME_OK;
}

void gex_frame_shutdown(gex_frame *f)
{
    if (f == NULL || !f->initialized) {
        return;
    }
    memset(&f->hooks, 0, sizeof(f->hooks));
    f->audio = NULL;
    f->paused = 0;
    f->initialized = 0;
    f->last_status = GEX_FRAME_OK;
}

const char *gex_frame_status_name(gex_frame_status status)
{
    switch (status) {
    case GEX_FRAME_OK:
        return "ok";
    case GEX_FRAME_WARN_CLOCK:
        return "clock went backwards (recovered)";
    case GEX_FRAME_ERR_ARGUMENT:
        return "invalid argument";
    case GEX_FRAME_ERR_STATE:
        return "frame not initialised or already shut down";
    case GEX_FRAME_ERR_PRE_FRAME:
        return "pre-frame callback failed";
    case GEX_FRAME_ERR_UPDATE:
        return "update callback failed";
    case GEX_FRAME_ERR_AUDIO:
        return "audio scheduler failed";
    case GEX_FRAME_ERR_PRESENT:
        return "present callback failed";
    default:
        return "unknown frame status";
    }
}
