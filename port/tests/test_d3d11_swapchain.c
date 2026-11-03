/*
 * test_d3d11_swapchain.c - R-2 swapchain checks (Windows-only).
 *
 * Real window + real DXGI operations, WARP first (GPU-independent), then
 * hardware when the machine has one. Nothing presents: Present belongs to
 * R-3 (docs/D3D11_SWAPCHAIN.md). What each group verifies:
 *
 *   - argument/state errors: NULLs, a dead HWND, zero sizes, a closed R-1
 *     device, double init (without touching the live swap chain);
 *   - WARP: creation on the real window, DXGI_SWAP_CHAIN_DESC matches the
 *     request (size, format, 2 buffers, windowed, bitblt, render-target
 *     usage), backbuffer obtained with the right texture desc and exactly
 *     one reference;
 *   - resize: valid, repeated, same-size; zero/partial sizes refused with
 *     BAD_SIZE and no DXGI call; the swap chain stays usable afterwards;
 *   - COM: swap chain and backbuffer start with one reference, survive
 *     shutdown only on a manual reference, and the final Release returns 0
 *     (nothing left pending, destruction order respected);
 *   - lifecycle: init -> shutdown -> init again, two swap chains over two
 *     windows on one device;
 *   - hardware: same core checks when a GPU is available; documented skip
 *     otherwise (never a faked pass);
 *   - error path: an absurd buffer count is rejected by DXGI with a fully
 *     closed struct (HRESULT recorded);
 *   - dead window: creating with a destroyed HWND is refused; a swap chain
 *     whose window dies keeps a coherent struct and shuts down cleanly
 *     (the resize HRESULT is recorded, both outcomes accepted);
 *   - destruction order: swap chain released before the device in every
 *     full-release check.
 */

#include "gex/render/d3d11_swapchain.h"
#include "gex/platform/win32_window.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int checks;
static int failures;
static int skipped;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

static int make_window(gex_win32_window *win, int width, int height)
{
    memset(win, 0, sizeof(*win));
    return gex_win32_window_create(win, NULL, L"R-2 swapchain test window",
                                   width, height);
}

static gex_d3d11_swapchain_config
fill_config(HWND hwnd, UINT width, UINT height)
{
    gex_d3d11_swapchain_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.hwnd = hwnd;
    cfg.width = width;
    cfg.height = height;
    cfg.buffer_count = 0; /* default 2 */
    cfg.format = 0;       /* default R8G8B8A8_UNORM */
    cfg.vsync_retraces = 3; /* the original's grBufferSwap(3), stored only */
    return cfg;
}

/* Live reference count of the device (its base is > 1: R-1 finding). */
static ULONG device_refs(ID3D11Device *device)
{
    ULONG refs = ID3D11Device_AddRef(device);
    (void)ID3D11Device_Release(device);
    return refs - 1;
}

