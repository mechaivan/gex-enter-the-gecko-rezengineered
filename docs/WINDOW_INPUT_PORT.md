# Ventana Win32, entrada de teclado y reloj nativo (I-4)

> Estado: **implementado y probado** el 2026-11-02. Es el primer proceso
> nativo del port con ventana Win32 real, bucle continuo, reloj monotónico real
> y frame loop de I-3 integrado. El renderer es **nulo**: `present` se ejecuta
> y contabiliza, pero no dibuja nada. `pre_frame` y `update` siguen siendo
> stubs explícitos: **no hay lógica de juego**.
>
> Evidencia de partida: `docs/FRAME_STEP_TRACE.md`,
> `docs/SUBSYSTEM_SEGMENTS.md`, `docs/FRAME_LOOP_PORT.md` y
> `docs/FUNCTION_MAP.md`. Los ports de PC y REA se revisaron como referencia;
> REA **no** está operativo para GEX y no se usó para ninguna afirmación.

## 1. Qué se ha construido

| Pieza | Fichero | Plataforma | Estado |
| --- | --- | --- | --- |
| Conversión ticks → µs | `port/include/gex/platform/clock.h`, `port/src/platform/clock.c` | portable | probada, 117 checks (con `test_app`) |
| Estado de teclado | `port/include/gex/input/key_state.h`, `port/src/input/key_state.c` | portable | probado, 826 checks |
| Renderer nulo | `port/include/gex/render/null_renderer.h`, `port/src/render/null_renderer.c` | portable | probado (present contabilizado, sin dibujo) |
| Núcleo de aplicación | `port/include/gex/app/i4_app.h`, `port/src/app/i4_app.c` | portable | probado, 117 checks |
| Reloj real | `port/include/gex/platform/win32_time.h`, `port/src/platform/win32_time.c` | Windows | probado, 1009 checks |
| Ventana y mensajes | `port/include/gex/platform/win32_window.h`, `port/src/platform/win32_window.c` | Windows | ejecutado en ventana real |
| Ejecutable | `port/src/app/i4_main_win32.c` → `port/build/gexloop.exe` | Windows | ejecutado en ventana real |
| Pruebas | `port/tests/test_input.c`, `test_app.c`, `test_win32_time.c` | mixto | 826 + 117 + 1009 checks |

El núcleo temporal (`loop_core.c`), el frame loop (`frame.c`) y el planificador
de audio (`sched.c`) **no se han duplicado ni reimplementado**. `gexloop` usa
las mismas fuentes que I-3 y no adjunta audio (`gex_frame.audio == NULL`).

## 2. Evidencia del original y decisiones

### 2.1 `0x0043E719` no es la entrada (CONFIRMADO por la traza)

La propuesta inicial lo trataba como «candidato de entrada». La evidencia
dinámica lo desmiente:

- `docs/FRAME_STEP_TRACE.md` §3.1: el frame step `0x0043EA36` llama a
  `0x0043F80D` (entrada), luego a `0x0043E1BA` (update), y desde el update se
  llama a `0x0043E719` (sitio `0x0043E2F5`, 599 de 599 frames).
- §7 y §10: `0x0043E719` es una **máquina de estados de acción** sobre
  `WORD [obj+0x4E3E]` (valores 0/4/6/7, con ramas a `0x00423A6A`/`0x00423AFE`
  y `0x0043E63D`), con bits de `+0xA0`, `+0x6FC0` y `+0x28+4`. **No es entrada
  ni audio.**
- `docs/FUNCTION_MAP.md` lo lista como función de 339 bytes con un llamante;
  `docs/SUBSYSTEM_SEGMENTS.md` §S5 explica que la lectura guiada ya se hizo y
  que quedó descartado como candidato de entrada.

Por tanto, **no se le atribuye** ninguna semántica de teclado.

### 2.2 La ruta de entrada del original es S5 (HIPÓTESIS)

