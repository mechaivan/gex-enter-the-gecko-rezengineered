# Swapchain Direct3D 11 sobre la ventana Win32 (R-2)

> Estado: **implementado y probado** el 2026-11-03. Es la segunda pieza del
> renderer real: una swapchain DXGI creada con el dispositivo de R-1 sobre el
> `HWND` que ya usa I-4, con backbuffer accesible, `ResizeBuffers` y ciclo de
> vida COM verificado. **No se presenta ningún frame, no se limpia el
> backbuffer, no se dibuja y no hay render target**: eso es R-3 y posteriores,
> según `docs/RENDERER_ARCHITECTURE_RESEARCH.md` §7.
>
> Documentación oficial consultada (Microsoft Learn, 2026-11-03):
> [`IDXGISwapChain::ResizeBuffers`](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)
> y [`DXGI_SWAP_EFFECT`](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect).
> Todo lo demás son mediciones de este equipo; nada se dio por cierto sin
> ejecutarlo.

## 1. Qué se ha construido

| Pieza | Fichero | Papel |
| --- | --- | --- |
| Cabecera de la swapchain | `port/include/gex/render/d3d11_swapchain.h` | contrato Windows-only; no lo incluye ningún fichero portable |
| Implementación | `port/src/render/d3d11_swapchain.c` | `IDXGIFactory::CreateSwapChain` + `GetBuffer(0)` + `ResizeBuffers` + ciclo COM |
| Prueba automatizada | `port/tests/test_d3d11_swapchain.c` | 429 checks (WARP, hardware, errores, resize, COM, ciclo de vida, ventana muerta) |
| Sonda de consola | `port/tools/swapprobe.c` → `build/swapprobe.exe` | ventana I-4 real + swapchain WARP y hardware, informe legible |
| Build | `port/build.ps1` | `test` ejecuta la prueba; `probe`/`all` compilan y ejecutan la sonda |
| `port/Makefile` | solo comentario | la pieza queda documentada como Windows-only; **no se compila en POSIX** |

El núcleo portable (`loop_core.c`, `frame.c`, `i4_app.c`,
`null_renderer.c`), la ventana (`win32_window.c`), la entrada y el
planificador de audio **no se han modificado**. `gexloop` sigue arrancando con
el renderer nulo y no sabe que existe DXGI. El renderer nulo sigue disponible
para todas las pruebas anteriores.

## 2. Arquitectura

```text
gex_d3d11_device (R-1, prestado, nunca duplicado)
        │  QueryInterface(IID_IDXGIDevice)
        ▼
   IDXGIDevice ──GetParent──► IDXGIAdapter ──GetParent──► IDXGIFactory
                                                              │
                        CreateSwapChain(device, desc, &sc) ◄──┘
                                                              │
                        GetBuffer(0, IID_ID3D11Texture2D) ◄───┘
```

Las tres interfaces de paso (`IDXGIDevice`, `IDXGIAdapter`, `IDXGIFactory`) se
liberan **antes** de que `init` retorne. El módulo conserva exactamente dos
referencias propias: la swapchain y el backbuffer. El dispositivo pertenece al
llamante y no se duplica en ningún momento.

```c
typedef struct gex_d3d11_swapchain {
    IDXGISwapChain *swapchain;   /* 1 referencia nuestra tras init */
    ID3D11Texture2D *backbuffer; /* buffer 0; referencia propia, se renueva
                                  * en cada resize */
    DXGI_SWAP_CHAIN_DESC desc;   /* lo que DXGI creó de verdad (GetDesc) */
    HRESULT last_hresult;
    int initialized;
} gex_d3d11_swapchain;
```

`desc` se rellena con `IDXGISwapChain::GetDesc`, no con la petición: lo que se
prueba y se documenta es lo que DXGI aceptó. El acceso al backbuffer para R-3
es `gex_d3d11_swapchain_backbuffer()` (puntero prestado, válido hasta el
siguiente resize o `shutdown`).

### 2.1 Por qué no `D3D11CreateDeviceAndSwapChain`

