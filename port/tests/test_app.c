/*
 * test_app.c - I-4 application core and clock conversion checks.
 *
 * No window is created: the application core is driven with a scripted clock
 * and synthetic key transitions. The Win32 window itself is exercised by the
 * separate real-window run described in docs/WINDOW_INPUT_PORT.md.
 */

#include "gex/app/i4_app.h"
#include "gex/platform/clock.h"
#include "gex/render/null_renderer.h"

#include <stdio.h>
#include <string.h>

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

/* T1: counter ticks to microseconds, including truncation and freq = 0. */
static void test_clock_conversion(void)
{
    CHECK(gex_ticks_to_us(0, 10000000u) == 0);
    CHECK(gex_ticks_to_us(1, 10000000u) == 0);
    CHECK(gex_ticks_to_us(9, 10000000u) == 0);
    CHECK(gex_ticks_to_us(10, 10000000u) == 1);
    CHECK(gex_ticks_to_us(19, 10000000u) == 1);
    CHECK(gex_ticks_to_us(20, 10000000u) == 2);
    CHECK(gex_ticks_to_us(10000000u, 10000000u) == 1000000);
    CHECK(gex_ticks_to_us(12345678u, 10000000u) == 1234567);
    CHECK(gex_ticks_to_us(864000000000u, 10000000u) == 86400000000u);
    CHECK(gex_ticks_to_us(1234, 1000) == 1234000);
    CHECK(gex_ticks_to_us(999, 1000) == 999000);
    CHECK(gex_ticks_to_us(1, 3) == 333333);
    CHECK(gex_ticks_to_us(2, 3) == 666666);
    CHECK(gex_ticks_to_us(123, 0) == 0);
}

/* T2: the null renderer records the tick and never draws. */
static void test_null_renderer(void)
{
    gex_null_renderer renderer;
    gex_frame_tick tick;

    memset(&renderer, 0xA5, sizeof(renderer));
    gex_null_renderer_init(&renderer);
    CHECK(renderer.initialized == 1);
    CHECK(renderer.presents == 0);
    CHECK(renderer.last_now_us == 0);

    tick.now_us = 1000;
    tick.step_us = 16667;
    tick.frame_index = 2;
    tick.steps = 1;
    CHECK(gex_null_renderer_present(&renderer, &tick) == 0);
    CHECK(renderer.presents == 1);
    CHECK(renderer.last_now_us == 1000);
    CHECK(renderer.last_steps == 1);
    CHECK(renderer.last_frame_index == 2);

    CHECK(gex_null_renderer_present(&renderer, NULL) == 1);
    CHECK(renderer.presents == 1);

    gex_null_renderer_shutdown(&renderer);
    CHECK(renderer.initialized == 0);
    CHECK(renderer.presents == 1); /* counters stay readable */
    CHECK(gex_null_renderer_present(&renderer, &tick) == 1);
    CHECK(renderer.presents == 1);

    gex_null_renderer_shutdown(NULL);
    gex_null_renderer_init(NULL);
    CHECK(gex_null_renderer_present(NULL, &tick) == 1);
}

/* T3: the app binds the three hooks and runs the I-3 frame loop. */
static void test_app_integration(void)
{
    gex_i4_app app;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    memset(&app, 0xA5, sizeof(app));
    CHECK(gex_i4_app_init(&app, &cfg, 0) == GEX_FRAME_OK);
    CHECK(app.initialized == 1);
    CHECK(app.quit == 0);
    CHECK(app.frame.audio == NULL);
    CHECK(app.frame.audio_ticks == 0);

    CHECK(gex_i4_app_step(&app, 16667) == GEX_FRAME_OK);
    CHECK(app.pre_calls == 1);
    CHECK(app.updates == 1);
    /* The renderer could be either null or d3d11, but it should increment presents */
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents > 0);
    } else {
        CHECK(app.renderer.null_renderer.presents > 0);
    }
#else
    CHECK(app.renderer.null_renderer.presents > 0);
#endif
    CHECK(app.frame.frames == 1);
    CHECK(app.frame.steps == 1);
    CHECK(app.frame.last_steps == 1);
    CHECK(app.frame.core.acc_us == 0);

    CHECK(gex_i4_app_step(&app, 33334) == GEX_FRAME_OK);
    CHECK(app.pre_calls == 2);
    CHECK(app.updates == 2);
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents >= 2);
    } else {
        CHECK(app.renderer.null_renderer.presents >= 2);
    }
#else
    CHECK(app.renderer.null_renderer.presents >= 2);
#endif
#else
    CHECK(app.renderer.null_renderer.presents >= 2);
