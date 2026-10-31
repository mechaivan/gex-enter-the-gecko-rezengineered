/*
 * test_adpcm.c - checks for the 4-bit ADPCM decoder (0x00538285).
 *
 * No framework, no game data: exit 0 means every check passed. The golden
 * vector is computed by hand from the recovered tables and the instruction
 * order of the original; see docs/AUDIO_MIXER_RE.md §3 and §8.
 */

#include "gex/audio/adpcm.h"

#include <stdio.h>
#include <string.h>

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

static void test_recovered_tables(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    CHECK(t != NULL);
    if (t == NULL) {
        return;
    }
    CHECK(t->step[0] == 7);
    CHECK(t->step[GEX_ADPCM_STEP_ENTRIES - 1] == 32767);
    CHECK(t->adjust[0] == -1);
    CHECK(t->adjust[4] == 2);
    CHECK(t->adjust[7] == 8);
    CHECK(t->adjust[8] == -1);
    CHECK(t->adjust[15] == 8);

    int increasing = 1;
    for (size_t i = 1; i < GEX_ADPCM_STEP_ENTRIES; i++) {
        if (t->step[i] <= t->step[i - 1]) {
            increasing = 0;
        }
    }
    CHECK(increasing);
    CHECK(t->step[GEX_ADPCM_MAX_INDEX] == 32767);
}

static void test_golden_vector(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    gex_adpcm_state state;
    const uint8_t packed[2] = {0x07, 0xF0};
    int16_t out[4] = {0, 0, 0, 0};

    gex_adpcm_reset(&state);
    CHECK(state.history == 0);
    CHECK(state.step_index == 0);

    CHECK(gex_adpcm_decode(&state, t, out, 4, packed, sizeof(packed)) == 4);
    CHECK(out[0] == 0);    /* nibble 0: step 7 -> delta 0 */
    CHECK(out[1] == 11);   /* nibble 7: 7>>3 + 7 + 3 + 1 */
    CHECK(out[2] == -19);  /* nibble F: 11 - (2 + 16 + 8 + 4) */
    CHECK(out[3] == -15);  /* nibble 0: -19 + (34>>3) */
    CHECK(state.history == -15);
    CHECK(state.step_index == 15);
}

static void test_nibble_order(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    gex_adpcm_state a;
    gex_adpcm_state b;
    const uint8_t packed[1] = {0x70};

    /* High nibble first: 0x70 is the same stream as 0x07 for one byte. */
    gex_adpcm_reset(&a);
    gex_adpcm_reset(&b);
    const int16_t high = gex_adpcm_apply(&a, t, 7);
    CHECK(high == 11);
    CHECK(gex_adpcm_decode(&b, t, (int16_t[2]){0, 0}, 2, packed, 1) == 2);
    CHECK(b.history != a.history); /* b decoded two samples, a one */

    gex_adpcm_state c;
    gex_adpcm_reset(&c);
    int16_t out[2] = {0, 0};
    CHECK(gex_adpcm_decode(&c, t, out, 2, packed, 1) == 2);
    CHECK(out[0] == 11); /* high nibble 7 */
    CHECK(out[1] == 13); /* low nibble 0: step[8]=16, delta 2 */
}

static void test_state_continuity(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    const uint8_t packed[4] = {0x12, 0x34, 0x56, 0x78};
    gex_adpcm_state whole;
    gex_adpcm_state split;
    int16_t a[8];
    int16_t b[8];

    gex_adpcm_reset(&whole);
    CHECK(gex_adpcm_decode(&whole, t, a, 8, packed, sizeof(packed)) == 8);

    gex_adpcm_reset(&split);
    CHECK(gex_adpcm_decode(&split, t, b, 4, packed, 2) == 4);
    CHECK(gex_adpcm_decode(&split, t, b + 4, 4, packed + 2, 2) == 4);

    CHECK(memcmp(a, b, sizeof(a)) == 0);
    CHECK(whole.history == split.history);
    CHECK(whole.step_index == split.step_index);
}

static void test_clamp_positive(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    gex_adpcm_state state;
    gex_adpcm_reset(&state);
    int16_t v = 0;
    for (int i = 0; i < 400; i++) {
        v = gex_adpcm_apply(&state, t, 7);
    }
    CHECK(v == 32767);
    CHECK(state.history == 32767);
    CHECK(state.step_index == GEX_ADPCM_MAX_INDEX);
}

static void test_clamp_negative(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    gex_adpcm_state state;
    gex_adpcm_reset(&state);
    int16_t v = 0;
    for (int i = 0; i < 400; i++) {
        v = gex_adpcm_apply(&state, t, 15);
    }
    CHECK(v == -32768);
    CHECK(state.history == -32768);
}

