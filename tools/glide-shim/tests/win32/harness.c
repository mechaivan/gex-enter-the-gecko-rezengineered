/* w32harness — native Win32 behaviour harness for glide-shim (no Gex).
 *
 * Design: the PRODUCTION shim DLL (fresh build-win32.sh output, copied
 * to temp — the repo file is never loaded and hash-compared after the
 * run) is loaded by ABSOLUTE path in a throwaway child process per
 * scenario, with a FAKE glide2x_gex_real.dll staged beside it. The
 * seam is the shim's own beside-shim LoadLibrary path: no gshim.c
 * changes, no injection points, no hooks — the real loader, the real
 * GetProcAddress, the real DllMain-via-loader run on Windows. The
 * fake's stubs record args/returns and serve counts live.
 *
 * Isolation: one scenario dir per child under a temp run dir (CWD);
 * all shim outputs are CWD-relative, so nothing escapes. The child
 * asserts the loaded module paths (shim + fake) byte-for-byte, so a
 * wrong DLL can only FAIL loudly. SetDllDirectory("") drops CWD from
 * the DLL search as defense in depth. Exit codes: 0 pass, 1 fail,
 * 111 fail_fast proven (expected only in W06/W07), 42 no-shutdown
 * crash (W08 only), 99 finalize-before-forward violated (fake STRICT).
 * Any unexpected code is a FAIL naming the scenario. Per-scenario
 * timeout 180 s (kill + FAIL).
 *
 * Usage: w32harness.exe [--run-all]          parent: run all scenarios
 *        w32harness.exe child <id> <dir>     child: one scenario (spawned)
 * Build (MSYS2 MINGW32): i686-w64-mingw32-gcc -m32 -O2 -Wall -Wextra
 *        -o w32harness.exe harness.c   (see build-harness.sh)
 *
 * Portability notes: NO %llu/%lld anywhere (MSVCRT-unreliable, see
 * TESTING finding F-1) — 64-bit fields are parsed with parse_u64 and
 * never printed; diagnostics print raw lines. File reads use text
 * mode ("r": CRLF already translated by the CRT).
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIMEOUT_MS 180000
#define MAX_LINE 512
#define MAX_PATHBUF 1024

typedef void (__stdcall *swap_fn)(int32_t);
typedef int32_t (__stdcall *pending_fn)(void);
typedef void (__stdcall *shutdown_fn)(void);
typedef void (__stdcall *counts_fn)(int *, int *, int *, int *);

/* ---- shared file/parsing helpers (CRT only) ---- */

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

/* Strict: all-digits, non-empty, <= 20 chars. No CRT 64-bit dependency. */
static int parse_u64(const char *s, unsigned long long *out)
{
    unsigned long long v = 0;
    int digits = 0;
    if (!s || !out)
        return 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned long long)(*s - '0');
        s++;
        digits++;
    }
    if (digits == 0 || digits > 20 || *s != '\0')
        return 0;
    *out = v;
    return 1;
}

static int parse_i32(const char *s, int32_t *out)
{
    unsigned long long v;
    int neg = 0;
    if (!s || !out)
        return 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    if (!parse_u64(s, &v))
        return 0;
    if (neg ? v > 2147483648ULL : v > 2147483647ULL)
        return 0;
    *out = neg ? (int32_t)(0ULL - v) : (int32_t)v;
    return 1;
}

static int file_exists(const char *path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1L;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1L;
    }
    n = ftell(f);
    fclose(f);
    return n;
}