#endif
    CHECK(app.frame.steps == 2);

    /* Less than one step: the frame presents but runs no update. */
    CHECK(gex_i4_app_step(&app, 40000) == GEX_FRAME_OK);
    CHECK(app.updates == 2);
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents >= 3);
    } else {
        CHECK(app.renderer.null_renderer.presents >= 3);
    }
#else
    CHECK(app.renderer.null_renderer.presents >= 3);
#endif
#else
    CHECK(app.renderer.null_renderer.presents >= 3);
#endif
    CHECK(app.frame.last_steps == 0);
    CHECK(app.frame.core.acc_us == 6666);
    CHECK(app.frame.steps == 2);

    /* The carry plus one step runs exactly one update. */
    CHECK(gex_i4_app_step(&app, 56667) == GEX_FRAME_OK);
    CHECK(app.updates == 3);
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents >= 4);
    } else {
        CHECK(app.renderer.null_renderer.presents >= 4);
    }
#else
    CHECK(app.renderer.null_renderer.presents >= 4);
#endif
#else
    CHECK(app.renderer.null_renderer.presents >= 4);
#endif
    CHECK(app.frame.steps == 3);
    CHECK(app.frame.core.acc_us == 6666);
    CHECK(app.frame.core.steps_total == 3);
    CHECK(app.frame.core.presents_total == 4);

    #if defined(_WIN32)
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.last_now_us == 56667);
        CHECK(app.renderer.d3d11_renderer.last_steps == 1);
        CHECK(app.renderer.d3d11_renderer.last_frame_index == 3);
    } else {
        CHECK(app.renderer.null_renderer.last_now_us == 56667);
        CHECK(app.renderer.null_renderer.last_steps == 1);
        CHECK(app.renderer.null_renderer.last_frame_index == 3);
    }
#else
    CHECK(app.renderer.null_renderer.last_now_us == 56667);
    CHECK(app.renderer.null_renderer.last_steps == 1);
    CHECK(app.renderer.null_renderer.last_frame_index == 3);
#endif
#else
    CHECK(app.renderer.null_renderer.last_now_us == 56667);
    CHECK(app.renderer.null_renderer.last_steps == 1);
    CHECK(app.renderer.null_renderer.last_frame_index == 3);
#endif

    gex_i4_app_shutdown(&app);
    CHECK(app.initialized == 0);
    CHECK(gex_i4_app_step(&app, 60000) == GEX_FRAME_ERR_STATE);
}

/* T4: Escape quits the app; ordinary held keys survive the frame. */
static void test_app_escape_and_keys(void)
{
    gex_i4_app app;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    CHECK(gex_i4_app_init(&app, &cfg, 0) == GEX_FRAME_OK);
    gex_input_key_down(&app.input, 'W');
    CHECK(gex_input_is_down(&app.input, 'W') == 1);
    CHECK(gex_i4_app_step(&app, 16667) == GEX_FRAME_OK);
    CHECK(gex_i4_app_should_quit(&app) == 0);
    CHECK(gex_input_is_down(&app.input, 'W') == 1);
    CHECK(gex_input_was_pressed(&app.input, 'W') == 0); /* edges cleared */
    CHECK(app.input.presses == 1);

    gex_input_key_up(&app.input, 'W');
    CHECK(app.input.releases == 1);
    CHECK(gex_i4_app_step(&app, 33334) == GEX_FRAME_OK);
    CHECK(app.input.releases == 1);

    gex_input_key_down(&app.input, GEX_KEY_ESCAPE);
    CHECK(gex_i4_app_step(&app, 50001) == GEX_FRAME_OK);
    CHECK(gex_i4_app_should_quit(&app) == 1);
    CHECK(gex_input_was_pressed(&app.input, GEX_KEY_ESCAPE) == 0);

    gex_i4_app_shutdown(&app);
}

/* T5: focus loss releases held keys through the same app path. */
static void test_app_focus_loss(void)
{
    gex_i4_app app;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    CHECK(gex_i4_app_init(&app, &cfg, 0) == GEX_FRAME_OK);
    gex_input_key_down(&app.input, 'A');
    gex_input_focus_lost(&app.input);
    CHECK(gex_input_is_down(&app.input, 'A') == 0);
    CHECK(gex_input_was_released(&app.input, 'A') == 1);

    CHECK(gex_i4_app_step(&app, 16667) == GEX_FRAME_OK);
    CHECK(gex_i4_app_should_quit(&app) == 0); /* focus loss is not Escape */
    CHECK(app.input.focus_losses == 1);
    CHECK(app.input.releases == 1);
    CHECK(gex_input_was_released(&app.input, 'A') == 0);

    gex_i4_app_shutdown(&app);
}

