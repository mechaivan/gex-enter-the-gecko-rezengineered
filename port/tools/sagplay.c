/*
 * sagplay - run the audio path end to end and report what came out.
 *
 *   sagplay                      build a synthetic SLAG (440 Hz, 0.5 s) in
 *                                memory, decode it and write build/tone.wav
 *   sagplay <file.sag> [out.wav] load a container, decode it, write a WAV
 *
 * This is the visible check of the port's audio route: SLAG container ->
 * WAVEFORMATEX + payload -> 4-bit ADPCM decode (0x00538285) -> 0x4000-byte
 * halves -> output sink. The sink here is a WAV file, so nothing needs a sound
 * card; the same interface is what a real device backend would implement.
 *
 * The synthetic path uses a small greedy ADPCM encoder written for this demo.
 * It is NOT part of the original game: the retail engine only decodes.
 */

#include "gex/asset/sag.h"
#include "gex/audio/adpcm.h"
#include "gex/audio/stream.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------- synthetic encoder */

static int choose_nibble(const gex_adpcm_tables *tables,
                         const gex_adpcm_state *state, int target)
{
    int best = 0;
    int best_err = 0x7FFFFFFF;
    for (int n = 0; n < 16; n++) {
        gex_adpcm_state probe = *state;
        const int value = gex_adpcm_apply(&probe, tables, n);
        const int err = value > target ? value - target : target - value;
        if (err < best_err) {
            best_err = err;
            best = n;
        }
    }
    return best;
}

