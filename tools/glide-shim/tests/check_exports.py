#!/usr/bin/env python3
"""V-1/V-1b export checker for glide-shim (REAL verification logic).

Reads the export tables of the built shim DLL and the renamed real DLL
DIRECTLY from their bytes (no objdump text scraping) and verdicts:
  shim: exactly the 38 expected names; the 3 intercepted calls are code,
        the other 35 are forwarders to glide2x_gex_real.<same name>;
  real: ALL 38 expected names present as named exports (incl. the 3
        intercepted calls the shim resolves; extras OK);
  both: PE32 / i386.
  --shim-only verdicts the shim alone (V-1); V-1b stays pending then.
Needs the real artifacts (built on Windows). Self-tested structurally by
tests/test_exports.py against hand-built PE fixtures (parser correctness,
NOT a validation claim). Exit 0 = PASS, 1 = FAIL, 2 = usage/tool error.
"""
import struct
import sys

CODE_EXPORTS = frozenset(
    ['_grBufferSwap@4', '_grBufferNumPending@0', '_grGlideShutdown@0'])
FWD_MODULE = 'glide2x_gex_real'


class ExportParseError(Exception):
    pass


def _u16(b, off):
    if off < 0 or off + 2 > len(b):
        raise ExportParseError(f'u16 read out of bounds @{off:#x}')
    return struct.unpack_from('<H', b, off)[0]


def _u32(b, off):
    if off < 0 or off + 4 > len(b):
        raise ExportParseError(f'u32 read out of bounds @{off:#x}')
    return struct.unpack_from('<I', b, off)[0]


def _cstr(b, off, limit=512):
    if off < 0 or off >= len(b):
        raise ExportParseError(f'string read out of bounds @{off:#x}')
    end = off
    while end < len(b) and end - off < limit and b[end] != 0:
        end += 1
    if end >= len(b) or end - off >= limit:
        raise ExportParseError(f'unterminated string @{off:#x}')
    return bytes(b[off:end]).decode('ascii', errors='replace')


def parse_exports(path):
    """Return {'machine','magic','nfuncs','nnames','dll','exports'}
    where exports maps name -> {'rva':int,'forwarder':str|None}."""
    try:
        with open(path, 'rb') as f:
            b = f.read()
    except OSError as e:
        raise ExportParseError(f'cannot read {path}: {e}')
    if len(b) < 64 or b[0:2] != b'MZ':
        raise ExportParseError(f'{path}: no MZ header')
    pe = _u32(b, 0x3C)
    if pe <= 0 or pe + 26 > len(b):
        raise ExportParseError(f'{path}: bad e_lfanew ({pe})')
    if bytes(b[pe:pe + 2]) != b'PE':
        raise ExportParseError(f'{path}: no PE signature')
    machine = _u16(b, pe + 4)
    magic = _u16(b, pe + 24)
    if machine != 0x014C or magic != 0x010B:
        raise ExportParseError(
            f'{path}: not PE32/i386 (machine={machine:#06x} magic={magic:#06x})')
    nsec = _u16(b, pe + 6)
    optsize = _u16(b, pe + 20)
    if optsize < 96 + 8:
        raise ExportParseError(f'{path}: optional header too small')
    exp_rva = _u32(b, pe + 24 + 96)
    exp_size = _u32(b, pe + 24 + 100)
    if exp_rva == 0 or exp_size == 0:
        raise ExportParseError(f'{path}: no export directory')
    secs = []
    sec_off = pe + 24 + optsize
    for i in range(nsec):
        o = sec_off + i * 40
        if o + 40 > len(b):
            raise ExportParseError(f'{path}: section table truncated')
        vsize = _u32(b, o + 8)
        vaddr = _u32(b, o + 12)
        rawsize = _u32(b, o + 16)
        rawptr = _u32(b, o + 20)
        secs.append((vaddr, max(vsize, rawsize), rawptr))

    def r2f(rva):
        for (vaddr, size, rawptr) in secs:
            if vaddr <= rva < vaddr + size:
                f = rawptr + (rva - vaddr)
                if 0 <= f < len(b):
                    return f
        raise ExportParseError(f'{path}: RVA {rva:#x} unmapped')

    ed = r2f(exp_rva)
    if ed + 40 > len(b):
        raise ExportParseError(f'{path}: export directory truncated')
    nfuncs = _u32(b, ed + 20)
    nnames = _u32(b, ed + 24)
    if nfuncs > 100000 or nnames > 100000 or nnames > nfuncs:
        raise ExportParseError(
            f'{path}: insane export counts ({nfuncs}/{nnames})')
    dll_name = _cstr(b, r2f(_u32(b, ed + 12)))
    a_funcs = r2f(_u32(b, ed + 28))
    a_names = r2f(_u32(b, ed + 32))
    a_ords = r2f(_u32(b, ed + 36))
    exports = {}
    for i in range(nnames):
        if a_names + 4 * i + 4 > len(b) or a_ords + 2 * i + 2 > len(b):
            raise ExportParseError(f'{path}: name/ordinal table truncated')
        nm = _cstr(b, r2f(_u32(b, a_names + 4 * i)))
        if nm in exports:
            raise ExportParseError(f'{path}: duplicate export {nm!r}')
        oi = _u16(b, a_ords + 2 * i)
        if oi >= nfuncs or a_funcs + 4 * oi + 4 > len(b):
            raise ExportParseError(f'{path}: ordinal {oi} out of range')
        frva = _u32(b, a_funcs + 4 * oi)
        fwd = None
        if exp_rva <= frva < exp_rva + exp_size:
            fwd = _cstr(b, r2f(frva))
        exports[nm] = {'rva': frva, 'forwarder': fwd}
    return {'machine': machine, 'magic': magic, 'nfuncs': nfuncs,
            'nnames': nnames, 'dll': dll_name, 'exports': exports}


