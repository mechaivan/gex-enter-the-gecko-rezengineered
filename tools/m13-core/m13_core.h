/* M-13 loop core (SIM/PRESENT decoupling) — pure C, no platform API.
 *
 * Reference behaviour from docs/M13_DECOUPLE_PROPOSAL.md Rev.1 §4:
 * fixed-step accumulator. Clock is injected (microseconds, monotonic)
 * so the same core runs in the Linux test harness and in the Windows
 * shim. No dynamic allocation, no threads.
 *
 * Units: time in MICROSECONDS (u64). STEP/CADENCE chosen by caller.
 */
#ifndef M13_CORE_H
#define M13_CORE_H

#include <stdint.h>

typedef struct {
    uint64_t step_us;      /* fixed sim-step duration (e.g. 40000 or 33333) */
    uint64_t max_dt_us;    /* anti-spiral clamp per iteration (e.g. 250000) */
    int      max_steps;    /* catch-up bound per present (e.g. 5) */
    int      original_mode;/* 1 => exactly 1 step per present (1:1 fallback) */
} m13_config_t;

typedef struct {
    m13_config_t cfg;
    uint64_t acc_us;       /* accumulated, unconsumed time */
    uint64_t t_prev_us;    /* last clock sample */
    int      initialized;
    /* instrumentation (prototype only) */
    uint64_t steps_total;  /* sim steps executed */
    uint64_t presents_total;
    uint64_t discards_total; /* accumulator overflows dropped */
    uint64_t max_n;          /* max steps consumed in one iteration */
} m13_state_t;

/* Returns 0 on valid config, nonzero otherwise (state untouched). */
int m13_init(m13_state_t *s, const m13_config_t *cfg, uint64_t t0_us);

/*
 * Advance one loop iteration ending in a present.
 *  - dt = now - t_prev (clamped to max_dt_us), added to accumulator.
 *  - consumes floor(acc/step) steps (bounded by max_steps; original_mode
 *    forces exactly 1), calls sim_step(uctx) for each.
 *  - calls present(pctx) exactly once.
 * Returns steps consumed this iteration, or -1 on clock going backwards
 * (sample ignored, present still runs once).
 */
int m13_iterate(m13_state_t *s, uint64_t now_us,
                void (*sim_step)(void *uctx), void *uctx,
                void (*present)(void *pctx), void *pctx);

#endif /* M13_CORE_H */