static int file_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char line[MAX_LINE];
    int found = 0;
    if (!f)
        return 0;
    while (read_line(f, line, sizeof(line))) {
        if (strstr(line, needle)) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

/* Data row "seq,tick,ev,arg". Returns 1 parsed, 0 comment/blank, -1 bad. */
static int parse_row(char *line, unsigned long long *seq,
                     unsigned long long *tick, char *ev, int32_t *arg)
{
    char *c1, *c2, *c3;
    if (line[0] == '#' || line[0] == '\0')
        return 0;
    c1 = strchr(line, ',');
    c2 = c1 ? strchr(c1 + 1, ',') : NULL;
    c3 = c2 ? strchr(c2 + 1, ',') : NULL;
    if (!c1 || !c2 || !c3 || strchr(c3 + 1, ','))
        return -1;
    *c1 = *c2 = *c3 = '\0';
    if (!parse_u64(line, seq) || !parse_u64(c1 + 1, tick))
        return -1;
    if ((c2 + 1)[0] == '\0' || (c2 + 1)[1] != '\0')
        return -1;
    *ev = (c2 + 1)[0];
    if (!parse_i32(c3 + 1, arg))
        return -1;
    return 1;
}

/* Footer "# end rows=R overflow=O swaps=S pending_calls=P ... by=B".
 * log_cost fields are presence-checked (timing-dependent values). */
static int parse_footer(const char *line, unsigned long long *rows,
                        unsigned long long *overflow,
                        unsigned long long *swaps,
                        unsigned long long *pcalls, char *by, size_t bycap)
{
    const char *p;
    char num[32];
    size_t i;
    if (strncmp(line, "# end ", 6) != 0)
        return 0;
    if (!strstr(line, "log_cost_us_sum=") || !strstr(line, "max="))
        return 0;
#define GETNUM(tag, out) do { \
        p = strstr(line, tag); \
        if (!p) return 0; \
        p += sizeof(tag) - 1; \
        i = 0; \
        while (i + 1 < sizeof(num) && p[i] >= '0' && p[i] <= '9') { \
            num[i] = p[i]; \
            i++; \
        } \
        num[i] = '\0'; \
        if (!parse_u64(num, out)) return 0; \
    } while (0)
    GETNUM("rows=", rows);
    GETNUM("overflow=", overflow);
    GETNUM("swaps=", swaps);
    GETNUM("pending_calls=", pcalls);
#undef GETNUM
    p = strstr(line, "by=");
    if (!p || bycap == 0)
        return 0;
    p += 3;
    i = 0;
    while (i + 1 < bycap && *p && *p != ' ' && *p != '\t') {
        by[i++] = *p++;
    }
    by[i] = '\0';
    return by[0] != '\0';
}

/* Join a + "\\" + b (no doubled separator when a already ends in one,
 * e.g. GetTempPathA's trailing backslash); 0 if it would not fit
 * (no silent truncation). */
static int join_path(char *dst, size_t cap, const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    int need_sep = la == 0 || (a[la - 1] != '\\' && a[la - 1] != '/');
    if (la + (need_sep ? 1 : 0) + lb + 1 > cap)
        return 0;
    memcpy(dst, a, la);
    if (need_sep) {
        dst[la] = '\\';
        la++;
    }
    memcpy(dst + la, b, lb);
    dst[la + lb] = '\0';
    return 1;
}

static int parse_counts(const char *path, int *swap, int *pending,
                        int *shutdown, int *last_arg)
{
    FILE *f = fopen(path, "r");
    char line[MAX_LINE];
    int ok = 0;
    if (!f)
        return 0;
    if (read_line(f, line, sizeof(line))) {
        ok = sscanf(line, "swap=%d pending=%d shutdown=%d last_arg=%d",
                    swap, pending, shutdown, last_arg) == 4;
    }
    fclose(f);
    return ok;
}

/* ---- child: one scenario ---- */

static char child_fail[1024];
static const char *child_id = "?";

#define CHECK(cond, ...) do { \
        if (!(cond) && !child_fail[0]) { \
            snprintf(child_fail, sizeof(child_fail), __VA_ARGS__); \
        } \
    } while (0)

struct child_ctx {
    char dir[MAX_PATHBUF];
    char shim_path[MAX_PATHBUF];
    char fake_path[MAX_PATHBUF];
    HMODULE hshim;
    HMODULE hfake;
    swap_fn p_swap;
    pending_fn p_pending;
    shutdown_fn p_shutdown;
    counts_fn p_counts;
    int nswap;
    int npending;
    int nshut;
    int sarg;
};

/* Canonicalize a Win32 path for STRICT file-identity comparison: full
 * path, long (non-8.3) spelling, extended-prefix stripped, separators
 * folded to backslash (interior duplicates collapsed; a leading "\\"
 * root is preserved). Returns 1 on success; on ANY failure dst is ""
 * — callers must check the return, since "" never proves identity. */
static int canon_path(char *dst, size_t cap, const char *src)
{
    char tmp[MAX_PATHBUF], lng[MAX_PATHBUF];
    const char *s;
    size_t i, o;
    if (!cap)
        return 0;
    dst[0] = '\0';
    if (!GetFullPathNameA(src, (DWORD)sizeof(tmp), tmp, NULL))
        return 0;
    s = tmp;
    if (GetLongPathNameA(tmp, lng, (DWORD)sizeof(lng)))
        s = lng;
    if (!strncmp(s, "\\\\?\\", 4) || !strncmp(s, "\\??\\", 4))
        s += 4;
    o = 0;
    for (i = 0; s[i]; i++) {
        char ch = s[i] == '/' ? '\\' : s[i];
        if (ch == '\\' && o > 1 && dst[o - 1] == '\\')
            continue;
        if (o + 1 >= cap) {
            dst[0] = '\0';
            return 0;
        }
        dst[o++] = ch;
    }
    dst[o] = '\0';
    return 1;
}

