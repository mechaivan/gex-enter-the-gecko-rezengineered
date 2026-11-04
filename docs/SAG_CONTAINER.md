# Contenedor de audio `SLAG` - formato y lector nativo

> **Nivel de evidencia:** estático. Todo lo marcado CONFIRMADO procede del
> desensamblado del ejecutable EU v1.00.000 (`objdump` sobre `.text`). Lo
> marcado HIPÓTESIS es inferencia explícita. Lo marcado UNKNOWN no está
> establecido.
>
> **Código nuevo:** `port/include/gex/asset/sag.h`, `port/src/asset/sag.c`,
> `port/tests/test_sag.c`, `port/tools/sagdump.c`. Este documento describe el
> original; el código es una transcripción, no una copia de binario.

## 1. Por qué este componente

Es el primer componente **implementado** del port. Cumple las condiciones que
se exigían para empezar:

- **Semántica completa por instrucción.** Las ~90 instrucciones de
  `0x0053899C` se leyeron enteras; no queda ningún bucle interno sin abrir.
  Cuando se escribió esto, el consumidor (`0x005380B3`) tenía dos bucles sin
  reconstruir (`0x00538285` y `0x00538409`); ambos están cerrados ahora
  (`docs/AUDIO_MIXER_RE.md` §4-§5).
- **Cero dependencias de plataforma.** No toca DirectSound, Glide, Win32 ni
  memoria global del motor: opera sobre un buffer.
- **Consumidor claro.** El parser llena un `WAVEFORMATEX` y devuelve el tamaño
  del payload comprimido; ese payload alimenta la bomba de audio
  (`docs/AUDIO_INIT_PATHS.md`).
- **Prueba sin el juego.** Un contenedor sintético basta para ejercitar todas
  las ramas.

No se eligió por facilidad: es una pieza real de carga de recursos, y el
formato que define condiciona el futuro subsistema de audio.

## 2. Mapa de funciones

| Dirección | Rol | Evidencia |
| --- | --- | --- |
| `0x0053899C` | parser del contenedor | CONFIRMADO, leído entero |
| `0x00538B01` | `read4()`: lee 4 bytes y los devuelve como dword | CONFIRMADO |
| `0x00538409` | `read(dst, n)`: fichero o buffer en memoria + cursor | CONFIRMADO |
| `0x00538A99` | `tell()`: posición actual | CONFIRMADO |
| `0x00538ACA` | `seek(abs)`: posición absoluta | CONFIRMADO |
| `0x005388AC` | carga de fichero completo a memoria | secuencia CONFIRMADO; identidad CRT HYPOTHESIS |
| `0x005387AD` | llamante: parser con `dest=0x57C348`, `size=0x12` | CONFIRMADO |
| `0x00538731` | llamante del anterior; solo comprueba `!= -1` | CONFIRMADO |

`0x005388AC` es además el que fija el modo del lector: guarda el buffer en
`[0x0057C368]`, el tamaño en `[0x0057C36C]` y pone el cursor
`[0x0057C370]` a 0. `0x00538409` comprueba primero `[0x0057C364]`: si no es
NULL lee del fichero, y si lo es usa el buffer en memoria.

## 3. Formato observado

```
offset 0   "SLAG"           (4 bytes)
offset 4   dword != 0xFFFFFFFF   -- "form size" (significado UNKNOWN)
offset 8   "WAVE"           (4 bytes)
offset 12  chunk*
```

Cada chunk:

```
+0   tag     (4 bytes, little-endian dword comparado contra 0x20746D66 = "fmt ")
+4   size    (dword, tamaño del payload; NO incluye la cabecera)
+8   payload (size bytes)
```

Chunks reconocidos:

- `"fmt "` (`0x20746D66`): el parser hace `memset(dest, 0, 18)`, copia
  `min(size, 18)` bytes y salta a `pos + size`. El destino es el
  `WAVEFORMATEX` de 18 bytes del llamante (`0x0057C348`). Si aparece más de
  una vez, **gana la última** antes de `data`.
- `"data"` (`0x61746164`): el parser **devuelve `size`** y deja el cursor al
  inicio del payload. No copia nada.

**Hallazgo posterior (2026-10-31):** el payload de `data` **no es PCM**. El
consumidor real (`0x00538731` → `0x005380B3` → `0x00538285`) lo decodifica como
**IMA ADPCM de 4 bits** (2 muestras por byte, nibble alto primero). El
`WAVEFORMATEX` del chunk `fmt ` describe la señal **decodificada** (mono, 16
bits), no el contenido del chunk. Detalle en `docs/AUDIO_MIXER_RE.md` §8.

Cualquier otro tag se salta con `seek(pos + size)`.

El layout de `WAVEFORMATEX` que el motor espera (18 bytes):

| Offset | Campo | Tipo |
| --- | --- | --- |
| 0 | `wFormatTag` | u16 |
| 2 | `nChannels` | u16 |
| 4 | `nSamplesPerSec` | u32 |
| 8 | `nAvgBytesPerSec` | u32 |
| 12 | `nBlockAlign` | u16 |
| 14 | `wBitsPerSample` | u16 |
| 16 | `cbSize` | u16 |

