# Dispositivo Direct3D 11 (R-1)

> Estado: **implementado y probado** el 2026-11-02. Es la primera pieza del
> renderer real: crear, usar y destruir un dispositivo D3D11 en modo
> **hardware** y en modo **WARP**, con el *feature level* registrado y sin
> dejar referencias COM abiertas. **No hay swapchain, ni ventana, ni dibujo,
> ni presentación**: eso es R-2 y posteriores, según el diseño de
> `docs/RENDERER_ARCHITECTURE_RESEARCH.md` §6–§7.
>
> **Actualización 2026-11-03 (mantenimiento previo a R-3):** contrato de
> inicialización formalizado — macro `GEX_D3D11_DEVICE_INITIALIZER`, tabla
> exacta de resultados de `init` y prueba ampliada a **332 checks** (§4.1 y
> §6.1). El diseño y las decisiones de R-1 no cambian.
>
> Documentación oficial consultada (Microsoft Learn, 2026-11-02):
> `D3D11CreateDevice` (nf-d3d11-d3d11createdevice) y la guía WARP
> (direct3darticles/directx-warp). Nada se dio por disponible sin comprobarlo.

## 1. Qué se ha construido

| Pieza | Fichero | Papel |
| --- | --- | --- |
| Cabecera del dispositivo | `port/include/gex/render/d3d11_device.h` | contrato Windows-only (patrón de `backend_dsound.h`) |
| Implementación | `port/src/render/d3d11_device.c` | `D3D11CreateDevice` + ciclo de vida COM |
| Prueba automatizada | `port/tests/test_d3d11_device.c` | 332 checks (WARP, hardware, errores, COM, ciclo de vida, contrato de inicialización) |
| Sonda de consola | `port/tools/d3d11probe.c` → `build/d3d11probe.exe` | informe legible por el mantenedor |
| Build | `port/build.ps1` | `test` ejecuta la prueba; `probe` (y `all`) compilan y ejecutan la sonda |
| `port/Makefile` | solo comentario | la pieza queda documentada como Windows-only, **no se compila en POSIX** |

El núcleo portable (`loop_core.c`, `frame.c`, `i4_app.c`, `null_renderer.c`)
**no toca** este módulo ni sabe que existe: `d3d11.h` solo se incluye en la
cabecera propia y en sus dos ficheros Windows. El renderer nulo de I-4 sigue
intacto; `gexloop` no ha cambiado.

## 2. Arquitectura del dispositivo

```c
typedef struct gex_d3d11_device {
    ID3D11Device *device;            /* 1 referencia nuestra tras init */
    ID3D11DeviceContext *context;    /* contexto inmediato, referencia propia */
    D3D_FEATURE_LEVEL feature_level; /* el nivel que el runtime aceptó */
    UINT creation_flags;             /* eco de GetCreationFlags */
    gex_d3d11_driver driver;         /* HARDWARE o WARP */
    HRESULT last_hresult;            /* S_OK o el error de creación */
    int initialized;
} gex_d3d11_device;
```

- **Selección explícita de modo:** `GEX_D3D11_DRIVER_HARDWARE` crea con
  `D3D_DRIVER_TYPE_HARDWARE` (adaptador por defecto, `pAdapter = NULL`) y
  `GEX_D3D11_DRIVER_WARP` con `D3D_DRIVER_TYPE_WARP` (rasterizador software
  del sistema). Un valor distinto se rechaza con `BAD_ARGUMENT` sin llamar a
  la API.
- **Feature levels solicitados:** lista explícita
  `11_1 → 11_0 → 10_1 → 10_0 → 9_3 → 9_2 → 9_1`. Microsoft documenta que un
  nivel 11_1 **solo** se obtiene si se pide explícitamente (una lista `NULL`
  nunca lo devuelve) y que pedir 11_1 en un sistema sin el runtime 11.1 hace
  fallar la llamada con `E_INVALIDARG` — caveat de la época Windows 7 que no
  afecta a la base Windows 8+ de este port. El nivel **realmente obtenido**
  se registra en el struct; no se asume el mayor de la lista.
