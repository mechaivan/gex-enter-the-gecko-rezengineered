/* Behavioural harness: controllable Win32 fakes + scripted "real DLL".
 * Control knobs (env, read live so scenarios stay in the shell):
 *   GSHIM_TEST_QPC_STEP=N     QPC ticks advanced per call (default 100)
 *   GSHIM_TEST_QPF=N          QueryPerformanceFrequency value (def 10000000)
 *   GSHIM_TEST_NO_QPC=1       QPF fails (fail-fast path)
 *   GSHIM_TEST_NULL_MODULE=1  GetModuleHandle returns NULL (loads via path)
 *   GSHIM_TEST_LOAD_FAIL=1    LoadLibrary returns NULL (fail-fast path)
 *   GSHIM_TEST_NULL_PROC=sym  that GetProcAddress returns NULL (fail-fast)
 *   GSHIM_TEST_PENDING=...    const:V | alt | ramp:N (fake queue depth)
 *   GSHIM_TEST_STRICT=1       fake_shutdown asserts footer/marker order
 * Fake DLL calls are counted; counts are dumped by fake_shutdown to
 * forwarded.counts (proves forwarding happened, with exact argc).
 * setvbuf is interposed (test double): normally passes through to the
 * real one, but GSHIM_TEST_SETVBUF_FAIL=1 forces nonzero (B14) without
 * depending on the OS refusing spontaneously.
 */
#define _GNU_SOURCE
#include <windows.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static long long h_tick = 1000000LL;
static long h_swap_calls = 0;
static long h_pending_calls = 0;
static long h_shutdown_calls = 0;
static long h_last_swap_arg = -1;

static long env_long(const char *name, long def)
{
    const char *v = getenv(name);
    return v ? atol(v) : def;
}

/* ---- Win32 fakes ---- */
void DisableThreadLibraryCalls(HMODULE h) { (void)h; }

int QueryPerformanceFrequency(LARGE_INTEGER *f)
{
    if (getenv("GSHIM_TEST_NO_QPC"))
        return 0;
    f->QuadPart = (long long)env_long("GSHIM_TEST_QPF", 10000000L);
    return 1;
}

int QueryPerformanceCounter(LARGE_INTEGER *t)
{
    h_tick += (long long)env_long("GSHIM_TEST_QPC_STEP", 100L);
    t->QuadPart = h_tick;
    return 1;
}

DWORD GetModuleFileNameA(HMODULE h, char *buf, DWORD n)
{
    const char *fake = "C:\\game\\glide2x.dll";
    (void)h;
    strncpy(buf, fake, (size_t)n);
    buf[n - 1] = '\0';
    return (DWORD)strlen(buf);
}

static void record_bind(const char *tag, LPCSTR arg)
{
    FILE *f = fopen("bind_log.txt", "a");
    if (f) {
        fprintf(f, "%s:%s\n", tag, arg ? arg : "(null)");
        fclose(f);
    }
}

HMODULE GetModuleHandleA(LPCSTR name)
{
    record_bind("getmodule", name);
    if (getenv("GSHIM_TEST_NULL_MODULE"))
        return NULL;
    return (HMODULE)0x1000;
}

HMODULE LoadLibraryA(LPCSTR path)
{
    FILE *f = fopen("load_arg.txt", "w");
    if (f) {
        fprintf(f, "%s\n", path ? path : "(null)");
        fclose(f);
    }
    record_bind("loadlib", path);
    if (getenv("GSHIM_TEST_LOAD_FAIL"))
        return NULL;
    return (HMODULE)0x2000;
}

static void fake_swap(int32_t arg)
{
    h_swap_calls++;
    h_last_swap_arg = (long)arg;
}

static int32_t fake_pending(void)
{
    const char *mode = getenv("GSHIM_TEST_PENDING");
    long i = h_pending_calls++;
    if (!mode || strncmp(mode, "const:", 6) == 0)
        return (int32_t)(mode ? atol(mode + 6) : 0);
    if (strcmp(mode, "alt") == 0)
        return (int32_t)(i & 1);
    if (strncmp(mode, "ramp:", 5) == 0) {
        long n = atol(mode + 5);
        return (int32_t)(n > 0 ? i % n : 0);
    }
    return 0;
}

static int file_last_line_is(const char *path, const char *prefix)
{
    FILE *f = fopen(path, "r");
    char line[512], last[512];
    int have = 0;
    if (!f)
        return 0;
    while (fgets(line, sizeof(line), f)) {
        strcpy(last, line);
        have = 1;
    }
    fclose(f);
    return have && strncmp(last, prefix, strlen(prefix)) == 0;
}

static int file_line_count(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];
    int n = 0;
    if (!f)
        return -1;
    while (fgets(line, sizeof(line), f))
        n++;
    fclose(f);
    return n;
}

static void fake_shutdown(void)
{
    FILE *f;
    h_shutdown_calls++;
    if (getenv("GSHIM_TEST_STRICT")) {
        /* finalize MUST have completed before the real call is made. */
        if (getenv("GSHIM_NOLOG")) {
            if (file_line_count("gshim_nolog.marker") != 2)
                _exit(99);
        } else {
            if (!file_last_line_is("gshim_log.csv", "# end"))
                _exit(99);
        }
    }
    f = fopen("forwarded.counts", "w");
    if (f) {
        fprintf(f, "swap=%ld pending=%ld shutdown=%ld last_arg=%ld\n",
                h_swap_calls, h_pending_calls, h_shutdown_calls,
                h_last_swap_arg);
        fclose(f);
    }
}

void *GetProcAddress(HMODULE h, LPCSTR name)
{
    const char *nullme;
    (void)h;
    nullme = getenv("GSHIM_TEST_NULL_PROC");
    if (nullme && name && strcmp(nullme, name) == 0)
        return NULL;
    if (name && strcmp(name, "_grBufferSwap@4") == 0)
        return (void *)fake_swap;
    if (name && strcmp(name, "_grBufferNumPending@0") == 0)
        return (void *)fake_pending;
    if (name && strcmp(name, "_grGlideShutdown@0") == 0)
        return (void *)fake_shutdown;
    return NULL;
}

DWORD GetEnvironmentVariableA(LPCSTR name, char *buf, DWORD n)
{
    const char *v = name ? getenv(name) : NULL;
    size_t len;
    if (!v)
        return 0;
    len = strlen(v);
    if (n > 0) {
        strncpy(buf, v, (size_t)n);
        buf[n - 1] = '\0';
    }
    return (DWORD)len;
}

void OutputDebugStringA(LPCSTR s)
{
    FILE *f = fopen("debug.log", "a");
    if (f) {
        fprintf(f, "%s\n", s ? s : "(null)");
        fclose(f);
    }
}

void ExitProcess(DWORD code)
{
    _exit((int)code);
}

/* setvbuf test double: gshim's call resolves here (executable symbols
 * win over libc's); passthrough unless failure is forced. */
int setvbuf(FILE *fp, char *buf, int mode, size_t size)
{
    static int (*real_setvbuf)(FILE *, char *, int, size_t) = NULL;
    if (getenv("GSHIM_TEST_SETVBUF_FAIL"))
        return 1;
    if (!real_setvbuf)
        real_setvbuf = dlsym(RTLD_NEXT, "setvbuf");
    return real_setvbuf(fp, buf, mode, size);
}

long InterlockedCompareExchange(volatile long *dst, long ex, long cmp)
{
    long old = *dst;
    if (old == cmp)
        *dst = ex;
    return old;
}