static size_t encode_tone(uint8_t *out, size_t capacity, uint32_t rate,
                          double hz, double seconds)
{
    const gex_adpcm_tables *tables = gex_adpcm_recovered_tables();
    gex_adpcm_state state;
    gex_adpcm_reset(&state);

    size_t samples = (size_t)((double)rate * seconds);
    samples &= ~(size_t)1; /* even, so no half byte is left over */
    if (samples / 2 > capacity) {
        samples = capacity * 2;
    }

    for (size_t i = 0; i < samples; i += 2) {
        const double t0 = (double)i / (double)rate;
        const double t1 = (double)(i + 1) / (double)rate;
        const int a = (int)(12000.0 * sin(2.0 * M_PI * hz * t0));
        const int b = (int)(12000.0 * sin(2.0 * M_PI * hz * t1));
        const int hi = choose_nibble(tables, &state, a);
        gex_adpcm_apply(&state, tables, hi);
        const int lo = choose_nibble(tables, &state, b);
        gex_adpcm_apply(&state, tables, lo);
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return samples / 2;
}

/* Build a SLAG container in memory: "fmt " describes the DECODED format
 * (16-bit mono), the "data" payload is the packed 4-bit stream. */
static size_t build_slag(uint8_t *out, size_t capacity, uint32_t rate,
                         const uint8_t *payload, size_t payload_size)
{
    if (capacity < 12 + 8 + 18 + 8 + payload_size) {
        return 0;
    }
    size_t p = 0;
    const uint32_t form = (uint32_t)(4 + 8 + 18 + 8 + payload_size);
    const uint32_t avg = rate * 2u;

    memcpy(out + p, "SLAG", 4);
    p += 4;
    out[p++] = (uint8_t)(form & 0xFF);
    out[p++] = (uint8_t)((form >> 8) & 0xFF);
    out[p++] = (uint8_t)((form >> 16) & 0xFF);
    out[p++] = (uint8_t)((form >> 24) & 0xFF);
    memcpy(out + p, "WAVE", 4);
    p += 4;

    memcpy(out + p, "fmt ", 4);
    p += 4;
    out[p++] = 18;
    out[p++] = 0;
    out[p++] = 0;
    out[p++] = 0;
    /* WAVEFORMATEX: PCM, mono, 16-bit */
    out[p++] = 1;
    out[p++] = 0;
    out[p++] = 1;
    out[p++] = 0;
    out[p++] = (uint8_t)(rate & 0xFF);
    out[p++] = (uint8_t)((rate >> 8) & 0xFF);
    out[p++] = (uint8_t)((rate >> 16) & 0xFF);
    out[p++] = (uint8_t)((rate >> 24) & 0xFF);
    out[p++] = (uint8_t)(avg & 0xFF);
    out[p++] = (uint8_t)((avg >> 8) & 0xFF);
    out[p++] = (uint8_t)((avg >> 16) & 0xFF);
    out[p++] = (uint8_t)((avg >> 24) & 0xFF);
    out[p++] = 2;
    out[p++] = 0;
    out[p++] = 16;
    out[p++] = 0;
    out[p++] = 0;
    out[p++] = 0;

    memcpy(out + p, "data", 4);
    p += 4;
    out[p++] = (uint8_t)(payload_size & 0xFF);
    out[p++] = (uint8_t)((payload_size >> 8) & 0xFF);
    out[p++] = (uint8_t)((payload_size >> 16) & 0xFF);
    out[p++] = (uint8_t)((payload_size >> 24) & 0xFF);
    memcpy(out + p, payload, payload_size);
    p += payload_size;
    return p;
}

/* -------------------------------------------------------------- WAV sink */

typedef struct {
    FILE *file;
    uint32_t rate;
    uint64_t samples;
    int failed;
    int16_t *mirror;   /* everything the stream delivered */
    size_t mirror_count;
    size_t mirror_capacity;
    size_t peak;
} wav_sink;

static void write_u32le(FILE *f, uint32_t v)
{
    const uint8_t b[4] = {(uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF),
                          (uint8_t)((v >> 16) & 0xFF),
                          (uint8_t)((v >> 24) & 0xFF)};
    fwrite(b, 1, 4, f);
}

static int wav_open(wav_sink *sink, const char *path, uint32_t rate)
{
    memset(sink, 0, sizeof(*sink));
    sink->rate = rate;
    sink->file = fopen(path, "wb");
    if (sink->file == NULL) {
        return 0;
    }
    const uint32_t avg = rate * 2u;
    const uint8_t fmt[16] = {
        1, 0, /* PCM */
        1, 0, /* mono */
        (uint8_t)(rate & 0xFF), (uint8_t)((rate >> 8) & 0xFF),
        (uint8_t)((rate >> 16) & 0xFF), (uint8_t)((rate >> 24) & 0xFF),
        (uint8_t)(avg & 0xFF), (uint8_t)((avg >> 8) & 0xFF),
        (uint8_t)((avg >> 16) & 0xFF), (uint8_t)((avg >> 24) & 0xFF),
        2, 0,  /* block align */
        16, 0, /* bits */
    };
    fwrite("RIFF", 1, 4, sink->file);
    write_u32le(sink->file, 0); /* patched on close */
    fwrite("WAVEfmt ", 1, 8, sink->file);
    write_u32le(sink->file, 16);
    fwrite(fmt, 1, 16, sink->file);
    fwrite("data", 1, 4, sink->file);
    write_u32le(sink->file, 0); /* patched on close */
    return 1;
}

static int wav_submit(void *ctx, const int16_t *samples, size_t count,
                      uint32_t rate)
{
    wav_sink *sink = (wav_sink *)ctx;
    if (sink->file == NULL || sink->failed) {
        return 1;
    }
    if (rate != sink->rate) {
        return 1;
    }
    if (fwrite(samples, sizeof(int16_t), count, sink->file) != count) {
        sink->failed = 1;
        return 1;
    }
    sink->samples += count;

    for (size_t i = 0; i < count; i++) {
        const int v = samples[i] < 0 ? -samples[i] : samples[i];
        if ((size_t)v > sink->peak) {
            sink->peak = (size_t)v;
        }
        if (sink->mirror_count < sink->mirror_capacity) {
            sink->mirror[sink->mirror_count++] = samples[i];
        }
    }
    return 0;
}

static void wav_stop(void *ctx)
{
    (void)ctx;
}

static int wav_close(wav_sink *sink)
{
    if (sink->file == NULL) {
        return 0;
    }
    const long data_bytes = (long)(sink->samples * 2u);
    fseek(sink->file, 4, SEEK_SET);
    write_u32le(sink->file, (uint32_t)(data_bytes + 36));
    fseek(sink->file, 40, SEEK_SET);
    write_u32le(sink->file, (uint32_t)data_bytes);
    const int ok = fclose(sink->file) == 0;
    sink->file = NULL;
    return ok && !sink->failed;
}

static uint32_t fnv1a(const int16_t *samples, size_t count)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; i++) {
        hash = (hash ^ (uint8_t)(samples[i] & 0xFF)) * 16777619u;
        hash = (hash ^ (uint8_t)((samples[i] >> 8) & 0xFF)) * 16777619u;
    }
    return hash;
}

/* ------------------------------------------------------------------ main */

static uint8_t g_image[1 << 20];
static uint8_t g_payload[1 << 19];
static int16_t g_mirror[1 << 18];

