/*
 * gex/audio/sched.h - when to call the streaming pump.
 *
 * The original never has a timer for audio: the frame loop calls 0x005380B3
 * once per pass (0x00463205, 0x0053A5FB, 0x0053FCB7), and the player calls it
 * once before Play (0x00538842). The pump is idempotent: it locks a half and,
 * if the play cursor still sits in that half, returns immediately
 * (0x00538108-0x0053812a). This header provides that same contract for the
 * future game loop:
 *
 *   gex_audio_sched sched;
 *   gex_audio_sched_init(&sched, &stream);
 *   ... once per frame, after the update phase:
 *   gex_audio_sched_tick(&sched);
 *
 * The tick reads the play cursor from the sink itself (backend.cursor), the
 * same value the pump reads, so the two can never disagree. Calling the tick
 * more often than needed is harmless: it just counts a deferral. Calling it
 * less often than once per half (8192 samples, ~0.37 s at 22050 Hz) makes the
 * device replay the previous half, exactly like the original would.
 *
 * Nothing here starts a thread: the pump runs on the caller's thread, which is
 * also the thread that submits to the device. That is what the original does
 * and it keeps the device callbacks free of decoding work.
 */

#ifndef GEX_AUDIO_SCHED_H
#define GEX_AUDIO_SCHED_H

#include <stdint.h>

#include "gex/audio/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gex_audio_sched {
    gex_audio_stream *stream;
    int paused;
    int finished;            /* mirror of stream->finished once Stop() ran */
    gex_audio_status last_status;
    uint64_t ticks;          /* scheduled ticks since init */
    uint64_t fills;          /* ticks that submitted a half */
    uint64_t deferred;       /* ticks that arrived before the gate opened */
} gex_audio_sched;

/* Attach to a stream that has already been opened (stream_play is optional;
 * ticks before play do nothing because the buffer is not playing yet). */
void gex_audio_sched_init(gex_audio_sched *sched, gex_audio_stream *stream);

/*
 * Side-effect free: would a pump call fill a half right now? Mirrors the gate
 * of 0x005380B3 against the cursor the sink reports. False when paused,
 * finished, before play, or when the cursor is still in the half that would
 * be overwritten. With a sink that has no cursor, always true: the sink
 * consumes what it is given, so the scheduler drains at its own pace.
 */
int gex_audio_sched_due(const gex_audio_sched *sched);

/*
 * One frame-loop pass: pump only if due, keep the counters, and propagate the
 * backend status. A backend error is returned and stored in last_status; it
 * does not stop the scheduler by itself, so the caller decides whether to
 * close the stream.
 */
gex_audio_status gex_audio_sched_tick(gex_audio_sched *sched);

/*
 * Pause stops pumping and, when the sink supports it, freezes the device at
 * its current position (backend.pause). Resume re-arms pumping and continues
 * from that position (backend.resume). For a file/memory sink, which has no
 * position, pause only stops the pump.
 */
void gex_audio_sched_pause(gex_audio_sched *sched);
void gex_audio_sched_resume(gex_audio_sched *sched);

#ifdef __cplusplus
}
#endif

#endif /* GEX_AUDIO_SCHED_H */
