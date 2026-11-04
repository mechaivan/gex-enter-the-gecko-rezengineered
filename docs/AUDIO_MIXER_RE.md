# Ruta de audio: decodificador IMA ADPCM, bomba de streaming y reproductor

> **Nivel de evidencia:** estático sobre el desensamblado de la referencia EU
> (`objdump` de `.text`), más una implementación nativa probada sin hardware y,
> desde I-2b, un backend DirectSound verificado en el dispositivo real (§16).
> Lo marcado CONFIRMADO está en las instrucciones citadas; lo marcado HIPÓTESIS
> es inferencia explícita; lo marcado UNKNOWN no está establecido.
>
> **Código nuevo:** `port/include/gex/audio/adpcm.h`,
> `port/include/gex/audio/stream.h`, `port/include/gex/audio/sched.h`,
> `port/include/gex/audio/backend_dsound.h`, `port/src/audio/adpcm.c`,
> `port/src/audio/stream.c`, `port/src/audio/sched.c`,
> `port/src/audio/backend_dsound.c`, `port/tests/test_adpcm.c`,
> `port/tests/test_audio.c`, `port/tests/test_device.c`,
> `port/tools/sagplay.c`.
>
> **Documentos hermanos:** `docs/SAG_CONTAINER.md` (contenedor y desviaciones
> D1-D6), `docs/AUDIO_INIT_PATHS.md` (rutas de inicialización de DirectSound).

## 1. Resumen ejecutivo

Lo que la documentación previa llamaba «mezclador `0x005380B3`» son en realidad
**tres piezas distintas** y ninguna de ellas mezcla voces:

| Dirección | Qué es de verdad | Evidencia |
| --- | --- | --- |
| `0x00538285` | decodificador **IMA/DVI ADPCM de 4 bits** (2 muestras por byte) | CONFIRMADO, 388 B leídos enteros |
| `0x005380B3` | **bomba de streaming**: Lock 0x4000 → leer 0x1000 → decodificar → Unlock | CONFIRMADO, 466 B leídos enteros |
| `0x00538731` | **reproductor**: cargar `.sag`, crear buffer, prefill, Play(looping), volumen | CONFIRMADO, 379 B leídos enteros |

No hay suma de varias voces en esta ruta: el juego usa **un único**
`IDirectSoundBuffer` global en `[0x57C374]` de 32 KiB (`0x8000`), dividido en dos
mitades de 16 KiB (`0x4000`) que se rellenan alternativamente. La
«mezcla» es, literalmente, la decodificación de un flujo comprimido hacia una de
las dos mitades.

Consecuencia para el port: la ruta completa **se puede reproducir sin
DirectSound**, sustituyendo el buffer secundario por memoria propia y la salida
por un *backend* inyectable. Eso es lo que implementa `port/src/audio/`. Desde
I-2b hay además un *backend* DirectSound real (§16), que es el que hace sonar la
ruta en un altavoz.

## 2. Evidencia y método

- Desensamblado completo de la referencia (1 557 504 B, MD5
  `692b12825003417cc4a5a9d4db13eebe`), base de imagen `0x00400000`.
- Mapa estático `analysis/EU/{functions.tsv,calls.tsv}` para llamantes y
  callees; los nombres de import vienen de `functions.tsv`
  (p. ej. `DSOUND.dll!DirectSoundCreate` en `0x005404F0`).
- Para los **valores de las tablas** no hay más imagen que la build mod presente
  (1 559 040 B, MD5 `3198350EB398DB9A64771842D7781B4B`), con `.data` desplazada.
  El método de recuperación y su confianza están en §9.
- No hay ninguna ejecución instrumentada de la referencia: todo el
  comportamiento dinámico de este documento es **predicción**, no medida.

## 3. Mapa de funciones y globales

### 3.1 Funciones

| Dirección | Rol | Notas |
| --- | --- | --- |
| `0x00538551` | crea el dispositivo DirectSound y el buffer primario | 467 B; §7 |
| `0x00538731` | reproductor de un `.sag` | 379 B; §6 |
| `0x005388AC` | carga el fichero (o lo deja en disco y lo lee por partes) | 240 B |
| `0x0053899C` | parser `SLAG` | 357 B; ver `docs/SAG_CONTAINER.md` |
| `0x005380B3` | bomba de streaming | 466 B; §5 |
| `0x00538285` | decodificador ADPCM | 388 B; §4 |
| `0x00538409` | `read(dst, n)` del fichero o del buffer en memoria | 153 B |
| `0x005384A3` | cierra y libera la fuente | 74 B |
| `0x005384ED` | «¿queda fuente?» | 45 B |
| `0x00538040` | `Restore()` si el buffer está perdido | 40 B |
| `0x00538068` | `GetStatus() & DSBSTATUS_BUFFERLOST` | 14 B |
| `0x00538075` | `GetStatus()` (0 si no hay buffer) | 49 B |
| `0x005380A6` | `GetStatus() & DSBSTATUS_PLAYING` | 13 B |
| `0x0053851A` | `Stop()` si hay buffer y está sonando | 55 B |
| `0x00538724` | fija el global de volumen | 13 B |

### 3.2 Globales

| Dirección | Tipo | Uso | Escritores |
| --- | --- | --- | --- |
| `0x0057C35C` | i32 | muestra anterior (predictor ADPCM) | `0x5383F7`, `0x538824` |
| `0x0057C360` | i32 | índice de paso ADPCM (0..88) | `0x5383FF`, `0x53881A` |
| `0x0057C374` | ptr | `IDirectSoundBuffer*` secundario | `0x53880B`, `0x538779` |
| `0x0057C378` | ptr | `IDirectSoundBuffer*` primario | `0x5385FE`, `0x5386F4` |
| `0x0057C37C` | u32 | mitad a rellenar: 0 o `0x4000` | `0x538189`, `0x53823A`, `0x53882E` |
| `0x0057C380` | i32 | ya se emitió la mitad de silencio | `0x53821A`, `0x538838` |
| `0x0057C384` | ptr | `IDirectSound*` | `0x53856D`, `0x5385AA` |
| `0x0057C364` | ptr | `FILE*` de la fuente | `0x5388C5`, `0x5384BD` |
| `0x0057C368` | ptr | fuente cargada en memoria | `0x53892C`, `0x5384E1` |
| `0x0057C36C` | u32 | tamaño de esa fuente | `0x538906` |
| `0x0057C370` | u32 | cursor de lectura | `0x538491` |
| `0x0057C330` | `DSBCAPS` | descriptor reutilizado por las dos creaciones | `0x5385BC`, `0x5387C9` |
| `0x0057C348` | `WAVEFORMATEX` | formato del `.sag` (18 B) | `0x53899C` |
| `0x0057C340` | ptr | recibe `0x57C348` antes de crear el secundario | `0x5387C9` |
| `0x005582E0` | `WAVEFORMATEX` | formato del buffer primario | sólo lectura |
| `0x005582F4` | i32 | volumen de la ruta (100 por defecto en la mod) | `0x00538724` |
| `0x00558138` | i32[16] | tabla de ajuste de índice | sólo lectura |
| `0x00558178` | i32[89] | tabla de tamaños de paso | sólo lectura |