static void child_load(struct child_ctx *c, int preload_fake)
{
    char got[MAX_PATHBUF], cg[MAX_PATHBUF], cw[MAX_PATHBUF];
    int ok;
    if (!SetCurrentDirectoryA(c->dir)) {
        CHECK(0, "setup: cannot CWD to scenario dir");
        return;
    }
    if (preload_fake) {
        c->hfake = LoadLibraryA(c->fake_path);
        CHECK(c->hfake != NULL, "setup: preload fake failed");
    }
    c->hshim = LoadLibraryA(c->shim_path);
    CHECK(c->hshim != NULL, "setup: LoadLibrary(shim copy) failed");
    if (!c->hshim)
        return;
    /* Isolation proof: the handle MUST be our staged copy. Both
     * spellings are canonicalized (the loader returns a normalized
     * path; ours may carry GetTempPathA's trailing separator or
     * short-name segments) and BOTH are printed on mismatch. */
    got[0] = '\0';
    GetModuleFileNameA(c->hshim, got, sizeof(got));
    ok = canon_path(cg, sizeof(cg), got) &&
         canon_path(cw, sizeof(cw), c->shim_path);
    CHECK(ok && _stricmp(cg, cw) == 0,
          "setup: shim module is not the staged copy (got=%s want=%s)",
          got, c->shim_path);
    c->p_swap = (swap_fn)GetProcAddress(c->hshim, "_grBufferSwap@4");
    c->p_pending =
        (pending_fn)GetProcAddress(c->hshim, "_grBufferNumPending@0");
    c->p_shutdown =
        (shutdown_fn)GetProcAddress(c->hshim, "_grGlideShutdown@0");
    CHECK(c->p_swap && c->p_pending && c->p_shutdown,
          "setup: shim copy lacks an intercepted export");
}

static void child_check_fake(struct child_ctx *c)
{
    char got[MAX_PATHBUF], cg[MAX_PATHBUF], cw[MAX_PATHBUF];
    int ok;
    /* The shim must have bound OUR staged fake (never anything else). */
    c->hfake = GetModuleHandleA("glide2x_gex_real.dll");
    CHECK(c->hfake != NULL, "fake: shim did not bind glide2x_gex_real");
    if (!c->hfake)
        return;
    got[0] = '\0';
    GetModuleFileNameA(c->hfake, got, sizeof(got));
    ok = canon_path(cg, sizeof(cg), got) &&
         canon_path(cw, sizeof(cw), c->fake_path);
    CHECK(ok && _stricmp(cg, cw) == 0,
          "fake: bound module is not the staged fake (got=%s want=%s)",
          got, c->fake_path);
    c->p_counts =
        (counts_fn)GetProcAddress(c->hfake, "_fakereal_counts@16");
    CHECK(c->p_counts != NULL, "fake: counts export missing");
}

static void child_actions(struct child_ctx *c)
{
    int i;
    for (i = 0; i < c->nswap && !child_fail[0]; i++)
        c->p_swap((int32_t)c->sarg);
    for (i = 0; i < c->npending && !child_fail[0]; i++)
        (void)c->p_pending();
    for (i = 0; i < c->nshut && !child_fail[0]; i++)
        c->p_shutdown();
}

/* Live + file counts must agree with the expected script. */
static void child_check_counts(struct child_ctx *c, int eswap, int epending,
                               int eshut, int earg)
{
    int s = -1, p = -1, h = -1, a = -2;
    int fs = -1, fp = -1, fh = -1, fa = -2;
    if (!c->p_counts) {
        CHECK(0, "counts: no live counts (setup failed?)");
        return;
    }
    c->p_counts(&s, &p, &h, &a);
    CHECK(s == eswap && p == epending && h == eshut && a == earg,
          "counts: live swap=%d pending=%d shutdown=%d last_arg=%d, want %d/%d/%d/%d",
          s, p, h, a, eswap, epending, eshut, earg);
    CHECK(parse_counts("forwarded.counts", &fs, &fp, &fh, &fa),
          "counts: forwarded.counts missing/unparseable");
    CHECK(fs == eswap && fp == epending && fh == eshut && fa == earg,
          "counts: file disagrees with expected script");
}

/* Full CSV audit: contiguous seq, all-digit non-decreasing ticks, event
 * tallies, single footer with exact counters. Returns data-row count. */