R-1 creó el dispositivo con `D3D11CreateDevice` y mantiene un contrato propio
(hardware/WARP, *feature levels*, cierre idempotente). Volver a crear el
dispositivo con la variante «y swapchain» duplicaría el dispositivo y rompería
R-1. La ruta `device → DXGI device → adapter → factory` es la que Microsoft
documenta para crear una swapchain a partir de un dispositivo existente, y es
la única que respeta el contrato de R-1.

## 3. Decisiones técnicas

| Decisión | Valor | Motivo |
| --- | --- | --- |
| Modelo de presentación | **bitblt**, `DXGI_SWAP_EFFECT_DISCARD` | DXGI 1.0, disponible en toda la base Windows del port; no exige `IDXGIFactory2` ni reglas extra de flip. Es el modelo que el proyecto puede medir ya |
| Número de buffers | **2** (`BufferCount = 2`) | reproduce los 2 buffers de color que el original pide en `grSstWinOpen` (0x0046324D) y es el mínimo del modelo bitblt |
| Formato | `DXGI_FORMAT_R8G8B8A8_UNORM` (28) | formato de render target estándar en D3D11, sin dependencia de soporte de swapchain flip; configurable por el llamante |
| Modo | ventana (`Windowed = TRUE`) | el port vive en una ventana Win32; sin cambio de modo ni `ALLOW_MODE_SWITCH` |
| MSAA | `Count = 1`, `Quality = 0` | fuera de alcance; el original no lo pedía en la evidencia de I-5 |
| Refresh rate | `0/0` | en ventana DXGI elige el modo; `GetDesc` devuelve el efectivo |
| vsync | `vsync_retraces` (por defecto 3) **almacenado** | equivalente futuro de `grBufferSwap(3)`; R-2 no presenta, así que no se usa todavía |
| Tamaño | explícito y ≥ 1 | el llamante decide (512×384 del original, tamaño de cliente, etc.); 0 se rechaza con `BAD_ARGUMENT` |
| Ventana | el `HWND` existente | validado con `IsWindow`; no se crea ninguna ventana |
| Resize | `ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0)` | conserva número de buffers y formato; solo cambia el tamaño |
| Tamaño 0 | rechazado con `BAD_SIZE`, sin llamar a DXGI | ventana minimizada: el área de cliente es 0×0 y DXGI no puede crear buffers de tamaño cero |
| Error | cualquier fallo deja el struct a cero | sin estados a medias; `last_hresult` conserva el HRESULT |

**D-1 (flip-model vs bitblt) no se cierra aquí.** La elección de R-2 es bitblt
porque es la base mínima y medible, y porque el modelo flip cambia la latencia
y la cadencia de `Present`, que es precisamente lo que R-4 va a medir. La
decisión definitiva se tomará con datos de R-4, no por preferencia. Nada fuera
de este módulo codifica el modelo elegido: la configuración expone formato y
número de buffers, y el cambio de modelo afectaría solo a este fichero.

## 4. Ciclo de vida y COM

### 4.1 Referencias medidas

| Objeto | Referencias tras `init` | Medición |
| --- | --- | --- |
| Dispositivo (R-1) | 2 (base del runtime) | `AddRef`→3, `Release`→2; coincide con el hallazgo de R-1 de que el runtime mantiene referencias internas |
| Swapchain | 2 | 1 nuestra + 1 retenida por su propio backbuffer |
| Backbuffer | 1 | la nuestra |

Al crear la swapchain, el dispositivo pasa de **2 a 3** referencias: la
swapchain mantiene vivo a su dispositivo. Al destruirla por completo vuelve a
**2**. Esto no es una suposición: se mide con `AddRef`/`Release` y se exige en
la prueba (`test_d3d11_swapchain.c`, bloque WARP).

La consecuencia práctica es la que el hito exige: **el dispositivo se libera
después de la swapchain**. Si se liberara antes, la swapchain seguiría
manteniéndolo vivo y la destrucción quedaría en manos del runtime. El orden
probado es: recursos dependientes (backbuffer) → swapchain → dispositivo.

### 4.2 Reglas aplicadas

1. Cada out-pointer posee exactamente una referencia.
2. Las interfaces de paso se liberan al terminar `init`, también en las ramas
   de error (no hay `Release` olvidados).