## 4. `0x00538285` — decodificador IMA ADPCM

### 4.1 Firma y convención

```c
void decode(const uint8_t *src, int16_t *dst, int samples);  /* cdecl, 3 args */
```

- `[ebp+0x8]` → `src` (se guarda en `[ebp-0x4]`),
- `[ebp+0xc]` → `dst` (en `[ebp-0x8]`),
- `[ebp+0x10]` → número de **muestras** (se decrementa por iteración).
- Sin valor de retorno: el estado vive en `[0x57C35C]` y `[0x57C360]`, que
  escribe al salir (`0x5383F7`, `0x5383FF`).
- No comprueba nada: ni punteros, ni longitud, ni formato.

El llamante (`0x005380B3`) invoca con `samples = n*2`, donde `n` es el número de
bytes comprimidos leídos. Es decir, **2 muestras por byte**, sin cabecera de
bloque: el flujo ADPCM del `.sag` es continuo de principio a fin.

### 4.2 Bucle, instrucción a instrucción

```
538285  prólogo; [ebp-4]=src, [ebp-8]=dst, [ebp+0x10]=samples
538297  history    = [0x57C35C]
5382A0  index      = [0x57C360]
5382AB  step_size  = step[index]              ; [ebp-0x28]
5382B5  half       = 0                        ; [ebp-0x18]
5382BE  samples--; if (samples <= 0) goto 5383F4
5382D1  if (half) nibble = byte & 0x0F        ; segunda muestra del byte
5382E2  else { byte = (int8)*src++; nibble = (byte >> 4) & 0x0F }
538300  half = (half == 0)                    ; conmuta 0 <-> 1
538312  index += adjust[nibble]
53831C  if (index < 0)  index = 0
538329  if (index > 88) index = 88            ; 0x58
538336  sign  = nibble & 8                    ; [ebp-0x24]
53833F  nibble &= 7
538348  delta  = step_size >> 3
538354  if (nibble & 4) delta += step_size
538367  if (nibble & 2) delta += step_size >> 1
53837E  if (nibble & 1) delta += step_size >> 2
538393  if (sign) history -= delta; else history += delta
5383AD  if (history >  32767) history =  32767
5383BF  if (history < -32768) history = -32768
5383CF  step_size = step[index]               ; para la muestra siguiente
5383DC  *(int16*)dst++ = (int16)history
5383EF  goto 5382BE
5383F4  [0x57C35C] = history; [0x57C360] = index; ret
```

Tres detalles que no son obvios y que la implementación reproduce:

1. **El índice se ajusta antes de calcular el delta, pero el delta usa el paso
   del índice anterior.** `step_size` se carga al entrar en la iteración
   (`0x5382AB` la primera vez, `0x5383CF` las siguientes) y no se recalcula
   tras `index += adjust[nibble]`.
2. **El clamp del índice es `[0, 88]`**, no `[0, 88)` ni `[0, 15]`. Con la
   tabla de 89 entradas, 88 es la última.
3. **La primera muestra de cada byte es el nibble alto** (`0x5382E2`), la
   segunda el bajo (`0x5382D7`). El estado `half` empieza en 0 y se conmuta
   después de elegir la muestra (`0x538300`).

### 4.3 Pseudocódigo equivalente

```
step_size = step[index]
por cada muestra:
    nibble = (half ? byte & 15 : byte >> 4)
    index  = clamp(index + adjust[nibble], 0, 88)
    delta  = step_size>>3 + (n&4 ? step_size : 0)
                         + (n&2 ? step_size>>1 : 0)
                         + (n&1 ? step_size>>2 : 0)
    history += (n&8) ? -delta : delta
    history  = clamp(history, -32768, 32767)
    step_size = step[index]
    emit history
```

Esto es exactamente el decodificador **IMA ADPCM** (DVI) de 4 bits, con dos
diferencias respecto a la forma habitual: no hay cabecera por bloque (no se
redefine el predictor cada N bytes) y el estado **persiste entre llamadas**, de
modo que el flujo se decodifica como una única secuencia.

### 4.4 Vector dorado

Con las tablas de §9, los bytes `07 F0` producen:

| # | nibble | `step_size` | índice nuevo | delta | history |
| --- | --- | --- | --- | --- | --- |
| 1 | `0` (alto de `07`) | 7 | 0 | 0 | 0 |
| 2 | `7` (bajo de `07`) | 7 | 8 | 11 | 11 |
| 3 | `F` (alto de `F0`) | 16 | 16 | 30 | −19 |
| 4 | `0` (bajo de `F0`) | 34 | 15 | 4 | −15 |

Estado final: `history = −15`, `index = 15`. Es el caso
`test_golden_vector` de `port/tests/test_adpcm.c`, calculado a mano sobre las
instrucciones de arriba, no sobre la implementación.

## 5. `0x005380B3` — bomba de streaming

### 5.1 Firma

```c
void pump(void);   /* sin argumentos, sin retorno */
```

Todo el estado está en globales. La llama el bucle del juego
(`0x00463205`, `0x0053A5FB`, `0x0053FCB7`) y el propio reproductor antes del
`Play` (`0x00538842`).

### 5.2 Flujo