def load_expected(path):
    names = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                names.append(line)
    return names


def check_pair(shim_path, real_path, expected):
    """Return (ok, report_lines). real_path None = shim-only (V-1).
    Never raises on verdicts; parse errors become FAIL lines ending
    in a CHECK_EXPORTS summary (evidence, not tracebacks)."""
    rep = []
    ok = True

    def gate(passed, text):
        nonlocal ok
        rep.append(('PASS ' if passed else 'FAIL ') + text)
        if not passed:
            ok = False

    shim_only = real_path is None
    try:
        shim = parse_exports(shim_path)
    except ExportParseError as e:
        return False, ['FAIL shim unreadable: ' + str(e),
                       'CHECK_EXPORTS: FAIL']
    gate(True, f'shim arch PE32/i386 ({shim_path})')
    snames = set(shim['exports'])
    gate(shim['nfuncs'] == shim['nnames'],
         f'shim fully named ({shim["nfuncs"]}/{shim["nnames"]})')
    missing = [n for n in expected if n not in snames]
    extra = sorted(snames - set(expected))
    gate(not missing, f'shim exports all 38 expected'
         + ('' if not missing else f' (missing: {", ".join(missing)})'))
    gate(not extra, 'shim exports nothing extra'
         + ('' if not extra else f' (extra: {", ".join(extra)})'))
    code = sorted(n for n, e in shim['exports'].items() if not e['forwarder'])
    gate(set(code) == CODE_EXPORTS,
         f'shim code exports are exactly the 3 intercepted'
         + ('' if set(code) == CODE_EXPORTS else f' (got: {", ".join(code)})'))
    bad_fwd = []
    for n, e in sorted(shim['exports'].items()):
        if e['forwarder'] is None:
            continue
        tgt = e['forwarder']
        mod, _, sym = tgt.partition('.')
        if mod.lower() != FWD_MODULE or not sym or sym != n:
            bad_fwd.append(f'{n}->{tgt}')
    gate(not bad_fwd, f'shim forwarders target {FWD_MODULE}.<same name>'
         + ('' if not bad_fwd else f' (bad: {"; ".join(bad_fwd)})'))
    if shim_only:
        rep.append('CHECK_EXPORTS: ' + ('PASS' if ok else 'FAIL'))
        rep.append('NOTE shim-only: V-1b pending (real DLL not checked)')
        return ok, rep
    try:
        real = parse_exports(real_path)
    except ExportParseError as e:
        rep.append('FAIL real DLL unreadable: ' + str(e))
        rep.append('CHECK_EXPORTS: FAIL')
        return False, rep
    gate(True, f'real arch PE32/i386 ({real_path})')
    rnames = set(real['exports'])
    unresolved = sorted(n for n in expected if n not in rnames)
    gate(not unresolved, 'real DLL exports all expected names'
         + ('' if not unresolved else f' (unresolved: {", ".join(unresolved)})'))
    rep.append('CHECK_EXPORTS: ' + ('PASS' if ok else 'FAIL'))
    return ok, rep


def main(argv):
    args = argv[1:]
    shim_only = args[:1] == ['--shim-only']
    if shim_only:
        args = args[1:]
    if len(args) != (2 if shim_only else 3):
        print('usage: check_exports.py [--shim-only] <shim.dll> '
              '[<real.dll>] <expected_iat.txt>')
        return 2
    try:
        expected = load_expected(args[-1])
    except OSError as e:
        print(f'FAIL cannot read expected list: {e}')
        return 2
    if len(expected) != 38:
        print(f'FAIL expected list has {len(expected)} names, want 38')
        return 1
    ok, rep = check_pair(args[0], None if shim_only else args[1], expected)
    for line in rep:
        print(line)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
