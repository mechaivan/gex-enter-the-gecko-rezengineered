#!/bin/bash
# V-1 reproducible Win32 build for glide-shim.
# Run on the maintainer PC inside an "MSYS2 MINGW32" shell
# (needs mingw-w64-i686-gcc + python3 for the export verdicts;
# 64-bit shells are refused on purpose).
# Usage: ./build-win32.sh [path/to/verified-real-glide2x-copy.dll]
#   (CC env override: single executable path/name only, no argument strings.)
#   (OUTDIR env override: build outputs (glide2x.dll, exports_*.txt) go
#    there instead of this dir; the win32 harness sets it to its temp
#    build dir so the repo tree is never written. Default "." keeps the
#    documented V-1 layout.)
# objdump is auto-detected beside CC (triplet preferred, plain accepted;
# never presumed) via tests/env.sh find_objdump, and the chosen path is
# echoed for the evidence record.
# Exit: 0 = built + shim exports byte-verified (+ V-1b verdict if real given),
#       1 = FAIL, 2 = BLOCKED (wrong environment / missing tool).
cd "$(dirname "$0")" || exit 2
. tests/env.sh
CC="${CC:-i686-w64-mingw32-gcc}"
OUTDIR="${OUTDIR:-.}"
if ! command -v "$CC" >/dev/null; then
  echo "V-1 BLOCKED: no $CC (MSYS2 MINGW32 shell: pacman -S mingw-w64-i686-gcc)"
  exit 2
fi
echo "compiler: $("$CC" --version | head -1)"
MACH=$("$CC" -dumpmachine)
echo "target: $MACH"
if [ "$MACH" != "i686-w64-mingw32" ]; then
  echo "V-1 BLOCKED: need an i686-w64-mingw32 toolchain, got $MACH"
  echo "(open MSYS2 MINGW32, not MINGW64/UCRT64; MinGW-w64 is not multilib)"
  exit 2
fi
mkdir -p "$OUTDIR" || { echo "V-1 BLOCKED: cannot create OUTDIR=$OUTDIR"; exit 2; }
DLL="$OUTDIR/glide2x.dll"
echo "outdir: $OUTDIR"
echo "build: $CC -m32 -shared -O2 -o $DLL gshim.c gshim.def"
if ! "$CC" -m32 -shared -O2 -o "$DLL" gshim.c gshim.def; then
  echo "V-1 FAIL: build error (see above)"
  exit 1
fi
echo "V-1 BUILD: PASS"
if ! sha256sum "$DLL"; then
  echo "(sha256sum unavailable or failed — record the DLL hash manually)"
fi
"$CC" -m32 -shared -O2 -Wall -Wextra -fsyntax-only gshim.c 2>&1 | head -20
OBJDUMP=$(find_objdump "$CC") || OBJDUMP=""
if [ -z "$OBJDUMP" ]; then
  echo "V-1 BLOCKED: no working objdump for $CC (tried <cc-dir>/{i686-w64-mingw32-objdump,objdump} + PATH objdump; MSYS2 MINGW32: pacman -S mingw-w64-i686-binutils)"
  exit 2
fi
echo "objdump: $OBJDUMP"
if ! "$OBJDUMP" -p "$DLL" > "$OUTDIR/exports_shim.txt"; then
  echo "V-1 BLOCKED: objdump failed ($OBJDUMP)"
  exit 2
fi
echo "wrote $OUTDIR/exports_shim.txt (human evidence; keep it)"
if ! command -v python3 >/dev/null; then
  echo "V-1 BLOCKED: python3 missing (MSYS2 MINGW32: pacman -S mingw-w64-i686-python)"
  exit 2
fi
if ! python3 tests/check_exports.py --shim-only "$DLL" tests/expected_iat.txt; then
  echo "V-1 FAIL: shim exports (see above)"
  exit 1
fi
echo "V-1: PASS (build + 38 shim exports byte-verified)"
if [ -n "${1:-}" ]; then
  if [ ! -f "$1" ]; then echo "V-1b BLOCKED: real DLL not found: $1"; exit 2; fi
  if ! "$OBJDUMP" -p "$1" > "$OUTDIR/exports_real.txt"; then
    echo "V-1b BLOCKED: objdump failed on the real DLL"
    exit 2
  fi
  echo "wrote $OUTDIR/exports_real.txt (human evidence; keep it)"
  python3 tests/check_exports.py "$DLL" "$1" tests/expected_iat.txt \
    || { echo "V-1b verdict: FAIL (see above)"; exit 1; }
  echo "V-1b verdict: PASS"
else
  echo "V-1b PENDING (not passed): re-run with the V-0-verified real DLL copy as argument"
fi