```
5380B6  __chkstk(0x4024)                  ; 16 KiB de buffer temporal en pila
5380C0  if (![0x57C384]) return           ; sin dispositivo
5380CE  if (![0x57C374]) return           ; sin buffer secundario
5380DC  Restore() si BUFFERLOST           ; 0x00538040
5380E1  if (playing) {                    ; 0x005380A6: GetStatus & 1
5380EA      GetCurrentPosition(&play, &write)          ; vtable+0x10
538108      if ([0x57C37C] == 0) { if (play <  0x4000) return; }
538121      else                { if (play >  0x4000) return; }
        }
53812F  Lock(0x4000, &ptr1, &bytes1, &ptr2, &bytes2, 0)  ; vtable+0x2C
53815D  if (resultado) return
538166  if (!source_open()) goto 5381FE   ; 0x005384ED
538173  [0x57C37C] ^= 0x4000              ; conmuta la mitad a escribir
53819D  n = read(chunk, 0x1000)           ; 0x00538409, buffer de pila
5381B4  decode(chunk, ptr1, n*2)          ; 0x00538285
5381CD  if (n != 0x1000) {
5381D6      memset(ptr1 + n*4, 0, 0x4000 - n*4)
5381F7      close_source()                ; 0x005384A3
        }
5381FC  goto 538254
5381FE  if ([0x57C380] == 0) {
538207      memset(ptr1, 0, 0x4000)       ; media mitad de silencio
53821A      [0x57C380] = 1
538224      [0x57C37C] ^= 0x4000
        } else {
538242      Stop()                        ; vtable+0x48
        }
538254  Unlock(ptr1, bytes1, ptr2, bytes2) ; vtable+0x4C
```

### 5.3 El *gate* del cursor

`0x005380B3` **no** escribe en cuanto se le llama: cuando el buffer está
reproduciéndose, espera a que el cursor de reproducción haya salido de la mitad
que va a sobrescribir.

- `write_offset == 0` (toca la mitad `0x0000`): exige `play_cursor >= 0x4000`.
- `write_offset == 0x4000` (toca la mitad `0x4000`): exige `play_cursor <= 0x4000`.

Con `DSBPLAY_LOOPING` el cursor da vueltas a `0x8000`, así que la condición
«ha llegado a la otra mitad» se cumple una vez por vuelta. El prefill del
reproductor no pasa por el *gate* porque el buffer aún no está en `PLAYING`
(`0x005380A6` devuelve 0).

En el port esto se modela con un callback opcional
`gex_audio_backend.cursor`; sin él, el *sink* se considera siempre al día.

### 5.4 Las cuatro ramas

| Rama | Condición | Efecto |
| --- | --- | --- |
| datos | `source_open()` | lee hasta `0x1000`, decodifica `n*2` muestras en la mitad |
| cola corta | `n != 0x1000` | rellena de ceros el resto de la mitad y **cierra la fuente** |
| silencio | fuente cerrada y `[0x57C380] == 0` | media mitad a cero y marca la bandera |
| parada | fuente cerrada y `[0x57C380] != 0` | `Stop()`; sigue emitiendo `Stop()` en cada llamada |

Nótese que la rama de parada **también hace `Unlock`** (`0x0053827E`) de una
mitad que no se ha rellenado. El original la desbloquea después de `Stop`, así
que el contenido obsoleto no llega a sonar; el port libera el bloqueo sin
entregar muestras al *backend* (desviación D11).

### 5.5 El bug latente del múltiplo exacto

`0x00538409` devuelve `-1` si el cursor ya está al final **antes** de mirar
cuántos bytes se piden. `0x005380B3` no comprueba ese `-1`: pasa `n*2` como
número de muestras (`0x5381B7`) y luego calcula `ptr1 + n*4` y
`0x4000 - n*4` (`0x5381D6`).

Con `n == -1` eso es:

- `decode(chunk, ptr1, 0xFFFFFFFE)`: unos 4 294 967 294 nibbles escritos en la
  mitad de 16 KiB;
- `memset(ptr1 - 4, 0, 0x4004)`: 4 bytes **antes** de la mitad bloqueada.

Es decir, un `.sag` cuyo payload sea múltiplo exacto de `0x1000` corrompe la
memoria del juego. No se ha observado en ejecución (no hay `.sag` reales), pero
se deduce de las instrucciones y es la razón por la que el port **no** lo
replica: trata `n == 0` como fin de fuente (desviación D8).

## 6. `0x00538731` — reproductor

### 6.1 Firma

```c
int play(int hwnd, const char *path, int load_to_memory);  /* 0 ó -1 */
```

Llamada documentada en `docs/AUDIO_INIT_PATHS.md` §3.2:
`0x00538731(0x537E9C(1), rutaB, 1)`.

### 6.2 Secuencia

| Paso | Dirección | Instrucción |
| --- | --- | --- |
| asegura dispositivo | `0x538735-0x538744` | `0x00538551(hwnd)`; si `-1` → `-1` |
| libera el buffer anterior | `0x53874E-0x538779` | `Stop(+0x48)`, `Release(+0x8)`, `[0x57C374]=0` |
| libera la fuente anterior | `0x538783-0x53878C` | `0x005384ED` / `0x005384A3` |
| carga | `0x538791-0x5387A3` | `0x005388AC(path, load_to_memory)` |
| parsea | `0x5387AD-0x5387BC` | `0x0053899C(&fmt, 18)`; si `-1` → `-1` |
| descriptor | `0x5387C9-0x5387E7` | `[0x57C340]=&fmt`, `dwSize=0x14`, `dwFlags=0xE0`, `dwBufferBytes=0x8000` |
| crea el secundario | `0x5387F1-0x538815` | `CreateSoundBuffer(+0xC, &desc, &[0x57C374], NULL)` |
| resetea estado | `0x53881A-0x538838` | `[0x57C360]=[0x57C35C]=[0x57C37C]=[0x57C380]=0` |
| prefill | `0x538842` | `pump()` (`0x005380B3`) |
| `Restore` si perdido | `0x538847` | `0x00538040` |
| arranca | `0x53884C-0x538865` | `Play(+0x30, 0, 0, 1)` = `DSBPLAY_LOOPING` |
| volumen | `0x53886C-0x5388A6` | `SetVolume(+0x3C, [0x5582F4]*30 - 3000)` |

`dwFlags = 0xE0` = `DSBCAPS_CTRLVOLUME (0x20) | DSBCAPS_CTRLPAN (0x40) |
DSBCAPS_CTRLFREQUENCY (0x80)`. `dwBufferBytes = 0x8000`.

### 6.3 El volumen y su «clamp» muerto

`0x53886C` calcula `v = [0x5582F4] * 30 - 3000`. Después hay dos comparaciones
(`v > 0`, `v < -10000`) cuyos cuerpos son **autoasignaciones** (`mov [ebp-4],
[ebp-4]` en `0x53888B` y en `0x538891`): el clamp no hace nada. Con
`[0x5582F4] = 100` (valor por defecto en la build mod, `+0x170`) sale `0`, que
es volumen máximo; con más de 100 el valor se sale del rango válido de
`IDirectSoundBuffer::SetVolume` (`-10000..0`).

