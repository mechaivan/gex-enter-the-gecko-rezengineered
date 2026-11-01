/*
 * framedemo.c - run the native frame loop with a scripted clock (I-3).
 *
 * This is the executable evidence for I-3: the real gex_frame loop, the real
 * M-13 accumulator and the real audio scheduler, driven by numbers instead of
 * a wall clock so the output is reproducible. The three phases are printing
 * stubs; no game logic and no renderer exist yet (that is I-4).
 *
 *   framedemo                          original 1:1, 10 frames at 40 ms
 *   framedemo --decoupled              free cadence, 60 Hz steps
 *   framedemo --decoupled --dt 60000   one present, three catch-up steps
 *   framedemo --frames 5 --dt 250000   catch-up bound + discard accounting
 *   framedemo --audio                  attach a synthetic ADPCM source
 *
 * Usage: framedemo [--decoupled] [--frames N] [--dt US] [--audio] [--help]
 */

#include "gex/audio/adpcm.h"
#include "gex/core/frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int submits;
    int stops;
} demo_sink;

static int sink_submit(void *ctx, const int16_t *samples, size_t count,
                       uint32_t rate)
{
    demo_sink *s = (demo_sink *)ctx;
    (void)samples;
    (void)count;
    (void)rate;
    s->submits++;
    return 0;
}

static void sink_stop(void *ctx)
{
    ((demo_sink *)ctx)->stops++;
}

/* The pump only needs a payload window; this is zeroed ADPCM, not a game
 * asset, and it is generated here rather than shipped. */
static uint8_t g_payload[0x8000];

typedef struct {
    int frames_done;
    int quiet;
} demo_ctx;

static int demo_pre(void *ctx, const gex_frame_tick *tick)
{
    (void)ctx;
    (void)tick;
    return 0; /* 0x0043F80D: nothing to read yet, I-4 owns the input */
}

static int demo_update(void *ctx, const gex_frame_tick *tick)
{
    (void)ctx;
    (void)tick;
    return 0; /* 0x0043E1BA: no game logic yet */
}

static int demo_present(void *ctx, const gex_frame_tick *tick)
{
    demo_ctx *d = (demo_ctx *)ctx;
    if (!d->quiet) {
        printf("frame %2llu: now=%8llu us  steps=%u  present\n",
               (unsigned long long)tick->frame_index,
               (unsigned long long)tick->now_us, (unsigned)tick->steps);
    }
    d->frames_done++;
    return 0; /* 0x0043DC3A + drain + swap: I-4 replaces this */
}

static void usage(const char *argv0)
{
    printf("usage: %s [--decoupled] [--frames N] [--dt US] [--audio]\n",
           argv0);
    printf("  --decoupled  free cadence with 60 Hz logical steps\n");
    printf("  --frames N   number of loop passes (default 10)\n");
    printf("  --dt US      clock advance per pass in microseconds (default "
           "40000)\n");
    printf("  --audio      attach the audio scheduler over a synthetic "
           "source\n");
}

int main(int argc, char **argv)
{
    gex_frame_config cfg = gex_frame_config_original();
    int decoupled = 0;
    int audio = 0;
    int frames = 10;
    uint64_t dt = GEX_FRAME_STEP_ORIGINAL_US;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--decoupled") == 0) {
            decoupled = 1;
        } else if (strcmp(argv[i], "--audio") == 0) {
            audio = 1;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dt") == 0 && i + 1 < argc) {
            dt = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }
    if (frames <= 0 || dt == 0) {
        fprintf(stderr, "frames and dt must be positive\n");
        return 2;
    }
    if (decoupled) {
        cfg = gex_frame_config_decoupled();
    }

    demo_sink sink;
    memset(&sink, 0, sizeof(sink));
    gex_audio_stream stream;
    gex_audio_sched sched;
    gex_audio_sched *audio_ptr = NULL;
    memset(&stream, 0, sizeof(stream));
    if (audio) {
        gex_audio_backend backend;
        backend.ctx = &sink;
        backend.submit = sink_submit;
        backend.stop = sink_stop;
        backend.cursor = NULL;
        backend.pause = NULL;
        backend.resume = NULL;
        gex_audio_buffer_init(&stream.buffer, backend, 22050);
        stream.tables = gex_adpcm_recovered_tables();
        gex_adpcm_reset(&stream.adpcm);
        stream.payload = g_payload;
        stream.payload_size = sizeof(g_payload);
        stream.source_open = 1;
        stream.rate = 22050;
        stream.channels = 1;
        stream.bits = 16;
        gex_audio_buffer_play(&stream.buffer);
        gex_audio_sched_init(&sched, &stream);
        audio_ptr = &sched;
    }

    demo_ctx ctx;
    ctx.frames_done = 0;
    ctx.quiet = 0;
    gex_frame_hooks hooks;
    hooks.pre_frame = demo_pre;
    hooks.update = demo_update;
    hooks.present = demo_present;
    hooks.ctx = &ctx;

    gex_frame f;
    const gex_frame_status init = gex_frame_init(&f, &cfg, &hooks, audio_ptr, 0);
    if (init != GEX_FRAME_OK) {
        fprintf(stderr, "init failed: %s\n", gex_frame_status_name(init));
        return 1;
    }

    printf("config:  %s, step=%llu us, max_dt=%llu us, max_steps=%d\n",
           decoupled ? "decoupled" : "original 1:1",
           (unsigned long long)cfg.step_us,
           (unsigned long long)cfg.max_dt_us, cfg.max_steps);
    printf("clock:   injected (no wall clock), dt=%llu us per pass\n",
           (unsigned long long)dt);
    printf("audio:   %s\n", audio ? "synthetic ADPCM source, counting sink"
                                  : "none attached");

    int rc = 0;
    for (i = 1; i <= frames; i++) {
        const gex_frame_status status = gex_frame_run(&f, (uint64_t)i * dt);
        if (status != GEX_FRAME_OK) {
            fprintf(stderr, "frame %d failed: %s\n", i - 1,
                    gex_frame_status_name(status));
            rc = 1;
            break;
        }
    }

    printf("totals:  frames=%llu steps=%llu presents=%llu audio_ticks=%llu "
           "discards=%llu aborted=%llu backwards=%llu\n",
           (unsigned long long)f.frames, (unsigned long long)f.steps,
           (unsigned long long)f.core.presents_total,
           (unsigned long long)f.audio_ticks,
           (unsigned long long)f.core.discards_total,
           (unsigned long long)f.aborted,
           (unsigned long long)f.clock_backwards);
    if (audio) {
        printf("audio:   %d halves submitted, %d stops, finished=%d\n",
               sink.submits, sink.stops, sched.finished);
    }
    printf("note:    the phases are stubs; no game logic and no renderer\n");

    gex_frame_shutdown(&f);
    if (audio) {
        gex_audio_stream_close(&stream);
    }
    return rc;
}
