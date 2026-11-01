/*
 * test_frame.c - deterministic checks for the native frame loop (I-3).
 *
 * No window, no game, no sound card: the clock is a number the test passes
 * in, the three phases are recording stubs, and the audio scheduler runs over
 * a synthetic stream whose sink counts calls. The only thing borrowed from
 * the audio path is the documented pump, so the integration being tested is
 * the real one.
 *
 * What is asserted, in the order the brief asks for:
 *   - exact callback order pre -> update -> audio -> present;
 *   - exactly one update and one present in Original 1:1 mode;
 *   - several updates and one present when the accumulator catches up;
 *   - the scheduler runs exactly once per presented frame, after the update
 *     and before the present;
 *   - no scheduler call when a previous phase failed;
 *   - backwards clock, oversized delta, catch-up bound, discard accounting;
 *   - pause/resume, audio errors, shutdown and safe re-initialisation.
 */

#include "gex/audio/adpcm.h"
#include "gex/core/frame.h"

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

/* ------------------------------------------------------------- recorder */

#define LOG_MAX 256

typedef struct {
    char log[LOG_MAX];
    int n;
    int pre_calls;
    int update_calls;
    int present_calls;
    int fail_pre_at;     /* 1-based call number; 0 = never */
    int fail_update_at;
    int fail_present_at;
    uint64_t present_now[16];
    uint64_t present_frame_index[16];
    uint32_t present_steps[16];
    uint64_t ticks_at_present[16];
    uint32_t update_steps[16];
    gex_frame_hooks hooks;
} recorder;

static void rec_push(recorder *r, char c)
{
    if (r->n < LOG_MAX - 1) {
        r->log[r->n++] = c;
        r->log[r->n] = '\0';
    }
}

static gex_audio_sched *g_audio; /* only read by the present stub */

static int rec_pre(void *ctx, const gex_frame_tick *tick)
{
    recorder *r = (recorder *)ctx;
    r->pre_calls++;
    rec_push(r, 'P');
    CHECK(tick->steps == 0);
    return (r->fail_pre_at == r->pre_calls) ? 1 : 0;
}

static int rec_update(void *ctx, const gex_frame_tick *tick)
{
    recorder *r = (recorder *)ctx;
    r->update_calls++;
    rec_push(r, 'U');
    if (r->update_calls <= 16) {
        r->update_steps[r->update_calls - 1] = tick->steps;
    }
    return (r->fail_update_at == r->update_calls) ? 1 : 0;
}

static int rec_present(void *ctx, const gex_frame_tick *tick)
{
    recorder *r = (recorder *)ctx;
    r->present_calls++;
    rec_push(r, 'R');
    if (r->present_calls <= 16) {
        r->present_now[r->present_calls - 1] = tick->now_us;
        r->present_frame_index[r->present_calls - 1] = tick->frame_index;
        r->present_steps[r->present_calls - 1] = tick->steps;
        r->ticks_at_present[r->present_calls - 1] =
            (g_audio != NULL) ? g_audio->ticks : 0;
    }
    return (r->fail_present_at == r->present_calls) ? 1 : 0;
}

static gex_frame_hooks *rec_hooks(recorder *r)
{
    r->hooks.pre_frame = rec_pre;
    r->hooks.update = rec_update;
    r->hooks.present = rec_present;
    r->hooks.ctx = r;
    return &r->hooks;
}

/* ----------------------------------------------------------- fake audio */

typedef struct {
    int submits;
    int stops;
    int pauses;
    int resumes;
    int fail_submit;
} sink;

static int sink_submit(void *ctx, const int16_t *samples, size_t count,
                       uint32_t rate)
{
    sink *s = (sink *)ctx;
    (void)samples;
    (void)count;
    (void)rate;
    s->submits++;
    if (s->fail_submit) {
        s->fail_submit = 0;
        return 1;
    }
    return 0;
}

static void sink_stop(void *ctx)
{
    ((sink *)ctx)->stops++;
}

static void sink_pause(void *ctx)
{
    ((sink *)ctx)->pauses++;
}

static void sink_resume(void *ctx)
{
    ((sink *)ctx)->resumes++;
}

static gex_audio_backend make_sink_backend(sink *s)
{
    gex_audio_backend backend;
    backend.ctx = s;
    backend.submit = sink_submit;
    backend.stop = sink_stop;
    backend.cursor = NULL; /* no cursor: the scheduler drains at its pace */
    backend.pause = sink_pause;
    backend.resume = sink_resume;
    return backend;
}

/* 64 KiB of zero nibbles: 16 pumps of 0x1000 before the source runs out. */
static uint8_t g_payload[0x10000];

/* A stream ready to pump, built without a SLAG file: the pump only needs the
 * payload window and the ADPCM tables. */
