#!/usr/bin/env python3
"""test_docs_encoding: regression test for the test_docs cp1252 crash.

On the maintainer PC (Windows, cp1252 locale) test_docs.py crashed in
Path.read_text() because README.md is UTF-8 and contains byte 0x81
(undefined in cp1252). This test guards the fix hermetically:
  1. encoding contract: README.md + gshim.c decode as strict UTF-8;
  2. bug precondition: README.md does NOT decode as strict cp1252, i.e. a
     bare read_text() under a cp1252 default would crash (SKIP if the
     README ever becomes cp1252-clean — then the premise is gone);
  3. regression: test_docs.main() exits 0 with implicit-locale decoding
     disabled (Path.read_text patched to refuse encoding=None, which is
     what a cp1252 default does to these files).
Exit 0 = all pass.
"""
import contextlib
import importlib.util
import io
from pathlib import Path

SHIM = Path(__file__).resolve().parent.parent
fails = []
skips = []


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


def main():
    readme_bytes = (SHIM / 'README.md').read_bytes()
    code_bytes = (SHIM / 'gshim.c').read_bytes()

    # 1. contract: both files read by test_docs are strict UTF-8
    try:
        readme_bytes.decode('utf-8')
        code_bytes.decode('utf-8')
        check(True, 'README.md + gshim.c decode as strict UTF-8')
    except UnicodeDecodeError as e:
        check(False, f'strict UTF-8 contract broken ({e})')

    # 2. bug precondition: README.md is not cp1252-decodable
    try:
        readme_bytes.decode('cp1252')
        print('SKIP README.md happens to decode as cp1252 (premise gone)')
        skips.append('cp1252-premise')
    except UnicodeDecodeError as e:
        check(True, f'bug precondition holds: cp1252 strict fails ({e})')

    # 3. regression: test_docs must not rely on implicit locale decoding
    spec = importlib.util.spec_from_file_location(
        'test_docs_under_test', SHIM / 'tests' / 'test_docs.py')
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    real_read_text = Path.read_text

    def no_implicit(self, *args, **kwargs):
        enc = kwargs.get('encoding', args[0] if args else None)
        if enc is None:
            raise UnicodeDecodeError(
                'cp1252-sim', b'\x81', 0, 1,
                'implicit locale decoding disabled (cp1252 default sim)')
        return real_read_text(self, *args, **kwargs)

    Path.read_text = no_implicit
    try:
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = mod.main()
    except UnicodeDecodeError as e:
        check(False, f'test_docs relies on implicit decoding ({e})')
        rc = None
    finally:
        Path.read_text = real_read_text
    if rc is not None:
        if rc != 0:
            print(buf.getvalue(), end='')
        check(rc == 0,
              f'test_docs exits 0 under simulated cp1252 default (rc={rc})')

    print(f'{len(fails)} failures' if fails else
          f'ALL PASS ({len(skips)} skipped)' if skips else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
