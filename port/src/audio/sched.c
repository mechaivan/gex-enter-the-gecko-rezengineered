/*
 * sched.c - the frame-loop side of 0x005380B3.
 *
 * The pump itself (stream.c) is the reconstruction; this file only decides
 * when to call it, using the same test the pump uses at 0x00538108-0x0053812a
 * so a scheduled tick costs nothing when there is nothing to do. The original
 * callers are the frame loop (0x00463205, 0x0053A5FB, 0x0053FCB7), which call
 * the pump once per frame whether or not it will produce samples.
 *
 * The cursor is read from the sink (backend.cursor), the same call the pump
 * makes, so the scheduler and the pump always see the same position.
 */

#include "gex/audio/sched.h"

#include <stddef.h>

void gex_audio_sched_init(gex_audio_sched *sched, gex_audio_stream *stream)
{
    if (sched == NULL) {
        return;
    }
    sched->stream = stream;
    sched->paused = 0;
    sched->finished = 0;
    sched->last_status = GEX_AUDIO_OK;
    sched->ticks = 0;
    sched->fills = 0;
    sched->deferred = 0;
}

static uint32_t sched_cursor(const gex_audio_sched *sched)
{
    const gex_audio_backend *backend = &sched->stream->buffer.backend;
    if (backend->cursor == NULL) {
        return 0;
    }
    return backend->cursor(backend->ctx);
}

int gex_audio_sched_due(const gex_audio_sched *sched)
{
    if (sched == NULL || sched->stream == NULL) {
        return 0;
    }
    const gex_audio_stream *stream = sched->stream;
    if (sched->paused || sched->finished || stream->finished) {
        return 0;
    }
    if (!stream->buffer.playing) {
        /* The pre-fill of gex_audio_stream_play happens before Play; a tick
         * must not drain the source into the buffer ahead of time. */
        return 0;
    }
    if (stream->buffer.backend.cursor == NULL) {
        return 1;
    }

    /* Same comparisons as 0x00538108-0x0053812a. */
    const uint32_t play_cursor = sched_cursor(sched) % GEX_AUDIO_BUFFER_BYTES;
    if (stream->buffer.write_offset == 0) {
        return play_cursor >= GEX_AUDIO_HALF_BYTES;
    }
    return play_cursor <= GEX_AUDIO_HALF_BYTES;
}

gex_audio_status gex_audio_sched_tick(gex_audio_sched *sched)
{
    if (sched == NULL || sched->stream == NULL) {
        return GEX_AUDIO_ERR_ARGUMENT;
    }
    sched->ticks++;

    if (!gex_audio_sched_due(sched)) {
        const gex_audio_stream *stream = sched->stream;
        if (!sched->paused && !sched->finished && !stream->finished &&
            stream->buffer.playing) {
            sched->deferred++;
        }
        return sched->last_status;
    }

    const uint64_t halves = sched->stream->buffer.halves;
    const gex_audio_status status = gex_audio_stream_pump(sched->stream);
    sched->last_status = status;
    if (sched->stream->buffer.halves != halves) {
        sched->fills++;
    }
    if (sched->stream->finished) {
        sched->finished = 1;
    }
    return status;
}

void gex_audio_sched_pause(gex_audio_sched *sched)
{
    if (sched == NULL || sched->stream == NULL || sched->paused) {
        return;
    }
    sched->paused = 1;
    const gex_audio_backend *backend = &sched->stream->buffer.backend;
    if (backend->pause != NULL) {
        backend->pause(backend->ctx);
    }
}

void gex_audio_sched_resume(gex_audio_sched *sched)
{
    if (sched == NULL || sched->stream == NULL || !sched->paused) {
        return;
    }
    sched->paused = 0;
    const gex_audio_backend *backend = &sched->stream->buffer.backend;
    if (backend->resume != NULL) {
        backend->resume(backend->ctx);
    }
}
