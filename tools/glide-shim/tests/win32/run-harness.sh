#!/bin/bash
# Run the native Win32 behaviour harness (entry point for run_tests.sh).
# Modes, never a false PASS:
#   full ......... Windows + i686 toolchain: build + export checks + run
#                  the 14 behaviour scenarios (W00-W11).
#   compile-only . i686 toolchain but not Windows: build + export checks
#                  PASS, behaviour SKIP (loud).
#   skip ......... no i686 toolchain: whole stage SKIP (loud), exit 0.
# Integrity: the repo glide2x.dll is hash-compared (cmp vs a pristine
# copy) after the run, and `git status` must be unchanged — the harness
# loads only temp copies and writes only temp dirs.
# Exit: 0 all-PASS (or SKIP/compile-only); 1 any FAIL.
cd "$(dirname "$0")/../.." || exit 1
. tests/env.sh
if ! reason=$(win32_build_ok); then
  echo "SKIP win32-harness ($reason)"
  exit 0
fi
RUNDIR=$(mktemp -d) || { echo "FAIL win32-harness (no temp dir)"; exit 1; }
echo "win32-harness run dir: $RUNDIR"
HAVE_GIT=0
if command -v git >/dev/null 2>&1; then
  HAVE_GIT=1
  git status --short > "$RUNDIR/git_before.txt" 2>/dev/null || HAVE_GIT=0
fi
[ "$HAVE_GIT" -eq 0 ] && echo "(note: git absent — repo-pollution check skipped)"
if ! ./tests/win32/build-harness.sh "$RUNDIR/build"; then
  echo "FAIL win32-harness (build or export checks failed; see above)"
  exit 1
fi
if ! reason=$(win32_run_ok); then
  echo "W32HARNESS BEHAVIOUR: SKIP (built + exports PASS; $reason)"
  exit 0
fi
cp glide2x.dll "$RUNDIR/shim_pristine.dll" \
  || { echo "FAIL win32-harness (cannot snapshot repo DLL)"; exit 1; }
"$RUNDIR/build/w32harness.exe"
rc=$?
fail=0
if ! cmp -s glide2x.dll "$RUNDIR/shim_pristine.dll"; then
  echo "FAIL win32-harness: repo glide2x.dll changed during the run"
  fail=1
else
  echo "PASS repo glide2x.dll byte-identical after run"
fi
if [ "$HAVE_GIT" -eq 1 ]; then
  if ! git status --short | cmp -s - "$RUNDIR/git_before.txt"; then
    echo "FAIL win32-harness: repo tree changed during the run (see git status)"
    fail=1
  else
    echo "PASS repo tree unchanged after run"
  fi
fi
if [ "$rc" -ne 0 ]; then
  echo "FAIL win32-harness (scenario failures above)"
  exit 1
fi
[ "$fail" -eq 0 ] && echo "W32HARNESS: ALL PASS" || exit 1
