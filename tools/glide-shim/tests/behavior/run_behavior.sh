#!/bin/bash
# Behavioural suite: compiles the REAL gshim.c against functional Win32
# fakes (tests/behavior) and asserts intended BEHAVIOUR (files, counts,
# order, exit codes) — not text patterns. Needs gcc (native Linux is
# fine: gshim.c is platform-C + Win32 API surface). SKIP (loud) if no gcc.
# Scenarios: B1a/b logging exactness, B2 overflow, B3 NOLOG, B4 marker
# failure, B5/B7 fail-fast, B6 crash bound, B8 double shutdown,
# B10 log-blocked degrade, B11 DETACH no-op, B12 resolve fast path.
cd "$(dirname "$0")/../.." || exit 1
fails=0
pass=0
ok() { # ok <cond-exit> <label>
  if [ "$1" -eq 0 ]; then pass=$((pass+1)); echo "PASS $2";
  else fails=$((fails+1)); echo "FAIL $2"; fi
}
if ! command -v gcc >/dev/null; then echo "SKIP behaviour (no gcc)"; exit 0; fi
BIN=$(mktemp -d)/behavior_test
gcc -std=c11 -O1 -I tests/behavior tests/behavior/driver.c \
  tests/behavior/harness_impl.c gshim.c -ldl -o "$BIN" 2>"$(mktemp -d)/build.log" \
  || { echo "FAIL behaviour build"; exit 1; }
echo "PASS behaviour build"
TRACER_DIR=$(mktemp -d); TRACER="$TRACER_DIR/syscall_tracer"
TLOG=$(mktemp); gcc -Wall -Wextra -o "$TRACER" tests/behavior/syscall_tracer.c >"$TLOG" 2>&1 && [ -d /proc/self/fd ] && [ "$(uname -m)" = "x86_64" ] && echo "PASS tracer build" || { echo "SKIP B13 (needs Linux x86_64 + /proc + ptrace)"; cat "$TLOG"; TRACER=""; }
T() { mktemp -d; }

# B1a: exact logging, constant pending (1 change + 1 periodic@4096)
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 GSHIM_TEST_STRICT=1 \
  GSHIM_TEST_PENDING=const:7 "$BIN" 3000 5000 1 0 0 >/dev/null 2>&1 )
[ -f "$d/gshim_log.csv" ]; ok $? "B1a csv exists"
[ "$(grep -c '^[0-9]' "$d/gshim_log.csv")" = "3003" ]; ok $? "B1a 3003 data rows"
grep -q '^1,[0-9]*,S,3$' "$d/gshim_log.csv"; ok $? "B1a first row S/arg3"
[ "$(grep -c ',P,7$' "$d/gshim_log.csv")" = "2" ]; ok $? "B1a 2 P rows (change+1/4096)"
[ "$(grep -c ',X,0$' "$d/gshim_log.csv")" = "1" ]; ok $? "B1a 1 X row"
grep '^[0-9]' "$d/gshim_log.csv" | awk -F, '{if ($1 != NR) bad=1} END{exit bad}'; ok $? "B1a seq contiguous"
grep '^[0-9]' "$d/gshim_log.csv" | awk -F, '{if ($2 <= p) bad=1; p=$2} END{exit bad}'; ok $? "B1a ticks monotonic"
tail -1 "$d/gshim_log.csv" | grep -q '# end rows=3003 overflow=0 swaps=3000 pending_calls=5000 .* by=shutdown'; ok $? "B1a footer exact"
head -1 "$d/gshim_log.csv" | grep -q 'buf=2048'; ok $? "B1a header buf=2048"
tail -1 "$d/gshim_log.csv" | grep -q 'log_cost_us_sum=.* max='; ok $? "B1a footer cost fields"
grep -q '^swap=3000 pending=5000 shutdown=1 last_arg=3$' "$d/forwarded.counts"; ok $? "B1a forwarded counts"
grep -qF 'C:\game\glide2x_gex_real.dll' "$d/load_arg.txt"; ok $? "B1a full-path LoadLibrary"
[ ! -e "$d/gshim_error.txt" ]; ok $? "B1a no error file"

# B1b: alternating pending (every call changes -> 5000 P rows)
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 GSHIM_TEST_STRICT=1 \
  GSHIM_TEST_PENDING=alt "$BIN" 3000 5000 1 0 0 >/dev/null 2>&1 )
[ "$(grep -c '^[0-9]' "$d/gshim_log.csv")" = "8001" ]; ok $? "B1b 8001 data rows"
tail -1 "$d/gshim_log.csv" | grep -q '# end rows=8001 overflow=0 .* by=shutdown'; ok $? "B1b footer exact"

# B2: overflow (300k swaps; ring 262144, stop-on-full + trip marker)
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 GSHIM_TEST_STRICT=1 \
  "$BIN" 300000 0 1 0 0 >/dev/null 2>&1 ); ok $? "B2 exit 0"