static void audio_open(gex_audio_stream *stream, gex_audio_sched *sched,
                       sink *s)
{
    memset(stream, 0, sizeof(*stream));
    gex_audio_buffer_init(&stream->buffer, make_sink_backend(s), 22050);
    stream->tables = gex_adpcm_recovered_tables();
    gex_adpcm_reset(&stream->adpcm);
    stream->payload = g_payload;
    stream->payload_size = sizeof(g_payload);
    stream->cursor = 0;
    stream->source_open = 1;
    stream->rate = 22050;
    stream->channels = 1;
    stream->bits = 16;
    gex_audio_buffer_play(&stream->buffer);
    gex_audio_sched_init(sched, stream);
    g_audio = sched;
}

/* --------------------------------------------------------------- suites */

/* T1: Original 1:1, exact order, one update and one present per frame. */
static void test_original_order(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_original();
    int i;

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    for (i = 0; i < 3; i++) {
        CHECK(gex_frame_run(&f, (uint64_t)(i + 1) * 40000) == GEX_FRAME_OK);
        CHECK(f.last_steps == 1);
    }

    CHECK(strcmp(r.log, "PURPURPUR") == 0);
    CHECK(r.pre_calls == 3);
    CHECK(r.update_calls == 3);
    CHECK(r.present_calls == 3);
    CHECK(f.frames == 3);
    CHECK(f.steps == 3);
    CHECK(f.audio_ticks == 3);
    CHECK(f.pre_frames == 3);
    CHECK(f.aborted == 0);
    CHECK(f.clock_backwards == 0);

    /* One scheduler tick per presented frame, already counted inside present:
     * that is the "after update, before present" order. */
    CHECK(sched.ticks == 3);
    CHECK(r.ticks_at_present[0] == 1);
    CHECK(r.ticks_at_present[1] == 2);
    CHECK(r.ticks_at_present[2] == 3);

    /* The core and the frame agree on the totals. */
    CHECK(f.core.steps_total == 3);
    CHECK(f.core.presents_total == 3);

    /* Tick fields: frame index is 0-based, update reports 1-based steps. */
    CHECK(r.present_frame_index[0] == 0);
    CHECK(r.present_frame_index[2] == 2);
    CHECK(r.present_now[1] == 80000);
    CHECK(r.present_steps[0] == 1);
    CHECK(r.update_steps[0] == 1);
    CHECK(r.update_steps[2] == 1);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T2: catch-up - several updates, one present, one audio tick. */
static void test_catch_up(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    /* 60 ms at a 16.667 ms step: three steps, 9.999 ms of carry. */
    CHECK(gex_frame_run(&f, 60000) == GEX_FRAME_OK);
    CHECK(f.last_steps == 3);
    CHECK(r.update_calls == 3);
    CHECK(r.present_calls == 1);
    CHECK(f.frames == 1);
    CHECK(f.steps == 3);
    CHECK(f.audio_ticks == 1);
    CHECK(sched.ticks == 1); /* not once per update */
    CHECK(strcmp(r.log, "PUUUR") == 0);
    CHECK(r.present_steps[0] == 3);
    CHECK(r.update_steps[0] == 1);
    CHECK(r.update_steps[1] == 2);
    CHECK(r.update_steps[2] == 3);
    CHECK(f.core.acc_us == 9999);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T3: catch-up bound and discard accounting. */
static void test_discard(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    /* 250 ms of real time, 5 steps max, the rest is dropped and counted. */
    CHECK(gex_frame_run(&f, 250000) == GEX_FRAME_OK);
    CHECK(f.last_steps == GEX_FRAME_MAX_STEPS);
    CHECK(r.update_calls == GEX_FRAME_MAX_STEPS);
    CHECK(f.frames == 1);
    CHECK(f.audio_ticks == 1);
    CHECK(f.core.discards_total == 1);
    CHECK(f.core.acc_us == 0);
    CHECK(f.core.max_n == GEX_FRAME_MAX_STEPS);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T4: an oversized delta is clamped before it reaches the accumulator. */
static void test_delta_clamp(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    /* Ten seconds of stall must behave exactly like the 250 ms clamp. */
    CHECK(gex_frame_run(&f, 10000000) == GEX_FRAME_OK);
    CHECK(f.last_steps == GEX_FRAME_MAX_STEPS);
    CHECK(f.core.discards_total == 1);
    CHECK(f.core.t_prev_us == 10000000);
    CHECK(f.core.acc_us == 0);
    CHECK(f.audio_ticks == 1);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T5: a backwards clock is ignored, the frame still presents once. */
static void test_clock_backwards(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_original();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 100000) ==
          GEX_FRAME_OK);

    CHECK(gex_frame_run(&f, 90000) == GEX_FRAME_WARN_CLOCK);
    CHECK(f.last_steps == 0);
    CHECK(r.update_calls == 0);
    CHECK(r.present_calls == 1);
    CHECK(f.frames == 1);
    CHECK(f.clock_backwards == 1);
    CHECK(f.audio_ticks == 1);
    CHECK(f.core.t_prev_us == 100000); /* the bad sample did not move it */
    CHECK(strcmp(r.log, "PR") == 0);

    /* The next good sample is measured from the last good one. */
    CHECK(gex_frame_run(&f, 140000) == GEX_FRAME_OK);
    CHECK(f.last_steps == 1);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T6: a pre-frame error stops the frame before update, audio and present. */
static void test_pre_frame_error(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_original();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    r.fail_pre_at = 1;
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    CHECK(gex_frame_run(&f, 40000) == GEX_FRAME_ERR_PRE_FRAME);
    CHECK(r.pre_calls == 1);
    CHECK(r.update_calls == 0);
    CHECK(sched.ticks == 0);
    CHECK(r.present_calls == 0);
    CHECK(f.frames == 0);
    CHECK(f.aborted == 1);
    CHECK(f.last_status == GEX_FRAME_ERR_PRE_FRAME);
    CHECK(strcmp(r.log, "P") == 0);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T7: an update error skips the remaining updates, audio and present. */
static void test_update_error(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    r.fail_update_at = 2;
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    CHECK(gex_frame_run(&f, 60000) == GEX_FRAME_ERR_UPDATE);
    CHECK(r.update_calls == 2); /* the third never runs */
    CHECK(sched.ticks == 0);
    CHECK(r.present_calls == 0);
    CHECK(f.frames == 0);
    CHECK(f.aborted == 1);
    CHECK(f.last_steps == 2);
    CHECK(f.steps == 2);
    CHECK(f.core.acc_us == 0); /* no replay of the failed frame */
    CHECK(strcmp(r.log, "PUU") == 0);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T8: an audio error skips the present and keeps the exact status. */
static void test_audio_error(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_original();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    s.fail_submit = 1;
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    CHECK(gex_frame_run(&f, 40000) == GEX_FRAME_ERR_AUDIO);
    CHECK(f.last_audio == GEX_AUDIO_ERR_BACKEND);
    CHECK(f.audio_ticks == 1);
    CHECK(r.present_calls == 0);
    CHECK(f.frames == 0);
    CHECK(f.aborted == 1);
    CHECK(f.core.acc_us == 0);
    CHECK(strcmp(r.log, "PU") == 0);

    /* The sink recovers: the next frame runs normally, no replay. */
    CHECK(gex_frame_run(&f, 80000) == GEX_FRAME_OK);
    CHECK(f.last_audio == GEX_AUDIO_OK);
    CHECK(f.frames == 1);
    /* Two updates ran in total: the one in the aborted frame (it did execute,
     * it just never reached present) and this one. */
    CHECK(f.steps == 2);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T9: a present error is reported after the audio tick already ran. */
static void test_present_error(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_original();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    r.fail_present_at = 1;
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    CHECK(gex_frame_run(&f, 40000) == GEX_FRAME_ERR_PRESENT);
    CHECK(sched.ticks == 1);
    CHECK(r.present_calls == 1);
    CHECK(f.frames == 0); /* not counted as presented */
    CHECK(f.aborted == 1);
    CHECK(strcmp(r.log, "PUR") == 0);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T10: pause freezes every phase; resume does not turn the pause into
 * catch-up steps. */
static void test_pause_resume(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    CHECK(gex_frame_run(&f, 20000) == GEX_FRAME_OK);
    CHECK(f.steps == 1);
    const int updates_before = r.update_calls;

    gex_frame_pause(&f);
    gex_frame_pause(&f); /* idempotent */
    CHECK(f.paused == 1);
    CHECK(s.pauses == 1);
    CHECK(sched.paused == 1);

    /* Five seconds paused: no phase runs at all. */
    CHECK(gex_frame_run(&f, 5020000) == GEX_FRAME_OK);
    CHECK(r.update_calls == updates_before);
    CHECK(r.present_calls == 1);
    CHECK(f.frames == 1);
    CHECK(sched.ticks == 1);
    CHECK(f.steps == 1);

    CHECK(gex_frame_resume(&f, 5020000) == GEX_FRAME_OK);
    gex_frame_resume(&f, 5020000); /* idempotent, does not re-rebase */
    CHECK(f.paused == 0);
    CHECK(s.resumes == 1);
    CHECK(sched.paused == 0);
    CHECK(f.core.t_prev_us == 5020000);

    /* 20 ms after resume: one step, not a burst from the pause. */
    CHECK(gex_frame_run(&f, 5040000) == GEX_FRAME_OK);
    CHECK(f.last_steps == 1);
    CHECK(f.core.discards_total == 0);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

/* T11: lifecycle - refuse uninitialised use, survive shutdown, re-init. */
static void test_lifecycle(void)
{
    recorder r;
    gex_frame f;
    gex_frame_config bad;
    const gex_frame_config good = gex_frame_config_original();

    memset(&r, 0, sizeof(r));
    memset(&f, 0, sizeof(f));
    CHECK(gex_frame_run(&f, 0) == GEX_FRAME_ERR_STATE);
    CHECK(gex_frame_run(NULL, 0) == GEX_FRAME_ERR_ARGUMENT);
    CHECK(gex_frame_init(NULL, &good, NULL, NULL, 0) == GEX_FRAME_ERR_ARGUMENT);
    CHECK(gex_frame_init(&f, NULL, NULL, NULL, 0) == GEX_FRAME_ERR_ARGUMENT);

    bad = good;
    bad.step_us = 0;
    CHECK(gex_frame_init(&f, &bad, NULL, NULL, 0) == GEX_FRAME_ERR_ARGUMENT);
    CHECK(f.initialized == 0);
    bad = good;
    bad.max_steps = 0;
    CHECK(gex_frame_init(&f, &bad, NULL, NULL, 0) == GEX_FRAME_ERR_ARGUMENT);
    bad = good;
    bad.max_dt_us = 1000;
    CHECK(gex_frame_init(&f, &bad, NULL, NULL, 0) == GEX_FRAME_ERR_ARGUMENT);

    /* A frame with no hooks and no audio is still a valid loop. */
    CHECK(gex_frame_init(&f, &good, NULL, NULL, 0) == GEX_FRAME_OK);
    CHECK(gex_frame_run(&f, 40000) == GEX_FRAME_OK);
    CHECK(f.frames == 1);
    CHECK(f.steps == 1);
    CHECK(f.audio_ticks == 0); /* no scheduler attached, none invented */

    gex_frame_shutdown(&f);
    CHECK(gex_frame_run(&f, 80000) == GEX_FRAME_ERR_STATE);
    gex_frame_shutdown(&f); /* idempotent */

    /* Re-initialisation is safe and starts from zero counters. */
    CHECK(gex_frame_init(&f, &good, rec_hooks(&r), NULL, 80000) ==
          GEX_FRAME_OK);
    CHECK(f.frames == 0);
    CHECK(f.steps == 0);
    CHECK(f.pre_frames == 0);
    CHECK(f.audio_ticks == 0);
    CHECK(f.core.steps_total == 0);
    CHECK(gex_frame_run(&f, 120000) == GEX_FRAME_OK);
    CHECK(f.frames == 1);
    gex_frame_shutdown(&f);

    /* Status names are never NULL. */
    CHECK(gex_frame_status_name(GEX_FRAME_OK) != NULL);
    CHECK(gex_frame_status_name(GEX_FRAME_WARN_CLOCK) != NULL);
    CHECK(gex_frame_status_name(GEX_FRAME_ERR_AUDIO) != NULL);
    CHECK(gex_frame_status_name((gex_frame_status)-99) != NULL);
}

/* T12: the logical rate does not depend on the presentation cadence. */
static void test_rate_independence(void)
{
    recorder r;
    sink s;
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_frame f;
    const gex_frame_config cfg = gex_frame_config_decoupled();
    uint64_t t;
    int i;

    memset(&r, 0, sizeof(r));
    memset(&s, 0, sizeof(s));
    audio_open(&stream, &sched, &s);
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);

    /* 400 ms presented at 25 Hz. */
    for (i = 1; i <= 10; i++) {
        CHECK(gex_frame_run(&f, (uint64_t)i * 40000) == GEX_FRAME_OK);
    }
    CHECK(f.frames == 10);
    CHECK(f.steps == 23); /* 400 ms / 16.667 ms */
    CHECK(f.audio_ticks == 10);
    CHECK(f.core.discards_total == 0);

    /* Same 400 ms presented at 200 Hz: same 23 logical steps. */
    CHECK(gex_frame_init(&f, &cfg, rec_hooks(&r), &sched, 0) == GEX_FRAME_OK);
    for (t = 5000; t <= 400000; t += 5000) {
        CHECK(gex_frame_run(&f, t) == GEX_FRAME_OK);
    }
    CHECK(f.frames == 80);
    CHECK(f.steps == 23);
    CHECK(f.audio_ticks == 80); /* one tick per presented frame, always */
    CHECK(f.core.discards_total == 0);

    gex_frame_shutdown(&f);
    gex_audio_stream_close(&stream);
}

int main(void)
{
    test_original_order();
    test_catch_up();
    test_discard();
    test_delta_clamp();
    test_clock_backwards();
    test_pre_frame_error();
    test_update_error();
    test_audio_error();
    test_present_error();
    test_pause_resume();
    test_lifecycle();
    test_rate_independence();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