`docs/SUBSYSTEM_SEGMENTS.md` §S5 («HYPOTHESIS: entrada (teclado y joystick)»)
documenta:

- `0x0043F80D` (178 B) es el **paso de entrada del frame**, llamado por el
  frame step confirmado antes del update; no tiene import directo.
- De él cuelgan `0x0043F51D` y `0x0043EE6B`, que usan `joyGetPosEx` y
  `joyGetDevCapsA` (sondeo de joystick, confianza ALTA), y `0x0043FD44` /
  `0x0043FB8D`, que usan `VkKeyScanA` (conversión de tecla, ALTA).
- `0x0043EA70` (1 019 B) cuelga del paso de entrada pero no tiene API ni ancla:
  se queda en confianza BAJA y **no se le inventa semántica**.
- Tres usos de `VkKeyScanA` (`0x004403D6`, `0x00440CD1`, `0x00440D79`) no
  tienen camino directo desde `WinMain`; podrían ser callbacks o una ruta
  alternativa, y el mapa no lo decide.

La **fuente exacta del estado de teclado del original no está confirmada**:
`VkKeyScanA` traduce un carácter a código de tecla, no lee el estado físico, y
la lectura podría venir de mensajes, de `GetAsyncKeyState`/`GetKeyboardState`
o de otra ruta no identificada. No se afirma ninguna de ellas.

### 2.3 Decisión del port: mensajes Win32

I-4 usa **`WM_KEYDOWN`/`WM_KEYUP`** como decisión explícita del port, no como
reproducción demostrada del original:

- es la vía nativa mínima y estable del toolchain actual (MinGW-w64/Clang 22);
- no añade dependencias (solo `user32`, `gdi32` y `winmm`, todas del sistema);
- separa la fuente de entrada (Win32) del estado (tablas C11) y del consumo
  (frame loop), de modo que una futura ruta basada en `GetAsyncKeyState` o en
  el sondeo de S5 pueda sustituir solo el adaptador.

Esta diferencia queda registrada como **W1** en §11.

### 2.4 REA y ports de referencia

- REA (`morluto/rea`) es capa de orquestación y necesita un proveedor externo
  (Ghidra/IDA/Hopper). En este entorno `rea doctor` no está sano y **ningún
  binario de GEX ha sido analizado con REA** (`docs/REA_WINDOWS_ENABLEMENT.md`,
  `RESEARCH.md` S-18).
- Los ports de PC y `Hydroloxed/gexdecomp` se consultaron como referencia
  secundaria de la familia Gex (RESEARCH S-27); no se copió código ni se
  trasladaron direcciones entre ejecutables.
- No se atribuye a estas fuentes ninguna prueba sobre la ruta de entrada de
  `GEX3D.EXE`.

## 3. Arquitectura

```text
i4_main_win32.c (consola + bucle)
├── gex_win32_clock        QueryPerformanceCounter → µs monotónicos
├── gex_win32_window       HWND, WndProc, cola de mensajes
│     └── gex_input          down[] / pressed[] / released[]
└── gex_i4_app
      └── gex_frame           frame loop de I-3 (sin cambios)
            ├── pre_frame     stub: cuenta llamadas
            ├── update        stub: cuenta llamadas (× N)
            ├── audio         no adjunto (gex_frame.audio == NULL)
            └── present       gex_null_renderer_present: cuenta, no dibuja
```

Regla de aislamiento: **solo `win32_time.c`, `win32_window.c` e
`i4_main_win32.c` incluyen `windows.h`**. `clock.c`, `key_state.c`,
`null_renderer.c`, `i4_app.c`, `loop_core.c` y `frame.c` son C11 portable y se
prueban sin ventana. `win32_window.h` expone `HWND` porque el llamante es
Windows; el resto de la API no filtra tipos Win32.

### 3.1 Por qué esta separación