- CONFIRMADO: la fórmula, el destino (`+0x3C`) y que el clamp es inerte.
- HIPÓTESIS: `[0x5582F4]` es un volumen en `0..100`. Lo escribe
  `0x00538724(valor)` y `0x0053FF0D` le pasa `valor/0x7F`, lo que sugiere
  `0..127`; con 127 el resultado sería `810`, fuera de rango. No se puede
  decidir con lo disponible.

## 7. `0x00538551` — dispositivo y buffer primario (contexto)

Ya descrito en `docs/AUDIO_INIT_PATHS.md` §4.1. Dos datos que ahora se pueden
leer y que afectan al audio:

- El formato del primario, `0x5582E0`, es en la build mod
  `{wFormatTag=1, nChannels=1, nSamplesPerSec=11025, nAvgBytesPerSec=22050,
  nBlockAlign=2, wBitsPerSample=16, cbSize=0}` (leído en `0x558450`, `+0x170`).
  HIPÓTESIS para la referencia: el desplazamiento está verificado en tres
  anclas, pero el contenido de la referencia no se ha leído.
- El secundario se crea con el `WAVEFORMATEX` **del propio `.sag`**
  (`0x57C348`), no con el del primario. Por eso la frecuencia de reproducción
  la decide el fichero.

## 8. Relación con `SLAG`

```
0x00538731(hwnd, "…\\x.sag", 1)
  ├─ 0x005388AC   carga el fichero entero
  ├─ 0x0053899C   parsea: "SLAG" / dword / "WAVE" / chunks
  │     ├─ "fmt " → 18 bytes en 0x57C348 (WAVEFORMATEX)
  │     └─ "data" → devuelve el tamaño declarado, cursor al payload
  ├─ CreateSoundBuffer(0x8000, 0xE0, fmt)
  └─ pump()  →  Lock 0x4000 → read 0x1000 → decode → Unlock
```

Consecuencias verificables:

1. El payload del `.sag` **no** es PCM: son nibbles IMA ADPCM, 2 muestras por
   byte. El `WAVEFORMATEX` describe la señal **decodificada** (16 bits), no el
   payload.
2. El tamaño del payload no necesita ser múltiplo de nada: la bomba lee
   `0x1000` y la última lectura puede ser corta. (Y si es múltiplo exacto,
   aparece el bug de §5.5.)
3. `nBlockAlign = 1` en el fixture de `sagdump` es coherente con «1 byte
   comprimido», pero el motor no lo usa para nada: `0x00538285` ignora el
   formato.

## 9. Las tablas ADPCM y su recuperación

### 9.1 Qué se busca

`0x00538285` indexa dos tablas de `int32`:

- `adjust[16]` en `[4*nibble + 0x558138]` (`0x538312`),
- `step[89]` en `[4*index + 0x558178]` (`0x5382AB`, `0x5383CF`).

`0x558138 + 64 = 0x558178`: son **contiguas**, 16 dwords de ajuste seguidas de
89 dwords de paso.

### 9.2 El problema

Los valores están en `.data`, que `objdump` no vuelca y que no se puede leer sin
la imagen. La referencia no está en la máquina (§2).

### 9.3 El método

La única imagen disponible es la build mod. Su `.data` está desplazada
**+0x170** respecto a la referencia. El desplazamiento no se supone: se verifica
en tres puntos independientes, comparando la dirección que cita el
desensamblado con la que ocupa el mismo dato en la mod:

| Referencia | Mod | Diferencia | Cómo se verificó |
| --- | --- | --- | --- |
| `0x558138` (adjust) | `0x5582A8` | +0x170 | 16 dwords con la forma esperada |
| `0x558178` (step) | `0x5582E8` | +0x170 | 89 dwords `7 … 32767` |
| `0x5582E0` (fmt primario) | `0x558450` | +0x170 | `WAVEFORMATEX` coherente |

Y hay una comprobación independiente: la tabla de 89 dwords aparece **tres
veces** en el `.data` de la mod (offsets de fichero `0x156EE8`, `0x1570C8`,
`0x1572B0`), y **cada copia va precedida por los mismos 16 dwords** de ajuste.
Las tres copias coinciden entre sí.

### 9.4 Valores y confianza

```c
adjust[16] = {-1,-1,-1,-1, 2,4,6,8, -1,-1,-1,-1, 2,4,6,8}
step[89]   = {7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
              50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,
              253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,
              1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,
              3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
              10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,
              27086,29794,32767}
```

Estas son, exactamente, las tablas canónicas de **IMA ADPCM** (89 pasos y 16
ajustes de índice), lo que **corrobora** los valores de forma independiente: no
son números arbitrarios, son la tabla estándar del códec que el algoritmo
reproduce instrucción a instrucción.

- CONFIRMADO: el algoritmo es IMA ADPCM y las tablas son las canónicas.
- CONFIRMADO (para la mod): los valores están donde dice §9.3.
- HIPÓTESIS: que la referencia tenga **los mismos valores**. Falta leer
  `.data` de la referencia en `0x558138` (64 B) y `0x558178` (356 B).
  Es una hipótesis fuerte —el códec no funciona con otras tablas y las tres
  copias de la mod coinciden— pero es una hipótesis.

El decodificador **no** lleva las tablas incrustadas: recibe un
`const gex_adpcm_tables *`. `gex_adpcm_recovered_tables()` devuelve las de
arriba, y el test las sustituye por unas planas para probar que el algoritmo no
depende de ellas.

## 10. Implementación en `port/`

### 10.1 Módulos

```
port/include/gex/audio/adpcm.h         gex_adpcm_state, gex_adpcm_tables, decode
port/include/gex/audio/stream.h        gex_audio_buffer, gex_audio_stream, backend
port/include/gex/audio/sched.h         gex_audio_sched (cuándo llamar a la bomba)
port/include/gex/audio/backend_dsound.h  gex_audio_dsound (dispositivo, sólo Windows)
port/src/audio/adpcm.c                 0x00538285
port/src/audio/stream.c                0x005380B3 + 0x00538731 + buffer
port/src/audio/sched.c                 los llamantes de 0x005380B3 (§16.3)
port/src/audio/backend_dsound.c        IDirectSoundBuffer real (§16)
port/tests/test_adpcm.c                vector dorado, clamp, continuidad, fuzz
port/tests/test_audio.c                buffer, bomba, reproductor, backend, sched
port/tests/test_device.c               dispositivo real (se omite sin tarjeta)
port/tools/sagplay.c                   demo: SLAG sintético → PCM → WAV o dispositivo
```

### 10.2 El buffer (`gex_audio_buffer`)

Sustituye al `IDirectSoundBuffer` secundario:

| Original | Port |
| --- | --- |
| `CreateSoundBuffer(0x8000, 0xE0, fmt)` | `gex_audio_buffer_init(&b, backend, rate)` |
| `Lock(0x4000, &ptr1, …)` | `gex_audio_buffer_lock(&b, 0x4000)` |
| `Unlock(ptr1, …)` | `gex_audio_buffer_unlock(&b, 0x4000)` |
| `[0x57C37C] ^= 0x4000` | `gex_audio_buffer_flip(&b)` |
| `Play(0,0,1)` | `gex_audio_buffer_play(&b)` |
| `Stop()` | `gex_audio_buffer_stop(&b)` |
| `GetStatus() & 1` | `buffer.playing` |

`Lock` guarda el offset bloqueado (`lock_offset`) porque el original conmuta
`0x57C37C` **después** de `Lock` y **antes** de `Unlock`; sin ese detalle se
entrega la mitad equivocada. Es un fallo real que apareció al comparar la salida
de la bomba con una decodificación de una sola pasada (§12).

### 10.3 La bomba (`gex_audio_stream_pump`)

Reproduce §5.2 con estas equivalencias:

- el buffer temporal de 16 KiB de la pila desaparece: se decodifica
  directamente desde el payload a la mitad bloqueada;
- `read` se sustituye por el recorte `payload_size - cursor`;
- `n == -1` se convierte en «fuente agotada» (D8);
- el *gate* del cursor se aplica sólo si el backend expone `cursor`.

### 10.4 El *backend*

```c
typedef struct gex_audio_backend {
    void *ctx;
    int  (*submit)(void *ctx, const int16_t *samples, size_t count, uint32_t rate);
    void  (*stop)(void *ctx);
    uint32_t (*cursor)(void *ctx);
    void  (*pause)(void *ctx);   /* opcional: congela el cursor */
    void  (*resume)(void *ctx);  /* opcional: sigue desde ahí */
} gex_audio_backend;
```

- `submit` recibe una mitad decodificada (8 192 muestras mono de 16 bits).
- `stop` corresponde al `Stop()` de la rama de parada.
- `cursor` es opcional; sin él no hay *gate*.
- `pause`/`resume` son opcionales y sólo tienen sentido en un dispositivo:
  pausar congela el cursor donde está (el buffer del original conserva su
  posición entre `Stop`/`Play`, así que el *gate* sigue siendo válido) y
  reanudar continúa desde ahí. **No** equivalen a `stop`: `stop` termina la
  reproducción, `pause` la suspende. Los llama el planificador, nunca la bomba.
- Todos pueden ser `NULL`: el flujo se decodifica y las muestras se descartan
  sin fallar.

Hay dos implementaciones: la captura en memoria de las pruebas y el WAV de la
demo (ambas sin hardware, sin `cursor`), y el dispositivo DirectSound real
(`backend_dsound.c`, sólo Windows), que es el objeto que el original maneja.
Nada del decodificador cambia entre ellos. Detalle en §16.

## 11. Desviaciones deliberadas (D7-D14)

| # | Original | Port | Motivo |
| --- | --- | --- | --- |
| D7 | decodifica `samples` sin mirar cuántos bytes quedan | `gex_adpcm_decode` se detiene en `src_bytes` | evita leer fuera del payload; el flujo válido no cambia |
| D8 | `n == -1` → `decode(…, 0xFFFFFFFE)` + `memset(ptr-4, 0, 0x4004)` | `n == 0` → fin de fuente, media mitad a cero | el original corrompe memoria; no se replica |
| D9 | no valida `WAVEFORMATEX` | rechaza si no es mono/16 bits/rate>0 | el decodificador siempre produce mono 16 bits; otro formato sería basura |
| D10 | un payload declarado mayor que el fichero se lee hasta donde haya | igual, pero con `payload_truncated = 1` | el fallo debe ser visible, no silencioso |
| D11 | `Unlock` tras `Stop` de una mitad sin rellenar | se libera el bloqueo sin entregar muestras | el original ya ha parado el buffer; entregar datos obsoletos al *sink* sería un artefacto del port |
| D12 | `DSSCL_PRIORITY` + ventana del juego | `DSSCL_NORMAL` + ventana de escritorio (`NULL` → `GetDesktopWindow()`) | la demo no tiene ventana de juego; DirectSound convierte el formato del secundario al del primario por sí mismo. El resto de la cadena (`DirectSoundCreate` → `SetCooperativeLevel` → `CreateSoundBuffer`) es la misma y en el mismo orden |
| D13 | `dwFlags = 0xE0` (`CTRLVOLUME|CTRLPAN|CTRLFREQUENCY`) | los mismos + `DSBCAPS_GETCURRENTPOSITION2` + `DSBCAPS_GLOBALFOCUS` | `GETCURRENTPOSITION2` hace que `GetCurrentPosition` devuelva la posición real (el *gate* depende de ella); `GLOBALFOCUS` mantiene audible la demo de consola cuando no es la ventana activa. Sobre el DirectSound emulado de Windows 10/11 el resultado de `CreateSoundBuffer` es `DS_OK` (verificado en hardware, §16.5) |
| D14 | recrea el buffer secundario en cada reproducción (`0x0053874e..0x00538815`) | `stop` hace `Stop()` + `SetCurrentPosition(0)` + `write_offset = 0` sobre el mismo buffer | equivale a «buffer nuevo» para un segundo flujo sin destruir y recrear el objeto. `pause` **no** resetea: debe conservar la posición |

Además, el índice inicial se satura a `[0, 88]` antes de indexar la tabla
(`gex_adpcm_apply`), mientras que el original indexa con el global tal cual. Con
el estado siempre en rango el resultado es idéntico; sólo protege ante memoria
corrupta.

Lo que **no** se ha cambiado: orden del bucle, nibble alto primero, ajuste del
índice antes del delta, `step_size` del índice anterior, clamp `[-32768,
32767]`, clamp del índice `[0, 88]`, tamaños `0x8000`/`0x4000`/`0x1000`,
`DSBPLAY_LOOPING`, la media mitad de silencio y la rama de parada.

## 12. Pruebas

```
$ cd port
$ .\build.ps1 test          # Windows
$ make test                 # POSIX
69305 checks, 0 failures    # test_sag
60052 checks, 0 failures    # test_adpcm
215 checks, 0 failures      # test_audio
50 checks, 0 failures       # test_device (se omite sin tarjeta de sonido)
```

`test_adpcm.c` cubre:

- tablas recuperadas (extremos, monotonía, valores conocidos);
- vector dorado `07 F0` → `{0, 11, −19, −15}`, estado `{−15, 15}`;
- orden de nibbles (alto primero) y conmutación por byte;
- continuidad del estado al partir el flujo en dos llamadas;
- saturación a `32767` y a `−32768`;
- límites del índice (0 y 88) y saturación de un índice corrupto;
- fuente corta (menos bytes que muestras pedidas) y fuente vacía;
- validación de argumentos `NULL` y máscara de nibble fuera de rango;
- tablas alternativas (el algoritmo no depende de las recuperadas);
- 20 000 flujos aleatorios con invariantes (índice en rango, historia en rango,
  muestras producidas = bytes × 2).

`test_audio.c` cubre:

- ciclo de vida del buffer: lock, doble lock rechazado, tamaño distinto
  rechazado, unlock, `halves`/`samples_submitted`, conmutación de mitades,
  play/stop, unlock sin lock;
- fallo del *backend* en `submit` → `GEX_AUDIO_ERR_BACKEND`;
- rechazo de formatos: sin `fmt `, estéreo, 8 bits, rate 0, payload vacío,
  argumentos `NULL`;
- payload corto: una lectura corta cierra la fuente, rellena de ceros, emite la
  mitad de silencio y luego `Stop` (y sigue emitiendo `Stop`);
- payload múltiplo exacto de `0x1000`: la fuente se cierra en la llamada
  siguiente con `want == 0` (D8) y **no** se escribe fuera de la mitad;
- equivalencia con una decodificación de una sola pasada: la concatenación de
  las mitades entregadas es idéntica, muestra a muestra, al flujo completo;
- payload truncado (`data_size` > fichero): se reproduce lo disponible y se
  marca `payload_truncated`;
- *gate* del cursor: el prefill no espera, la segunda mitad se rellena con el
  cursor en 0, la primera espera a `0x4000` y cuenta `deferred`;
- cola silenciosa: después de las muestras decodificadas no se entrega ni una
  muestra obsoleta (regresión del fallo descrito en §10.2);
- errores y flujo sin backend (`submit == NULL`);
- planificador (§16.3): no bombea antes de `play`, respeta el *gate* contra el
  cursor del *sink*, cuenta `fills`/`deferred`, cierra en `Stop`, deja de hacer
  nada al terminar, drena sin cursor, pausa/reanudación idempotentes (dos
  `pause` no llaman dos veces al backend), propaga el error del backend y valida
  argumentos.

`test_device.c` (§16.5) se ejecuta sólo si existe un dispositivo DirectSound; si
no, imprime `skipped` y devuelve 0, de modo que la batería sigue siendo
ejecutable en cualquier máquina. Cubre apertura/cierre (incluido doble cierre),
rechazo de formatos, prefill → `submits == 1` y `playing == 1`, contenido de la
mitad escrita leído por `Lock` y comparado con `gex_adpcm_decode`, avance del
cursor a 44 100 B/s, *gate* con el cursor congelado, pausa/reanudación, volumen
(`100 → 0 dB`, `0 → -3000 dB`), `Stop` que congela el cursor y reinicio con un
segundo flujo.

## 13. Demostración

