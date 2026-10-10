#!/usr/bin/env python3
"""test_api: every .def arity (@N) matches the Glide 2.x SDK signature.

Fixture glide2x_signatures.txt pins (func, nparams, @N) derived from the
Glide 2.x SDK headers (provenance inside the file; FX_CALL=__stdcall).
This test asserts:
  1. 4*nparams == @N for all 38 rows (x86 stdcall arithmetic; all SDK
     params are 4-byte kinds — verified at fixture generation).
  2. Every row's decorated name _(func)@(N) is exported by gshim.def
     (forwarder or code export) and vice versa (no extras).
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


def main():
    rows = [l.split() for l in open(SHIM / 'tests' / 'glide2x_signatures.txt')
            if l.strip() and not l.startswith('#')]
    check(len(rows) == 38, f'fixture has 38 rows (got {len(rows)})')
    ok = True
    for r in rows:
        if len(r) != 3:
            ok = False
            break
        _, n, w = r
        if 4 * int(n) != int(w):
            ok = False
    check(ok, '4*nparams == @N for every row')

    deftext = (SHIM / 'gshim.def').read_text()
    exports = set(re.findall(r'(_gr\w+@\d+|_gu\w+@\d+)=', deftext))
    exports |= set(re.findall(r'(?m)^\s*(_gr\w+@\d+|_gu\w+@\d+)\s*$', deftext))
    want = {f'_{fn}@{w}' for fn, _, w in rows}
    check(want == exports,
          f'fixture names == .def exports (missing={sorted(want - exports)}, '
          f'extra={sorted(exports - want)})')

    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