- `gex_input` no sabe qué es un `HWND`: recibe códigos de tecla virtuales.
  Así las transiciones se prueban de forma determinista (826 checks).
- `gex_i4_app` no sabe qué es Windows: recibe `now_us` y devuelve el estado
  del frame. Los 117 checks lo demuestran con un reloj guionizado.
- `gex_win32_window` no contiene lógica de juego ni de tiempo: solo traduce
  mensajes a las cuatro operaciones de `gex_input` y marca el cierre.
- El frame loop conserva su contrato de I-3: no abre, cierra ni reinicia
  recursos que no posee.

## 4. Ventana y flujo de mensajes

Clase: `GexNativeLoopWindow`. Título: `Gex native loop - I-4 (null renderer)`.
Área de cliente: 960×540. Estilo `WS_OVERLAPPEDWINDOW`.

| Mensaje | Acción |
| --- | --- |
| `WM_NCCREATE` | guarda el puntero `gex_win32_window` en `GWLP_USERDATA` |
| `WM_KEYDOWN` / `WM_SYSKEYDOWN` | `gex_input_key_down(vk)`; el auto-repeat se coalesce |
| `WM_KEYUP` / `WM_SYSKEYUP` | `gex_input_key_up(vk)` |
| `WM_KILLFOCUS` | `gex_input_focus_lost()`: libera todas las teclas pulsadas |
| `WM_CLOSE` | `DestroyWindow` |
| `WM_DESTROY` | marca `quit`, limpia `GWLP_USERDATA`, `PostQuitMessage(0)` |
| `WM_ERASEBKGND` | devuelve 1: no se pinta ni el fondo |
| `WM_PAINT` | `BeginPaint`/`EndPaint`: solo valida la región; no dibuja |
| resto | `DefWindowProcW` (Alt+F4 y menú del sistema siguen funcionando) |

Cada iteración del bucle hace, en este orden:

1. `gex_win32_window_poll()`: drena la cola con `PeekMessageW(PM_REMOVE)`.
2. Si `WM_QUIT` o `WM_DESTROY`, termina.
3. Registra en consola solo las **transiciones** (`key down`, `key up`,
   `focus lost`), nunca un mensaje por frame.
4. `gex_i4_app_step(now_us)`: ejecuta el frame y después interpreta Escape.
5. `gex_input_begin_frame()` limpia `pressed[]`/`released[]`; `down[]` persiste.

### 4.1 Cierre

- **Escape**: `gex_i4_app_step` comprueba `gex_input_was_pressed(ESCAPE)`
  después del frame y marca `app.quit`.
- **Cierre de ventana / Alt+F4**: `WM_CLOSE` → `DestroyWindow` → `WM_DESTROY`
  → `window.quit` y `PostQuitMessage`. El bucle sale y llama a
  `gex_win32_window_destroy` + `gex_i4_app_shutdown`.
- Ambos caminos comparten la misma limpieza: `timeEndPeriod(1)` (si se
  activó), `DestroyWindow` si aún existe, `UnregisterClassW`, `gex_frame_shutdown`
  y `gex_null_renderer_shutdown`.

## 5. Reloj real

`gex_win32_clock_init` llama a `QueryPerformanceFrequency` y
`QueryPerformanceCounter`; guarda la frecuencia y el valor inicial. Cada
`gex_win32_clock_now_us` devuelve **microsegundos desde el inicio**:

```text
us = (now - start) / freq * 1 000 000 + ((now - start) % freq) * 1 000 000 / freq
```

La división se parte en cociente y resto para que `ticks * 1 000 000` no tenga
que caber en 64 bits. `gex_ticks_to_us` es portable y se prueba con
truncamiento, frecuencia 0 y valores grandes. Si el contador retrocede, el
proveedor devuelve 0 y el frame loop lo trata como `GEX_FRAME_WARN_CLOCK`
(aviso, no error), como ya fijó I-3.

En esta máquina `QueryPerformanceFrequency` devolvió **10 000 000 Hz**.

