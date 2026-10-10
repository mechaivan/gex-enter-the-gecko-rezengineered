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
#define APIENTRY
#define MAX_PATH 260
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define __stdcall
#define __declspec(x)
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
