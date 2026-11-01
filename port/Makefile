# Port build (POSIX). On Windows use build.ps1 instead: there is no make here.
#
#   make test   build and run the native checks (container + audio)
#   make demo   build sagdump/sagplay and run their synthetic demos
#   make audio  build sagplay and decode the synthetic tone
#   make clean  remove build/ (only files this build produced)
#
# The DirectSound device backend (src/audio/backend_dsound.c, test_device.c)
# is Windows-only and is built by build.ps1; `sagplay --device` therefore
# exists only in Windows builds. Everything else here is portable C11.

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
LDLIBS  ?= -lm
BUILD   := build

SAG_SRC   := src/asset/sag.c
SAG_HDR   := include/gex/asset/sag.h
AUDIO_SRC := src/audio/adpcm.c src/audio/stream.c src/audio/sched.c
AUDIO_HDR := include/gex/audio/adpcm.h include/gex/audio/stream.h \
             include/gex/audio/sched.h
CORE_SRC  := src/core/loop_core.c src/core/frame.c
CORE_HDR  := include/gex/core/loop_core.h include/gex/core/frame.h

.PHONY: all test demo audio clean

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_sag: tests/test_sag.c $(SAG_SRC) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_sag.c $(SAG_SRC)

$(BUILD)/test_adpcm: tests/test_adpcm.c $(AUDIO_SRC) $(AUDIO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_adpcm.c $(AUDIO_SRC)

$(BUILD)/test_audio: tests/test_audio.c $(AUDIO_SRC) $(SAG_SRC) $(AUDIO_HDR) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_audio.c $(AUDIO_SRC) $(SAG_SRC)

$(BUILD)/sagdump: tools/sagdump.c $(SAG_SRC) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/sagdump.c $(SAG_SRC)

$(BUILD)/test_frame: tests/test_frame.c $(CORE_SRC) $(AUDIO_SRC) $(SAG_SRC) $(CORE_HDR) $(AUDIO_HDR) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_frame.c $(CORE_SRC) $(AUDIO_SRC) $(SAG_SRC)

$(BUILD)/framedemo: tools/framedemo.c $(CORE_SRC) $(AUDIO_SRC) $(SAG_SRC) $(CORE_HDR) $(AUDIO_HDR) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/framedemo.c $(CORE_SRC) $(AUDIO_SRC) $(SAG_SRC) $(LDLIBS)

$(BUILD)/sagplay: tools/sagplay.c $(AUDIO_SRC) $(SAG_SRC) $(AUDIO_HDR) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/sagplay.c $(AUDIO_SRC) $(SAG_SRC) $(LDLIBS)

test: $(BUILD)/test_sag $(BUILD)/test_adpcm $(BUILD)/test_audio $(BUILD)/test_frame
	./$(BUILD)/test_sag
	./$(BUILD)/test_adpcm
	./$(BUILD)/test_audio
	./$(BUILD)/test_frame

demo: $(BUILD)/sagdump $(BUILD)/sagplay $(BUILD)/framedemo
	node tests/make_demo_sag.mjs $(BUILD)/demo.sag
	./$(BUILD)/sagdump $(BUILD)/demo.sag
	./$(BUILD)/framedemo --frames 4
	./$(BUILD)/framedemo --decoupled --frames 3 --dt 60000 --audio
	./$(BUILD)/sagplay

audio: $(BUILD)/sagplay
	./$(BUILD)/sagplay

clean:
	rm -rf $(BUILD)
