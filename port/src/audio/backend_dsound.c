/*
 * backend_dsound.c - the device sink: a 0x8000-byte DirectSound secondary
 * buffer, filled one 0x4000 half at a time, exactly the object the original
 * creates at 0x005387f1 and drives from 0x005380B3.
 *
 * Mapping to the original:
 *
 *   gex_audio_dsound_open    0x00538551 (device + cooperative level) and the
 *                            CreateSoundBuffer of 0x005387f1
 *   submit                   Lock(0x4000) + the sample write + Unlock, at the
 *                            half 0x0057C37C points to (0x0053812f..0x0053827e)
 *   submit (first half)      Play(0, 0, DSBPLAY_LOOPING) of 0x0053884c: the
 *                            original plays right after the pre-fill
 *   cursor                   GetCurrentPosition (vtable +0x10), the value the
 *                            gate at 0x00538108 compares against 0x4000
 *   stop                     Stop (vtable +0x48) of the 0x00538251 branch
 *   pause/resume             Stop/Play with the position kept, so the frozen
 *                            cursor keeps the gate valid
 *   set_volume               0x0053886c: level*30 - 3000 dB
 *
 * The buffer is fixed at 0x8000 bytes, so the audio queue cannot grow: the
 * gate refuses to overwrite a half that is still playing and a late pump just
 * leaves the previous half looping, which is what the original does.
 */

#include "gex/audio/backend_dsound.h"

#include <string.h>

/* -------------------------------------------------------------- callbacks */

static int dsound_submit(void *ctx, const int16_t *samples, size_t count,
                         uint32_t rate)
{
    gex_audio_dsound *dev = (gex_audio_dsound *)ctx;
    if (dev == NULL || dev->buffer == NULL || samples == NULL) {
        return -1;
    }
    /* The pump always hands over a whole half at the stream rate. */
    if (count != GEX_AUDIO_HALF_SAMPLES ||
        rate != dev->format.nSamplesPerSec) {
        return -1;
    }

    void *ptr1 = NULL;
    void *ptr2 = NULL;
    DWORD bytes1 = 0;
    DWORD bytes2 = 0;
    HRESULT h = IDirectSoundBuffer_Lock(dev->buffer, dev->write_offset,
                                        GEX_AUDIO_HALF_BYTES, &ptr1, &bytes1,
                                        &ptr2, &bytes2, 0);
    if (h == DSERR_BUFFERLOST) {
        /* 0x00538040: Restore() when the buffer was lost. */
        dev->restores++;
        h = IDirectSoundBuffer_Restore(dev->buffer);
        if (h == DS_OK) {
            h = IDirectSoundBuffer_Lock(dev->buffer, dev->write_offset,
                                        GEX_AUDIO_HALF_BYTES, &ptr1, &bytes1,
                                        &ptr2, &bytes2, 0);
        }
    }
    if (h != DS_OK || ptr1 == NULL) {
        dev->lock_failures++;
        dev->last_hresult = h;
        return -1;
    }

    size_t first = bytes1;
    if (first > GEX_AUDIO_HALF_BYTES) {
        first = GEX_AUDIO_HALF_BYTES;
    }
    memcpy(ptr1, samples, first);
    if (ptr2 != NULL && bytes2 > 0) {
        memcpy(ptr2, (const uint8_t *)samples + first, bytes2);
    }
    IDirectSoundBuffer_Unlock(dev->buffer, ptr1, bytes1, ptr2, bytes2);

    /* 0x00538173: write_offset = (write_offset == 0) ? 0x4000 : 0. */
    dev->write_offset =
        (dev->write_offset == 0) ? GEX_AUDIO_HALF_BYTES : 0;
    dev->submits++;

    if (!dev->playing) {
        /* 0x0053884c: the first half is in place, start looping. */
        h = IDirectSoundBuffer_Play(dev->buffer, 0, 0, DSBPLAY_LOOPING);
        if (h != DS_OK) {
            dev->last_hresult = h;
            return -1;
        }
        dev->playing = 1;
    }
    return 0;
}

static void dsound_stop(void *ctx)
{
    gex_audio_dsound *dev = (gex_audio_dsound *)ctx;
    if (dev == NULL || dev->buffer == NULL) {
        return;
    }
    IDirectSoundBuffer_Stop(dev->buffer);
    /* 0x00538731 stops and *recreates* the secondary buffer on every play
     * (0x0053874e..0x00538815). Stop alone keeps the position, so reset it
     * here: a second stream on the same device then starts from the same
     * state as the first (pre-fill half 0, Play from 0). Pause does not do
     * this; it must keep the position. */
    IDirectSoundBuffer_SetCurrentPosition(dev->buffer, 0);
    dev->write_offset = 0;
    dev->playing = 0;
}

static uint32_t dsound_cursor(void *ctx)
{
    gex_audio_dsound *dev = (gex_audio_dsound *)ctx;
    if (dev == NULL || dev->buffer == NULL) {
        return 0;
    }
    DWORD play = 0;
    DWORD write = 0;
    const HRESULT h =
        IDirectSoundBuffer_GetCurrentPosition(dev->buffer, &play, &write);
    if (h != DS_OK) {
        dev->last_hresult = h;
        return 0;
    }
    return (uint32_t)(play % GEX_AUDIO_BUFFER_BYTES);
}

static void dsound_pause(void *ctx)
{
    gex_audio_dsound *dev = (gex_audio_dsound *)ctx;
    if (dev == NULL || dev->buffer == NULL || !dev->playing) {
        return;
    }
    /* Stop keeps the play position: a later Play continues from there, so
     * the cursor stays frozen at a valid place for the gate. */
    IDirectSoundBuffer_Stop(dev->buffer);
    dev->playing = 0;
}

