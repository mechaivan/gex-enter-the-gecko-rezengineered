#!/usr/bin/env python3
"""test_init: STRUCTURAL regression test for the init design (D-02/D-05/D-06).

What it pins (regex/brace analysis of gshim.c — NOT a behavioural test;
behaviour needs Windows + the real DLL, see README "Validar"):
  1. DllMain body contains no LoadLibrary/GetProcAddress/fopen/fflush
     (loader-lock + CRT-I/O ban inside DllMain).
  2. Both wrappers call ensure_init() before touching p_Swap/p_Pending.
  3. Resolution (LoadLibrary/GetProcAddress) only occurs outside DllMain
     (do_init), guarded by InterlockedCompareExchange.
  4. 32-bit build guard present (#error unless i386 on _WIN32).
  5. Detach path flushes only on normal detach (r == NULL guard).
  6. No per-frame unconditional file I/O in wrappers: log goes through
     log_row (ring), fflush appears only in log_row batch / init / detach.
Exit 0 = all pass.
"""
import re
import sys
from pathlib import Path

SHIM = Path(__file__).resolve().parent.parent
fails = []


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


def func_body(src, name):
    """Brace-matched body of the DEFINITION of `name` (comments stripped,
    `) {` required so prototypes/calls never match)."""
    code = re.sub(r'/\*.*?\*/', ' ', src, flags=re.S)
    code = re.sub(r'//[^\n]*', '', code)
    m = re.search(r'\b%s\s*\([^();]*\)\s*\{' % re.escape(name), code)
    assert m, name
    i = code.index('{', m.end() - 1)
    depth, j = 0, i
    while True:
        if code[j] == '{':
            depth += 1
        elif code[j] == '}':
            depth -= 1
            if depth == 0:
                return code[i:j + 1]
        j += 1


def main():
    src = (SHIM / 'gshim.c').read_text()
    dllmain = func_body(src, 'DllMain')
    for tok in ['LoadLibrary', 'GetProcAddress', 'fopen', 'fflush',
                'GetModuleHandle', 'GetEnvironmentVariable']:
        check(tok not in dllmain, f'DllMain has no {tok}')
    check('DisableThreadLibraryCalls' in dllmain,
          'DllMain disables thread calls')
    check(re.search(r'DLL_PROCESS_DETACH\s*&&\s*r\s*==\s*NULL', dllmain)
          is not None, 'detach flush guarded by r == NULL')

    for w in ['grBufferSwap', 'grBufferNumPending']:
        body = func_body(src, w)
        check('ensure_init()' in body, f'{w} calls ensure_init()')

    check('InterlockedCompareExchange(&init_state, 1, 0)' in src,
          'one-time init via InterlockedCompareExchange')
    doinit = func_body(src, 'do_init')
    check('LoadLibrary' in doinit and 'GetProcAddress' in doinit,
          'resolution lives in do_init (outside DllMain)')

    check(re.search(r'#if defined\(_WIN32\).*__i386__.*_M_IX86', src, re.S)
          is not None and '#error' in src,
          '32-bit build guard (#error unless i386 on _WIN32)')

    for w in ['grBufferSwap', 'grBufferNumPending']:
        body = func_body(src, w)
        for tok in ['fprintf', 'fflush', 'fopen', 'fwrite']:
            check(tok not in body, f'{w} wrapper has no direct {tok}')

    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