- **Contexto inmediato:** se obtiene en la misma llamada (tercer
  out-parámetro) y se valida: puntero no nulo, llamada real a través de su
  vtable (`ClearState`/`Flush`, operaciones de estado, sin dibujo).
- **Comprobación de HRESULT en todas las operaciones:** el estado de la
  creación queda en `last_hresult`; `GetCreationFlags` confirma que el
  runtime aplicó los flags pedidos.
- **Política de fallo:** cualquier fallo deja el struct **a cero**. La rama
  defensiva libera los punteros incluso si el runtime devolviera alguno
  junto a un `HRESULT` fallido (no se ha observado, pero el código no lo
  asume). Un segundo `init` sobre un dispositivo vivo se rechaza con
  `BAD_STATE` **sin tocar** el dispositivo existente (verificado: el puntero
  no cambia y el device sigue utilizable).
- **Cierre:** `shutdown` libera primero el contexto y después el device, y
  pone el struct a cero. Es idempotente y permite `init` de nuevo
  inmediatamente.
- **Flags:** `extra_flags` se pasa tal cual (por defecto 0). Sirve, por
  ejemplo, para pedir la capa debug (§6.3).

El módulo es deliberadamente **mínimo** (criterio del hito): nada de DXGI
factory, adaptadores, colas ni swapchain. La identidad del adaptador físico
quedó accesible en R-2 vía `IDXGIDevice::GetAdapter` (usado como paso hacia la
factoría, sin registrar el nombre del adaptador).

## 3. Hardware y WARP: diferencias reales medidas

| Aspecto | Hardware | WARP |
| --- | --- | --- |
| Driver type | `D3D_DRIVER_TYPE_HARDWARE` | `D3D_DRIVER_TYPE_WARP` |
| Resultado en esta máquina | creado, **FL 11_1** | creado, **FL 11_1** |
| Proveedor de rasterización | GPU física (adaptador por defecto) | CPU (rasterizador software de Windows) |
| Disponibilidad | requiere GPU/driver D3D11; puede fallar (RDP, driver roto) | garantizada en Windows 8+; FL 9_1…11_1 según la guía WARP |
| Papel en el proyecto | camino real del renderer | **baseline determinista** para pruebas sin GPU y para CI |
| Coincidencia de FL | ambos 11_1 aquí — el port pide la misma lista y registra el obtenido | idem |

La guía WARP (Microsoft) sostiene que WARP es *fully conformant* y que en
Windows 8+ cubre 9_1–11_1; la medición aquí lo corrobora para este equipo.
WARP no es un simulador: es el dispositivo con el que las pruebas R-3/R-6
podrán verificar píxeles y errores sin depender de un driver de GPU.

## 4. Gestión de recursos COM

Reglas aplicadas (una sola vez, en todas partes):

1. Cada out-pointer de `D3D11CreateDevice` posee exactamente una referencia.
2. `shutdown` libera contexto y device y **pone el struct a cero**; un doble
   `shutdown` y un `init` tras el cierre son operaciones normales.
3. No hay `Release` condicionales olvidados: la rama de fallo también libera
   (defensa en profundidad).

**Hallazgo medido (importante para R-2+):** el *contexto inmediato* arranca
con exactamente **1** referencia (AddRef→2, Release→1, verificado), pero el
**device** no arranca con 1: el runtime mantiene referencias internas (la
documentación de D3D11 establece que el contexto inmediato mantiene vivo a su
device). Por eso la prueba no puede fijar «device refcount == 1» como
invariante; en su lugar verifica por **comportamiento**:

1. El conteo base del device es estable (dos medidas consecutivas idénticas).
2. Se retiene una referencia manual, se llama a `shutdown` (que libera las
   suyas) y se comprueba que el objeto sigue vivo **solo** con la referencia
   manual (`GetCreationFlags` responde).
