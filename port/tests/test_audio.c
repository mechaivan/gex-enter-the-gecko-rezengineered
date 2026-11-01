/*
 * test_audio.c - checks for the buffer, the streaming pump and the player.
 *
 * Everything runs without hardware and without a single byte of the retail
 * game: the fixtures are built here from the format documented in
 * docs/SAG_CONTAINER.md and the pump sizes recovered in docs/AUDIO_MIXER_RE.md.
 */

#include "gex/audio/sched.h"
#include "gex/audio/stream.h"

#include <stdio.h>
#include <stdlib.h>
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

/* ------------------------------------------------------------- fixtures */

typedef struct {
    uint8_t bytes[16384];
    size_t size;
} fixture;

static void put_u16(fixture *f, uint16_t v)
{
    f->bytes[f->size++] = (uint8_t)(v & 0xFF);
    f->bytes[f->size++] = (uint8_t)((v >> 8) & 0xFF);
}

static void put_u32(fixture *f, uint32_t v)
{
    f->bytes[f->size++] = (uint8_t)(v & 0xFF);
    f->bytes[f->size++] = (uint8_t)((v >> 8) & 0xFF);
    f->bytes[f->size++] = (uint8_t)((v >> 16) & 0xFF);
    f->bytes[f->size++] = (uint8_t)((v >> 24) & 0xFF);
}

static void put_tag(fixture *f, const char tag[4])
{
    memcpy(f->bytes + f->size, tag, 4);
    f->size += 4;
}

static void put_payload(fixture *f, size_t count, uint8_t value)
{
    memset(f->bytes + f->size, value, count);
    f->size += count;
}

static void put_header(fixture *f)
{
    f->size = 0;
    put_tag(f, "SLAG");
    put_u32(f, 0x2E);
    put_tag(f, "WAVE");
}

/* fmt + data; `declared` may exceed `available` to build a truncated file. */
static void build(fixture *f, uint16_t channels, uint16_t bits, uint32_t rate,
                  size_t declared, size_t available)
{
    put_header(f);
    put_tag(f, "fmt ");
    put_u32(f, 18);
    put_u16(f, 1);
    put_u16(f, channels);
    put_u32(f, rate);
    put_u32(f, rate * channels * (uint32_t)(bits / 8u));
    put_u16(f, (uint16_t)(channels * (bits / 8u)));
    put_u16(f, bits);
    put_u16(f, 0);
    put_tag(f, "data");
    put_u32(f, (uint32_t)declared);
    put_payload(f, available, 0x07);
}

/* --------------------------------------------------------------- capture */

typedef struct {
    int16_t samples[131072];
    size_t capacity;
    size_t count;
    uint32_t rate;
    int submits;
    int stops;
    int pauses;
    int resumes;
    int fail_next;
    uint32_t play_cursor; /* only read by make_cursor_backend */
} capture;

static int capture_submit(void *ctx, const int16_t *samples, size_t count,
                          uint32_t rate)
{
    capture *c = (capture *)ctx;
    c->submits++;
    c->rate = rate;
    if (c->fail_next) {
        return 1;
    }
    if (c->count + count > c->capacity) {
        return 1;
    }
    memcpy(c->samples + c->count, samples, count * sizeof(int16_t));
    c->count += count;
    return 0;
}

static void capture_stop(void *ctx)
{
    ((capture *)ctx)->stops++;
}

static void capture_pause(void *ctx)
{
    ((capture *)ctx)->pauses++;
}

static void capture_resume(void *ctx)
{
    ((capture *)ctx)->resumes++;
}

static gex_audio_backend make_backend(capture *c)
{
    gex_audio_backend backend;
    backend.ctx = c;
    backend.submit = capture_submit;
    backend.stop = capture_stop;
    backend.cursor = NULL;
    backend.pause = capture_pause;
    backend.resume = capture_resume;
    return backend;
}

static uint32_t capture_cursor(void *ctx)
{
    return ((capture *)ctx)->play_cursor;
}

static gex_audio_backend make_cursor_backend(capture *c)
{
    gex_audio_backend backend = make_backend(c);
    backend.cursor = capture_cursor;
    return backend;
}