## 6. Cadencia, espera y CPU

### 6.1 Elección

- **Paso lógico: 16 667 µs (60 Hz).** Es el preset desacoplado de I-3 y la
  cuantía del sello `16.666` que el update del original ya usa. No se elige
  40 ms: los 40 ms medidos son el periodo de **presentación bloqueante** del
  original (swap), no la frecuencia lógica.
- **Presentación: objetivo 8 333 µs (120 Hz).** Es una decisión del port para
  mantener la ventana y la entrada responsivas con un renderer nulo; no afirma
  nada sobre la cadencia del original ni sobre VSync. El paso lógico no depende
  de ella: en las pruebas reales se mantuvo en 59,5–60,0 pasos/s.

### 6.2 Espera

Entre presentaciones el bucle espera en:

```c
MsgWaitForMultipleObjectsEx(0, NULL, wait_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
```

Así duerme sin consumir un núcleo y despierta inmediatamente si llega un
mensaje (tecla, cierre, pintura). No hay `Sleep(0)` ni sondeo activo.

### 6.3 Resolución del temporizador (hallazgo medido)

La primera ejecución real, **sin** tocar la resolución del temporizador, dio
~65 presents/s, no 120: `MsgWaitForMultipleObjectsEx` usa la granularidad por
defecto de Windows (~15,6 ms), así que una espera de 9 ms se redondeaba al
siguiente tick. El paso lógico seguía siendo correcto (59,7–59,9/s), pero la
presentación no alcanzaba el objetivo.

Solución: `timeBeginPeriod(1)` al empezar el bucle y `timeEndPeriod(1)` al
terminar. Con 1 ms de resolución se midieron **104,9–107,7 presents/s** y
**59,5–60,0 pasos/s**. La diferencia restante hasta 120 es el redondeo a
milisegundos de `MsgWaitForMultipleObjectsEx` (8 333 µs → 9 ms), no un error
del acumulador. Se documenta el valor efectivo, no se presenta 120 como
medido.

### 6.4 CPU medida

Medición con `Process.TotalProcessorTime` / tiempo de pared durante la prueba
real:

| Ejecución | Duración | CPU | % de un núcleo |
| --- | ---: | ---: | ---: |
| Escape tras teclas y foco | 2,27 s | 0,031 s | 1,38 % |
| Cierre con `WM_CLOSE` | 1,62 s | 0,031 s | 1,93 % |
| 360 presents sin mensajes | 3,68 s | 0,031 s | 0,85 % |

No hay consumo permanente de un núcleo. La cifra incluye arranque, creación de
ventana y cierre.

## 7. Estado de entrada

`gex_input` mantiene tres tablas de 256 entradas (códigos virtuales Win32):

- `down[vk]`: nivel actual (tecla mantenida).
- `pressed[vk]`: flanco de bajada en este frame.
- `released[vk]`: flanco de subida en este frame.

Reglas, todas probadas:

1. `key_down` sobre una tecla ya pulsada **no** crea otro flanco (auto-repeat).
2. `key_up` sobre una tecla que no estaba pulsada **no** crea flanco.
3. `focus_lost` libera todas las teclas pulsadas y produce sus flancos de
   subida; un `WM_KEYUP` tardío se ignora.
4. `begin_frame` limpia solo los flancos; el nivel persiste.
5. Códigos fuera de `0..255` y punteros `NULL` se ignoran.
6. Escape se interpreta **después** del frame, para que el frame actual vea la
   pulsación.

El estado de foco es deliberadamente conservador: si la ventana pierde el
foco, ninguna tecla queda «atascada» aunque el `WM_KEYUP` llegue a otra
ventana.

## 8. API y ciclo de vida

