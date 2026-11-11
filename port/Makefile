# Port build (POSIX). On Windows use build.ps1 instead: there is no make here.
#
#   make test   build and run the native checks (container + camera/BSP +
#               audio + loop + I-4 app)
#   make demo   build sagdump/sagplay and run their synthetic demos
#   make audio  build sagplay and decode the synthetic tone
#   make clean  remove build/ (only files this build produced)
#
# The DirectSound device backend (src/audio/backend_dsound.c, test_device.c),
# the D3D11 device, swap chain and mesh buffers (src/render/d3d11_device.c,
# src/render/d3d11_swapchain.c, src/render/d3d11_mesh.c, test_d3d11_device.c,
# test_d3d11_swapchain.c, test_d3d11_mesh.c, tools/d3d11probe.c,
# tools/swapprobe.c) and the Win32 layer (src/platform/win32_*.c,
# test_win32_time.c, gexloop) are Windows-only and are built by build.ps1.
# Everything else here is portable C11, including the I-4 input tables, app
# core, clock conversion and the geometry conversion of Module 3
# (src/geometry/geometry.c, test_geometry.c).

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
CAMERA_SRC := src/camera/camera.c
CAMERA_HDR := include/gex/camera.h
BSP_SRC   := src/bsp/bsp.c
BSP_HDR   := include/gex/bsp.h
DFX_SRC   := src/asset/dfx.c
DFX_HDR   := include/gex/asset/dfx.h
EMIT_SRC  := src/emit/emit.c
EMIT_HDR  := include/gex/emit.h
GEOMETRY_SRC := src/geometry/geometry.c
GEOMETRY_HDR := include/gex/geometry.h
INPUT_SRC := src/input/key_state.c
INPUT_HDR := include/gex/input/key_state.h
RENDER_SRC := src/render/null_renderer.c
RENDER_HDR := include/gex/render/null_renderer.h
APP_SRC   := src/app/i4_app.c
APP_HDR   := include/gex/app/i4_app.h
CLOCK_SRC := src/platform/clock.c
CLOCK_HDR := include/gex/platform/clock.h

.PHONY: all test demo audio clean

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_sag: tests/test_sag.c $(SAG_SRC) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_sag.c $(SAG_SRC)

$(BUILD)/test_camera: tests/test_camera.c $(CAMERA_SRC) $(BSP_SRC) $(CAMERA_HDR) $(BSP_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_camera.c $(CAMERA_SRC) $(BSP_SRC) $(LDLIBS)

$(BUILD)/test_bsp: tests/test_bsp.c $(BSP_SRC) $(CAMERA_SRC) $(DFX_SRC) $(SAG_SRC) $(BSP_HDR) $(CAMERA_HDR) $(DFX_HDR) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -DGEX_BSP_TEST_FAULT_INJECT -o $@ tests/test_bsp.c $(BSP_SRC) $(CAMERA_SRC) $(DFX_SRC) $(SAG_SRC) $(LDLIBS)

$(BUILD)/test_level_emit: tests/test_level_emit.c $(EMIT_SRC) $(BSP_SRC) $(CAMERA_SRC) $(DFX_SRC) $(EMIT_HDR) $(BSP_HDR) $(CAMERA_HDR) $(DFX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_level_emit.c $(EMIT_SRC) $(BSP_SRC) $(CAMERA_SRC) $(DFX_SRC) $(LDLIBS)

$(BUILD)/test_geometry: tests/test_geometry.c $(GEOMETRY_SRC) $(EMIT_SRC) $(BSP_SRC) $(CAMERA_SRC) $(DFX_SRC) $(GEOMETRY_HDR) $(EMIT_HDR) $(BSP_HDR) $(CAMERA_HDR) $(DFX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -DGEX_GEOM_TEST_FAULT_INJECT -o $@ tests/test_geometry.c $(GEOMETRY_SRC) $(EMIT_SRC) $(BSP_SRC) $(CAMERA_SRC) $(DFX_SRC) $(LDLIBS)

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

$(BUILD)/test_input: tests/test_input.c $(INPUT_SRC) $(INPUT_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_input.c $(INPUT_SRC)

$(BUILD)/test_i4_app: tests/test_app.c $(APP_SRC) $(INPUT_SRC) $(RENDER_SRC) $(CORE_SRC) $(AUDIO_SRC) $(CLOCK_SRC) $(APP_HDR) $(INPUT_HDR) $(RENDER_HDR) $(CORE_HDR) $(AUDIO_HDR) $(CLOCK_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_app.c $(APP_SRC) $(INPUT_SRC) $(RENDER_SRC) $(CORE_SRC) $(AUDIO_SRC) $(CLOCK_SRC)

$(BUILD)/sagplay: tools/sagplay.c $(AUDIO_SRC) $(SAG_SRC) $(AUDIO_HDR) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/sagplay.c $(AUDIO_SRC) $(SAG_SRC) $(LDLIBS)

test: $(BUILD)/test_sag $(BUILD)/test_camera $(BUILD)/test_bsp $(BUILD)/test_level_emit $(BUILD)/test_geometry $(BUILD)/test_adpcm $(BUILD)/test_audio $(BUILD)/test_frame $(BUILD)/test_input $(BUILD)/test_i4_app
	./$(BUILD)/test_sag
	./$(BUILD)/test_camera
	./$(BUILD)/test_bsp
	./$(BUILD)/test_level_emit
	./$(BUILD)/test_geometry
	./$(BUILD)/test_adpcm
	./$(BUILD)/test_audio
	./$(BUILD)/test_frame
	./$(BUILD)/test_input
	./$(BUILD)/test_i4_app

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
