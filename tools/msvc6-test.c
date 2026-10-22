/**
 * Test de compilación MSVC 6.0 → .obj COFF i386
 * Debe compilar con: cl.exe /nologo /c /Od /Ob0 /Oi- /Oy- /Gd /MT /Gy /Fo test.obj test.c
 */
#include <windows.h>

/* Función simple que usa tipos y calling convention estándar */
__declspec(dllexport) int __cdecl add_int(int a, int b) {
    return a + b;
}

/* Función que llama a API importada (simula lo que haría el juego) */
__declspec(dllexport) DWORD __cdecl get_tick_count_wrapper(void) {
    return GetTickCount();
}

/* Variable global (termina en .data/.bss del .obj) */
int g_test_value = 0x12345678;

/* Función con float (ejercita emulación FPU / libreria runtime) */
__declspec(dllexport) float __cdecl float_mul(float a, float b) {
    return a * b;
}