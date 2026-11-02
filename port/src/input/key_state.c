/*
 * key_state.c - keyboard level and edge state (I-4).
 *
 * Pure C11, no Windows headers. The window layer translates messages into
 * these four calls; the frame hooks read the result.
 */

#include "gex/input/key_state.h"

#include <string.h>

static int valid_vk(int vk)
{
    return vk >= 0 && vk < GEX_KEY_COUNT;
}

void gex_input_init(gex_input *input)
{
    if (input == NULL) {
        return;
    }
    memset(input, 0, sizeof(*input));
}

void gex_input_key_down(gex_input *input, int vk)
{
    if (input == NULL || !valid_vk(vk)) {
        return;
    }
    if (input->down[vk]) {
        return; /* auto-repeat, not a new edge */
    }
    input->down[vk] = 1;
    input->pressed[vk] = 1;
    input->presses++;
}

void gex_input_key_up(gex_input *input, int vk)
{
    if (input == NULL || !valid_vk(vk)) {
        return;
    }
    if (!input->down[vk]) {
        return; /* duplicate up, or a key that was already released */
    }
    input->down[vk] = 0;
    input->released[vk] = 1;
    input->releases++;
}

void gex_input_focus_lost(gex_input *input)
{
    int vk;

    if (input == NULL) {
        return;
    }
    input->focus_losses++;
    for (vk = 0; vk < GEX_KEY_COUNT; vk++) {
        if (input->down[vk]) {
            input->down[vk] = 0;
            input->released[vk] = 1;
            input->releases++;
        }
    }
}

void gex_input_begin_frame(gex_input *input)
{
    if (input == NULL) {
        return;
    }
    memset(input->pressed, 0, sizeof(input->pressed));
    memset(input->released, 0, sizeof(input->released));
}

int gex_input_is_down(const gex_input *input, int vk)
{
    if (input == NULL || !valid_vk(vk)) {
        return 0;
    }
    return input->down[vk] ? 1 : 0;
}

int gex_input_was_pressed(const gex_input *input, int vk)
{
    if (input == NULL || !valid_vk(vk)) {
        return 0;
    }
    return input->pressed[vk] ? 1 : 0;
}

int gex_input_was_released(const gex_input *input, int vk)
{
    if (input == NULL || !valid_vk(vk)) {
        return 0;
    }
    return input->released[vk] ? 1 : 0;
}
