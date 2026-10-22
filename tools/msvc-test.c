/**
 * Test de compilación con cl.exe moderno (VS 2022) para objdiff
 * Flags: /Od /Ob0 /Oi- /Oy- /Gd /MT /Gy /Z7
 */
#include <windows.h>

__declspec(dllexport) int __cdecl add_int(int a, int b) {
    return a + b;
}

__declspec(dllexport) DWORD __cdecl get_tick_count_wrapper(void) {
    return GetTickCount();
}

int g_test_value = 0x12345678;

__declspec(dllexport) float __cdecl float_mul(float a, float b) {
    return a * b;
}