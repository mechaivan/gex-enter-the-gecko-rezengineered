/*
 * test_win32_time.c - QueryPerformanceCounter provider checks (I-4).
 *
 * Windows-only, unlike the rest of the suite: it verifies that the real
 * monotonic clock initialises, advances and never goes backwards. The frame
 * loop itself is tested with a scripted clock in test_app.c.
 */

#include "gex/platform/win32_time.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdio.h>

static int checks;
static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

int main(void)
{
    gex_win32_clock clock;
    uint64_t previous;
    uint64_t now;
    uint64_t elapsed;
    int i;

    CHECK(gex_win32_clock_init(NULL) == 0);
    CHECK(gex_win32_clock_now_us(NULL) == 0);
    CHECK(gex_win32_clock_freq(NULL) == 0);

    CHECK(gex_win32_clock_init(&clock) == 1);
    CHECK(clock.initialized == 1);
    CHECK(gex_win32_clock_freq(&clock) > 0);

    previous = gex_win32_clock_now_us(&clock);
    CHECK(previous < 1000000); /* the clock was started moments ago */
    for (i = 0; i < 1000; i++) {
        now = gex_win32_clock_now_us(&clock);
        CHECK(now >= previous);
        previous = now;
    }

    Sleep(20);
    elapsed = gex_win32_clock_now_us(&clock) - previous;
    CHECK(elapsed >= 5000);   /* at least 5 ms even with coarse scheduling */
    CHECK(elapsed < 2000000); /* and not two seconds for a 20 ms sleep */

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
