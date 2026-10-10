#!/usr/bin/env python3
"""test_init: STRUCTURAL regression test for the init/exit design (v3).

What it pins (regex/brace analysis of gshim.c — NOT a behavioural test;
behaviour needs Windows + the real DLL, see README "Validar"):
  1. DllMain is ATTACH-only: no DETACH branch, no LoadLibrary/
     GetProcAddress/fopen/fprintf/fclose/fflush (nothing under the
     loader lock except the handle stash + DisableThreadLibraryCalls).
  2. Fail fast, never fake: a fail_fast helper (error file +
     ExitProcess) exists; do_init calls it on every fatal path (QPC +
     resolution, >= 2 call sites); wrappers have no fallback returns
     (no `?:` on p_* pointers, no `if (p_` guards); the old
     init_failed degraded mode is gone.
  3. NOLOG isolation: gshim_log.csv is opened ONLY inside the open_log
     choke point, whose call sites are all `if (!nolog)`-guarded; the
     NOLOG signal is a separate marker file.
  4. Final dump on the game thread: the shutdown wrapper calls
     finalize_log, which is idempotent, writes rows + `# end` footer
     with `by=`, and closes; NOLOG finalizes to the marker instead.
  5. One-time init via InterlockedCompareExchange; resolution lives in
     do_init (outside DllMain); 32-bit build guard present.
  6. No direct file I/O in any wrapper: logging goes through log_row
     (ring); fopen/fflush appear only in init/finalize paths.
  7. Capacity & I/O (v4): 262144-row stop-on-full ring with a trip-once
     invalidation marker; trickle writes, no fflush on the measured
     path; 1/4096 pending sampling; loud marker failures.
  8. CRT buffer pinned (v5): 2 KB user buffer via setvbuf BEFORE any I/O,
     so implicit flushes are <= 2 KB when honored (observed glibc;
     MSVCRT pending V-2 — v6 makes the condition explicit).
  9. setvbuf return checked (v6): refusal is LOUDLY flagged (error file +
     header buf=default-UNPINNED), never silently claimed; forwarding is
     not fail-fast on this path.
Exit 0 = all pass.
"""
import re
import sys
from pathlib import Path

SHIM = Path(__file__).resolve().parent.parent
fails = []
WRAPPERS = ['grBufferSwap', 'grBufferNumPending', 'grGlideShutdown']


def check(cond, msg):
    print(('PASS ' if cond else 'FAIL ') + msg)
    if not cond:
        fails.append(msg)


def strip_comments(src):
    code = re.sub(r'/\*.*?\*/', ' ', src, flags=re.S)
    return re.sub(r'//[^\n]*', '', code)


def func_body(code, name):
    """Brace-matched body of the DEFINITION of `name` (comments already
    stripped, `) {` required so prototypes/calls never match)."""
    m = re.search(r'\b%s\s*\([^();]*\)\s*\{' % re.escape(name), code)
    assert m, name
    i = code.index('{', m.end() - 1)
    depth, j = 0, i
    while True:
        if code[j] == '{':
            depth += 1
        elif code[j] == '}':
            depth -= 1
            if depth == 0:
                return code[i:j + 1]
        j += 1