```
$ cd port
$ .\build.ps1 audio
compile sagplay.exe (clang)
source:  synthetic SLAG (440 Hz, 0.5 s, 22050 Hz, built in memory)
file:    (in memory)
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

- El tono se genera con un codificador ADPCM voraz escrito para la demo
  (`encode_tone`), **no** forma parte del juego: el motor original sólo
  decodifica.
- `sagplay` construye el contenedor `SLAG` en memoria, lo parsea con el lector
  del port, abre el flujo, hace el prefill, bombea hasta `Stop` y escribe cada
  mitad al WAV.
- Verificación independiente del WAV: periodo mediano de 50 muestras a
  22 050 Hz → **441 Hz** (el tono pedido es 440 Hz; 22 050/440 = 50,11 muestras
  por periodo, así que 50 es la cuantización correcta), pico 12 241, y **cero**
  muestras no nulas después de la señal.
- `sagplay <fichero.sag> [salida.wav]` hace lo mismo con un contenedor real, si
  algún día hay uno.
- `sagplay --device` hace el mismo recorrido contra el dispositivo real (§16.6).

## 14. Límites y lo que sigue pendiente

### 14.1 Confirmado por instrucciones

- Algoritmo, orden y estado del decodificador (§4).
- Tamaños, ramas, *gate* y orden `Lock`/flip/`Unlock` de la bomba (§5).
- Secuencia del reproductor y flags de `DSBCAPS` (§6).
- El bug del múltiplo exacto (§5.5).

### 14.2 Hipótesis

- Los **valores** de las tablas en la referencia (§9.4).
- El contenido de `0x5582E0` (formato del primario) en la referencia.
- El significado de `[0x5582F4]` (volumen 0..100 vs 0..127).
- La identidad CRT de `0x00541060`/`0x00540DE0`/`0x005416E0` (`fopen`/`ftell`/
  `malloc`) y de `0x00541950` (`HeapFree`): se deducen de la forma de las
  llamadas.

### 14.3 UNKNOWN

- **No hay mezcla multi-voz en esta ruta.** Si el juego reproduce varios
  sonidos a la vez (efectos + música + voz), debe haber **otro** camino que no
  pasa por `0x00538731`; no se ha encontrado ni descartado. La ruta aquí
  reconstruida es de **un solo** buffer: cada `0x00538731` para y libera el
  anterior.
- No se sabe si el juego real usa `0x005380B3` desde el hilo principal o desde
  otro; los llamantes están en `0x00463205`, `0x0053A5FB` y `0x0053FCB7`, pero
  no se ha medido su frecuencia.
- No se ha observado ninguna reproducción real: no hay `.sag` ni imagen de
  referencia instrumentada.

### 14.4 Para integrar audio en el juego

1. ~~**Backend de dispositivo**~~ → hecho en I-2b con DirectSound (§16.1).
   Sigue abierto un backend para otras plataformas (ALSA/PipeWire/SDL): basta
   con implementar `submit`/`stop`/`cursor`/`pause`/`resume`.
2. ~~**Hilo o temporizador de bombeo**~~ → hecho en I-2b: `gex_audio_sched`
   (§16.3) reproduce el contrato del frame loop. Conectado al frame en I-3:
   `port/src/core/frame.c` llama al tick una vez por frame presentado (§16.3).
3. **Localizar los `.sag`** en los datos del juego y el camino que los nombra
   (`0x00537ED1` construye rutas `…\audio\voiceuk\%s.sag`), para poder probar
   con audio real.
4. **Decidir qué hacer con el bug del múltiplo exacto** en ficheros reales: si
   alguno lo es, el original se corrompe; el port lo reproduce truncado.
5. **Volumen**: `gex_audio_dsound_set_volume` ya aplica `level*30 - 3000`
   (§16.4); falta decidir de dónde sale el nivel (el global `[0x5582F4]`) y si
   se expone como ajuste de usuario.

## 15. Cómo reproducir

```powershell
cd port
.\build.ps1 test        # contenedor + ADPCM + ruta de audio + dispositivo
.\build.ps1 audio       # demo: SLAG sintético -> build\tone.wav
.\build\sagplay.exe --device   # el mismo tono por el altavoz (Windows)
.\build.ps1 all         # todo lo anterior + sagdump
```

```bash
cd port
make test && make audio
```

Regresión del resto del repositorio:

```bash
node --test tools/gexmap/test.mjs
node tools/gexmap/cli.mjs check
node tools/gexmap/cli.mjs segments
```

## 16. Backend de dispositivo y programación del bombeo (I-2b)

Hasta I-2 el port producía PCM correcto pero no sonaba: el *backend* era la
captura en memoria de las pruebas y el WAV de la demo. I-2b cierra las dos
piezas que faltaban —un *sink* que habla con el dispositivo y algo que llame a
la bomba a tiempo— sin tocar el decodificador ni la bomba.

### 16.1 Backend elegido: DirectSound

`gex_audio_dsound` (`port/include/gex/audio/backend_dsound.h`,
`port/src/audio/backend_dsound.c`) recrea el objeto que el original maneja:

| Original | Port |
| --- | --- |
| `DirectSoundCreate` + `SetCooperativeLevel` (`0x00538551`) | `gex_audio_dsound_open` |
| `CreateSoundBuffer(0x8000, 0xE0, fmt)` (`0x005387f1`) | `gex_audio_dsound_open` (`DSBUFFERDESC`) |
| `Lock`/`Unlock` de la mitad `[0x57C37C]` (`0x0053812f..0x0053827e`) | `submit` |
| `Play(0,0,DSBPLAY_LOOPING)` (`0x0053884c`) | `submit` la primera vez |
| `GetCurrentPosition` (`+0x10`) | `cursor` |
| `Stop` (`0x00538251`) | `stop` |
| `Restore` tras `DSERR_BUFFERLOST` (`0x00538040`) | `submit` |
| `SetVolume(level*30-3000)` (`0x0053886c`) | `gex_audio_dsound_set_volume` |

**Por qué DirectSound y no SDL/WASAPI:**

1. **Es la API del original.** El cursor, el *wrap* del `DSBPLAY_LOOPING` y la
   regla de «no sobrescribir la mitad que suena» se conservan 1:1 en vez de
   re-derivarlos sobre otro transporte. Un backend distinto obligaría a
   reconstruir la semántica de posición y a demostrar que coincide.
2. **Cero dependencias nuevas.** `dsound.h` viene con el toolchain
   (MinGW-w64/llvm-mingw) y `dsound.dll` con Windows; el enlace es `-ldsound`.
   No se descarga ni se vendoriza nada.
3. **SDL no está disponible aquí.** En la máquina de desarrollo no existen
   `sdl2-config` ni `sdl3-config`, no hay `SDL.h` en el toolchain ni
   `pkg-config`. Adoptar SDL habría significado traer código de terceros,
   revisar su licencia y convertirlo en dependencia obligatoria del build para
   un solo *sink*. Se descarta, y queda como opción documentada para Linux.

**Formato:** mono PCM 16 bits a la frecuencia que declara el `.sag` (la misma
restricción que ya imponía `gex_audio_stream_open`). Cualquier otro formato se
rechaza con `GEX_AUDIO_DEVICE_BAD_FORMAT` antes de crear el dispositivo.

**Cola acotada por construcción:** el buffer es fijo de `0x8000` bytes, igual
que el original. Si el bombeo llega tarde, el *gate* rechaza sobrescribir la
mitad que suena y el dispositivo repite la mitad anterior; la cola no crece.
No hay hilo ni *callback* de audio: `submit` corre en el hilo del llamante, que
es el mismo que llama a la bomba (el original hace lo mismo desde el frame
loop), así que el dispositivo nunca ejecuta trabajo de decodificación.

### 16.2 Diferencias con el original (D12-D14)

Están en la tabla de §11. Resumen: `DSSCL_NORMAL` con ventana de escritorio
(D12), dos flags extra en `DSBUFFERDESC` (D13) y `stop` que resetea la posición
en lugar de recrear el buffer (D14). Ninguna cambia el flujo de datos ni el
resultado audible; las tres son consecuencia de que la demo no tiene ventana de
juego y de que DirectSound en Windows 10/11 está emulado.

### 16.3 Programación del bombeo: `gex_audio_sched`

El original no tiene temporizador de audio: el frame loop llama a `0x005380B3`
una vez por pasada (`0x00463205`, `0x0053A5FB`, `0x0053FCB7`) y el reproductor
una vez antes de `Play` (`0x00538842`). La bomba es idempotente: bloquea una
mitad y, si el cursor sigue en ella, vuelve sin escribir
(`0x00538108..0x0053812a`).

`gex_audio_sched` (`port/include/gex/audio/sched.h`, `port/src/audio/sched.c`)
reproduce ese contrato:

```c
gex_audio_sched sched;
gex_audio_sched_init(&sched, &stream);
/* una vez por frame, después de la fase de update */
gex_audio_sched_tick(&sched);
```

- `gex_audio_sched_due(&sched)` es puro: dice si la bomba escribiría ahora,
  aplicando el mismo test que la bomba. **No** recibe el cursor: lo lee del
  propio *sink* (`backend.cursor`), que es la misma llamada que hace la bomba.
  Una sola fuente de verdad evita que planificador y bomba discrepen (fue un
  fallo real detectado por una prueba roja durante I-2b: el planificador decía
  «toca» con un cursor que la bomba no veía, y la bomba difería).
- `gex_audio_sched_tick(&sched)` llama a `gex_audio_stream_pump` sólo si toca,
  mantiene `ticks`/`fills`/`deferred` y propaga el estado del backend en
  `last_status`. Un error del backend **no** cierra el flujo por sí solo: el
  llamante decide.
- Llamar de más es inocuo (sólo cuenta un `deferred`). Llamar de menos de una
  vez por mitad (8 192 muestras, ≈0,37 s a 22 050 Hz) hace que el dispositivo
  repita la mitad anterior, exactamente como el original.
- Sin `cursor` (WAV, captura en memoria) el planificador drena a su ritmo:
  `due` es siempre cierto mientras haya flujo.
- `gex_audio_sched_pause`/`_resume` paran y reanudan el bombeo y, si el *sink*
  los implementa, congelan/reanudan el dispositivo en su posición. Son
  idempotentes: dos `pause` seguidos no llaman dos veces al backend.

**Punto de integración con el frame loop:** una única llamada
`gex_audio_sched_tick(&sched)` por frame, **después** de la fase de update y
antes del swap (el orden medido en `docs/FRAME_STEP_TRACE.md`). El planificador
no necesita reloj propio ni hilo: si el frame dura más de ≈0,37 s, el *gate*
simplemente difiere y el sonido repite la mitad anterior, que es el
comportamiento del original bajo carga. `sched.finished` avisa de que el flujo
terminó para que el frame loop libere el stream.

**Estado I-3 (2026-11-01):** ese punto ya está insertado y probado. Es
`port/src/core/frame.c`, dentro de la fase de presentación del frame loop
(`docs/FRAME_LOOP_PORT.md` §5): una llamada por frame presentado, nunca por
paso lógico de recuperación, y ninguna llamada si el frame aborta antes. El
frame loop no cierra el stream: lo libera quien lo abrió.

### 16.4 Volumen

`gex_audio_dsound_set_volume(dev, level, &applied)` aplica
`level * 30 - 3000` dB, la fórmula de `0x0053886c` (`level` 0..100, 100 = 0 dB
= máximo). El «clamp» posterior del original (`0x0053888b`/`0x00538891`) son dos
auto-asignaciones: código muerto, no se replica. `applied` devuelve el valor
enviado al dispositivo para que la prueba pueda comprobarlo.

### 16.5 Pruebas

| Suite | Qué prueba | Resultado |
| --- | --- | --- |
| `test_audio` | planificador: ciclo completo, `gate`, `deferred`, pausa/reanudación, error del backend, argumentos | **215 checks, 0 fallos** |
| `test_device` | dispositivo real: apertura, formatos, contenido de la mitad leída por `Lock`, avance del cursor, `gate` con cursor congelado, pausa/reanudación, volumen, `Stop`, reinicio | **50 checks, 0 fallos** |
| `test_adpcm` / `test_sag` | sin cambios | 60 052 / 69 305, 0 fallos |

`test_device` se **auto-omite** si no hay dispositivo (`device: no DirectSound
device on this machine; skipped`, salida 0), de modo que la batería se puede
ejecutar en cualquier máquina.

Evidencia medida en esta máquina (Windows 11, DirectSound emulado, clang
22.1.8):

```text
device: cursor after pause = 0 bytes
device: cursor moved 8446 bytes in 219 ms (expected ~9657)
device checks: 50, 0 failures
```

El cursor avanza a ≈44 100 B/s (22 050 Hz × 2 B) con una tolerancia de un
tercio, lo que demuestra que el dispositivo consume el PCM que produce el port.
La prueba además lee la mitad escrita con `IDirectSoundBuffer::Lock` y la
compara muestra a muestra con `gex_adpcm_decode`, así que el contenido que llega
al hardware es exactamente el decodificado.

### 16.6 Qué es hardware y qué es simulado

- **Hardware real:** `test_device` y `sagplay --device` abren un dispositivo
  DirectSound de verdad y le entregan buffers; el cursor medido es del
  dispositivo. En esta máquina: `CreateSoundBuffer` con los flags de D13
  devuelve `DS_OK`, `Lock` por mitades da `b1 = 16384`, `b2 = 0`, y el cursor
  avanza a la frecuencia del flujo.
- **Simulado / sin hardware:** `test_audio` (captura en memoria) y
  `build/tone.wav` (fichero). Verifican la lógica, no el altavoz.
- **No verificado:** que una persona **oiga** el tono. El agente no tiene oídos
  ni acceso a la salida de audio; lo único objetivable es que el dispositivo
  aceptó los buffers y que su cursor avanza. La confirmación auditiva queda para
  el mantenedor con `sagplay --device`.
- **Sin `.sag` reales:** todo se valida con fixtures sintéticos. Las tablas
  ADPCM siguen siendo **HIPÓTESIS** para la imagen de referencia (§9.4).

### 16.7 Cómo oírlo

```powershell
cd port
.\build.ps1 audio                 # compila sagplay y escribe build\tone.wav
.\build\sagplay.exe --device      # reproduce el tono sintético por el altavoz
.\build\sagplay.exe --device mi.sag   # o un contenedor propio, si lo hay
```

Salida medida con el tono sintético (el `Sleep(1)` del bucle de espera hace que
el número de *pumps* y el tiempo de pared varíen entre ejecuciones; el audio
entregado no):

```text
source:  synthetic SLAG (440 Hz, 0.5 s, 22050 Hz, built in memory)
payload: 5512 bytes of 4-bit ADPCM (11024 samples)
pumps:   60 calls of 0x005380B3 (56 deferred by the gate)
decoded: 11024 samples
halves:  3 submitted, 24576 samples (1.115 s)
stops:   1
device:  DirectSound default, DSSCL_NORMAL, 22050 Hz mono 16-bit
submits: 3 halves, 0 restores, 0 lock failures
cursor:  16384 bytes before the stop, 390 ms wall
note:    the PCM was handed to the device; confirm by ear
```

Los 56 `deferred` son la prueba de que el *gate* está actuando en tiempo real:
el bucle sólo escribe cuando el cursor ha dejado la mitad. `stops: 1` es la rama
de parada de `0x00538242`, que llega cuando la fuente está cerrada y la mitad de
silencio ya está en el buffer; por eso el tiempo de pared (≈0,4 s) es menor que
la duración nominal del buffer (1,115 s): el `Stop` corta la cola de silencio,
igual que el original.

### 16.8 Límites de I-2b

- **Audibilidad humana:** no verificada por el agente (ver §16.6).
- **Sólo Windows:** `backend_dsound.c` es `_WIN32`; el `Makefile` POSIX no lo
  compila y `sagplay --device` no existe allí. `test_audio`, `test_adpcm` y
  `test_sag` siguen siendo portables.
- **Un solo flujo:** igual que la ruta reconstruida, no hay mezcla multi-voz
  (§14.3). Dos `gex_audio_stream` sobre el mismo `gex_audio_dsound` se pisan.
- **Sin `.sag` reales** ni imagen de referencia instrumentada: la equivalencia
  con el original sigue siendo estática.
- **Volumen:** la fórmula está implementada y probada, pero nadie conecta aún el
  global `[0x5582F4]` del juego.
