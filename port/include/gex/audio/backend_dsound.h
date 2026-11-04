/*
 * gex/audio/backend_dsound.h - device backend: the same IDirectSoundBuffer the
 * original drives, recreated on modern Windows.
 *
 * Why DirectSound and not SDL/WASAPI (see docs/AUDIO_MIXER_RE.md §16):
 *
 *   - The original path is DirectSound: 0x00538551 creates the device and
 *     primary buffer, 0x00538731 creates the 0x8000-byte secondary, and
 *     0x005380B3 gates on GetCurrentPosition. Using the same API keeps the
 *     cursor, the looping wrap and the half-overwrite rule 1:1 instead of
 *     re-deriving them on top of a different transport.
 *   - It adds no third-party dependency: dsound.h ships with the toolchain
 *     and dsound.dll with Windows. SDL2/SDL3 is not installed in this
 *     environment (no sdl2-config, no SDL.h, no pkg-config), so adopting it
 *     would mean vendoring code and a mandatory dependency for one sink.
 *   - The pure modules (adpcm/stream/sched) still do not see Windows: this
 *     header is the only place that does, and the tests keep running with the
 *     in-memory sink on any platform.
 *
 * Format: mono PCM 16-bit at the rate the .sag declares (the same restriction
 * gex_audio_stream_open enforces). Differences from the original's device
 * setup, all deliberate and documented (D12/D13):
 *
 *   - DSSCL_NORMAL with the desktop window instead of DSSCL_PRIORITY with the
 *     game window: the tool has no game window, and DirectSound converts the
 *     secondary format to the primary one by itself.
 *   - DSBCAPS_GETCURRENTPOSITION2: modern Windows emulates DirectSound and the
 *     flag makes GetCurrentPosition report the true play position, which is
 *     what the gate needs. DSBCAPS_GLOBALFOCUS keeps the console demo audible
 *     when it is not the foreground window.
 */

#ifndef GEX_AUDIO_BACKEND_DSOUND_H
#define GEX_AUDIO_BACKEND_DSOUND_H

#if defined(_WIN32)

#include <windows.h>

#include <dsound.h>

#include "gex/audio/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gex_audio_device_status {
    GEX_AUDIO_DEVICE_OK = 0,
    GEX_AUDIO_DEVICE_BAD_ARGUMENT,
    GEX_AUDIO_DEVICE_BAD_FORMAT, /* not mono PCM 16-bit / rate == 0 */
    GEX_AUDIO_DEVICE_NO_DEVICE,  /* DirectSoundCreate failed */
    GEX_AUDIO_DEVICE_NO_COOP,    /* SetCooperativeLevel failed */
    GEX_AUDIO_DEVICE_NO_BUFFER   /* CreateSoundBuffer failed */
} gex_audio_device_status;

typedef struct gex_audio_dsound {
    gex_audio_backend backend; /* ready to pass to gex_audio_stream_open */
    LPDIRECTSOUND dsound;
    LPDIRECTSOUNDBUFFER buffer; /* the 0x8000-byte secondary of 0x00538731 */
    WAVEFORMATEX format;
    uint32_t write_offset; /* mirror of 0x0057C37C: 0 or 0x4000 */
    int playing;
    uint64_t submits;      /* halves handed to the device */
    uint64_t restores;     /* DSERR_BUFFERLOST recoveries (0x00538040) */
    uint64_t lock_failures;
    HRESULT last_hresult;
} gex_audio_dsound;

/*
 * Create the device, set the cooperative level and create the secondary
 * buffer, in the order 0x00538551 + 0x00538731 do it. `hwnd` may be NULL, in
 * which case the desktop window is used (see D13). On success the `backend`
 * member is wired up and can be handed to gex_audio_stream_open().
 */
gex_audio_device_status gex_audio_dsound_open(gex_audio_dsound *dev, HWND hwnd,
                                              uint32_t rate, uint16_t channels,
                                              uint16_t bits);

/* Stop and release the buffer and the device. Safe on a zeroed/closed struct
 * and safe to call twice. */
void gex_audio_dsound_close(gex_audio_dsound *dev);

/* Full probe: opens and closes a 22050 Hz mono 16-bit device. Returns 1 when
 * a device exists and accepts the buffer, 0 otherwise. */
int gex_audio_dsound_available(void);

/* Pause/resume the device at its current position. Same callbacks the
 * scheduler invokes through gex_audio_backend.pause/resume; exposed directly
 * for callers that drive the pump without the scheduler. */
int gex_audio_dsound_pause(gex_audio_dsound *dev);
int gex_audio_dsound_resume(gex_audio_dsound *dev);

/*
 * 0x00538724 + 0x0053886c: level is the global 0x005582F4 (0..100 in the mod
 * build), the value applied is level*30 - 3000 dB (0 = maximum). Writes the
 * applied value to *applied when non-NULL. Returns 0 on success, -1 if the
 * device rejected the level.
 */
int gex_audio_dsound_set_volume(gex_audio_dsound *dev, int32_t level,
                                int32_t *applied);

const char *gex_audio_device_status_name(gex_audio_device_status status);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* GEX_AUDIO_BACKEND_DSOUND_H */
