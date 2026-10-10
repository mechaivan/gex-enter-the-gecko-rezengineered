#!/usr/bin/env python3
"""test_def: gshim.def + dllexport cover the exe's 38 glide imports.

F-3 SPLIT (2026-10-11, MinGW-32 matrix-proven): no .def spelling can
express the 3 decorated stdcall code exports (bare and alias-with-
underscore internals do not link; a mismatched internal is ALSO
exported as a twin; undecorated lines export the wrong names).
Authority is therefore split:
  - gshim.def carries ONLY the 35 same-name forwarder exports;
  - the 3 intercepted calls are exported ONLY via __declspec(dllexport)
    in gshim.c (explicit annotation, not linker auto-export).
Checks (all static, stdlib only):
  1. expected_iat.txt holds 38 unique decorated names (fixture provenance
     inside the file: EU exe md5 692b1282..., 0 ordinal-only).
  2. gshim.def has a LIBRARY line, 35 same-name forwarder exports to
     glide2x_gex_real.<same name>, and NO other export line.
  3. The 3 code names are ABSENT from gshim.def (any spelling: bare,
     quoted, or alias — every F-3-failing shape is rejected).
  4. .def forwarders UNION dllexport-3 == IAT set (no missing, no extras).
  5. Each code name exists in gshim.c as a __stdcall function with
     __declspec(dllexport) and the verified signature (the authority).
  6. The shutdown wrapper calls finalize_log (final dump on the game
     thread; DllMain DETACH must stay a no-op — see test_init).
  7. tests/win32/fakereal_full.def has LIBRARY + EXPORTS and NO export
     lines (same F-3 rule; all 4 fake exports are dllexport-only).
  8. fakereal.c guards each droppable function in its #ifndef
     FAKE_NO_* block and dllexports exactly the 4 fake functions.
  9. build-harness.sh compiles the 3 -DFAKE_NO_* variants (the variant
     mechanism replacing .def surgery) and generates no .def variants.
Exit 0 = all pass; prints FAIL lines otherwise.
"""
import re
import sys
from pathlib import Path

SHIM = Path(__file__).resolve().parent.parent
CODE3 = ['_grBufferNumPending@0', '_grBufferSwap@4', '_grGlideShutdown@0']
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

    fwds, others = {}, []
    for l in lines:
        m = re.fullmatch(r'(_gr\w+@\d+|_gu\w+@\d+)=glide2x_gex_real\.(_gr\w+@\d+|_gu\w+@\d+)', l)
        if m:
            fwds[m.group(1)] = m.group(2)
        elif not l.startswith('LIBRARY') and l != 'EXPORTS':
            others.append(l)
    check(len(fwds) == 35, f'35 forwarders (got {len(fwds)})')
    check(all(k == v for k, v in fwds.items()),
          'every forwarder target == export name (same-name)')
    check(not others, f'.def carries no non-forwarder line (got {others})')
    code_in_def = [l for l in lines
                   if 'BufferSwap' in l or 'BufferNumPending' in l
                   or 'GlideShutdown' in l]
    check(not code_in_def,
          f'code-3 absent from .def in any spelling (got {code_in_def})')
    union = set(fwds) | set(CODE3)
    check(union == set(iat),
          f'.def FORWARDERS union dllexport-3 == IAT set '
          f'(missing={sorted(set(iat) - union)}, '
          f'extra={sorted(union - set(iat))})')
    check(len(fwds) + len(CODE3) == len(union), 'no duplicate export names')

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
              f'{dec}: dllexport + __stdcall wrapper with ({sig}) '
              f'in gshim.c (authority)')
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
    fothers = [l for l in flines
               if not l.startswith('LIBRARY') and l != 'EXPORTS']
    check(not fothers,
          f'fakereal_full.def carries no export line (got {fothers})')

    fsrc = (SHIM / 'tests' / 'win32' / 'fakereal.c').read_text()
    for guard, fn in [('FAKE_NO_SWAP', 'grBufferSwap'),
                      ('FAKE_NO_PENDING', 'grBufferNumPending'),
                      ('FAKE_NO_SHUTDOWN', 'grGlideShutdown')]:
        m = re.search(r'#ifndef %s.*?__declspec\(dllexport\).*?__stdcall\s+%s\s*\(.*?#endif'
                      % (guard, re.escape(fn)), fsrc, re.S)
        check(m is not None,
              f'fakereal.c: {fn} dllexported inside #ifndef {guard}')
    ndll = len(re.findall(r'__declspec\(dllexport\)', fsrc))
    check(ndll == 4, f'fakereal.c dllexports exactly 4 (got {ndll})')

    harness = (SHIM / 'tests' / 'win32' / 'build-harness.sh').read_text()
    for flag in ['-DFAKE_NO_SWAP', '-DFAKE_NO_PENDING', '-DFAKE_NO_SHUTDOWN']:
        check(flag in harness,
              f'build-harness.sh compiles {flag} variant')
    check('fakereal_noswap.def' not in harness
          and 'fakereal_nopending.def' not in harness
          and 'fakereal_noshutdown.def' not in harness,
          'build-harness.sh generates no .def variants (.def surgery dead)')

    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
