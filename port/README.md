# `port/` - port nativo (esqueleto mínimo)

Código nuevo del port de Gex: Enter the Gecko (PC, EU v1.00.000). No contiene
ni redistribuye el ejecutable original ni assets del juego: solo implementación
propia escrita a partir de la evidencia documentada en `docs/`.

## Estado

Tres subsistemas implementados y probados sin el juego; el último habla con el
dispositivo de sonido real:

| Componente | Original | Cabecera | Estado |
| --- | --- | --- | --- |
| Lector del contenedor de audio `SLAG` | `0x0053899C` + carga `0x005388AC` | `gex/asset/sag.h` | funcional, 69 305 checks |
| Decodificador IMA ADPCM de 4 bits | `0x00538285` + tablas `0x00558138`/`0x00558178` | `gex/audio/adpcm.h` | funcional, 60 052 checks |
| Bomba de streaming + reproductor | `0x005380B3`, `0x00538731` | `gex/audio/stream.h` | funcional, 215 checks |
| Planificador del bombeo (frame loop) | llamantes `0x00463205`, `0x0053A5FB`, `0x0053FCB7` | `gex/audio/sched.h` | funcional |
| Backend DirectSound (dispositivo) | `0x00538551`, `CreateSoundBuffer(0x8000, 0xE0)`, `GetCurrentPosition`, `0x0053886c` | `gex/audio/backend_dsound.h` | funcional (Windows), 50 checks |

No hay motor, ni render, ni entrada, ni bucle de juego. El audio sí suena:
`sagplay --device` entrega el PCM a un `IDirectSoundBuffer` real. Lo que **no**
está verificado es la audición humana (el agente no puede oír); la confirmación
por oído es del mantenedor. `port/` es el sitio donde los componentes irán
apareciendo, uno por uno, cada uno con su prueba.

## Estructura

```
port/
  include/gex/asset/sag.h            API pública del contenedor (C11, sin dependencias)
  include/gex/audio/adpcm.h          decodificador ADPCM + tablas inyectables
  include/gex/audio/stream.h         buffer, bomba, reproductor y backend
  include/gex/audio/sched.h          cuándo llamar a la bomba (una vez por frame)
  include/gex/audio/backend_dsound.h dispositivo DirectSound (sólo Windows)
  src/asset/sag.c                    transcripción del parser y del cargador
  src/audio/adpcm.c                  0x00538285
  src/audio/stream.c                 0x005380B3 + 0x00538731 + buffer secundario
  src/audio/sched.c                  el contrato de los llamantes de 0x005380B3
  src/audio/backend_dsound.c         IDirectSoundBuffer real (Lock/Play/cursor/volumen)
  tests/test_sag.c                   batería sintética + fuzz (sin el juego)
  tests/test_adpcm.c                 vector dorado, clamp, continuidad, fuzz
  tests/test_audio.c                 buffer, bomba, gate del cursor, backend, sched
  tests/test_device.c                dispositivo real (se omite si no hay tarjeta)
  tests/make_demo_sag.mjs            genera un contenedor sintético de 54 bytes
  tools/sagdump.c                    CLI que muestra lo que el lector entendió
  tools/sagplay.c                    demo: SLAG sintético -> PCM -> WAV o dispositivo
  build.ps1                          build en Windows (no hay make)
  Makefile                           equivalente POSIX (sin backend DirectSound)
```

`build/` está ignorado por Git: solo contiene artefactos de compilación.

## Compilar y probar

Windows (PowerShell, sin `make` en este entorno):

```powershell
cd port
.\build.ps1 all      # compila, ejecuta los checks y las demos
.\build.ps1 test     # solo checks (contenedor + ADPCM + ruta + dispositivo)
.\build.ps1 audio    # solo la demo de audio (escribe build\tone.wav)
.\build.ps1 demo     # fixture sintético + sagdump + demo de audio
.\build.ps1 clean    # borra build\ (solo lo que genera este script)
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
porque la demo usa `sin`. El backend DirectSound es sólo Windows: el `Makefile`
no lo compila y `sagplay --device` no existe en POSIX; `test_sag`,
`test_adpcm` y `test_audio` siguen siendo portables.

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

`sagdump` y `sagplay` aceptan cualquier ruta: si tienes un `.sag` propio,
pásalo como argumento (`sagplay fichero.sag salida.wav`, o
`sagplay --device fichero.sag`) y procesan ese. No se incluye ninguno.

## Reglas del código

- **Evidencia primero.** Cada función nueva cita la dirección del original en
  su comentario de cabecera. Sin dirección, no hay implementación.
- **Sin dependencias.** C11 y libc en las piezas puras: nada de Win32 en
  `adpcm`/`stream`/`sched`, lo que permite probarlas en cualquier plataforma y
  reutilizarlas después. La salida de audio es un *backend* con
  `submit`/`stop`/`cursor`/`pause`/`resume` que inyecta el llamante; el único
  archivo que ve Windows es `backend_dsound.c`, y usa la API del sistema
  (`dsound.h` + `-ldsound`), no una librería de terceros.
- **Las desviaciones se documentan.** Si el original confía en el contenedor y
  la implementación comprueba límites, se dice y se justifica en
  `docs/SAG_CONTAINER.md` §6 y en `docs/AUDIO_MIXER_RE.md` §11 (D1-D14).
- **Datos y código separados.** Ningún asset extraído del binario vive en
  `port/`. Los fixtures se generan por script.
- **Nada de placeholders presentados como implementación.** Si una pieza es
  simulación, se llama simulación. El audio llega a un dispositivo real y su
  cursor avanza a la frecuencia correcta, pero la audición humana no la verifica
  el agente: eso se dice tal cual y se deja la comprobación a `--device`.

## Documentación

- `docs/SAG_CONTAINER.md` - formato `SLAG`, algoritmo con direcciones,
  desviaciones, pruebas y límites.
- `docs/AUDIO_MIXER_RE.md` - decodificador ADPCM, bomba de streaming,
  reproductor, tablas, desviaciones D7-D14, backend DirectSound y planificador
  (§16) y demos.
- `docs/AUDIO_INIT_PATHS.md` - rutas de inicialización de DirectSound en el
  original.
- `docs/FUNCTION_MAP.md` - mapa estático de funciones y grafo de llamadas.
- `TESTING.md` - batería completa del repositorio.
