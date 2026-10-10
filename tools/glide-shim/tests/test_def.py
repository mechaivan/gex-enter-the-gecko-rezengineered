#!/usr/bin/env python3
"""test_def: gshim.def covers the exe's 38 glide imports, exactly.

F-3 RESOLVED (2026-10-11, H4a verified 38-exact on PC): the 3 decorated
stdcall code exports use the mismatch-alias form "decorated"=internal
with internal == decorated minus the leading underscore, and gshim.c
carries NO dllexport annotation — MinGW-32 dllexport emits the
underscore-less spelling as extra twins (38+3), while the alias alone
yields exactly the 38. Bare decorated lines still do not link.
Checks (all static, stdlib only):
  1. expected_iat.txt holds 38 unique decorated names (fixture provenance
     inside the file: EU exe md5 692b1282..., 0 ordinal-only).
  2. gshim.def has a LIBRARY line, 35 same-name forwarder exports and
     exactly the 3 mismatch-alias code exports (_grBufferSwap@4,
     _grBufferNumPending@0, _grGlideShutdown@0).
  3. Union of .def exports == IAT set (no missing, no extras, no dupes).
  4. Each forwarder target is glide2x_gex_real.<same-name>.
  5. gshim.c defines the 3 __stdcall wrappers with verified signatures
     and carries NO dllexport annotation (H4a rule: any re-added
     dllexport reintroduces the 38+3 twins — FAIL).
  6. The shutdown wrapper calls finalize_log (final dump on the game
     thread; DllMain DETACH must stay a no-op — see test_init).
  7. REGRESSION (F-3 shapes): no bare decorated line, no exact-match
     alias ("dec"="dec", either quoting) and no undecorated line in
     gshim.def — matrix A/B/D shapes, all link or export wrong.
  8. tests/win32/fakereal_full.def mirrors H4a (LIBRARY + exactly the
     4 mismatch-alias exports: 3 stdcall + _fakereal_counts@16).
  9. fakereal.c defines the 4 stdcall functions, carries NO dllexport,
     and has NO FAKE_NO_* guards (single variant mechanism).
  10. build-harness.sh derives variants by .def surgery (grep -v) and
     passes NO -DFAKE_NO_* flag (single variant mechanism).
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
    bare, badalias, undecorated = [], [], []
    for l in lines:
        m = re.fullmatch(r'(_gr\w+@\d+|_gu\w+@\d+)=glide2x_gex_real\.(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            fwds[m.group(1)] = m.group(2)
            continue
        m = re.fullmatch(r'"(_gr\w+@\d+|_gu\w+@\d+)"=((?:gr|gu)\w+@\d+)', l)
        if m:
            if m.group(1) == '_' + m.group(2):
                codes.append(m.group(1))
            else:
                badalias.append(l)
            continue
        m = re.fullmatch(r'"(_gr\w+@\d+|_gu\w+@\d+)"="(_gr\w+@\d+|_gu\w+@\d+)"', l)
        if m:
            badalias.append(l)
            continue
        m = re.fullmatch(r'"(_gr\w+@\d+|_gu\w+@\d+)"=(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            badalias.append(l)
            continue
        m = re.fullmatch(r'(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            bare.append(m.group(1))
            continue
        m = re.fullmatch(r'((?:gr|gu)\w+@\d+)', l)
        if m:
            undecorated.append(m.group(1))
    check(len(fwds) == 35, f'35 forwarders (got {len(fwds)})')
    check(all(k == v for k, v in fwds.items()),
          'every forwarder target == export name (same-name)')
    check(sorted(codes) == ['_grBufferNumPending@0', '_grBufferSwap@4',
                            '_grGlideShutdown@0'],
          f'code exports are exactly Swap+Pending+Shutdown, mismatch-alias '
          f'form (got {sorted(codes)})')
    check(not bare,
          f'no bare decorated lines in EXPORTS (got {sorted(bare)})')
    check(not badalias,
          f'no exact-match/mispaired aliases in EXPORTS (got {badalias})')
    check(not undecorated,
          f'no undecorated lines in EXPORTS (got {sorted(undecorated)})')
    union = set(fwds) | set(codes)
    check(union == set(iat),
          f'.def union == IAT set (missing={sorted(set(iat) - union)}, '
          f'extra={sorted(union - set(iat))})')
    check(len(fwds) + len(codes) == len(union), 'no duplicate export names')

    csrc = (SHIM / 'gshim.c').read_text()
    check(re.search(r'__declspec\s*\(\s*dllexport\s*\)', csrc) is None,
          'gshim.c carries no dllexport annotation (H4a rule: twins)')
    for dec, pat in [('_grBufferSwap@4',
                      r'\bvoid\s+__stdcall\s+grBufferSwap\s*\(\s*int32_t(\s+\w+)?\s*\)'),
                     ('_grBufferNumPending@0',
                      r'\bint32_t\s+__stdcall\s+grBufferNumPending\s*\(\s*void\s*\)'),
                     ('_grGlideShutdown@0',
                      r'\bvoid\s+__stdcall\s+grGlideShutdown\s*\(\s*void\s*\)')]:
        check(re.search(pat, csrc) is not None,
              f'{dec}: __stdcall definition with verified signature in gshim.c')
    m = re.search(r'grGlideShutdown\s*\(\s*void\s*\)\s*\{(.*?)\n\}',
                  csrc, re.S)
    check(m is not None and 'finalize_log(' in m.group(1),
          'grGlideShutdown wrapper calls finalize_log')

    faketext = (SHIM / 'tests' / 'win32' / 'fakereal_full.def').read_text()
    flines = [l.split(';')[0].strip() for l in faketext.split('\n')]
    flines = [l for l in flines if l]
    check(any(l.startswith('LIBRARY') for l in flines),
          'fakereal_full.def has LIBRARY')
    check('EXPORTS' in flines, 'fakereal_full.def has EXPORTS')
    falias, fbad = {}, []
    for l in flines:
        m = re.fullmatch(r'"(_\w+@\d+)"=(\w+@\d+)', l)
        if m and m.group(1) == '_' + m.group(2):
            falias[m.group(1)] = m.group(2)
        elif not l.startswith('LIBRARY') and l != 'EXPORTS':
            fbad.append(l)
    check(sorted(falias) == ['_fakereal_counts@16',
                             '_grBufferNumPending@0', '_grBufferSwap@4',
                             '_grGlideShutdown@0'],
          f'fake exports are exactly 3 stdcall + counts, mismatch-alias '
          f'form (got {sorted(falias)})')
    check(not fbad,
          f'fakereal_full.def carries no other line (got {fbad})')

    fsrc = (SHIM / 'tests' / 'win32' / 'fakereal.c').read_text()
    check(re.search(r'__declspec\s*\(\s*dllexport\s*\)', fsrc) is None,
          'fakereal.c carries no dllexport annotation (H4a rule: twins)')
    for dec, pat in [('_grBufferSwap@4',
                      r'\bvoid\s+__stdcall\s+grBufferSwap\s*\(\s*int32_t(\s+\w+)?\s*\)'),
                     ('_grBufferNumPending@0',
                      r'\bint32_t\s+__stdcall\s+grBufferNumPending\s*\(\s*void\s*\)'),
                     ('_grGlideShutdown@0',
                      r'\bvoid\s+__stdcall\s+grGlideShutdown\s*\(\s*void\s*\)'),
                     ('_fakereal_counts@16',
                      r'\bvoid\s+__stdcall\s+fakereal_counts\s*\(')]:
        check(re.search(pat, fsrc) is not None,
              f'{dec}: __stdcall definition in fakereal.c')
    check('FAKE_NO_' not in fsrc,
          'fakereal.c has no FAKE_NO_* guards (single mechanism)')

    harness = (SHIM / 'tests' / 'win32' / 'build-harness.sh').read_text()
    for v in ['fakereal_noswap.def', 'fakereal_nopending.def',
              'fakereal_noshutdown.def']:
        check(v in harness,
              f'build-harness.sh derives {v} (.def surgery)')
    check('grep -vF' in harness,
          'build-harness.sh derives variants with grep -vF')
    check('-DFAKE_NO_' not in harness,
          'build-harness.sh passes no -DFAKE_NO_* (single mechanism)')

    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