```c
/* reloj */
int      gex_win32_clock_init(gex_win32_clock *clock);
uint64_t gex_win32_clock_now_us(gex_win32_clock *clock);
uint64_t gex_win32_clock_freq(const gex_win32_clock *clock);

/* ventana */
int  gex_win32_window_create(gex_win32_window *w, gex_input *input,
                             const wchar_t *title, int width, int height);
void gex_win32_window_poll(gex_win32_window *w);
int  gex_win32_window_should_quit(const gex_win32_window *w);
void gex_win32_window_destroy(gex_win32_window *w);

/* aplicación */
gex_frame_status gex_i4_app_init(gex_i4_app *app,
                                 const gex_frame_config *cfg, uint64_t t0_us);
gex_frame_status gex_i4_app_step(gex_i4_app *app, uint64_t now_us);
int  gex_i4_app_should_quit(const gex_i4_app *app);
void gex_i4_app_shutdown(gex_i4_app *app);
```

Orden de arranque: reloj → `gex_i4_app_init` (crea `gex_frame`, `gex_input` y
el renderer nulo) → ventana (recibe `&app.input` prestado) → bucle → limpieza.
Si falla el reloj, la app o la ventana, se libera lo ya creado y se devuelve
un código distinto de 0. `gex_i4_app_shutdown` es idempotente; `step` tras el
cierre devuelve `GEX_FRAME_ERR_STATE`.

## 9. Pruebas automatizadas

```powershell
cd port
.\build.ps1 all
```

Resultado exacto (Windows 11, Clang 22.1.8, `-std=c11 -O2 -Wall -Wextra
-Wpedantic`, sin avisos):

```text
69305 checks, 0 failures    test_sag
60052 checks, 0 failures    test_adpcm
215 checks, 0 failures      test_audio
410 checks, 0 failures      test_frame
826 checks, 0 failures      test_input          (nuevo)
117 checks, 0 failures      test_i4_app         (nuevo)
1009 checks, 0 failures     test_win32_time     (nuevo)
50 checks, 0 failures       test_device
```

Total: **131 984 checks, 0 fallos**. El harness `tools/m13-core` sigue dando
**120 checks, 0 fallos**; `node --test tools/gexmap/test.mjs` da **22/22**;
`cli.mjs check` y `cli.mjs segments` no reportan problemas.

**Validación independiente con GCC de GNU 16.1.0 (MSYS2, fuera del `PATH`).**
La misma batería compiló con
`C:\msys64\mingw64\bin\gcc.exe -std=c11 -O2 -Wall -Wextra -Wpedantic` sin
avisos, ejecutó **131 984 checks, 0 fallos** y `gexloop.exe` compilado con GCC
también arrancó y salió limpio (`--frames 20`: 108,4 presents/s, 59,6 pasos/s,
0 errores). El `gcc.exe` del `PATH` es un driver de Clang 22.1.8 y no se usó
como segunda validación. Para que GNU ld enlazara, `build.ps1` coloca ahora
las librerías (`-ldsound`, `-luser32`, `-lgdi32`, `-lwinmm`) **después** de las
fuentes; LLD las aceptaba en el orden anterior.

Qué cubre cada batería nueva:

- `test_input` (826): init, pulsación, auto-repeat, liberación, varias teclas
  en el mismo frame, pérdida de foco, códigos inválidos, `NULL` y Escape.
- `test_i4_app` (117): conversión de reloj, renderer nulo, orden
  `pre → update × N → audio → present`, catch-up, Escape, pérdida de foco,
  reloj invertido, init/cierre idempotente, re-init y errores de argumento.
- `test_win32_time` (1009): `NULL`, frecuencia > 0, 1 000 muestras monótonas,
  avance tras `Sleep(20)` y ausencia de saltos absurdos.

`test_win32_time` es Windows-only y no está en el `Makefile` POSIX;
`test_input` y `test_i4_app` sí son portables y se añadieron a `make test`.
El `Makefile` **no se pudo ejecutar** en este Windows (no hay `make`).

## 10. Prueba real de ventana

