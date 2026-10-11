#!/usr/bin/env python3
"""test_win32logic: Linux/Win32-executed equivalents of Win32-only C logic.

W03 (V-2): the trip-marker scan compared 38 chars against a 37-char
prefix, matching nothing. TRIPSCAN extracts the REAL scan predicate
(literal + length) from tests/win32/harness.c, the REAL marker format
from gshim.c, and the REAL read_line, compiles a scan driver, and runs
it over a fixture: trips must be exactly 1. Fails on the old 38.
W05/W10 (V-2): the fake's STRICT finalize-before-forward check 99'd
when IO was blocked, although finalize HAD run (loudly). FAKELOGIC
compiles the REAL tests/win32/fakereal.c and drives grGlideShutdown
through 6 file states: perfect artifacts pass, degraded + error-note
passes (the fix), missing-everything still exits 99 (not relaxed).
PC re-run on 85d72d2: the Win32 harness itself went 14/14, but the
suite failed because FAKELOGIC could not compile under MSYS2 MINGW32:
the vehicle linked the Linux-only harness_impl.c (<dlfcn.h>,
dlsym(RTLD_NEXT), -ldl: POSIX) and the stubs redefined __stdcall,
which the Win32 compilers predefine (GCC: gcc/config/i386/cygming.h;
clang: <built-in>). FAKELOGIC now links a generated 25-line portable
mini-impl and the stubs #ifndef-guard the Win32 spellings, so the
test compiles AND runs natively on the PC too. STUBTARGET pins both
mechanisms on either host (emulated predefines on Linux, the real
ones on Windows). Compile diagnostics are dumped in full on failure
(the old 300-char cap hid the fatal dlfcn.h error on the PC).
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
BEHAVIOR_STUB = os.path.join(BEHAVIOR, 'windows.h')
WIN32_STUB = os.path.join(TESTDIR, 'win32_stub', 'windows.h')
fails = []


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


def diag(p, limit=400):
    """Short inline diagnostic for a failed subprocess."""
    if p.returncode == 0:
        return ''
    err = (p.stderr or p.stdout or '').strip()
    if not err:
        return ' (no diagnostics)'
    if len(err) > limit:
        err = err[:limit] + ' [truncated here, full text below]'
    return ' (%s)' % err


def dump_diag(p, cap=80):
    """Print the FULL diagnostics of a failed subprocess to the stage log."""
    lines = (p.stderr or '').strip().splitlines()
    for ln in lines[:cap]:
        print('| ' + ln)
    if len(lines) > cap:
        print('| ...(%d more lines)' % (len(lines) - cap))


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
#include <windows.h>
#include <stdio.h>
void __stdcall grGlideShutdown(void);
int main(void)
{
    grGlideShutdown();
    printf("returned\n");
    return 0;
}
'''

# Minimal portable Win32 surface for FAKELOGIC (generated into a temp
# dir and compiled together with the REAL fakereal.c). Only what the
# STRICT path touches: env passthrough + ExitProcess that really exits
# (child-process assertions). It deliberately does NOT include the
# Linux behaviour harness (harness_impl.c), which pulls <dlfcn.h> and
# dlsym(RTLD_NEXT) and needs -ldl -- POSIX-only, hence uncompilable
# under MSYS2 MINGW32, which is exactly what broke the PC run.
MINI_C = r'''
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
DWORD GetEnvironmentVariableA(LPCSTR name, char *buf, DWORD cap)
{
    const char *v = getenv(name);
    size_t n;
    if (v == NULL || cap == 0)
        return 0;
    n = strlen(v);
    if (n >= (size_t)cap) {
        memcpy(buf, v, (size_t)cap - 1);
        buf[cap - 1] = '\0';
        return cap;
    }
    memcpy(buf, v, n + 1);
    return (DWORD)n;
}
void ExitProcess(DWORD code)
{
    _Exit((int)code);
}
'''

# Stringizes the target's own __stdcall after the stub was included:
# the stub must NOT shadow it (that is the bug the PC caught).
STUBKEEP_MAIN = r'''
#include <windows.h>
#include <stdio.h>
#define S1(x) #x
#define S(x) S1(x)
int main(void)
{
    printf("%s\n", S(__stdcall));
    return 0;
}
'''

SPELLINGS = ('APIENTRY', '__stdcall', '__declspec')


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
        check(p.returncode == 0,
              'tripscan: scan driver compiles' + diag(p))
        if p.returncode != 0:
            dump_diag(p)
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
    """Compile the REAL fakereal.c + a portable mini-impl, then drive it."""
    with tempfile.TemporaryDirectory() as d:
        mini = os.path.join(d, 'w32mini.c')
        open(mini, 'w').write(MINI_C)
        fmain = os.path.join(d, 'fmain.c')
        open(fmain, 'w').write(FAKE_MAIN)
        objs = []
        for label, csrc, oname in (
                ('real fakereal.c compiles', FAKE_C, 'fakereal.o'),
                ('portable mini-impl compiles', mini, 'w32mini.o'),
                ('driver compiles', fmain, 'fmain.o')):
            obj = os.path.join(d, oname)
            p = subprocess.run([gcc, '-std=c11', '-O1', '-I', BEHAVIOR,
                                '-c', csrc, '-o', obj],
                               capture_output=True, text=True)
            check(p.returncode == 0, 'fakelogic: ' + label + diag(p))
            if p.returncode != 0:
                dump_diag(p)
                return
            objs.append(obj)
        exe = os.path.join(d, 'fmain.exe')
        p = subprocess.run([gcc, '-o', exe] + objs,
                           capture_output=True, text=True)
        check(p.returncode == 0, 'fakelogic: links (no POSIX -ldl)' + diag(p))
        if p.returncode != 0:
            dump_diag(p)
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


def compiler_predefines(gcc, names):
    """Which of `names` the compiler target already defines as macros."""
    p = subprocess.run([gcc, '-dM', '-E', '-x', 'c', '-'],
                       input='', capture_output=True, text=True)
    out = p.stdout
    return {n: re.search(r'^#define\s+%s\b' % re.escape(n), out, re.M)
            is not None for n in names}


def stubtarget(gcc):
    """The test stubs must never clash with spellings the target provides.

    MSYS2 MINGW32 (GCC) predefines __stdcall as __attribute__((__stdcall__))
    -- gcc/config/i386/cygming.h:135 -- and clang additionally predefines
    __declspec. Defining them again in the stub emits -Wmacro-redefined
    (or fails under -Werror) and, worse, silently drops the calling
    convention to cdecl. On Linux the host has none of them, so the same
    mechanism is pinned by emulating the predefines.
    """
    for label, path, extra in (('behaviour stub', BEHAVIOR_STUB, []),
                               ('c-syntax stub', WIN32_STUB, [])):
        text = open(path, encoding='utf-8').read()
        unguarded = []
        for name in SPELLINGS:
            d = re.search(r'^#\s*define\s+%s\b' % re.escape(name),
                          text, re.M)
            if d is None:
                continue
            if not re.search(r'^#\s*ifndef\s+%s\b' % re.escape(name),
                             text[:d.start()], re.M):
                unguarded.append(name)
        msg = 'stubtarget: %s guards every Win32 spelling' % label
        if unguarded:
            msg += ' (unguarded: %s)' % ', '.join(unguarded)
        check(not unguarded, msg)

    pre = compiler_predefines(gcc, ('__stdcall', '__declspec'))
    emul = []
    if not pre['__stdcall']:
        emul.append('-D__stdcall=__attribute__((__cold__))')
    if not pre['__declspec']:
        emul.append('-D__declspec(x)=__attribute__((x))')
    mode = 'real predefines' if not emul else 'emulated predefines'

    with tempfile.TemporaryDirectory() as d:
        # 1) the REAL fakereal.c + stub: zero redefinition diagnostics
        p = subprocess.run([gcc, '-std=c11', '-O1', '-c', '-I', BEHAVIOR]
                           + emul + [FAKE_C, '-o',
                                     os.path.join(d, 'fakereal.o')],
                           capture_output=True, text=True)
        ok = p.returncode == 0 and 'redefined' not in p.stderr
        check(ok, 'stubtarget: no redefinition diagnostics (%s)' % mode
              + diag(p))
        if not ok:
            dump_diag(p)
        # 2) the target's own definition must survive the stub
        c = os.path.join(d, 'keep.c')
        exe = os.path.join(d, 'keep.exe')
        open(c, 'w').write(STUBKEEP_MAIN)
        p = subprocess.run([gcc, '-std=c11', '-O1', '-I', BEHAVIOR]
                           + emul + ['-o', exe, c],
                           capture_output=True, text=True)
        if p.returncode != 0:
            check(False, 'stubtarget: keep-the-target-definition (%s)'
                  % mode + diag(p))
            dump_diag(p)
        else:
            p = subprocess.run([exe], capture_output=True, text=True, cwd=d)
            out = p.stdout.strip()
            check('attribute' in out,
                  'stubtarget: keep-the-target-definition (%s: __stdcall '
                  '-> %s)' % (mode, out or 'EMPTY (downgraded to cdecl)'))


def main():
    ctx = tripscan_static()
    gcc = shutil.which('gcc')
    if gcc is None:
        print('SKIP tripscan behavioural (no gcc)')
        print('SKIP fakelogic (no gcc)')
        print('SKIP stubtarget (no gcc)')
    else:
        if ctx is not None:
            tripscan_run(gcc, ctx)
        fakelogic(gcc)
        stubtarget(gcc)
    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