[ "$(grep -c '^[0-9]' "$d/gshim_log.csv")" = "262144" ]; ok $? "B2 rows capped at 262144"
[ "$(grep -c '^# overflow at seq=262144: RUN INVALID' "$d/gshim_log.csv")" = "1" ]; ok $? "B2 trip marker once"
[ "$(grep -c ',X,' "$d/gshim_log.csv")" = "0" ]; ok $? "B2 X row dropped (full)"
tail -1 "$d/gshim_log.csv" | grep -q '# end rows=262144 overflow=1 swaps=300000 .* by=shutdown'; ok $? "B2 footer overflow=1"
grep -q 'ring overflow' "$d/gshim_error.txt"; ok $? "B2 error notes overflow"

# B3: NOLOG (log untouched, marker init+end, forwarding intact)
d=$(T); ( cd "$d" && GSHIM_NOLOG=1 GSHIM_TEST_NULL_MODULE=1 GSHIM_TEST_STRICT=1 \
  GSHIM_TEST_PENDING=const:2 "$BIN" 1000 2000 1 0 0 >/dev/null 2>&1 ); ok $? "B3 exit 0"
[ ! -e "$d/gshim_log.csv" ]; ok $? "B3 csv absent"
[ "$(wc -l < "$d/gshim_nolog.marker")" = "2" ]; ok $? "B3 marker 2 lines"
head -1 "$d/gshim_nolog.marker" | grep -q 'nolog=1 qpf='; ok $? "B3 marker init line"
tail -1 "$d/gshim_nolog.marker" | grep -q 'end swaps=1000 pending_calls=2000 by=shutdown'; ok $? "B3 marker end counts"
grep -q '^swap=1000 pending=2000 shutdown=1 ' "$d/forwarded.counts"; ok $? "B3 forwarded in NOLOG"
[ ! -e "$d/gshim_error.txt" ]; ok $? "B3 no error file"

# B4: marker unwritable (dir blocks fopen) -> loud, forwarding intact
d=$(T); mkdir "$d/gshim_nolog.marker"
( cd "$d" && GSHIM_NOLOG=1 GSHIM_TEST_NULL_MODULE=1 "$BIN" 100 50 1 0 0 >/dev/null 2>&1 ); ok $? "B4 exit 0 (forwarding intact)"
grep -q '^swap=100 pending=50 shutdown=1 ' "$d/forwarded.counts"; ok $? "B4 forwarded despite marker fail"
grep -q 'marker init failed' "$d/gshim_error.txt"; ok $? "B4 error: init failed"
grep -q 'marker end failed' "$d/gshim_error.txt"; ok $? "B4 error: end failed"
[ ! -e "$d/gshim_log.csv" ]; ok $? "B4 csv still absent"

# B5: resolution failures -> exit 111 + named FATAL (x3 symbols + load fail)
for sym in _grBufferSwap@4 _grBufferNumPending@0 _grGlideShutdown@0; do
  d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 GSHIM_TEST_NULL_PROC="$sym" \
    "$BIN" 10 10 1 0 0 >/dev/null 2>&1 ); [ $? -eq 111 ]; ok $? "B5 exit 111 ($sym)"
  grep -q "FATAL (exit 111).*cannot resolve $sym" "$d/gshim_error.txt"; ok $? "B5 names $sym"
done
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 GSHIM_TEST_LOAD_FAIL=1 \
  "$BIN" 10 10 1 0 0 >/dev/null 2>&1 ); [ $? -eq 111 ]; ok $? "B5 exit 111 (load fail)"

# B6: crash mid-run (no shutdown) -> partial rows, no footer
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 "$BIN" 5000 0 0 1 0 >/dev/null 2>&1 ); [ $? -eq 42 ]; ok $? "B6 exit 42"
[ "$(grep -c '^[0-9]' "$d/gshim_log.csv")" -ge 4700 ]; ok $? "B6 >=4700/5000 rows on disk (2KB buf: worst loss ~233)"
grep -q '^# gshim ' "$d/gshim_log.csv"; ok $? "B6 header present"
head -1 "$d/gshim_log.csv" | grep -q 'buf=2048'; ok $? "B6 header buf=2048"
! grep -q '^# end' "$d/gshim_log.csv"; ok $? "B6 no footer"

# B7: no QPC -> exit 111
d=$(T); ( cd "$d" && GSHIM_TEST_NO_QPC=1 "$BIN" 10 10 1 0 0 >/dev/null 2>&1 ); [ $? -eq 111 ]; ok $? "B7 exit 111"
grep -q 'FATAL (exit 111).*no QPC' "$d/gshim_error.txt"; ok $? "B7 names no QPC"