static unsigned long child_audit_csv(const char *id, unsigned long long exp_rows,
                                     unsigned long long exp_overflow,
                                     unsigned long long exp_swaps,
                                     unsigned long long exp_pcalls,
                                     long exp_parg, unsigned long exp_p,
                                     unsigned long exp_x)
{
    FILE *f = fopen("gshim_log.csv", "r");
    char line[MAX_LINE];
    unsigned long long seq, tick, frows, fov, fsw, fpc;
    unsigned long long prev_tick = 0;
    unsigned long n = 0, np = 0, nx = 0, nfoot = 0, nhead = 0;
    char ev, by[32];
    int32_t arg;
    int r, first = 1;
    if (!f) {
        CHECK(0, "%s: gshim_log.csv missing", id);
        return 0;
    }
    if (!read_line(f, line, sizeof(line))) {
        CHECK(0, "%s: csv empty", id);
        fclose(f);
        return 0;
    }
    CHECK(strncmp(line, "# gshim ", 8) == 0, "%s: header line bad: %.60s",
          id, line);
    CHECK(strstr(line, "buf=2048") != NULL,
          "%s: header not buf=2048 (MSVCRT refused setvbuf?): %.80s",
          id, line);
    nhead = 1;
    (void)nhead;
    while (read_line(f, line, sizeof(line))) {
        if (strncmp(line, "# end ", 6) == 0) {
            nfoot++;
            if (parse_footer(line, &frows, &fov, &fsw, &fpc, by,
                             sizeof(by))) {
                CHECK(frows == exp_rows && fov == exp_overflow &&
                      fsw == exp_swaps && fpc == exp_pcalls &&
                      strcmp(by, "shutdown") == 0,
                      "%s: footer counters off (want rows=%lu ov=%lu sw=%lu pc=%lu by=shutdown)",
                      id, (unsigned long)exp_rows,
                      (unsigned long)exp_overflow,
                      (unsigned long)exp_swaps,
                      (unsigned long)exp_pcalls);
            } else {
                CHECK(0, "%s: footer unparseable: %.80s", id, line);
            }
            continue;
        }
        r = parse_row(line, &seq, &tick, &ev, &arg);
        if (r == 0)
            continue; /* comment/blank (trip marker handled by caller) */
        if (r < 0) {
            CHECK(0, "%s: bad row #%lu: %.80s", id, n + 1, line);
            break;
        }
        n++;
        CHECK(seq == n, "%s: seq not contiguous at row %lu", id, n);
        if (n > 1)
            CHECK(tick >= prev_tick, "%s: tick decreases at row %lu",
                  id, n);
        prev_tick = tick;
        if (ev == 'P') {
            np++;
            if (exp_parg >= 0)
                CHECK(arg == exp_parg, "%s: P row arg=%d want %ld",
                      id, (int)arg, exp_parg);
        } else if (ev == 'X') {
            nx++;
            CHECK(arg == 0, "%s: X row arg=%d want 0", id, (int)arg);
        } else if (ev != 'S') {
            CHECK(0, "%s: unknown event '%c' at row %lu", id, ev, n);
        }
        if (first) {
            first = 0;
            CHECK(seq == 1 && ev == 'S',
                  "%s: first row is not S/seq1: %.60s", id, line);
        }
    }
    fclose(f);
    CHECK(nfoot == 1, "%s: want exactly 1 footer, got %lu", id, nfoot);
    CHECK(n == exp_rows, "%s: want %lu data rows, got %lu", id,
          (unsigned long)exp_rows, n);
    CHECK(np == exp_p, "%s: want %lu P rows, got %lu", id, exp_p, np);
    CHECK(nx == exp_x, "%s: want %lu X rows, got %lu", id, exp_x, nx);
    return n;
}

