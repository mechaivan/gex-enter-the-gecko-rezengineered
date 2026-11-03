# `port/` - port nativo (esqueleto mínimo)

Código nuevo del port de Gex: Enter the Gecko (PC, EU v1.00.000). No contiene
ni redistribuye el ejecutable original ni assets del juego: solo implementación
propia escrita a partir de la evidencia documentada en `docs/`.

## Estado

Cuatro subsistemas implementados y probados sin el juego; el último habla con el
dispositivo de sonido real y, desde I-4, existe además un **proceso nativo con
ventana Win32 real**. Sobre esa ventana, R-1 crea el dispositivo Direct3D 11 y
R-2 la swapchain DXGI, todavía sin presentar ni dibujar:

| Componente | Original | Cabecera | Estado |
| --- | --- | --- | --- |
| Lector del contenedor de audio `SLAG` | `0x0053899C` + carga `0x005388AC` | `gex/asset/sag.h` | funcional, 69 305 checks |
| Decodificador IMA ADPCM de 4 bits | `0x00538285` + tablas `0x00558138`/`0x00558178` | `gex/audio/adpcm.h` | funcional, 60 052 checks |
| Bomba de streaming + reproductor | `0x005380B3`, `0x00538731` | `gex/audio/stream.h` | funcional, 215 checks |
| Planificador del bombeo (frame loop) | llamantes `0x00463205`, `0x0053A5FB`, `0x0053FCB7` | `gex/audio/sched.h` | funcional |
| Backend DirectSound (dispositivo) | `0x00538551`, `CreateSoundBuffer(0x8000, 0xE0)`, `GetCurrentPosition`, `0x0053886c` | `gex/audio/backend_dsound.h` | funcional (Windows), 50 checks |
| Núcleo temporal (acumulador M-13) | `0x0043EA36` + `16.666` de `0x0054C060` | `gex/core/loop_core.h` | funcional, 120 checks + 410 |
| Frame loop nativo | orden de `0x0043F80D` → `0x0043E1BA` → `0x005380B3` → `0x0043DC3A` | `gex/core/frame.h` | funcional, 410 checks (incl. regresión pre_frame) |
| Estado de teclado | decisión del port (`WM_KEYDOWN`/`WM_KEYUP`; S5 del original = HYPOTHESIS) | `gex/input/key_state.h` | funcional, 826 checks |
| Reloj monotónico real | `QueryPerformanceCounter` | `gex/platform/win32_time.h` | funcional (Windows), 1 009 checks |
| Renderer nulo | no aplica: I-4 declara que no dibuja | `gex/render/null_renderer.h` | funcional, sin dibujo |
| Núcleo de aplicación | orden de I-3 + Escape/foco | `gex/app/i4_app.h` | funcional, 117 checks |
| Ventana Win32 | shell S6 (`0x00463091`/`0x00463278`), adaptado | `gex/platform/win32_window.h` | funcional (Windows), probada |
| Ejecutable I-4 | — | `src/app/i4_main_win32.c` | `gexloop.exe`, ventana real |
| Dispositivo D3D11 (R-1) | decisión de I-5 confirmada: D3D 11 | `gex/render/d3d11_device.h` | funcional (Windows), 205 checks; hardware y WARP a FL 11_1; sin swapchain |
| Swapchain DXGI (R-2) | `grSstWinOpen` `0x0046324D` (2 buffers de color), adaptado | `gex/render/d3d11_swapchain.h` | funcional (Windows), 429 checks; bitblt, 2 buffers, 512×384, resize; sin `Present` |
| Renderer real | semántica Glide del original (38 imports, `docs/RENDERER_ARCHITECTURE_RESEARCH.md`) | — | **no implementado aún**: R-3+ (clear, presentación, geometría) |

No hay motor, ni render, ni lógica de juego. La ventana, el teclado, el reloj
QPC y el frame loop **sí** son reales: `gexloop.exe` abre una ventana, responde
a teclas, libera el estado al perder foco y sale con Escape o al cerrar. El
audio sigue siendo audible por separado: `sagplay --device` entrega el PCM a un
`IDirectSoundBuffer` real. Lo que **no** está verificado es la audición humana
(el agente no puede oír); la confirmación por oído es del mantenedor.