3. `shutdown` libera primero el backbuffer y después la swapchain, y pone el
   struct a cero; es idempotente y permite `init` de nuevo inmediatamente.
4. Un `init` sobre una swapchain viva se rechaza con `BAD_STATE` **sin tocar**
   la swapchain existente (se comprueba que el puntero no cambia y que sigue
   operativa).
5. El backbuffer se libera **antes** de `ResizeBuffers` y se vuelve a obtener
   después, tanto si el resize tuvo éxito como si no, de modo que la swapchain
   nunca queda sin su referencia al buffer 0.
6. La referencia final de cada objeto devuelve **0** cuando se libera en el
   orden correcto (backbuffer antes que swapchain): no queda ninguna
   referencia pendiente.

## 5. Resize

Microsoft exige, para que `ResizeBuffers` tenga éxito, liberar **todas** las
referencias directas e indirectas a los backbuffers: referencias propias,
vistas, recursos ligados al contexto, listas de comandos, etc., y recomienda
`ClearState` si hay dudas. Este módulo solo puede liberar la suya; por eso el
contrato de la cabecera deja explícito que **R-3 debe liberar su render target
view antes de llamar a `resize`**.

Comportamiento implementado y medido:

| Caso | Resultado |
| --- | --- |
| 320×240 → 512×384 | OK; `desc` y textura a 512×384 |
| mismo tamaño repetido | OK |
| 512×384 → 384×512 → 240×240 → 640×480 | OK, cada uno con su `GetBuffer` nuevo |
| ventana real redimensionada (`SetWindowPos` 700×520) → `GetClientRect` 684×481 → resize | OK, `desc` y textura a 684×481 |
| 0×0, 0×h, w×0 | `BAD_SIZE`, **sin** llamada a DXGI; la swapchain sigue usable |
| resize tras destruir el `HWND` | **OK** (ver §7.3): DXGI no valida la ventana en `ResizeBuffers` |
| resize con swapchain cerrada o `NULL` | `BAD_ARGUMENT` |

En cada resize exitoso se actualiza `desc` con `GetDesc`; la prueba verifica
tamaño, formato, número de buffers, modo ventana, modelo bitblt, uso de render
target y `OutputWindow`.

## 6. Integración con la ventana de I-4

- La prueba crea ventanas reales con `gex_win32_window_create` (la misma clase
  `GexNativeLoopWindow` que usa `gexloop`) y pasa su `HWND` a la swapchain. No
  hay ventana de prueba falsa ni HWND sintético: un `HWND` inventado
  (`(HWND)0x1`) se rechaza con `BAD_ARGUMENT`.
- La sonda `swapprobe.exe` crea una ventana real de 640×480, bombea mensajes y
  crea swapchains WARP y hardware de 512×384 sobre ella, sin presentar.
- `gexloop` **no se ha tocado**: sigue con 960×540, renderer nulo y su
  cadencia intacta (5 frames en la prueba de humo, 0 errores).
- Una swapchain puede crearse después de que la ventana exista y antes de que
  se muestre; no se necesita crear una segunda ventana ni una ventana oculta.

## 7. Resultados reales

### 7.1 Batería automatizada (429 checks)

Salida exacta con Clang 22.1.8 (idéntica con GCC 16.1.0):

```text
warp: device ok, feature level 11_1
device refs before swapchain: 2
device refs with swapchain: 3
warp: swapchain ok 320x240, 2 buffers, format 28
window resize: client 684x481
refs before shutdown: swapchain 3, backbuffer 2
device refs after full release: 2
refs before shutdown: swapchain 3, backbuffer 2
refs before shutdown: swapchain 3, backbuffer 2
refs before shutdown: swapchain 3, backbuffer 2
refs before shutdown: swapchain 3, backbuffer 2
buffer_count=1000: status=create failed hr=0x887A0001
resize after window destroyed: status=ok hr=0x00000000
refs before shutdown: swapchain 3, backbuffer 2
hardware: creating D3D_DRIVER_TYPE_HARDWARE device...
hardware: swapchain ok 320x240
refs before shutdown: swapchain 3, backbuffer 2
429 checks, 0 failures, 0 skipped
```

