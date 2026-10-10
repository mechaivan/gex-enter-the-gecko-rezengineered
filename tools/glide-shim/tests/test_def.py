#!/usr/bin/env python3
"""test_def: gshim.def covers the exe's 38 glide imports, exactly.

Checks (all static, stdlib only):
  1. expected_iat.txt holds 38 unique decorated names (fixture provenance
     inside the file: EU exe md5 692b1282..., 0 ordinal-only).
  2. gshim.def has a LIBRARY line, 35 same-name forwarder exports and
     exactly the 3 code exports (_grBufferSwap@4, _grBufferNumPending@0,
     _grGlideShutdown@0; shutdown runs the final dump, v3) — each in
     explicit-alias form "decorated"=internal (MinGW-32 link rule).
  3. Union of .def exports == IAT set (no missing, no extras, no dupes).
  4. Each forwarder target is glide2x_gex_real.<same-name>.
  5. Each code-exported name exists in gshim.c as a __stdcall function
     with __declspec(dllexport) (belt and braces: .def + attribute).
  6. The shutdown wrapper calls finalize_log (final dump on the game
     thread; DllMain DETACH must stay a no-op — see test_init).
  7. REGRESSION (MinGW-32 decorated-name link failure, 2026-10-11): no
     bare decorated line may appear in gshim.def EXPORTS, and every
     code export must be "decorated"=internal with internal ==
     decorated minus the leading underscore — the /tmp-proven form.
  8. tests/win32/fakereal_full.def carries the same alias-only rule
     (3 stdcall + _fakereal_counts@16; same latent link defect).
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
    bare = []
    for l in lines:
        m = re.fullmatch(r'(_gr\w+@\d+|_gu\w+@\d+)=glide2x_gex_real\.(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            fwds[m.group(1)] = m.group(2)
            continue
        m = re.fullmatch(r'"(_gr\w+@\d+|_gu\w+@\d+)"=((?:gr|gu)\w+@\d+)', l)
        if m and m.group(1) == '_' + m.group(2):
            codes.append(m.group(1))
            continue
        m = re.fullmatch(r'(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            bare.append(m.group(1))
    check(len(fwds) == 35, f'35 forwarders (got {len(fwds)})')
    check(all(k == v for k, v in fwds.items()),
          'every forwarder target == export name (same-name)')
    check(sorted(codes) == ['_grBufferNumPending@0', '_grBufferSwap@4',
                            '_grGlideShutdown@0'],
          f'code exports are exactly Swap+Pending+Shutdown (got {sorted(codes)})')
    check(not bare,
          f'no bare decorated lines in EXPORTS (got {sorted(bare)})')
    union = set(fwds) | set(codes)
    check(union == set(iat),
          f'.def union == IAT set (missing={sorted(set(iat) - union)}, '
          f'extra={sorted(union - set(iat))})')
    check(len(fwds) + len(codes) == len(union), 'no duplicate export names')

    csrc = (SHIM / 'gshim.c').read_text()
    for dec, fn, sig in [('_grBufferSwap@4', 'grBufferSwap',
                          r'int32_t(\s+\w+)?'),
                         ('_grBufferNumPending@0', 'grBufferNumPending',
                          r'void'),
                         ('_grGlideShutdown@0', 'grGlideShutdown',
                          r'void')]:
        m = re.search(r'__declspec\(dllexport\)\s+\S.*__stdcall\s+%s\s*\(\s*%s\s*\)'
                      % (re.escape(fn), sig), csrc)
        check(m is not None,
              f'{dec}: dllexport + __stdcall wrapper with ({sig}) in gshim.c')
    m = re.search(r'grGlideShutdown\s*\(\s*void\s*\)\s*\{(.*?)\n\}',
                  csrc, re.S)
    check(m is not None and 'finalize_log(' in m.group(1),
          'grGlideShutdown wrapper calls finalize_log')

    faketext = (SHIM / 'tests' / 'win32' / 'fakereal_full.def').read_text()
    flines = [l.split(';')[0].strip() for l in faketext.split('\n')]
    flines = [l for l in flines if l]
    check(any(l.startswith('LIBRARY') for l in flines),
          'fakereal_full.def has LIBRARY')
    fake_alias = {}
    fake_bare = []
    for l in flines:
        m = re.fullmatch(r'"(_\w+@\d+)"=(\w+@\d+)', l)
        if m and m.group(1) == '_' + m.group(2):
            fake_alias[m.group(1)] = m.group(2)
            continue
        m = re.fullmatch(r'(_\w+@\d+)', l)
        if m:
            fake_bare.append(m.group(1))
    check(sorted(fake_alias) == ['_fakereal_counts@16',
                                 '_grBufferNumPending@0', '_grBufferSwap@4',
                                 '_grGlideShutdown@0'],
          f'fake exports are exactly 3 stdcall + counts, alias form '
          f'(got {sorted(fake_alias)})')
    check(not fake_bare,
          f'no bare decorated lines in fakereal_full.def '
          f'(got {sorted(fake_bare)})')

    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
