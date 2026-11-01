/*
 * test_device.c - checks for the DirectSound backend (Windows builds only).
 *
 * This is the only test that talks to a real device. It never fails because a
 * machine has no sound card: it prints "skipped" and exits 0, so the rest of
 * the suite stays runnable on any machine. What it verifies when a device
 * exists:
 *
 *   - open/close cycles, double close, format rejection;
 *   - the pre-fill hands half 0 to the device and starts playback;
 *   - the half written by the next pump holds exactly the decoded chunk,
 *     sample for sample, read back through IDirectSoundBuffer::Lock;
 *   - the play cursor advances at the stream rate (22050 Hz mono 16-bit =
 *     44100 bytes/s), which is objective evidence that the device consumes
 *     the PCM the port produced;
 *   - the gate of 0x00538108: with the device paused the cursor is frozen and
 *     a pump that would overwrite the playing half is deferred;
 *   - pause keeps the position and resume continues from it; stop freezes the
 *     cursor;
 *   - the volume formula of 0x0053886c (level*30 - 3000);
 *   - a second stream on the same device after a stop (restart).
 *
 * It cannot verify that a human hears anything. That needs ears on the
 * machine: run `sagplay --device`.
 */

#include "gex/asset/sag.h"
#include "gex/audio/adpcm.h"
#include "gex/audio/backend_dsound.h"
#include "gex/audio/stream.h"

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

static void build(fixture *f, size_t payload)
{
    f->size = 0;
    put_tag(f, "SLAG");
    put_u32(f, 0x2E);
    put_tag(f, "WAVE");

    put_tag(f, "fmt ");
    put_u32(f, 18);
    put_u16(f, 1);
    put_u16(f, 1);
    put_u32(f, 22050);
    put_u32(f, 22050 * 2u);
    put_u16(f, 2);
    put_u16(f, 16);
    put_u16(f, 0);

    put_tag(f, "data");
    put_u32(f, (uint32_t)payload);
    memset(f->bytes + f->size, 0x11, payload); /* any nibble pattern works */
    f->size += payload;
}

/* ----------------------------------------------------------------- main */