Cobertura por bloques:

1. **Nombres de estado**, incluido un valor desconocido.
2. **Argumentos y estado:** `NULL` en cada parámetro, dispositivo R-1 cerrado,
   `HWND` nulo, `HWND` falso, tamaño 0, resize sobre swapchain cerrada,
   `backbuffer(NULL)`, `shutdown(NULL)` y doble `shutdown`. Un struct cerrado
   pero con basura se deja **a cero** tras un `init` rechazado (contrato de la
   cabecera).
3. **WARP real:** creación sobre la ventana de I-4, `desc` exacta (tamaño,
   formato, 2 buffers, ventana, bitblt, uso de render target, `OutputWindow`),
   backbuffer con textura 1 mip / 1 array / `DEFAULT` / sin MSAA, y su
   referencia (AddRef→2, Release→1).
4. **Resize:** válido, mismo tamaño, repetido, **ventana real redimensionada
   con `SetWindowPos` y resize al área de cliente resultante** (684×481),
   tamaños 0 rechazados sin tocar DXGI y swapchain todavía usable después.
5. **Doble `init`:** `BAD_STATE` con la swapchain viva intacta.
6. **COM:** referencias medidas (§4.1), liberación completa a 0, idempotencia y
   dos ciclos completos de `init → resize → cierre`.
7. **Dos swapchains sobre dos ventanas con un solo dispositivo:** punteros
   distintos, ambas operativas, resize independiente, cierre independiente.
8. **Error de creación:** `BufferCount = 1000` → DXGI lo rechaza con
   `DXGI_ERROR_INVALID_CALL` (`0x887A0001`) y el struct queda **totalmente
   cerrado** (sin swapchain, sin backbuffer, `initialized = 0`).
9. **Ventana no válida:** creación con `HWND` destruido → `BAD_ARGUMENT`;
   creación válida y destrucción posterior de la ventana → `ResizeBuffers`
   devuelve `OK`, struct coherente y cierre limpio con todas las referencias
   a 0.
10. **Hardware:** las mismas comprobaciones cuando hay GPU (aquí la hay, FL
    11_1); si no la hubiera, el bloque se marca `skipped` con aviso, nunca un
    «pass» fingido.

### 7.2 Sonda de consola (ventana real, WARP + hardware)

```text
warp: device ok, feature level 11_1
warp: swapchain 512x384, 2 buffers, format 28, windowed 1, swap effect 0
warp: backbuffer texture 512x384, mips 1, array 1, usage 0
warp: resize 512x384 ok
warp: resize 640x480 ok
warp: 0x0 refused without touching DXGI (minimized case)
warp: closed (backbuffer, swapchain, device)
hardware: device ok, feature level 11_1
hardware: swapchain 512x384, 2 buffers, format 28, windowed 1, swap effect 0
hardware: backbuffer texture 512x384, mips 1, array 1, usage 0
hardware: resize 512x384 ok
hardware: resize 640x480 ok
hardware: 0x0 refused without touching DXGI (minimized case)
hardware: closed (backbuffer, swapchain, device)
```

### 7.3 Hallazgos no evidentes

1. **El backbuffer referencia a la swapchain.** Tras `init`, la swapchain tiene
   2 referencias, no 1: la segunda la retiene su propio buffer. Liberar la
   swapchain antes que el backbuffer **no** la destruye (el `Release` devuelve
   1). El orden correcto (backbuffer primero) devuelve 0 en ambos objetos. Esto
   se midió, no se dedujo.
2. **`GetBuffer` no toma una referencia al dispositivo por buffer**: el
   dispositivo sube exactamente +1 al crear la swapchain (2→3) y vuelve a 2 al
   destruirla.
3. **`ResizeBuffers` no valida el `HWND`.** Con la ventana ya destruida, el
   resize devolvió `S_OK` y el struct siguió coherente; el fallo real de una
   ventana muerta aparecerá en `Present` (R-3), no aquí. La prueba acepta
   ambos desenlaces y registra el medido.