# B8: double shutdown -> single footer, single X
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 "$BIN" 100 0 2 0 0 >/dev/null 2>&1 ); ok $? "B8 exit 0"
[ "$(grep -c '^# end' "$d/gshim_log.csv")" = "1" ]; ok $? "B8 one footer"
[ "$(grep -c ',X,' "$d/gshim_log.csv")" = "1" ]; ok $? "B8 one X row"

# B10: log blocked (dir) -> degrade loudly, forwarding intact
d=$(T); mkdir "$d/gshim_log.csv"
( cd "$d" && GSHIM_TEST_NULL_MODULE=1 "$BIN" 200 0 1 0 0 >/dev/null 2>&1 ); ok $? "B10 exit 0"
grep -q '^swap=200 pending=0 shutdown=1 ' "$d/forwarded.counts"; ok $? "B10 forwarded despite log block"
grep -q 'cannot open log' "$d/gshim_error.txt"; ok $? "B10 error: cannot open"
grep -q 'rows lost' "$d/gshim_error.txt"; ok $? "B10 error: rows lost"

# B11: DETACH is a strict no-op on our files
d=$(T); out=$( cd "$d" && GSHIM_TEST_NULL_MODULE=1 "$BIN" 500 0 1 0 1 2>/dev/null ); ok $? "B11 exit 0"
echo "$out" | grep -qE 'csv=[0-9]+/[0-9]+ marker=-1/-1' && \
  [ "$(echo "$out" | sed -E 's/.*csv=([0-9]+)\/([0-9]+).*/\1 \2/')" != "" ] && \
  [ "$(echo "$out" | sed -E 's/.*csv=([0-9]+)\/([0-9]+).*/\1/')" = "$(echo "$out" | sed -E 's/.*csv=([0-9]+)\/([0-9]+).*/\2/')" ]; ok $? "B11 csv size unchanged by DETACH"

# B12: module already bound -> LoadLibrary never called (fast path)
d=$(T); ( cd "$d" && "$BIN" 50 0 1 0 0 >/dev/null 2>&1 ); ok $? "B12 exit 0"
[ ! -e "$d/load_arg.txt" ]; ok $? "B12 no LoadLibrary call"
grep -q '^swap=50 pending=0 shutdown=1 ' "$d/forwarded.counts"; ok $? "B12 forwarded via fast path"

# B13a: syscall-level proof — implicit writes are <= 2KB, batched
if [ -n "$TRACER" ]; then
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 "$TRACER" "$d/trace.log" "$BIN" 3000 0 1 0 0 >/dev/null 2>&1 ); ok $? "B13a exit 0"
cnt=$(grep -c '^csv_write ' "$d/trace.log"); mx=$(grep -oE '^csv_write [0-9]+' "$d/trace.log" | awk '{print $2}' | sort -n | tail -1)
[ "$mx" -le 2048 ] 2>/dev/null; ok $? "B13a max csv write <= 2048 (got $mx)"
[ "$cnt" -ge 10 ] && [ "$cnt" -le 60 ]; ok $? "B13a batched count in [10,60] (got $cnt)"
# B13b: crash run has ZERO explicit flushes anywhere -> every counted
# write is an implicit CRT flush. This is point-2's smoking gun.
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 "$TRACER" "$d/trace.log" "$BIN" 5000 0 0 1 0 >/dev/null 2>&1 ); [ $? -eq 42 ]; ok $? "B13b exit 42"
cnt=$(grep -c '^csv_write ' "$d/trace.log"); mx=$(grep -oE '^csv_write [0-9]+' "$d/trace.log" | awk '{print $2}' | sort -n | tail -1)
[ "$cnt" -ge 15 ]; ok $? "B13b implicit writes without any fflush (got $cnt)"
[ "$mx" -le 2048 ] 2>/dev/null; ok $? "B13b max csv write <= 2048 (got $mx)"
fi

# B14: forced setvbuf failure -> forwarding intact, data complete, run LOUDLY flagged
d=$(T); ( cd "$d" && GSHIM_TEST_NULL_MODULE=1 GSHIM_TEST_SETVBUF_FAIL=1 \
  "$BIN" 500 0 1 0 0 >/dev/null 2>&1 ); ok $? "B14 exit 0 (forwarding intact)"
grep -q '^swap=500 pending=0 shutdown=1 ' "$d/forwarded.counts"; ok $? "B14 forwarded despite setvbuf fail"
grep -q 'setvbuf failed' "$d/gshim_error.txt"; ok $? "B14 error notes setvbuf"
head -1 "$d/gshim_log.csv" | grep -q 'buf=default-UNPINNED'; ok $? "B14 header flagged UNPINNED"
tail -1 "$d/gshim_log.csv" | grep -q '^# end rows=501 overflow=0'; ok $? "B14 data complete (501 rows)"
! grep -q 'FATAL' "$d/gshim_error.txt"; ok $? "B14 no fail-fast (measurement survives)"

echo "BEHAVIOUR: $pass passed, $fails failed"
[ "$fails" -eq 0 ]
