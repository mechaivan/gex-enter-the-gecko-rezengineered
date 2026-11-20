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
#include "gex/render/d3d11_texture.h"
#include "gex/geometry_shapes.h"
#include "gex/scene_level.h"
#include "gex/vfx_texture.h"
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
    const char *level_path; /* .DFX to load, or NULL for the test cube */
    int diagnostic_view; /* 1 = vista sintetica + encuadre automatico */
    int camera_path; /* 1 = reproduce la trayectoria de camara del nivel */
    const char *screenshot_path; /* .ppm de salida, o NULL */
    uint64_t screenshot_frame; /* 0 = el ultimo frame del limite */
    int textures_final; /* 1 = texturas del fotograma final del vuelo */
    int menu; /* 1 = anade el menu del televisor al final de la trayectoria */
    uint64_t camera_freeze; /* muestra exacta en la que congelar la camara */
    int camera_freeze_set;  /* 1 = --camera-path-freeze presente */
} i4_options;

static void usage(const char *exe)
{
    printf("usage: %s [--frames N] [--seconds N] [--level PATH] "
           "[--diagnostic-view] [--camera-path] [--textures-final] "
           "[--camera-path-freeze N] "
           "[--screenshot PATH [--screenshot-frame N]]\n", exe);
    printf("  --frames N   exit after N presented frames\n");
    printf("  --seconds N  exit after N seconds\n");
    printf("  --level PATH load a .DFX level with the ORIGINAL game camera "
           "(the reference MAP7 camera state)\n");
    printf("  --diagnostic-view  use the synthetic matrix and the automatic "
           "framing instead of the game camera\n");
    printf("  --camera-path  play the camera path stored in the level itself "
           "(MAP7: 428 frames of the menu fly-through)\n");
    printf("  --screenshot PATH  write the presented backbuffer as a binary "
           "PPM (P6) to PATH\n");
    printf("  --screenshot-frame N  capture on presented frame N (default: the "
           "--frames limit)\n");
    printf("  --textures-final  with --camera-path, ask the .VFX for the "
           "textures of the final camera pose (the menu framing) instead of "
           "the most used over the flight\n");
    printf("  --menu  when the camera path ends, add the TV main menu "
           "(tvmenu__ panel + the four tvbutn__ labels) to the scene\n");
    printf("  --camera-path-freeze N  advance the camera path exactly N "
           "samples and freeze it (reproducible screenshots)\n");
    printf("  (default: run until Escape or the window is closed)\n");
}

#if defined(_WIN32)
/*
 * Deduce el `.VFX` de un `.DFX`: mismo directorio y mismo nombre base con
 * la extensión `.vfx` (es la convención del juego: `map7.dfx` y
 * `map7.vfx`). Devuelve 0 si no cabe en `dst`.
 */
static int vfx_path_for(const char *dfx_path, char *dst, size_t dst_size)
{
    size_t len;
    size_t i;

    if (dfx_path == NULL || dst == NULL || dst_size == 0u) {
        return -1;
    }
    len = strlen(dfx_path);
    if (len + 1u > dst_size) {
        return -1;
    }
    memcpy(dst, dfx_path, len + 1u);
    i = len;
    while (i > 0u && dst[i - 1u] != '.' && dst[i - 1u] != '\\' &&
           dst[i - 1u] != '/') {
        i--;
    }
    if (i == 0u || dst[i - 1u] != '.') {
        return -1;
    }
    if (len - i + 4u + 1u > dst_size) {
        return -1;
    }
    memcpy(dst + i, "vfx", 3);
    dst[i + 3] = '\0';
    return 0;
}
#endif