`gexloop` no es un build jugable: `pre_frame` y `update` solo cuentan llamadas
y `present` usa un renderer nulo que no dibuja. Es el primer «prototipo
ejecutable», no el juego.

## Estructura

```
port/
  include/gex/asset/sag.h            API pública del contenedor (C11, sin dependencias)
  include/gex/audio/adpcm.h          decodificador ADPCM + tablas inyectables
  include/gex/audio/stream.h         buffer, bomba, reproductor y backend
  include/gex/audio/sched.h          cuándo llamar a la bomba (una vez por frame)
  include/gex/audio/backend_dsound.h dispositivo DirectSound (sólo Windows)
  include/gex/core/loop_core.h       acumulador de paso fijo (núcleo M-13)
  include/gex/core/frame.h           frame loop: fases, hooks, ciclo de vida
  include/gex/input/key_state.h      teclado: nivel, flancos y pérdida de foco
  include/gex/render/null_renderer.h renderer nulo (present sin dibujar)
  include/gex/render/d3d11_device.h  dispositivo D3D11 real (sólo Windows, R-1)
  include/gex/render/d3d11_swapchain.h swapchain DXGI sobre el HWND (sólo Windows, R-2)
  include/gex/app/i4_app.h           núcleo de aplicación (frame + input + present)
  include/gex/platform/clock.h       conversión portable ticks → µs
  include/gex/platform/win32_time.h  reloj QPC (sólo Windows)
  include/gex/platform/win32_window.h ventana, WndProc y mensajes (sólo Windows)
  src/asset/sag.c                    transcripción del parser y del cargador
  src/audio/adpcm.c                  0x00538285
  src/audio/stream.c                 0x005380B3 + 0x00538731 + buffer secundario
  src/audio/sched.c                  el contrato de los llamantes de 0x005380B3
  src/audio/backend_dsound.c         IDirectSoundBuffer real (Lock/Play/cursor/volumen)
  src/core/loop_core.c               la única implementación del acumulador
  src/core/frame.c                   orden de frame + tick de audio + errores
  src/input/key_state.c              tablas de teclado (C11 puro)
  src/render/null_renderer.c         present que contabiliza y no dibuja
  src/render/d3d11_device.c          D3D11CreateDevice + ciclo COM (sólo Windows)
  src/render/d3d11_swapchain.c       DXGI factory/swapchain/backbuffer/resize (sólo Windows)
  src/app/i4_app.c                   orquestación portable (Escape, limpieza)
  src/app/i4_main_win32.c            ejecutable I-4 (bucle, espera, estadísticas)
  src/platform/clock.c               aritmética de ticks a microsegundos
  src/platform/win32_time.c          QueryPerformanceCounter (Windows)
  src/platform/win32_window.c        CreateWindowExW + WndProc (Windows)
  tests/test_sag.c                   batería sintética + fuzz (sin el juego)
  tests/test_adpcm.c                 vector dorado, clamp, continuidad, fuzz
  tests/test_audio.c                 buffer, bomba, gate del cursor, backend, sched
  tests/test_frame.c                 frame loop: orden, catch-up, errores, pausa
  tests/test_input.c                 teclado: flancos, auto-repeat, foco, Escape
  tests/test_app.c                   app + reloj + renderer + ciclo de vida
  tests/test_win32_time.c            QPC real: monotonía y avance (Windows)
  tests/test_device.c                dispositivo real (se omite si no hay tarjeta)
  tests/test_d3d11_device.c          dispositivo D3D11: HW+WARP, COM, errores (Windows)
  tests/test_d3d11_swapchain.c       swapchain: ventana real, resize, COM, ciclo (Windows)
  tests/make_demo_sag.mjs            genera un contenedor sintético de 54 bytes
  tools/sagdump.c                    CLI que muestra lo que el lector entendió
  tools/sagplay.c                    demo: SLAG sintético -> PCM -> WAV o dispositivo
  tools/framedemo.c                  demo: frame loop con reloj guionizado
  tools/d3d11probe.c                 sonda del dispositivo D3D11 (R-1, Windows)
  tools/swapprobe.c                  sonda de la swapchain sobre ventana real (R-2, Windows)
  build.ps1                          build en Windows (no hay make)
  Makefile                           equivalente POSIX (sin backend DirectSound)
```

