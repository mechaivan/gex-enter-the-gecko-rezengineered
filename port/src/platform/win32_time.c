/*
 * win32_time.c - QueryPerformanceCounter provider for I-4.
 *
 * Only this file (and win32_window.c) includes windows.h on the app path. The
 * arithmetic lives in clock.c so it is testable without a Windows timer.
 */

#include "gex/platform/win32_time.h"

#include "gex/platform/clock.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

int gex_win32_clock_init(gex_win32_clock *clock)
{
    LARGE_INTEGER freq;
    LARGE_INTEGER start;

    if (clock == NULL) {
        return 0;
    }
    clock->freq = 0;
    clock->start_ticks = 0;
    clock->initialized = 0;

    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0) {
        return 0;
    }
    if (!QueryPerformanceCounter(&start)) {
        return 0;
    }
    clock->freq = (uint64_t)freq.QuadPart;
    clock->start_ticks = (uint64_t)start.QuadPart;
    clock->initialized = 1;
    return 1;
}

uint64_t gex_win32_clock_now_us(gex_win32_clock *clock)
{
    LARGE_INTEGER now;

    if (clock == NULL || !clock->initialized || clock->freq == 0) {
        return 0;
    }
    if (!QueryPerformanceCounter(&now)) {
        return 0;
    }
    if ((uint64_t)now.QuadPart < clock->start_ticks) {
        return 0; /* never report a negative elapsed time */
    }
    return gex_ticks_to_us((uint64_t)now.QuadPart - clock->start_ticks,
                           clock->freq);
}

uint64_t gex_win32_clock_freq(const gex_win32_clock *clock)
{
    return (clock != NULL) ? clock->freq : 0;
}