#if defined(_WIN32)
/*
 * Estado de la secuencia de camara del menu (S-3) en la aplicacion.
 *
 * La ruta (los 26 registros de 32 B) se lee del propio `.DFX` del nivel, con
 * la cadena de punteros documentada. Un update logico avanza la trayectoria
 * **un frame** (como el original: 0x411B94 -> 0x53E98C avanza el nodo una vez
 * por update) y la escena se vuelve a emitir con la camara nueva.
 *
 * La geometria resultante se sube a la malla D3D11 para que el dibujo
 * refleje el movimiento.
 */
typedef struct i4_camera_path {
    gex_scene_level *scene;
    gex_camera *cam;
    ID3D11Device *device;
    gex_d3d11_mesh *mesh;
    uint32_t samples;   /* muestras producidas */
    int finished;       /* el centinela ya se alcanzo */
    int frozen;         /* 1 = camara congelada (captura reproducible) */
    int error;          /* fallo de la re-emision */
    int menu;           /* 1 = anadir el menu del televisor al terminar */
} i4_camera_path;

/*
 * Un update logico: un paso de trayectoria, la re-emision con la camara
 * nueva y la subida de la geometria a la GPU.
 */
static int i4_camera_path_update(void *ctx)
{
    i4_camera_path *state = (i4_camera_path *)ctx;
    int step;

    if (state->frozen || state->finished || state->error) {
        return 0;
    }

    step = gex_scene_level_camera_path_step(state->scene, state->cam);
    if (step < 0) {
        fprintf(stderr, "error: camera path step failed\n");
        state->error = 1;
        return 1;
    }
    if (step == 0) {
        /* Fin de la trayectoria: la ultima posicion se conserva y no se
         * escribe nada mas (como en el original). */
        state->finished = 1;
        /* S-4: el menu del televisor aparece cuando termina el vuelo. Se
         * anade a la emision del ultimo fotograma y se sube a la GPU. */
        if (state->menu) {
            if (gex_scene_level_add_menu(state->scene, 0) != GEX_SCENE_OK) {
                fprintf(stderr, "error: no se pudo anadir el menu\n");
                state->error = 1;
                return 1;
            }
            if (gex_d3d11_mesh_update(state->device, &state->scene->geometry,
                                       state->mesh) != GEX_D3D11_MESH_OK) {
                fprintf(stderr, "error: mesh update failed (menu)\n");
                state->error = 1;
                return 1;
            }
            printf("menu: panel tvmenu__ + rotulos tvbutn__ anadidos "
                   "(triangulos=%zu)\n", state->scene->emit.count);
        }
        return 0;
    }
    state->samples++;

    /* La geometria cambio: se sube a la malla D3D11. */
    if (gex_d3d11_mesh_update(state->device, &state->scene->geometry,
                               state->mesh) != GEX_D3D11_MESH_OK) {
        fprintf(stderr, "error: mesh update failed at frame %u\n",
                (unsigned)state->samples);
        state->error = 1;
        return 1;
    }
    return 0;
}
#endif

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
    options->level_path = NULL;
    options->diagnostic_view = 0;
    options->camera_path = 0;
    options->screenshot_path = NULL;
    options->screenshot_frame = 0;
    options->textures_final = 0;
    options->menu = 0;
    options->camera_freeze = 0;
    options->camera_freeze_set = 0;
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
        } else if (strcmp(argv[i], "--diagnostic-view") == 0) {
            options->diagnostic_view = 1;
        } else if (strcmp(argv[i], "--camera-path") == 0) {
            options->camera_path = 1;
        } else if (strcmp(argv[i], "--textures-final") == 0) {
            options->textures_final = 1;
        } else if (strcmp(argv[i], "--camera-path-freeze") == 0 &&
                   i + 1 < argc) {
            if (!parse_u64(argv[++i], &options->camera_freeze)) {
                fprintf(stderr, "invalid --camera-path-freeze value\n");
                return 0;
            }
            options->camera_freeze_set = 1;
        } else if (strcmp(argv[i], "--menu") == 0) {
            options->menu = 1;
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            options->level_path = argv[++i];
            if (options->level_path[0] == '\0') {
                fprintf(stderr, "invalid --level path\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            options->screenshot_path = argv[++i];
            if (options->screenshot_path[0] == '\0') {
                fprintf(stderr, "invalid --screenshot path\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--screenshot-frame") == 0 && i + 1 < argc) {
            if (!parse_u64(argv[++i], &options->screenshot_frame)) {
                fprintf(stderr, "invalid --screenshot-frame value\n");
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

#if defined(_WIN32)
/* Estado de la secuencia de camara en curso (para el informe de stats). */
static i4_camera_path *g_camera_path_state = NULL;
#endif

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
#if defined(_WIN32)
    if (g_camera_path_state != NULL) {
        const i4_camera_path *p = g_camera_path_state;
        printf("%s camera path: samples=%u finished=%d error=%d "
               "pos=(%d, %d, %d) euler=(%d, %d, %d) "
               "leaves=%u triangles=%zu\n", prefix,
               (unsigned)p->samples, p->finished, p->error,
               p->cam != NULL ? p->cam->pos[0] : 0,
               p->cam != NULL ? p->cam->pos[1] : 0,
               p->cam != NULL ? p->cam->pos[2] : 0,
               p->cam != NULL ? p->cam->euler[0] : 0,
               p->cam != NULL ? p->cam->euler[1] : 0,
               p->cam != NULL ? p->cam->euler[2] : 0,
               p->scene != NULL ? p->scene->stats.leaf_count : 0u,
               p->scene != NULL ? p->scene->stats.triangles : 0);
    }
#endif
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
    uint64_t screenshot_target = 0;
    int screenshot_requested = 0;
    int parsed;
    int exit_code = 0;
    int timer_resolution_set = 0;
#if defined(_WIN32)
    /* Módulo 5A: pipeline de dibujo, malla y geometría de prueba. */
    gex_d3d11_pipeline d3d11_pipeline = GEX_D3D11_PIPELINE_INITIALIZER;
    gex_d3d11_mesh d3d11_mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_emit_output cube_emit;
    /* Módulo 5B: nivel real (opcional). */
    gex_scene_level scene;
    /* Módulo 4C: texturas del `.VFX` del nivel. */
    gex_vfx_texture_bank texture_bank;
    gex_d3d11_texture_set d3d11_textures = GEX_D3D11_TEXTURE_SET_INITIALIZER;
    /* S-3: camara de la secuencia de trayectoria y su estado. */
    gex_camera path_cam;
    i4_camera_path path_state;
    gex_d3d11_renderer_status geometry_status = GEX_D3D11_RENDERER_BAD_STATE;
    gex_scene_status scene_status = GEX_SCENE_OK;
#endif

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#if defined(_WIN32)
    memset(&cube_emit, 0, sizeof(cube_emit));
    memset(&scene, 0, sizeof(scene));
    memset(&texture_bank, 0, sizeof(texture_bank));
#endif

    parsed = parse_options(argc, argv, &options);
    if (parsed < 0) {
        return 0;
    }
    if (parsed == 0) {
        return 1;
    }

    /* La captura es un pedido diferido al renderer: necesita un fotograma
     * concreto. Sin --screenshot-frame se usa el limite de --frames. */
    if (options.screenshot_path != NULL) {
        if (options.screenshot_frame == 0 && options.frame_limit == 0) {
            fprintf(stderr, "error: --screenshot needs --frames N or "
                            "--screenshot-frame N\n");
            return 1;
        }
        screenshot_target = (options.screenshot_frame > 0)
                                ? options.screenshot_frame
                                : options.frame_limit;
    } else if (options.screenshot_frame != 0) {
        fprintf(stderr, "error: --screenshot-frame needs --screenshot PATH\n");
        return 1;
    }
    if (options.textures_final &&
        (!options.camera_path || options.level_path == NULL)) {
        fprintf(stderr, "error: --textures-final needs --camera-path and "
                        "--level PATH\n");
        return 1;
    }
    if (options.camera_freeze_set &&
        (!options.camera_path || options.level_path == NULL)) {
        fprintf(stderr, "error: --camera-path-freeze needs --camera-path and "
                        "--level PATH\n");
        return 1;
    }
#if !defined(_WIN32)
    if (options.screenshot_path != NULL) {
        fprintf(stderr, "error: --screenshot needs the Direct3D 11 "
                        "renderer (Windows only)\n");
        return 1;
    }
#endif

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
    float clear_color[4];
    /* S-11: el fondo del original no es azul. Medido en la grabacion del
     * ejecutable (vuelo de MAP7): (8,9,40), plano, igual que el tile 325 del
     * .VFX. Ver gex/scene_level.h y docs/D3D11_FIRST_SCENE_PLAN.md, seccion 27. */
    gex_scene_background_rgba(clear_color);
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
    printf("clear color: RGB(%u, %u, %u)\n",
           (unsigned)GEX_SCENE_BACKGROUND_R, (unsigned)GEX_SCENE_BACKGROUND_G,
           (unsigned)GEX_SCENE_BACKGROUND_B);

    /* Módulo 5A: pipeline de dibujo y geometría visible. Módulo 5B: con
     * `--level` se dibuja la geometría real de un nivel .DFX; sin él, el
     * cubo de prueba. Si algo falla (por ejemplo, que no haya
     * d3dcompiler_47.dll), la aplicación sigue funcionando: el renderer solo
     * limpia y presenta. Cada etapa se registra por separado para poder
     * diagnosticar cuál falló. */
    {
        gex_d3d11_pipeline_status pipeline_status =
            gex_d3d11_pipeline_init(&d3d11_pipeline, &d3d11_dev);
        gex_d3d11_mesh_status mesh_status = GEX_D3D11_MESH_BAD_ARGUMENT;
        gex_d3d11_texture_status texture_status = GEX_D3D11_TEXTURE_BAD_ARGUMENT;
        int shape_status = -1; /* -1 = no se intentó */

        if (pipeline_status == GEX_D3D11_PIPELINE_OK) {
            if (options.level_path != NULL) {
                /* Nivel real: .DFX -> descriptor -> BSP -> emisión ->
                 * geometría -> buffers D3D11.
                 *
                 * Con la cámara original del juego (matriz de emisión
                 * cam+0xE0, prueba de esfera del BSP y la misma
                 * proyección) o, con --diagnostic-view, con la vista
                 * sintética y el encuadre automático de siempre. */
                if (options.diagnostic_view) {
                    scene_status = gex_scene_level_load(&scene,
                                                        options.level_path);
                } else if (options.camera_path) {
                    /* Camara original + la trayectoria de posicion que el
                     * nivel trae en su .DFX (MAP7). La camara vive en
                     * `path_cam` para que el callback de update la pueda
                     * mover. */
                    gex_scene_reference_camera(&path_cam);
                    scene_status = gex_scene_level_load_camera_path(
                        &scene, options.level_path, &path_cam);
                } else {
                    gex_camera cam;
                    gex_scene_reference_camera(&cam);
                    scene_status = gex_scene_level_load_camera(&scene,
                                                              options.level_path,
                                                              &cam);
                }
                if (scene_status == GEX_SCENE_OK) {
                    mesh_status = gex_d3d11_mesh_create(
                        d3d11_dev.device, &scene.geometry, &d3d11_mesh);
                    if (mesh_status == GEX_D3D11_MESH_OK) {
                        geometry_status = gex_d3d11_renderer_set_geometry(
                            &app.renderer.d3d11_renderer, &d3d11_pipeline,
                            &d3d11_mesh, GEX_D3D11_REFERENCE_WIDTH,
                            GEX_D3D11_REFERENCE_HEIGHT);
                    }
                } else {
                    fprintf(stderr, "error: level '%s': %s\n",
                            options.level_path,
                            gex_scene_status_name(scene_status));
                }
            } else {
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
        }

        /* Módulo 4C: texturas del `.VFX` del nivel. Solo cuando hay un nivel
         * real: el cubo de prueba no lleva texturas (y su camino sigue
         * intacto). */
        if (options.level_path != NULL && scene_status == GEX_SCENE_OK) {
            char vfx_path[512];
            uint32_t *used = NULL;
            uint32_t used_count = 0;
            size_t vi;

            if (vfx_path_for(options.level_path, vfx_path,
                             sizeof(vfx_path)) == 0) {
                /* Indices de textura que usa de verdad la geometria.
                 *
                 * Con la secuencia de camara la geometria cambia en cada
                 * frame: las texturas que se ven NO son solo las del
                 * frame 0, sino las de todo el vuelo (mas que las 128 que
                 * cabian en el atlas antes de S-8). Se recorre la trayectoria
                 * una vez y se piden las texturas MAS USADAS; si caben todas
                 * (S-8: 231 <= 256) la cobertura es el 100 % y la lista se
                 * usa como conjunto completo. */
                if (options.camera_path && scene.path_loaded) {
                    uint32_t *counts = (uint32_t *)calloc(16384u,
                                                          sizeof(uint32_t));
                    size_t distinct = 0;
                    gex_scene_status usage_status = GEX_SCENE_ERR_ARG;
                    if (counts != NULL) {
                        usage_status =
                            options.textures_final
                                ? gex_scene_camera_path_final_texture_usage(
                                      options.level_path, &path_cam, counts,
                                      16384u, &distinct)
                                : gex_scene_camera_path_texture_usage(
                                      options.level_path, &path_cam, counts,
                                      16384u, &distinct);
                    }
                    if (counts != NULL && usage_status == GEX_SCENE_OK) {
                        uint32_t *idxs = (uint32_t *)malloc(16384u *
                                                             sizeof(uint32_t));
                        if (idxs != NULL) {
                            uint32_t n = 0;
                            uint32_t i;
                            for (i = 0; i < 16384u; i++) {
                                if (counts[i] != 0u) {
                                    idxs[n++] = i;
                                }
                            }
                            /* Orden por uso descendente. */
                            {
                                uint32_t a, b;
                                for (a = 0; a + 1u < n; a++) {
                                    for (b = 0; b + 1u < n - a; b++) {
                                        if (counts[idxs[b + 1u]] >
                                            counts[idxs[b]]) {
                                            uint32_t tmp = idxs[b];
                                            idxs[b] = idxs[b + 1u];
                                            idxs[b + 1u] = tmp;
                                        }
                                    }
                                }
                            }
                            used_count =
                                (n > (uint32_t)GEX_D3D11_TEXTURE_MAX_ENTRIES)
                                ? (uint32_t)GEX_D3D11_TEXTURE_MAX_ENTRIES : n;
                            /* S-4: el menu necesita sus ocho texturas
                             * (600..607). Se ponen las primeras de la lista
                             * para que entren siempre en el atlas, aunque el
                             * vuelo use el resto (S-8: 231 indices, caben en
                             * las 256 entradas). */
                            if (options.menu && idxs != NULL) {
                                static const uint32_t menu_tex[8] = {
                                    600, 601, 602, 603, 604, 605, 606, 607};
                                uint32_t *prio = (uint32_t *)malloc(
                                    ((size_t)n + 8u) * sizeof(uint32_t));
                                if (prio != NULL) {
                                    uint32_t total = 0, k;
                                    for (k = 0; k < 8u; k++) {
                                        prio[total++] = menu_tex[k];
                                    }
                                    for (k = 0; k < n; k++) {
                                        uint32_t m;
                                        int is_menu = 0;
                                        for (m = 0; m < 8u; m++) {
                                            if (idxs[k] == menu_tex[m]) {
                                                is_menu = 1;
                                                break;
                                            }
                                        }
                                        if (!is_menu) {
                                            prio[total++] = idxs[k];
                                        }
                                    }
                                    free(idxs);
                                    idxs = prio;
                                    n = total;
                                    used_count =
                                        (n > (uint32_t)
                                                 GEX_D3D11_TEXTURE_MAX_ENTRIES)
                                            ? (uint32_t)
                                                  GEX_D3D11_TEXTURE_MAX_ENTRIES
                                            : n;
                                }
                            }
                            used = idxs;
                            {
                                uint64_t total = 0;
                                uint64_t covered = 0;
                                uint32_t k;
                                for (k = 0; k < 16384u; k++) {
                                    total += counts[k];
                                }
                                for (k = 0; k < used_count; k++) {
                                    covered += counts[idxs[k]];
                                }
                                printf("textures: %u distinctas en %s, "
                                       "se piden %u (limite del atlas; cubren "
                                       "el %.1f%% de los vertices)\n",
                                       (unsigned)distinct,
                                       options.textures_final
                                           ? "el fotograma final"
                                           : "el vuelo",
                                       (unsigned)used_count,
                                       total != 0
                                           ? 100.0 * (double)covered /
                                                 (double)total
                                           : 0.0);
                            }
                        }
                    }
                    free(counts);
                }
                if (used == NULL) {
                    used_count = 0;
                    used = (uint32_t *)calloc(scene.geometry.vertex_count + 1u,
                                             sizeof(uint32_t));
                    if (used != NULL) {
                        for (vi = 0; vi < scene.geometry.vertex_count; vi++) {
                            uint32_t idx = scene.geometry.vertices[vi].texture &
                                           0x3FFFu;
                            uint32_t k;
                            int seen = 0;
                            for (k = 0; k < used_count; k++) {
                                if (used[k] == idx) {
                                    seen = 1;
                                    break;
                                }
                            }
                            if (!seen && used_count <
                                scene.geometry.vertex_count) {
                                used[used_count++] = idx;
                            }
                        }
                    }
                }
                if (gex_vfx_texture_bank_load(&texture_bank, vfx_path, used,
                                              used_count) == GEX_VFX_OK) {
                    /* S-7: el original elige la mezcla por comando y, para
                     * los comandos con `opcode & 2` y modo 0/1, la decisión
                     * depende de si la textura tiene canal alfa: el formato
                     * 12 (ARGB_4444) es el único que lo activa. La lista
                     * sale del propio .VFX (todos los registros, no solo los
                     * decodificados: un fallo de decodificación no cambia el
                     * formato del comando). */
                    {
                        uint16_t *alpha_tex = NULL;
                        size_t alpha_n = 0;
                        uint32_t ri;
                        if (texture_bank.vfx.record_count > 0) {
                            alpha_tex = (uint16_t *)calloc(
                                texture_bank.vfx.record_count,
                                sizeof(*alpha_tex));
                        }
                        if (alpha_tex != NULL) {
                            for (ri = 0; ri < texture_bank.vfx.record_count;
                                 ri++) {
                                if (texture_bank.vfx.records[ri].format ==
                                    GEX_VFX_FORMAT_ARGB_4444) {
                                    alpha_tex[alpha_n++] = (uint16_t)ri;
                                }
                            }
                            if (gex_scene_level_set_alpha_textures(
                                    &scene, alpha_tex, alpha_n) !=
                                GEX_SCENE_OK) {
                                fprintf(stderr,
                                        "error: no se pudo asignar la mezcla "
                                        "por comando\n");
                            } else if (gex_d3d11_mesh_update(
                                           d3d11_dev.device, &scene.geometry,
                                           &d3d11_mesh) !=
                                       GEX_D3D11_MESH_OK) {
                                fprintf(stderr,
                                        "error: mesh update failed (blend)\n");
                            } else {
                                printf("blend: %u texturas ARGB_4444 de '%s' "
                                       "(tramos=%zu)\n",
                                       (unsigned)alpha_n, vfx_path,
                                       scene.geometry.run_count);
                            }
                            free(alpha_tex);
                        }
                    }
                    texture_status = gex_d3d11_texture_set_create(
                        &d3d11_textures, &d3d11_dev, &texture_bank,
                        GEX_NCC_UV_MODE_TEXEL);
                    if (texture_status == GEX_D3D11_TEXTURE_OK) {
                        gex_d3d11_pipeline_set_textures(&d3d11_pipeline,
                                                        &d3d11_textures);
                        printf("textures: %u decodificadas de '%s' "
                               "(atlas %u x %u), %u fallidas\n",
                               texture_bank.decoded_count, vfx_path,
                               d3d11_textures.atlas_width,
                               d3d11_textures.atlas_height,
                               texture_bank.failed_count);
                    } else {
                        fprintf(stderr, "error: textures '%s': %s\n",
                                vfx_path,
                                gex_d3d11_texture_status_name(texture_status));
                    }
                } else {
                    fprintf(stderr, "error: no se pudo cargar el VFX '%s'\n",
                            vfx_path);
                }
                free(used);
            } else {
                fprintf(stderr, "error: no se pudo deducir el VFX de '%s'\n",
                        options.level_path);
            }
        }

        if (geometry_status == GEX_D3D11_RENDERER_OK) {
            if (options.level_path != NULL) {
                const gex_scene_stats *s = &scene.stats;
                printf("geometry: level '%s' (initial frame)\n", options.level_path);
                printf("  level: entries=%u vertices=%u polygons=%u leaves=%u\n",
                       s->container_entries, s->descriptor_vertices,
                       s->descriptor_polygons, s->leaf_count);
                printf("  emitted: triangles=%zu (fog passes=%zu) "
                       "discarded by depth=%zu\n",
                       s->triangles, s->fog_triangles, s->skipped_clipped);
                printf("  geometry: vertices=%zu indices=%zu\n",
                       s->vertices, s->indices);
                if (s->camera_mode) {
                    printf("  view: ORIGINAL GAME CAMERA (cam+0xE0)\n");
                    printf("    emit matrix: [%d %d %d] [%d %d %d] "
                           "[%d %d %d]\n",
                           s->emit_m[0], s->emit_m[1], s->emit_m[2],
                           s->emit_m[3], s->emit_m[4], s->emit_m[5],
                           s->emit_m[6], s->emit_m[7], s->emit_m[8]);
                    printf("    translation: (%d, %d, %d)\n",
                           s->emit_t[0], s->emit_t[1], s->emit_t[2]);
                    printf("    projection: focal=%d cx=%d cy=%d near_z=%d\n",
                           s->proj_focal, s->view_cx, s->view_cy,
                           s->proj_near_z);
                    if (scene.path_loaded) {
                        printf("    camera path: %d records of 32 B, "
                               "variant %u, flags %u\n",
                               (int)scene.dfx_path.path.count,
                               (unsigned)scene.dfx_path.path.variant,
                               (unsigned)scene.dfx_path.path.flags);
                    }
                } else {                    printf("  view: DIAGNOSTIC (synthetic matrix + automatic "
                           "framing): scale=0x%X cx=%d cy=%d\n",
                           (unsigned)s->view_scale, s->view_cx, s->view_cy);
                }
            } else {
                printf("geometry: cube, %u triangles, vertex colors, no "
                       "textures (%ux%u logical)\n",
                       (unsigned)GEX_SHAPE_CUBE_TRIANGLES,
                       (unsigned)GEX_D3D11_REFERENCE_WIDTH,
                       (unsigned)GEX_D3D11_REFERENCE_HEIGHT);
            }
        } else {
            printf("geometry: none (pipeline: %s, shape: %s, level: %s, "
                   "mesh: %s)\n",
                   gex_d3d11_pipeline_status_name(pipeline_status),
                   (shape_status < 0)
                       ? "not attempted"
                       : ((shape_status == 0) ? "ok" : "out of memory"),
                   gex_scene_status_name(scene_status),
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

#if defined(_WIN32)
    /* Secuencia de camara del menu (S-3): se engancha al update logico del
     * bucle, de modo que hay un paso de trayectoria por update (como en el
     * original: 0x411B94 avanza el nodo una vez por update). */
    if (options.camera_path && scene_status == GEX_SCENE_OK &&
        scene.path_loaded && geometry_status == GEX_D3D11_RENDERER_OK) {
        path_state.scene = &scene;
        path_state.cam = &path_cam;
        path_state.device = d3d11_dev.device;
        path_state.mesh = &d3d11_mesh;
        path_state.samples = 0;
        path_state.finished = 0;
        path_state.frozen = 0;
        path_state.error = 0;
        path_state.menu = options.menu;
        /* Captura reproducible (S-8): avanzar exactamente N muestras antes
         * de entrar en el bucle y congelar la camara. Sin esta opcion el
         * numero de muestras por present depende del reloj y dos capturas
         * del "mismo" frame pueden caer en poses distintas. */
        if (options.camera_freeze_set) {
            uint64_t k;
            for (k = 0; k < options.camera_freeze; k++) {
                if (i4_camera_path_update(&path_state) != 0 ||
                    path_state.finished) {
                    break;
                }
            }
            path_state.frozen = 1;
            printf("camera path: frozen at sample %u, pos=(%d, %d, %d) "
                   "euler=(%d, %d, %d)\n",
                   (unsigned)path_state.samples, path_cam.pos[0],
                   path_cam.pos[1], path_cam.pos[2], path_cam.euler[0],
                   path_cam.euler[1], path_cam.euler[2]);
            /* Las estadisticas impresas antes de congelar son las del
             * fotograma inicial; las de la pose congelada son las que se van a
             * capturar. */
            printf("camera path: frozen pose stats: leaves=%u "
                   "triangles=%zu vertices=%zu indices=%zu\n",
                   scene.stats.leaf_count, scene.stats.triangles,
                   scene.stats.vertices, scene.stats.indices);
        }
        gex_i4_app_set_update_callback(&app, i4_camera_path_update, &path_state);
        g_camera_path_state = &path_state;
        printf("camera path: playing (%d records, one step per logical "
               "update)\n", (int)scene.dfx_path.path.count);
    }
#endif

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
#if defined(_WIN32)
        /* La captura se pide justo antes del step que producira el Present
         * objetivo: el renderer la atiende dentro de ese Present, con la
         * geometria de ese fotograma ya dibujada. */
        if (!screenshot_requested && options.screenshot_path != NULL &&
            app.d3d11_enabled && screenshot_target > 0 &&
            app.renderer.d3d11_renderer.presents + 1u >= screenshot_target) {
            screenshot_requested = 1;
            if (gex_d3d11_renderer_request_capture(
                    &app.renderer.d3d11_renderer,
                    options.screenshot_path) != GEX_D3D11_RENDERER_OK) {
                fprintf(stderr, "error: could not request screenshot '%s'\n",
                        options.screenshot_path);
            }
        }
#endif
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
#if defined(_WIN32)
    if (options.screenshot_path != NULL && !screenshot_requested) {
        fprintf(stderr, "warning: screenshot frame %" PRIu64
                        " was never reached\n", screenshot_target);
    }
#endif
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
    /* 2b. Las texturas del nivel (Módulo 4C): el pipeline ya está apagado. */
    gex_d3d11_texture_set_destroy(&d3d11_textures);
    gex_vfx_texture_bank_free(&texture_bank);
    /* 3. La geometría en memoria del host: el cubo y el nivel cargado. */
    gex_emit_output_free(&cube_emit);
    gex_scene_level_free(&scene);
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
