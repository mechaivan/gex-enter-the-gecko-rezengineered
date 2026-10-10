#!/bin/bash
# Run the native Win32 behaviour harness (entry point for run_tests.sh).
# Modes, never a false PASS:
#   full ......... Windows + i686 toolchain: build + export checks + run
#                  the 14 behaviour scenarios (W00-W11).
#   compile-only . i686 toolchain but not Windows: build + export checks
#                  PASS, behaviour SKIP (loud).
#   skip ......... no i686 toolchain: whole stage SKIP (loud), exit 0.
# Integrity: everything builds and runs in temp dirs (build-win32.sh
# OUTDIR + per-scenario dirs under %TEMP%); after the run the repo-side
# build outputs (glide2x.dll, exports_shim.txt) must be byte-identical
# to before — or still absent — AND `git status` must be unchanged.
# Any repo write = FAIL, never a PASS.
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
for p in glide2x.dll exports_shim.txt; do
  if [ -e "$p" ]; then
    cp "$p" "$RUNDIR/repo_before_$(basename "$p")" \
      || { echo "FAIL win32-harness (cannot snapshot repo $p)"; exit 1; }
    echo "repo $p: present before run (snapshot kept)"
  else
    echo "repo $p: absent before run (must stay absent)"
  fi
done
check_repo_intact() {
  _fail=0
  for p in glide2x.dll exports_shim.txt; do
    snap="$RUNDIR/repo_before_$(basename "$p")"
    if [ -e "$snap" ]; then
      if ! cmp -s "$p" "$snap"; then
        echo "FAIL win32-harness: repo $p changed during the run"
        _fail=1
      else
        echo "PASS repo $p byte-identical after run"
      fi
    elif [ -e "$p" ]; then
      echo "FAIL win32-harness: repo $p appeared during the run (builds must stay in temp)"
      _fail=1
    else
      echo "PASS repo $p still absent after run"
    fi
  done
  if [ "$HAVE_GIT" -eq 1 ]; then
    if ! git status --short | cmp -s - "$RUNDIR/git_before.txt"; then
      echo "FAIL win32-harness: repo tree changed during the run (see git status)"
      _fail=1
    else
      echo "PASS repo tree unchanged after run"
    fi
  fi
  return "$_fail"
}
if ! ./tests/win32/build-harness.sh "$RUNDIR/build"; then
  echo "FAIL win32-harness (build or export checks failed; see above)"
  exit 1
fi
if ! reason=$(win32_run_ok); then
  echo "W32HARNESS BEHAVIOUR: SKIP (built + exports PASS; $reason)"
  check_repo_intact || exit 1
  exit 0
fi
"$RUNDIR/build/w32harness.exe"
rc=$?
fail=0
check_repo_intact || fail=1
if [ "$rc" -ne 0 ]; then
  echo "FAIL win32-harness (scenario failures above)"
  exit 1
fi
[ "$fail" -eq 0 ] && echo "W32HARNESS: ALL PASS" || exit 1
