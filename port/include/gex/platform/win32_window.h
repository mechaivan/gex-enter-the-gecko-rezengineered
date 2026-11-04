/*
 * gex/platform/win32_window.h - the I-4 Win32 window and message pump.
 *
 * This is the only place where the application touches HWNDs, window classes
 * and the Win32 message queue. The rest of the port (core, input tables,
 * renderer, app core) stays platform independent.
 *
 * Messages routed into gex_input:
 *   WM_KEYDOWN / WM_SYSKEYDOWN   key down (auto-repeat is coalesced there)
 *   WM_KEYUP   / WM_SYSKEYUP     key up
 *   WM_KILLFOCUS                 every held key is released
 *
 * WM_CLOSE destroys the window and WM_DESTROY marks the window as finished,
 * so both the title-bar close button and Alt+F4 end the loop cleanly.
 */

#ifndef GEX_PLATFORM_WIN32_WINDOW_H
#define GEX_PLATFORM_WIN32_WINDOW_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "gex/input/key_state.h"

#define GEX_WINDOW_CLASS_NAME L"GexNativeLoopWindow"

typedef struct gex_win32_window {
    HINSTANCE instance;
    HWND hwnd;
    gex_input *input; /* borrowed; owned by the application core */
    int quit;
    int created;
} gex_win32_window;

/*
 * Create and show a real top-level window. Returns 1 on success, 0 on any
 * failure (the caller reports it and exits). `input` may be NULL, in which
 * case keyboard messages are ignored.
 */
int gex_win32_window_create(gex_win32_window *window, gex_input *input,
                            const wchar_t *title, int width, int height);

/* Dispatch every message currently queued for this thread. */
void gex_win32_window_poll(gex_win32_window *window);

int gex_win32_window_should_quit(const gex_win32_window *window);

/* Destroy the window and unregister the class. Idempotent. */
void gex_win32_window_destroy(gex_win32_window *window);

#endif /* GEX_PLATFORM_WIN32_WINDOW_H */