/* Everything that must hold right after a successful init. */
static void check_live_swapchain(gex_d3d11_swapchain *sw, HWND hwnd,
                                 UINT width, UINT height)
{
    D3D11_TEXTURE2D_DESC tex;
    ULONG refs;

    CHECK(sw->initialized == 1);
    CHECK(sw->swapchain != NULL);
    CHECK(sw->backbuffer != NULL);
    CHECK(sw->last_hresult == S_OK);

    CHECK(sw->desc.BufferDesc.Width == width);
    CHECK(sw->desc.BufferDesc.Height == height);
    CHECK(sw->desc.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(sw->desc.BufferCount == 2);
    CHECK(sw->desc.Windowed == TRUE);
    CHECK(sw->desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD);
    CHECK((sw->desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) != 0);
    CHECK(sw->desc.SampleDesc.Count == 1);
    CHECK(sw->desc.OutputWindow == hwnd);

    /* Backbuffer accessor and texture description. */
    CHECK(gex_d3d11_swapchain_backbuffer(sw) == sw->backbuffer);
    ID3D11Texture2D_GetDesc(sw->backbuffer, &tex);
    CHECK(tex.Width == width);
    CHECK(tex.Height == height);
    CHECK(tex.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(tex.MipLevels == 1);
    CHECK(tex.ArraySize == 1);
    CHECK(tex.SampleDesc.Count == 1);
    CHECK(tex.Usage == D3D11_USAGE_DEFAULT);

    /* COM hygiene: one reference per out-pointer. */
    refs = ID3D11Texture2D_AddRef(sw->backbuffer);
    CHECK(refs == 2);
    CHECK(ID3D11Texture2D_Release(sw->backbuffer) == 1);
}

/*
 * Full-release check for both COM objects.
 *
 * Measured refcounts (identical in WARP and hardware, R-2):
 *
 *   after init:  swap chain 2  (one owned by this module, one held by its
 *                               own buffer: a DXGI buffer keeps the swap
 *                               chain alive, which is why releasing the swap
 *                               chain while a buffer reference exists does
 *                               NOT reach 0)
 *                backbuffer 1  (owned by this module)
 *
 * The check therefore holds one manual reference of each, lets shutdown
 * release the module's own references (backbuffer first, then swap chain),
 * and then releases the manual references in the only order that reaches
 * zero: the backbuffer first (that releases the swap chain reference it
 * holds), then the swap chain. Both final Release calls must return 0: no
 * COM reference is left pending and the destruction order is the one
 * required by the milestone (dependent resources, swap chain, device — the
 * device is not touched here and is released by the caller afterwards).
 */
static void check_full_release(gex_d3d11_swapchain *sw)
{
    IDXGISwapChain *held_sw = sw->swapchain;
    ID3D11Texture2D *held_bb = sw->backbuffer;
    DXGI_SWAP_CHAIN_DESC desc;
    D3D11_TEXTURE2D_DESC tex;
    ULONG sw_refs;
    ULONG bb_refs;
    ULONG remaining;

    CHECK(held_sw != NULL);
    CHECK(held_bb != NULL);

    sw_refs = IDXGISwapChain_AddRef(held_sw);
    bb_refs = ID3D11Texture2D_AddRef(held_bb);
    printf("refs before shutdown: swapchain %lu, backbuffer %lu\n",
           (unsigned long)sw_refs, (unsigned long)bb_refs);
    CHECK(sw_refs == 3); /* module + buffer + manual */
    CHECK(bb_refs == 2); /* module + manual */

    gex_d3d11_swapchain_shutdown(sw);
    CHECK(sw->swapchain == NULL);
    CHECK(sw->backbuffer == NULL);
    CHECK(sw->initialized == 0);

    /* Both objects still alive on the manual references: */
    CHECK(IDXGISwapChain_GetDesc(held_sw, &desc) == S_OK);
    ID3D11Texture2D_GetDesc(held_bb, &tex); /* void: live-or-crash smoke */
    CHECK(tex.Width > 0);

    /* Dependent resource first, then the swap chain. */
    remaining = ID3D11Texture2D_Release(held_bb);
    CHECK(remaining == 0);
    remaining = IDXGISwapChain_Release(held_sw);
    CHECK(remaining == 0);
}

int main(void)
{
    gex_win32_window win;
    gex_win32_window win_second;
    gex_win32_window win_inval;
    gex_d3d11_device dev;
    gex_d3d11_swapchain sw;
    gex_d3d11_swapchain sw_other;
    gex_d3d11_swapchain_config cfg;
    gex_d3d11_swapchain_status st;
    ULONG refs_base;
    ULONG refs_with_sw;
    int i;

    /* ---- names ------------------------------------------------------- */
    CHECK(gex_d3d11_swapchain_status_name(GEX_D3D11_SWAPCHAIN_OK) != NULL);
    CHECK(gex_d3d11_swapchain_status_name(GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT) !=
          NULL);
    CHECK(gex_d3d11_swapchain_status_name(GEX_D3D11_SWAPCHAIN_BAD_STATE) !=
          NULL);
    CHECK(gex_d3d11_swapchain_status_name(GEX_D3D11_SWAPCHAIN_BAD_SIZE) !=
          NULL);
    CHECK(gex_d3d11_swapchain_status_name(GEX_D3D11_SWAPCHAIN_CREATE_FAILED) !=
          NULL);
    CHECK(gex_d3d11_swapchain_status_name(GEX_D3D11_SWAPCHAIN_RESIZE_FAILED) !=
          NULL);
    CHECK(gex_d3d11_swapchain_status_name((gex_d3d11_swapchain_status)77) !=
          NULL);

    /* ---- the real window of I-4 --------------------------------------- */
    CHECK(make_window(&win, 480, 360) == 1);
    CHECK(win.hwnd != NULL && IsWindow(win.hwnd));

    /* ---- argument and state errors ------------------------------------ */
    cfg = fill_config(win.hwnd, 320, 240);
    memset(&dev, 0, sizeof(dev));
    memset(&sw, 0, sizeof(sw));

    CHECK(gex_d3d11_swapchain_init(NULL, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    CHECK(gex_d3d11_swapchain_init(&sw, NULL, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, NULL) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    /* closed (zeroed) R-1 device */
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    /* A closed but dirty struct is zeroed on failure (documented contract):
     * nothing half-open survives an init that never reached DXGI. */
    {
        gex_d3d11_swapchain dirty;
        memset(&dirty, 0xAB, sizeof(dirty));
        dirty.initialized = 0;
        CHECK(gex_d3d11_swapchain_init(&dirty, &dev, &cfg) ==
              GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
        CHECK(dirty.swapchain == NULL);
        CHECK(dirty.backbuffer == NULL);
        CHECK(dirty.last_hresult == 0);
        CHECK(dirty.initialized == 0);
    }

    cfg = fill_config(NULL, 320, 240);
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    cfg = fill_config((HWND)(uintptr_t)0x1, 320, 240); /* fake handle */
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    cfg = fill_config(win.hwnd, 0, 240);
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    cfg = fill_config(win.hwnd, 320, 0);
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);

    /* resize argument errors on a closed swap chain */
    CHECK(gex_d3d11_swapchain_resize(NULL, 10, 10) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    CHECK(gex_d3d11_swapchain_resize(&sw, 10, 10) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);
    CHECK(gex_d3d11_swapchain_backbuffer(NULL) == NULL);
    CHECK(gex_d3d11_swapchain_backbuffer(&sw) == NULL);
    gex_d3d11_swapchain_shutdown(NULL);  /* no crash */
    gex_d3d11_swapchain_shutdown(&sw);   /* zeroed struct: no-op */
    CHECK(1);

    /* ---- WARP: creation, desc, backbuffer, resize, lifecycle ---------- */
    memset(&dev, 0, sizeof(dev));
    CHECK(gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
          GEX_D3D11_DEVICE_OK);
    printf("warp: device ok, feature level %s\n",
           gex_d3d11_feature_level_name(dev.feature_level));

    memset(&sw, 0, sizeof(sw));
    refs_base = device_refs(dev.device);
    printf("device refs before swapchain: %lu\n", (unsigned long)refs_base);
    cfg = fill_config(win.hwnd, 320, 240);
    st = gex_d3d11_swapchain_init(&sw, &dev, &cfg);
    CHECK(st == GEX_D3D11_SWAPCHAIN_OK);
    if (st == GEX_D3D11_SWAPCHAIN_OK) {
        /* The swap chain keeps its device alive: this is why the device is
         * released last (destruction order required by R-2). */
        refs_with_sw = device_refs(dev.device);
        printf("device refs with swapchain: %lu\n", (unsigned long)refs_with_sw);
        CHECK(refs_with_sw > refs_base);
        printf("warp: swapchain ok %ux%u, %u buffers, format %d\n",
               (unsigned)sw.desc.BufferDesc.Width,
               (unsigned)sw.desc.BufferDesc.Height,
               (unsigned)sw.desc.BufferCount,
               (int)sw.desc.BufferDesc.Format);
        check_live_swapchain(&sw, win.hwnd, 320, 240);

        /* resize to the original game's backbuffer size */
        CHECK(gex_d3d11_swapchain_resize(&sw, 512, 384) ==
              GEX_D3D11_SWAPCHAIN_OK);
        CHECK(sw.desc.BufferDesc.Width == 512);
        CHECK(sw.desc.BufferDesc.Height == 384);
        check_live_swapchain(&sw, win.hwnd, 512, 384);

        /* same-size resize and repeated resizes */
        CHECK(gex_d3d11_swapchain_resize(&sw, 512, 384) ==
              GEX_D3D11_SWAPCHAIN_OK);
        CHECK(gex_d3d11_swapchain_resize(&sw, 384, 512) ==
              GEX_D3D11_SWAPCHAIN_OK);
        CHECK(gex_d3d11_swapchain_resize(&sw, 240, 240) ==
              GEX_D3D11_SWAPCHAIN_OK);
        CHECK(gex_d3d11_swapchain_resize(&sw, 640, 480) ==
              GEX_D3D11_SWAPCHAIN_OK);
        check_live_swapchain(&sw, win.hwnd, 640, 480);

        /* Real window resize: change the client area with SetWindowPos and
         * feed the resulting client size to ResizeBuffers, exactly the path
         * the future WM_SIZE wiring will take. */
        {
            RECT rc;
            UINT cw;
            UINT ch;
            CHECK(SetWindowPos(win.hwnd, NULL, 0, 0, 700, 520,
                               SWP_NOMOVE | SWP_NOZORDER) != 0);
            CHECK(GetClientRect(win.hwnd, &rc) != 0);
            cw = (UINT)(rc.right - rc.left);
            ch = (UINT)(rc.bottom - rc.top);
            CHECK(cw > 0 && ch > 0);
            printf("window resize: client %ux%u\n", (unsigned)cw,
                   (unsigned)ch);
            CHECK(gex_d3d11_swapchain_resize(&sw, cw, ch) ==
                  GEX_D3D11_SWAPCHAIN_OK);
            check_live_swapchain(&sw, win.hwnd, cw, ch);
        }

        /* zero / partial sizes: refused without touching DXGI */
        CHECK(gex_d3d11_swapchain_resize(&sw, 0, 0) ==
              GEX_D3D11_SWAPCHAIN_BAD_SIZE);
        CHECK(gex_d3d11_swapchain_resize(&sw, 0, 480) ==
              GEX_D3D11_SWAPCHAIN_BAD_SIZE);
        CHECK(gex_d3d11_swapchain_resize(&sw, 640, 0) ==
              GEX_D3D11_SWAPCHAIN_BAD_SIZE);
        /* still usable after the refusals */
        CHECK(gex_d3d11_swapchain_resize(&sw, 320, 240) ==
              GEX_D3D11_SWAPCHAIN_OK);
        check_live_swapchain(&sw, win.hwnd, 320, 240);

        /* double init refused, live swap chain untouched */
        {
            IDXGISwapChain *same = sw.swapchain;
            cfg = fill_config(win.hwnd, 640, 480);
            CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
                  GEX_D3D11_SWAPCHAIN_BAD_STATE);
            CHECK(sw.swapchain == same);
            CHECK(sw.initialized == 1);
        }

        /* full COM release: swap chain first, device stays with the caller */
        check_full_release(&sw);
        refs_with_sw = device_refs(dev.device);
        printf("device refs after full release: %lu\n",
               (unsigned long)refs_with_sw);
        CHECK(refs_with_sw == refs_base); /* swap chain released the device */

        /* idempotent shutdown + re-init cycle (twice) */
        gex_d3d11_swapchain_shutdown(&sw);
        CHECK(sw.initialized == 0);
        for (i = 0; i < 2; i++) {
            cfg = fill_config(win.hwnd, 320, 240);
            CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
                  GEX_D3D11_SWAPCHAIN_OK);
            check_live_swapchain(&sw, win.hwnd, 320, 240);
            CHECK(gex_d3d11_swapchain_resize(&sw, 512, 384) ==
                  GEX_D3D11_SWAPCHAIN_OK);
            check_full_release(&sw);
        }
    } else {
        printf("warp: swapchain FAILED (hr=0x%08lX, %s)\n",
               (unsigned long)sw.last_hresult,
               gex_d3d11_swapchain_status_name(st));
        failures++; /* WARP is the guaranteed baseline */
    }

    /* ---- two swap chains over two windows, one device ------------------ */
    /* The WARP device from the block above is still alive: two swap chains
     * share one device (no second device, no duplicated context). */
    CHECK(make_window(&win_second, 240, 180) == 1);
    CHECK(dev.initialized == 1);
    memset(&sw, 0, sizeof(sw));
    memset(&sw_other, 0, sizeof(sw_other));
    cfg = fill_config(win.hwnd, 320, 240);
    if (gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
        GEX_D3D11_SWAPCHAIN_OK) {
        cfg = fill_config(win_second.hwnd, 200, 150);
        CHECK(gex_d3d11_swapchain_init(&sw_other, &dev, &cfg) ==
              GEX_D3D11_SWAPCHAIN_OK);
        CHECK(sw.swapchain != NULL && sw_other.swapchain != NULL);
        CHECK(sw.swapchain != sw_other.swapchain);
        check_live_swapchain(&sw_other, win_second.hwnd, 200, 150);
        CHECK(gex_d3d11_swapchain_resize(&sw_other, 300, 200) ==
              GEX_D3D11_SWAPCHAIN_OK);
        check_live_swapchain(&sw_other, win_second.hwnd, 300, 200);
        check_live_swapchain(&sw, win.hwnd, 320, 240); /* first intact */
        check_full_release(&sw_other);
        check_full_release(&sw);
    } else {
        failures++; /* unreachable if the WARP block above passed */
    }
    gex_d3d11_device_shutdown(&dev);
    gex_win32_window_destroy(&win_second);

    /* ---- absurd buffer count: DXGI rejects, struct stays closed ------- */
    CHECK(gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
          GEX_D3D11_DEVICE_OK);
    memset(&sw, 0, sizeof(sw));
    cfg = fill_config(win.hwnd, 320, 240);
    cfg.buffer_count = 1000;
    st = gex_d3d11_swapchain_init(&sw, &dev, &cfg);
    printf("buffer_count=1000: status=%s hr=0x%08lX\n",
           gex_d3d11_swapchain_status_name(st),
           (unsigned long)sw.last_hresult);
    CHECK(st == GEX_D3D11_SWAPCHAIN_CREATE_FAILED);
    CHECK(sw.swapchain == NULL);
    CHECK(sw.backbuffer == NULL);
    CHECK(sw.initialized == 0);
    gex_d3d11_device_shutdown(&dev);

    /* ---- the window stops being valid ---------------------------------- */
    /* (a) creating with a destroyed HWND is refused deterministically: */
    CHECK(make_window(&win_inval, 320, 240) == 1);
    CHECK(IsWindow(win_inval.hwnd));
    gex_win32_window_destroy(&win_inval);
    CHECK(!IsWindow(win_inval.hwnd));
    CHECK(gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
          GEX_D3D11_DEVICE_OK);
    memset(&sw, 0, sizeof(sw));
    cfg = fill_config(win_inval.hwnd, 320, 240);
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT);

    /* (b) a live swap chain whose window dies afterwards: observational.
     * The struct must stay coherent and the shutdown clean; the resize
     * outcome (OK or RESIZE_FAILED) is recorded, not assumed. */
    CHECK(make_window(&win_inval, 320, 240) == 1);
    memset(&sw, 0, sizeof(sw));
    cfg = fill_config(win_inval.hwnd, 320, 240);
    CHECK(gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
          GEX_D3D11_SWAPCHAIN_OK);
    gex_win32_window_destroy(&win_inval);
    st = gex_d3d11_swapchain_resize(&sw, 512, 384);
    printf("resize after window destroyed: status=%s hr=0x%08lX\n",
           gex_d3d11_swapchain_status_name(st),
           (unsigned long)sw.last_hresult);
    CHECK(st == GEX_D3D11_SWAPCHAIN_OK ||
          st == GEX_D3D11_SWAPCHAIN_RESIZE_FAILED);
    CHECK(sw.initialized == 1); /* struct coherent either way */
    /* Hold manual references: both objects must close cleanly even though
     * the window is gone (the swap chain outlives the HWND it was made
     * for; only presentation depends on it, and R-3 owns that). */
    check_full_release(&sw);
    gex_d3d11_device_shutdown(&dev);

    /* ---- hardware ------------------------------------------------------ */
    memset(&dev, 0, sizeof(dev));
    printf("hardware: creating D3D_DRIVER_TYPE_HARDWARE device...\n");
    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_HARDWARE, 0) ==
        GEX_D3D11_DEVICE_OK) {
        memset(&sw, 0, sizeof(sw));
        cfg = fill_config(win.hwnd, 320, 240);
        st = gex_d3d11_swapchain_init(&sw, &dev, &cfg);
        CHECK(st == GEX_D3D11_SWAPCHAIN_OK);
        if (st == GEX_D3D11_SWAPCHAIN_OK) {
            printf("hardware: swapchain ok %ux%u\n",
                   (unsigned)sw.desc.BufferDesc.Width,
                   (unsigned)sw.desc.BufferDesc.Height);
            check_live_swapchain(&sw, win.hwnd, 320, 240);
            CHECK(gex_d3d11_swapchain_resize(&sw, 512, 384) ==
                  GEX_D3D11_SWAPCHAIN_OK);
            check_live_swapchain(&sw, win.hwnd, 512, 384);
            check_full_release(&sw);
        }
        gex_d3d11_device_shutdown(&dev);
    } else {
        skipped++;
        printf("hardware: NOT AVAILABLE on this machine (hr=0x%08lX) - "
               "documented limitation, hardware block skipped\n",
               (unsigned long)dev.last_hresult);
    }

    gex_win32_window_destroy(&win);

    printf("%d checks, %d failures, %d skipped\n", checks, failures, skipped);
    return failures ? 1 : 0;
}
