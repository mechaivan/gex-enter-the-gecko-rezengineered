/*
 * test_input.c - keyboard state checks for I-4.
 *
 * No window and no Windows headers: the same transitions the WndProc sends
 * are applied directly, so edge behaviour is deterministic in the suite.
 */

#include "gex/input/key_state.h"

#include <stdio.h>
#include <string.h>

/* Win32 virtual-key values used as ordinary bytes; no windows.h needed. */
#define TEST_VK_LEFT 0x25
#define TEST_VK_SHIFT 0x10

static int checks;
static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

/* T1: a fresh table has no levels, no edges and no counters. */
static void test_init(void)
{
    gex_input input;
    int vk;

    memset(&input, 0xA5, sizeof(input));
    gex_input_init(&input);
    CHECK(input.presses == 0);
    CHECK(input.releases == 0);
    CHECK(input.focus_losses == 0);
    for (vk = 0; vk < GEX_KEY_COUNT; vk++) {
        CHECK(input.down[vk] == 0);
        CHECK(input.pressed[vk] == 0);
        CHECK(input.released[vk] == 0);
    }
    CHECK(gex_input_is_down(&input, 'W') == 0);
    CHECK(gex_input_was_pressed(&input, 'W') == 0);
    CHECK(gex_input_was_released(&input, 'W') == 0);
}

/* T2: down creates one edge; auto-repeat does not create another. */
static void test_press_and_repeat(void)
{
    gex_input input;

    gex_input_init(&input);
    gex_input_key_down(&input, 'W');
    CHECK(gex_input_is_down(&input, 'W') == 1);
    CHECK(gex_input_was_pressed(&input, 'W') == 1);
    CHECK(input.presses == 1);

    /* WM_KEYDOWN auto-repeat while the key is held. */
    gex_input_key_down(&input, 'W');
    gex_input_key_down(&input, 'W');
    CHECK(input.presses == 1);
    CHECK(gex_input_was_pressed(&input, 'W') == 1);

    gex_input_begin_frame(&input);
    CHECK(gex_input_is_down(&input, 'W') == 1);
    CHECK(gex_input_was_pressed(&input, 'W') == 0);
    CHECK(input.presses == 1);
}

/* T3: release creates one edge; a duplicate release is ignored. */
static void test_release(void)
{
    gex_input input;

    gex_input_init(&input);
    gex_input_key_down(&input, 'A');
    gex_input_begin_frame(&input);

    gex_input_key_up(&input, 'A');
    CHECK(gex_input_is_down(&input, 'A') == 0);
    CHECK(gex_input_was_released(&input, 'A') == 1);
    CHECK(input.releases == 1);

    gex_input_key_up(&input, 'A');
    CHECK(input.releases == 1);

    gex_input_begin_frame(&input);
    CHECK(gex_input_was_released(&input, 'A') == 0);
}

/* T4: several keys can transition in the same frame. */
static void test_same_frame(void)
{
    gex_input input;

    gex_input_init(&input);
    gex_input_key_down(&input, 'A');
    gex_input_key_down(&input, TEST_VK_LEFT);
    gex_input_key_up(&input, 'A');

    CHECK(gex_input_was_pressed(&input, 'A') == 1);
    CHECK(gex_input_was_pressed(&input, TEST_VK_LEFT) == 1);
    CHECK(gex_input_was_released(&input, 'A') == 1);
    CHECK(gex_input_is_down(&input, 'A') == 0);
    CHECK(gex_input_is_down(&input, TEST_VK_LEFT) == 1);
    CHECK(input.presses == 2);
    CHECK(input.releases == 1);

    gex_input_begin_frame(&input);
    CHECK(gex_input_was_pressed(&input, 'A') == 0);
    CHECK(gex_input_was_pressed(&input, TEST_VK_LEFT) == 0);
    CHECK(gex_input_was_released(&input, 'A') == 0);
    CHECK(gex_input_is_down(&input, TEST_VK_LEFT) == 1);
}

/* T5: losing focus releases every held key, so none stays stuck. */
static void test_focus_loss(void)
{
    gex_input input;

    gex_input_init(&input);
    gex_input_key_down(&input, 'W');
    gex_input_key_down(&input, TEST_VK_LEFT);
    gex_input_key_down(&input, TEST_VK_SHIFT);
    gex_input_begin_frame(&input);
    CHECK(input.presses == 3);

    gex_input_focus_lost(&input);
    CHECK(input.focus_losses == 1);
    CHECK(input.releases == 3);
    CHECK(gex_input_is_down(&input, 'W') == 0);
    CHECK(gex_input_is_down(&input, TEST_VK_LEFT) == 0);
    CHECK(gex_input_is_down(&input, TEST_VK_SHIFT) == 0);
    CHECK(gex_input_was_released(&input, 'W') == 1);
    CHECK(gex_input_was_released(&input, TEST_VK_LEFT) == 1);
    CHECK(gex_input_was_released(&input, TEST_VK_SHIFT) == 1);

    gex_input_begin_frame(&input);
    CHECK(gex_input_was_released(&input, 'W') == 0);

    /* A late WM_KEYUP for a key already released by focus loss is ignored. */
    gex_input_key_up(&input, 'W');
    CHECK(input.releases == 3);

    /* The table still works after regaining focus. */
    gex_input_key_down(&input, 'W');
    CHECK(gex_input_is_down(&input, 'W') == 1);
    CHECK(input.presses == 4);

    /* Focus loss with nothing held only increments its counter. */
    gex_input_focus_lost(&input);
    CHECK(input.focus_losses == 2);
    CHECK(input.releases == 4);
}

/* T6: out-of-range codes and NULL are ignored, not crashes. */
static void test_invalid(void)
{
    gex_input input;

    gex_input_init(&input);
    gex_input_key_down(&input, -1);
    gex_input_key_down(&input, GEX_KEY_COUNT);
    gex_input_key_up(&input, -1);
    gex_input_key_up(&input, GEX_KEY_COUNT);
    CHECK(input.presses == 0);
    CHECK(input.releases == 0);
    CHECK(gex_input_is_down(&input, -1) == 0);
    CHECK(gex_input_is_down(&input, GEX_KEY_COUNT) == 0);
    CHECK(gex_input_was_pressed(&input, -1) == 0);
    CHECK(gex_input_was_released(&input, GEX_KEY_COUNT) == 0);

    gex_input_init(NULL);
    gex_input_key_down(NULL, 'W');
    gex_input_key_up(NULL, 'W');
    gex_input_focus_lost(NULL);
    gex_input_begin_frame(NULL);
    CHECK(gex_input_is_down(NULL, 'W') == 0);
    CHECK(gex_input_was_pressed(NULL, 'W') == 0);
    CHECK(gex_input_was_released(NULL, 'W') == 0);
}

/* T7: the Escape key the I-4 loop interprets behaves like any other key. */
static void test_escape(void)
{
    gex_input input;

    gex_input_init(&input);
    gex_input_key_down(&input, GEX_KEY_ESCAPE);
    CHECK(gex_input_was_pressed(&input, GEX_KEY_ESCAPE) == 1);
    CHECK(gex_input_is_down(&input, GEX_KEY_ESCAPE) == 1);
    gex_input_key_up(&input, GEX_KEY_ESCAPE);
    CHECK(gex_input_was_released(&input, GEX_KEY_ESCAPE) == 1);
    CHECK(gex_input_is_down(&input, GEX_KEY_ESCAPE) == 0);
}

int main(void)
{
    test_init();
    test_press_and_repeat();
    test_release();
    test_same_frame();
    test_focus_loss();
    test_invalid();
    test_escape();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
