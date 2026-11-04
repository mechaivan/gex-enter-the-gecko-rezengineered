/*
 * gex/platform/clock.h - counter-to-microseconds conversion (I-4).
 *
 * The conversion is platform independent so it can be checked on any host.
 * The Win32 clock that feeds it lives in win32_time.h.
 */

#ifndef GEX_PLATFORM_CLOCK_H
#define GEX_PLATFORM_CLOCK_H

#include <stdint.h>

/*
 * Convert `ticks` of a counter running at `freq` Hz to microseconds.
 *
 * Truncates towards zero, like the integer arithmetic used by the rest of the
 * port. `freq == 0` returns 0 instead of dividing by zero. The quotient and
 * remainder are split so `ticks * 1000000` never has to fit in 64 bits.
 */
uint64_t gex_ticks_to_us(uint64_t ticks, uint64_t freq);

#endif /* GEX_PLATFORM_CLOCK_H */