int main(int argc, char **argv)
{
    size_t image_size = 0;
    const char *wav_path = argc >= 3 ? argv[2] : "build/tone.wav";
    const char *sag_path = argc >= 2 ? argv[1] : NULL;

    if (sag_path != NULL) {
        unsigned char *loaded = NULL;
        size_t loaded_size = 0;
        const gex_sag_status status = gex_sag_load(sag_path, &loaded, &loaded_size);
        if (status != GEX_SAG_OK) {
            fprintf(stderr, "%s: %s\n", sag_path, gex_sag_status_name(status));
            return 1;
        }
        if (loaded_size > sizeof(g_image)) {
            fprintf(stderr, "%s: too large for this demo (%zu bytes)\n", sag_path,
                    loaded_size);
            gex_sag_release(loaded);
            return 1;
        }
        memcpy(g_image, loaded, loaded_size);
        image_size = loaded_size;
        gex_sag_release(loaded);
    } else {
        const uint32_t rate = 22050;
        const size_t packed = encode_tone(g_payload, sizeof(g_payload), rate, 440.0, 0.5);
        image_size = build_slag(g_image, sizeof(g_image), rate, g_payload, packed);
        if (image_size == 0) {
            fprintf(stderr, "synthetic build failed\n");
            return 1;
        }
        printf("source:  synthetic SLAG (440 Hz, 0.5 s, 22050 Hz, built in memory)\n");
    }

    gex_sag sag;
    const gex_sag_status parsed = gex_sag_parse(g_image, image_size, &sag);
    if (parsed != GEX_SAG_OK) {
        fprintf(stderr, "parse failed: %s\n", gex_sag_status_name(parsed));
        return 1;
    }

    const uint32_t rate = (uint32_t)(sag.format[4] | (sag.format[5] << 8));

    wav_sink sink;
    if (!wav_open(&sink, wav_path, rate)) {
        fprintf(stderr, "%s: cannot write\n", wav_path);
        return 1;
    }
    sink.mirror = g_mirror;
    sink.mirror_capacity = sizeof(g_mirror) / sizeof(g_mirror[0]);

    gex_audio_backend backend;
    backend.ctx = &sink;
    backend.submit = wav_submit;
    backend.stop = wav_stop;
    backend.cursor = NULL; /* the file sink has no play position */

    gex_audio_stream stream;
    gex_audio_status status =
        gex_audio_stream_open(&stream, &sag, g_image, image_size, backend);
    if (status != GEX_AUDIO_OK) {
        fprintf(stderr, "open failed: %s\n", gex_audio_status_name(status));
        wav_close(&sink);
        return 1;
    }

    status = gex_audio_stream_play(&stream);
    if (status != GEX_AUDIO_OK) {
        fprintf(stderr, "play failed: %s\n", gex_audio_status_name(status));
        wav_close(&sink);
        return 1;
    }

    unsigned pumps = 1;
    while (!stream.finished && pumps < 100000u) {
        status = gex_audio_stream_pump(&stream);
        if (status != GEX_AUDIO_OK) {
            fprintf(stderr, "pump failed: %s\n", gex_audio_status_name(status));
            break;
        }
        pumps++;
    }

    const int wav_ok = wav_close(&sink);

    printf("file:    %s\n", sag_path != NULL ? sag_path : "(in memory)");
    printf("bytes:   %zu\n", image_size);
    printf("format:  PCM mono %u Hz, 16-bit (declared by \"fmt \")\n", rate);
    printf("payload: %u bytes of 4-bit ADPCM (%u samples)\n", sag.data_size,
           sag.data_size * 2u);
    printf("pumps:   %u calls of 0x005380B3\n", pumps);
    printf("chunks:  %llu read, %llu short\n",
           (unsigned long long)stream.chunks,
           (unsigned long long)stream.short_chunks);
    printf("decoded: %llu samples\n", (unsigned long long)stream.samples_decoded);
    printf("halves:  %llu submitted, %llu samples (%.3f s)\n",
           (unsigned long long)stream.buffer.halves,
           (unsigned long long)stream.buffer.samples_submitted,
           rate ? (double)stream.buffer.samples_submitted / (double)rate : 0.0);
    printf("stops:   %u\n", stream.buffer.stops);
    printf("pcm:     peak=%zu fnv1a=0x%08X over %zu delivered samples\n",
           sink.peak, fnv1a(g_mirror, sink.mirror_count), sink.mirror_count);
    printf("wav:     %s (%s, %llu samples)\n", wav_path,
           wav_ok ? "written" : "FAILED", (unsigned long long)sink.samples);

    gex_audio_stream_close(&stream);
    return (status == GEX_AUDIO_OK && wav_ok) ? 0 : 1;
}
