/*
 * d3d11_swapchain.c - R-2: the DXGI swap chain bound to the I-4 window.
 *
 * One job, done with the COM rules stated once:
 *
 *   device (R-1, borrowed) -> IDXGIDevice -> IDXGIAdapter -> IDXGIFactory
 *          -> CreateSwapChain(hwnd, DISCARD, N buffers, windowed)
 *          -> GetBuffer(0) kept as the backbuffer for R-3
 *
 * The factory/adapter/dxgi-device interfaces are stepping stones: they are
 * released before init returns, so the module permanently holds exactly two
 * references (swap chain + backbuffer) and the caller keeps owning the
 * device. Shutdown releases backbuffer first, then swap chain, and zeroes
 * the struct; the device is released by the caller afterwards, which is the
 * required destruction order (dependent resources, then swap chain, then
 * device).
 *
 * Resize is the only operation DXGI constrains hard: every reference to the
 * back buffer must be gone before ResizeBuffers, so the held reference is
 * released first and re-acquired after (also on failure, so the swap chain
 * never ends up without its back buffer). A zero size is refused before
 * touching DXGI: a minimized window reports 0x0 and the caller decides to
 * skip the resize, which is also what the future frame loop will do.
 *
 * Windows-only and excluded from the POSIX Makefile, like d3d11_device.c.
 */

#include "gex/render/d3d11_swapchain.h"

#if defined(_WIN32)

#include <string.h>

static void release_staging(IDXGIDevice *dxgi_device, IDXGIAdapter *adapter,
                            IDXGIFactory *factory)
{
    if (factory != NULL) {
        IDXGIFactory_Release(factory);
    }
    if (adapter != NULL) {
        IDXGIAdapter_Release(adapter);
    }
    if (dxgi_device != NULL) {
        IDXGIDevice_Release(dxgi_device);
    }
}

static void release_partial(ID3D11Texture2D *backbuffer,
                            IDXGISwapChain *swapchain)
{
    if (backbuffer != NULL) {
        ID3D11Texture2D_Release(backbuffer);
    }
    if (swapchain != NULL) {
        IDXGISwapChain_Release(swapchain);
    }
}

gex_d3d11_swapchain_status gex_d3d11_swapchain_init(
    gex_d3d11_swapchain *sw, const gex_d3d11_device *dev,
    const gex_d3d11_swapchain_config *cfg)
{
    DXGI_SWAP_CHAIN_DESC sd;
    DXGI_SWAP_CHAIN_DESC actual;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory *factory = NULL;
    IDXGISwapChain *swapchain = NULL;
    ID3D11Texture2D *backbuffer = NULL;
    UINT buffer_count;
    DXGI_FORMAT format;
    HRESULT hr;

    if (sw == NULL || dev == NULL || cfg == NULL) {
        return GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT;
    }
    if (sw->initialized) {
        /* A live swap chain must not be touched by a failed init. */
        return GEX_D3D11_SWAPCHAIN_BAD_STATE;
    }

    /* Closed struct: zero it first, so any failure below leaves nothing
     * half-open and the documented "zeroed on failure" contract holds. */
    memset(sw, 0, sizeof(*sw));

    if (!dev->initialized || dev->device == NULL) {
        return GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT; /* closed R-1 device */
    }
    if (cfg->hwnd == NULL || !IsWindow(cfg->hwnd)) {
        return GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT; /* dead or fake HWND */
    }
    if (cfg->width == 0 || cfg->height == 0) {
        return GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT; /* explicit size required */
    }

    buffer_count = (cfg->buffer_count != 0) ? cfg->buffer_count
                                            : GEX_D3D11_SWAPCHAIN_DEFAULT_BUFFER_COUNT;
    format = (cfg->format != 0) ? cfg->format
                                : GEX_D3D11_SWAPCHAIN_DEFAULT_FORMAT;

    hr = ID3D11Device_QueryInterface(dev->device, &IID_IDXGIDevice,
                                     (void **)&dxgi_device);
    sw->last_hresult = hr;
    if (FAILED(hr)) {
        return GEX_D3D11_SWAPCHAIN_CREATE_FAILED;
    }
    hr = IDXGIDevice_GetParent(dxgi_device, &IID_IDXGIAdapter,
                               (void **)&adapter);
    if (FAILED(hr)) {
        sw->last_hresult = hr;
        release_staging(dxgi_device, NULL, NULL);
        return GEX_D3D11_SWAPCHAIN_CREATE_FAILED;
    }
    hr = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory,
                                (void **)&factory);
    if (FAILED(hr)) {
        sw->last_hresult = hr;
        release_staging(dxgi_device, adapter, NULL);
        return GEX_D3D11_SWAPCHAIN_CREATE_FAILED;
    }

    memset(&sd, 0, sizeof(sd));
    sd.BufferDesc.Width = cfg->width;
    sd.BufferDesc.Height = cfg->height;
    sd.BufferDesc.RefreshRate.Numerator = 0;  /* let DXGI pick the mode */
    sd.BufferDesc.RefreshRate.Denominator = 0;
    sd.BufferDesc.Format = format;
    sd.SampleDesc.Count = 1; /* no MSAA in R-2 */
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = buffer_count;
    sd.OutputWindow = cfg->hwnd;
    sd.Windowed = TRUE; /* windowed per milestone; scaling is R-3+ */
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; /* bitblt; D-1 stays open */
    sd.Flags = 0; /* no ALLOW_MODE_SWITCH: windowed, no mode changes */

    hr = IDXGIFactory_CreateSwapChain(factory, (IUnknown *)dev->device, &sd,
                                      &swapchain);
    sw->last_hresult = hr;
    /* The factory and its chain are stepping stones: release them now,
     * keeping only swap chain + backbuffer references afterwards. */
    release_staging(dxgi_device, adapter, factory);
    if (FAILED(hr)) {
        release_partial(NULL, swapchain);
        return GEX_D3D11_SWAPCHAIN_CREATE_FAILED;
    }

    hr = IDXGISwapChain_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D,
                                  (void **)&backbuffer);
    sw->last_hresult = hr;
    if (FAILED(hr)) {
        release_partial(NULL, swapchain);
        return GEX_D3D11_SWAPCHAIN_CREATE_FAILED;
    }

    hr = IDXGISwapChain_GetDesc(swapchain, &actual);
    if (FAILED(hr)) {
        sw->last_hresult = hr;
        release_partial(backbuffer, swapchain);
        return GEX_D3D11_SWAPCHAIN_CREATE_FAILED;
    }

    sw->swapchain = swapchain;
    sw->backbuffer = backbuffer;
    sw->desc = actual;
    sw->initialized = 1;
    return GEX_D3D11_SWAPCHAIN_OK;
}

