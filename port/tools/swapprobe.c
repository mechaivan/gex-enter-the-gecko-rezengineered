/*
 * swapprobe.c - R-2 report: a real I-4 window, a real device and a real DXGI
 * swap chain, printed to the console. No Present, no clear, no drawing:
 * those belong to R-3. WARP by default (works on any machine); pass
 * --hardware to use the GPU.
 *
 * The window is the same class the frame loop uses (gex_win32_window), so the
 * probe exercises the real HWND path, not a test stub. The message queue is
 * pumped between steps and the window is closed through WM_CLOSE, exactly
 * like the loop does.
 */

#include "gex/platform/win32_window.h"
#include "gex/render/d3d11_swapchain.h"

#include <stdio.h>
#include <string.h>

static void pump(gex_win32_window *win)
{
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    (void)win;
}

static int report(gex_d3d11_driver driver, const char *label, HWND hwnd)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_swapchain sw = GEX_D3D11_SWAPCHAIN_INITIALIZER;
    gex_d3d11_swapchain_config cfg = GEX_D3D11_SWAPCHAIN_CONFIG_INITIALIZER;
    gex_d3d11_swapchain_status st;
    D3D11_TEXTURE2D_DESC tex;
    int failures = 0;

    if (gex_d3d11_device_init(&dev, driver, 0) != GEX_D3D11_DEVICE_OK) {
        printf("%s: device FAILED hr=0x%08lX\n", label,
               (unsigned long)dev.last_hresult);
        return 1;
    }
    printf("%s: device ok, feature level %s\n", label,
           gex_d3d11_feature_level_name(dev.feature_level));

    cfg.hwnd = hwnd;
    cfg.width = 512;
    cfg.height = 384;
    cfg.vsync_retraces = 3; /* stored for R-3; nothing presents yet */

    st = gex_d3d11_swapchain_init(&sw, &dev, &cfg);
    if (st != GEX_D3D11_SWAPCHAIN_OK) {
        printf("%s: swapchain FAILED %s hr=0x%08lX\n", label,
               gex_d3d11_swapchain_status_name(st),
               (unsigned long)sw.last_hresult);
        gex_d3d11_device_shutdown(&dev);
        return 1;
    }
    ID3D11Texture2D_GetDesc(sw.backbuffer, &tex);
    printf("%s: swapchain %ux%u, %u buffers, format %d, windowed %d, "
           "swap effect %d\n",
           label, (unsigned)sw.desc.BufferDesc.Width,
           (unsigned)sw.desc.BufferDesc.Height,
           (unsigned)sw.desc.BufferCount, (int)sw.desc.BufferDesc.Format,
           (int)sw.desc.Windowed, (int)sw.desc.SwapEffect);
    printf("%s: backbuffer texture %ux%u, mips %u, array %u, usage %d\n", label,
           (unsigned)tex.Width, (unsigned)tex.Height, (unsigned)tex.MipLevels,
           (unsigned)tex.ArraySize, (int)tex.Usage);

    /* Resize to the original's 512x384 backbuffer, then once more. */
    if (gex_d3d11_swapchain_resize(&sw, 512, 384) != GEX_D3D11_SWAPCHAIN_OK) {
        printf("%s: resize 512x384 FAILED hr=0x%08lX\n", label,
               (unsigned long)sw.last_hresult);
        failures++;
    } else {
        printf("%s: resize 512x384 ok\n", label);
    }
    if (gex_d3d11_swapchain_resize(&sw, 640, 480) != GEX_D3D11_SWAPCHAIN_OK) {
        printf("%s: resize 640x480 FAILED hr=0x%08lX\n", label,
               (unsigned long)sw.last_hresult);
        failures++;
    } else {
        printf("%s: resize 640x480 ok\n", label);
    }
    if (gex_d3d11_swapchain_resize(&sw, 0, 0) !=
        GEX_D3D11_SWAPCHAIN_BAD_SIZE) {
        printf("%s: 0x0 was not refused\n", label);
        failures++;
    } else {
        printf("%s: 0x0 refused without touching DXGI (minimized case)\n",
               label);
    }

    gex_d3d11_swapchain_shutdown(&sw);
    gex_d3d11_device_shutdown(&dev);
    printf("%s: closed (backbuffer, swapchain, device)\n", label);
    return failures;
}

int main(int argc, char **argv)
{
    gex_win32_window win;
    int hardware = 0;
    int failures = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hardware") == 0) {
            hardware = 1;
        } else {
            printf("usage: swapprobe [--hardware]\n");
            return 2;
        }
    }

    memset(&win, 0, sizeof(win));
    if (!gex_win32_window_create(&win, NULL, L"GEX R-2 swapchain probe", 640,
                                 480)) {
        printf("window creation FAILED\n");
        return 1;
    }
    pump(&win);

    failures += report(GEX_D3D11_DRIVER_WARP, "warp", win.hwnd);
    if (hardware) {
        failures += report(GEX_D3D11_DRIVER_HARDWARE, "hardware", win.hwnd);
    } else {
        printf("hardware: skipped (pass --hardware to include it)\n");
    }

    pump(&win);
    gex_win32_window_destroy(&win);
    printf("window destroyed\n");
    return failures ? 1 : 0;
}