`build/` está ignorado por Git: solo contiene artefactos de compilación.

## Compilar y probar

Windows (PowerShell, sin `make` en este entorno):

```powershell
cd port
.\build.ps1 all      # compila, ejecuta los checks y las demos
.\build.ps1 test     # solo checks (contenedor + ADPCM + audio + frame + entrada + app + reloj + dispositivos)
.\build.ps1 loop     # solo gexloop.exe (ventana Win32 real)
.\build.ps1 probe    # d3d11probe.exe + swapprobe.exe (dispositivo y swapchain; Windows)
.\build.ps1 audio    # solo la demo de audio (escribe build\tone.wav)
.\build.ps1 demo     # fixture sintético + sagdump + framedemo + demo de audio
.\build.ps1 clean    # borra build\ (solo lo que genera este script)
```

Frame loop (I-3), sin ventana ni juego:

```powershell
.\build\framedemo.exe --frames 4
.\build\framedemo.exe --decoupled --frames 3 --dt 60000 --audio
```

Primer proceso con ventana (I-4), renderer nulo:

```powershell
.\build.ps1 loop
.\build\gexloop.exe                 # corre hasta Escape o cerrar la ventana
.\build\gexloop.exe --frames 360
.\build\gexloop.exe --seconds 5
```

Para oírlo (necesita dispositivo de sonido; el tono sintético dura 0,5 s):

```powershell
.\build\sagplay.exe --device
```

POSIX:

```bash
cd port
make test
make audio
make demo
```

El `Makefile` es el equivalente POSIX del script; **no se ha podido ejecutar en
este entorno** porque el Windows del mantenedor no tiene `make`. El build que
sí está verificado es `build.ps1` (clang y gcc). `make audio` enlaza con `-lm`
porque la demo usa `sin`. El backend DirectSound, los módulos D3D11 y la capa
Win32 son sólo Windows: el `Makefile` no compila `backend_dsound.c`,
`d3d11_device.c`, `d3d11_swapchain.c`, `win32_time.c`,
`win32_window.c`, `test_device.c`, `test_d3d11_device.c`,
`test_d3d11_swapchain.c`, `test_win32_time.c`, `d3d11probe.c`, `swapprobe.c`
ni `gexloop`, así que `sagplay --device`, `d3d11probe`, `swapprobe` y
`gexloop` no existen en POSIX; `test_sag`, `test_adpcm`,
`test_audio`, `test_frame`, `test_input`, `test_i4_app` y `framedemo` siguen
siendo portables.

Compilador: `clang` o `gcc` con `-std=c11`. Se puede forzar con
`.\build.ps1 test -Compiler gcc`.

## Verlo funcionar

Contenedor:

```powershell
cd port
.\build.ps1 demo
```

Salida esperada del `sagdump` (fixture sintético, no del juego):

```
demo.sag: 54 bytes (synthetic)
file:   build/demo.sag
size:   54 bytes
form:   SLAG size=0x0000002E (46)
format: chunk=18 copied=18
        wFormatTag=0x0001 nChannels=1 nSamplesPerSec=22050
        nAvgBytesPerSec=22050 nBlockAlign=1 wBitsPerSample=8 cbSize=0
data:   offset=46 size=8 complete
```

Audio:

```powershell
cd port
.\build.ps1 audio
```

Salida medida:

```
source:  synthetic SLAG (440 Hz, 0.5 s, 22050 Hz, built in memory)
bytes:   5558
format:  PCM mono 22050 Hz, 16-bit (declared by "fmt ")
payload: 5512 bytes of 4-bit ADPCM (11024 samples)
pumps:   4 calls of 0x005380B3 (0 deferred by the gate)
chunks:  2 read, 1 short
decoded: 11024 samples
halves:  3 submitted, 24576 samples (1.115 s)
stops:   1
pcm:     peak=12241 fnv1a=0x80D4F719 over 24576 delivered samples
wav:     build/tone.wav (written, 24576 samples)
```

Con `--device` el mismo recorrido va al dispositivo. Salida medida en esta
máquina (el número de `pumps` y el tiempo de pared varían entre ejecuciones
porque el bucle de espera usa `Sleep(1)`; el audio entregado no):

