/* GEX caloD shim v6 — proxy glide2x.dll for INSTRUMENTATION ONLY (Fase A).
 *
 * What it does: forwards all 38 Glide imports the EU exe uses to the real
 * DLL (renamed), and logs grBufferSwap issue + grBufferNumPending depth
 * (sampled) with raw QueryPerformanceCounter ticks to gshim_log.csv.
 * What it does NOT do: change any call, argument, timing or return value.
 * The exe is untouched (md5 verifiable before/after).
 *
 * Design (setvbuf audit 2026-10-10, v5 -> v6; see git history):
 *  - 35 pure-forward functions go through gshim.def forwarder exports
 *    (no code, no arity/type assumptions at all). 3 intercepted calls are
 *    BOTH __declspec(dllexport) here AND listed in gshim.def, so their
 *    export never depends on linker auto-export behaviour:
 *      void  grBufferSwap(FxI32 swap_interval)  -> _grBufferSwap@4
 *      FxI32 grBufferNumPending(void)           -> _grBufferNumPending@0
 *      void  grGlideShutdown(void)              -> _grGlideShutdown@0
 *    (signatures VERIFIED: Glide 2.x SDK headers + exe IAT).
 *  - DllMain is ATTACH-only (handle stash + DisableThreadLibraryCalls).
 *    NO file I/O under the loader lock, ever: CRT locks (fopen/fprintf/
 *    fclose) inside DETACH risk a loader-lock/CRT-lock deadlock triangle,
 *    and abnormal termination skips DETACH (or arrives with the CRT gone)
 *    anyway. The final dump runs on the GAME thread instead: the
 *    grGlideShutdown wrapper calls finalize_log() (idempotent) BEFORE
 *    forwarding. Trickle appends (64 rows, no fflush) bound crash loss
 *    without on-path stalls. If the game ever quits without
 *    grGlideShutdown, the run visibly lacks its footer (rows stay
 *    usable; self-cost lost) — must be confirmed present in V-2;
 *    hooking grSstWinClose as fallback is explicitly pending that
 *    evidence.
 *  - No EXPLICIT fflush on the measured path (verified structurally).
 *    Implicit CRT flushes DO happen (any fprintf can trigger one when
 *    the stream buffer fills — standard stdio, no exception): they are
 *    bounded, not banished. setvbuf requests a 2 KB stream buffer and
 *    its return value is CHECKED: if honored (the common case), every
 *    implicit write is <= ~2 KB (~1-10 us page-cache) with
 *    deterministic cadence (~1 per ~100 rows); if refused, the run is
 *    LOUDLY flagged (error file + header buf=default-UNPINNED) and
 *    must be discarded — the bound is never claimed blindly. Observed
 *    <= 2048 B on glibc (B13 syscall trace); MSVCRT effectiveness is
 *    expected by C89 contract but pending V-2 measurement (see README).
 *    Each implicit flush lands inside some call's measured window, so
 *    footer max captures the worst one. Crash loss <= ~2 KB + 64 rows
 *    while pinned.
 *  - CAPACITY (stop-on-full, never wrap, never hide loss): 262144 rows
 *    (~25 swaps/s + pending changes + 1/4096 periodic). Planned 60 s
 *    runs need < ~5K rows typical, < ~150K pathological-hot-spin;
 *    overflow is therefore a canary for pathological behaviour, not a
 *    routine event. On trip: an invalidation marker + fflush (once) +
 *    error note; footer overflow=1; the file keeps a contiguous prefix
 *    but the RUN MUST BE DISCARDED (policy, see README). The footer
 *    pending_calls counter then sizes the next attempt rationally.
 *  - FAILURE POLICY (fail fast, never fake):
 *      forwarding unusable (no QPC / any of the 3 symbols unresolvable,
 *        all-or-nothing) -> gshim_error.txt + ExitProcess(111). The game
 *        NEVER continues with fictitious Swap/Pending returns. Removing
 *        the shim reverts everything (same restore procedure).
 *      log file unopenable -> forwarding continues, error noted loudly
 *        (the missing/short log + error file make the gap visible; the
 *        game itself is unaffected). Retry at finalize.
 *      setvbuf refused -> forwarding continues, data still complete,
 *        but the 2KB bound is void: error noted + header flagged
 *        (buf=default-UNPINNED); the run MUST be discarded (same
 *        error-file criterion). Never fail-fast: measurement survives.
 *      nolog marker unwritable (init or end) -> forwarding continues,
 *        error noted loudly; the NOLOG run is INVALID for A/B (mode or
 *        clean exit unprovable without its marker lines).
 *    NOTE: if the game runs at all, the loader has already bound the 35
 *    forwarder targets, so resolution failure is a defense-in-depth path.
 *  - GSHIM_NOLOG=1: pure pass-through. gshim_log.csv is NEVER opened
 *    (single choke point: open_log, nolog-guarded call sites only).
 *    Mode signal = gshim_nolog.marker (init line at resolve + end line
 *    with counters at shutdown; init-time/exit-time only, zero cost on
 *    the measured path, A/B-comparable). Errors still go to
 *    gshim_error.txt in both modes (failure signalling is not logging).
 *  - Self-cost accounting: QPC ticks spent inside the logger (sum + max)
 *    are reported in the footer for impact measurement (plus NOLOG A/B).
 *
 * Import list (EU exe IAT, 38 glide2x.dll entries, 0 ordinal-only,
 * 0 undecorated — verified 2026-10-10) lives in gshim.def. The real
 * DLL's exports are NOT assumed from the exe's imports: V-1b dumps the
 * renamed real DLL directly (see README). Build (32-bit, MinGW):
 *   i686-w64-mingw32-gcc -m32 -shared -O2 -o glide2x.dll gshim.c gshim.def
 * Then VALIDATE (V-0/V-1/V-1b/V-2) — see README. STATUS: source only,
 * never compiled here (no MinGW in sandbox); C syntax checked with gcc
 * -fsyntax-only against tests/win32_stub (2026-10-10).
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The exe is PE32/i386 and stdcall _name@N decorations are x86-32
 * specific: a 64-bit build would silently produce a useless DLL. */
