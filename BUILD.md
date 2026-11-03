# BUILD — Construcción

> Estado: **parcial**. El port nativo tiene componentes con build y pruebas
> (`port/`, 2026-10-31 / 2026-11-03), incluido un backend de audio DirectSound
> que habla con el dispositivo real, un frame loop ejecutable con reloj
> inyectado, un proceso con **ventana Win32 real, teclado y reloj
> `QueryPerformanceCounter`** (renderer nulo) y, desde R-1/R-2, el
> **dispositivo Direct3D 11 real** (hardware y WARP) con una **swapchain DXGI**
> sobre esa ventana, verificados **sin `Present` ni dibujo**. No hay build
> jugable del juego: el resto de la implementación (Olas, tras Hito P) sigue
> pendiente.

## Port nativo (`port/`) — funcional

Requisitos: un compilador C11 (`clang` o `gcc`) y Node ≥ 18 solo para generar
el fixture de demostración. Sin SDKs, sin assets del juego. En Windows el
backend de audio usa `dsound.h` + `-ldsound`, el proceso I-4 usa
`user32`, `gdi32` y `winmm`, y los módulos R-1/R-2 usan `d3d11.h`/`dxgi.h` +
`-ld3d11 -luuid`: todo viene con el toolchain (MinGW-w64/llvm-mingw) y
Windows, **no hay dependencias de terceros que descargar**.

```powershell
cd port
.\build.ps1 all      # compila, ejecuta 132 870 checks, las sondas D3D11 y las demos
.\build.ps1 test     # solo checks (contenedor + ADPCM + audio + frame loop + entrada + app + reloj + audio-device + D3D11 device + swapchain)
.\build.ps1 loop     # solo gexloop.exe (ventana Win32 real; Windows)
.\build.ps1 probe    # d3d11probe.exe y swapprobe.exe (dispositivo y swapchain; Windows)
.\build.ps1 audio    # solo la demo de audio (SLAG sintético -> build\tone.wav)
.\build.ps1 demo     # fixture sintético + sagdump + framedemo + demo de audio
.\build.ps1 clean    # borra build\ (solo lo que genera el script)
```

Frame loop (I-3), sin ventana ni juego:

```powershell
cd port
.\build\framedemo.exe --frames 4
.\build\framedemo.exe --decoupled --frames 3 --dt 60000 --audio
```

Primer proceso con ventana (I-4, Windows; renderer nulo):

```powershell
cd port
.\build.ps1 loop
.\build\gexloop.exe                 # corre hasta Escape o cerrar la ventana
.\build\gexloop.exe --frames 360    # sale tras 360 presentaciones
.\build\gexloop.exe --seconds 5     # sale tras 5 segundos
```

La consola muestra inicialización, frecuencia de QPC, cadencia y **solo** las
transiciones de teclado/foco; las estadísticas salen cada 2 s. No hay mensajes
por frame. `Escape` o el botón de cerrar terminan el proceso con código 0.

Sondas de Direct3D 11 (R-1 dispositivo, R-2 swapchain; Windows, sin dibujo):

```powershell
cd port
.\build.ps1 probe
.\build\d3d11probe.exe
.\build\swapprobe.exe --hardware
```

```text
hardware ok      feature level 11_1  flags 0x0  hr 0x00000000
warp     ok      feature level 11_1  flags 0x0  hr 0x00000000
baseline: ok
warp: swapchain 512x384, 2 buffers, format 28, windowed 1, swap effect 0
warp: resize 512x384 ok
warp: resize 640x480 ok
warp: 0x0 refused without touching DXGI (minimized case)
hardware: swapchain 512x384, 2 buffers, format 28, windowed 1, swap effect 0
hardware: closed (backbuffer, swapchain, device)
```

Reproducir por el altavoz (Windows, necesita dispositivo de sonido):

```powershell
cd port
.\build\sagplay.exe --device            # tono sintético 440 Hz, 0,5 s
.\build\sagplay.exe --device mi.sag     # un contenedor propio, si lo hay
```

En POSIX, `make test` / `make audio` / `make demo` / `make clean` (mismo
contenido; **no ejecutado en este entorno** porque no hay `make` en este
Windows). `make audio` enlaza con `-lm` (la demo usa `sin`). El backend
DirectSound, el dispositivo y la swapchain D3D11 y toda la capa Win32 son sólo
Windows: `Makefile` no compila `backend_dsound.c`, `d3d11_device.c`,
`d3d11_swapchain.c`, `test_device.c`, `test_d3d11_device.c`,
`test_d3d11_swapchain.c`, `d3d11probe.c`, `swapprobe.c`, `win32_time.c`,
`win32_window.c`, `test_win32_time.c` ni `gexloop`, así que `sagplay
--device`, `d3d11probe`, `swapprobe` y `gexloop` no existen en POSIX;
`test_sag`,
`test_adpcm`, `test_audio`, `test_frame`, `test_input`, `test_i4_app` y
`framedemo` siguen siendo portables (C11 puro). Desde I-3 el `Makefile`
también compila `src/core/loop_core.c` y `src/core/frame.c`; desde I-4 añade
`test_input` y `test_i4_app`. Los targets se actualizaron en la misma
revisión, pero **no se han podido ejecutar aquí**. Compilador alternativo:
`.\build.ps1 test -Compiler gcc` (el `gcc` del `PATH` es un driver de Clang).
Para el GCC de GNU de MSYS2, añade su `bin` al `PATH` y pasa la ruta completa;
ver «Entorno verificado».