gex_d3d11_swapchain_status gex_d3d11_swapchain_resize(gex_d3d11_swapchain *sw,
                                                      UINT width, UINT height)
{
    HRESULT hr;

    if (sw == NULL) {
        return GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT;
    }
    if (!sw->initialized) {
        return GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT; /* closed or never opened */
    }
    if (width == 0 || height == 0) {
        /* Minimized window: DXGI takes 0x0 as "use the client area", which
         * fails while minimized; refuse here so the caller can skip cleanly. */
        return GEX_D3D11_SWAPCHAIN_BAD_SIZE;
    }

    /* DXGI requires every backbuffer reference released before ResizeBuffers. */
    if (sw->backbuffer != NULL) {
        ID3D11Texture2D_Release(sw->backbuffer);
        sw->backbuffer = NULL;
    }

    hr = IDXGISwapChain_ResizeBuffers(sw->swapchain, 0 /* keep count */, width,
                                      height, DXGI_FORMAT_UNKNOWN /* keep */, 0);
    sw->last_hresult = hr;

    /* Re-acquire buffer 0 in both outcomes so the swap chain is never left
     * without its back buffer reference. */
    hr = IDXGISwapChain_GetBuffer(sw->swapchain, 0, &IID_ID3D11Texture2D,
                                  (void **)&sw->backbuffer);
    if (FAILED(hr)) {
        sw->last_hresult = hr;
        sw->backbuffer = NULL;
        return GEX_D3D11_SWAPCHAIN_RESIZE_FAILED;
    }
    if (FAILED(sw->last_hresult)) {
        return GEX_D3D11_SWAPCHAIN_RESIZE_FAILED;
    }

    hr = IDXGISwapChain_GetDesc(sw->swapchain, &sw->desc);
    if (FAILED(hr)) {
        sw->last_hresult = hr;
        return GEX_D3D11_SWAPCHAIN_RESIZE_FAILED;
    }
    return GEX_D3D11_SWAPCHAIN_OK;
}

ID3D11Texture2D *
gex_d3d11_swapchain_backbuffer(const gex_d3d11_swapchain *sw)
{
    if (sw == NULL || !sw->initialized) {
        return NULL;
    }
    return sw->backbuffer;
}

void gex_d3d11_swapchain_shutdown(gex_d3d11_swapchain *sw)
{
    if (sw == NULL) {
        return;
    }
    /* Dependent resources first, then the swap chain; the device belongs to
     * the caller and is released after this. */
    if (sw->backbuffer != NULL) {
        ID3D11Texture2D_Release(sw->backbuffer);
    }
    if (sw->swapchain != NULL) {
        IDXGISwapChain_Release(sw->swapchain);
    }
    memset(sw, 0, sizeof(*sw));
}

const char *gex_d3d11_swapchain_status_name(gex_d3d11_swapchain_status status)
{
    switch (status) {
    case GEX_D3D11_SWAPCHAIN_OK:
        return "ok";
    case GEX_D3D11_SWAPCHAIN_BAD_ARGUMENT:
        return "bad argument";
    case GEX_D3D11_SWAPCHAIN_BAD_STATE:
        return "bad state";
    case GEX_D3D11_SWAPCHAIN_BAD_SIZE:
        return "bad size";
    case GEX_D3D11_SWAPCHAIN_CREATE_FAILED:
        return "create failed";
    case GEX_D3D11_SWAPCHAIN_RESIZE_FAILED:
        return "resize failed";
    }
    return "unknown";
}

#endif /* _WIN32 */
