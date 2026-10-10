/* Linux test harness for m13_core. No mocks of the game: it verifies the
 * loop mathematics the prototype depends on (proposal §4/§7 PASO-1):
 * fixed-step rate independent of present cadence, bounded catch-up,
 * discard accounting, 1:1 Original Mode.
 *
 * Build:  gcc -std=c11 -Wall -Wextra -O2 -o test_m13_core \
 *             m13_core.c test_m13_core.c
 * Exit 0 = all pass.
 */
#include <stdio.h>
#include "m13_core.h"

static int failures = 0;
static int checks = 0;
#define CHECK(cond, ...) do { \
    checks++; \
    if (!(cond)) { failures++; printf("FAIL t%d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static void count_step(void *u) { (*(uint64_t *)u)++; }
static void count_present(void *p) { (*(uint64_t *)p)++; }

int main(void)
{
    m13_state_t s;
    m13_config_t c = { 40000, 250000, 5, 0 };
    uint64_t steps, presents;
    int n, i;

    /* T1: config validation */
    {
        m13_config_t bad = { 0, 250000, 5, 0 };
        CHECK(m13_init(&s, &bad, 0) != 0, "zero step accepted");
        bad.step_us = 40000; bad.max_dt_us = 1000;
        CHECK(m13_init(&s, &bad, 0) != 0, "max_dt<step accepted");
        bad.max_dt_us = 250000; bad.max_steps = 0;
        CHECK(m13_init(&s, &bad, 0) != 0, "max_steps=0 accepted");
        CHECK(m13_init(NULL, &c, 0) != 0, "null state accepted");
        CHECK(m13_init(&s, &c, 1000) == 0, "valid config rejected");
        CHECK(s.t_prev_us == 1000 && s.acc_us == 0, "init state");
    }

    /* T2: Original Mode is exactly 1:1 */
    {
        m13_config_t o = { 40000, 250000, 5, 1 };
        m13_init(&s, &o, 0);
        steps = presents = 0;
        for (i = 0; i < 100; i++)
            CHECK(m13_iterate(&s, (uint64_t)(i + 1) * 40000,
                              count_step, &steps,
                              count_present, &presents) == 1,
                  "original iteration %d != 1 step", i);
        CHECK(steps == 100 && presents == 100, "original 1:1 totals");
        CHECK(s.acc_us == 0, "original carries no time");
    }

    /* T3: sim rate independent of present cadence (10 s, STEP=40 ms) */
    {
        const uint64_t T = 10000000; /* 10 s */
        uint64_t t;
        /* 25 presents/s */
        m13_init(&s, &c, 0);
        steps = presents = 0;
        for (t = 40000; t <= T; t += 40000)
            m13_iterate(&s, t, count_step, &steps, count_present, &presents);
        CHECK(steps == 250 && presents == 250, "25Hz: steps=%llu presents=%llu",
              (unsigned long long)steps, (unsigned long long)presents);
        /* 120 presents/s: same 250 steps, ~1200 presents */
        m13_init(&s, &c, 0);
        steps = presents = 0;
        for (t = 8333; t <= T; t += 8333)
            m13_iterate(&s, t, count_step, &steps, count_present, &presents);
        CHECK(steps >= 249 && steps <= 251, "120Hz steps=%llu (want ~250)",
              (unsigned long long)steps);
        CHECK(presents >= 1199 && presents <= 1201, "120Hz presents=%llu",
              (unsigned long long)presents);
    }

    /* T4: catch-up bounded + T5: discard accounting */
    {
        m13_init(&s, &c, 0);
        steps = presents = 0;
        n = m13_iterate(&s, 200000, count_step, &steps, count_present, &presents);
        CHECK(n == 5 && steps == 5, "200ms yields 5 steps (got %d)", n);
        n = m13_iterate(&s, 1200000, count_step, &steps, count_present, &presents);
        /* dt=1000ms clamped to 250ms -> 6.25 steps -> 5 + 1 discard */
        CHECK(n == 5, "clamped 1000ms yields 5 steps (got %d)", n);
        CHECK(s.discards_total == 1, "one discard recorded (got %llu)",
              (unsigned long long)s.discards_total);
        CHECK(s.max_n == 5, "max_n tracks 5");
    }

    /* T6: backward clock ignored, present still runs */
    {
        m13_init(&s, &c, 50000);
        steps = presents = 0;
        n = m13_iterate(&s, 40000, count_step, &steps, count_present, &presents);
        CHECK(n == -1 && steps == 0 && presents == 1, "backward clock");
    }

    /* T7: fractional carry is exact */
    {
        m13_init(&s, &c, 0);
        steps = presents = 0;
        n = m13_iterate(&s, 50000, count_step, &steps, count_present, &presents);
        CHECK(n == 1 && s.acc_us == 10000, "carry 10ms (n=%d acc=%llu)", n,
              (unsigned long long)s.acc_us);
        n = m13_iterate(&s, 80000, count_step, &steps, count_present, &presents);
        CHECK(n == 1 && s.acc_us == 0, "carry completes step");
    }

    /* T8: 60 s variable-cadence run holds sim rate (deterministic seq) */
    {
        static const uint64_t gaps[] = { 8333, 40000, 16666, 50000, 12500 };
        uint64_t t = 0;
        m13_init(&s, &c, 0);
        steps = presents = 0;
        for (i = 0; t < 60000000; i++) {
            t += gaps[i % 5];
            m13_iterate(&s, t, count_step, &steps, count_present, &presents);
        }
        /* 60 s / 40 ms = 1500 steps, allow rounding of last gap */
        CHECK(steps >= 1499 && steps <= 1501, "60s variable: steps=%llu",
              (unsigned long long)steps);
        CHECK(s.discards_total == 0, "no discards in normal load");
    }

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