```
pumps:   60 calls of 0x005380B3 (56 deferred by the gate)
halves:  3 submitted, 24576 samples (1.115 s)
stops:   1
device:  DirectSound default, DSSCL_NORMAL, 22050 Hz mono 16-bit
submits: 3 halves, 0 restores, 0 lock failures
cursor:  16384 bytes before the stop, 390 ms wall
note:    the PCM was handed to the device; confirm by ear
```

Los `deferred` son el *gate* del cursor actuando en tiempo real. El `Stop` de la
rama de parada corta la cola de silencio, igual que el original.

Frame loop (I-3):

```powershell
cd port
.\build\framedemo.exe --frames 4
```

Salida medida (reloj guionizado, sin hardware):

```text
config:  original 1:1, step=40000 us, max_dt=250000 us, max_steps=5
frame  0: now=   40000 us  steps=1  present
frame  1: now=   80000 us  steps=1  present
frame  2: now=  120000 us  steps=1  present
frame  3: now=  160000 us  steps=1  present
totals:  frames=4 steps=4 presents=4 audio_ticks=0 discards=0 aborted=0 backwards=0
```

Con `--decoupled --dt 60000` el acumulador recupera: tres presentaciones, diez
pasos lógicos (3, 4, 3) y un tick de audio por presentación. Las fases siguen
siendo stubs: no hay juego ni renderer.

Ventana y reloj (I-4, Windows; renderer nulo):

```powershell
.\build\gexloop.exe --frames 360
```

Salida medida (ventana real, 3,65 s):

```text
clock: QueryPerformanceCounter, 10000000 Hz
frame: decoupled, step=16667 us (60 Hz), max_dt=250000 us, max_steps=5
present: target 8333 us (120 Hz), logical and present cadences independent
timer: 1 ms resolution (timeBeginPeriod(1))
window: class=GexNativeLoopWindow, 960x540 client area
renderer: null (draws nothing)
audio: none attached
controls: Escape or close the window to exit
stats: t=2.01s presents=214 fps=106.6 steps=120 steps/s=59.7 ...
final: t=3.65s presents=360 fps=98.6 steps=219 steps/s=60.0 ...
exit: clean, frame errors=0, clock warnings=0
```

`Escape` o cerrar la ventana terminan el proceso con código 0. La consola solo
imprime transiciones de teclado/foco y estadísticas cada 2 s, nunca un mensaje
por frame. En esta máquina la prueba real midió 104,9–107,7 presents/s,
59,5–60,0 pasos/s y 0,85–1,93 % de CPU. Sin `timeBeginPeriod(1)` la
granularidad de Windows (~15,6 ms) capaba la presentación a ~65/s; se restaura
con `timeEndPeriod(1)`. La ventana está vacía: `present` es nulo y
`pre_frame`/`update` son stubs.

Dispositivo D3D11 (R-1, Windows; sin swapchain ni dibujo):

```powershell
.\build\d3d11probe.exe
```

```text
hardware ok      feature level 11_1  flags 0x0  hr 0x00000000
warp     ok      feature level 11_1  flags 0x0  hr 0x00000000
baseline: ok
```

Ambos modos se crean, se consultan y se destruyen sin dejar referencias COM.
La prueba automatizada (`test_d3d11_device`) añade los caminos de error
(`E_INVALIDARG` con bits reservados, capa debug ausente) y el ciclo de vida.
Detalle en `docs/D3D11_DEVICE.md`.

Swapchain DXGI sobre la ventana real (R-2, Windows; sin `Present`):

```powershell
.\build\swapprobe.exe --hardware
```

```text
warp: swapchain 512x384, 2 buffers, format 28, windowed 1, swap effect 0
warp: backbuffer texture 512x384, mips 1, array 1, usage 0
warp: resize 512x384 ok
warp: resize 640x480 ok
warp: 0x0 refused without touching DXGI (minimized case)
warp: closed (backbuffer, swapchain, device)
hardware: swapchain 512x384, 2 buffers, format 28, windowed 1, swap effect 0
hardware: closed (backbuffer, swapchain, device)
```

La prueba (`test_d3d11_swapchain`) añade el ciclo COM medido, resize repetido,
errores de creación, doble `init`, dos ventanas con un dispositivo y el caso de
ventana destruida. **No se presenta ningún frame ni se dibuja**: la ventana
sigue vacía. Detalle en `docs/D3D11_SWAPCHAIN.md`.

