/*
 * adpcm.c - 4-bit ADPCM decoder transcribed from 0x00538285.
 *
 * The instruction-level mapping of every step is in docs/AUDIO_MIXER_RE.md
 * section 3. Two points worth repeating here:
 *
 *   - the step index is updated BEFORE the delta is computed, but the delta
 *     uses the step size of the index the sample started with (the original
 *     loads table[index] at the top of the iteration into [ebp-0x28] and
 *     refreshes it at the end). gex_adpcm_apply() keeps that order by reading
 *     step[state->step_index] on entry;
 *   - the history clamp is [-32768, 32767], applied after the add/subtract.
 */

#include "gex/audio/adpcm.h"

/* Recovered tables. Both are the canonical IMA/DVI ADPCM tables (89 step
 * sizes, 16 index adjustments), found verbatim in the only build available at
 * exactly the relative distance the two referenced addresses imply. See
 * docs/AUDIO_MIXER_RE.md §9. */
static const gex_adpcm_tables gex_adpcm_tables_recovered = {
    /* step[89] - original 0x00558178 */
    {
        7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
        19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
        50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
        130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
        337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
        876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
        2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
        5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
    },
    /* adjust[16] - original 0x00558138 */
    {
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8,
    },
};

const gex_adpcm_tables *gex_adpcm_recovered_tables(void)
{
    return &gex_adpcm_tables_recovered;
}

void gex_adpcm_reset(gex_adpcm_state *state)
{
    if (state == NULL) {
        return;
    }
    state->history = 0;
    state->step_index = 0;
}

int16_t gex_adpcm_apply(gex_adpcm_state *state, const gex_adpcm_tables *tables,
                        int nibble)
{
    if (state == NULL || tables == NULL) {
        return 0;
    }

    nibble &= 0x0F;

    int32_t index = state->step_index;
    if (index < 0) {
        index = 0;
    } else if (index > GEX_ADPCM_MAX_INDEX) {
        index = GEX_ADPCM_MAX_INDEX;
    }

    /* Step size for THIS sample (loaded before the index is adjusted). */
    const int32_t step_size = tables->step[index];

    /* 0x00538312: step += adjust[nibble]; clamp to [0, 0x58]. */
    index += tables->adjust[nibble];
    if (index < 0) {
        index = 0;
    } else if (index > GEX_ADPCM_MAX_INDEX) {
        index = GEX_ADPCM_MAX_INDEX;
    }

    /* 0x00538348..0x00538390: delta from the three magnitude bits. */
    int32_t delta = step_size >> 3;
    if (nibble & 4) {
        delta += step_size;
    }
    if (nibble & 2) {
        delta += step_size >> 1;
    }
    if (nibble & 1) {
        delta += step_size >> 2;
    }

    /* 0x00538393..0x005383aa: bit 3 is the sign. */
    int32_t history = state->history;
    if (nibble & 8) {
        history -= delta;
    } else {
        history += delta;
    }

    /* 0x005383ad..0x005383cd: clamp to the int16 range. */
    if (history > 32767) {
        history = 32767;
    } else if (history < -32768) {
        history = -32768;
    }

    state->history = history;
    state->step_index = index;
    return (int16_t)history;
}

size_t gex_adpcm_decode(gex_adpcm_state *state, const gex_adpcm_tables *tables,
                        int16_t *dst, size_t samples, const uint8_t *src,
                        size_t src_bytes)
{
    if (state == NULL || tables == NULL || dst == NULL) {
        return 0;
    }
    if (src == NULL) {
        return 0;
    }

    size_t produced = 0;
    size_t pos = 0;

    while (produced < samples && pos < src_bytes) {
        const uint8_t byte = src[pos++];
        /* 0x005382e2: first sample is the high nibble. */
        dst[produced++] = gex_adpcm_apply(state, tables, (int)(byte >> 4));
        if (produced < samples) {
            /* 0x005382d7: second sample is the low nibble of the same byte. */
            dst[produced++] = gex_adpcm_apply(state, tables, (int)(byte & 0x0F));
        }
    }

    return produced;
}
