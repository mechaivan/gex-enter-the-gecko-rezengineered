#!/bin/bash
# Audit test-suite for glide-shim: static + behavioural + native win32 stage (loud SKIP when not on Windows).
# 1-6: python checks (.def coverage, SDK arities, init/exit structure,
# docs honesty tripwires, export-checker self-test vs PE fixtures,
# environment-detection self-test). Python resolved portably
# (python3/python/py, verified to run: the MS Store alias is skipped).
# 7: C syntax check vs the win32 stub (needs gcc; SKIP if absent or if
# the shim's own 32-bit guard trips — e.g. a 64-bit Windows gcc, which
# is an environment mismatch, not a regression).
# 8: behavioural suite — the REAL gshim.c compiled against functional
#    Win32 fakes; asserts files/counts/order/exit codes. LINUX HOST
#    STAGE ONLY (needs native Linux gcc); skipped elsewhere, and NEVER
#    a substitute for the Win32 build (build-win32.sh: V-1/V-1b).
# 9: native Win32 harness (tests/win32) — the production DLL loaded for
#    real in temp dirs against a fake real DLL; 14 behaviour scenarios.
#    Needs an i686 toolchain to build (else SKIP); needs Windows to run
#    (else compile-only: build + export checks PASS, behaviour SKIP).
cd "$(dirname "$0")/.." || exit 1
. tests/env.sh
echo "stages: python + c-syntax + behaviour (Linux host) + win32-harness (needs i686/Windows); Win32 build validation = build-win32.sh (V-1/V-1b)"
fail=0
PYBIN=$(find_python) || PYBIN=""
if [ -z "$PYBIN" ]; then
  echo "=== python stages ==="
  echo "BLOCKED: no working python (tried python3/python/py >= 3.8; the Microsoft Store alias does not count)"
  fail=1
else
  for t in test_def test_api test_init test_docs test_docs_encoding test_exports test_env test_harness; do
    echo "=== $t ==="
    "$PYBIN" "tests/$t.py" || fail=1
  done
fi
echo "=== c-syntax ==="
if command -v gcc >/dev/null; then
  CSLOG=$(mktemp)
  if gcc -std=c11 -Wall -Wextra -fsyntax-only -I tests/win32_stub gshim.c >"$CSLOG" 2>&1; then
    echo "PASS c-syntax (stub)"
  elif [ "$(classify_csyntax_fail "$CSLOG")" = "SKIP" ]; then
    echo "SKIP c-syntax (shim 32-bit guard tripped: host gcc is 64-bit Windows; Win32 validation is build-win32.sh, not this suite)"
  else
    echo "FAIL c-syntax (see log)"; cat "$CSLOG"; fail=1
  fi
  rm -f "$CSLOG"
else
  echo "SKIP c-syntax (no gcc)"
fi
echo "=== behaviour ==="
if reason=$(host_run_ok); then
  ./tests/behavior/run_behavior.sh || fail=1
else
  echo "SKIP behaviour ($reason)"
fi
echo "=== win32-harness ==="
./tests/win32/run-harness.sh || fail=1
[ "$fail" -eq 0 ] && echo "SUITE: ALL PASS" || echo "SUITE: FAILURES"
exit "$fail"
