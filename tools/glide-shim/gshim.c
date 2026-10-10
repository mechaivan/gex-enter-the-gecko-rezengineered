/* GEX caloD shim v2 — proxy glide2x.dll for INSTRUMENTATION ONLY (Fase A).
 *
 * What it does: forwards all 38 Glide imports the EU exe uses to the real
 * DLL (renamed), and logs grBufferSwap issue + grBufferNumPending depth
 * (sampled) with raw QueryPerformanceCounter ticks to gshim_log.csv.
 * What it does NOT do: change any call, argument, timing or return value.
 * The exe is untouched (md5 verifiable before/after).
 *
 * Design (audit 2026-10-10, defects D-01..D-06 fixed from v1):
 *  - 36 pure-forward functions go through gshim.def forwarder exports
 *    (no code, no arity/type assumptions at all). D-01: the 2 intercepted
 *    calls are BOTH __declspec(dllexport) here AND listed in gshim.def, so
 *    their export does not depend on linker auto-export behaviour.
 *  - D-02: DllMain is minimal (handle stash + DisableThreadLibraryCalls).
 *    No LoadLibrary/fopen inside DllMain: resolution happens lazily on the
 *    first intercepted call (loader lock NOT held there). By then the real
 *    DLL is normally already bound via the 36 forwarders, so resolution is
 *    a GetModuleHandle + 2 GetProcAddress.
 *  - D-03/D-04: NO per-frame file I/O and NO per-row division. Rows go to
 *    an in-RAM ring (raw QPC ticks; offline conversion with qpf from the
 *    header is exact). Incremental append every 4096 rows + tail at
 *    detach bounds crash loss without per-frame jitter.
 *  - Failure policy: FORWARDING is the hard requirement (fail loud via
 *    gshim_error.txt + debug string + safe defaults); LOGGING is
 *    best-effort (late-open retry at detach, never breaks the game).
 *  - Self-cost accounting: QPC ticks spent inside the logger (sum + max)
 *    are reported in the footer, plus GSHIM_NOLOG=1 for A/B runs.
 *    Wrappers (VERIFIED Glide 2.x signatures, SDK headers + exe IAT):
 *      void  grBufferSwap(FxI32 swap_interval)      -> _grBufferSwap@4
 *      FxI32 grBufferNumPending(void)               -> _grBufferNumPending@0
 * D-05: 32-bit build enforced below (_WIN32 && !i386 => #error).
 *
 * Import list (EU exe IAT, 38 glide2x.dll entries, 0 ordinal-only,
 * 0 undecorated — re-verified 2026-10-10) lives in gshim.def.
 * Build (32-bit, on Windows with MinGW):
 *   i686-w64-mingw32-gcc -m32 -shared -O2 -o glide2x.dll gshim.c gshim.def
 * Then VALIDATE (dumpbin/objdump: exactly the 38 exports) — see README.
 * STATUS: source only, never compiled here (no MinGW in sandbox); C syntax
 * checked with gcc -fsyntax-only against tests/win32_stub (2026-10-10).
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

#define GSHIM_VERSION "2 (audit 2026-10-10)"
#define REAL_DLL "glide2x_gex_real.dll"
#define LOG_FILE "gshim_log.csv"
#define ERR_FILE "gshim_error.txt"
#define NOLOG_ENV "GSHIM_NOLOG"

/* 65536 rows >> any session (25 swaps/s + sparse pending samples). */
#define RING_N 65536u
#define FLUSH_EVERY 4096u

typedef struct {
    uint64_t tick;  /* raw QueryPerformanceCounter */
    int32_t arg;
    uint32_t seq;   /* 1-based */
    uint8_t ev;     /* 'S' swap, 'P' pending */
    uint8_t pad[3];
} row_t;

static void (__stdcall *p_Swap)(int32_t) = NULL;
static int32_t (__stdcall *p_Pending)(void) = NULL;

static HMODULE hShim = NULL;
static HMODULE hReal = NULL;
static FILE *logf = NULL;
static int64_t qpf = 0;
static int nolog = 0;        /* GSHIM_NOLOG=1: pure pass-through */
static int init_failed = 0;  /* forwarding unusable: safe defaults */
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