Se arrancó `port\build\gexloop.exe` desde PowerShell y se localizó la ventana
por clase con `FindWindowEx`. Los mensajes se entregaron con `PostMessage`
(`WM_KEYDOWN`/`WM_KEYUP`/`WM_KILLFOCUS`/`WM_CLOSE`), que es la misma cola y el
mismo `WndProc` que usa el teclado real. **No se inyectó teclado físico**
(limitación W7).

| Ejecución | Ventana | Salida | Código | Duración | Presents/s | Pasos/s | CPU |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| A: W↓, W↑, A↓, foco perdido, Escape | sí (`GexNativeLoopWindow`) | limpia | 0 | 2,20 s | 107,7 | 60,0 | 1,38 % |
| B: `WM_CLOSE` | sí | limpia | 0 | 1,56 s | 104,9 | 59,5 | 1,93 % |
| C: 360 presents sin mensajes | sí | límite de frames | 0 | 3,65 s | 98,6 | 60,0 | 0,85 % |

Salida relevante de A (recortada):

```text
clock: QueryPerformanceCounter, 10000000 Hz
frame: decoupled, step=16667 us (60 Hz), max_dt=250000 us, max_steps=5
present: target 8333 us (120 Hz), logical and present cadences independent
timer: 1 ms resolution (timeBeginPeriod(1))
input: key down W (0x57)
input: key up W (0x57)
input: key down A (0x41)
input: focus lost (1 total)
input: key up A (0x41)
stats: t=2.01s presents=216 fps=107.6 steps=120 steps/s=59.8 ... errors=0
input: key down Escape (0x1B)
final: t=2.20s presents=237 fps=107.7 steps=132 steps/s=60.0 ... aborted=0 backwards=0
exit: clean, frame errors=0, clock warnings=0
```

En las tres ejecuciones: `discards=0`, `aborted=0`, `backwards=0`,
`frame errors=0`, `clock warnings=0`, sin bloqueos. El cierre por Escape y por
`WM_CLOSE` termina el proceso con código 0.

Notas de método (fallos reales del proceso de prueba, no del ejecutable):

- El primer intento usó `FindWindow` con título nulo y no localizó la ventana;
  se sustituyó por `FindWindowEx` por clase. El ejecutable sí había creado la
  ventana.
- El primer enlace con GCC de GNU falló con `undefined reference to
  DirectSoundCreate` porque `build.ps1` ponía `-ldsound` antes de las fuentes
  (Clang/LLD lo toleraba, GNU ld no). Se movieron las librerías al final; el
  mismo cambio se aplicó a `-luser32`, `-lgdi32` y `-lwinmm`.
- La primera medición sin `timeBeginPeriod(1)` dio ~65 presents/s; es el
  hallazgo de §6.3, no un fallo del acumulador (el paso lógico siguió en
  ~60/s).

**Qué demuestra:** que el proceso arranca, crea una ventana Win32 real, corre
de forma sostenida, recibe y clasifica pulsaciones/liberaciones, libera el
estado al perder foco, responde a Escape y al cierre, usa un reloj real y no
acumula tiempo de forma descontrolada.

**Qué no demuestra:** que la ventana se vea o se haya operado con un teclado
físico (la comprobación es programática), ni que haya juego o renderizado.

## 11. Qué es real y qué es stub

| Elemento | Estado |
| --- | --- |
| Ventana Win32, clase, `WndProc`, cola de mensajes | **real** |
| `WM_KEYDOWN`/`WM_KEYUP`/`WM_KILLFOCUS`/`WM_CLOSE` | **real**, enrutados |
| `QueryPerformanceCounter`/`QueryPerformanceFrequency` | **real** |
| Conversión a µs y guarda de retroceso | **real**, probada |
| Estado `down`/`pressed`/`released` y foco | **real**, probado |
| Escape y cierre limpio | **real**, ejecutado |
| Frame loop, acumulador, catch-up y descartes | **real** (I-3) |
| `present` conectado y contabilizado | **real** |
| Dibujo de píxeles, superficies, swap | **no existe** (renderer nulo) |
| `pre_frame` | **stub**: cuenta llamadas; no traduce entrada |
| `update` | **stub**: cuenta llamadas; no simula nada |
| Audio | **no adjunto**: `gex_frame.audio == NULL` |
| Lógica de juego, assets, cámara, colisiones | **no existe** |

