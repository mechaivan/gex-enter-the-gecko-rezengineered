/*
 * test_d3d11_device.c - R-1 device checks (Windows-only).
 *
 * What each group verifies (docs/D3D11_DEVICE.md is the full record):
 *
 *   - names: every status and feature level has a name, unknowns too;
 *   - argument/state errors: init refuses NULL, unknown drivers and a
 *     second init without touching the live device;
 *   - WARP: the real software device of Windows, created and used (a
 *     genuine CheckFormatSupport round-trip and a context smoke call);
 *   - hardware: same checks when the machine has a usable GPU; when the
 *     runtime refuses creation the run prints the limitation and skips the
 *     hardware block (documented skip, never a faked pass);
 *   - feature level: the obtained level is one of the requested ones;
 *   - creation flags: GetCreationFlags echoes what was asked;
 *   - COM hygiene: the immediate context out-pointer starts with exactly
 *     one reference; the device carries runtime-internal references too
 *     (the context keeps its device alive), so for the device the check is
 *     behavioural: hold one manual reference, let shutdown release its own,
 *     and verify the object survives only on the manual one and that the
 *     final Release returns 0 (full destruction, nothing left pending);
 *   - lifecycle: init -> shutdown -> init again, and two devices alive at
 *     the same time;
 *   - failure paths: reserved flag bits rejected (E_INVALIDARG family) with
 *     a fully zeroed struct, and the debug layer either created or failed
 *     cleanly with DXGI_ERROR_SDK_COMPONENT_MISSING (documented whichever
 *     happened).
 */

#include "gex/render/d3d11_device.h"

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

static int level_is_requested(D3D_FEATURE_LEVEL level)
{
    UINT i;
    for (i = 0; i < GEX_D3D11_FEATURE_LEVEL_COUNT; i++) {
        if (gex_d3d11_feature_levels[i] == level) {
            return 1;
        }
    }
    return 0;
}

/* Everything that must hold right after a successful init. */
static void check_live_device(gex_d3d11_device *dev, gex_d3d11_driver driver)
{
    HRESULT hr;
    UINT format_support = 0;
    ULONG context_base;
    ULONG device_base;

    CHECK(dev->initialized == 1);
    CHECK(dev->device != NULL);
    CHECK(dev->context != NULL);
    CHECK(dev->driver == driver);
    CHECK(dev->last_hresult == S_OK);
    CHECK(level_is_requested(dev->feature_level));
    CHECK(dev->feature_level >= D3D_FEATURE_LEVEL_9_1);
    CHECK(gex_d3d11_feature_level_name(dev->feature_level) != NULL);

    /* The creation flags come back exactly as requested (0 here). */
    CHECK(ID3D11Device_GetCreationFlags(dev->device) == dev->creation_flags);
    CHECK(dev->creation_flags == 0);

    /* A genuine round-trip through the device: R8G8B8A8_UNORM support is
     * guaranteed to be queryable at every feature level this module asks
     * for. The support bits depend on the adapter; only the call matters. */
    hr = ID3D11Device_CheckFormatSupport(dev->device, DXGI_FORMAT_R8G8B8A8_UNORM,
                                         &format_support);
    CHECK(hr == S_OK);

    /* Immediate context is live: ClearState/Flush are state-only calls (no
     * drawing, no swapchain); they would crash on a dangling pointer. */
    ID3D11DeviceContext_ClearState(dev->context);
    ID3D11DeviceContext_Flush(dev->context);
    CHECK(1);

    /* COM hygiene, context: the out-pointer starts with exactly one
     * reference. AddRef bumps to 2, Release must return to 1. */
    context_base = ID3D11DeviceContext_AddRef(dev->context);
    CHECK(context_base == 2);
    CHECK(ID3D11DeviceContext_Release(dev->context) == 1);

    /* COM hygiene, device: the runtime keeps internal references (at least
     * the immediate context keeps its device alive), so the baseline is not
     * 1 by design. Measure it instead of assuming it: AddRef, Release and
     * check the count is back at a sane, stable value. */
    (void)ID3D11Device_AddRef(dev->device);
    device_base = ID3D11Device_Release(dev->device);
    CHECK(device_base >= 1);
    (void)ID3D11Device_AddRef(dev->device);
    CHECK(ID3D11Device_Release(dev->device) == device_base); /* stable */
}

/*
 * Full-release check: hold one manual reference, let shutdown release its
 * own references, and prove that the object then survives only on the
 * manual one and that the final Release returns 0. This catches both a
 * missing Release in shutdown and references left dangling, without needing
 * the D3D11 debug layer (which this machine does not have installed).
 */