/* ---------------------------------------------------------------- buffer */

static void test_buffer_lifecycle(void)
{
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    gex_audio_buffer buffer;
    gex_audio_buffer_init(&buffer, make_backend(&cap), 22050);

    CHECK(buffer.write_offset == 0);
    CHECK(buffer.playing == 0);
    CHECK(buffer.halves == 0);

    int16_t *half = gex_audio_buffer_lock(&buffer, GEX_AUDIO_HALF_BYTES);
    CHECK(half != NULL);
    CHECK(half == buffer.pcm);
    CHECK(gex_audio_buffer_lock(&buffer, GEX_AUDIO_HALF_BYTES) == NULL);
    CHECK(gex_audio_buffer_lock(&buffer, GEX_AUDIO_CHUNK_BYTES) == NULL);

    for (size_t i = 0; i < GEX_AUDIO_HALF_SAMPLES; i++) {
        half[i] = (int16_t)(i % 1000);
    }

    CHECK(gex_audio_buffer_unlock(&buffer, GEX_AUDIO_HALF_BYTES) == GEX_AUDIO_OK);
    CHECK(buffer.halves == 1);
    CHECK(buffer.samples_submitted == GEX_AUDIO_HALF_SAMPLES);
    CHECK(cap.submits == 1);
    CHECK(cap.count == GEX_AUDIO_HALF_SAMPLES);
    CHECK(cap.rate == 22050);
    CHECK(cap.samples[0] == 0);
    CHECK(cap.samples[999] == 999);
    CHECK(cap.samples[1000] == 0);

    CHECK(gex_audio_buffer_flip(&buffer) == GEX_AUDIO_HALF_BYTES);
    half = gex_audio_buffer_lock(&buffer, GEX_AUDIO_HALF_BYTES);
    CHECK(half == buffer.pcm + GEX_AUDIO_HALF_SAMPLES);
    CHECK(gex_audio_buffer_unlock(&buffer, GEX_AUDIO_HALF_BYTES) == GEX_AUDIO_OK);
    CHECK(buffer.halves == 2);
    CHECK(gex_audio_buffer_flip(&buffer) == 0);

    gex_audio_buffer_play(&buffer);
    CHECK(buffer.playing == 1);
    gex_audio_buffer_stop(&buffer);
    CHECK(buffer.playing == 0);
    CHECK(buffer.stops == 1);
    CHECK(cap.stops == 1);

    CHECK(gex_audio_buffer_unlock(&buffer, GEX_AUDIO_HALF_BYTES) ==
          GEX_AUDIO_ERR_ARGUMENT);
}

static void test_buffer_backend_failure(void)
{
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = 16; /* too small for a half */

    gex_audio_buffer buffer;
    gex_audio_buffer_init(&buffer, make_backend(&cap), 11025);
    CHECK(gex_audio_buffer_lock(&buffer, GEX_AUDIO_HALF_BYTES) != NULL);
    CHECK(gex_audio_buffer_unlock(&buffer, GEX_AUDIO_HALF_BYTES) ==
          GEX_AUDIO_ERR_BACKEND);
}

/* ---------------------------------------------------------------- stream */

