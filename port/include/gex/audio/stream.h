/*
 * gex/audio/stream.h - the audio path of GEX3D.EXE, without DirectSound.
 *
 * Three pieces, each one a transcription of a different original function:
 *
 *   gex_audio_buffer  the 0x8000-byte secondary buffer of 0x00538731
 *                     (Lock/Unlock/Play/Stop/GetStatus semantics)
 *   gex_audio_stream  the streaming pump 0x005380B3: read 0x1000 compressed
 *                     bytes, decode them with 0x00538285, write one half
 *   gex_audio_backend where the samples go. The original hands them to
 *                     DirectSound; here the caller injects a sink, so tests
 *                     capture samples and the demo writes a WAV file.
 *
 * Sizes are the ones the original hardcodes: buffer 0x8000 bytes split in two
 * halves of 0x4000, compressed chunk 0x1000 bytes (which decodes to exactly
 * one half: 4096 bytes -> 8192 samples -> 16384 bytes).
 */

#ifndef GEX_AUDIO_STREAM_H
#define GEX_AUDIO_STREAM_H

#include <stddef.h>
#include <stdint.h>

#include "gex/asset/sag.h"
#include "gex/audio/adpcm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GEX_AUDIO_BUFFER_BYTES 0x8000u /* 0x005387e7: dwBufferBytes */
#define GEX_AUDIO_HALF_BYTES 0x4000u
#define GEX_AUDIO_CHUNK_BYTES 0x1000u /* 0x0053819d: read size */
#define GEX_AUDIO_HALF_SAMPLES (GEX_AUDIO_HALF_BYTES / 2u)

typedef enum gex_audio_status {
    GEX_AUDIO_OK = 0,
    GEX_AUDIO_ERR_ARGUMENT,
    GEX_AUDIO_ERR_FORMAT,  /* WAVEFORMATEX this path cannot produce */
    GEX_AUDIO_ERR_SOURCE,  /* no "data" payload to stream */
    GEX_AUDIO_ERR_BACKEND  /* the sink rejected a submit */
} gex_audio_status;

/*
 * Output sink. `submit` receives one decoded half: `count` samples of signed
 * 16-bit mono at `rate` Hz. It must not retain the pointer. `stop` is called
 * when the stream reaches the Stop() the original issues at 0x00538251.
 *
 * `cursor`, when present, reports the current playback position in bytes
 * inside the 0x8000-byte buffer, like IDirectSoundBuffer::GetCurrentPosition.
 * The pump uses it to wait for the other half before overwriting it, exactly
 * as 0x005380B3 does at 0x00538108-0x0053812a. With no cursor the sink is
 * assumed to have consumed everything it was given.
 *
 * `pause`/`resume` are optional and only meaningful for a device sink: pause
 * must freeze the play cursor where it is (the original's buffer keeps its
 * position across Stop/Play, so a frozen cursor keeps the gate valid), and
 * resume must continue from that position. They are NOT the same as `stop`:
 * stop ends playback, pause suspends it. The scheduler calls them; the pump
 * never does.
 *
 * Any callback may be NULL.
 */
typedef struct gex_audio_backend {
    void *ctx;
    int (*submit)(void *ctx, const int16_t *samples, size_t count, uint32_t rate);
    void (*stop)(void *ctx);
    uint32_t (*cursor)(void *ctx);
    void (*pause)(void *ctx);
    void (*resume)(void *ctx);
} gex_audio_backend;

typedef struct gex_audio_buffer {
    int16_t pcm[GEX_AUDIO_HALF_SAMPLES * 2]; /* 0x8000 bytes */
    uint32_t write_offset;                   /* original 0x0057C37C: 0 or 0x4000 */
    uint32_t lock_offset;                    /* offset captured by Lock */
    uint32_t rate;
    int playing;
    int locked;
    gex_audio_backend backend;
    uint64_t halves;
    uint64_t samples_submitted;
    uint32_t stops;
} gex_audio_buffer;

