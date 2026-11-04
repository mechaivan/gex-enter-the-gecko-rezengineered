/*
 * gex/core/loop_core.h - fixed-step accumulator (the M-13 loop core).
 *
 * This is the SAME code that lived in tools/m13-core/m13_core.c until I-3.
 * It was moved here so the port has one implementation and the prototype
 * harness keeps testing that implementation instead of a copy:
 *
 *   tools/m13-core/test_m13_core.c   the 120 checks, unchanged
 *   tools/m13-core/m13_core.h        compatibility shim -> this header
 *   port/src/core/loop_core.c        the only implementation
 *
 * Reference behaviour: docs/M13_DECOUPLE_PROPOSAL.md Rev.1 section 4.
 * Clock is injected (microseconds, monotonic), so the same core runs in a
 * test with a scripted clock and in the real frame loop. No dynamic
 * allocation, no threads, no platform API.
 *
 * Units: time in MICROSECONDS (u64). STEP/cadence are chosen by the caller;
 * nothing here assumes 25 FPS or 60 Hz (see gex/core/frame.h for the two
 * presets the port ships and why).
 *
 * Contract, unchanged from the prototype:
 *   - dt = now - t_prev, clamped to max_dt_us, added to the accumulator;
 *   - floor(acc/step) steps are consumed, bounded by max_steps; if the bound
 *     is hit with time still left, the remainder is dropped and counted;
 *   - original_mode forces exactly one step per iteration and carries no
 *     time (the 1:1 witness of the original serial loop);
 *   - a clock sample that goes backwards is ignored and the iteration still
 *     presents once (returns -1 to make that visible);
 *   - present() runs exactly once per iteration, even when 0 steps ran.
 */

#ifndef GEX_CORE_LOOP_CORE_H
#define GEX_CORE_LOOP_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gex_loop_config {
    uint64_t step_us;   /* fixed logical-step duration (e.g. 16667 or 40000) */
    uint64_t max_dt_us; /* anti-spiral clamp per iteration (e.g. 250000) */
    int max_steps;      /* catch-up bound per present (e.g. 5) */
    int original_mode;  /* 1 => exactly 1 step per present (1:1 witness) */
} gex_loop_config;

typedef struct gex_loop_core {
    gex_loop_config cfg;
    uint64_t acc_us;    /* accumulated, unconsumed time */
    uint64_t t_prev_us; /* last clock sample */
    int initialized;
    /* instrumentation */
    uint64_t steps_total;    /* logical steps executed */
    uint64_t presents_total; /* present() calls */
    uint64_t discards_total; /* accumulator overflows dropped */
    uint64_t max_n;          /* max steps consumed in one iteration */
} gex_loop_core;

/* Returns 0 on valid config, nonzero otherwise (state untouched). */
int gex_loop_core_init(gex_loop_core *s, const gex_loop_config *cfg,
                       uint64_t t0_us);

/*
 * Advance one loop iteration ending in a present.
 *  - dt = now - t_prev (clamped to max_dt_us), added to the accumulator;
 *  - consumes floor(acc/step) steps (bounded by max_steps; original_mode
 *    forces exactly 1), calling sim_step(uctx) for each;
 *  - calls present(pctx) exactly once.
 * Returns the number of steps consumed this iteration, or -1 when the clock
 * went backwards (sample ignored, present still runs once) or when the state
 * is not initialised (present does NOT run in that case).
 */
int gex_loop_core_iterate(gex_loop_core *s, uint64_t now_us,
                          void (*sim_step)(void *uctx), void *uctx,
                          void (*present)(void *pctx), void *pctx);

/*
 * Re-anchor the clock and drop the accumulator. This is what a paused loop
 * needs on resume: without it, the first iteration after a pause would see
 * the whole pause as dt (clamped, but still a burst of catch-up steps).
 * Does nothing when the state is not initialised.
 */
void gex_loop_core_rebase(gex_loop_core *s, uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif /* GEX_CORE_LOOP_CORE_H */
