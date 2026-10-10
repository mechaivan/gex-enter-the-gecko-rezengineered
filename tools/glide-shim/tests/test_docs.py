#!/usr/bin/env python3
"""test_docs: honesty tripwires for CRT-buffer claims (v5 audit).

The v4 docs claimed "zero disk syscalls on the path", which implicit CRT
flushes disprove. This test bans absolute syscall-denial phrases from the
README and gshim.c, and requires the honest mechanism words in the README.
Case-insensitive substring scan — a tripwire, not a substitute for
reading the docs. Exit 0 = all pass.
"""
import sys
from pathlib import Path

SHIM = Path(__file__).resolve().parent.parent
fails = []

BANNED = ['cero syscalls', 'zero syscall', 'ninguna syscall',
          'sin syscalls', 'no file i/o syscalls',
          'no syscalls on the measured path', 'garantía portable',
          'portable guarantee', 'cota portable', 'on both crts',
          'en ambos crt']
REQUIRED_README = ['implícit', 'setvbuf', '2048', 'msvcrt']


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


def main():
    readme = (SHIM / 'README.md').read_text().lower()
    code = (SHIM / 'gshim.c').read_text().lower()
    hit = [b for b in BANNED if b in readme]
    check(not hit, f'README has no syscall-denial overclaim (hit={hit})')
    hit = [b for b in BANNED if b in code]
    check(not hit, f'gshim.c has no syscall-denial overclaim (hit={hit})')
    missing = [r for r in REQUIRED_README if r not in readme]
    check(not missing, f'README names the mechanism (missing={missing})')
    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
