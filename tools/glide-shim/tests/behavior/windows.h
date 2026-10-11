/* FUNCTIONAL Win32 emulation for the behavioural harness.
 * Same API surface gshim.c uses, but implemented (harness_impl.c) with
 * controllable fakes: scripted QPC, env passthrough, fake module/proc
 * handles, ExitProcess that really exits (child-process assertions).
 * Used ONLY when compiling the Linux behaviour test (-I tests/behavior);
 * the real MinGW build uses the SDK headers. Never shipped. */
#ifndef W32HARNESS_H
#define W32HARNESS_H
typedef void *HMODULE;
typedef unsigned long DWORD;
typedef void *LPVOID;
typedef int BOOL;
typedef long LONG;
typedef const char *LPCSTR;
#define TRUE 1
#define FALSE 0
/* Win32 calling-convention/declspec spellings. A real Win32 target
 * ALREADY provides them: GCC predefines __stdcall as
 * __attribute__((__stdcall__)) (gcc/config/i386/cygming.h), clang
 * additionally predefines __declspec, and mingw-w64's _mingw.h only
 * fills __declspec in when absent (#ifdef-guarded). Redefining them
 * here would emit -Wmacro-redefined and silently downgrade the
 * convention to cdecl (MSYS2 MINGW32 PC run, 85d72d2). Guarded, the
 * target's own definition wins there, and on the Linux host -- where
 * nothing defines them -- this stub stays the single source. */
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef __stdcall
#define __stdcall
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