4. **`BufferCount = 1000`** no se rechaza en nuestra validación (es un valor
   legal de la API) sino en DXGI, con `DXGI_ERROR_INVALID_CALL`; el módulo
   propaga el fallo con el struct cerrado, que es el comportamiento correcto.

## 8. Compatibilidad de compiladores y enlazado

| Compilador | Versión | Compilación | Ejecución |
| --- | --- | --- | --- |
| Clang (llvm-mingw, retcomm) | 22.1.8 (`x86_64-w64-windows-gnu`) | limpia, sin avisos con `-std=c11 -O2 -Wall -Wextra -Wpedantic` | 429 checks, 0 fallos, 0 saltos |
| GCC de GNU (MSYS2 mingw64) | 16.1.0 | limpia, sin avisos | 429 checks, 0 fallos, 0 saltos |

Enlace: `-ld3d11 -luuid`, sin `-ldxgi`.

- `dxgi.h` viene con el mismo toolchain que `d3d11.h` (headers WIDL), así que
  no hay dependencia nueva que descargar.
- Los **IID** de DXGI (`IID_IDXGIDevice`, `IID_IDXGIAdapter`,
  `IID_IDXGIFactory`, `IID_ID3D11Texture2D`) están definidos en
  `libuuid.a` de ambos toolchains; sin `-luuid` el enlace falla. `guiddef.h`
  los declara `extern` en C para GCC/Clang.
- **No hace falta `-ldxgi`**: no se llama a `CreateDXGIFactory`; todas las
  operaciones son llamadas a vtable (`IDXGIFactory_CreateSwapChain`, …) y el
  único símbolo externo es `D3D11CreateDevice`. La librería de importación
  `DXGI.lib` que cita Microsoft es requisito de MSVC para llamar a
  `CreateDXGIFactory`, no de este camino. En tiempo de ejecución, `dxgi.dll`
  lo carga `d3d11.dll`.
- `IID_IDXGIFactory` resuelve igualmente de `libuuid.a`; no se ha usado
  `__uuidof` ni C++.

## 9. Errores encontrados y cómo se resolvieron

Todos los fallos de esta fase fueron **suposiciones de la prueba**, no del
módulo; se documentan porque cambiaron el contrato verificado:

1. **«La swapchain tiene 1 referencia».** La primera ejecución falló 6 checks
   con `Release → 1`. Causa real: el backbuffer retiene una referencia a la
   swapchain. Solución: medir y documentar los conteos reales (§4.1) y liberar
   el backbuffer antes que la swapchain.
2. **Segundo `init` del dispositivo en el bloque de dos ventanas.** El bloque
   re-inicializaba un dispositivo que seguía vivo, y R-1 (correctamente) lo
   rechaza con `BAD_STATE`. Solución: reutilizar el dispositivo vivo para las
   dos swapchains y cerrarlo después; el `init` tras `shutdown` ya lo cubre
   R-1.
3. **Aserción invertida al cerrar con ventana muerta.** Se comprobaba
   `Release(held) >= 1` después de liberar la última referencia; lo correcto es
   `== 0` (no quedan referencias). Se unificó con el chequeo completo de
   liberación.
4. **Enlace incompleto de la prueba.** Faltaban `win32_window.c` y
   `key_state.c` (la prueba crea ventanas reales) y los IID no resolvían sin
   `-luuid`. `build.ps1` ahora compila la prueba con esas fuentes y esas
   librerías.
5. **`ID3D11Texture2D_GetDesc` devuelve `void`** en estos headers WIDL, no
   `HRESULT`; la primera compilación falló al compararlo con `S_OK`. Se usa
   como comprobación de vida (si el objeto estuviera destruido, la llamada
   sería inválida) y se valida el contenido de la descripción.

## 10. Limitaciones y cuestiones pendientes

- **No se presenta todavía.** Sin `Present` no hay imagen, ni medición de
  cadencia, ni verificación visual. Es el alcance de R-3/R-4, no una omisión.
- **No hay render target view.** R-3 debe crearla desde
  `gex_d3d11_swapchain_backbuffer()` y liberarla antes de cualquier resize.
- **D-1 sigue abierta.** bitblt es la base actual; la comparación con
  flip-model se hará en R-4 con medidas de presentación. Cambiar de modelo no
  afecta al resto del port.
