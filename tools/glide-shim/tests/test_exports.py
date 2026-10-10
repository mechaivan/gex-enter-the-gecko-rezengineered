#!/usr/bin/env python3
"""Structural self-test for check_exports.py (parser correctness ONLY).

Builds minimal PE32 fixtures in memory (NOT the real DLLs: naming a
fixture 'glide2x' would prove nothing) and asserts the checker verdicts
good and mutated pairs correctly. Real V-1/V-1b verdicts need the actual
Windows build + renamed real DLL. Exit 0 = all pass."""
import contextlib
import io
import os
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_exports

fails = []


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


CODE3 = ['_grBufferSwap@4', '_grBufferNumPending@0', '_grGlideShutdown@0']
FWD2 = ['_grAlphaBlendFunction@16', '_grTexSource@16']
REAL_MOD = 'glide2x_gex_real'


def build_pe(names, fwd, machine=0x14C, magic=0x10B, extra_unnamed=0):
    """Minimal PE32 DLL bytes. fwd: {name: target_string}."""
    n = len(names)
    nf = n + extra_unnamed
    ed = bytearray(0x1000)
    rva_funcs = 0x1028
    rva_names = rva_funcs + 4 * nf
    rva_ords = rva_names + 4 * n
    strs = {}
    off = (rva_ords + 2 * n) - 0x1000
    blobs = [('n:' + nm, nm.encode() + b'\0') for nm in names]
    blobs += [('f:' + t, t.encode() + b'\0') for t in sorted(set(fwd.values()))]
    blobs.append(('dll', b'fx.dll\0'))
    for tag, b in blobs:
        strs[tag] = 0x1000 + off
        ed[off:off + len(b)] = b
        off += len(b)
    assert off <= len(ed), 'fixture .edata overflow'
    struct.pack_into('<IIHHIIIIIII', ed, 0, 0, 0, 0, 0, strs['dll'], 1,
                     nf, n, rva_funcs, rva_names, rva_ords)
    oF, oN, oO = rva_funcs - 0x1000, rva_names - 0x1000, rva_ords - 0x1000
    for i, nm in enumerate(names):
        r = strs['f:' + fwd[nm]] if nm in fwd else 0x2000
        struct.pack_into('<I', ed, oF + i * 4, r)
    for i in range(extra_unnamed):
        struct.pack_into('<I', ed, oF + (n + i) * 4, 0x2000)
    for i, nm in enumerate(names):
        struct.pack_into('<I', ed, oN + i * 4, strs['n:' + nm])
    for i in range(n):
        struct.pack_into('<H', ed, oO + i * 2, i)
    img = bytearray(0x1200)
    img[0:2] = b'MZ'
    struct.pack_into('<I', img, 0x3C, 0x80)
    img[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', img, 0x84, machine, 1, 0, 0, 0, 0xE0, 0x210E)
    o = 0x98
    struct.pack_into('<H', img, o, magic)
    struct.pack_into('<III', img, o + 28, 0x10000000, 0x1000, 0x200)
    struct.pack_into('<II', img, o + 56, 0x3000, 0x200)
    struct.pack_into('<I', img, o + 92, 16)
    struct.pack_into('<II', img, o + 96, 0x1000, 0x1000)
    sh = 0x178
    img[sh:sh + 8] = b'.edata\0\0'
    struct.pack_into('<IIIIIIHHI', img, sh + 8, 0x1000, 0x1000, 0x1000,
                     0x200, 0, 0, 0, 0, 0x40000040)
    img[0x200:0x1200] = ed
    return bytes(img)


def good_shim():
    return build_pe(CODE3 + FWD2,
                    {n: REAL_MOD + '.' + n for n in FWD2})


def good_real():
    return build_pe(CODE3 + FWD2 + ['_extra@0'], {})


def verdict(shim_bytes, real_bytes, expected):
    with tempfile.TemporaryDirectory() as d:
        s = os.path.join(d, 's.dll')
        r = os.path.join(d, 'r.dll')
        open(s, 'wb').write(shim_bytes)
        open(r, 'wb').write(real_bytes)
        return check_exports.check_pair(s, r, expected)


def verdict_shim(shim_bytes, expected):
    with tempfile.TemporaryDirectory() as d:
        s = os.path.join(d, 's.dll')
        open(s, 'wb').write(shim_bytes)
        return check_exports.check_pair(s, None, expected)


def main():
    exp = CODE3 + FWD2
    ok, rep = verdict(good_shim(), good_real(), exp)
    check(ok, 'good pair passes (real extra allowed)')
    ok, rep = verdict(build_pe(CODE3 + FWD2[1:],
                               {FWD2[1]: REAL_MOD + '.' + FWD2[1]}),
                      good_real(), exp)
    check(not ok and any(FWD2[0] in line for line in rep),
          'shim missing export fails and names it')
    ok, rep = verdict(build_pe(CODE3 + FWD2 + ['_rogue@0'],
                               {n: REAL_MOD + '.' + n for n in FWD2}),
                      good_real(), exp)
    check(not ok and any('_rogue@0' in line for line in rep),
          'shim extra export fails')
    ok, _ = verdict(build_pe(CODE3 + FWD2,
                             {FWD2[0]: 'other.' + FWD2[0],
                              FWD2[1]: REAL_MOD + '.' + FWD2[1]}),
                    good_real(), exp)
    check(not ok, 'forwarder to wrong module fails')
    ok, _ = verdict(build_pe(CODE3 + FWD2,
                             {FWD2[0]: REAL_MOD + '.' + FWD2[1],
                              FWD2[1]: REAL_MOD + '.' + FWD2[1]}),
                    good_real(), exp)
    check(not ok, 'forwarder target symbol mismatch fails')
    ok, _ = verdict(build_pe(CODE3 + FWD2,
                             {'_grBufferSwap@4': REAL_MOD + '._grBufferSwap@4',
                              FWD2[0]: REAL_MOD + '.' + FWD2[0],
                              FWD2[1]: REAL_MOD + '.' + FWD2[1]}),
                    good_real(), exp)
    check(not ok, 'intercepted call forwarded fails')
    ok, _ = verdict(build_pe(CODE3 + FWD2,
                             {FWD2[1]: REAL_MOD + '.' + FWD2[1]}),
                    good_real(), exp)
    check(not ok, 'non-intercepted left as code fails')
    ok, rep = verdict(build_pe(CODE3 + FWD2,
                               {n: REAL_MOD + '.' + n for n in FWD2},
                               machine=0x8664), good_real(), exp)
    check(not ok and any('i386' in line for line in rep),
          'x64 shim fails the arch gate')
    ok, rep = verdict(build_pe(CODE3 + FWD2,
                               {n: REAL_MOD + '.' + n for n in FWD2},
                               magic=0x20B), good_real(), exp)
    check(not ok and any('0x020b' in line for line in rep),
          'PE32+ magic fails the arch gate')
    ok, rep = verdict(good_shim()[:300], good_real(), exp)
    check(not ok and 'CHECK_EXPORTS: FAIL' in rep,
          'truncated shim fails with summary (no crash)')
    ok, rep = verdict(good_shim(), build_pe([FWD2[1]], {}), exp)
    check(not ok and any(FWD2[0] in line for line in rep),
          'real missing a forwarder target fails and names it')
    ok, rep = verdict(good_shim(), build_pe(
        ['_grBufferNumPending@0', '_grGlideShutdown@0'] + FWD2, {}), exp)
    check(not ok and any('_grBufferSwap@4' in line for line in rep),
          'real missing _grBufferSwap@4 fails and names it')
    ok, rep = verdict(build_pe(CODE3 + FWD2 + [FWD2[0]],
                               {n: REAL_MOD + '.' + n for n in FWD2}),
                      good_real(), exp)
    check(not ok and any('duplicate' in line for line in rep),
          'duplicate export name fails')
    ok, rep = verdict(build_pe(CODE3 + FWD2,
                               {n: REAL_MOD + '.' + n for n in FWD2},
                               extra_unnamed=1), good_real(), exp)
    check(not ok and any('fully named' in line for line in rep),
          'ordinal-only extra export fails')
    ok, rep = verdict_shim(good_shim(), exp)
    check(ok and 'CHECK_EXPORTS: PASS' in rep
          and any('V-1b pending' in line for line in rep),
          'shim-only good passes and flags V-1b pending')
    ok, rep = verdict_shim(build_pe(CODE3 + FWD2[1:],
                                    {FWD2[1]: REAL_MOD + '.' + FWD2[1]}), exp)
    check(not ok and 'CHECK_EXPORTS: FAIL' in rep,
          'shim-only bad fails with summary')
    tdir = os.path.dirname(os.path.abspath(__file__))
    exp38 = check_exports.load_expected(os.path.join(tdir, 'expected_iat.txt'))
    code38 = [n for n in exp38 if n in check_exports.CODE_EXPORTS]
    fwd38 = [n for n in exp38 if n not in check_exports.CODE_EXPORTS]
    shim38 = build_pe(exp38, {n: REAL_MOD + '.' + n for n in fwd38})
    with tempfile.TemporaryDirectory() as d:
        s = os.path.join(d, 's.dll')
        open(s, 'wb').write(shim38)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = check_exports.main(['check_exports.py', '--shim-only', s,
                                     os.path.join(tdir, 'expected_iat.txt')])
    check(rc == 0 and 'CHECK_EXPORTS: PASS' in buf.getvalue()
          and len(exp38) == 38 and len(code38) == 3 and len(fwd38) == 35,
          'CLI --shim-only passes a full-38 shim via the real list')
    twins = [n[1:] for n in code38]
    ok, rep = verdict_shim(build_pe(exp38 + twins,
                                    {n: REAL_MOD + '.' + n for n in fwd38}),
                           exp38)
    check(not ok and all(t in ' '.join(rep) for t in twins),
          'F-3 shape (38 + undecorated alias twins) fails and names them')
    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