def main():
    src = (SHIM / 'gshim.c').read_text()
    code = strip_comments(src)
    lines = code.split('\n')

    # 1. ATTACH-only DllMain
    dllmain = func_body(code, 'DllMain')
    check('DLL_PROCESS_DETACH' not in dllmain, 'DllMain has no DETACH branch')
    for tok in ['LoadLibrary', 'GetProcAddress', 'fopen', 'fprintf',
                'fclose', 'fflush', 'GetModuleHandle',
                'GetEnvironmentVariable', 'ExitProcess']:
        check(tok not in dllmain, f'DllMain has no {tok}')
    check('DisableThreadLibraryCalls' in dllmain,
          'DllMain disables thread calls')
    check('hShim = h' in dllmain, 'DllMain stashes the module handle')

    # 2. fail fast, never fake
    failfast = func_body(code, 'fail_fast')
    check('ExitProcess' in failfast and 'note_error' in failfast,
          'fail_fast = error file + ExitProcess')
    check('EXIT_INIT_FAILED' in failfast, 'fail_fast uses EXIT_INIT_FAILED')
    doinit = func_body(code, 'do_init')
    check(doinit.count('fail_fast(') >= 2,
          f"do_init has QPC + resolution fail_fast calls "
          f"(got {doinit.count('fail_fast(')})")
    check('init_failed' not in code, 'no init_failed degraded mode left')
    for w in WRAPPERS:
        body = func_body(code, w)
        check('ensure_init()' in body, f'{w} calls ensure_init()')
        check('?' not in body, f'{w} has no fallback ternary')
        check('if (p_' not in body, f'{w} has no null-pointer guard')

    # 3. NOLOG isolation (single choke point for the log file)
    check(code.count('fopen(LOG_FILE') == 1,
          f"gshim_log.csv opened in exactly one place "
          f"(got {code.count('fopen(LOG_FILE')})")
    openlog = func_body(code, 'open_log')
    check('fopen(LOG_FILE' in openlog, 'that place is open_log')
    check('if (logf || nolog)' in openlog,
          'open_log re-guards internally (defense in depth)')
    callsites = [i for i, l in enumerate(lines)
                 if 'open_log(' in l and 'static void open_log(' not in l
                 and 'open_log(const char' not in l]
    guarded = all(any('if (!nolog)' in lines[j]
                      for j in range(max(0, i - 3), i))
                  for i in callsites)
    check(len(callsites) >= 1 and guarded,
          f'all {len(callsites)} open_log call sites are if (!nolog)-guarded')
    check('#define NOLOG_MARKER' in code, 'NOLOG marker filename defined')
    check('fopen(NOLOG_MARKER' in code, 'marker written via its own path')
    loguses = [i for i, l in enumerate(lines) if 'LOG_FILE' in l]
    ok = True
    open_span = (code.index(openlog), code.index(openlog) + len(openlog))
    off = 0
    for idx, l in enumerate(lines):
        if 'LOG_FILE' in l:
            if '#define LOG_FILE' in l:
                pass
            elif not (open_span[0] <= off <= open_span[1]):
                ok = False
        off += len(l) + 1
    check(ok and len(loguses) >= 2,
          'LOG_FILE token only in #define + open_log body')

    # 4. final dump on the game thread
    fin = func_body(code, 'finalize_log')
    check('if (finalized)' in fin and 'finalized = 1' in fin,
          'finalize_log is idempotent')
    check('# end' in fin and 'by=%s' in fin,
          'finalize_log writes rows + `# end` footer with by=')
    check('fclose' in fin, 'finalize_log closes the log')
    check('marker_end(' in fin, 'NOLOG finalizes to the marker instead')
    check('finalize_log(' in func_body(code, 'grGlideShutdown'),
          'shutdown wrapper calls finalize_log')

    # 5. one-time init outside DllMain + arch guard
    check('InterlockedCompareExchange(&init_state, 1, 0)' in code,
          'one-time init via InterlockedCompareExchange')
    check('LoadLibrary' in doinit and 'GetProcAddress' in doinit,
          'resolution lives in do_init (outside DllMain)')
    check(re.search(r'#if defined\(_WIN32\).*__i386__.*_M_IX86', code, re.S)
          is not None and '#error' in code,
          '32-bit build guard (#error unless i386 on _WIN32)')

    # 6. no direct file I/O in wrappers
    for w in WRAPPERS:
        body = func_body(code, w)
        for tok in ['fprintf', 'fflush', 'fopen', 'fwrite', 'fclose']:
            check(tok not in body, f'{w} wrapper has no direct {tok}')

    # 7. capacity & I/O (v4)
    check('#define RING_N 262144u' in code, 'ring is 262144 rows')
    check('FLUSH_EVERY' not in code, 'no FLUSH_EVERY batching left')
    logrow = func_body(code, 'log_row')
    trip = logrow[logrow.index('else if'):]
    valid_path = logrow[:logrow.index('else if')]
    check('fflush' not in valid_path,
          'log_row valid path has no fflush (no on-path stall)')
    check('fflush' in trip,
          'overflow trip marker is flushed once (durable invalidation)')
    check('TRICKLE_N' in logrow, 'log_row trickles bounded batches')
    check('# overflow' in logrow, 'overflow trips an in-file marker')
    check('note_error' in logrow, 'overflow also notes the error file')
    check('(pending_calls & 4095)' in func_body(code, 'grBufferNumPending'),
          'pending sampling is 1/4096')
    for fn in ['marker_init', 'marker_end']:
        body = func_body(code, fn)
        check('else' in body and 'note_error' in body,
              f'{fn} reports failure loudly')

    # 8. CRT buffer pinned (v5)
    check('#define FILEBUF_N 2048u' in code, 'stream buffer is 2048 bytes')
    check('static char filebuf[FILEBUF_N]' in code,
          'user buffer is static storage')
    check('setvbuf(logf, filebuf, _IOFBF, FILEBUF_N)' in openlog,
          'open_log pins the buffer')
    check(openlog.index('setvbuf') < openlog.index('fprintf'),
          'setvbuf precedes any I/O on the stream')

    # 9. setvbuf return checked (v6)
    check('setvbuf(logf, filebuf, _IOFBF, FILEBUF_N) == 0' in openlog,
          'open_log checks the setvbuf return value')
    check('buf_pinned' in code, 'pin state is tracked')
    check('setvbuf failed' in openlog, 'refusal notes the error file')
    check('buf=%s' in openlog and 'default-UNPINNED' in openlog,
          'header flags the unpinned run')
    check('fail_fast(' not in openlog, 'no fail-fast on setvbuf refusal')

    print(f'{len(fails)} failures' if fails else 'ALL PASS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
