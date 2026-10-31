# `port/` - port nativo (esqueleto mínimo)

Código nuevo del port de Gex: Enter the Gecko (PC, EU v1.00.000). No contiene
ni redistribuye el ejecutable original ni assets del juego: solo implementación
propia escrita a partir de la evidencia documentada en `docs/`.

## Estado

Un componente implementado:

| Componente | Original | Cabecera | Estado |
| --- | --- | --- | --- |
| Lector del contenedor de audio `SLAG` | `0x0053899C` + carga `0x005388AC` | `gex/asset/sag.h` | funcional, 69 305 checks |

No hay motor, ni render, ni entrada, ni bucle de juego. `port/` es el sitio
donde esos componentes irán apareciendo, uno por uno, cada uno con su prueba.

## Estructura

```
port/
  include/gex/asset/sag.h   API pública (C11, sin dependencias)
  src/asset/sag.c           transcripción del parser y del cargador
  tests/test_sag.c          batería sintética + fuzz (sin el juego)
  tests/make_demo_sag.mjs   genera un contenedor sintético de 54 bytes
  tools/sagdump.c           CLI que muestra lo que el lector entendió
  build.ps1                 build en Windows (no hay make)
  Makefile                  equivalente POSIX
```

`build/` está ignorado por Git: solo contiene artefactos de compilación.

## Compilar y probar

Windows (PowerShell, sin `make` en este entorno):

```powershell
cd port
.\build.ps1 all      # compila, ejecuta los checks y la demo
.\build.ps1 test     # solo checks
.\build.ps1 demo     # solo la demo
.\build.ps1 clean    # borra build\ (solo lo que genera este script)
```

POSIX:

```bash
cd port
make test
make demo
```

El `Makefile` es el equivalente POSIX del script; **no se ha podido ejecutar en
este entorno** porque el Windows del mantenedor no tiene `make`. El build que
sí está verificado es `build.ps1` (clang y gcc).

Compilador: `clang` o `gcc` con `-std=c11`. Se puede forzar con
`.\build.ps1 test -Compiler gcc`.

## Verlo funcionar

```powershell
cd port
.\build.ps1 demo
```

Salida esperada (fixture sintético, no del juego):

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

`sagdump` acepta cualquier ruta: si tienes un `.sag` propio, pásalo como
argumento y muestra lo mismo. No se incluye ninguno.

## Reglas del código

- **Evidencia primero.** Cada función nueva cita la dirección del original en
  su comentario de cabecera. Sin dirección, no hay implementación.
- **Sin dependencias.** C11 y libc. Nada de DirectSound, Glide ni Win32 en las
  piezas puras: eso permite probarlas aquí y reutilizarlas después.
- **Las desviaciones se documentan.** Si el original confía en el contenedor y
  la implementación comprueba límites, se dice y se justifica en
  `docs/SAG_CONTAINER.md` §6.
- **Datos y código separados.** Ningún asset extraído del binario vive en
  `port/`. Los fixtures se generan por script.
- **Nada de placeholders presentados como implementación.** Si una pieza es
  simulación, se llama simulación.

## Documentación

- `docs/SAG_CONTAINER.md` - formato `SLAG`, algoritmo con direcciones,
  desviaciones, pruebas y límites.
- `docs/AUDIO_INIT_PATHS.md` - rutas de inicialización de DirectSound en el
  original.
- `docs/FUNCTION_MAP.md` - mapa estático de funciones y grafo de llamadas.
- `TESTING.md` - batería completa del repositorio.
