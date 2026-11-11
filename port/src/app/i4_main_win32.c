/*
 * i4_main_win32.c - the first executable native port process (I-4).
 *
 * It owns the real window, the real monotonic clock and the console. Every
 * piece of loop mathematics still comes from I-3:
 *
 *   gex_win32_clock  QueryPerformanceCounter in microseconds
 *   gex_win32_window real top-level window + WM_KEYDOWN/UP routing
 *   gex_i4_app       gex_frame + gex_input + null renderer, no game logic
 *
 * Cadence: the LOGICAL step is the decoupled preset (16667 us, 60 Hz). The
 * presentation cadence is 120 Hz (8333 us), chosen only so input and window
 * messages stay responsive; it is not a claim about the original, whose
 * measured 40 ms is the blocking swap period. The loop sleeps in
 * MsgWaitForMultipleObjectsEx() between presentations, so it does not spin.
 *
 * The renderer draws nothing. `update` and `pre_frame` are explicit stubs.
 */

#include "gex/app/i4_app.h"
#include "gex/platform/win32_time.h"
#include "gex/platform/win32_window.h"

#if defined(_WIN32)
#include "gex/render/d3d11_device.h"
#include "gex/render/d3d11_swapchain.h"
#include "gex/render/d3d11_renderer.h"
#include "gex/render/d3d11_pipeline.h"
#include "gex/render/d3d11_mesh.h"
#include "gex/geometry_shapes.h"
#endif

#include <mmsystem.h> /* timeBeginPeriod/timeEndPeriod (winmm) */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GEX_I4_PRESENT_PERIOD_US 8333u /* 120 Hz presentation, not the step */
#define GEX_I4_STATS_PERIOD_US 2000000u
#define GEX_I4_MAX_FRAME_ERRORS 3 /* cumulative; the run aborts past this */

typedef struct i4_options {
    uint64_t frame_limit; /* 0 = unlimited */
    uint64_t second_limit; /* 0 = unlimited */
} i4_options;

static void usage(const char *exe)
{
    printf("usage: %s [--frames N] [--seconds N]\n", exe);
    printf("  --frames N   exit after N presented frames\n");
    printf("  --seconds N  exit after N seconds\n");
    printf("  (default: run until Escape or the window is closed)\n");
}

