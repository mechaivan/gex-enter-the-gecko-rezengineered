CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
PORT = ../../port
CORE = $(PORT)/src/core/loop_core.c
CORE_HDR = $(PORT)/include/gex/core/loop_core.h

all: test_m13_core

test_m13_core: test_m13_core.c m13_core.h $(CORE) $(CORE_HDR)
	$(CC) $(CFLAGS) -I$(PORT)/include -o $@ test_m13_core.c $(CORE)

clean:
	rm -f test_m13_core

.PHONY: all clean
