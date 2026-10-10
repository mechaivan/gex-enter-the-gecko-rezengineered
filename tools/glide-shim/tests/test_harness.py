#!/usr/bin/env python3
"""test_harness: regression pins for the Win32 harness (paths + tools).

Bug 1 (V-2 PC 2026-10-11, all 14 scenarios): child_load failed with
"shim module is not the staged copy" because our staged path carried a
double backslash (GetTempPathA's trailing separator + join_path adding
another) while GetModuleFileNameA returns the normalized spelling.
Bug 2 (same run): run-harness.sh integrity checks shelled out to cmp,
which MSYS2 MINGW32 does not ship.
Checks:
  A. BEHAVIOURAL (needs gcc; loud SKIP otherwise): join_path is
     extracted VERBATIM from tests/win32/harness.c, compiled, and run:
     no doubled separator (the GetTempPathA shape), plain join,
     slash-terminated base, overlong refused, exact fit accepted.
     This executes the shipped code, not a copy.
  B. STRUCTURAL (stdlib only): harness.c defines canon_path using
     GetFullPathNameA + GetLongPathNameA; both staged-copy checks go
     through it (the raw _stricmp(got, c->shim_path/fake_path) lines
     are gone); mismatch messages print both spellings; and
     run-harness.sh invokes no cmp (files_identical instead).
Exit 0 = all pass (or behavioural SKIP without gcc).
"""
import os
import shutil
import subprocess
import sys
import tempfile

TESTDIR = os.path.dirname(os.path.abspath(__file__))
HARNESS_C = os.path.join(TESTDIR, 'win32', 'harness.c')
RUNHARNESS = os.path.join(TESTDIR, 'win32', 'run-harness.sh')
fails = []


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


JOIN_MAIN = '''
#include <stdio.h>
#include <string.h>
%s
int main(void) {
    char b[64];
    int fails = 0;
#define CASE(cond, label) do { \\
        if (cond) { printf("PASS join: %%s\\n", label); } \\
        else { printf("FAIL join: %%s (got %%s)\\n", label, b); fails++; } \\
    } while (0)
    CASE(join_path(b, sizeof(b), "C:\\\\Temp\\\\", "x") &&
         strcmp(b, "C:\\\\Temp\\\\x") == 0, "trailing backslash not doubled");
    CASE(join_path(b, sizeof(b), "C:\\\\Temp", "x") &&
         strcmp(b, "C:\\\\Temp\\\\x") == 0, "plain join");
    CASE(join_path(b, sizeof(b), "C:/Temp/", "x") &&
         strcmp(b, "C:/Temp/x") == 0, "trailing slash not doubled");
    CASE(!join_path(b, 12, "C:\\\\Temp12", "xy"), "overlong refused");
    CASE(join_path(b, 13, "C:\\\\Temp12", "xy") &&
         strcmp(b, "C:\\\\Temp12\\\\xy") == 0, "exact fit accepted");
    return fails ? 1 : 0;
}
'''


def extract_join(src):
    start = src.find('static int join_path')
    if start < 0:
        return None
    end = src.find('\n}\n', start)
    if end < 0:
        return None
    return src[start:end + 3]


def behavioural(src):
    gcc = shutil.which('gcc')
    if gcc is None:
        print('SKIP join behavioural (no gcc)')
        return
    fn = extract_join(src)
    check(fn is not None, 'join_path extracted verbatim from harness.c')
    if fn is None:
        return
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, 'j.c')
        exe = os.path.join(d, 'j')
        open(c, 'w').write(JOIN_MAIN % fn)
        p = subprocess.run([gcc, '-std=c11', '-O2', '-o', exe, c],
                           capture_output=True, text=True)
        check(p.returncode == 0, 'extracted join_path compiles'
              + ('' if p.returncode == 0
                 else ' (%s)' % p.stderr.strip()[:200]))
        if p.returncode != 0:
            return
        p = subprocess.run([exe], capture_output=True, text=True)
        seen = 0
        for line in p.stdout.split('\n'):
            if line.startswith('PASS join: ') \
                    or line.startswith('FAIL join: '):
                seen += 1
                check(line.startswith('PASS'), line[5:])
        if p.returncode != 0 and seen == 0:
            check(False, 'join runner rc=%d with no verdicts'
                  % p.returncode)


def structural(src):
    check('static int canon_path(char *dst' in src,
          'canon_path helper defined')
    check('GetFullPathNameA' in src and 'GetLongPathNameA' in src,
          'canon uses GetFullPathNameA + GetLongPathNameA')
    check('_stricmp(got, c->shim_path)' not in src,
          'raw shim compare gone (was the V-2 failure line)')
    check('_stricmp(got, c->fake_path)' not in src,
          'raw fake compare gone')
    check(src.count('canon_path(cg,') >= 2,
          'both checks canonicalize the loader spelling')
    check(src.count('canon_path(cw,') >= 2,
          'both checks canonicalize the staged spelling')
    check('shim module is not the staged copy (got=%s want=%s)' in src,
          'shim mismatch prints both spellings')
    check('not the staged fake (got=%s want=%s)' in src,
          'fake mismatch prints both spellings')
    rhs = open(RUNHARNESS, encoding='utf-8').read()
    stripped = '\n'.join(l.split('#')[0] for l in rhs.split('\n'))
    words = stripped.replace(';', ' ').replace('|', ' ').replace(
        '&', ' ').split()
    check('cmp' not in words,
          'run-harness.sh invokes no cmp (absent on MSYS2)')
    check('files_identical' in rhs,
          'run-harness.sh uses files_identical')


def main():
    src = open(HARNESS_C, encoding='utf-8').read()
    structural(src)
    behavioural(src)
    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
