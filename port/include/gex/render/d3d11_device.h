/*
 * gex/render/d3d11_device.h - the R-1 Direct3D 11 device (Windows-only).
 *
 * The smallest piece of the real renderer
 * (docs/RENDERER_ARCHITECTURE_RESEARCH.md section 6): create a device with
 * D3D11CreateDevice in hardware or WARP mode, record the feature level the
 * runtime actually honoured, expose the immediate context and release every
 * COM reference on shutdown. No swapchain, no window, no drawing: that is
 * R-2 and later.
 *
 * Windows-only on purpose (same rule as backend_dsound.h): portable code
 * must not include this header and the POSIX Makefile does not compile the
 * source. Direct3D 11 adds no third-party dependency: d3d11.h ships with the
 * toolchain (verified identical by MD5 in llvm-mingw and MSYS2 mingw64) and
 * d3d11.dll ships with Windows (msdn: req.dll = D3D11.dll, req.lib =
 * D3D11.lib).
 *
 * WARP is the software rasterizer of Windows: per the Microsoft WARP guide,
 * a D3D11 WARP device exists on Windows 7 (with KB971644) and Windows 8+,
 * and on Windows 8+ it reaches feature levels 9_1 through 11_1. It is the
 * deterministic baseline the project can test without a GPU.
 */

#ifndef GEX_RENDER_D3D11_DEVICE_H
#define GEX_RENDER_D3D11_DEVICE_H

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
/* WIDL C bindings: without COBJMACROS d3d11.h exposes only the raw vtables,
 * not the ID3D11Device_Release-style helpers. */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>

#include <d3d11.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which runtime path creates the device. */
typedef enum gex_d3d11_driver {
    GEX_D3D11_DRIVER_HARDWARE = 0, /* D3D_DRIVER_TYPE_HARDWARE: the default
                                    * adapter (first enumerated by DXGI) */
    GEX_D3D11_DRIVER_WARP = 1      /* D3D_DRIVER_TYPE_WARP: software rasterizer */
} gex_d3d11_driver;

typedef enum gex_d3d11_device_status {
    GEX_D3D11_DEVICE_OK = 0,
    GEX_D3D11_DEVICE_BAD_ARGUMENT,  /* NULL device or unknown driver */
    GEX_D3D11_DEVICE_BAD_STATE,     /* already initialized */
    GEX_D3D11_DEVICE_CREATE_FAILED  /* D3D11CreateDevice failed; see last_hresult */
} gex_d3d11_device_status;

/*
 * Feature levels requested, newest first. The runtime walks the list and
 * reports the first one it can honour. 11_1 is included because it is only
 * reachable when explicitly requested (Microsoft: a NULL list never returns
 * 11_1); on a system without the Direct3D 11.1 runtime the whole call fails
 * with E_INVALIDARG, a documented Windows 7-era caveat that does not apply
 * to the Windows 8+ baseline of this port.
 */
#define GEX_D3D11_FEATURE_LEVEL_COUNT 7
extern const D3D_FEATURE_LEVEL
    gex_d3d11_feature_levels[GEX_D3D11_FEATURE_LEVEL_COUNT];

typedef struct gex_d3d11_device {
    ID3D11Device *device;            /* one live reference after init */
    ID3D11DeviceContext *context;    /* immediate context, own reference */
    D3D_FEATURE_LEVEL feature_level; /* the level the runtime honoured */
    UINT creation_flags;             /* echo of GetCreationFlags */
    gex_d3d11_driver driver;
    HRESULT last_hresult;            /* S_OK or the creation failure */
    int initialized;
} gex_d3d11_device;

/*
 * Create the device. `extra_flags` is passed to D3D11CreateDevice verbatim
 * (0 by default; e.g. D3D11_CREATE_DEVICE_DEBUG needs the SDK layers
 * installed and fails cleanly with DXGI_ERROR_SDK_COMPONENT_MISSING when
 * they are absent). On any failure the struct is left zeroed: no partially
 * open resources. A second init on an initialized device is refused with
 * GEX_D3D11_DEVICE_BAD_STATE and changes nothing.
 */
gex_d3d11_device_status gex_d3d11_device_init(gex_d3d11_device *dev,
                                              gex_d3d11_driver driver,
                                              UINT extra_flags);

/* Release the context and the device, then zero the struct. Idempotent. */
void gex_d3d11_device_shutdown(gex_d3d11_device *dev);

const char *gex_d3d11_device_status_name(gex_d3d11_device_status status);
const char *gex_d3d11_feature_level_name(D3D_FEATURE_LEVEL level);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* GEX_RENDER_D3D11_DEVICE_H */