`sagdump` y `sagplay` aceptan cualquier ruta: si tienes un `.sag` propio,
pásalo como argumento (`sagplay fichero.sag salida.wav`, o
`sagplay --device fichero.sag`) y procesan ese. No se incluye ninguno.

## Reglas del código

- **Evidencia primero.** Cada función nueva cita la dirección del original en
  su comentario de cabecera. Sin dirección, no hay implementación.
- **Sin dependencias.** C11 y libc en las piezas puras: nada de Win32 en
  `adpcm`/`stream`/`sched`, ni en `key_state`/`null_renderer`/`i4_app`/
  `clock`, lo que permite probarlas en cualquier plataforma y reutilizarlas
  después. La salida de audio es un *backend* con
  `submit`/`stop`/`cursor`/`pause`/`resume` que inyecta el llamante; la capa
  Windows vive solo en `backend_dsound.c`, `win32_time.c`, `win32_window.c`,
  `d3d11_device.c`, `d3d11_swapchain.c` e `i4_main_win32.c`, y usa la API del
  sistema (`dsound.h`, `d3d11.h`, `dxgi.h`, `user32`, `gdi32`, `winmm`), no una
  librería de terceros.
- **Las desviaciones se documentan.** Si el original confía en el contenedor y
  la implementación comprueba límites, se dice y se justifica en
  `docs/SAG_CONTAINER.md` §6 y en `docs/AUDIO_MIXER_RE.md` §11 (D1-D14).
- **Datos y código separados.** Ningún asset extraído del binario vive en
  `port/`. Los fixtures se generan por script.
- **Nada de placeholders presentados como implementación.** Si una pieza es
  simulación, se llama simulación. El audio llega a un dispositivo real y su
  cursor avanza a la frecuencia correcta, pero la audición humana no la verifica
  el agente: eso se dice tal cual y se deja la comprobación a `--device`.
  En el frame loop pasa lo mismo: `pre_frame`, `update` y `present` son
  callbacks de prueba y el bucle se llama bucle, no juego. El renderer de I-4
  se llama **nulo** y no aparenta dibujar; la ventana es real, pero está vacía.
- **Una sola implementación por pieza.** El acumulador de paso fijo vive en
  `src/core/loop_core.c`; `tools/m13-core` compila esa misma fuente con un shim
  de compatibilidad en vez de tener una copia que diverja.

## Documentación

- `docs/SAG_CONTAINER.md` - formato `SLAG`, algoritmo con direcciones,
  desviaciones, pruebas y límites.
- `docs/AUDIO_MIXER_RE.md` - decodificador ADPCM, bomba de streaming,
  reproductor, tablas, desviaciones D7-D14, backend DirectSound y planificador
  (§16) y demos.
- `docs/AUDIO_INIT_PATHS.md` - rutas de inicialización de DirectSound en el
  original.
- `docs/FRAME_STEP_TRACE.md` - traza dinámica del frame step original y su
  correspondencia con el port (§7).
- `docs/FRAME_LOOP_PORT.md` - frame loop nativo: arquitectura, decisiones de
  tiempo, política de catch-up/errores, API, pruebas y discrepancias F1-F6.
- `docs/WINDOW_INPUT_PORT.md` - ventana Win32, mensajes, teclado, reloj QPC,
  cadencia/espera, renderer nulo, pruebas reales y discrepancias W1-W8.
- `docs/RENDERER_ARCHITECTURE_RESEARCH.md` - I-5: evidencia del render Glide
  del original, comparativa de APIs (D3D 11 recomendado), contrato
  `gex_renderer` propuesto (sin implementar) y plan de validación R-1…R-8.
- `docs/D3D11_DEVICE.md` - R-1: dispositivo D3D11 real, hardware y WARP,
  gestión COM, resultados medidos y límites.
- `docs/D3D11_SWAPCHAIN.md` - R-2: swapchain DXGI sobre el HWND de I-4,
  decisiones bitblt/formato/buffers, resize, COM, resultados medidos y
  contrato para R-3.
- `docs/FUNCTION_MAP.md` - mapa estático de funciones y grafo de llamadas.
- `TESTING.md` - batería completa del repositorio.