**UNKNOWN:** el significado exacto del dword de `offset 4` (el original solo
exige que no sea `0xFFFFFFFF`). Se expone como `form_size` sin interpretarlo.
**HIPÓTESIS:** es un tamaño tipo RIFF del cuerpo, porque el generador
sintético produce ese valor y el lector nunca lo usa.

## 4. Algoritmo del original (`0x0053899C`)

Transcripción literal del flujo:

```
53899c  read4() == "SLAG"?          no -> -1
5389b6  read4() == -1?              si -> -1
5389c8  read4() == "WAVE"?          no -> -1
5389dc  loop:
5389e9    tag = read4();   tag == -1 -> -1
5389ff    len = read4();   len == -1 -> -1
538a15    pos = tell()
538a1d    tag == "fmt "?
538a26      memset(dest, 0, dest_size)
538a38      n = min(len, dest_size)
538a5c      read(dest, n) == n?   no -> -1
538a70    tag == "data"?   si -> return len
538a7e    seek(pos + len)
538a8d    goto loop
538a92  (fin del bucle: return -1)
```

Detalles que importan y no son obvios:

1. **El retorno de `data` es el tamaño declarado**, no un offset. El cursor
   queda al inicio del payload, así que el llamante lee `size` bytes desde
   ahí. `0x00538731` solo comprueba `!= -1`.
2. **`read4()` no distingue EOF de dato.** `0x00538B01` hace
   `read(&tmp, 4)` y devuelve `-1` si el resultado no es 4; si es 4 devuelve
   el dword. Un dword legítimo `0xFFFFFFFF` es indistinguible de un error.
3. **`read(dst, 0)` puede fallar.** `0x00538409` en modo memoria devuelve `-1`
   si `cursor >= size` **antes** de mirar `n`, de modo que un `fmt ` de tamaño
   0 situado al final del fichero hace fallar al parser.
4. **No hay comprobación de límites.** `seek(pos + len)` es absoluto y el
   original confía en el contenedor: un `len` corrupto salta fuera del buffer
   o provoca un bucle infinito sobre sí mismo.
5. **El bucle es `while(1)`**, materializado por el compilador como
   `mov eax,1 / test eax,eax / je` en `0x5389dc`.
6. `0x005387AD` llama con `dest = 0x0057C348` y `dest_size = 0x12` (18), es
   decir, exactamente un `WAVEFORMATEX` sin cola.

## 5. Implementación nativa

`port/src/asset/sag.c` transcribe el flujo anterior sobre un buffer en
memoria. La API es pura:

```c
gex_sag_status gex_sag_parse(const void *data, size_t size, gex_sag *out);
gex_sag_status gex_sag_load(const char *path, unsigned char **data, size_t *size);
void           gex_sag_release(void *data);
const char    *gex_sag_status_name(gex_sag_status status);
```

- `gex_sag_parse` no reserva memoria, no hace I/O y nunca lee fuera de
  `[data, data + size)`.
- `gex_sag_load` reproduce la secuencia de `0x005388AC`: abrir, medir,
  rebobinar, reservar, leer, cerrar. La secuencia es CONFIRMADO; los nombres
  CRT concretos (`fopen`, `fseek`, `ftell`, `malloc`, `fread`, `fclose`) son
  HYPOTHESIS, como ya se anota en `docs/AUDIO_INIT_PATHS.md` §7.
- El resultado (`gex_sag`) lleva los bytes crudos del `fmt ` **y** el offset y
  tamaño del `data`, que es lo que el motor necesita para alimentar el buffer.

## 6. Desviaciones deliberadas

Se documentan porque son diferencias reales con el original, no mejoras
silenciosas:

| # | Original | Implementación | Motivo |
| --- | --- | --- | --- |
| D1 | `read4` devuelve `-1` tanto en lectura corta como en dword `0xFFFFFFFF` | `GEX_SAG_ERR_TRUNCATED` y `GEX_SAG_ERR_SENTINEL` separados | el código de error debe decir qué pasó; el comportamiento de rechazo es el mismo |
| D2 | confía en `len`; `seek(pos+len)` puede salir del buffer | toda lectura se valida con `size_t`; un chunk que excede el fichero da `TRUNCATED` | un fichero corrupto no debe leer memoria ajena |
| D3 | si `data` está truncado, el parser lo acepta y devuelve el tamaño declarado | se acepta igual, pero `data_complete = 0` lo declara | el motor fallaría al leer el payload; el fallo debe ser visible |
| D4 | `fmt ` de tamaño 0 al final del fichero -> `-1` | `GEX_SAG_ERR_NO_DATA` (mismo rechazo, otra etiqueta) | el motivo real es que no queda ningún `data` que leer |
| D5 | `min(len, 18)` con `int` | aritmética de 64 bits (`size_t`) | un `len` de `0xFFFFFFFE` no debe desbordar |
| D6 | no hay distinción entre "sin `fmt `" y "`fmt ` vacío" | `has_format` + `format_bytes` | el consumidor necesita saber si el formato es válido |