static void dsound_resume(void *ctx)
{
    gex_audio_dsound *dev = (gex_audio_dsound *)ctx;
    if (dev == NULL || dev->buffer == NULL || dev->playing ||
        dev->submits == 0) {
        return;
    }
    if (IDirectSoundBuffer_Play(dev->buffer, 0, 0, DSBPLAY_LOOPING) == DS_OK) {
        dev->playing = 1;
    }
}

/* ------------------------------------------------------------------- API */

gex_audio_device_status gex_audio_dsound_open(gex_audio_dsound *dev,
                                              HWND hwnd, uint32_t rate,
                                              uint16_t channels,
                                              uint16_t bits)
{
    if (dev == NULL) {
        return GEX_AUDIO_DEVICE_BAD_ARGUMENT;
    }
    memset(dev, 0, sizeof(*dev));
    dev->backend.ctx = dev;
    dev->backend.submit = dsound_submit;
    dev->backend.stop = dsound_stop;
    dev->backend.cursor = dsound_cursor;
    dev->backend.pause = dsound_pause;
    dev->backend.resume = dsound_resume;

    if (rate == 0 || channels != 1 || bits != 16) {
        return GEX_AUDIO_DEVICE_BAD_FORMAT;
    }

    dev->format.wFormatTag = WAVE_FORMAT_PCM;
    dev->format.nChannels = channels;
    dev->format.nSamplesPerSec = rate;
    dev->format.wBitsPerSample = bits;
    dev->format.nBlockAlign = (WORD)(channels * (bits / 8u));
    dev->format.nAvgBytesPerSec = rate * dev->format.nBlockAlign;
    dev->format.cbSize = 0;

    HRESULT h = DirectSoundCreate(NULL, &dev->dsound, NULL);
    dev->last_hresult = h;
    if (h != DS_OK) {
        dev->dsound = NULL;
        return GEX_AUDIO_DEVICE_NO_DEVICE;
    }

    h = IDirectSound_SetCooperativeLevel(
        dev->dsound, hwnd != NULL ? hwnd : GetDesktopWindow(),
        DSSCL_NORMAL);
    dev->last_hresult = h;
    if (h != DS_OK) {
        IDirectSound_Release(dev->dsound);
        dev->dsound = NULL;
        return GEX_AUDIO_DEVICE_NO_COOP;
    }

    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN |
                   DSBCAPS_CTRLFREQUENCY | DSBCAPS_GETCURRENTPOSITION2 |
                   DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = GEX_AUDIO_BUFFER_BYTES;
    desc.lpwfxFormat = &dev->format;

    h = IDirectSound_CreateSoundBuffer(dev->dsound, &desc, &dev->buffer, NULL);
    dev->last_hresult = h;
    if (h != DS_OK) {
        dev->buffer = NULL;
        IDirectSound_Release(dev->dsound);
        dev->dsound = NULL;
        return GEX_AUDIO_DEVICE_NO_BUFFER;
    }

    dev->write_offset = 0;
    return GEX_AUDIO_DEVICE_OK;
}

void gex_audio_dsound_close(gex_audio_dsound *dev)
{
    if (dev == NULL) {
        return;
    }
    if (dev->buffer != NULL) {
        IDirectSoundBuffer_Stop(dev->buffer);
        IDirectSoundBuffer_Release(dev->buffer);
        dev->buffer = NULL;
    }
    if (dev->dsound != NULL) {
        IDirectSound_Release(dev->dsound);
        dev->dsound = NULL;
    }
    dev->playing = 0;
}

int gex_audio_dsound_available(void)
{
    gex_audio_dsound probe;
    if (gex_audio_dsound_open(&probe, NULL, 22050, 1, 16) !=
        GEX_AUDIO_DEVICE_OK) {
        return 0;
    }
    gex_audio_dsound_close(&probe);
    return 1;
}

int gex_audio_dsound_pause(gex_audio_dsound *dev)
{
    if (dev == NULL || dev->buffer == NULL) {
        return -1;
    }
    dsound_pause(dev);
    return 0;
}

int gex_audio_dsound_resume(gex_audio_dsound *dev)
{
    if (dev == NULL || dev->buffer == NULL) {
        return -1;
    }
    dsound_resume(dev);
    return 0;
}

int gex_audio_dsound_set_volume(gex_audio_dsound *dev, int32_t level,
                                int32_t *applied)
{
    if (dev == NULL || dev->buffer == NULL) {
        return -1;
    }
    /* 0x0053886c: [0x005582F4] * 30 - 3000. The original's clamp afterwards
     * is dead code (two self-assignments at 0x0053888b/0x00538891). */
    const int32_t value = level * 30 - 3000;
    const HRESULT h = IDirectSoundBuffer_SetVolume(dev->buffer, value);
    dev->last_hresult = h;
    if (h != DS_OK) {
        return -1;
    }
    if (applied != NULL) {
        *applied = value;
    }
    return 0;
}

const char *gex_audio_device_status_name(gex_audio_device_status status)
{
    switch (status) {
    case GEX_AUDIO_DEVICE_OK:
        return "ok";
    case GEX_AUDIO_DEVICE_BAD_ARGUMENT:
        return "argument";
    case GEX_AUDIO_DEVICE_BAD_FORMAT:
        return "format";
    case GEX_AUDIO_DEVICE_NO_DEVICE:
        return "no DirectSound device";
    case GEX_AUDIO_DEVICE_NO_COOP:
        return "SetCooperativeLevel failed";
    case GEX_AUDIO_DEVICE_NO_BUFFER:
        return "CreateSoundBuffer failed";
    }
    return "unknown";
}
