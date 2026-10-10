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
#define APIENTRY
#define MAX_PATH 260
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define __stdcall __attribute__((stdcall))
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
