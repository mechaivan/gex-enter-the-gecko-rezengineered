/*
 * clock.c - counter ticks to microseconds (I-4).
 *
 * Kept free of Windows headers: the Win32 provider passes the raw
 * QueryPerformanceCounter values, and the tests can exercise the arithmetic
 * without creating a window or a high-resolution timer.
 */

#include "gex/platform/clock.h"

uint64_t gex_ticks_to_us(uint64_t ticks, uint64_t freq)
{
    uint64_t seconds;
    uint64_t remainder;

    if (freq == 0) {
        return 0;
    }
    seconds = ticks / freq;
    remainder = ticks % freq;
    return seconds * 1000000u + (remainder * 1000000u) / freq;
}
