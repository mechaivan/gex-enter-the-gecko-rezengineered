/*
 * gex/input/key_state.h - platform-independent keyboard state (I-4).
 *
 * The Win32 window feeds this module with virtual-key codes from
 * WM_KEYDOWN/WM_KEYUP. Nothing here knows about Windows: the tests drive the
 * same transitions without a window.
 *
 * Two kinds of state are kept apart on purpose:
 *
 *   down[]      current level: the key is held right now.
 *   pressed[]   edge: the key went down during this frame.
 *   released[]  edge: the key went up during this frame.
 *
 * Edges are cleared by gex_input_begin_frame() at the end of a frame, so the
 * hooks of gex_frame_run() can query them while the frame is being processed.
 * Auto-repeat (`WM_KEYDOWN` while the key is already down) does not create a
 * second edge: a held key is pressed once, not once per repeat.
 *
 * Focus loss releases every key that is currently down. A game that keeps
 * running in the background must not leave W or an arrow "stuck" because the
 * WM_KEYUP went to another window.
 */

#ifndef GEX_INPUT_KEY_STATE_H
#define GEX_INPUT_KEY_STATE_H

#include <stdint.h>

/* Virtual-key codes are 8-bit in the Win32 message contract. */
#define GEX_KEY_COUNT 256

/* The only key the I-4 application interprets; value from the Win32 VK map. */
#define GEX_KEY_ESCAPE 0x1B

typedef struct gex_input {
    uint8_t down[GEX_KEY_COUNT];
    uint8_t pressed[GEX_KEY_COUNT];
    uint8_t released[GEX_KEY_COUNT];
    /* counters, never reset except by gex_input_init() */
    uint64_t presses;      /* edges produced by real key-down transitions */
    uint64_t releases;     /* edges produced by key-up or focus loss */
    uint64_t focus_losses; /* WM_KILLFOCUS-equivalent transitions */
} gex_input;

/* Zero every table and counter. */
void gex_input_init(gex_input *input);

/* A key went down. A repeated down for a held key is ignored. */
void gex_input_key_down(gex_input *input, int vk);

/* A key went up. A key that was not down produces no released edge. */
void gex_input_key_up(gex_input *input, int vk);

/* Release every held key and make those releases visible this frame. */
void gex_input_focus_lost(gex_input *input);

/* Clear pressed[]/released[] after a frame has consumed them. Down[] stays. */
void gex_input_begin_frame(gex_input *input);

int gex_input_is_down(const gex_input *input, int vk);
int gex_input_was_pressed(const gex_input *input, int vk);
int gex_input_was_released(const gex_input *input, int vk);

#endif /* GEX_INPUT_KEY_STATE_H */
