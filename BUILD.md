# BUILD — Construcción

> Estado: **parcial**. El port nativo tiene componentes con build y pruebas
> (`port/`, 2026-10-31 / 2026-11-01), incluido un backend de audio DirectSound
> que habla con el dispositivo real y un frame loop ejecutable con reloj
> inyectado. No hay build jugable del juego: el resto de la implementación
> (Olas, tras Hito P) sigue pendiente.

## Port nativo (`port/`) — funcional

Requisitos: un compilador C11 (`clang` o `gcc`) y Node ≥ 18 solo para generar
el fixture de demostración. Sin SDKs, sin assets del juego. En Windows el
backend de audio usa `dsound.h` + `-ldsound`, que vienen con el toolchain
(MinGW-w64/llvm-mingw): **no hay dependencias de terceros que descargar**.

```powershell
cd port
.\build.ps1 all      # compila, ejecuta 69 305 + 60 052 + 215 + 365 + 50 checks y las demos
.\build.ps1 test     # solo checks (contenedor + ADPCM + ruta de audio + frame loop + dispositivo)
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

Reproducir por el altavoz (Windows, necesita dispositivo de sonido):

```powershell
cd port
.\build\sagplay.exe --device            # tono sintético 440 Hz, 0,5 s
.\build\sagplay.exe --device mi.sag     # un contenedor propio, si lo hay
```

En POSIX, `make test` / `make audio` / `make demo` / `make clean` (mismo
contenido; **no ejecutado en este entorno** porque no hay `make` en este
Windows). `make audio` enlaza con `-lm` (la demo usa `sin`). El backend
DirectSound es sólo Windows: `Makefile` no lo compila y `sagplay --device` no
existe en POSIX; `test_sag`, `test_adpcm`, `test_audio`, `test_frame` y
`framedemo` siguen siendo portables (C11 puro). Desde I-3 el `Makefile`
también compila `src/core/loop_core.c` y `src/core/frame.c`; los targets se
añadieron en la misma revisión, pero **no se han podido ejecutar aquí**.
Compilador alternativo: `.\build.ps1 test -Compiler gcc`.

Qué produce:

| Target | Artefacto | Uso |
| --- | --- | --- |
| `test` | `build/test_sag.exe`, `build/test_adpcm.exe`, `build/test_audio.exe`, `build/test_frame.exe`, `build/test_device.exe` | baterías del lector, de la ruta de audio, del frame loop y del dispositivo (no se distribuyen) |
| `audio` | `build/sagplay.exe` + `build/tone.wav` | demo: `SLAG` sintético → PCM → WAV (o dispositivo con `--device`) |
| `demo` | `build/sagdump.exe` + `build/demo.sag` + `build/framedemo.exe` + `build/sagplay.exe` | CLI de inspección, fixture sintético, frame loop guionizado y demo de audio |

`build/` está ignorado por Git. Estos artefactos son de desarrollo: no son
entregables del proyecto y no contienen nada del juego. `build/tone.wav` se
genera en cada ejecución de `audio`/`demo`.

Entorno verificado (2026-10-31): `clang` 22.1.8 (`x86_64-w64-windows-gnu`),
`gcc` del mismo toolchain, Node 26.7.0. **No hay `make` en este Windows**, de
ahí `build.ps1`.

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