static void test_step_index_bounds(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    gex_adpcm_state state;

    gex_adpcm_reset(&state);
    state.step_index = GEX_ADPCM_MAX_INDEX;
    gex_adpcm_apply(&state, t, 7);
    CHECK(state.step_index == GEX_ADPCM_MAX_INDEX);

    state.step_index = 0;
    gex_adpcm_apply(&state, t, 0);
    CHECK(state.step_index == 0);

    /* Out-of-range input is clamped, not read out of bounds. */
    state.step_index = 999;
    gex_adpcm_apply(&state, t, 0);
    CHECK(state.step_index >= 0 && state.step_index <= GEX_ADPCM_MAX_INDEX);
}

static void test_short_source(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    gex_adpcm_state state;
    const uint8_t packed[1] = {0x07};
    int16_t out[8];

    gex_adpcm_reset(&state);
    /* 8 samples requested, only 1 byte available: 2 samples produced. */
    CHECK(gex_adpcm_decode(&state, t, out, 8, packed, 1) == 2);

    gex_adpcm_reset(&state);
    CHECK(gex_adpcm_decode(&state, t, out, 8, packed, 0) == 0);
}

static void test_argument_validation(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    gex_adpcm_state state;
    int16_t out[2] = {0, 0};
    const uint8_t packed[1] = {0x07};

    gex_adpcm_reset(&state);
    CHECK(gex_adpcm_decode(NULL, t, out, 2, packed, 1) == 0);
    CHECK(gex_adpcm_decode(&state, NULL, out, 2, packed, 1) == 0);
    CHECK(gex_adpcm_decode(&state, t, NULL, 2, packed, 1) == 0);
    CHECK(gex_adpcm_decode(&state, t, out, 2, NULL, 1) == 0);
    CHECK(gex_adpcm_apply(NULL, t, 7) == 0);
    CHECK(gex_adpcm_apply(&state, NULL, 7) == 0);
    gex_adpcm_reset(NULL); /* must not crash */

    /* A nibble outside 0..15 is masked, not rejected. */
    gex_adpcm_reset(&state);
    CHECK(gex_adpcm_apply(&state, t, 0x17) == gex_adpcm_apply(&(gex_adpcm_state){0, 0}, t, 7));
}

static void test_custom_tables(void)
{
    /* The decoder must not depend on the recovered values. */
    gex_adpcm_tables flat;
    for (size_t i = 0; i < GEX_ADPCM_STEP_ENTRIES; i++) {
        flat.step[i] = 8;
    }
    for (size_t i = 0; i < GEX_ADPCM_ADJUST_ENTRIES; i++) {
        flat.adjust[i] = 0;
    }

    gex_adpcm_state state;
    gex_adpcm_reset(&state);
    /* step 8 -> delta 1 for nibble 0, 8 + 4 + 2 + 1 = 15 for nibble 7. */
    CHECK(gex_adpcm_apply(&state, &flat, 0) == 1);
    CHECK(gex_adpcm_apply(&state, &flat, 7) == 16);
    CHECK(gex_adpcm_apply(&state, &flat, 8) == 15); /* 16 - 1 */
    CHECK(state.step_index == 0);
}

static uint32_t rng_state = 0x9E3779B9u;

static uint32_t next_random(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static void test_fuzz(void)
{
    const gex_adpcm_tables *t = gex_adpcm_recovered_tables();
    uint8_t packed[64];
    int16_t out[128];

    for (int i = 0; i < 20000; i++) {
        const size_t bytes = next_random() % sizeof(packed);
        for (size_t j = 0; j < bytes; j++) {
            packed[j] = (uint8_t)(next_random() & 0xFF);
        }
        gex_adpcm_state state;
        gex_adpcm_reset(&state);
        const size_t produced = gex_adpcm_decode(&state, t, out, sizeof(out) / sizeof(out[0]), packed, bytes);
        CHECK(produced == bytes * 2);
        CHECK(state.step_index >= 0 && state.step_index <= GEX_ADPCM_MAX_INDEX);
        CHECK(state.history >= -32768 && state.history <= 32767);
    }
}

int main(void)
{
    test_recovered_tables();
    test_golden_vector();
    test_nibble_order();
    test_state_continuity();
    test_clamp_positive();
    test_clamp_negative();
    test_step_index_bounds();
    test_short_source();
    test_argument_validation();
    test_custom_tables();
    test_fuzz();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
