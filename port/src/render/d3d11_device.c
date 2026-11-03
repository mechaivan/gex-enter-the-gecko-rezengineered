/*
 * d3d11_device.c - R-1: the Direct3D 11 device, hardware or WARP.
 *
 * The whole module is one documented entry point (D3D11CreateDevice,
 * d3d11.dll, SDK version D3D11_SDK_VERSION = 7) plus the two interfaces it
 * returns. COM rules, stated once and enforced everywhere:
 *
 *   - every out-pointer owns exactly one reference;
 *   - shutdown releases the context first, then the device, then zeroes the
 *     struct, so a second shutdown and an init-after-shutdown both work;
 *   - a failed init leaves the struct zeroed: the defensive branch releases
 *     pointers even in the (never observed) case where the runtime returned
 *     one together with a failed HRESULT.
 *
 * The module is Windows-only and excluded from the POSIX Makefile, exactly
 * like backend_dsound.c. Nothing in the portable core (loop_core, frame,
 * i4_app) knows this file exists.
 */

#include "gex/render/d3d11_device.h"

#if defined(_WIN32)

#include <string.h>
#include <d3d11.h>

const D3D_FEATURE_LEVEL
    gex_d3d11_feature_levels[GEX_D3D11_FEATURE_LEVEL_COUNT] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1};

static D3D_DRIVER_TYPE driver_type(gex_d3d11_driver driver)
{
    switch (driver) {
    case GEX_D3D11_DRIVER_HARDWARE:
        return D3D_DRIVER_TYPE_HARDWARE;
    case GEX_D3D11_DRIVER_WARP:
        return D3D_DRIVER_TYPE_WARP;
    default:
        return D3D_DRIVER_TYPE_UNKNOWN; /* not reachable through init() */
    }
}

static void release_partial(ID3D11DeviceContext *context,
                            ID3D11Device *device)
{
    if (context != NULL) {
        ID3D11DeviceContext_Release(context);
    }
    if (device != NULL) {
        ID3D11Device_Release(device);
    }
}

gex_d3d11_device_status gex_d3d11_device_init(gex_d3d11_device *dev,
                                              gex_d3d11_driver driver,
                                              UINT extra_flags)
{
    HRESULT hr;
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    D3D_FEATURE_LEVEL obtained = 0;

    if (dev == NULL) {
        return GEX_D3D11_DEVICE_BAD_ARGUMENT;
    }
    /*
     * State first: a live device is never touched, not even to validate the
     * remaining arguments. `initialized` is the only field read before the
     * module owns the struct (see the pre-condition in the header).
     */
    if (dev->initialized) {
        return GEX_D3D11_DEVICE_BAD_STATE;
    }
    if (driver != GEX_D3D11_DRIVER_HARDWARE &&
        driver != GEX_D3D11_DRIVER_WARP) {
        return GEX_D3D11_DEVICE_BAD_ARGUMENT; /* struct untouched */
    }

    /* Closed struct with valid arguments: from here on every failure leaves
     * it closed (zeroed), never half-open. */
    memset(dev, 0, sizeof(*dev));

    /* Check if debug layer was requested */
    dev->debug_layer_requested = (extra_flags & D3D11_CREATE_DEVICE_DEBUG) ? 1 : 0;
    dev->debug_layer_fallback = 0; /* Initially no fallback */
    
    /* Try to create device with debug layer first if requested */
    if (extra_flags & D3D11_CREATE_DEVICE_DEBUG) {
        hr = D3D11CreateDevice(NULL, driver_type(driver), NULL, extra_flags,
                               gex_d3d11_feature_levels,
                               GEX_D3D11_FEATURE_LEVEL_COUNT, D3D11_SDK_VERSION,
                               &device, &obtained, &context);
        
        /* If debug layer is not available, try without it */
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
            /* Remove debug flag and try again */
            extra_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            dev->debug_layer_fallback = 1; /* Mark that fallback was executed */
            hr = D3D11CreateDevice(NULL, driver_type(driver), NULL, extra_flags,
                                   gex_d3d11_feature_levels,
                                   GEX_D3D11_FEATURE_LEVEL_COUNT, D3D11_SDK_VERSION,
                                   &device, &obtained, &context);
        }
    } else {
        hr = D3D11CreateDevice(NULL, driver_type(driver), NULL, extra_flags,
                               gex_d3d11_feature_levels,
                               GEX_D3D11_FEATURE_LEVEL_COUNT, D3D11_SDK_VERSION,
                               &device, &obtained, &context);
    }
    
    dev->last_hresult = hr;
    if (FAILED(hr)) {
        release_partial(context, device);
        return GEX_D3D11_DEVICE_CREATE_FAILED;
    }
    if (device == NULL || context == NULL || obtained == 0) {
        /* The runtime reported success but did not deliver a usable triple;
         * treat it as a creation failure, not as a valid device. E_FAIL keeps
         * the invariant "CREATE_FAILED => FAILED(last_hresult)" true. */
        release_partial(context, device);
        memset(dev, 0, sizeof(*dev));
        dev->last_hresult = E_FAIL;
        return GEX_D3D11_DEVICE_CREATE_FAILED;
    }

    dev->device = device;
    dev->context = context;
    dev->feature_level = obtained;
    dev->creation_flags = ID3D11Device_GetCreationFlags(device); /* Store the actual flags used */
    dev->driver = driver;
    dev->initialized = 1;
    
    /* Check if debug layer was actually enabled (based on actual flags used) */
    dev->debug_layer_enabled = (dev->creation_flags & D3D11_CREATE_DEVICE_DEBUG) ? 1 : 0;
   
    return GEX_D3D11_DEVICE_OK;
}

void gex_d3d11_device_shutdown(gex_d3d11_device *dev)
{
    if (dev == NULL) {
        return;
    }
    if (dev->context != NULL) {
        ID3D11DeviceContext_Release(dev->context);
    }
    if (dev->device != NULL) {
        ID3D11Device_Release(dev->device);
    }
    memset(dev, 0, sizeof(*dev));
}

const char *gex_d3d11_device_status_name(gex_d3d11_device_status status)
{
    switch (status) {
    case GEX_D3D11_DEVICE_OK:
        return "ok";
    case GEX_D3D11_DEVICE_BAD_ARGUMENT:
        return "bad argument";
    case GEX_D3D11_DEVICE_BAD_STATE:
        return "bad state";
    case GEX_D3D11_DEVICE_CREATE_FAILED:
        return "create failed";
    }
    return "unknown";
}

const char *gex_d3d11_feature_level_name(D3D_FEATURE_LEVEL level)
{
    switch (level) {
    case D3D_FEATURE_LEVEL_11_1:
        return "11_1";
    case D3D_FEATURE_LEVEL_11_0:
        return "11_0";
    case D3D_FEATURE_LEVEL_10_1:
        return "10_1";
    case D3D_FEATURE_LEVEL_10_0:
        return "10_0";
    case D3D_FEATURE_LEVEL_9_3:
        return "9_3";
    case D3D_FEATURE_LEVEL_9_2:
        return "9_2";
    case D3D_FEATURE_LEVEL_9_1:
        return "9_1";
    case D3D_FEATURE_LEVEL_12_0:
        return "12_0";
    case D3D_FEATURE_LEVEL_12_1:
        return "12_1";
    case D3D_FEATURE_LEVEL_12_2:
        return "12_2";
    case D3D_FEATURE_LEVEL_1_0_CORE:
        return "1_0_core";
    }
    return "unknown";
}

#endif /* _WIN32 */