void gex_audio_buffer_init(gex_audio_buffer *buffer, gex_audio_backend backend,
                           uint32_t rate);

/* Mirrors IDirectSoundBuffer::Lock at `write_offset` for `bytes` (0x4000).
 * Returns the writable pointer, or NULL if the buffer is already locked or
 * `bytes` is not the half size. The offset is remembered for Unlock. */
int16_t *gex_audio_buffer_lock(gex_audio_buffer *buffer, uint32_t bytes);

/* Mirrors Unlock: hands the half captured by Lock to the backend. */
gex_audio_status gex_audio_buffer_unlock(gex_audio_buffer *buffer, uint32_t bytes);

/* The Stop path of 0x005380B3 (0x00538242) unlocks a region it never filled.
 * Releasing the lock without submitting keeps the backend from receiving stale
 * samples that the original would have stopped before playing. */
void gex_audio_buffer_unlock_discard(gex_audio_buffer *buffer);

/* 0x00538173: write_offset = (write_offset == 0) ? 0x4000 : 0. */
uint32_t gex_audio_buffer_flip(gex_audio_buffer *buffer);

void gex_audio_buffer_play(gex_audio_buffer *buffer);
void gex_audio_buffer_stop(gex_audio_buffer *buffer);

typedef struct gex_audio_stream {
    gex_audio_buffer buffer;
    gex_adpcm_state adpcm;
    const gex_adpcm_tables *tables;
    const uint8_t *payload; /* inside the SLAG file image */
    size_t payload_size;
    size_t cursor;         /* original 0x0057C370 */
    int source_open;       /* 0x005384ED: buffer still owned */
    int silence_sent;      /* original 0x0057C380 */
    int finished;
    uint32_t rate;
    uint16_t channels;
    uint16_t bits;
    int payload_truncated; /* SLAG declared more bytes than the file holds */
    uint64_t chunks;       /* pump calls that read compressed bytes */
    uint64_t samples_decoded;
    uint64_t short_chunks; /* chunks that ended the source */
    uint64_t deferred;     /* pumps that returned early waiting for a half */
} gex_audio_stream;

/*
 * Prepare the stream from a parsed SLAG and the file image it came from.
 * `image`/`image_size` must be the same buffer passed to gex_sag_parse().
 * The payload is data_offset .. data_offset + data_size, clamped to the image.
 *
 * Rejects (GEX_AUDIO_ERR_FORMAT) anything this path cannot reproduce: no
 * "fmt " chunk, nChannels != 1, wBitsPerSample != 16, nSamplesPerSec == 0.
 * The original never checks; the pump always writes mono 16-bit samples, so
 * any other declared format would be played as garbage (deviation D9).
 */
gex_audio_status gex_audio_stream_open(gex_audio_stream *stream,
                                       const gex_sag *sag, const void *image,
                                       size_t image_size,
                                       gex_audio_backend backend);

/* 0x00538731 after CreateSoundBuffer: reset ADPCM state, pump once, play. */
gex_audio_status gex_audio_stream_play(gex_audio_stream *stream);

/*
 * One call of 0x005380B3: fill one half. Returns OK whether or not samples
 * were produced; `stream->finished` becomes 1 once Stop() has been issued.
 *
 * When the backend reports a play cursor, a call that arrives before the
 * other half has been played returns immediately and bumps `deferred`
 * (0x00538108-0x0053812a). The pre-fill of gex_audio_stream_play is exempt
 * because the buffer is not playing yet (0x005380a6).
 */
gex_audio_status gex_audio_stream_pump(gex_audio_stream *stream);

/* Drop the source and stop the backend. Safe on a zeroed stream. */
void gex_audio_stream_close(gex_audio_stream *stream);

const char *gex_audio_status_name(gex_audio_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_AUDIO_STREAM_H */