static void test_open_rejects(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    /* No "fmt " before "data": has_format == 0. */
    put_header(&f);
    put_tag(&f, "data");
    put_u32(&f, 4);
    put_payload(&f, 4, 0x07);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_FORMAT);

    build(&f, 2, 16, 22050, 8, 8);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_FORMAT);

    build(&f, 1, 8, 22050, 8, 8);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_FORMAT);

    build(&f, 1, 16, 0, 8, 8);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_FORMAT);

    build(&f, 1, 16, 22050, 0, 0);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_SOURCE);

    CHECK(gex_audio_stream_open(NULL, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_ARGUMENT);
    CHECK(gex_audio_stream_open(&stream, NULL, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_ARGUMENT);
    CHECK(gex_audio_stream_open(&stream, &sag, NULL, f.size,
                                make_backend(&cap)) == GEX_AUDIO_ERR_ARGUMENT);
}

static void test_stream_short_payload(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    build(&f, 1, 16, 22050, 0x800, 0x800);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_OK);
    CHECK(stream.payload_size == 0x800);
    CHECK(stream.rate == 22050);
    CHECK(stream.channels == 1);
    CHECK(stream.bits == 16);
    CHECK(stream.payload_truncated == 0);

    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);
    CHECK(stream.buffer.playing == 1);
    CHECK(stream.buffer.halves == 1);   /* the pre-fill of 0x00538731 */
    CHECK(stream.chunks == 1);
    CHECK(stream.short_chunks == 1);    /* 0x800 < 0x1000 ends the source */
    CHECK(stream.source_open == 0);
    CHECK(stream.samples_decoded == 0x1000);

    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK); /* silent half */
    CHECK(stream.silence_sent == 1);
    CHECK(stream.finished == 0);
    CHECK(stream.buffer.halves == 2);

    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK); /* Stop */
    CHECK(stream.finished == 1);
    CHECK(stream.buffer.stops == 1);
    CHECK(cap.stops == 1);

    /* The original keeps issuing Stop if it is pumped again. */
    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.buffer.stops == 2);

    CHECK(cap.submits == 2);
    CHECK(cap.count == 2 * GEX_AUDIO_HALF_SAMPLES);
    CHECK(cap.rate == 22050);

    /* Past the decoded samples everything is the zero padding of the short
     * chunk plus the silent half: no stale half reaches the backend. */
    int silent_tail = 1;
    for (size_t i = 0x1000; i < cap.count; i++) {
        if (cap.samples[i] != 0) {
            silent_tail = 0;
        }
    }
    CHECK(silent_tail);

    gex_audio_stream_close(&stream);
}

static void test_stream_exact_multiple(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    /* 0x1000 bytes: the first pump reads a full chunk, so the source is only
     * dropped on the next call with want == 0 (the original's out-of-bounds
     * memset case, deviation D8). */
    build(&f, 1, 16, 22050, 0x1000, 0x1000);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_OK);

    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);
    CHECK(stream.source_open == 1);
    CHECK(stream.samples_decoded == 0x2000);

    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.source_open == 0);
    CHECK(stream.short_chunks == 1);
    CHECK(stream.samples_decoded == 0x2000);

    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.buffer.stops == 0);
    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.buffer.stops == 1);
}

static void test_stream_matches_one_shot_decode(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    /* Three chunks: two full ones and a short tail. */
    build(&f, 1, 16, 22050, 0x2000 + 0x321, 0x2000 + 0x321);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_OK);

    const uint8_t *payload = (const uint8_t *)f.bytes + sag.data_offset;
    int16_t expected[0x5000];
    gex_adpcm_state state;
    gex_adpcm_reset(&state);
    const size_t total =
        gex_adpcm_decode(&state, gex_adpcm_recovered_tables(), expected,
                         sizeof(expected) / sizeof(expected[0]), payload,
                         sag.data_size);
    CHECK(total == sag.data_size * 2);
    CHECK(total == 0x2000 * 2 + 0x321 * 2);

    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);
    while (!stream.finished) {
        CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    }

    /* The concatenation of the submitted halves starts with exactly the same
     * samples as a single decode of the whole payload. */
    CHECK(cap.count >= total);
    CHECK(memcmp(cap.samples, expected, total * sizeof(int16_t)) == 0);

    gex_audio_stream_close(&stream);
}

static void test_stream_truncated_payload(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    build(&f, 1, 16, 22050, 0x2000, 0x100);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.data_complete == 0);

    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_OK);
    CHECK(stream.payload_truncated == 1);
    CHECK(stream.payload_size == 0x100);
    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);
    CHECK(stream.samples_decoded == 0x200);
}