3. El `Release` final devuelve **0**: destrucción completa, ninguna referencia
   pendiente ni de nuestro código ni colgada del runtime.

Este check se ejecuta para WARP, para hardware, en el ciclo de 3
init/shutdown, en el caso de dos dispositivos vivos a la vez y en la rama de
la capa debug. Es la verificación de fugas disponible sin la capa debug
(§6.3); con ella, R-2+ podrá repetir el mismo patrón por cada recurso nuevo.

### 4.1 Contrato de inicialización y errores (mantenimiento 2026-11-03)

**Precondición declarada una vez y respetada en todo el módulo:** un
`gex_d3d11_device` debe inicializarse con la macro
`GEX_D3D11_DEVICE_INITIALIZER` (o a cero) **antes** del primer `init` y
antes de `shutdown`:

```c
gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) ==
    GEX_D3D11_DEVICE_OK) {
    /* ... */
    gex_d3d11_device_shutdown(&dev);
}
```

`shutdown` deja el struct exactamente en el estado de la macro, así que un
struct cerrado se reutiliza sin trabajo extra y un segundo `shutdown` es un
no-op. La macro es un inicializador agregado con designadores, también
utilizable como literal compuesto:
`(gex_d3d11_device)GEX_D3D11_DEVICE_INITIALIZER`.

Resultado exacto de `init` en cada caso:

| Caso | Resultado | Estado del struct |
| --- | --- | --- |
| `dev == NULL` | `BAD_ARGUMENT` | nada se lee ni se escribe |
| `dev` vivo (`initialized != 0`) | `BAD_STATE` | **intacto**: dispositivo y contexto no se liberan ni se reescriben; la comprobación precede a la de cualquier argumento escalar |
| `dev` cerrado + driver inválido | `BAD_ARGUMENT` | **no se escribe**: un rechazo nunca descarta el estado del llamante |
| `dev` cerrado + argumentos válidos, creación falla | `CREATE_FAILED` | a cero (cerrado), sin recursos parcialmente abiertos; `last_hresult` = HRESULT del runtime |
| `dev` cerrado + argumentos válidos, creación correcta | `OK` | vivo |

Invariante nuevo y probado: **`CREATE_FAILED` implica `FAILED(last_hresult)`**,
incluido el caso defensivo «el runtime respondió éxito pero no entregó un
trío usable», que ahora registra `E_FAIL` en lugar de `S_OK`.

Por qué la precondición no se puede autocorregir (decisión descartada):
«¿está inicializado este struct?» no tiene respuesta en C para un struct que
el llamante nunca escribió — leer esos bytes es comportamiento indefinido
(valores indeterminados), y un campo *magic cookie* leería exactamente la
misma memoria indeterminada: añade un campo a mantener sin añadir garantía.
La alternativa de un punto de entrada `create` que nunca inspeccione el
struct eliminaría la protección de doble `init` (`BAD_STATE`) que R-1 prueba
y documenta. Lo que sí hace el módulo: leer **un solo campo**
(`initialized`), nunca un puntero, y solo para distinguir vivo de cerrado;
las pruebas nunca leen structs deliberadamente sin inicializar.

La prueba añadió 109 checks: equivalencia macro ↔ cero campo a campo (sin
`memcmp`, por el padding), ciclo completo iniciado desde la macro,
`shutdown` → estado de la macro, reutilización, `BAD_STATE` con precedencia
sobre argumentos inválidos, struct cerrado «sucio» con punteros centinela
que el rechazo no toca ni libera, y fallo de creación sobre struct de macro
con la invariante de `last_hresult`.

## 5. Compatibilidad de compiladores

| Compilador | Versión | Compilación | Ejecución |
| --- | --- | --- | --- |
| Clang (llvm-mingw, retcomm) | 22.1.8 (`x86_64-w64-windows-gnu`) | limpia, sin avisos con `-std=c11 -O2 -Wall -Wextra -Wpedantic` | 332 checks, 0 fallos |
| GCC de GNU (MSYS2 mingw64) | 16.1.0 | limpia, sin avisos | 332 checks, 0 fallos |

