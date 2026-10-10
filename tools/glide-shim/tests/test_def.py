#!/usr/bin/env python3
"""test_def: gshim.def covers the exe's 38 glide imports, exactly.

Checks (all static, stdlib only):
  1. expected_iat.txt holds 38 unique decorated names (fixture provenance
     inside the file: EU exe md5 692b1282..., 0 ordinal-only).
  2. gshim.def has a LIBRARY line, 36 same-name forwarder exports and
     exactly the 2 code exports (_grBufferSwap@4, _grBufferNumPending@0).
  3. Union of .def exports == IAT set (no missing, no extras, no dupes).
  4. Each forwarder target is glide2x_gex_real.<same-name>.
  5. Each code-exported name exists in gshim.c as a __stdcall function
     with __declspec(dllexport) (belt and braces: .def + attribute).
Exit 0 = all pass; prints FAIL lines otherwise.
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
    iat = [l.strip() for l in open(SHIM / 'tests' / 'expected_iat.txt')
           if l.strip() and not l.startswith('#')]
    check(len(iat) == 38, f'fixture has 38 names (got {len(iat)})')
    check(len(set(iat)) == 38, 'fixture names unique')
    check(all(re.match(r'_(gr|gu)\w+@\d+$', n) for n in iat),
          'fixture names all decorated stdcall')

    deftext = (SHIM / 'gshim.def').read_text()
    lines = [l.split(';')[0].strip() for l in deftext.split('\n')]
    lines = [l for l in lines if l]
    check(any(l.startswith('LIBRARY') for l in lines), '.def has LIBRARY')
    check('EXPORTS' in lines, '.def has EXPORTS')

    fwds, codes = {}, []
    for l in lines:
        m = re.fullmatch(r'(_gr\w+@\d+|_gu\w+@\d+)=glide2x_gex_real\.(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            fwds[m.group(1)] = m.group(2)
            continue
        m = re.fullmatch(r'(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            codes.append(m.group(1))
    check(len(fwds) == 36, f'36 forwarders (got {len(fwds)})')
    check(all(k == v for k, v in fwds.items()),
          'every forwarder target == export name (same-name)')
    check(sorted(codes) == ['_grBufferNumPending@0', '_grBufferSwap@4'],
          f'code exports are exactly Swap+Pending (got {sorted(codes)})')
    union = set(fwds) | set(codes)
    check(union == set(iat),
          f'.def union == IAT set (missing={sorted(set(iat) - union)}, '
          f'extra={sorted(union - set(iat))})')
    check(len(fwds) + len(codes) == len(union), 'no duplicate export names')

    csrc = (SHIM / 'gshim.c').read_text()
    for dec, fn, sig in [('_grBufferSwap@4', 'grBufferSwap',
                          r'int32_t(\s+\w+)?'),
                         ('_grBufferNumPending@0', 'grBufferNumPending',
                          r'void')]:
        m = re.search(r'__declspec\(dllexport\)\s+\S.*__stdcall\s+%s\s*\(\s*%s\s*\)'
                      % (re.escape(fn), sig), csrc)
        check(m is not None,
              f'{dec}: dllexport + __stdcall wrapper with ({sig}) in gshim.c')

    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