static int child_run(const char *id, const char *dir)
{
    struct child_ctx c;
    size_t dl;
    FILE *res;
    memset(&c, 0, sizeof(c));
    child_id = id;
    /* Stage paths (scenario dir + file names). */
    dl = strlen(dir);
    CHECK(dl + 32 < sizeof(c.dir), "setup: scenario path too long");
    if (child_fail[0])
        goto verdict;
    strcpy(c.dir, dir);
    CHECK(join_path(c.shim_path, sizeof(c.shim_path), dir,
                    "glide2x.dll") &&
          join_path(c.fake_path, sizeof(c.fake_path), dir,
                    "glide2x_gex_real.dll"),
          "setup: scenario path too long");
    if (child_fail[0])
        goto verdict;
    SetDllDirectoryA("");

    if (strcmp(id, "W00_loader") == 0) {
        long c1, c2;
        c.nswap = 1;
        c.npending = 1;
        c.nshut = 1;
        c.sarg = 1;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:9");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        child_actions(&c);
        /* AFTER the first call: the shim binds lazily via
         * ensure_init, so before any call NOTHING is bound
         * by design (V-2: checking earlier always fails).
         * The identity proof itself is unchanged (STRICT). */
        child_check_fake(&c);
        child_check_counts(&c, 1, 1, 1, 1);
        c1 = file_size("gshim_log.csv");
        CHECK(c1 > 0, "W00: csv empty before DETACH");
        FreeLibrary(c.hshim);
        c.hshim = NULL;
        c2 = file_size("gshim_log.csv");
        CHECK(c1 == c2, "W00: DETACH changed the csv (%ld -> %ld)",
              c1, c2);
        CHECK(!file_exists("gshim_error.txt"), "W00: error file present");
    } else if (strcmp(id, "W01_exact") == 0) {
        c.nswap = 3000;
        c.npending = 5000;
        c.nshut = 1;
        c.sarg = 3;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:7");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 3000, 5000, 1, 3);
        child_audit_csv(id, 3003, 0, 3000, 5000, 7, 2, 1);
        CHECK(!file_exists("gshim_error.txt"), "W01: error file present");
    } else if (strcmp(id, "W02_alt") == 0) {
        c.nswap = 3000;
        c.npending = 5000;
        c.nshut = 1;
        c.sarg = 3;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "alt");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 3000, 5000, 1, 3);
        child_audit_csv(id, 8001, 0, 3000, 5000, -1, 5000, 1);
        CHECK(!file_exists("gshim_error.txt"), "W02: error file present");
    } else if (strcmp(id, "W03_overflow") == 0) {
        FILE *f;
        char line[MAX_LINE];
        int trips = 0;
        c.nswap = 300000;
        c.npending = 0;
        c.nshut = 1;
        c.sarg = 5;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:0");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 300000, 0, 1, 5);
        child_audit_csv(id, 262144, 1, 300000, 0, -1, 0, 0);
        f = fopen("gshim_log.csv", "r");
        CHECK(f != NULL, "W03: csv missing for marker scan");
        if (f) {
            while (read_line(f, line, sizeof(line))) {
                if (strncmp(line, "# overflow at seq=262144: RUN INVALID",
                             38) == 0)
                    trips++;
            }
            fclose(f);
        }
        CHECK(trips == 1, "W03: want 1 trip marker, got %d", trips);
        CHECK(file_contains("gshim_error.txt", "ring overflow"),
              "W03: error file lacks overflow note");
    } else if (strcmp(id, "W04_nolog") == 0) {
        FILE *f;
        char line[MAX_LINE];
        int n = 0;
        char l1[MAX_LINE], l2[MAX_LINE];
        c.nswap = 1000;
        c.npending = 2000;
        c.nshut = 1;
        c.sarg = 4;
        SetEnvironmentVariableA("GSHIM_NOLOG", "1");
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:2");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 1000, 2000, 1, 4);
        CHECK(!file_exists("gshim_log.csv"), "W04: csv must be absent");
        f = fopen("gshim_nolog.marker", "r");
        CHECK(f != NULL, "W04: marker missing");
        l1[0] = l2[0] = '\0';
        if (f) {
            while (read_line(f, line, sizeof(line))) {
                n++;
                if (n == 1)
                    strcpy(l1, line);
                else if (n == 2)
                    strcpy(l2, line);
            }
            fclose(f);
        }
        CHECK(n == 2, "W04: marker wants 2 lines, got %d", n);
        CHECK(strstr(l1, "nolog=1 qpf=") != NULL,
              "W04: marker init line bad: %.60s", l1);
        CHECK(strstr(l2, "end swaps=1000 pending_calls=2000 by=shutdown")
              != NULL, "W04: marker end line bad: %.60s", l2);
        CHECK(!file_exists("gshim_error.txt"), "W04: error file present");
    } else if (strcmp(id, "W05_markerfail") == 0) {
        c.nswap = 100;
        c.npending = 50;
        c.nshut = 1;
        c.sarg = 6;
        SetEnvironmentVariableA("GSHIM_NOLOG", "1");
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:1");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        if (!CreateDirectoryA("gshim_nolog.marker", NULL) &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            CHECK(0, "W05: cannot stage blocking dir");
            goto verdict;
        }
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 100, 50, 1, 6);
        CHECK(file_contains("gshim_error.txt", "marker init failed"),
              "W05: error lacks marker-init note");
        CHECK(file_contains("gshim_error.txt", "marker end failed"),
              "W05: error lacks marker-end note");
        CHECK(!file_exists("gshim_log.csv"), "W05: csv must be absent");
    } else if (strcmp(id, "W06_swap") == 0 ||
               strcmp(id, "W06_pending") == 0 ||
               strcmp(id, "W06_shutdown") == 0) {
        /* Resolution failure: the swap below must fail_fast (exit 111).
         * Reaching the verdict line means init wrongly succeeded. */
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:0");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        c.nswap = 1;
        child_actions(&c);
        CHECK(0, "%s: init succeeded with a missing export (want 111)",
              id);
    } else if (strcmp(id, "W07_loadfail") == 0) {
        /* No fake staged: init must fail_fast (exit 111). */
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:0");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        c.nswap = 1;
        child_actions(&c);
        CHECK(0, "W07: init succeeded with no real DLL (want 111)");
    } else if (strcmp(id, "W08_noshutdown") == 0) {
        int i;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:0");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        for (i = 0; i < 5000; i++)
            c.p_swap(3);
        fflush(stdout);
        /* Crash: no shutdown, no footer (parent checks). ExitProcess
         * skips CRT stdio flush, like the Linux suite's _exit(42). */
        ExitProcess(42);
    } else if (strcmp(id, "W09_double") == 0) {
        c.nswap = 100;
        c.npending = 0;
        c.nshut = 2;
        c.sarg = 8;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:0");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 100, 0, 2, 8);
        child_audit_csv(id, 101, 0, 100, 0, -1, 0, 1);
        CHECK(!file_exists("gshim_error.txt"), "W09: error file present");
    } else if (strcmp(id, "W10_logblocked") == 0) {
        c.nswap = 200;
        c.npending = 0;
        c.nshut = 1;
        c.sarg = 2;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:0");
        child_load(&c, 0);
        if (child_fail[0])
            goto verdict;
        if (!CreateDirectoryA("gshim_log.csv", NULL) &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            CHECK(0, "W10: cannot stage blocking dir");
            goto verdict;
        }
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 200, 0, 1, 2);
        CHECK(file_contains("gshim_error.txt", "cannot open log"),
              "W10: error lacks cannot-open note");
        CHECK(file_contains("gshim_error.txt", "rows lost"),
              "W10: error lacks rows-lost note");
    } else if (strcmp(id, "W11_prebound") == 0) {
        c.nswap = 50;
        c.npending = 0;
        c.nshut = 1;
        c.sarg = 7;
        SetEnvironmentVariableA("GSHIM_FAKE_PENDING", "const:0");
        child_load(&c, 1); /* fake already bound: fast path */
        if (child_fail[0])
            goto verdict;
        child_actions(&c);
        child_check_fake(&c);
        child_check_counts(&c, 50, 0, 1, 7);
        child_audit_csv(id, 51, 0, 50, 0, -1, 0, 1);
        CHECK(!file_exists("gshim_error.txt"), "W11: error file present");
    } else {
        CHECK(0, "unknown scenario");
    }

