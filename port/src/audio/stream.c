/*
 * stream.c - secondary buffer, streaming pump and player of GEX3D.EXE.
 *
 * Original functions:
 *   0x00538731  player: parse already done, CreateSoundBuffer(0x8000, 0xE0),
 *               reset ADPCM state, pump once, Play(looping), SetVolume
 *   0x005380B3  pump:   Lock(0x4000) -> read 0x1000 -> 0x00538285 -> Unlock
 *   0x005384ED  "source still open"
 *   0x005384A3  drop the source (free buffer / close file)
 *
 * Everything the original delegates to DirectSound (Lock/Unlock/Play/Stop/
 * Restore/GetCurrentPosition/SetVolume) is modelled here as a plain double
 * buffer plus a caller-provided sink. See docs/AUDIO_MIXER_RE.md §5-§6 for the
 * mapping and §7 for the deviations.
 */

#include "gex/audio/stream.h"

#include <string.h>

/* ------------------------------------------------------------------ buffer */

void gex_audio_buffer_init(gex_audio_buffer *buffer, gex_audio_backend backend,
                           uint32_t rate)
{
    if (buffer == NULL) {
        return;
    }
    memset(buffer, 0, sizeof(*buffer));
    buffer->rate = rate;
    buffer->backend = backend;
}

int16_t *gex_audio_buffer_lock(gex_audio_buffer *buffer, uint32_t bytes)
{
    if (buffer == NULL || bytes != GEX_AUDIO_HALF_BYTES) {
        return NULL;
    }
    if (buffer->locked) {
        return NULL;
    }
    if (buffer->write_offset > GEX_AUDIO_BUFFER_BYTES - GEX_AUDIO_HALF_BYTES) {
        return NULL;
    }
    buffer->locked = 1;
    buffer->lock_offset = buffer->write_offset;
    return buffer->pcm + (buffer->lock_offset / 2u);
}

gex_audio_status gex_audio_buffer_unlock(gex_audio_buffer *buffer, uint32_t bytes)
{
    if (buffer == NULL || !buffer->locked) {
        return GEX_AUDIO_ERR_ARGUMENT;
    }
    buffer->locked = 0;

    if (bytes != GEX_AUDIO_HALF_BYTES) {
        return GEX_AUDIO_OK;
    }

    /* The original flips 0x0057C37C right after Lock (0x00538173), before the
     * samples are written, so Unlock must use the offset the Lock captured. */
    const int16_t *half = buffer->pcm + (buffer->lock_offset / 2u);
    buffer->halves++;
    buffer->samples_submitted += GEX_AUDIO_HALF_SAMPLES;

    if (buffer->backend.submit == NULL) {
        return GEX_AUDIO_OK;
    }
    if (buffer->backend.submit(buffer->backend.ctx, half, GEX_AUDIO_HALF_SAMPLES,
                               buffer->rate) != 0) {
        return GEX_AUDIO_ERR_BACKEND;
    }
    return GEX_AUDIO_OK;
}

uint32_t gex_audio_buffer_flip(gex_audio_buffer *buffer)
{
    if (buffer == NULL) {
        return 0;
    }
    /* 0x00538173: write_offset = (write_offset == 0) ? 0x4000 : 0. */
    buffer->write_offset = (buffer->write_offset == 0) ? GEX_AUDIO_HALF_BYTES : 0;
    return buffer->write_offset;
}

void gex_audio_buffer_unlock_discard(gex_audio_buffer *buffer)
{
    if (buffer != NULL) {
        buffer->locked = 0;
    }
}

void gex_audio_buffer_play(gex_audio_buffer *buffer)
{
    if (buffer != NULL) {
        buffer->playing = 1;
    }
}

void gex_audio_buffer_stop(gex_audio_buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }
    buffer->playing = 0;
    buffer->stops++;
    if (buffer->backend.stop != NULL) {
        buffer->backend.stop(buffer->backend.ctx);
    }
}

/* ------------------------------------------------------------------ stream */

