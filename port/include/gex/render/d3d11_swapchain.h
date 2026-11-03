/*
 * gex/render/d3d11_swapchain.h - the R-2 DXGI swapchain (Windows-only).
 *
 * Presentation infrastructure for the real renderer: a DXGI swap chain bound
 * to the existing Win32 window (the HWND of I-4, never a second window) and
 * built on top of the R-1 device. This milestone stops there on purpose
 * (docs/RENDERER_ARCHITECTURE_RESEARCH.md section 7, R-2):
 *
 *   - no Present() call yet: R-3 clears the backbuffer and presents;
 *     presenting an undefined backbuffer now would flash garbage;
 *   - no render target view yet: R-3 creates it from the backbuffer exposed
 *     here (gex_d3d11_swapchain_backbuffer);
 *   - no geometry, no textures, no Glide semantics.
 *
 * Design notes fixed for R-2 (full rationale in docs/D3D11_SWAPCHAIN.md):
 *
 *   - created through IDXGIFactory::CreateSwapChain, reached from the R-1
 *     device via IDXGIDevice -> IDXGIAdapter -> IDXGIFactory (a swap chain
 *     cannot be created from the device directly, and R-1 deliberately did
 *     not use D3D11CreateDeviceAndSwapChain);
 *   - bitblt model (DXGI_SWAP_EFFECT_DISCARD) with 2 buffers: it mirrors the
 *     2 color buffers the original requests in grSstWinOpen (0x0046324D) and
 *     it is the model that needs no extra factory rules. The flip-model
 *     comparison (D-1 of the I-5 research) stays deferred to R-4, where
 *     Present cadence can actually be measured; nothing here hard-codes the
 *     choice beyond this module;
 *   - DXGI_FORMAT_R8G8B8A8_UNORM, windowed, no MSAA, no mode switch;
 *   - ResizeBuffers: the backbuffer reference held here is released before
 *     the call and re-acquired afterwards (DXGI requirement), and a zero
 *     size (minimized window) is refused without touching DXGI.
 *
 * Measured reference counts after init (WARP and hardware, R-2): the swap
 * chain reports 2 references and the backbuffer 1. The extra swap chain
 * reference is held by its own buffer, so releasing every reference this
 * module owns still leaves the swap chain alive until the last buffer
 * reference is gone. Shutdown therefore releases the backbuffer first and
 * the swap chain second, and callers must do the same with any reference
 * they hold.
 *
 * Windows-only, like d3d11_device.h: portable code must not include this
 * header and the POSIX Makefile does not compile the source. No new
 * dependency: dxgi.h ships with the toolchain (same WIDL headers as d3d11.h,
 * identical by MD5 across llvm-mingw and MSYS2) and dxgi.dll with Windows;
 * the IIDs used here resolve from libuuid.a, hence -luuid (no -ldxgi is
 * needed: this path never calls CreateDXGIFactory).
 */

#ifndef GEX_RENDER_D3D11_SWAPCHAIN_H
#define GEX_RENDER_D3D11_SWAPCHAIN_H

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
/* WIDL C bindings: the ID3D11Device_/IDXGISwapChain_ helpers need this. */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>

#include <d3d11.h>

#include <stdint.h>

#include "gex/render/d3d11_device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GEX_D3D11_SWAPCHAIN_DEFAULT_BUFFER_COUNT 2u
#define GEX_D3D11_SWAPCHAIN_DEFAULT_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM

typedef struct gex_d3d11_swapchain_config {
    HWND hwnd;           /* the existing I-4 window; must be a live window */
    UINT width;          /* backbuffer width, >= 1 (explicit per milestone) */
    UINT height;         /* backbuffer height, >= 1 */
    UINT buffer_count;   /* 0 = default 2 (the original's 2 color buffers) */
    DXGI_FORMAT format;  /* 0 = default DXGI_FORMAT_R8G8B8A8_UNORM */
    /*
     * SyncInterval the presentation will use once R-3 presents: the direct
     * analogue of the 3 retraces of grBufferSwap(3) measured in the
     * original's frame step (docs/FRAME_STEP_TRACE.md 3.1). R-2 only stores
     * it; nothing presents yet.
     */
    UINT vsync_retraces;
} gex_d3d11_swapchain_config;

/*
 * The config is an input struct: init reads every field of it, so it must be
 * initialized too, with this macro or with zero. The macro leaves the
 * defaults in place (2 buffers, R8G8B8A8_UNORM) and an explicit size of 0
 * that init refuses, so a forgotten size fails loudly instead of silently
 * creating a 0x0 swap chain.
 */
#define GEX_D3D11_SWAPCHAIN_CONFIG_INITIALIZER                                \
    {                                                                         \
        .hwnd = NULL, .width = 0, .height = 0, .buffer_count = 0,              \
        .format = 0, .vsync_retraces = 0                                       \
    }

typedef enum gex_d3d11_swapchain_status {
    GEX_D3D11_SWAPCHAIN_OK = 0,
    GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT,  /* NULL device/config, closed R-1
                                        * device, dead/fake HWND, zero size */
    GEX_D3D11_SWAPCHAIN_BAD_STATE,     /* already initialized */
    GEX_D3D11_SWAPCHAIN_BAD_SIZE,      /* resize with 0 width/height: refused
                                        * without touching DXGI (minimized) */
    GEX_D3D11_SWAPCHAIN_CREATE_FAILED, /* CreateSwapChain/GetBuffer failed */
    GEX_D3D11_SWAPCHAIN_RESIZE_FAILED  /* ResizeBuffers failed; last_hresult
                                        * says why (incl. DEVICE_REMOVED) */
} gex_d3d11_swapchain_status;

