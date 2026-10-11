/* Minimal windows.h stub ONLY for Linux syntax-checking of gshim.c.
 * Declares just the API surface gshim.c uses; the real build uses the
 * MinGW SDK headers. Never shipped, never linked. */
#ifndef W32STUB_H
#define W32STUB_H
typedef void *HMODULE;
typedef unsigned long DWORD;
typedef void *LPVOID;
typedef int BOOL;
typedef long LONG;
typedef const char *LPCSTR;
#define TRUE 1
#define FALSE 0
/* Guarded: a real Win32 compiler already predefines these (GCC:
 * gcc/config/i386/cygming.h; the PC's stage-7 log showed the
 * redefinition warning). The guard keeps the toolchain's own
 * definition where it exists and stays self-contained on Linux. */
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#define MAX_PATH 260
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
typedef struct { long long QuadPart; } LARGE_INTEGER;
void DisableThreadLibraryCalls(HMODULE);
int QueryPerformanceFrequency(LARGE_INTEGER *);
int QueryPerformanceCounter(LARGE_INTEGER *);
DWORD GetModuleFileNameA(HMODULE, char *, DWORD);
HMODULE GetModuleHandleA(LPCSTR);
HMODULE LoadLibraryA(LPCSTR);
void *GetProcAddress(HMODULE, LPCSTR);
DWORD GetEnvironmentVariableA(LPCSTR, char *, DWORD);
void OutputDebugStringA(LPCSTR);
void ExitProcess(DWORD);
long InterlockedCompareExchange(volatile long *, long, long);
#endif