verdict:
    if (c.hshim)
        FreeLibrary(c.hshim);
    res = fopen("result.txt", "w");
    if (res) {
        if (child_fail[0])
            fprintf(res, "FAIL %s: %s\n", id, child_fail);
        else
            fprintf(res, "PASS %s\n", id);
        fclose(res);
    }
    if (child_fail[0])
        printf("child %s: FAIL %s\n", id, child_fail);
    else
        printf("child %s: PASS\n", id);
    return child_fail[0] ? 1 : 0;
}

/* ---- parent: spawn + verdict per scenario ---- */

/* fake: 0 full, 1 noswap, 2 nopending, 3 noshutdown, 4 none.
 * post: 0 child verdict (result.txt), 1 parent post-mortem check. */
static const struct {
    const char *id;
    int fake;
    int expect;
    int post;
} scenarios[] = {
    {"W00_loader", 0, 0, 0},
    {"W01_exact", 0, 0, 0},
    {"W02_alt", 0, 0, 0},
    {"W03_overflow", 0, 0, 0},
    {"W04_nolog", 0, 0, 0},
    {"W05_markerfail", 0, 0, 0},
    {"W06_swap", 1, 111, 1},
    {"W06_pending", 2, 111, 1},
    {"W06_shutdown", 3, 111, 1},
    {"W07_loadfail", 4, 111, 1},
    {"W08_noshutdown", 0, 42, 1},
    {"W09_double", 0, 0, 0},
    {"W10_logblocked", 0, 0, 0},
    {"W11_prebound", 0, 0, 0},
};