Lo que **no** se ha cambiado: reglas de aceptación, orden de comprobación de
la cabecera, `memset` a 18, `min(size,18)`, "gana el último `fmt `",
`data` devuelve el tamaño declarado y el cursor al payload, y el salto
`pos + size` para tags desconocidos.

## 7. Pruebas

`port/tests/test_sag.c` no usa framework ni el juego:

```
$ cd port
$ ./build.ps1 test          # Windows
$ make test                 # POSIX
69305 checks, 0 failures
```

Cubre 29 casos con nombre, entre ellos:

- contenedor válido (campos de `WAVEFORMATEX`, `data_offset`, `data_complete`);
- chunk desconocido antes de `fmt `;
- `fmt ` de 16, 18 y 20 bytes (recorte y relleno a cero);
- dos `fmt ` (gana el último) y `data` antes de `fmt ` (`has_format = 0`);
- `data` de tamaño 0 y `data` truncado (`data_complete = 0`);
- `SLAG`/`WAVE` incorrectos, tamaño de formulario `0xFFFFFFFF`, cabecera
  corta, cabecera de chunk corta, payload de `fmt ` corto;
- fichero sin `data`, chunk que excede el fichero, `size` de `0xFFFFFFFE`;
- `fmt ` de tamaño 0 en medio y al final;
- validación de argumentos, nombres de estado únicos;
- carga real de un fichero temporal y errores de carga (inexistente, vacío);
- 20 000 buffers aleatorios y 20 000 mutaciones del contenedor válido, con
  invariantes (`data_offset <= size`, `format_bytes <= 18`, coherencia entre
  `has_format` y `format_bytes`).

Resultado del fuzz: **0 buffers aleatorios aceptados** de 20 000; ninguna
lectura fuera de rango ni campo incoherente en las mutaciones.

Compilado sin avisos con `clang` 22.1.8 y `gcc` (`-std=c11 -O2 -Wall -Wextra
-Wpedantic`).

## 8. Demostración

```
$ cd port
$ ./build.ps1 demo
demo.sag: 54 bytes (synthetic)
file:   build/demo.sag
size:   54 bytes
form:   SLAG size=0x0000002E (46)
format: chunk=18 copied=18
        wFormatTag=0x0001 nChannels=1 nSamplesPerSec=22050
        nAvgBytesPerSec=22050 nBlockAlign=1 wBitsPerSample=8 cbSize=0
data:   offset=46 size=8 complete
```

El fixture lo genera `port/tests/make_demo_sag.mjs`: 8 bytes de silencio
8 bits a 22050 Hz mono. Es sintético, no procede del juego.

## 9. Límites

- **No hay ficheros `.sag` reales disponibles.** Ni en el repositorio ni en
  disco (se comprobó el árbol de la build mod: solo `GEX3D.EXE`). La
  validación es contra fixtures construidos a partir del formato; un fichero
  real podría revelar chunks adicionales o un uso distinto de `form_size`.
- **No se ha ejecutado el parser original** sobre los mismos bytes: no hay
  imagen de referencia para instrumentar. La equivalencia es estática
  (transcripción instrucción a instrucción), no dinámica.
- **La reproducción de audio ya está implementada, pero no verificada contra un
  `.sag` real.** `port/src/audio/` contiene el decodificador IMA ADPCM
  (`0x00538285`), la bomba de streaming (`0x005380B3`) y el reproductor
  (`0x00538731`) sobre un *backend* inyectable; ver `docs/AUDIO_MIXER_RE.md`.
  Lo que sigue sin validar es que un fichero real del juego se decodifique como
  se espera: no hay ningún `.sag` en disco.
- **`form_size` se expone sin interpretar.** Si algún día se demuestra su
  significado, hay que añadir la comprobación correspondiente.

## 10. Cómo reproducir

```powershell
cd port
.\build.ps1 all                 # compila, ejecuta checks y demo
.\build.ps1 test                # solo checks
node tests\make_demo_sag.mjs build\demo.sag
.\build\sagdump.exe build\demo.sag
```

```bash
cd port
make test && make demo
```

## 11. Siguiente paso

El consumidor natural de este lector es el camino de carga de sonido:
`0x005387AD` -> `0x00538731` -> `0x0053899C`, y desde ahi el buffer de audio.
Ese camino ya esta implementado en `port/src/audio/` (ver
`docs/AUDIO_MIXER_RE.md`): el bucle `0x00538285` resulto ser un decodificador
IMA ADPCM de 4 bits, no un mezclador.

Lo que queda de este componente es de validacion, no de implementacion:
conseguir un `.sag` real (o extraerlo de los datos del juego) y comprobar que el
contenedor no tiene chunks adicionales ni un `form_size` con significado.