Qué produce:

| Target | Artefacto | Uso |
| --- | --- | --- |
| `test` | `build/test_sag.exe`, `build/test_adpcm.exe`, `build/test_audio.exe`, `build/test_frame.exe`, `build/test_input.exe`, `build/test_i4_app.exe`, `build/test_win32_time.exe`, `build/test_device.exe`, `build/test_d3d11_device.exe`, `build/test_d3d11_swapchain.exe` | baterías del lector, de la ruta de audio, del frame loop, de la entrada, de la app, del reloj, del dispositivo de audio, del dispositivo D3D11 y de la swapchain (no se distribuyen) |
| `loop` | `build/gexloop.exe` | proceso I-4: ventana Win32 real, teclado, QPC y frame loop con renderer nulo (Windows) |
| `probe` | `build/d3d11probe.exe`, `build/swapprobe.exe` | informes del dispositivo D3D11 (R-1) y de la swapchain sobre la ventana real (R-2), WARP y hardware (Windows) |
| `audio` | `build/sagplay.exe` + `build/tone.wav` | demo: `SLAG` sintético → PCM → WAV (o dispositivo con `--device`) |
| `demo` | `build/sagdump.exe` + `build/demo.sag` + `build/framedemo.exe` + `build/sagplay.exe` | CLI de inspección, fixture sintético, frame loop guionizado y demo de audio |

`build/` está ignorado por Git. Estos artefactos son de desarrollo: no son
entregables del proyecto y no contienen nada del juego. `build/tone.wav` se
genera en cada ejecución de `audio`/`demo`.

Entorno verificado (2026-10-31, actualizado 2026-11-03): `clang` 22.1.8
(`x86_64-w64-windows-gnu`), `gcc` del mismo toolchain (driver de Clang) y GCC
de GNU 16.1.0 en `C:\msys64\mingw64\bin` (fuera del `PATH`), Node 26.7.0.
**No hay `make` en este Windows**, de ahí `build.ps1`. Desde R-1/R-2,
`d3d11.h` está verificado idéntico por MD5 en ambos toolchains y
`libd3d11.a`/`libuuid.a` están presentes en ambos. Para usar el GCC real:

```powershell
$env:PATH = 'C:\msys64\mingw64\bin;' + $env:PATH
.\build.ps1 test -Compiler 'C:\msys64\mingw64\bin\gcc.exe'
```

I-4, R-1 y R-2 se validaron con ambos compiladores: misma batería (I-4: 131 984
checks; R-1: 132 298 con la prueba D3D11 integrada; R-2: 132 870 con la de
swapchain), 0 fallos, sin avisos.

## Resto del proyecto — pendiente

Cuando exista implementación de juego (Olas, tras Hito P), este documento
describirá además:

- requisitos (compiladores, SDKs, dependencias);
- qué produce cada target (loader, DLL, parches…);
- cómo el usuario aporta sus archivos originales;
- verificación de artefactos (hashes).

De momento, ver [ROADMAP.md](ROADMAP.md) y [REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md).

Los builds de herramientas de desarrollo (`tools/glide-shim`,
`tools/m13-core`, `tools/gexmap`, `tools/frame-trace`) se documentan en el
README de cada herramienta y no producen entregables del proyecto. Desde I-3,
`tools/m13-core` no contiene la implementación: compila
`port/src/core/loop_core.c` con `-I port/include` y conserva sus 120 checks
como batería del núcleo que usa el frame loop.

## Distribución futura (planificación 2026-10-09 — NO implementado)

> Requisitos para el futuro instalador/validador/lanzador, cuando el diseño
> técnico esté decidido (Fase 3+). Nada de esto existe todavía.

- Distribuir solo mejoras y herramientas propias; jamás el exe original ni
  archivos protegidos (música, vídeos, texturas, niveles, recursos, ISOs,
  instaladores originales, DLL propietarias ni paquetes con recursos).
- No descargar, alojar ni distribuir automáticamente los originales en
  nombre del usuario: el futuro instalador pedirá la ubicación de su copia.
- Validación local (hashes/versión) con mensajes claros si falta algo o la
  versión es incompatible; sin subir archivos a servidores.
- Documentar requisitos de la copia original, instalación, copias de
  seguridad, restauración y desinstalación para otros usuarios.
- Separar en el diseño: código/herramientas/docs propios vs archivos del
  juego (el usuario los aporta; ver LICENSE.md §3).