- **Escalado del backbuffer sin medir.** Con bitblt y un backbuffer de 512×384
  en una ventana mayor, DXGI escala al área de cliente. La apariencia (filtrado
  y bandas) se decidirá en R-3, cuando exista una imagen que mirar.
- **Sin integración con `WM_SIZE` en el bucle.** El módulo ofrece `resize`,
  pero `gexloop` todavía no lo llama: conectar el mensaje al resize es parte de
  la integración del renderer (R-3/R-4), no de R-2. El caso 0×0 ya está
  preparado para que el bucle lo omita sin error.
- **Sin manejo de device-removed más allá de propagar el HRESULT.** La política
  de recuperación es R-6; aquí solo se garantiza un fallo limpio y un struct
  coherente.
- **Capa debug de D3D11 no instalada** en este equipo (R-1): el chequeo de
  objetos vivos por recurso sigue siendo conductual. Cuando el mantenedor
  instale las SDK layers, la prueba de R-2 podrá añadir `ID3D11Debug` sin
  cambios de contrato.
- **`Makefile` POSIX no ejecutado** (no hay `make` en este Windows); solo
  recibió el comentario que excluye la pieza. Ninguna cabecera portable incluye
  `d3d11.h` ni `dxgi.h`.
- **Solo `IDXGISwapChain` (DXGI 1.0).** No se usan `IDXGISwapChain1`,
  `IDXGIFactory2` ni flip; si R-4 los necesita, se obtendrán por
  `QueryInterface` documentado.
- **El tamaño inicial no se deriva de la ventana.** El llamante lo decide; la
  prueba usa 320×240 y la sonda 512×384, mientras la ventana mide 480×360 y
  640×480 respectivamente.

## 11. Discrepancias y decisiones que afectan a R-3

1. **Crear la RTV** desde el backbuffer prestado y liberarla antes de
   `gex_d3d11_swapchain_resize` (requisito de DXGI, §5). El módulo no puede
   hacerlo por el llamante.
2. **`Present(SyncInterval, 0)`** usará `vsync_retraces` (3 por defecto) para
   reproducir el `grBufferSwap(3)` del original. R-2 solo lo almacena; el valor
   efectivo y su impacto en la cadencia se medirán en R-4/R-5.
3. **El clear y el color son de R-3.** Ninguna decisión de R-2 fija el color de
   fondo ni el comportamiento visual.
4. **Un resize invalida el backbuffer anterior.** Cualquier RTV o vista
   creada antes del resize queda obsoleta; R-3 debe recrearla.
5. **La ventana muerta no se detecta en el resize** (§7.3): R-3 debe tratar
   `Present` como el punto donde un `HWND` inválido se manifiesta, y R-6
   definirá la recuperación.
6. **El modelo bitblt escala el backbuffer al cliente.** Si R-3 quiere píxeles
   1:1 (512×384), deberá decidir entre ajustar el tamaño de la ventana al
   backbuffer o asumir el escalado; no se ha decidido en R-2.

## 12. Referencias

**Externas (semántica, consultadas 2026-11-03):**
- `IDXGISwapChain::ResizeBuffers` — requisito de liberar todas las referencias
  directas e indirectas a los backbuffers, uso de `ClearState`, `BufferCount =
  0` para conservar el número de buffers, `Width/Height = 0` para usar el área
  de cliente.
- `DXGI_SWAP_EFFECT` — bitblt (`DISCARD`/`SEQUENTIAL`) frente a flip
  (`FLIP_SEQUENTIAL`/`FLIP_DISCARD`), disponibilidad por versión de Windows y
  diferencias de composición.

**Repositorio:**
`docs/RENDERER_ARCHITECTURE_RESEARCH.md` §2.3–2.4, §5.6, §6, §7;
`docs/D3D11_DEVICE.md`; `docs/WINDOW_INPUT_PORT.md`;
`docs/FRAME_STEP_TRACE.md` §3.1 (`grBufferSwap(3)`);
`port/include/gex/render/d3d11_swapchain.h`;
`port/tests/test_d3d11_swapchain.c`; `port/tools/swapprobe.c`.
