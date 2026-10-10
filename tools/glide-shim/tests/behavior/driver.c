/* Behaviour driver: exercises the REAL gshim.c against harness fakes.
 * Usage: behavior_test <nswap> <npending> <shutdowns> <crash01>
 *                       <detach01> [swap_arg]
 * Calls DllMain ATTACH, loops the wrappers, optionally shuts down
 * (xN), optionally crashes via _exit(42) (no shutdown), optionally
 * calls DETACH and reports file sizes around it (DETACH must be a
 * strict no-op on our files).
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void __stdcall grBufferSwap(int32_t swap_interval);
int32_t __stdcall grBufferNumPending(void);
void __stdcall grGlideShutdown(void);
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID r);

static long fsize(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 ? (long)st.st_size : -1L;
}

int main(int argc, char **argv)
{
    long nswap, npending, nshut, i;
    int crash, detach, sarg;
    if (argc < 6) {
        fprintf(stderr, "usage: %s nswap npending shutdowns crash01 detach01 [swap_arg]\n",
                argv[0]);
        return 2;
    }
    nswap = atol(argv[1]);
    npending = atol(argv[2]);
    nshut = atol(argv[3]);
    crash = atoi(argv[4]);
    detach = atoi(argv[5]);
    sarg = argc > 6 ? atoi(argv[6]) : 3;

    DllMain((HMODULE)1, DLL_PROCESS_ATTACH, NULL);
    for (i = 0; i < nswap; i++)
        grBufferSwap((int32_t)sarg);
    for (i = 0; i < npending; i++)
        (void)grBufferNumPending();
    for (i = 0; i < nshut; i++)
        grGlideShutdown();
    if (crash) {
        fflush(stdout);
        _exit(42);
    }
    if (detach) {
        long c1 = fsize("gshim_log.csv"), m1 = fsize("gshim_nolog.marker");
        DllMain((HMODULE)1, DLL_PROCESS_DETACH, NULL);
        printf("detach csv=%ld/%ld marker=%ld/%ld\n",
               c1, fsize("gshim_log.csv"), m1, fsize("gshim_nolog.marker"));
    }
    return 0;
}
