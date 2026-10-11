#!/usr/bin/env python3
"""test_win32logic: Linux-executed equivalents of Win32-only C logic.

W03 (V-2): the trip-marker scan compared 38 chars against a 37-char
prefix, matching nothing. TRIPSCAN extracts the REAL scan predicate
(literal + length) from tests/win32/harness.c, the REAL marker format
from gshim.c, and the REAL read_line, compiles a scan driver, and runs
it over a fixture: trips must be exactly 1. Fails on the old 38.
W05/W10 (V-2): the fake's STRICT finalize-before-forward check 99'd
when IO was blocked, although finalize HAD run (loudly). FAKELOGIC
compiles the REAL tests/win32/fakereal.c against the behaviour stubs
and drives grGlideShutdown through 6 file states: perfect artifacts
pass, degraded + error-note passes (the fix), missing-everything
still exits 99 (the assertion is NOT relaxed).
Needs gcc for the behavioural parts; loud SKIP otherwise (the
extraction pins still run). Exit 0 = all pass.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

TESTDIR = os.path.dirname(os.path.abspath(__file__))
SHIMDIR = os.path.dirname(TESTDIR)
HARNESS_C = os.path.join(TESTDIR, 'win32', 'harness.c')
FAKE_C = os.path.join(TESTDIR, 'win32', 'fakereal.c')
GSHIM_C = os.path.join(SHIMDIR, 'gshim.c')
BEHAVIOR = os.path.join(TESTDIR, 'behavior')
IMPL_C = os.path.join(BEHAVIOR, 'harness_impl.c')
fails = []


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


SCAN_MAIN = r'''
#include <stdio.h>
#include <string.h>
#include <stddef.h>
@READLINE@
@WANTDECL@
int main(void)
{
    FILE *f = fopen("scan.csv", "r");
    char line[512];
    int trips = 0;
    if (!f) {
        printf("no fixture\n");
        return 2;
    }
    while (read_line(f, line, sizeof(line))) {
        if (strncmp(line, @LIT@, @LEN@) == 0)
            trips++;
    }
    printf("trips=%d\n", trips);
    return trips == 1 ? 0 : 1;
}
'''

FAKE_MAIN = r'''
#include <stdio.h>
void grGlideShutdown(void);
int main(void)
{
    grGlideShutdown();
    printf("returned\n");
    return 0;
}
'''


def extract_fn(src, start_mark):
    start = src.find(start_mark)
    if start < 0:
        return None
    end = src.find('\n}\n', start)
    if end < 0:
        return None
    return src[start:end + 3]


def tripscan_static():
    """Extraction pins (stdlib only). Returns driver context or None."""
    h = open(HARNESS_C, encoding='utf-8').read()
    if h.count('W03_overflow') < 1 or h.count('W04_nolog') < 1:
        check(False, 'tripscan: W03 block markers found')
        return None
    check(True, 'tripscan: W03 block markers found')
    b = h.split('W03_overflow')[1].split('W04_nolog')[0]
    m = re.search(r'static const char want\[\] =\s*"([^"]+)";', b)
    if m:
        lit, litexpr, lenexpr = m.group(1), 'want', 'sizeof(want) - 1'
        wantdecl = 'static const char want[] = "%s";' % lit
    else:
        m = re.search(r'strncmp\(line,\s*"([^"]+)",\s*([^)]+)\)', b)
        if not m:
            check(False, 'tripscan: scan predicate extracted')
            return None
        lit = m.group(1)
        litexpr = '"%s"' % lit
        lenexpr = m.group(2).strip()
        wantdecl = ''
    check(True, 'tripscan: scan predicate extracted')
    check(lenexpr == 'sizeof(want) - 1',
          'tripscan: length derived via sizeof (no hand count)')
    g = open(GSHIM_C, encoding='utf-8').read()
    m = re.search(r'# overflow at seq=%u: ([^"]*?)\\n', g)
    if not m:
        check(False, 'tripscan: marker format extracted from gshim.c')
        return None
    check(True, 'tripscan: marker format extracted from gshim.c')
    marker = '# overflow at seq=262144: ' + m.group(1)
    check(marker.startswith(lit),
          'tripscan: shim marker agrees with harness literal')
    readline = extract_fn(h, 'static int read_line(FILE *f')
    if readline is None:
        check(False, 'tripscan: read_line extracted verbatim')
        return None
    check(True, 'tripscan: read_line extracted verbatim')
    return {'lit': lit, 'litexpr': litexpr, 'lenexpr': lenexpr,
            'wantdecl': wantdecl, 'marker': marker,
            'readline': readline}


def tripscan_run(gcc, ctx):
    with tempfile.TemporaryDirectory() as d:
        src = SCAN_MAIN.replace('@READLINE@', ctx['readline'])
        src = src.replace('@WANTDECL@', ctx['wantdecl'])
        src = src.replace('@LIT@', ctx['litexpr'])
        src = src.replace('@LEN@', ctx['lenexpr'])
        c = os.path.join(d, 'scan.c')
        exe = os.path.join(d, 'scan')
        open(c, 'w').write(src)
        fixture = ('# gshim test qpf=1 (init) buf=2048\n'
                   '# seq,tick_raw,event,arg\n'
                   '1,100,S,3\n'
                   '2,200,S,3\n'
                   '# overflow at seq=26214: RUN INVALID, tail dropped\n'
                   + ctx['marker'] + '\n'
                   + '3,300,S,3\n'
                   '# end rows=3 overflow=1 swaps=300000 by=shutdown\n')
        open(os.path.join(d, 'scan.csv'), 'w').write(fixture)
        p = subprocess.run([gcc, '-std=c11', '-O2', '-o', exe, c],
                           capture_output=True, text=True)
        check(p.returncode == 0, 'tripscan: scan driver compiles'
              + ('' if p.returncode == 0
                 else ' (%s)' % p.stderr.strip()[:200]))
        if p.returncode != 0:
            return
        p = subprocess.run([exe], capture_output=True, text=True, cwd=d)
        check(p.returncode == 0,
              'tripscan: exactly 1 trip (%s)' % p.stdout.strip())


FAKE_CASES = [
    # (label, env, files, want_rc, want_counts)
    ('perfect marker passes',
     {'GSHIM_NOLOG': '1'},
     {'gshim_nolog.marker': 'gshim test nolog=1 qpf=1\n'
                            'end swaps=1 pending_calls=0 by=shutdown\n'},
     0, True),
    ('blocked marker + end-failed note passes (the fix)',
     {'GSHIM_NOLOG': '1'},
     {'gshim_error.txt': 'gshim: nolog marker end failed '
                         '(clean exit unprovable)\n'},
     0, True),
    ('blocked marker without note still exits 99',
     {'GSHIM_NOLOG': '1'},
     {},
     99, False),
    ('perfect footer passes',
     {},
     {'gshim_log.csv': '# gshim test\n1,2,S,3\n'
                       '# end rows=1 overflow=0 swaps=1 by=shutdown\n'},
     0, True),
    ('blocked log + unavailable note passes (the fix)',
     {},
     {'gshim_error.txt': 'gshim: finalize: log unavailable (rows lost)\n'},
     0, True),
    ('blocked log without note still exits 99',
     {},
     {},
     99, False),
]


def fakelogic(gcc):
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, 'fmain.c')
        exe = os.path.join(d, 'fmain')
        open(c, 'w').write(FAKE_MAIN)
        p = subprocess.run(
            [gcc, '-std=c11', '-O1', '-I', BEHAVIOR, FAKE_C, IMPL_C,
             c, '-ldl', '-o', exe], capture_output=True, text=True)
        check(p.returncode == 0, 'fakelogic: real fakereal.c compiles'
              + ('' if p.returncode == 0
                 else ' (%s)' % p.stderr.strip()[:300]))
        if p.returncode != 0:
            return
        for label, env, files, want_rc, want_counts in FAKE_CASES:
            with tempfile.TemporaryDirectory() as t:
                for name, content in files.items():
                    open(os.path.join(t, name), 'w').write(content)
                runenv = {k: v for k, v in os.environ.items()
                          if not k.startswith('GSHIM_')}
                runenv.update(env)
                p = subprocess.run([exe], capture_output=True, text=True,
                                   cwd=t, env=runenv)
                ok = p.returncode == want_rc
                detail = 'rc=%d' % p.returncode
                if ok and want_counts:
                    counts = os.path.join(t, 'forwarded.counts')
                    ok = (os.path.exists(counts)
                          and 'shutdown=1' in open(counts).read())
                    detail += ' + counts'
                check(ok, 'fakelogic: %s (%s)' % (label, detail))


def main():
    ctx = tripscan_static()
    gcc = shutil.which('gcc')
    if gcc is None:
        print('SKIP tripscan behavioural (no gcc)')
        print('SKIP fakelogic (no gcc)')
    else:
        if ctx is not None:
            tripscan_run(gcc, ctx)
        fakelogic(gcc)
    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
