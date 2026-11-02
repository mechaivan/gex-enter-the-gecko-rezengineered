/*
 * gex/platform/win32_time.h - monotonic clock for the Win32 build (I-4).
 *
 * QueryPerformanceCounter is the smallest monotonic source the original
 * toolchain already exposes; QueryPerformanceFrequency gives its rate. The
 * clock is started once and every sample is reported as microseconds since
 * that start, which is the unit gex_frame_run() consumes.
 */

#ifndef GEX_PLATFORM_WIN32_TIME_H
#define GEX_PLATFORM_WIN32_TIME_H

#include <stdint.h>

typedef struct gex_win32_clock {
    uint64_t freq;        /* ticks per second */
    uint64_t start_ticks; /* counter value at init */
    int initialized;
} gex_win32_clock;

/* Returns 1 on success, 0 if the performance counter is unavailable. */
int gex_win32_clock_init(gex_win32_clock *clock);

/* Microseconds since gex_win32_clock_init(); 0 if not initialised. */
uint64_t gex_win32_clock_now_us(gex_win32_clock *clock);

uint64_t gex_win32_clock_freq(const gex_win32_clock *clock);

#endif /* GEX_PLATFORM_WIN32_TIME_H */