static int parse_u64(const char *text, uint64_t *out)
{
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || *text == '\0') {
        return 0;
    }
    value = strtoull(text, &end, 10);
    if (end == NULL || *end != '\0' || value == 0) {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

static int parse_options(int argc, char **argv, i4_options *options)
{
    int i;

    options->frame_limit = 0;
    options->second_limit = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return -1;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            if (!parse_u64(argv[++i], &options->frame_limit)) {
                fprintf(stderr, "invalid --frames value\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            if (!parse_u64(argv[++i], &options->second_limit)) {
                fprintf(stderr, "invalid --seconds value\n");
                return 0;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 0;
        }
    }
    return 1;
}

static void key_label(int vk, char *buffer, size_t size)
{
    if (vk >= 'A' && vk <= 'Z') {
        snprintf(buffer, size, "%c", vk);
        return;
    }
    if (vk >= '0' && vk <= '9') {
        snprintf(buffer, size, "%c", vk);
        return;
    }
    switch (vk) {
    case GEX_KEY_ESCAPE:
        snprintf(buffer, size, "Escape");
        break;
    case VK_SPACE:
        snprintf(buffer, size, "Space");
        break;
    case VK_RETURN:
        snprintf(buffer, size, "Return");
        break;
    case VK_TAB:
        snprintf(buffer, size, "Tab");
        break;
    case VK_SHIFT:
        snprintf(buffer, size, "Shift");
        break;
    case VK_CONTROL:
        snprintf(buffer, size, "Control");
        break;
    case VK_MENU:
        snprintf(buffer, size, "Alt");
        break;
    case VK_LEFT:
        snprintf(buffer, size, "Left");
        break;
    case VK_RIGHT:
        snprintf(buffer, size, "Right");
        break;
    case VK_UP:
        snprintf(buffer, size, "Up");
        break;
    case VK_DOWN:
        snprintf(buffer, size, "Down");
        break;
    default:
        snprintf(buffer, size, "VK_%02X", (unsigned)vk);
        break;
    }
}

static void log_input(const gex_input *input, uint64_t *focus_losses_seen)
{
    char label[32];
    int vk;

    if (input->focus_losses != *focus_losses_seen) {
        *focus_losses_seen = input->focus_losses;
        printf("input: focus lost (%" PRIu64 " total)\n",
               input->focus_losses);
    }
    for (vk = 0; vk < GEX_KEY_COUNT; vk++) {
        if (gex_input_was_pressed(input, vk)) {
            key_label(vk, label, sizeof(label));
            printf("input: key down %s (0x%02X)\n", label, (unsigned)vk);
        }
        if (gex_input_was_released(input, vk)) {
            key_label(vk, label, sizeof(label));
            printf("input: key up %s (0x%02X)\n", label, (unsigned)vk);
        }
    }
}

static void print_stats(const char *prefix, const gex_i4_app *app,
                        uint64_t now_us, uint64_t start_us)
{
    const gex_frame *frame = &app->frame;
    const double elapsed =
        (now_us > start_us) ? (double)(now_us - start_us) / 1000000.0 : 0.0;

    printf("%s t=%.2fs presents=%" PRIu64 " fps=%.1f steps=%" PRIu64
           " steps/s=%.1f last_steps=%u acc=%" PRIu64 "us discards=%" PRIu64
           " aborted=%" PRIu64 " backwards=%" PRIu64 " pre=%" PRIu64
           " updates=%" PRIu64 "\n",
           prefix, elapsed, 
#if defined(_WIN32)
            app->renderer.d3d11_renderer.presents,
#else
            app->renderer.null_renderer.presents,
#endif
           #if defined(_WIN32)
            (elapsed > 0.0) ? (double)app->renderer.d3d11_renderer.presents / elapsed : 0.0,
#else
            (elapsed > 0.0) ? (double)app->renderer.null_renderer.presents / elapsed : 0.0,
#endif
           frame->steps,
           (elapsed > 0.0) ? (double)frame->steps / elapsed : 0.0,
           frame->last_steps, frame->core.acc_us,
           frame->core.discards_total, frame->aborted, frame->clock_backwards,
           app->pre_calls, app->updates);
}

int main(int argc, char **argv)
{
    i4_options options;
    gex_win32_clock clock;
    gex_win32_window window;
    gex_i4_app app;
    gex_frame_config cfg;
    gex_frame_status status;
    gex_frame_status last_error;
    uint64_t start_us;
    uint64_t now_us;
    uint64_t next_present_us;
    uint64_t deadline_us;
    uint64_t last_stats_us;
    uint64_t focus_losses_seen = 0;
    uint64_t errors = 0;
    uint64_t clock_warnings = 0;
    int parsed;
    int exit_code = 0;
    int timer_resolution_set = 0;
#if defined(_WIN32)
    /* Módulo 5A: pipeline de dibujo, malla y geometría de prueba. */
    gex_d3d11_pipeline d3d11_pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh d3d11_mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_emit_output cube_emit;
#endif

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#if defined(_WIN32)
    memset(&cube_emit, 0, sizeof(cube_emit));
#endif

    parsed = parse_options(argc, argv, &options);
    if (parsed < 0) {
        return 0;
    }
    if (parsed == 0) {
        return 1;
    }

    if (!gex_win32_clock_init(&clock)) {
        fprintf(stderr, "error: QueryPerformanceCounter is unavailable\n");
        return 1;
    }
    start_us = gex_win32_clock_now_us(&clock);
    deadline_us = (options.second_limit > 0)
                      ? start_us + options.second_limit * 1000000u
                      : UINT64_MAX;

    cfg = gex_frame_config_decoupled();
    status = gex_i4_app_init(&app, &cfg, start_us);
    if (status != GEX_FRAME_OK) {
        fprintf(stderr, "error: app init failed: %s\n",
                gex_frame_status_name(status));
        return 1;
    }

    if (!gex_win32_window_create(&window, &app.input,
                                 L"Gex native loop - I-4", 960,
                                 540)) {
        fprintf(stderr, "error: could not create the Win32 window "
                        "(GetLastError=%lu)\n",
                (unsigned long)GetLastError());
        gex_i4_app_shutdown(&app);
        return 1;
    }

    printf("Gex native loop - I-4\n");
    printf("clock: QueryPerformanceCounter, %" PRIu64 " Hz\n",
           gex_win32_clock_freq(&clock));
    printf("frame: decoupled, step=%u us (60 Hz), max_dt=%u us, max_steps=%d\n",
           (unsigned)cfg.step_us, (unsigned)cfg.max_dt_us, cfg.max_steps);
    printf("present: target %u us (120 Hz), logical and present cadences "
           "independent\n",
           (unsigned)GEX_I4_PRESENT_PERIOD_US);
    timer_resolution_set = (timeBeginPeriod(1) == TIMERR_NOERROR);
    printf("timer: %s (timeBeginPeriod(1))\n",
           timer_resolution_set ? "1 ms resolution"
                                : "default resolution (present rate will be "
                                  "coarser)");
    printf("window: class=%ls, 960x540 client area\n", GEX_WINDOW_CLASS_NAME);
    
#if defined(_WIN32)
    /* Initialize D3D11 device and swapchain for R-3 */
    gex_d3d11_device d3d11_dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_swapchain d3d11_sw = GEX_D3D11_SWAPCHAIN_INITIALIZER;
    gex_d3d11_swapchain_config sw_cfg = GEX_D3D11_SWAPCHAIN_CONFIG_INITIALIZER;
    float clear_color[4] = {0.2f, 0.2f, 0.8f, 1.0f}; /* Blue color */
    
    /* Configure swapchain for the window */
    sw_cfg.hwnd = window.hwnd;
    sw_cfg.width = 960;
    sw_cfg.height = 540;
    sw_cfg.vsync_retraces = 1; /* Vsync enabled */
    
    /* Create D3D11 device */
    gex_d3d11_device_status dev_status = gex_d3d11_device_init(&d3d11_dev, 
                                                              GEX_D3D11_DRIVER_HARDWARE, 
                                                              0);
    if (dev_status != GEX_D3D11_DEVICE_OK) {
        fprintf(stderr, "error: D3D11 device init failed: %s (HRESULT=0x%08lX)\n",
                gex_d3d11_device_status_name(dev_status), d3d11_dev.last_hresult);
        gex_win32_window_destroy(&window);
        gex_i4_app_shutdown(&app);
        return 1;
    }
    
    /* Create swapchain */
    gex_d3d11_swapchain_status sw_status = gex_d3d11_swapchain_init(&d3d11_sw, 
                                                                   &d3d11_dev, 
                                                                   &sw_cfg);
    if (sw_status != GEX_D3D11_SWAPCHAIN_OK) {
        fprintf(stderr, "error: D3D11 swapchain init failed: %s (HRESULT=0x%08lX)\n",
                gex_d3d11_swapchain_status_name(sw_status), d3d11_sw.last_hresult);
        gex_d3d11_device_shutdown(&d3d11_dev);
        gex_win32_window_destroy(&window);
        gex_i4_app_shutdown(&app);
        return 1;
    }
    
    /* Enable D3D11 rendering in the app */
    gex_d3d11_renderer_status renderer_status = gex_i4_app_enable_d3d11(&app, 
                                                                       &d3d11_dev, 
                                                                       &d3d11_sw, 
                                                                       clear_color);
    if (renderer_status != GEX_D3D11_RENDERER_OK) {
        fprintf(stderr, "error: D3D11 renderer init failed: %s (HRESULT=0x%08lX)\n",
                gex_d3d11_renderer_status_name(renderer_status), app.renderer.d3d11_renderer.last_hresult);
        gex_d3d11_swapchain_shutdown(&d3d11_sw);
        gex_d3d11_device_shutdown(&d3d11_dev);
        gex_win32_window_destroy(&window);
        gex_i4_app_shutdown(&app);
        return 1;
    }
    
    printf("renderer: Direct3D 11 (hardware)\n");
    /* Update window title to reflect D3D11 renderer */
    SetWindowTextW(window.hwnd, L"Gex native loop - I-4 (Direct3D 11)");
    printf("backbuffer: %u x %u, %u buffers\n", 
           (unsigned)sw_cfg.width, (unsigned)sw_cfg.height, 
           (unsigned)d3d11_sw.desc.BufferCount);
    printf("clear color: RGB(%.1f, %.1f, %.1f)\n", clear_color[0], clear_color[1], clear_color[2]);

    /* Módulo 5A: pipeline de dibujo y primera geometría 3D visible.
     * Si algo falla (por ejemplo, que no haya d3dcompiler_47.dll), la
     * aplicación sigue funcionando: el renderer solo limpia y presenta. Cada
     * etapa se registra por separado para poder diagnosticar cuál falló. */
    {
        gex_d3d11_pipeline_status pipeline_status =
            gex_d3d11_pipeline_init(&d3d11_pipeline, &d3d11_dev);
        gex_d3d11_mesh_status mesh_status = GEX_D3D11_MESH_BAD_ARGUMENT;
        gex_d3d11_renderer_status geometry_status = GEX_D3D11_RENDERER_BAD_STATE;
        int shape_status = -1; /* -1 = no se intentó */

        if (pipeline_status == GEX_D3D11_PIPELINE_OK) {
            shape_status = gex_shape_cube(&cube_emit, 0.6f);
            if (shape_status == 0) {
                mesh_status = gex_d3d11_mesh_create_from_emit(
                    d3d11_dev.device, &cube_emit, &d3d11_mesh);
                if (mesh_status == GEX_D3D11_MESH_OK) {
                    geometry_status = gex_d3d11_renderer_set_geometry(
                        &app.renderer.d3d11_renderer, &d3d11_pipeline,
                        &d3d11_mesh, GEX_D3D11_REFERENCE_WIDTH,
                        GEX_D3D11_REFERENCE_HEIGHT);
                }
            }
        }
        if (geometry_status == GEX_D3D11_RENDERER_OK) {
            printf("geometry: cube, %u triangles, vertex colors, no textures "
                   "(%ux%u logical)\n",
                   (unsigned)GEX_SHAPE_CUBE_TRIANGLES,
                   (unsigned)GEX_D3D11_REFERENCE_WIDTH,
                   (unsigned)GEX_D3D11_REFERENCE_HEIGHT);
        } else {
            printf("geometry: none (pipeline: %s, shape: %s, mesh: %s)\n",
                   gex_d3d11_pipeline_status_name(pipeline_status),
                   (shape_status < 0)
                       ? "not attempted"
                       : ((shape_status == 0) ? "ok" : "out of memory"),
                   gex_d3d11_mesh_status_name(mesh_status));
            if (pipeline_status != GEX_D3D11_PIPELINE_OK &&
                d3d11_pipeline.last_error[0] != '\0') {
                printf("shader error: %s\n", d3d11_pipeline.last_error);
            }
        }
    }
#else
    printf("renderer: null (draws nothing)\n");
#endif
    
    printf("audio: none attached\n");
    printf("controls: Escape or close the window to exit\n");

    last_error = GEX_FRAME_OK;
    last_stats_us = start_us;
    next_present_us = start_us;

    while (!gex_i4_app_should_quit(&app) &&
           !gex_win32_window_should_quit(&window)) {
        now_us = gex_win32_clock_now_us(&clock);
        if (now_us >= deadline_us) {
            break;
        }

        if (now_us < next_present_us) {
            const uint64_t remaining = next_present_us - now_us;
            const DWORD wait_ms = (DWORD)((remaining + 999u) / 1000u);
            MsgWaitForMultipleObjectsEx(0, NULL, wait_ms, QS_ALLINPUT,
                                        MWMO_INPUTAVAILABLE);
        }

        gex_win32_window_poll(&window);
        if (gex_win32_window_should_quit(&window)) {
            break;
        }

        log_input(&app.input, &focus_losses_seen);
        now_us = gex_win32_clock_now_us(&clock);
        status = gex_i4_app_step(&app, now_us);

        if (status == GEX_FRAME_WARN_CLOCK) {
            clock_warnings++;
            if (last_error != GEX_FRAME_WARN_CLOCK) {
                printf("warning: clock went backwards (recovered)\n");
            }
            last_error = status;
        } else if (status < 0) {
            errors++;
#if defined(_WIN32)
            /* Diagnóstico del error de dibujo: el renderer guarda el estado
             * del pipeline y el HRESULT del último fallo. */
            if (app.d3d11_enabled &&
                app.renderer.d3d11_renderer.last_draw_status !=
                    GEX_D3D11_PIPELINE_OK) {
                fprintf(stderr,
                        "error: draw failed: %s (HRESULT=0x%08lX, draws=%" PRIu64
                        ", draw errors=%" PRIu64 ")\n",
                        gex_d3d11_pipeline_status_name(
                            app.renderer.d3d11_renderer.last_draw_status),
                        (unsigned long)app.renderer.d3d11_renderer.last_draw_hresult,
                        app.renderer.d3d11_renderer.draws,
                        app.renderer.d3d11_renderer.draw_errors);
            }
#endif
            printf("error: frame status: %s\n", gex_frame_status_name(status));
            last_error = status;
            if (errors >= GEX_I4_MAX_FRAME_ERRORS) {
                fprintf(stderr, "error: too many frame errors\n");
                exit_code = 2;
                break;
            }
        } else {
            last_error = GEX_FRAME_OK;
        }

        if (options.frame_limit > 0 &&
#if defined(_WIN32)
            app.renderer.d3d11_renderer.presents >= options.frame_limit) {
            printf("frame limit reached (%" PRIu64 " presents)\n",
                   app.renderer.d3d11_renderer.presents);
#else
            app.renderer.null_renderer.presents >= options.frame_limit) {
            printf("frame limit reached (%" PRIu64 " presents)\n",
                   app.renderer.null_renderer.presents);
#endif
            break;
        }
        if (options.second_limit > 0 && now_us >= deadline_us) {
            break;
        }

        if (now_us - last_stats_us >= GEX_I4_STATS_PERIOD_US) {
            print_stats("stats:", &app, now_us, start_us);
            last_stats_us = now_us;
        }
        next_present_us = now_us + GEX_I4_PRESENT_PERIOD_US;
    }

    now_us = gex_win32_clock_now_us(&clock);
    print_stats("final:", &app, now_us, start_us);
    printf("exit: %s, frame errors=%" PRIu64 ", clock warnings=%" PRIu64 "\n",
           exit_code == 0 ? "clean" : "with errors", errors, clock_warnings);

    if (timer_resolution_set) {
        timeEndPeriod(1);
    }
    
#if defined(_WIN32)
    /* Cierre de los recursos de D3D11 en orden inverso al de creación. Cada
     * recurso se libera según su propio estado (todas las funciones son
     * idempotentes), de modo que un fallo en cualquier etapa intermedia no
     * deja recursos vivos: el pipeline y la malla se sueltan siempre, no solo
     * cuando la geometría llegó a registrarse en el renderer. */
    if (gex_i4_app_is_d3d11_enabled(&app)) {
        /* 1. El renderer deja de dibujar la geometría (punteros prestados). */
        gex_d3d11_renderer_set_geometry(&app.renderer.d3d11_renderer, NULL,
                                        NULL, 0, 0);
    }
    /* 2. La malla y el pipeline: ya nadie los usa. */
    gex_d3d11_mesh_destroy(&d3d11_mesh);
    gex_d3d11_pipeline_shutdown(&d3d11_pipeline);
    /* 3. La geometría en memoria del host. */
    gex_emit_output_free(&cube_emit);
    /* 4. El renderer, la swapchain y el dispositivo. */
    if (gex_i4_app_is_d3d11_enabled(&app)) {
        gex_i4_app_disable_d3d11(&app);
        gex_d3d11_swapchain_shutdown(&d3d11_sw);
        gex_d3d11_device_shutdown(&d3d11_dev);
    }
#endif
    
    gex_win32_window_destroy(&window);
    gex_i4_app_shutdown(&app);
    return exit_code;
}