/* Cost of this function (one QPC pair + stores, + a rare batched flush)
 * is self-measured into log_cost_sum/max, reported in the footer. */
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
        if (logf && (ring_n % FLUSH_EVERY) == 0) {
            write_rows(flushed_n, ring_n);
            flushed_n = ring_n;
            fflush(logf);
        }
    } else {
        ring_overflow = 1;
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

    if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0) {
        note_error("no QPC");
        init_failed = 1;
        init_state = 2;
        return;
    }
    qpf = (int64_t)f.QuadPart;

    if (GetEnvironmentVariableA(NOLOG_ENV, nolog_env, sizeof(nolog_env)) > 0
        && nolog_env[0] == '1') {
        nolog = 1;
    }

    /* The loader has normally bound the real DLL already via the 36
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
    }
    if (!p_Swap || !p_Pending) {
        note_error("cannot resolve Swap/Pending in real DLL");
        p_Swap = NULL;
        p_Pending = NULL;
        init_failed = 1;
        init_state = 2;
        return;
    }
    logf = fopen(LOG_FILE, "a");
    if (logf) {
        fprintf(logf, "# gshim %s qpf=%lld nolog=%d\n",
                GSHIM_VERSION, (long long)qpf, nolog);
        if (!nolog)
            fprintf(logf, "# seq,tick_raw,event,arg\n");
        header_written = 1;
    } else if (!nolog) {
        note_error("cannot open log (retry at detach)");
    }
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

/* Detach-time only: last-chance open retry, tail write, footer, close. */
static void flush_log(void)
{
    if (!logf && !nolog) {
        logf = fopen(LOG_FILE, "a");
        if (logf && !header_written) {
            fprintf(logf, "# gshim %s qpf=%lld nolog=0 (late open)\n",
                    GSHIM_VERSION, (long long)qpf);
            fprintf(logf, "# seq,tick_raw,event,arg\n");
            header_written = 1;
        }
    }
    if (logf) {
        double csum = qpf > 0 ? (double)log_cost_sum * 1e6 / (double)qpf : -1.0;
        double cmax = qpf > 0 ? (double)log_cost_max * 1e6 / (double)qpf : -1.0;
        write_rows(flushed_n, ring_n);
        flushed_n = ring_n;
        fprintf(logf, "# end rows=%u overflow=%d swaps=%llu pending_calls=%llu "
                      "log_cost_us_sum=%.1f max=%.1f nolog=%d init_failed=%d\n",
                ring_n, ring_overflow,
                (unsigned long long)n_swaps,
                (unsigned long long)pending_calls,
                csum, cmax, nolog, init_failed);
        fclose(logf);
        logf = NULL;
    } else {
        note_error("detach: log unavailable (data lost)");
    }
}

/* ---- intercepted calls (pass-through + log) ---- */
__declspec(dllexport) void __stdcall grBufferSwap(int32_t swap_interval)
{
    ensure_init();
    if (!nolog && !init_failed)
        log_row('S', swap_interval);
    n_swaps++;
    if (p_Swap)
        p_Swap(swap_interval);
}

__declspec(dllexport) int32_t __stdcall grBufferNumPending(void)
{
    int32_t v;
    ensure_init();
    v = p_Pending ? p_Pending() : 0;
    pending_calls++;
    /* Spin loops call this thousands of times/s: log on change + 1/1024. */
    if (!nolog && !init_failed &&
        (v != last_pending || (pending_calls & 1023) == 0)) {
        last_pending = v;
        log_row('P', v);
    }
    return v;
}

/* ---- minimal DllMain: NO LoadLibrary/fopen here (see do_init) ---- */
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID r)
{
    if (reason == DLL_PROCESS_ATTACH) {
        hShim = h;
        DisableThreadLibraryCalls(h);
    } else if (reason == DLL_PROCESS_DETACH && r == NULL) {
        flush_log(); /* normal detach only; CRT may be gone on terminate */
    }
    return TRUE;
}