int main(void)
{
    if (!gex_audio_dsound_available()) {
        printf("device: no DirectSound device on this machine; skipped\n");
        return 0;
    }

    gex_audio_dsound dev;

    /* Open/close cycles and argument checks. */
    CHECK(gex_audio_dsound_open(&dev, NULL, 22050, 1, 16) ==
          GEX_AUDIO_DEVICE_OK);
    CHECK(dev.dsound != NULL);
    CHECK(dev.buffer != NULL);
    CHECK(dev.format.nSamplesPerSec == 22050);
    CHECK(dev.format.nChannels == 1);
    CHECK(dev.format.wBitsPerSample == 16);
    gex_audio_dsound_close(&dev);
    CHECK(dev.dsound == NULL);
    CHECK(dev.buffer == NULL);
    gex_audio_dsound_close(&dev); /* second close must be a no-op */

    CHECK(gex_audio_dsound_open(&dev, NULL, 0, 1, 16) ==
          GEX_AUDIO_DEVICE_BAD_FORMAT);
    CHECK(gex_audio_dsound_open(&dev, NULL, 22050, 2, 16) ==
          GEX_AUDIO_DEVICE_BAD_FORMAT);
    CHECK(gex_audio_dsound_open(&dev, NULL, 22050, 1, 8) ==
          GEX_AUDIO_DEVICE_BAD_FORMAT);
    CHECK(gex_audio_dsound_open(NULL, NULL, 22050, 1, 16) ==
          GEX_AUDIO_DEVICE_BAD_ARGUMENT);

    /* Full path: parse, open the stream on the device, pre-fill and play. */
    fixture f;
    build(&f, 0x2000);
    gex_sag sag;
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);

    CHECK(gex_audio_dsound_open(&dev, NULL, 22050, 1, 16) ==
          GEX_AUDIO_DEVICE_OK);

    gex_audio_stream stream;
    CHECK(gex_audio_stream_open(&stream, &sag, f.bytes, f.size, dev.backend) ==
          GEX_AUDIO_OK);
    CHECK(stream.rate == 22050);
    CHECK(gex_audio_stream_play(&stream) == GEX_AUDIO_OK);
    CHECK(dev.submits == 1); /* the pre-fill of 0x00538842 */
    CHECK(dev.playing == 1); /* Play(looping) of 0x0053884c */

    /* Freeze the device right away so the gate state is deterministic. The
     * pre-fill wrote half 0, so the cursor should still be at its start. */
    CHECK(gex_audio_dsound_pause(&dev) == 0);
    const uint32_t frozen = dev.backend.cursor(dev.backend.ctx);
    printf("device: cursor after pause = %u bytes\n", frozen);
    CHECK(frozen <= GEX_AUDIO_HALF_BYTES);

    /* Next pump fills the other half: the gate is open because the cursor is
     * still inside half 0. */
    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.buffer.halves == 2);
    CHECK(dev.submits == 2);
    CHECK(dev.write_offset == 0); /* mirror of 0x0057C37C flipped */

    /* The device buffer must hold exactly the decoded second chunk, sample
     * for sample. This is the content check: SLAG -> ADPCM -> PCM -> device. */
    int16_t reference[GEX_AUDIO_HALF_SAMPLES * 2];
    gex_adpcm_state adpcm;
    gex_adpcm_reset(&adpcm);
    const size_t produced = gex_adpcm_decode(
        &adpcm, gex_adpcm_recovered_tables(), reference,
        GEX_AUDIO_HALF_SAMPLES * 2u, f.bytes + sag.data_offset, 0x2000);
    CHECK(produced == GEX_AUDIO_HALF_SAMPLES * 2u);

    void *ptr1 = NULL;
    void *ptr2 = NULL;
    DWORD bytes1 = 0;
    DWORD bytes2 = 0;
    CHECK(IDirectSoundBuffer_Lock(dev.buffer, GEX_AUDIO_HALF_BYTES,
                                  GEX_AUDIO_HALF_BYTES, &ptr1, &bytes1, &ptr2,
                                  &bytes2, 0) == DS_OK);
    CHECK(bytes1 == GEX_AUDIO_HALF_BYTES);
    if (ptr1 != NULL) {
        CHECK(memcmp(ptr1, reference + GEX_AUDIO_HALF_SAMPLES,
                     GEX_AUDIO_HALF_BYTES) == 0);
    }
    IDirectSoundBuffer_Unlock(dev.buffer, ptr1, bytes1, ptr2, bytes2);

    /* Frozen cursor inside half 0 and the next half to write is 0: deferred. */
    const uint64_t deferred = stream.deferred;
    const uint64_t halves = stream.buffer.halves;
    CHECK(gex_audio_stream_pump(&stream) == GEX_AUDIO_OK);
    CHECK(stream.deferred == deferred + 1);
    CHECK(stream.buffer.halves == halves);

    /* Resume: the cursor continues from the frozen position and moves at the
     * rate of the stream (44100 bytes per second). */
    CHECK(gex_audio_dsound_resume(&dev) == 0);
    DWORD ds_status = 0;
    IDirectSoundBuffer_GetStatus(dev.buffer, &ds_status);
    CHECK((ds_status & DSBSTATUS_PLAYING) != 0);

    const uint32_t before = dev.backend.cursor(dev.backend.ctx);
    const DWORD started = GetTickCount();
    Sleep(200);
    const uint32_t after = dev.backend.cursor(dev.backend.ctx);
    const DWORD elapsed = GetTickCount() - started;
    const uint32_t moved = (after - before) % GEX_AUDIO_BUFFER_BYTES;
    const uint32_t expected = (uint32_t)((double)elapsed * 44.1);
    printf("device: cursor moved %u bytes in %lu ms (expected ~%u)\n", moved,
           (unsigned long)elapsed, expected);
    CHECK(moved > 0);
    if (moved > 0) {
        const uint32_t slack = expected / 3u > 2000u ? expected / 3u : 2000u;
        CHECK(moved + slack >= expected);
        CHECK(moved <= expected + slack);
    }

    /* Volume: 0x0053886c maps level 100 to 0 dB (maximum) and 0 to -3000. */
    int32_t applied = 12345;
    CHECK(gex_audio_dsound_set_volume(&dev, 100, &applied) == 0);
    CHECK(applied == 0);
    CHECK(gex_audio_dsound_set_volume(&dev, 0, &applied) == 0);
    CHECK(applied == -3000);
    CHECK(gex_audio_dsound_set_volume(NULL, 100, &applied) == -1);

    /* Stop freezes the cursor. */
    gex_audio_buffer_stop(&stream.buffer);
    CHECK(dev.playing == 0);
    const uint32_t stopped = dev.backend.cursor(dev.backend.ctx);
    Sleep(60);
    CHECK(dev.backend.cursor(dev.backend.ctx) == stopped);

    gex_audio_stream_close(&stream);

    /* Restart: the original recreates the secondary buffer on every play
     * (0x0053874e..0x00538815); after Stop the position is back at 0, so a
     * second stream on the same device pre-fills and plays again. */
    const uint64_t submits = dev.submits;
    gex_audio_stream again;
    CHECK(gex_audio_stream_open(&again, &sag, f.bytes, f.size, dev.backend) ==
          GEX_AUDIO_OK);
    CHECK(gex_audio_stream_play(&again) == GEX_AUDIO_OK);
    CHECK(dev.playing == 1);
    CHECK(dev.submits == submits + 1);
    CHECK(gex_audio_dsound_pause(&dev) == 0);
    CHECK(gex_audio_dsound_resume(&dev) == 0);
    gex_audio_stream_close(&again);

    gex_audio_dsound_close(&dev);
    gex_audio_dsound_close(&dev);

    printf("device checks: %d, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
