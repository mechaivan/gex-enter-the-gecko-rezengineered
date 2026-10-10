/* M-13 loop core — implementation. See m13_core.h and proposal §4. */
#include "m13_core.h"

int m13_init(m13_state_t *s, const m13_config_t *cfg, uint64_t t0_us)
{
    if (!s || !cfg)
        return -1;
    if (cfg->step_us == 0 || cfg->max_steps <= 0)
        return -2;
    if (cfg->max_dt_us < cfg->step_us)
        return -3; /* clamp must admit at least one step */
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

int m13_iterate(m13_state_t *s, uint64_t now_us,
                void (*sim_step)(void *uctx), void *uctx,
                void (*present)(void *pctx), void *pctx)
{
    uint64_t dt;
    int n = 0;

    if (!s || !s->initialized)
        return -1;
    if (now_us < s->t_prev_us) {
        /* Clock went backwards: ignore sample, still present once. */
        if (present)
            present(pctx);
        s->presents_total++;
        return -1;
    }
    dt = now_us - s->t_prev_us;
    s->t_prev_us = now_us;
    if (dt > s->cfg.max_dt_us)
        dt = s->cfg.max_dt_us;
    s->acc_us += dt;

    if (s->cfg.original_mode) {
        if (sim_step)
            sim_step(uctx);
        n = 1;
        s->acc_us = 0; /* 1:1 cadence: no carry (matches serial loop) */
    } else {
        while (s->acc_us >= s->cfg.step_us && n < s->cfg.max_steps) {
            if (sim_step)
                sim_step(uctx);
            s->acc_us -= s->cfg.step_us;
            n++;
        }
        if (n == s->cfg.max_steps && s->acc_us >= s->cfg.step_us) {
            s->acc_us = 0; /* drop backlog (observable via counter) */
            s->discards_total++;
        }
    }

    if (present)
        present(pctx);
    s->presents_total++;
    s->steps_total += (uint64_t)n;
    if ((uint64_t)n > s->max_n)
        s->max_n = (uint64_t)n;
    return n;
}