- `d3d11.h` es **idéntico por MD5** (`99543847B249FD31417E0753691591C9`) en
  llvm-mingw y en MSYS2 mingw64; `libd3d11.a` existe en ambos. No se
  descargó nada.
- Enlaces con `-ld3d11` al final de la línea (requisito de GNU ld ya
  conocido desde I-4; `build.ps1` ya lo hace así).
- **Requisito descubierto:** las macros C de conveniencia
  (`ID3D11Device_Release`, `ID3D11Device_GetCreationFlags`, …) de estos
  headers WIDL solo se declaran con `COBJMACROS` definido antes de incluir
  `d3d11.h`. La cabecera del módulo lo define (con guarda), de modo que los
  ficheros que la incluyan no necesitan saberlo.

## 6. Resultados reales de las pruebas

### 6.1 Batería automatizada (332 checks)

```text
warp: creating D3D_DRIVER_TYPE_WARP device...
warp: ok, feature level 11_1, flags 0x0
bogus flags 0xFFFFFFFF: status=create failed hr=0x80070057
debug layer: not available (hr=0x887A002D); failure was clean
hardware: creating D3D_DRIVER_TYPE_HARDWARE device...
hardware: ok, feature level 11_1, flags 0x0
332 checks, 0 failures, 0 skipped
```

Idéntico con Clang 22.1.8 y con GCC 16.1.0. Cobertura por bloques:

1. **Nombres** de estados y feature levels (con desconocidos incluidos).
2. **Contrato de inicialización (sin dispositivo):** macro `GEX_D3D11_DEVICE_INITIALIZER` ≡ cero campo a campo; literal compuesto; ciclo completo `init` → `shutdown` → estado macro; reutilización sin re-inicializar.
3. **Argumentos y estado:** `init(NULL)`, driver inválido, doble `init` (`BAD_STATE` sin tocar el vivo), `shutdown(NULL)` y `shutdown` de struct a cero como no-ops; `BAD_STATE` precede a argumentos inválidos.
4. **WARP real:** creación, validación completa, uso real (`CheckFormatSupport` de `R8G8B8A8_UNORM` devuelve `S_OK`; `ClearState`/`Flush` a través de la vtable), cierre verificado, segundo cierre idempotente, re-`init`, ciclo de 3 init/shutdown.
5. **Dos dispositivos vivos a la vez** (punteros distintos, ambos operativos, cierre independiente).
6. **Feature level:** obtenido ∈ lista pedida y ≥ 9_1; nombre no «unknown».
7. **Flags de creación:** eco exacto de `GetCreationFlags`.
8. **COM:** contexto con 1 referencia; device estable y destrucción completa vía referencia manual (§4), repetida en todos los caminos.
9. **Fallos:** bits reservados `0xFFFFFFFF` → `E_INVALIDARG` (0x80070057) con struct totalmente cerrado; capa debug ausente → `DXGI_ERROR_SDK_COMPONENT_MISSING` (0x887A002D) con fallo limpio (§6.3).
10. **Hardware:** creación real, mismas verificaciones; si no hubiera GPU, el bloque se **saltaría con aviso documentado** (`skipped`), nunca un «pass» fingido — en esta máquina no hizo falta: la GPU existe y el dispositivo se creó.

### 6.2 Sonda de consola

```text
D3D11 device probe (R-1, no swapchain, no drawing)
requested feature levels: 11_1 11_0 10_1 10_0 9_3 9_2 9_1
hardware ok      feature level 11_1  flags 0x0  hr 0x00000000
hardware closed  (no references left)
warp     ok      feature level 11_1  flags 0x0  hr 0x00000000
warp     closed  (no references left)
baseline: ok
```

### 6.3 Capa debug: hallazgo y significado