const char *gex_audio_status_name(gex_audio_status status)
{
    switch (status) {
    case GEX_AUDIO_OK:
        return "ok";
    case GEX_AUDIO_ERR_ARGUMENT:
        return "argument";
    case GEX_AUDIO_ERR_FORMAT:
        return "format";
    case GEX_AUDIO_ERR_SOURCE:
        return "source";
    case GEX_AUDIO_ERR_BACKEND:
        return "backend";
    }
    return "unknown";
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

gex_audio_status gex_audio_stream_open(gex_audio_stream *stream,
                                       const gex_sag *sag, const void *image,
                                       size_t image_size,
                                       gex_audio_backend backend)
{
    if (stream == NULL || sag == NULL || image == NULL) {
        return GEX_AUDIO_ERR_ARGUMENT;
    }

    memset(stream, 0, sizeof(*stream));

    if (!sag->has_format) {
        return GEX_AUDIO_ERR_FORMAT;
    }
    stream->channels = le16(sag->format + 2);
    stream->rate = (uint32_t)le16(sag->format + 4) |
                   ((uint32_t)le16(sag->format + 6) << 16);
    stream->bits = le16(sag->format + 14);

    if (stream->channels != 1 || stream->bits != 16 || stream->rate == 0) {
        return GEX_AUDIO_ERR_FORMAT;
    }

    if (sag->data_offset > image_size) {
        return GEX_AUDIO_ERR_SOURCE;
    }
    const size_t available = image_size - sag->data_offset;
    stream->payload = (const uint8_t *)image + sag->data_offset;
    stream->payload_size = sag->data_size;
    if (stream->payload_size > available) {
        /* Declared payload runs past the file: stream what exists and let the
         * short-chunk path end the source. Deviation D10. */
        stream->payload_size = available;
        stream->payload_truncated = 1;
    }
    if (stream->payload_size == 0) {
        return GEX_AUDIO_ERR_SOURCE;
    }

    stream->tables = gex_adpcm_recovered_tables();
    stream->source_open = 1;
    stream->cursor = 0;
    stream->silence_sent = 0;
    stream->finished = 0;
    gex_adpcm_reset(&stream->adpcm);
    gex_audio_buffer_init(&stream->buffer, backend, stream->rate);

    return GEX_AUDIO_OK;
}

gex_audio_status gex_audio_stream_play(gex_audio_stream *stream)
{
    if (stream == NULL) {
        return GEX_AUDIO_ERR_ARGUMENT;
    }

    /* 0x0053881a..0x00538838: reset state and write offset before pumping. */
    gex_adpcm_reset(&stream->adpcm);
    stream->buffer.write_offset = 0;
    stream->silence_sent = 0;
    stream->finished = 0;

    /* 0x00538842: one pump call pre-fills the first half. */
    const gex_audio_status status = gex_audio_stream_pump(stream);
    if (status != GEX_AUDIO_OK) {
        return status;
    }

    /* 0x0053884c: Play(0, 0, DSBPLAY_LOOPING). */
    gex_audio_buffer_play(&stream->buffer);
    return GEX_AUDIO_OK;
}

gex_audio_status gex_audio_stream_pump(gex_audio_stream *stream)
{
    if (stream == NULL) {
        return GEX_AUDIO_ERR_ARGUMENT;
    }

    /* 0x005380dc..0x0053812d: while the buffer is playing, wait until the
     * play cursor has left the half that is about to be overwritten. */
    if (stream->buffer.playing && stream->buffer.backend.cursor != NULL) {
        const uint32_t pos =
            stream->buffer.backend.cursor(stream->buffer.backend.ctx) %
            GEX_AUDIO_BUFFER_BYTES;
        if (stream->buffer.write_offset == 0) {
            if (pos < GEX_AUDIO_HALF_BYTES) {
                stream->deferred++;
                return GEX_AUDIO_OK;
            }
        } else if (pos > GEX_AUDIO_HALF_BYTES) {
            stream->deferred++;
            return GEX_AUDIO_OK;
        }
    }

    int16_t *dst = gex_audio_buffer_lock(&stream->buffer, GEX_AUDIO_HALF_BYTES);
    if (dst == NULL) {
        return GEX_AUDIO_ERR_ARGUMENT;
    }

    gex_audio_status status = GEX_AUDIO_OK;

    if (stream->source_open) {
        size_t want = stream->payload_size - stream->cursor;
        if (want > GEX_AUDIO_CHUNK_BYTES) {
            want = GEX_AUDIO_CHUNK_BYTES;
        }

        const uint8_t *src = stream->payload + stream->cursor;
        const size_t produced =
            gex_adpcm_decode(&stream->adpcm, stream->tables, dst, want * 2u, src,
                             want);
        stream->cursor += want;
        stream->chunks++;
        stream->samples_decoded += produced;

        if (want < GEX_AUDIO_CHUNK_BYTES) {
            /* 0x005381cd..0x005381f7: zero the rest of the half, then drop the
             * source (0x005384A3). A payload that is an exact multiple of
             * 0x1000 arrives here with want == 0. */
            const size_t used_bytes = produced * 2u;
            if (used_bytes < GEX_AUDIO_HALF_BYTES) {
                memset((uint8_t *)dst + used_bytes, 0,
                       GEX_AUDIO_HALF_BYTES - used_bytes);
            }
            stream->short_chunks++;
            stream->source_open = 0;
        }
        gex_audio_buffer_flip(&stream->buffer);
    } else if (!stream->silence_sent) {
        /* 0x00538207: one silent half so the buffer drains cleanly. */
        memset(dst, 0, GEX_AUDIO_HALF_BYTES);
        stream->silence_sent = 1;
        gex_audio_buffer_flip(&stream->buffer);
    } else {
        /* 0x00538242: Stop(), then the Unlock of 0x0053827e. The half was not
         * filled, so it is released without handing it to the backend. */
        gex_audio_buffer_stop(&stream->buffer);
        stream->finished = 1;
        gex_audio_buffer_unlock_discard(&stream->buffer);
        return status;
    }

    const gex_audio_status unlock =
        gex_audio_buffer_unlock(&stream->buffer, GEX_AUDIO_HALF_BYTES);
    if (unlock != GEX_AUDIO_OK) {
        status = unlock;
    }
    return status;
}

void gex_audio_stream_close(gex_audio_stream *stream)
{
    if (stream == NULL) {
        return;
    }
    stream->payload = NULL;
    stream->payload_size = 0;
    stream->cursor = 0;
    stream->source_open = 0;
    if (stream->buffer.playing) {
        gex_audio_buffer_stop(&stream->buffer);
    }
}
