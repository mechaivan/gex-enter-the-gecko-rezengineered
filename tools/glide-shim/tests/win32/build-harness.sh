#!/bin/bash
# Build the native Win32 behaviour harness (MSYS2 MINGW32 shell with
# mingw-w64-i686-gcc; any Windows shell works if that toolchain is in
# PATH — the -dumpmachine gate below is the real requirement).
# Builds, into a temp build dir (default mktemp -d, or $1):
#   glide2x.dll            staged PRODUCTION copy (fresh build-win32.sh;
#                          the repo file is never loaded by the harness)
#   fake_full.dll + 3 missing-one-export variants (staged per scenario
#                          as glide2x_gex_real.dll; NEVER that name here,
#                          or the W07 bare-name fallback would resolve it)
#   w32harness.exe         parent+child runner (harness.c)
# Then COMPILE/EXPORT checks (NOT behaviour): shim copy == 38 expected
# exports via check_exports.py (same V-1 verdict), fake export sets +
# PE32/i386 on all 6 binaries via an inline parse.
# Exit: 0 built + exports PASS; 1 FAIL; 2 BLOCKED (no i686 toolchain).
# Env: W32CC override (single executable, same rule as build-win32.sh CC).
cd "$(dirname "$0")/../.." || exit 2
. tests/env.sh
if ! reason=$(win32_build_ok); then
  echo "W32HARNESS BLOCKED: $reason"
  exit 2
fi
W32CC="${W32CC:-i686-w64-mingw32-gcc}"
B="${1:-$(mktemp -d)}"
mkdir -p "$B" || { echo "W32HARNESS BLOCKED: cannot create $B"; exit 2; }
echo "compiler: $("$W32CC" --version | head -1)"
echo "target: $("$W32CC" -dumpmachine)"
echo "build dir: $B"
BLOG="$B/build.log"
: > "$BLOG"
echo "step 1/3: fresh production DLL (build-win32.sh, shim-only)"
if ! CC="$W32CC" ./build-win32.sh; then
  echo "W32HARNESS FAIL: production build failed (see above)"
  exit 1
fi
echo "step 2/3: compile fakes + runner"
cp glide2x.dll "$B/glide2x.dll" || { echo "W32HARNESS FAIL: cannot stage shim copy"; exit 1; }
if ! "$W32CC" -m32 -O2 -Wall -Wextra -c tests/win32/fakereal.c \
    -o "$B/fakereal.o" 2>&1 | tee -a "$BLOG"; then
  echo "W32HARNESS FAIL: fakereal.c compile error (see $BLOG)"
  exit 1
fi
cp tests/win32/fakereal_full.def "$B/fakereal_full.def"
grep -vF '_grBufferSwap@4' tests/win32/fakereal_full.def > "$B/fakereal_noswap.def"
grep -vF '_grBufferNumPending@0' tests/win32/fakereal_full.def > "$B/fakereal_nopending.def"
grep -vF '_grGlideShutdown@0' tests/win32/fakereal_full.def > "$B/fakereal_noshutdown.def"
for v in full noswap nopending noshutdown; do
  if ! "$W32CC" -m32 -shared -O2 -o "$B/fake_$v.dll" "$B/fakereal.o" \
      "$B/fakereal_$v.def" 2>&1 | tee -a "$BLOG"; then
    echo "W32HARNESS FAIL: fake_$v.dll link error (see $BLOG)"
    exit 1
  fi
done
if ! "$W32CC" -m32 -O2 -Wall -Wextra -o "$B/w32harness.exe" \
    tests/win32/harness.c 2>&1 | tee -a "$BLOG"; then
  echo "W32HARNESS FAIL: harness.c compile error (see $BLOG)"
  exit 1
fi
if [ -e "$B/glide2x_gex_real.dll" ]; then
  echo "W32HARNESS FAIL: build dir must never contain a loadable-named fake (breaks W07)"
  exit 1
fi
echo "PASS w32 build (warnings, if any, in $BLOG — inspect them)"
echo "step 3/3: export checks (compile/export, NOT behaviour)"
PYBIN=$(find_python) || PYBIN=""
if [ -z "$PYBIN" ]; then
  echo "W32HARNESS BLOCKED: no working python for the export verdicts"
  exit 2
fi
if ! "$PYBIN" tests/check_exports.py --shim-only "$B/glide2x.dll" \
    tests/expected_iat.txt; then
  echo "W32HARNESS FAIL: staged shim copy exports (see above)"
  exit 1
fi
if ! "$PYBIN" - "$B" <<'EOF'; then
import sys
sys.path.insert(0, 'tests')
from check_exports import parse_exports, ExportParseError
b = sys.argv[1]
want = {'_grBufferSwap@4', '_grBufferNumPending@0', '_grGlideShutdown@0',
        '_fakereal_counts@16'}
variants = {'fake_full.dll': want,
            'fake_noswap.dll': want - {'_grBufferSwap@4'},
            'fake_nopending.dll': want - {'_grBufferNumPending@0'},
            'fake_noshutdown.dll': want - {'_grGlideShutdown@0'}}
ok = True
for name, expected in sorted(variants.items()):
    try:
        got = set(parse_exports(b + '/' + name)['exports'])
    except ExportParseError as e:
        print('FAIL %s: %s' % (name, e))
        ok = False
        continue
    if got == expected:
        print('PASS %s exports (%d, PE32/i386)' % (name, len(got)))
    else:
        print('FAIL %s: got %s' % (name, sorted(got)))
        ok = False
import struct
try:
    with open(b + '/w32harness.exe', 'rb') as f:
        exe = f.read()
    pe = struct.unpack_from('<I', exe, 0x3C)[0]
    machine = struct.unpack_from('<H', exe, pe + 4)[0]
    magic = struct.unpack_from('<H', exe, pe + 24)[0]
    if machine == 0x014C and magic == 0x010B:
        print('PASS w32harness.exe is PE32/i386')
    else:
        print('FAIL w32harness.exe: machine=%#x magic=%#x' % (machine, magic))
        ok = False
except (OSError, struct.error, IndexError) as e:
    print('FAIL w32harness.exe unreadable: %s' % e)
    ok = False
sys.exit(0 if ok else 1)
EOF
  echo "W32HARNESS FAIL: fake/runner binaries (see above)"
  exit 1
fi
echo "W32HARNESS BUILD: PASS ($B)"