static const char *fake_name(int fake)
{
    switch (fake) {
    case 0:
        return "fake_full.dll";
    case 1:
        return "fake_noswap.dll";
    case 2:
        return "fake_nopending.dll";
    case 3:
        return "fake_noshutdown.dll";
    default:
        return NULL;
    }
}

static const char *w06_symbol(const char *id)
{
    if (strcmp(id, "W06_swap") == 0)
        return "_grBufferSwap@4";
    if (strcmp(id, "W06_pending") == 0)
        return "_grBufferNumPending@0";
    if (strcmp(id, "W06_shutdown") == 0)
        return "_grGlideShutdown@0";
    return "?";
}

/* Parent post-mortem checks (child is dead: 111 or 42). CWD = scenario. */
static int parent_postcheck(const char *id, char *why, size_t whycap)
{
    why[0] = '\0';
    if (strncmp(id, "W06_", 4) == 0 || strcmp(id, "W07_loadfail") == 0) {
        const char *sym = strncmp(id, "W06_", 4) == 0 ?
            w06_symbol(id) : "_grBufferSwap@4";
        char want[128];
        snprintf(want, sizeof(want), "cannot resolve %s", sym);
        if (!file_contains("gshim_error.txt", "FATAL (exit 111)")) {
            snprintf(why, whycap, "error file lacks FATAL (exit 111)");
            return 0;
        }
        if (!file_contains("gshim_error.txt", want)) {
            snprintf(why, whycap, "error file does not name %s", sym);
            return 0;
        }
        return 1;
    }
    if (strcmp(id, "W08_noshutdown") == 0) {
        FILE *f = fopen("gshim_log.csv", "r");
        char line[MAX_LINE];
        unsigned long long seq, tick;
        char ev;
        int32_t arg;
        unsigned long n = 0;
        int head_ok = 0, foot = 0;
        if (!f) {
            snprintf(why, whycap, "csv missing");
            return 0;
        }
        if (read_line(f, line, sizeof(line)) &&
            strncmp(line, "# gshim ", 8) == 0 &&
            strstr(line, "buf=2048"))
            head_ok = 1;
        while (read_line(f, line, sizeof(line))) {
            int r;
            if (strncmp(line, "# end ", 6) == 0) {
                foot = 1;
                continue;
            }
            r = parse_row(line, &seq, &tick, &ev, &arg);
            if (r > 0)
                n++;
        }
        fclose(f);
        if (!head_ok) {
            snprintf(why, whycap, "header missing/not buf=2048");
            return 0;
        }
        if (foot) {
            snprintf(why, whycap, "footer must be absent");
            return 0;
        }
        if (n < 4700) {
            snprintf(why, whycap, "only %lu/5000 rows on disk", n);
            return 0;
        }
        return 1;
    }
    snprintf(why, whycap, "no postcheck for %s", id);
    return 0;
}

