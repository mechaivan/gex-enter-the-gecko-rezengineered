#!/bin/bash
# Audit test-suite for glide-shim: static only, no Windows needed.
# 1-3: python checks (.def coverage, SDK arities, init structure).
# 4: C syntax check vs the win32 stub (needs gcc; SKIP if absent).
cd "$(dirname "$0")/.." || exit 1
fail=0
for t in test_def test_api test_init; do
  echo "=== $t ==="
  python3 "tests/$t.py" || fail=1
done
echo "=== c-syntax ==="
if command -v gcc >/dev/null; then
  gcc -std=c11 -Wall -Wextra -fsyntax-only -I tests/win32_stub gshim.c \
    && echo "PASS c-syntax (stub)" || fail=1
else
  echo "SKIP c-syntax (no gcc)"
fi
[ "$fail" -eq 0 ] && echo "SUITE: ALL PASS" || echo "SUITE: FAILURES"
exit "$fail"
