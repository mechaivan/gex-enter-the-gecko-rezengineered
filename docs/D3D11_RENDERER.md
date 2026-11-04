# D3D11_RENDERER — Renderer Direct3D 11 (R-3)

**Fecha:** 2026-11-03  
**Estado:** R-3 completado; primera salida gráfica visible  
**Plataforma:** Windows-only.

## Alcance

R-3 obtiene el backbuffer de R-2, crea una render-target view (RTV), limpia el target con un color RGBA y presenta mediante `IDXGISwapChain::Present`. La imagen actual es solo un color de limpieza: no hay geometría, shaders, texturas, cámara, escena ni lógica del juego; `pre_frame` y `update` siguen siendo stubs.

## Arquitectura y ownership

- Device e immediate context: prestados desde R-1 (`gex_d3d11_device`).
- Swapchain y backbuffer: prestados desde R-2 (`gex_d3d11_swapchain`).
- RTV: propiedad del renderer; se libera en shutdown y antes de recrearla tras resize.
- El renderer no libera device, context, swapchain ni backbuffer.

Orden de cierre: renderer/RTV → swapchain/backbuffer → device. Inicializar con `GEX_D3D11_RENDERER_INITIALIZER` o a cero; shutdown es idempotente.

## API

Cabecera `port/include/gex/render/d3d11_renderer.h`; implementación `port/src/render/d3d11_renderer.c`.

- `gex_d3d11_renderer_init`: valida argumentos, estado, device y swapchain; crea la RTV. Color explícito RGBA; NULL usa azul oscuro `(0, 0, 0.5, 1)`.
- `gex_d3d11_renderer_resize`: libera la RTV anterior y crea otra desde el backbuffer actualizado; el llamante debe completar antes `ResizeBuffers`.
- `gex_d3d11_renderer_present`: limpia la RTV, llama a Present y actualiza contadores si tiene éxito.
- `gex_d3d11_renderer_shutdown`: libera solo la RTV propia y pone la struct a cero.

Estados: `OK`, `BAD_ARGUMENT`, `BAD_STATE`, `BAD_DEVICE`, `BAD_SWAPCHAIN`, `RTV_CREATE_FAILED`. `last_hresult` conserva el último HRESULT.

## Present y sincronización

El código llama `IDXGISwapChain_Present(renderer->swapchain, 0, 0)`: `SyncInterval=0`, `Flags=0`. No activa VSync ni tearing. `vsync_retraces` de R-2 todavía no se aplica. Véase [R4_DIAGNOSTIC.md](R4_DIAGNOSTIC.md).

## Resize

Orden: liberar RTV y otras referencias al backbuffer; llamar a `gex_d3d11_swapchain_resize`; obtener el backbuffer nuevo; llamar a `gex_d3d11_renderer_resize`.

**Limitación de la prueba:** `port/tests/test_d3d11_renderer.c` registra un fallo de resize como INFO y continúa con el tamaño original. Los checks de renderer-resize solo se ejecutan si el resize de swapchain devuelve OK. No se demuestra resize exitoso en todos los entornos.

## Integración y pruebas

La aplicación activa el renderer mediante `gex_i4_app_enable_d3d11`; el callback queda conectado al frame loop. El renderer es Windows-only; el núcleo portable conserva el renderer nulo.

- `test_d3d11_renderer.exe`: 28 checks propios del renderer.
- `test_app.exe`: 117 checks de aplicación; no son exclusivos del renderer.
- R-3: 145 checks asociados, 0 fallos en Clang.
- Batería global: 133.015 checks, 0 fallos en las 12 suites Clang documentadas.
- GCC: validación completa pendiente porque `test_sag.exe` no compila en el entorno documentado.

## Limitaciones explícitas

- Sin geometría, shaders, texturas, cámara ni contenido del juego.
- Sin VSync configurable ni tearing; Present usa `(0,0)`.
- Sin recuperación de device loss; se propaga HRESULT.
- Debug Layer no instalada en el equipo documentado.
- El test no exige que resize tenga éxito en todos los entornos.
- La salida visible es el color de limpieza, no una escena del juego.

## Referencias internas

- [D3D11_DEVICE.md](D3D11_DEVICE.md) — R-1.
- [D3D11_SWAPCHAIN.md](D3D11_SWAPCHAIN.md) — R-2.
- [R4_DIAGNOSTIC.md](R4_DIAGNOSTIC.md) — investigación posterior.
- `port/tests/test_d3d11_renderer.c`, `port/src/render/d3d11_renderer.c`, `port/include/gex/render/d3d11_renderer.h`.