`D3D11_CREATE_DEVICE_DEBUG` exige las *SDK layers* (`D3D11SDKLayers.dll`),
que **no están instaladas en esta máquina**: el runtime devolvió
`DXGI_ERROR_SDK_COMPONENT_MISSING` (0x887A002D), exactamente el código que
Microsoft documenta para ese caso, y nuestro módulo lo reportó como fallo
limpio sin recursos abiertos. Consecuencias:

- La verificación de fugas de R-1 es la conductual de §4 (suficiente para el
  ciclo de vida del dispositivo).
- Cuando el mantenedor instale el SDK de Windows (o las SDK layers), la misma
  prueba activará automáticamente la rama de éxito y contará con el *debug
  layer* y `ID3D11Debug` para chequeos de objetos vivos por recurso. No se
  descarga ni se vendoriza nada para conseguirlo.

### 6.4 Regresión completa

```text
build.ps1 all (Clang 22.1.8):
69305 + 60052 + 215 + 410 + 826 + 117 + 1009 + 50 checks previos, 0 fallos
332 checks, 0 failures   test_d3d11_device (actualizado)
Total: 132 298 checks, 0 fallos
tools/m13-core: 120 checks, 0 fallos (loop_core intacto)
tools/gexmap: 22/22
```

## 7. Errores encontrados y cómo se resolvieron

1. **Macros COM ausentes en C.** La primera compilación falló con «call to
   undeclared function `ID3D11Device_Release`»: los headers WIDL de MinGW
   solo declaran las macros de conveniencia con `COBJMACROS`. Solución: la
   cabecera del módulo define `COBJMACROS` (con guarda) antes de incluir
   `d3d11.h`.
2. **Invariante de refcount demasiado estricto.** La primera ejecución falló
   16 checks: se asumía «device con 1 referencia tras init», pero el runtime
   mantiene referencias internas (el contexto inmediato mantiene su device).
   Solución: verificación conductual (referencia manual + `Release` final a
   0) en vez de una cifra supuesta; el valor del contexto (1) sí se exige.
3. **`-Wswitch` por niveles 12.x.** `gex_d3d11_feature_level_name` no
   cubría `D3D_FEATURE_LEVEL_12_0/12_1/12_2`; se añadieron los casos (el
   módulo nunca los pide, pero el nombre ya existe).

## 8. Limitaciones y próximos pasos

- **Sin swapchain, ventana ni presentación** (por alcance de R-1). R-2 **ya
  ejecutado**: `IDXGIFactory`/`IDXGISwapChain` sobre el `HWND` de I-4, con
  bitblt, resize y COM verificado, sin `Present` (`docs/D3D11_SWAPCHAIN.md`).
  El paso siguiente es R-3: clear de color y presentación.
- **Identidad del adaptador no consultada.** No hay DXGI factory aquí; qué
  GPU concreta sirve el modo hardware queda fuera del registro de R-1.
- **Capa debug no instalada** en esta máquina (§6.3): el chequeo de objetos
  vivos por recurso esperará a que el mantenedor instale las SDK layers; la
  prueba ya está lista para ambas ramas.
- **Caveat Windows 7:** pedir 11_1 en un sistema sin runtime 11.1 falla con
  `E_INVALIDARG` (documentado por Microsoft). La base del port es Windows
  8+; si algún día se soportara 7, la lista de niveles se parametrizaría.
- **El `Makefile` POSIX no se ha ejecutado** (no hay `make` en este Windows);
  solo recibió un comentario documentando la exclusión de la pieza.
- **Sin `ID3D11Device1`/QInterface:** R-1 no necesita interfaces 11.1
  extendidas; si R-2+ las necesita, irán por `QueryInterface` documentado.

Decisiones cerradas por este hito: **R-1 verificado** — la recomendación
D3D11 de I-5 deja de estar condicionada a R-1 y queda **confirmada** en esta
máquina con ambos modos y ambos compiladores.