El renderer nulo **no es un placeholder que aparente renderizar**: su cabecera
dice explícitamente que no hay dispositivo, superficie ni swap, y `present`
devuelve 0 tras incrementar un contador. Cuando llegue un renderer real,
sustituirá este módulo en el hook `present` sin cambiar el frame loop.

## 12. Discrepancias con el original

| ID | Discrepancia | Motivo |
| --- | --- | --- |
| W1 | La entrada del port son mensajes Win32; la del original es S5 (sondeo de joystick + `VkKeyScanA`), con la fuente de teclado sin confirmar | Decisión explícita del port; no se presenta como reproducción |
| W2 | Paso lógico 60 Hz y presentación objetivo 120 Hz | I-3 desacopla a propósito; los 40 ms del original son presentación bloqueante, no paso lógico |
| W3 | Ventana 960×540 vacía; el original abre Glide 512×384@75 Hz (`0x0046324D`) | No hay renderer ni Glide en I-4; no se afirma equivalencia visual |
| W4 | Escape como control de salida | Comodidad del port; no está demostrado que el original salga con Escape en gameplay |
| W5 | Auto-repeat coalescido (una pulsación por tecla mantenida) | Semántica elegida y probada; el original podría repetir |
| W6 | `timeBeginPeriod(1)` mientras corre el bucle | Necesario para la cadencia de presentación; se restaura con `timeEndPeriod(1)` |
| W7 | Prueba con `PostMessage`, no con teclado físico | Verificación programática; el `WndProc` y la cola son los reales |
| W8 | Solo estado de teclas virtuales; sin texto, IME, teclas muertas ni `DirectInput` | I-4 solo necesita pulsaciones/liberaciones |

## 13. Límites y pendientes

- **Sin renderizado real.** I-5 (o el hito que corresponda) debe decidir la
  API y sustituir `gex_null_renderer`.
- **Sin lógica de juego.** `pre_frame` y `update` no traducen entrada a
  acciones; el mapeo de teclas del original no está demostrado.
- **Sin audio adjunto.** El planificador existe desde I-2b, pero `gexloop` no
  crea un stream ni un dispositivo; no se inventan assets `.sag`.
- **Fuente de entrada del original pendiente.** S5 es HIPÓTESIS; la lectura
  exacta de teclado sigue sin confirmar. I-4 no la usa como prueba.
- **Prueba de ventana programática.** No se inyectó teclado físico ni se
  comprobó visualmente el contenido (la ventana está vacía por diseño).
- **Sin `make` en este Windows.** El `Makefile` se actualizó pero no se
  ejecutó; el build verificado es `build.ps1`.
- **GCC de GNU 16.1.0 (MSYS2), fuera del `PATH`.** El `gcc.exe` del `PATH` es
  un driver de Clang 22.1.8 y **no** cuenta como segunda validación; el GCC
  real de MSYS2 sí se usó explícitamente y pasó la misma batería (§9). Se
  corrigió el orden de `-l...` en `build.ps1`: iban antes de las fuentes, lo
  que Clang/LLD toleraba pero GNU ld no resolvía.
- **Integridad de los cuatro archivos personales no verificada.** No se
  tocaron ni se publicaron; tras la incidencia del clon temporal no se afirma
  haber comprobado su integridad en disco.

`docs/PROJECT_STATUS.svg` **no se modifica**: mide MAIN (versión jugable), no
componentes del port. I-4 no cambia el estado de MAIN.