#if defined(_WIN32) && !defined(__i386__) && !defined(_M_IX86)
#error "gshim must be built 32-bit (e.g. i686-w64-mingw32-gcc -m32)"
#endif

#define GSHIM_VERSION "6 (setvbuf audit 2026-10-10)"
#define REAL_DLL "glide2x_gex_real.dll"
#define LOG_FILE "gshim_log.csv"
#define ERR_FILE "gshim_error.txt"
#define NOLOG_MARKER "gshim_nolog.marker"
#define NOLOG_ENV "GSHIM_NOLOG"
#define EXIT_INIT_FAILED 111

/* 262144 rows x 24 B = 6 MB static. Stop-on-full (never wrap): the ring
 * keeps a contiguous prefix and overflow becomes a discard-the-run alarm
 * (see trip marker in log_row + README). Flushed rows are NOT freed: the
 * RAM copy stays until shutdown; flushed_n only tracks file progress. */
#define RING_N 262144u
/* Trickle: at most TRICKLE_N rows per measured-path call into the CRT
 * buffer, never fflush (durability is finalize's job). */
/* Stream buffer, pinned (not defaulted): implicit flushes are then <= 2 KB
 * on ANY CRT, with identical cadence on MSVCRT and glibc — the Linux
 * behavioural bounds transfer to Windows by construction. */
#define TRICKLE_N 64u
#define FILEBUF_N 2048u
static char filebuf[FILEBUF_N];

typedef struct {
    uint64_t tick;  /* raw QueryPerformanceCounter */
    int32_t arg;
    uint32_t seq;   /* 1-based */
    uint8_t ev;     /* 'S' swap, 'P' pending, 'X' shutdown marker */
    uint8_t pad[3];
} row_t;

static void (__stdcall *p_Swap)(int32_t) = NULL;
static int32_t (__stdcall *p_Pending)(void) = NULL;
static void (__stdcall *p_Shutdown)(void) = NULL;

static HMODULE hShim = NULL;
static HMODULE hReal = NULL;
static FILE *logf = NULL;
static int buf_pinned = 0; /* setvbuf honored on the operative stream */
static int64_t qpf = 0;
static int nolog = 0;          /* GSHIM_NOLOG=1: pure pass-through */
static int finalized = 0;      /* finalize_log ran (shutdown hook) */
static const char *finalized_by = "none";
static volatile LONG init_state = 0; /* 0 virgin, 1 busy, 2 ready */

static row_t ring[RING_N];
static uint32_t ring_n = 0;
static uint32_t flushed_n = 0;
static int ring_overflow = 0;
static int header_written = 0;

static uint64_t n_swaps = 0;
static int32_t last_pending = -1;
static uint64_t pending_calls = 0;
static uint64_t log_cost_sum = 0; /* QPC ticks spent inside log_row */
static uint64_t log_cost_max = 0;

static void note_error(const char *msg)
{
    FILE *f;
    OutputDebugStringA(msg);
    f = fopen(ERR_FILE, "a");
    if (f) {
        fprintf(f, "gshim: %s\n", msg);
        fclose(f);
    }
}

