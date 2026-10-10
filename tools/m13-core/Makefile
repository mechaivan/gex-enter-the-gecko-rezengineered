CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

all: test_m13_core

test_m13_core: m13_core.c test_m13_core.c m13_core.h
	$(CC) $(CFLAGS) -o $@ m13_core.c test_m13_core.c

clean:
	rm -f test_m13_core

.PHONY: all clean
