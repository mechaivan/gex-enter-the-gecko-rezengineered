/*
 * loop_core.c - the M-13 fixed-step accumulator.
 *
 * Moved verbatim (modulo the gex_loop_core_ prefix) from
 * tools/m13-core/m13_core.c during I-3. The prototype harness now compiles
 * this file, so there is exactly one implementation of the loop mathematics
 * in the repository. See gex/core/loop_core.h and
 * docs/M13_DECOUPLE_PROPOSAL.md section 4.
 */

#include "gex/core/loop_core.h"

#include <stddef.h>

int gex_loop_core_init(gex_loop_core *s, const gex_loop_config *cfg,
                       uint64_t t0_us)
{
    if (s == NULL || cfg == NULL) {
        return -1;
    }
    if (cfg->step_us == 0 || cfg->max_steps <= 0) {
        return -2;
    }
    if (cfg->max_dt_us < cfg->step_us) {
        return -3; /* the clamp must admit at least one step */
    }
    s->cfg = *cfg;
    s->acc_us = 0;
    s->t_prev_us = t0_us;
    s->initialized = 1;
    s->steps_total = 0;
    s->presents_total = 0;
    s->discards_total = 0;
    s->max_n = 0;
    return 0;
}

int gex_loop_core_iterate(gex_loop_core *s, uint64_t now_us,
                          void (*sim_step)(void *uctx), void *uctx,
                          void (*present)(void *pctx), void *pctx)
{
    uint64_t dt;
    int n = 0;

    if (s == NULL || !s->initialized) {
        return -1;
    }
    if (now_us < s->t_prev_us) {
        /* Clock went backwards: ignore the sample, still present once. */
        if (present != NULL) {
            present(pctx);
        }
        s->presents_total++;
        return -1;
    }
    dt = now_us - s->t_prev_us;
    s->t_prev_us = now_us;
    if (dt > s->cfg.max_dt_us) {
        dt = s->cfg.max_dt_us;
    }
    s->acc_us += dt;

    if (s->cfg.original_mode) {
        if (sim_step != NULL) {
            sim_step(uctx);
        }
        n = 1;
        s->acc_us = 0; /* 1:1 cadence: no carry (matches the serial loop) */
    } else {
        while (s->acc_us >= s->cfg.step_us && n < s->cfg.max_steps) {
            if (sim_step != NULL) {
                sim_step(uctx);
            }
            s->acc_us -= s->cfg.step_us;
            n++;
        }
        if (n == s->cfg.max_steps && s->acc_us >= s->cfg.step_us) {
            s->acc_us = 0; /* drop the backlog (observable via the counter) */
            s->discards_total++;
        }
    }

    if (present != NULL) {
        present(pctx);
    }
    s->presents_total++;
    s->steps_total += (uint64_t)n;
    if ((uint64_t)n > s->max_n) {
        s->max_n = (uint64_t)n;
    }
    return n;
}

void gex_loop_core_rebase(gex_loop_core *s, uint64_t now_us)
{
    if (s == NULL || !s->initialized) {
        return;
    }
    s->acc_us = 0;
    s->t_prev_us = now_us;
}
