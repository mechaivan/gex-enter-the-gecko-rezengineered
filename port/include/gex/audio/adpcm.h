/*
 * gex/audio/adpcm.h - 4-bit ADPCM decoder of GEX3D.EXE (EU).
 *
 * Evidence (static; see docs/AUDIO_MIXER_RE.md):
 *   0x00538285  decode loop: nibble extraction, step adapt, delta, clamp
 *   0x00558178  89-entry step table (int32, indexed by step index)
 *   0x00558138  16-entry adjust table (int32, indexed by nibble)
 *   0x0057C35C  history sample (int32, persists across calls)
 *   0x0057C360  step index   (int32, persists across calls)
 *
 * The decode rule is the one implemented by 0x00538285:
 *
 *   step_size = step[step_index]
 *   step_index = clamp(step_index + adjust[nibble], 0, 88)
 *   delta = (step_size >> 3)
 *         + ((nibble & 4) ? step_size        : 0)
 *         + ((nibble & 2) ? step_size >> 1   : 0)
 *         + ((nibble & 1) ? step_size >> 2   : 0)
 *   history = clamp(history +/- delta, -32768, 32767)
 *
 * Nibbles come packed two per byte, high nibble first, and the state is
 * continuous across calls (the original keeps it in two globals and never
 * resets it inside the decoder).
 *
 * The table VALUES cannot be read from the disassembly: they live in .data.
 * gex_adpcm_recovered_tables() returns the values recovered from the only
 * build available on this machine (see docs/AUDIO_MIXER_RE.md §9 for the
 * shift verification and the remaining uncertainty). Callers may pass their
 * own tables instead; nothing in this module hardcodes them.
 */

#ifndef GEX_AUDIO_ADPCM_H
#define GEX_AUDIO_ADPCM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEX_ADPCM_STEP_ENTRIES 89u
#define GEX_ADPCM_ADJUST_ENTRIES 16u
#define GEX_ADPCM_MAX_INDEX 88

typedef struct gex_adpcm_state {
    int32_t history;    /* original global 0x0057C35C */
    int32_t step_index; /* original global 0x0057C360 */
} gex_adpcm_state;

typedef struct gex_adpcm_tables {
    int32_t step[GEX_ADPCM_STEP_ENTRIES];     /* original 0x00558178 */
    int32_t adjust[GEX_ADPCM_ADJUST_ENTRIES]; /* original 0x00558138 */
} gex_adpcm_tables;

/* Tables recovered from the available build. Not proof of the reference
 * image: see docs/AUDIO_MIXER_RE.md §9 before relying on them. */
const gex_adpcm_tables *gex_adpcm_recovered_tables(void);

/* history = 0, step_index = 0 (what 0x00538731 does before the first pump). */
void gex_adpcm_reset(gex_adpcm_state *state);

/*
 * Decode one nibble and return the produced sample. Advances `state` exactly
 * like one iteration of the 0x00538285 loop. `nibble` must be 0..15; other
 * values are masked to 4 bits. The state is clamped, never rejected.
 */
int16_t gex_adpcm_apply(gex_adpcm_state *state, const gex_adpcm_tables *tables,
                        int nibble);

/*
 * Decode `samples` samples from `src_bytes` packed bytes. Returns the number
 * of samples actually written, which is min(samples, src_bytes * 2).
 *
 * The original reads exactly `samples` samples and does not look at the
 * source length; the bound here is a deliberate safety addition (deviation
 * D7 in docs/AUDIO_MIXER_RE.md §11). State is updated for the samples that
 * were produced.
 */
size_t gex_adpcm_decode(gex_adpcm_state *state, const gex_adpcm_tables *tables,
                        int16_t *dst, size_t samples, const uint8_t *src,
                        size_t src_bytes);

#ifdef __cplusplus
}
#endif

#endif /* GEX_AUDIO_ADPCM_H */
