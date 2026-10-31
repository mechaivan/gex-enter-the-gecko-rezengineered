# Port build (POSIX). On Windows use build.ps1 instead: there is no make here.
#
#   make test   build and run the native checks
#   make demo   build sagdump and write a synthetic fixture
#   make clean  remove build/ (only files this build produced)

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
BUILD   := build

SAG_SRC := src/asset/sag.c
SAG_HDR := include/gex/asset/sag.h

.PHONY: all test demo clean

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_sag: tests/test_sag.c $(SAG_SRC) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_sag.c $(SAG_SRC)

$(BUILD)/sagdump: tools/sagdump.c $(SAG_SRC) $(SAG_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/sagdump.c $(SAG_SRC)

test: $(BUILD)/test_sag
	./$(BUILD)/test_sag

demo: $(BUILD)/sagdump
	node tests/make_demo_sag.mjs $(BUILD)/demo.sag
	./$(BUILD)/sagdump $(BUILD)/demo.sag

clean:
	rm -rf $(BUILD)
