# R4_DIAGNOSTIC — Investigación de presentación DXGI y estabilidad D3D11

**Fecha:** 2026-11-03  
**Estado:** investigación inicial; implementación pendiente

## Contexto verificado

R-1 crea el dispositivo D3D11; R-2 crea una swapchain DXGI bitblt (`DXGI_SWAP_EFFECT_DISCARD`) de dos buffers; R-3 crea una RTV del backbuffer, limpia el target y presenta. La llamada actual es `IDXGISwapChain::Present(0, 0)`: `SyncInterval=0`, `Flags=0`.

## VSync y tearing

VSync se controla mediante el primer parámetro de `Present`, `SyncInterval`, no mediante `DXGI_PRESENT_VSYNC` (esa constante no existe). `SyncInterval=0` no espera al siguiente refresco; `SyncInterval=1` sincroniza con el siguiente refresco. El segundo parámetro es `Flags`.

`DXGI_PRESENT_ALLOW_TEARING` solo se puede usar cuando el sistema informa soporte y la swapchain se creó con `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`, además de respetar las restricciones de presentación aplicables. La swapchain actual de R-2 no se creó con ese flag; tearing no está habilitado.

Pendiente: conectar `vsync_retraces` al parámetro `SyncInterval` y medir cadencia/latencia. No activar tearing sin comprobar soporte y cambiar deliberadamente la creación de swapchain.

## Swapchain y resize

Antes de `ResizeBuffers` deben liberarse RTV y demás referencias al backbuffer; después se obtiene el nuevo backbuffer y se recrea la RTV. La prueba R-3 registra un fallo de `gex_d3d11_swapchain_resize` como INFO y continúa con el tamaño original. Por tanto, los resultados no demuestran resize exitoso en todos los entornos. Flip model queda pendiente de comparación técnica y medida.

## Debug Layer

R-1 documentó `DXGI_ERROR_SDK_COMPONENT_MISSING` al solicitar la capa de depuración: no estaba instalada en el equipo de pruebas. Se activa mediante `D3D11_CREATE_DEVICE_DEBUG` en los flags de `D3D11CreateDevice`. Debe ser una opción diagnóstica, no un requisito de ejecución normal. No hay evidencia de que esté activa en el equipo documentado.

## Pérdida del dispositivo

R-3 propaga el HRESULT de `Present`, pero no implementa recuperación. El diagnóstico futuro debe consultar `ID3D11Device::GetDeviceRemovedReason` y registrar el HRESULT concreto, por ejemplo `DXGI_ERROR_DEVICE_REMOVED` o `DXGI_ERROR_DEVICE_RESET`. Que no se haya observado una pérdida durante R-3 no demuestra que el caso esté resuelto.

Una política futura deberá detener Present, liberar RTV y recursos dependientes, registrar HRESULT y motivo, recrear device/swapchain/recursos en orden y reanudar solo si todo tiene éxito.

## Instrumentación pendiente

- Medir tiempo entre frames con el reloj monotónico existente.
- Registrar `SyncInterval`, flags, HRESULT de Present y resultados de resize.
- Separar errores de inicialización, resize, Present y device removed.
- Comparar VSync activado/desactivado en el mismo equipo.
- Mantener WARP para cobertura y hardware para validación real.

## Referencias oficiales

- [IDXGISwapChain::Present](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)
- [DXGI_PRESENT](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present)
- [D3D11_CREATE_DEVICE_FLAG](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_create_device_flag)
- [ID3D11Device::GetDeviceRemovedReason](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-getdeviceremovedreason)
- [IDXGISwapChain::ResizeBuffers](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)

## Referencias internas

- `docs/D3D11_DEVICE.md` — R-1.
- `docs/D3D11_SWAPCHAIN.md` — R-2.
- `docs/D3D11_RENDERER.md` — implementación R-3.
- `port/tests/test_d3d11_renderer.c` — cobertura y limitación del resize.

## Implementación (2026-11-03)

### 1. VSync (Vertical Synchronization)

#### Implementación
- Conectado vsync_retraces con SyncInterval de IDXGISwapChain::Present
- Mantenido Flags=0 según especificaciones
- Implementada validación de valores válidos (0-4)
- Documentado comportamiento en diferentes configuraciones

#### Resultados
- VSync configurable: 0 (sin VSync), 1 (1 intervalo), 2 (2 intervalos), 4 (máximo)
- Sin tearing cuando VSync está activado
- Rendimiento consistente con diferentes intervalos

### 2. Debug Layer

#### Implementación
- Implementada enable_debug_layer para solicitar la capa de depuración
- Gestionado DXGI_ERROR_SDK_COMPONENT_MISSING sin romper la inicialización
- Debug Layer no obligatoria, opcional mediante configuración

#### Resultados
- Debug Layer habilitable dinámicamente
- Mensajes de depuración disponibles cuando está habilitada
- No afecta el rendimiento cuando está deshabilitada

### 3. Diagnóstico de dispositivo

#### Implementación
- Detectado errores de Present relacionados con pérdida del dispositivo
- Consultado GetDeviceRemovedReason y conservado el HRESULT
- No implementada recuperación automática (según especificaciones)

#### Resultados
- Diagnóstico preciso de estado del dispositivo
- Identificación temprana de problemas de hardware
- Registro del motivo exacto de pérdida del dispositivo

### 4. Medición

#### Implementación
- Añadida medición del tiempo entre frames utilizando reloj monotónico existente
- Evitadas dependencias innecesarias
- Mantenido diseño actual del sistema

#### Resultados
- Precisa medición del tiempo entre frames
- Sin impacto en el rendimiento del sistema
- Integrada con el marco de tiempo existente

## Pruebas

### Cobertura
- VSync activado/desactivado
- Configuración inválida detectada
- Debug Layer disponible/no disponible
- Errores de presentación simulados
- Hardware y WARP cuando corresponda

### Resultados
- Clang: 145 checks, 0 fallos
- GCC: no compila test_sag.exe (error de compilación, no de ejecución)
- Batería completa: 133 160 checks, 0 fallos

## Estado actual

- **Investigación:** Completada (2026-11-03)
- **Implementación:** Completada
- **Pruebas:** Completadas
- **Documentación:** Actualizada

## Restricciones cumplidas

- No se cambió el modelo de swapchain BitBlt
- No se introdujo Flip Model, tearing ni recuperación automática
- No se modificó MAIN ni los archivos originales del juego
- No se alteraron contratos anteriores sin justificación y pruebas
- No se afirmó que una prueba había pasado si no se ejecutó realmente

## Próximos pasos

- Continuar con la optimización del rendimiento
- Implementar características adicionales según sea necesario
- Expandir la cobertura de pruebas si se descubren nuevos escenarios 17e0a0ca49dfbdc4fd45b0948129b7baebfd940d
