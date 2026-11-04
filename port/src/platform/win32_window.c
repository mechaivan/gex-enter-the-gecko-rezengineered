/*
 * win32_window.c - I-4 window, message pump and keyboard routing.
 *
 * The window draws nothing. WM_ERASEBKGND returns 1 (handled, no paint) and
 * WM_PAINT only validates the update region; the null renderer is not a
 * graphics backend.
 */

#include "gex/platform/win32_window.h"

#include <stddef.h>

static int g_class_registered;

static gex_win32_window *window_from(HWND hwnd)
{
    return (gex_win32_window *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam)
{
    gex_win32_window *window = window_from(hwnd);

    switch (message) {
    case WM_NCCREATE: {
        const CREATESTRUCTW *create = (const CREATESTRUCTW *)lparam;
        window = (gex_win32_window *)create->lpCreateParams;
        if (window != NULL) {
            window->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (window != NULL && window->input != NULL) {
            gex_input_key_down(window->input, (int)wparam);
        }
        /* System keys still need DefWindowProc for Alt+F4 and the menu. */
        return (message == WM_SYSKEYDOWN)
                   ? DefWindowProcW(hwnd, message, wparam, lparam)
                   : 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (window != NULL && window->input != NULL) {
            gex_input_key_up(window->input, (int)wparam);
        }
        return (message == WM_SYSKEYUP)
                   ? DefWindowProcW(hwnd, message, wparam, lparam)
                   : 0;
    case WM_KILLFOCUS:
        if (window != NULL && window->input != NULL) {
            gex_input_focus_lost(window->input);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (window != NULL) {
            window->quit = 1;
            window->hwnd = NULL;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1; /* nothing is drawn, not even a background */
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

int gex_win32_window_create(gex_win32_window *window, gex_input *input,
                            const wchar_t *title, int width, int height)
{
    WNDCLASSEXW wc;
    RECT rect;
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD error;

    if (window == NULL) {
        return 0;
    }
    window->instance = GetModuleHandleW(NULL);
    window->hwnd = NULL;
    window->input = input;
    window->quit = 0;
    window->created = 0;

    if (width <= 0 || height <= 0) {
        return 0;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = window->instance;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = GEX_WINDOW_CLASS_NAME;

    if (!g_class_registered) {
        if (!RegisterClassExW(&wc)) {
            error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                return 0;
            }
        }
        g_class_registered = 1;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;
    if (!AdjustWindowRect(&rect, style, FALSE)) {
        return 0;
    }

    window->hwnd = CreateWindowExW(
        0, GEX_WINDOW_CLASS_NAME, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, NULL, NULL,
        window->instance, window);
    if (window->hwnd == NULL) {
        return 0;
    }

    window->created = 1;
    ShowWindow(window->hwnd, SW_SHOW);
    UpdateWindow(window->hwnd);
    return 1;
}

void gex_win32_window_poll(gex_win32_window *window)
{
    MSG msg;

    if (window == NULL || !window->created) {
        return;
    }
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            window->quit = 1;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

int gex_win32_window_should_quit(const gex_win32_window *window)
{
    return (window != NULL && window->quit) ? 1 : 0;
}

void gex_win32_window_destroy(gex_win32_window *window)
{
    if (window == NULL) {
        return;
    }
    if (window->hwnd != NULL && IsWindow(window->hwnd)) {
        DestroyWindow(window->hwnd);
    }
    window->hwnd = NULL;
    window->created = 0;
    if (g_class_registered) {
        UnregisterClassW(GEX_WINDOW_CLASS_NAME, window->instance);
        g_class_registered = 0;
    }
}