/* T6: a backwards clock is a warning, not an error, and the loop recovers. */
static void test_app_clock_warning(void)
{
    gex_i4_app app;
    const gex_frame_config cfg = gex_frame_config_decoupled();

    CHECK(gex_i4_app_init(&app, &cfg, 100000) == GEX_FRAME_OK);
    CHECK(gex_i4_app_step(&app, 116667) == GEX_FRAME_OK);
    CHECK(app.updates == 1);
    /* The renderer could be either null or d3d11, but it should increment presents */
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents > 0);
    } else {
        CHECK(app.renderer.null_renderer.presents > 0);
    }
#else
    CHECK(app.renderer.null_renderer.presents > 0);
#endif

    CHECK(gex_i4_app_step(&app, 100000) == GEX_FRAME_WARN_CLOCK);
    CHECK(app.updates == 1);
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents >= 2);
    } else {
        CHECK(app.renderer.null_renderer.presents >= 2);
    }
#else
    CHECK(app.renderer.null_renderer.presents >= 2);
#endif
#else
    CHECK(app.renderer.null_renderer.presents >= 2);
#endif
    CHECK(app.frame.clock_backwards == 1);
    CHECK(gex_i4_app_should_quit(&app) == 0);

    /* The bad sample did not move t_prev: one step from 116667 is 133334. */
    CHECK(gex_i4_app_step(&app, 133334) == GEX_FRAME_OK);
    CHECK(app.updates == 2);
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents >= 3);
    } else {
        CHECK(app.renderer.null_renderer.presents >= 3);
    }
#else
    CHECK(app.renderer.null_renderer.presents >= 3);
#endif
#else
    CHECK(app.renderer.null_renderer.presents >= 3);
#endif

    gex_i4_app_shutdown(&app);
}

/* T7: initialisation and shutdown reject misuse and re-init from zero. */
static void test_app_lifecycle(void)
{
    gex_i4_app app;
    gex_frame_config bad;
    const gex_frame_config good = gex_frame_config_original();

    memset(&app, 0, sizeof(app));
    CHECK(gex_i4_app_step(&app, 0) == GEX_FRAME_ERR_STATE);
    CHECK(gex_i4_app_should_quit(NULL) == 0);
    CHECK(gex_i4_app_init(NULL, &good, 0) == GEX_FRAME_ERR_ARGUMENT);
    CHECK(gex_i4_app_init(&app, NULL, 0) == GEX_FRAME_ERR_ARGUMENT);

    bad = good;
    bad.step_us = 0;
    CHECK(gex_i4_app_init(&app, &bad, 0) == GEX_FRAME_ERR_ARGUMENT);
    CHECK(app.initialized == 0);

    CHECK(gex_i4_app_init(&app, &good, 0) == GEX_FRAME_OK);
    CHECK(gex_i4_app_step(&app, 40000) == GEX_FRAME_OK);
    /* The renderer could be either null or d3d11, but it should increment presents */
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents > 0);
    } else {
        CHECK(app.renderer.null_renderer.presents > 0);
    }
#else
    CHECK(app.renderer.null_renderer.presents > 0);
#endif
    CHECK(app.updates == 1);
    gex_i4_app_shutdown(&app);
    gex_i4_app_shutdown(&app); /* idempotent */
    CHECK(gex_i4_app_step(&app, 80000) == GEX_FRAME_ERR_STATE);

    /* Re-init is safe and starts every counter again. */
    CHECK(gex_i4_app_init(&app, &good, 80000) == GEX_FRAME_OK);
    CHECK(app.pre_calls == 0);
    CHECK(app.updates == 0);
    #if defined(_WIN32)
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents == 0);
    } else {
        CHECK(app.renderer.null_renderer.presents == 0);
    }
#else
    CHECK(app.renderer.null_renderer.presents == 0);
#endif
#else
    CHECK(app.renderer.null_renderer.presents == 0);
#endif
    CHECK(app.frame.frames == 0);
    CHECK(app.input.presses == 0);
    CHECK(gex_i4_app_step(&app, 120000) == GEX_FRAME_OK);
    /* The renderer could be either null or d3d11, but it should increment presents */
    /* The renderer could be either null or d3d11, but it should increment presents */
    #if defined(_WIN32)
    if (app.d3d11_enabled) {
        CHECK(app.renderer.d3d11_renderer.presents > 0);
    } else {
        CHECK(app.renderer.null_renderer.presents > 0);
    }
#else
    CHECK(app.renderer.null_renderer.presents > 0);
#endif
    gex_i4_app_shutdown(&app);
}

int main(void)
{
    test_clock_conversion();
    test_null_renderer();
    test_app_integration();
    test_app_escape_and_keys();
    test_app_focus_loss();
    test_app_clock_warning();
    test_app_lifecycle();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