typedef struct gex_d3d11_swapchain {
    IDXGISwapChain *swapchain;   /* one live reference after init */
    ID3D11Texture2D *backbuffer; /* buffer 0, own reference, re-acquired on
                                  * resize (released before ResizeBuffers) */
    DXGI_SWAP_CHAIN_DESC desc;   /* what DXGI actually created (zero when
                                  * the struct is closed) */
    HRESULT last_hresult;
    int initialized;             /* 1 = live; the only field init reads
                                  * before it owns the struct */
} gex_d3d11_swapchain;

/*
 * PRE-CONDITION, identical to the device (docs/D3D11_SWAPCHAIN.md section 4):
 * a gex_d3d11_swapchain must be initialized with
 * GEX_D3D11_SWAPCHAIN_INITIALIZER (or zeroed) before the first init, and the
 * gex_d3d11_device passed to it must be initialized the same way. init reads
 * no field of the swap chain except `initialized`, and reads it only to tell
 * a live swap chain (never touched) from a closed one (may be written).
 *
 *   gex_d3d11_swapchain sw = GEX_D3D11_SWAPCHAIN_INITIALIZER;
 *   if (gex_d3d11_swapchain_init(&sw, &dev, &cfg) ==
 *       GEX_D3D11_SWAPCHAIN_OK) { ... gex_d3d11_swapchain_shutdown(&sw); }
 *
 * shutdown leaves the struct exactly in the initializer state, so it can be
 * reused for another init (and a second shutdown is a no-op).
 */
#define GEX_D3D11_SWAPCHAIN_INITIALIZER                                       \
    {                                                                         \
        .swapchain = NULL, .backbuffer = NULL, .last_hresult = S_OK,           \
        .initialized = 0                                                      \
    }

/*
 * Create the swap chain for `dev` (R-1 device, not owned, not duplicated)
 * on `cfg->hwnd`. The factory/adapter/dxgi interfaces used to reach the
 * factory are released before returning: only swap chain + backbuffer stay.
 *
 * Exact outcome per case (also recorded in docs/D3D11_SWAPCHAIN.md section 4):
 *
 *   sw == NULL                         -> BAD_ARGUMENT; nothing read.
 *   sw live (initialized != 0)         -> BAD_STATE; the swap chain, its
 *                                         backbuffer and the caller's device
 *                                         are left untouched, never released.
 *                                         This check precedes every argument
 *                                         check, so a live swap chain is never
 *                                         even validated against.
 *   sw closed + dev/cfg NULL           -> BAD_ARGUMENT; the struct is NOT
 *                                         written.
 *   sw closed + closed/absent device,
 *   dead or fake HWND, zero size       -> BAD_ARGUMENT; the struct is NOT
 *                                         written (a rejected init never
 *                                         discards caller state).
 *   sw closed + valid arguments        -> the struct is zeroed and creation
 *                                         is attempted. On success it is live;
 *                                         on failure it stays zeroed (no
 *                                         partially open resources) with
 *                                         last_hresult carrying the HRESULT.
 *
 * A creation failure always leaves FAILED(last_hresult), including the
 * defensive "DXGI reported success but delivered no object" case, which
 * records E_FAIL.
 */
gex_d3d11_swapchain_status gex_d3d11_swapchain_init(
    gex_d3d11_swapchain *sw, const gex_d3d11_device *dev,
    const gex_d3d11_swapchain_config *cfg);

/*
 * ResizeBuffers with the same buffer count and format. Zero width or height
 * (minimized window) is refused with BAD_SIZE and no DXGI call. The held
 * backbuffer reference is released first and re-acquired on success; on
 * failure the backbuffer is re-acquired too, so the swap chain stays usable.
 *
 * R-3 contract: DXGI requires every reference to a back buffer released
 * before ResizeBuffers, so the caller must release its render target view
 * (and any other view made from gex_d3d11_swapchain_backbuffer) before
 * calling this. This module can only release the reference it owns.
 */
gex_d3d11_swapchain_status gex_d3d11_swapchain_resize(gex_d3d11_swapchain *sw,
                                                      UINT width, UINT height);

/*
 * Borrowed pointer to buffer 0 for R-3 (render target view). Valid until
 * the next resize or shutdown. Returns NULL on a closed swap chain.
 */
ID3D11Texture2D *
gex_d3d11_swapchain_backbuffer(const gex_d3d11_swapchain *sw);

/*
 * Release the backbuffer and the swap chain, then zero the struct. The
 * device is NOT touched: the caller releases it after the swap chain
 * (destruction order: backbuffer/dependent resources, then swap chain,
 * then device). Idempotent.
 */
void gex_d3d11_swapchain_shutdown(gex_d3d11_swapchain *sw);

const char *gex_d3d11_swapchain_status_name(gex_d3d11_swapchain_status status);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* GEX_RENDER_D3D11_SWAPCHAIN_H */
