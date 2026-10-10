#!/usr/bin/env python3
"""Structural self-test for tests/env.sh (environment detection).

Hermetic pins (fake PATH entries, no real toolchain needed): python
fallback past a failing `python3` (Microsoft Store alias shape), `py`
launcher support, empty result when nothing runs, Linux/native-gcc
gating for host-run stages, and c-syntax failure classification
(32-bit guard trip = environment SKIP, anything else = FAIL), plus the
win32-harness gates (i686 toolchain builds anywhere, behaviour runs on
Windows only — compile-only elsewhere, never a false PASS).
Exit 0 = all pass."""
import os
import shutil
import subprocess
import sys
import tempfile

ENVSH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'env.sh')
BASH = shutil.which('bash')
fails = []


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


def sh(code, dirs):
    env = dict(os.environ)
    env['PATH'] = os.pathsep.join(dirs)
    p = subprocess.run([BASH, '-c', '. "$1"; ' + code, 'sh', ENVSH],
                       capture_output=True, text=True, env=env)
    return p.returncode, p.stdout.strip(), p.stderr.strip()


def stub(path, body):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('#!/bin/sh\n' + body + '\n')
    os.chmod(path, 0o755)


def main():
    if BASH is None:
        check(False, 'bash available (run via run_tests.sh)')
        return 1
    realpath = os.environ['PATH'].split(os.pathsep)
    rc, out, _ = sh('find_python', realpath)
    check(rc == 0 and out in ('python3', 'python', 'py'),
          'live environment resolves a working python')
    with tempfile.TemporaryDirectory() as d:
        stub(os.path.join(d, 'python3'), 'echo "store alias" >&2; exit 9009')
        stub(os.path.join(d, 'python'), 'exit 0')
        rc, out, _ = sh('find_python', [d])
        check(rc == 0 and out == 'python',
              'failing python3 falls back to python')
    with tempfile.TemporaryDirectory() as d:
        stub(os.path.join(d, 'py'), 'exit 0')
        rc, out, _ = sh('find_python', [d])
        check(rc == 0 and out == 'py', '`py` launcher is accepted')
    with tempfile.TemporaryDirectory() as d:
        rc, out, _ = sh('find_python', [d])
        check(rc != 0 and out == '', 'no usable python yields empty')
    with tempfile.TemporaryDirectory() as d:
        stub(os.path.join(d, 'uname'), 'echo Linux')
        stub(os.path.join(d, 'gcc'),
             'if [ "$1" = "-dumpmachine" ]; then echo x86_64-linux-gnu; fi; exit 0')
        rc, out, _ = sh('host_run_ok', [d])
        check(rc == 0 and out == '', 'linux native gcc is accepted')
        stub(os.path.join(d, 'gcc'),
             'if [ "$1" = "-dumpmachine" ]; then echo x86_64-w64-mingw32; fi; exit 0')
        rc, out, _ = sh('host_run_ok', [d])
        check(rc != 0 and 'build-win32.sh' in out,
              'mingw64 gcc is rejected pointing at build-win32.sh')
    with tempfile.TemporaryDirectory() as d:
        stub(os.path.join(d, 'uname'), 'echo MINGW64_NT-10.0-19045')
        rc, out, _ = sh('host_run_ok', [d])
        check(rc != 0 and 'not Linux' in out,
              'non-linux host is rejected for run stages')
    with tempfile.TemporaryDirectory() as d:
        g = os.path.join(d, 'guard.log')
        open(g, 'w').write('gshim.c:95:2: error: #error "gshim must be built 32-bit (e.g. x)"\n')
        rc, out, _ = sh('classify_csyntax_fail "%s"' % g, realpath)
        ok_skip = (rc == 0 and out == 'SKIP')
        open(g, 'w').write("gshim.c:10:5: error: use of undeclared identifier 'foo'\n")
        rc, out, _ = sh('classify_csyntax_fail "%s"' % g, realpath)
        check(ok_skip and rc == 0 and out == 'FAIL',
              'guard trip classifies SKIP, other errors FAIL')
    with tempfile.TemporaryDirectory() as d:
        stub(os.path.join(d, 'uname'), 'echo Linux')
        stub(os.path.join(d, 'i686-w64-mingw32-gcc'),
             'if [ "$1" = "-dumpmachine" ]; then echo i686-w64-mingw32; fi; exit 0')
        rc, out, _ = sh('win32_build_ok', [d])
        check(rc == 0 and out == '',
              'i686 toolchain is accepted for the w32 build')
        rc, out, _ = sh('win32_run_ok', [d])
        check(rc != 0 and 'Windows' in out,
              'linux host cannot run w32 behaviour (compile-only)')
    with tempfile.TemporaryDirectory() as d:
        stub(os.path.join(d, 'uname'), 'echo MINGW32_NT-10.0-19045')
        stub(os.path.join(d, 'i686-w64-mingw32-gcc'),
             'if [ "$1" = "-dumpmachine" ]; then echo i686-w64-mingw32; fi; exit 0')
        rc, out, _ = sh('win32_build_ok && win32_run_ok', [d])
        check(rc == 0 and out == '',
              'mingw32 + i686 runs the full w32 harness')
    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