/* Fail fast, never fake: the instrument refuses to run rather than emit
 * data it cannot stand behind. Revert = remove the shim (see README). */
static void fail_fast(const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "FATAL (exit %d): %s", EXIT_INIT_FAILED, msg);
    buf[sizeof(buf) - 1] = '\0';
    note_error(buf);
    ExitProcess(EXIT_INIT_FAILED);
}

/* SINGLE choke point for opening gshim_log.csv. Every call site is
 * guarded by `if (!nolog)` (structurally tested); the internal guard
 * below is defense in depth. NOLOG mode never reaches this function. */
static void open_log(const char *note)
{
    if (logf || nolog)
        return;
    logf = fopen(LOG_FILE, "a");
    if (logf) {
        if (setvbuf(logf, filebuf, _IOFBF, FILEBUF_N) == 0) {
            buf_pinned = 1;
        } else {
            buf_pinned = 0;
            note_error("setvbuf failed (2KB flush bound void; run flagged)");
        }
    }
    if (logf && !header_written) {
        fprintf(logf, "# gshim %s qpf=%lld (%s) buf=%s\n",
                GSHIM_VERSION, (long long)qpf, note,
                buf_pinned ? "2048" : "default-UNPINNED");
        fprintf(logf, "# seq,tick_raw,event,arg\n");
        header_written = 1;
    }
    if (!logf)
        note_error("cannot open log (will retry at finalize)");
}

static void marker_init(void)
{
    FILE *m = fopen(NOLOG_MARKER, "w");
    if (m) {
        fprintf(m, "gshim %s nolog=1 qpf=%lld\n",
                GSHIM_VERSION, (long long)qpf);
        fclose(m);
    } else {
        note_error("nolog marker init failed (NOLOG run invalid for A/B)");
    }
}

static void marker_end(const char *by)
{
    FILE *m = fopen(NOLOG_MARKER, "a");
    if (m) {
        fprintf(m, "end swaps=%llu pending_calls=%llu by=%s\n",
                (unsigned long long)n_swaps,
                (unsigned long long)pending_calls, by);
        fclose(m);
    } else {
        note_error("nolog marker end failed (clean exit unprovable)");
    }
}

static void write_rows(uint32_t from, uint32_t to)
{
    uint32_t i;
    if (!logf)
        return;
    for (i = from; i < to; i++) {
        fprintf(logf, "%u,%llu,%c,%d\n",
                ring[i].seq,
                (unsigned long long)ring[i].tick,
                ring[i].ev, (int)ring[i].arg);
    }
}

/* Cost of this function (one QPC pair + stores, + a 64-row trickle on
 * ~1/64 rows) is self-measured into log_cost_sum/max, reported in the
 * footer. No fflush here: stalls stay out of the measured path. */
static void log_row(uint8_t ev, int32_t arg)
{
    LARGE_INTEGER t0, t1;
    uint64_t dt;
    QueryPerformanceCounter(&t0);
    if (ring_n < RING_N) {
        ring[ring_n].tick = (uint64_t)t0.QuadPart;
        ring[ring_n].arg = arg;
        ring[ring_n].seq = ring_n + 1;
        ring[ring_n].ev = ev;
        ring_n++;
        if (logf && ring_n - flushed_n >= TRICKLE_N) {
            write_rows(flushed_n, flushed_n + TRICKLE_N);
            flushed_n += TRICKLE_N;
        }
    } else if (!ring_overflow) {
        /* Trip-once invalidation: this run is doomed (discard policy),
         * so one durable marker here is honest, not a perturbation. */
        ring_overflow = 1;
        if (logf) {
            fprintf(logf, "# overflow at seq=%u: RUN INVALID, tail dropped\n",
                    ring_n);
            fflush(logf);
        }
        note_error("ring overflow (run invalid, see footer/marker)");
    }
    QueryPerformanceCounter(&t1);
    dt = (uint64_t)(t1.QuadPart - t0.QuadPart);
    log_cost_sum += dt;
    if (dt > log_cost_max)
        log_cost_max = dt;
}

