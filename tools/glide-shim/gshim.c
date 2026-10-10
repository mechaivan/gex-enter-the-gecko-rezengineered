/* GEX caloD shim — proxy glide2x.dll for INSTRUMENTATION ONLY (Fase A).
 *
 * What it does: forwards all 38 Glide imports the EU exe uses to the real
 * DLL (renamed), and logs grBufferSwap interval + grBufferNumPending depth
 * with QueryPerformanceCounter timestamps to gshim_log.csv.
 * What it does NOT do: change any call, argument, timing or return value.
 * The exe is untouched (md5 verifiable before/after).
 *
 * Zero-signature-risk design:
 *  - 36 pure-forward functions go through gshim.def forwarder exports
 *    (no code, no arity/type assumptions at all).
 *  - Only the 2 intercepted calls are C wrappers, and both signatures are
 *    VERIFIED facts (Glide 2.x docs + gglide.c implementation):
 *      void  grBufferSwap(FxI32 swap_interval)
 *      FxI32 grBufferNumPending(void)
 *
 * Import list (EU exe IAT, 38 glide2x.dll entries, enumerated 2026-10-10)
 * lives in gshim.def. Build (32-bit, on Windows with MinGW):
 *   i686-w64-mingw32-gcc -m32 -shared -O2 -o glide2x.dll gshim.c gshim.def
 * See README.md for install / run / restore. STATUS: source only, never
 * compiled here (no MinGW in sandbox); C syntax checked with gcc
 * -fsyntax-only against a stub windows.h (2026-10-10).
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Real DLL (the renamed original, e.g. nGlide's) in the same directory. */
#define REAL_DLL "glide2x_gex_real.dll"
#define LOG_FILE "gshim_log.csv"

static void (__stdcall *p_Swap)(int32_t) = NULL;
static int32_t (__stdcall *p_Pending)(void) = NULL;

static HMODULE hReal = NULL;
static FILE *logf = NULL;
static LARGE_INTEGER qpf;
static uint64_t seqn = 0;
static int32_t last_pending = -1;
static uint64_t pending_calls = 0;

static uint64_t now_us(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)(t.QuadPart * 1000000ULL / (uint64_t)qpf.QuadPart);
}

static void log_row(const char *ev, int32_t arg)
{
    if (!logf)
        return;
    seqn++;
    fprintf(logf, "%llu,%llu,%s,%d\n",
            (unsigned long long)seqn,
            (unsigned long long)now_us(), ev, (int)arg);
}

/* ---- intercepted calls (pass-through + log) ---- */
void __stdcall grBufferSwap(int32_t swap_interval)
{
    log_row("swap", swap_interval);
    if (logf)
        fflush(logf);
    p_Swap(swap_interval);
}

int32_t __stdcall grBufferNumPending(void)
{
    int32_t v = p_Pending();
    pending_calls++;
    /* Spin loops call this thousands of times/s: log on change + 1/1024. */
    if (v != last_pending || (pending_calls & 1023) == 0) {
        last_pending = v;
        log_row("pending", v);
    }
    return v;
}

/* ---- init ---- */
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        char *bs, *p;
        DisableThreadLibraryCalls(h);
        if (!QueryPerformanceFrequency(&qpf) || qpf.QuadPart == 0)
            return FALSE;
        /* Resolve the real DLL next to this proxy (game dir). */
        GetModuleFileNameA(h, path, sizeof(path));
        path[sizeof(path) - 1] = '\0';
        bs = path;
        for (p = path; *p; p++)
            if (*p == '\\' || *p == '/')
                bs = p + 1;
        *bs = '\0';
        strncat(path, REAL_DLL, sizeof(path) - strlen(path) - 1);
        hReal = LoadLibraryA(path);
        if (!hReal)
            return FALSE; /* fail loud: nothing works silently */
        p_Swap = (void (__stdcall *)(int32_t))
            GetProcAddress(hReal, "_grBufferSwap@4");
        p_Pending = (int32_t (__stdcall *)(void))
            GetProcAddress(hReal, "_grBufferNumPending@0");
        if (!p_Swap || !p_Pending)
            return FALSE;
        logf = fopen(LOG_FILE, "a");
        if (logf) {
            fprintf(logf, "# seq,tick_us,event,arg (gshim 2026-10-10)\n");
            fflush(logf);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (logf) {
            log_row("detach", 0);
            fclose(logf);
            logf = NULL;
        }
    }
    return TRUE;
}
