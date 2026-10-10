/* fakereal.c — test double for glide2x_gex_real.dll (Win32 harness only).
 *
 * Staged ONLY in temp scenario dirs under its real name, so the shim's
 * beside-shim LoadLibrary path resolves it exactly like production —
 * the real loader, real GetProcAddress, real DllMain-via-loader. The
 * harness asserts the loaded module path, so a wrong DLL can only FAIL
 * loudly, never pass silently. NEVER installed anywhere real.
 *
 * Same object, 4 link variants (see build-harness.sh, one .def minus one
 * line each): full + missing-one-export x3. The omitted symbol keeps its
 * code but loses its export, so GetProcAddress fails exactly like a
 * real DLL lacking that export (fail_fast path, exit 111).
 *
 * Control: GSHIM_FAKE_PENDING=const:V | alt | ramp:N (queue depth
 * script, mirrors the Linux harness GSHIM_TEST_PENDING semantics).
 * STRICT is unconditional: finalize MUST precede the real shutdown
 * call (footer/marker order), else _exit(99). Counts are dumped by
 * the shutdown stub to forwarded.counts AND served live via
 * fakereal_counts. 32-bit MinGW: build with -m32 -shared.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long f_swap = 0;
static long f_pending = 0;
static long f_shutdown = 0;
static long f_last_arg = -1;

__declspec(dllexport) void __stdcall grBufferSwap(int32_t swap_interval)
{
    f_swap++;
    f_last_arg = (long)swap_interval;
}

__declspec(dllexport) int32_t __stdcall grBufferNumPending(void)
{
    char mode[64];
    long i = f_pending++;
    long n;
    if (GetEnvironmentVariableA("GSHIM_FAKE_PENDING", mode,
                                sizeof(mode)) == 0)
        return 0;
    if (strncmp(mode, "const:", 6) == 0)
        return (int32_t)atol(mode + 6);
    if (strcmp(mode, "alt") == 0)
        return (int32_t)(i & 1);
    if (strncmp(mode, "ramp:", 5) == 0) {
        n = atol(mode + 5);
        return (int32_t)(n > 0 ? i % n : 0);
    }
    return 0;
}

static int read_line(FILE *f, char *buf, size_t cap)
{
    size_t n;
    if (!fgets(buf, (int)cap, f))
        return 0;
    n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    return 1;
}

static int file_last_line_is(const char *path, const char *prefix)
{
    FILE *f = fopen(path, "r");
    char line[512], last[512];
    size_t n, plen;
    int have = 0;
    if (!f)
        return 0;
    last[0] = '\0';
    while (read_line(f, line, sizeof(line))) {
        n = strlen(line);
        if (n >= sizeof(last))
            n = sizeof(last) - 1;
        memcpy(last, line, n);
        last[n] = '\0';
        have = 1;
    }
    fclose(f);
    plen = strlen(prefix);
    return have && strncmp(last, prefix, plen) == 0;
}

static int file_line_count(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];
    int n = 0;
    if (!f)
        return -1;
    while (read_line(f, line, sizeof(line)))
        n++;
    fclose(f);
    return n;
}

__declspec(dllexport) void __stdcall grGlideShutdown(void)
{
    FILE *f;
    char nolog[8];
    f_shutdown++;
    /* STRICT: the shim must finalize BEFORE forwarding shutdown. */
    if (GetEnvironmentVariableA("GSHIM_NOLOG", nolog, sizeof(nolog)) > 0
        && nolog[0] == '1') {
        if (file_line_count("gshim_nolog.marker") != 2)
            ExitProcess(99);
    } else {
        if (!file_last_line_is("gshim_log.csv", "# end"))
            ExitProcess(99);
    }
    f = fopen("forwarded.counts", "w");
    if (f) {
        fprintf(f, "swap=%ld pending=%ld shutdown=%ld last_arg=%ld\n",
                f_swap, f_pending, f_shutdown, f_last_arg);
        fclose(f);
    }
}

/* Live counts for the harness child (cross-checked with the file). */
__declspec(dllexport) void __stdcall fakereal_counts(int *swap, int *pending,
                                                     int *shutdown,
                                                     int *last_arg)
{
    if (swap)
        *swap = (int)f_swap;
    if (pending)
        *pending = (int)f_pending;
    if (shutdown)
        *shutdown = (int)f_shutdown;
    if (last_arg)
        *last_arg = (int)f_last_arg;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID r)
{
    (void)h;
    (void)reason;
    (void)r;
    return TRUE;
}