static void do_init(void)
{
    LARGE_INTEGER f;
    char path[MAX_PATH];
    char *bs, *p;
    char nolog_env[8];
    const char *missing;

    if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0)
        fail_fast("no QPC");
    qpf = (int64_t)f.QuadPart;

    if (GetEnvironmentVariableA(NOLOG_ENV, nolog_env, sizeof(nolog_env)) > 0
        && nolog_env[0] == '1') {
        nolog = 1;
    }

    /* The loader has normally bound the real DLL already via the 35
     * forwarders; GetModuleHandle first, full-path LoadLibrary fallback.
     * Both run OUTSIDE DllMain (no loader-lock risk). */
    hReal = GetModuleHandleA(REAL_DLL);
    if (!hReal && hShim) {
        GetModuleFileNameA(hShim, path, sizeof(path));
        path[sizeof(path) - 1] = '\0';
        bs = path;
        for (p = path; *p; p++)
            if (*p == '\\' || *p == '/')
                bs = p + 1;
        *bs = '\0';
        strncat(path, REAL_DLL, sizeof(path) - strlen(path) - 1);
        hReal = LoadLibraryA(path);
    }
    if (!hReal)
        hReal = LoadLibraryA(REAL_DLL);
    if (hReal) {
        p_Swap = (void (__stdcall *)(int32_t))
            GetProcAddress(hReal, "_grBufferSwap@4");
        p_Pending = (int32_t (__stdcall *)(void))
            GetProcAddress(hReal, "_grBufferNumPending@0");
        p_Shutdown = (void (__stdcall *)(void))
            GetProcAddress(hReal, "_grGlideShutdown@0");
    }
    missing = !p_Swap ? "_grBufferSwap@4" :
        !p_Pending ? "_grBufferNumPending@0" :
        !p_Shutdown ? "_grGlideShutdown@0" : NULL;
    if (missing) {
        char msg[128];
        snprintf(msg, sizeof(msg), "cannot resolve %s in real DLL", missing);
        msg[sizeof(msg) - 1] = '\0';
        fail_fast(msg);
    }
    if (!nolog)
        open_log("init");
    else
        marker_init();
    init_state = 2;
}

static void ensure_init(void)
{
    if (init_state == 2)
        return;
    if (InterlockedCompareExchange(&init_state, 1, 0) != 0) {
        while (init_state == 1) {
            /* peer is in do_init: it holds no locks, finishes in <1 ms */
        }
        return;
    }
    do_init();
}

/* Game-thread only (grGlideShutdown wrapper). Idempotent: second and
 * later shutdown calls just forward. Never called from DllMain. */
static void finalize_log(const char *by)
{
    if (finalized)
        return;
    finalized = 1;
    finalized_by = by;
    if (!nolog) {
        open_log("late open at finalize");
        if (logf) {
            double csum = qpf > 0 ?
                (double)log_cost_sum * 1e6 / (double)qpf : -1.0;
            double cmax = qpf > 0 ?
                (double)log_cost_max * 1e6 / (double)qpf : -1.0;
            write_rows(flushed_n, ring_n);
            flushed_n = ring_n;
            fprintf(logf, "# end rows=%u overflow=%d swaps=%llu "
                          "pending_calls=%llu log_cost_us_sum=%.1f max=%.1f "
                          "by=%s\n",
                    ring_n, ring_overflow,
                    (unsigned long long)n_swaps,
                    (unsigned long long)pending_calls,
                    csum, cmax, finalized_by);
            fclose(logf);
            logf = NULL;
        } else {
            note_error("finalize: log unavailable (rows lost)");
        }
    } else {
        marker_end(by);
    }
}

/* ---- intercepted calls (pass-through + log) ---- */
__declspec(dllexport) void __stdcall grBufferSwap(int32_t swap_interval)
{
    ensure_init(); /* never returns on failure: p_Swap is non-NULL below */
    if (!nolog)
        log_row('S', swap_interval);
    n_swaps++;
    p_Swap(swap_interval);
}

__declspec(dllexport) int32_t __stdcall grBufferNumPending(void)
{
    int32_t v;
    ensure_init(); /* never returns on failure: p_Pending non-NULL below */
    v = p_Pending();
    pending_calls++;
    /* Spin loops call this hot: log every change + 1/4096 periodic
     * (plateau confirmation only; transitions are always exact). */
    if (!nolog && (v != last_pending || (pending_calls & 4095) == 0)) {
        last_pending = v;
        log_row('P', v);
    }
    return v;
}

__declspec(dllexport) void __stdcall grGlideShutdown(void)
{
    ensure_init(); /* never returns on failure: p_Shutdown non-NULL below */
    if (!nolog)
        log_row('X', 0);
    finalize_log("shutdown");
    p_Shutdown();
}

/* ---- ATTACH-only DllMain: DETACH intentionally does nothing ----
 * Rationale: file I/O (even fclose) under the loader lock risks a
 * loader-lock/CRT-lock deadlock triangle, and abnormal termination
 * skips DETACH (or arrives with the CRT gone) anyway. The final dump
 * runs on the game thread via grGlideShutdown; incremental appends
 * bound crash loss. Worst case without shutdown: tail + footer
 * missing, which is visible in the log (see README). */
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        hShim = h;
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}