static int run_one(const char *self, const char *rundir, const char *builddir,
                   const char *id, int fake, int expect, int post)
{
    char dir[MAX_PATHBUF], src[MAX_PATHBUF], dst[MAX_PATHBUF];
    char cmd[4096];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD rc, code = 0;
    char why[512];
    FILE *res;
    char line[MAX_LINE];
    int ok;

    if (!join_path(dir, sizeof(dir), rundir, id)) {
        printf("FAIL %s: scenario path too long\n", id);
        return 0;
    }
    if (!CreateDirectoryA(dir, NULL)) {
        printf("FAIL %s: cannot create scenario dir\n", id);
        return 0;
    }
    if (!join_path(src, sizeof(src), builddir, "glide2x.dll") ||
        !join_path(dst, sizeof(dst), dir, "glide2x.dll")) {
        printf("FAIL %s: shim copy path too long\n", id);
        return 0;
    }
    if (!CopyFileA(src, dst, FALSE)) {
        printf("FAIL %s: cannot stage shim copy\n", id);
        return 0;
    }
    if (fake_name(fake)) {
        if (!join_path(src, sizeof(src), builddir, fake_name(fake)) ||
            !join_path(dst, sizeof(dst), dir, "glide2x_gex_real.dll")) {
            printf("FAIL %s: fake path too long\n", id);
            return 0;
        }
        if (!CopyFileA(src, dst, FALSE)) {
            printf("FAIL %s: cannot stage fake\n", id);
            return 0;
        }
    }
    {
        int n = snprintf(cmd, sizeof(cmd), "\"%s\" child %s \"%s\"", self,
                         id, dir);
        if (n < 0 || n >= (int)sizeof(cmd)) {
            printf("FAIL %s: command line too long\n", id);
            return 0;
        }
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(self, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si,
                        &pi)) {
        printf("FAIL %s: CreateProcess failed (%lu)\n", id,
               (unsigned long)GetLastError());
        return 0;
    }
    rc = WaitForSingleObject(pi.hProcess, TIMEOUT_MS);
    if (rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 3);
        WaitForSingleObject(pi.hProcess, 5000);
        printf("FAIL %s: timeout %ds\n", id, TIMEOUT_MS / 1000);
        ok = 0;
        goto done;
    }
    if (!GetExitCodeProcess(pi.hProcess, &code)) {
        printf("FAIL %s: cannot read exit code\n", id);
        ok = 0;
        goto done;
    }
    if ((int)code != expect) {
        const char *extra = "";
        if (code == 99)
            extra = " (finalize-before-forward violated)";
        else if (code == 111)
            extra = " (unexpected fail_fast)";
        printf("FAIL %s: exit %lu, want %d%s\n", id,
               (unsigned long)code, expect, extra);
        if (!join_path(dst, sizeof(dst), dir, "result.txt"))
            dst[0] = '\0';
        res = dst[0] ? fopen(dst, "r") : NULL;
        if (res) {
            while (read_line(res, line, sizeof(line)))
                printf("  | %s\n", line);
            fclose(res);
        }
        ok = 0;
        goto done;
    }
    if (post) {
        char cwd[MAX_PATHBUF];
        GetCurrentDirectoryA(sizeof(cwd), cwd);
        SetCurrentDirectoryA(dir);
        ok = parent_postcheck(id, why, sizeof(why));
        SetCurrentDirectoryA(cwd);
        if (!ok)
            printf("FAIL %s: %s\n", id, why);
        else
            printf("PASS %s\n", id);
        goto done;
    }
    if (!join_path(dst, sizeof(dst), dir, "result.txt")) {
        printf("FAIL %s: result path too long\n", id);
        ok = 0;
        goto done;
    }
    res = fopen(dst, "r");
    if (!res) {
        printf("FAIL %s: exit 0 but no result.txt\n", id);
        ok = 0;
        goto done;
    }
    line[0] = '\0';
    read_line(res, line, sizeof(line));
    fclose(res);
    if (strncmp(line, "PASS ", 5) == 0) {
        printf("PASS %s\n", id);
        ok = 1;
    } else {
        printf("FAIL %s: %s\n", id, line[0] ? line : "(empty result)");
        ok = 0;
    }
done:
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return ok;
}

static int parent_run(void)
{
    char self[MAX_PATHBUF], builddir[MAX_PATHBUF], rundir[MAX_PATHBUF];
    char tmp[MAX_PATHBUF], *bs;
    size_t i, n = sizeof(scenarios) / sizeof(scenarios[0]);
    int pass = 0, fail = 0;

    if (!GetModuleFileNameA(NULL, self, sizeof(self))) {
        printf("parent: cannot locate self\n");
        return 1;
    }
    strcpy(builddir, self);
    bs = strrchr(builddir, '\\');
    if (bs)
        *bs = '\0';
    if (!GetTempPathA(sizeof(tmp), tmp)) {
        printf("parent: cannot locate temp dir\n");
        return 1;
    }
    {
        char leaf[32];
        snprintf(leaf, sizeof(leaf), "gshim_w32_%lu",
                 (unsigned long)GetCurrentProcessId());
        if (!join_path(rundir, sizeof(rundir), tmp, leaf)) {
            printf("parent: run path too long\n");
            return 1;
        }
    }
    if (!CreateDirectoryA(rundir, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        printf("parent: cannot create run dir\n");
        return 1;
    }
    printf("W32HARNESS run dir: %s\n", rundir);
    for (i = 0; i < n; i++) {
        if (run_one(self, rundir, builddir, scenarios[i].id,
                    scenarios[i].fake, scenarios[i].expect,
                    scenarios[i].post))
            pass++;
        else
            fail++;
    }
    printf("W32HARNESS: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc == 1 ||
        (argc == 2 && strcmp(argv[1], "--run-all") == 0))
        return parent_run();
    if (argc == 4 && strcmp(argv[1], "child") == 0)
        return child_run(argv[2], argv[3]);
    fprintf(stderr, "usage: %s [--run-all] | child <id> <dir>\n", argv[0]);
    return 2;
}
