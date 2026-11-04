/*
 * d3d11probe.c - R-1 console probe: create and destroy a D3D11 device in
 * hardware and WARP mode and print what the runtime reported.
 *
 * This is a diagnostic tool, not a test: the automated suite lives in
 * tests/test_d3d11_device.c. Exit code 0 when the WARP baseline works
 * (hardware may legitimately be absent), 1 otherwise. No swapchain, no
 * window, no drawing: that is R-2 and later.
 */

#include "gex/render/d3d11_device.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void probe(gex_d3d11_driver driver, const char *label)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_device_status status;
    HRESULT hr;

    status = gex_d3d11_device_init(&dev, driver, 0);
    hr = dev.last_hresult;
    if (status == GEX_D3D11_DEVICE_OK) {
        printf("%-8s ok      feature level %s  flags 0x%" PRIX32
               "  hr 0x%08lX\n",
               label, gex_d3d11_feature_level_name(dev.feature_level),
               (uint32_t)dev.creation_flags, (unsigned long)hr);
        gex_d3d11_device_shutdown(&dev);
        printf("%-8s closed  (no references left)\n", label);
    } else {
        printf("%-8s FAILED  %s  hr 0x%08lX\n", label,
               gex_d3d11_device_status_name(status), (unsigned long)hr);
    }
}

int main(void)
{
    UINT i;

    printf("D3D11 device probe (R-1, no swapchain, no drawing)\n");
    printf("requested feature levels:");
    for (i = 0; i < GEX_D3D11_FEATURE_LEVEL_COUNT; i++) {
        printf(" %s", gex_d3d11_feature_level_name(gex_d3d11_feature_levels[i]));
    }
    printf("\n");

    probe(GEX_D3D11_DRIVER_HARDWARE, "hardware");
    probe(GEX_D3D11_DRIVER_WARP, "warp");

    /* The WARP baseline decides the exit code: hardware may be absent, the
     * software rasterizer may not (Windows 8+). */
    {
        gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
        gex_d3d11_device_status status;
        status = gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0);
        gex_d3d11_device_shutdown(&dev);
        printf("baseline: %s\n", gex_d3d11_device_status_name(status));
        return (status == GEX_D3D11_DEVICE_OK) ? 0 : 1;
    }
}