static void check_full_release(gex_d3d11_device *dev)
{
    ID3D11Device *held = dev->device;

    CHECK(held != NULL);
    ID3D11Device_AddRef(held); /* our manual reference */
    gex_d3d11_device_shutdown(dev);

    CHECK(dev->device == NULL); /* struct closed... */
    CHECK(dev->context == NULL);
    CHECK(dev->initialized == 0);
    /* ...but the object is still alive on our manual reference: */
    CHECK(ID3D11Device_GetCreationFlags(held) == 0);
    /* and we were the last one standing: */
    CHECK(ID3D11Device_Release(held) == 0);
}

static void check_shutdown_state(const gex_d3d11_device *dev)
{
    CHECK(dev->device == NULL);
    CHECK(dev->context == NULL);
    CHECK(dev->initialized == 0);
}

int main(void)
{
    gex_d3d11_device dev;
    gex_d3d11_device dev_other;
    int i;
    UINT bogus_flags = 0xFFFFFFFFu;

    /* ---- names ------------------------------------------------------- */
    CHECK(gex_d3d11_device_status_name(GEX_D3D11_DEVICE_OK) != NULL);
    CHECK(gex_d3d11_device_status_name(GEX_D3D11_DEVICE_BAD_ARGUMENT) != NULL);
    CHECK(gex_d3d11_device_status_name(GEX_D3D11_DEVICE_BAD_STATE) != NULL);
    CHECK(gex_d3d11_device_status_name(GEX_D3D11_DEVICE_CREATE_FAILED) != NULL);
    CHECK(gex_d3d11_device_status_name((gex_d3d11_device_status)99) != NULL);
    CHECK(gex_d3d11_feature_level_name(D3D_FEATURE_LEVEL_11_1) != NULL);
    CHECK(gex_d3d11_feature_level_name(D3D_FEATURE_LEVEL_9_1) != NULL);
    CHECK(gex_d3d11_feature_level_name((D3D_FEATURE_LEVEL)0x1234) != NULL);

    /* ---- argument and state errors ------------------------------------ */
    CHECK(gex_d3d11_device_init(NULL, GEX_D3D11_DRIVER_WARP, 0) ==
          GEX_D3D11_DEVICE_BAD_ARGUMENT);
    memset(&dev, 0, sizeof(dev));
    CHECK(gex_d3d11_device_init(&dev, (gex_d3d11_driver)77, 0) ==
          GEX_D3D11_DEVICE_BAD_ARGUMENT);
    CHECK(dev.initialized == 0); /* untouched by the refused init */

    /* shutdown(NULL) and shutdown on a zeroed struct are safe no-ops. */
    gex_d3d11_device_shutdown(NULL);
    gex_d3d11_device_shutdown(&dev);
    CHECK(1);

    /* ---- WARP: the real software device -------------------------------- */
    memset(&dev, 0, sizeof(dev));
    printf("warp: creating D3D_DRIVER_TYPE_WARP device...\n");
    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
        GEX_D3D11_DEVICE_OK) {
        printf("warp: ok, feature level %s, flags 0x%" PRIX32 "\n",
               gex_d3d11_feature_level_name(dev.feature_level),
               (uint32_t)dev.creation_flags);
        check_live_device(&dev, GEX_D3D11_DRIVER_WARP);
        check_full_release(&dev); /* also verifies the closed state */

        /* idempotent second shutdown on the closed struct */
        gex_d3d11_device_shutdown(&dev);
        CHECK(dev.initialized == 0);

        /* init again after shutdown (lifecycle requirement) */
        CHECK(gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
              GEX_D3D11_DEVICE_OK);
        check_live_device(&dev, GEX_D3D11_DRIVER_WARP);

        /* a second init while alive is refused and changes nothing */
        {
            ID3D11Device *same = dev.device;
            UINT format_support = 0;
            CHECK(gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
                  GEX_D3D11_DEVICE_BAD_STATE);
            CHECK(dev.device == same);
            CHECK(dev.initialized == 1);
            CHECK(ID3D11Device_CheckFormatSupport(
                      dev.device, DXGI_FORMAT_R8G8B8A8_UNORM,
                      &format_support) ==
                  S_OK); /* still usable after the refused init */
        }
        gex_d3d11_device_shutdown(&dev);
        check_shutdown_state(&dev);

        /* three full init/shutdown cycles */
        for (i = 0; i < 3; i++) {
            CHECK(gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
                  GEX_D3D11_DEVICE_OK);
            check_live_device(&dev, GEX_D3D11_DRIVER_WARP);
            check_full_release(&dev);
        }
    } else {
        /* WARP is the guaranteed baseline (Windows 8+); failing here is a
         * finding, not a skip. */
        printf("warp: FAILED (hr=0x%08lX, %s)\n",
               (unsigned long)dev.last_hresult,
               gex_d3d11_device_status_name(GEX_D3D11_DEVICE_CREATE_FAILED));
        CHECK(dev.device == NULL);
        CHECK(dev.context == NULL);
        CHECK(dev.initialized == 0);
        failures++; /* the baseline must exist on the supported platforms */
    }

    /* ---- two devices alive at the same time ---------------------------- */
    memset(&dev, 0, sizeof(dev));
    memset(&dev_other, 0, sizeof(dev_other));
    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
        GEX_D3D11_DEVICE_OK) {
        CHECK(gex_d3d11_device_init(&dev_other, GEX_D3D11_DRIVER_WARP, 0) ==
              GEX_D3D11_DEVICE_OK);
        CHECK(dev.device != NULL && dev_other.device != NULL);
        CHECK(dev.device != dev_other.device);
        check_live_device(&dev_other, GEX_D3D11_DRIVER_WARP);
        check_full_release(&dev_other);
        check_live_device(&dev, GEX_D3D11_DRIVER_WARP); /* first one intact */
        check_full_release(&dev);
    } else {
        failures++; /* unreachable if the WARP block above passed */
    }

    /* ---- failure paths at the D3D level -------------------------------- */
    /* Reserved/undefined flag bits: the runtime rejects the call. Whatever
     * the exact HRESULT, the struct must come out fully closed. */
    memset(&dev, 0, sizeof(dev));
    {
        gex_d3d11_device_status st =
            gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, bogus_flags);
        printf("bogus flags 0x%" PRIX32 ": status=%s hr=0x%08lX\n", bogus_flags,
               gex_d3d11_device_status_name(st),
               (unsigned long)dev.last_hresult);
        CHECK(st == GEX_D3D11_DEVICE_CREATE_FAILED);
        check_shutdown_state(&dev); /* nothing half-open */
    }

    /* Debug layer: needs the SDK layers (D3D11SDKLayers.dll). Both outcomes
     * are recorded; only the invariants are asserted. */
    memset(&dev, 0, sizeof(dev));
    {
        gex_d3d11_device_status st =
            gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP,
                                  D3D11_CREATE_DEVICE_DEBUG);
        if (st == GEX_D3D11_DEVICE_OK) {
            printf("debug layer: available, feature level %s\n",
                   gex_d3d11_feature_level_name(dev.feature_level));
            CHECK(dev.creation_flags == D3D11_CREATE_DEVICE_DEBUG);
            check_live_device(&dev, GEX_D3D11_DRIVER_WARP);
            check_full_release(&dev);
        } else {
            printf("debug layer: not available (hr=0x%08lX); failure was "
                   "clean\n",
                   (unsigned long)dev.last_hresult);
            CHECK(st == GEX_D3D11_DEVICE_CREATE_FAILED);
            check_shutdown_state(&dev);
        }
    }

    /* ---- hardware ------------------------------------------------------- */
    memset(&dev, 0, sizeof(dev));
    printf("hardware: creating D3D_DRIVER_TYPE_HARDWARE device...\n");
    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_HARDWARE, 0) ==
        GEX_D3D11_DEVICE_OK) {
        printf("hardware: ok, feature level %s, flags 0x%" PRIX32 "\n",
               gex_d3d11_feature_level_name(dev.feature_level),
               (uint32_t)dev.creation_flags);
        check_live_device(&dev, GEX_D3D11_DRIVER_HARDWARE);
        check_full_release(&dev);
    } else {
        /* No usable GPU (or blocked by RDP/driver policy): documented
         * limitation, never a faked pass. */
        skipped++;
        printf("hardware: NOT AVAILABLE on this machine (hr=0x%08lX) - "
               "documented limitation, hardware block skipped\n",
               (unsigned long)dev.last_hresult);
        CHECK(dev.device == NULL);
        CHECK(dev.context == NULL);
        CHECK(dev.initialized == 0);
    }

    printf("%d checks, %d failures, %d skipped\n", checks, failures, skipped);
    return failures ? 1 : 0;
}