static void test_stream_errors(void)
{
    CHECK(gex_audio_stream_play(NULL) == GEX_AUDIO_ERR_ARGUMENT);
    CHECK(gex_audio_stream_pump(NULL) == GEX_AUDIO_ERR_ARGUMENT);
    gex_audio_stream_close(NULL); /* must not crash */

    gex_audio_stream zeroed;
    memset(&zeroed, 0, sizeof(zeroed));
    gex_audio_stream_close(&zeroed);

    /* No backend at all: samples are produced and dropped, not crashed on. */
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    build(&f, 1, 16, 22050, 0x40, 0x40);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    gex_audio_backend none;
    none.ctx = NULL;
    none.submit = NULL;
    none.stop = NULL;
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size, none) == GEX_AUDIO_OK);
    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);
    CHECK(stream.samples_decoded == 0x80);
    CHECK(stream.buffer.halves == 1);
}

static void test_status_names(void)
{
    const gex_audio_status all[] = {
        GEX_AUDIO_OK,          GEX_AUDIO_ERR_ARGUMENT, GEX_AUDIO_ERR_FORMAT,
        GEX_AUDIO_ERR_SOURCE,  GEX_AUDIO_ERR_BACKEND,
    };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        const char *name = gex_audio_status_name(all[i]);
        CHECK(name != NULL);
        CHECK(name[0] != '\0');
        for (size_t j = 0; j < i; j++) {
            CHECK(strcmp(name, gex_audio_status_name(all[j])) != 0);
        }
    }
    CHECK(strcmp(gex_audio_status_name((gex_audio_status)99), "unknown") == 0);
}

static void test_stream_play_cursor_gate(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    build(&f, 1, 16, 22050, 0x2000, 0x2000);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_cursor_backend(&cap)) == GEX_AUDIO_OK);

    /* The pre-fill happens while the buffer is not playing: no gate. */
    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);
    CHECK(stream.buffer.halves == 1);
    CHECK(stream.deferred == 0);
    CHECK(stream.buffer.write_offset == GEX_AUDIO_HALF_BYTES);

    /* Play cursor still in the first half: the second half can be filled. */
    cap.play_cursor = 0;
    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.buffer.halves == 2);
    CHECK(stream.buffer.write_offset == 0);
    CHECK(stream.deferred == 0);

    /* Now the first half is about to be overwritten while it is still being
     * played: 0x00538111 waits for the cursor to reach 0x4000. */
    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.deferred == 1);
    CHECK(stream.buffer.halves == 2);

    cap.play_cursor = 0x4000;
    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.deferred == 1);
    CHECK(stream.buffer.halves == 3);

    gex_audio_stream_close(&stream);
}

/* ---------------------------------------------------------------- sched */

static void test_sched_lifecycle(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    gex_audio_sched sched;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    build(&f, 1, 16, 22050, 0x2000, 0x2000);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_cursor_backend(&cap)) == GEX_AUDIO_OK);

    gex_audio_sched_init(&sched, &stream);
    CHECK(sched.ticks == 0);
    CHECK(sched.fills == 0);
    CHECK(gex_audio_sched_due(&sched) == 0); /* nothing before play */

    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK); /* pre-fill */
    CHECK(stream.buffer.halves == 1);
    CHECK(stream.buffer.write_offset == GEX_AUDIO_HALF_BYTES);

    /* Cursor still in half 0: the half about to be written is free. */
    cap.play_cursor = 0;
    CHECK(gex_audio_sched_due(&sched) == 1);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(stream.buffer.halves == 2);
    CHECK(sched.fills == 1);
    CHECK(sched.deferred == 0);

    /* Now half 0 would be overwritten while it plays: deferred, no pump. */
    CHECK(gex_audio_sched_due(&sched) == 0);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(sched.deferred == 1);
    CHECK(sched.fills == 1);
    CHECK(stream.buffer.halves == 2);

    /* Cursor reaches the midpoint: the gate opens. The payload is exactly two
     * chunks, so this pump sees want == 0 (D8) and closes the source. */
    cap.play_cursor = GEX_AUDIO_HALF_BYTES;
    CHECK(gex_audio_sched_due(&sched) == 1);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(stream.buffer.halves == 3);
    CHECK(stream.source_open == 0);

    /* Silent half, then the Stop branch. */
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(stream.buffer.halves == 4);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(stream.finished == 1);
    CHECK(sched.finished == 1);
    CHECK(cap.stops == 1);

    /* Once finished, ticks do nothing at all. */
    const uint64_t ticks = sched.ticks;
    CHECK(gex_audio_sched_due(&sched) == 0);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(sched.ticks == ticks + 1);
    CHECK(stream.buffer.halves == 4);

    gex_audio_stream_close(&stream);
}

static void test_sched_no_cursor_drain(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    gex_audio_sched sched;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    build(&f, 1, 16, 22050, 0x2000, 0x2000);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_backend(&cap)) == GEX_AUDIO_OK);
    gex_audio_sched_init(&sched, &stream);
    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);

    int guard = 0;
    while (!sched.finished && guard++ < 16) {
        CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    }
    CHECK(sched.finished == 1);
    CHECK(stream.buffer.halves == 4);
    CHECK(sched.fills == 3);    /* the three pumps after the pre-fill */
    CHECK(sched.deferred == 0); /* a sink without cursor never defers */
    CHECK(cap.submits == 4);

    gex_audio_stream_close(&stream);
}

static void test_sched_pause_resume(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    gex_audio_sched sched;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    build(&f, 1, 16, 22050, 0x2000, 0x2000);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_cursor_backend(&cap)) == GEX_AUDIO_OK);
    gex_audio_sched_init(&sched, &stream);
    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);

    gex_audio_sched_pause(&sched);
    CHECK(sched.paused == 1);
    CHECK(cap.pauses == 1);
    CHECK(gex_audio_sched_due(&sched) == 0);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(sched.fills == 0);
    CHECK(sched.deferred == 0); /* a paused tick is not a deferral */
    CHECK(stream.buffer.halves == 1);

    gex_audio_sched_resume(&sched);
    CHECK(sched.paused == 0);
    CHECK(cap.resumes == 1);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_OK);
    CHECK(stream.buffer.halves == 2);
    CHECK(sched.fills == 1);

    /* Pausing or resuming twice must not call the backend twice. */
    gex_audio_sched_pause(&sched);
    gex_audio_sched_pause(&sched);
    CHECK(cap.pauses == 2);
    gex_audio_sched_resume(&sched);
    gex_audio_sched_resume(&sched);
    CHECK(cap.resumes == 2);

    gex_audio_stream_close(&stream);
}

static void test_sched_backend_error(void)
{
    fixture f;
    gex_sag sag;
    gex_audio_stream stream;
    gex_audio_sched sched;
    capture cap;
    memset(&cap, 0, sizeof(cap));
    cap.capacity = sizeof(cap.samples) / sizeof(cap.samples[0]);

    build(&f, 1, 16, 22050, 0x2000, 0x2000);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size,
                                make_cursor_backend(&cap)) == GEX_AUDIO_OK);
    gex_audio_sched_init(&sched, &stream);
    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);

    cap.fail_next = 1;
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_ERR_BACKEND);
    CHECK(sched.last_status == GEX_AUDIO_ERR_BACKEND);
    CHECK(stream.buffer.halves == 2); /* the half was produced, not delivered */

    gex_audio_stream_close(&stream);
}

static void test_sched_arguments(void)
{
    gex_audio_sched sched;
    gex_audio_sched_init(&sched, NULL);
    CHECK(gex_audio_sched_due(&sched) == 0);
    CHECK(gex_audio_sched_tick(&sched) == GEX_AUDIO_ERR_ARGUMENT);
    CHECK(gex_audio_sched_due(NULL) == 0);
    CHECK(gex_audio_sched_tick(NULL) == GEX_AUDIO_ERR_ARGUMENT);
    gex_audio_sched_pause(NULL);
    gex_audio_sched_resume(NULL);
    gex_audio_sched_init(NULL, NULL);
}

int main(void)
{
    test_buffer_lifecycle();
    test_buffer_backend_failure();
    test_open_rejects();
    test_stream_short_payload();
    test_stream_exact_multiple();
    test_stream_matches_one_shot_decode();
    test_stream_truncated_payload();
    test_stream_play_cursor_gate();
    test_stream_errors();
    test_sched_lifecycle();
    test_sched_no_cursor_drain();
    test_sched_pause_resume();
    test_sched_backend_error();
    test_sched_arguments();
    test_status_names();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
