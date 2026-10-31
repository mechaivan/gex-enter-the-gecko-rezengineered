# Rutas de inicialización de DirectSound (EU)

> Estado: **análisis estático** sobre el desensamblado y el grafo de llamadas
> publicados. Ninguna ruta de este documento se ha observado en ejecución.
> Datos declarativos y validables: `tools/gexmap/audio.mjs`.
> Fuente de verdad: `analysis/EU/{functions.tsv,calls.tsv,anchors.json,summary.json}`.

## 1. Qué se ha hecho

`docs/SUBSYSTEM_SEGMENTS.md` §6.3 dejaba abierto si el segundo
`DirectSoundCreate` (`0x00538CB0`) era alcanzable. Este documento cierra esa
pregunta y documenta las tres cadenas de inicialización de audio que el código
sí conecta con la entrada del programa:

1. **Ruta A** — arranque: `WinMain` → frontera audio/cine → dispositivo +
   buffer primario.
2. **Ruta B** — reproducción por nombre: `WinMain` → trampolín → sonido →
   buffer secundario.
3. **Ruta C** — voz: ruta de voz → buffer secundario (prefijo parcial).

Y la **ruta D**, la que existe pero no tiene llamantes: `0x00538FD5` → `0x00538CB0`.

Resultado en una línea: `0x00538551` es el **único** creador del dispositivo
DirectSound vivo, tiene exactamente **dos llamantes** (`0x00538731` y
`0x0053B10B`) y ambos están conectados a la entrada por llamadas directas; el
segundo `DirectSoundCreate` de `0x00538CB0` sigue **sin ningún llamante en toda
la cadena** y además construye menos cosas (no crea buffer primario).

## 2. Método

### 2.1 Fuentes

| Fuente | Uso |
|---|---|
| `analysis/EU/functions.tsv` | inicio, tamaño, tipo, base de detección, llamantes/callees |
| `analysis/EU/calls.tsv` | aristas `from → to` con su tipo |
| `analysis/EU/anchors.json` | anclas documentales (no usadas para estas rutas) |
| Desensamblado completo (`objdump` Intel x86) | lectura instrucción a instrucción |

El desensamblado se generó con el mismo ejecutable de referencia que el mapa
(1 557 504 B, MD5 `692b12825003417cc4a5a9d4db13eebe`). Las direcciones de este
documento son **direcciones virtuales con base de imagen `0x00400000`**.

### 2.2 Política de evidencia

Tres niveles, que no se mezclan:

- **CONFIRMED (estático)** — la instrucción está en el desensamblado: el
  `call`, el slot de vtable, el global escrito, la rama tomada. Se cita la
  dirección de la instrucción.
- **HYPOTHESIS** — la lectura de *para qué* sirve (rol del buffer, nombre de una
  función CRT, significado de un formato). Se marca siempre.
- **NO OBSERVADO** — comportamiento dinámico. Ninguna ruta de aquí tiene hits,
  frames ni trazas: no se instrumentó la imagen de referencia (ver §7).

La distinción importa: que una cadena **exista** en el código y sea alcanzable
por llamadas directas desde la entrada no demuestra que **se ejecute**, ni en
qué orden, ni con qué argumentos reales.

## 3. Las cuatro cadenas

### 3.1 Ruta A — arranque (dispositivo + buffer primario)

```text
0x00541B20  CRT startup
  └─ 0x00463278  WinMain                          sitio 0x00541C5A
       └─ 0x0053D822  frontera audio/cine         sitio 0x00463A02
            └─ 0x0053B10B  init del sonido        sitio 0x0053D82F
                 └─ 0x00538551  dispositivo       sitio 0x0053B120
```

- `0x00541B20` es la **entrada PE** (`basis=entry` en el mapa) y llama a
  `0x00463278` con los cuatro argumentos de `WinMain`.
- `0x0053D822` importa `auxGetVolume` y `mciSendCommandA`: es la frontera entre
  el sonido del juego y el volumen/MCI del sistema.
- `0x0053B10B` es el envoltorio de inicialización: llama a `0x00538551` **solo
  si** `[0x5AC5C8] == 0`, y guarda el dispositivo resultante en `[0x5AC5C8]`.
  Es decir, la creación del dispositivo ocurre **una sola vez** por proceso
  mientras esa bandera siga puesta.
- `0x00538551` recibe el `HWND` de `[0x65D7C8]`, una copia hecha en
  `0x00401031`/`0x00401042` del handle que `WinMain` guarda en `[0x6488F8]`.

### 3.2 Ruta B — reproducción por nombre (buffer secundario)

```text
0x00541B20 -> 0x00463278  WinMain
  └─ 0x00538017  trampolín                        sitios 0x004637F0, 0x00463808
       └─ 0x00537ED1  reproducir por nombre       sitio 0x00538026
            └─ 0x0053A5A2  comprueba y arranca    sitio 0x00537FB7
                 └─ 0x00538731  buffer secundario sitio 0x0053A5D1
                      └─ 0x00538551  dispositivo  sitio 0x00538739
```

- `0x00538017` es un trampolín de 41 B: reenvía sus tres argumentos a
  `0x00537ED1` y retorna 0.
- `0x00537ED1` construye dos rutas con `0x5406B0` (formatos `0x55811C` y
  `0x558124`), comprueba la segunda con `0x541060`/`0x540C20` y llama a
  `0x0053A5A2`.
- `0x0053A5A2` verifica que ambos ficheros existen (`0x53A7D8`), llama a
  `0x00538731(0x537E9C(1), rutaB, 1)`, luego a `0x0053A5FB` y finalmente a
  `0x0053851A` (parada del buffer).
- `0x00538731` llama a `0x00538551` como **guarda**: si el dispositivo ya
  existe, `0x00538551` retorna 0 de inmediato (`0x538557-0x53855E`).

### 3.3 Ruta C — voz (parcial)

```text
0x0053FF0D  ruta de voz
  └─ 0x00538731  buffer secundario                sitio 0x0054002A
       └─ 0x00538551  dispositivo                 sitio 0x00538739
```

- `0x0053FF0D` fija el volumen con `0x00538724(valor/0x7F)` y, **solo si**
  `0x005380A6()` (bit `DSBSTATUS_PLAYING`) devuelve 0, llama a `0x00538731`
  con `0x64FD80`.
- **Es parcial a propósito**: `0x0053FF0D` no se alcanza desde la entrada por
  una cadena de llamadas directas documentada aquí. Su prefijo está en
  `docs/SUBSYSTEM_SEGMENTS.md` §S3 (`0x0053FE80 <- 0x00540036 / 0x00540301 /
  0x00540088 / 0x005402C8 / 0x0054032D`). Por eso el validador exige que esta
  ruta **no** empiece en la entrada, y falla si alguien la «arregla» sin
  evidencia.

### 3.4 Ruta D — la segunda creación de DirectSound (sin llamantes)

```text
0x00538FD5  prólogo (0 llamantes)
  └─ 0x00538CB0  segunda creación                sitio 0x00538FD8
```

`0x00538FD5` (35 B, `basis=prologue`) tiene **0 aristas entrantes** en
`calls.tsv`; `0x00538CB0` (107 B) tiene **1**: la de `0x00538FD5`.

## 4. Lectura de las funciones

### 4.1 `0x00538551` — dispositivo + buffer primario (467 B, CONFIRMED)

| Paso | Dirección | Instrucción |
|---|---|---|
| guarda de reentrada | `0x538557-0x53855E` | `cmp [0x57C384],0 ; jne 0x53871E` (retorna 0) |
| creación | `0x538564-0x538579` | `DirectSoundCreate(NULL,&[0x57C384],NULL)`; si falla → `-1` |
| nivel cooperativo | `0x53857E-0x538597` | `SetCooperativeLevel(+0x18, hwnd, 3)` |
| descriptor | `0x5385BC-0x5385DA` | `[0x57C330]=0x14`, `+4=1` (PRIMARYBUFFER), `+8=0`, `+0xC=0` |
| buffer primario | `0x5385E4-0x538608` | `CreateSoundBuffer(+0xC, &0x57C330, &[0x57C378], NULL)` |
| errores | `0x538614-0x5386A6` | switch de 7 `HRESULT` con **cuerpos vacíos** |
| formato | `0x5386C9-0x5386E1` | `SetFormat(+0x38, 0x5582E0)` |
| limpieza | `0x5386E3-0x53871C` | `Release(+0x8)` de primario y dispositivo, punteros a 0, `-1` |

`dwFlags=1` es `DSBCAPS_PRIMARYBUFFER`; `lpwfxFormat=0` es lo correcto para un
buffer primario. Los 18 bytes de `0x5582E0` (WAVEFORMATEX) **no se pueden leer**
sin la imagen original: no se afirma la frecuencia ni el número de canales.

### 4.2 `0x00538731` — buffer secundario (379 B, CONFIRMED)

| Paso | Dirección | Instrucción |
|---|---|---|
| asegura dispositivo | `0x538735-0x538744` | `call 0x538551`; si `-1` → `-1` |
| libera el anterior | `0x53874E-0x538779` | `Stop(+0x48)`, `Release(+0x8)`, `[0x57C374]=0` |
| limpia fichero | `0x538783-0x53878C` | `0x5384ED` (¿hay fichero?) y `0x5384A3` (cierra/libera) |
| carga | `0x538791-0x5387A3` | `0x5388AC(arg2,arg3)` |
| formato | `0x5387AD-0x5387BC` | `0x53899C(0x57C348, 0x12)` |
| descriptor | `0x5387C9-0x5387E7` | `+0x10=0x57C348`, `+0=0x14`, `+4=0xE0`, `+8=0x8000` |
| buffer secundario | `0x5387F1-0x538815` | `CreateSoundBuffer(+0xC, &0x57C330, &[0x57C374], NULL)` |
| cursores | `0x53881A-0x538838` | `[0x57C360]=[0x57C35C]=[0x57C37C]=[0x57C380]=0` |
| mezclador | `0x538842-0x538847` | `call 0x5380B3`, `call 0x538040` |
| arranque | `0x53884C-0x538865` | `Play(+0x30, 0, 0, 1)` (`DSBPLAY_LOOPING`) |
| volumen | `0x53886C-0x5388A6` | `SetVolume(+0x3C, [0x5582F4]*0x1E-0xBB8)` |

`dwFlags=0xE0` = `DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_CTRLFREQUENCY`
(0x20+0x40+0x80); `dwBufferBytes=0x8000` (32 KiB) es el tamaño del **único**
buffer de streaming. El clamp posterior a `SetVolume` (`-10000..0`) es
**código muerto**: la expresión `volumen*30-3000` con `volumen` en `[0,127]`
nunca sale de ese rango.

### 4.3 `0x0053B10B` — envoltorio de inicialización (400 B, CONFIRMED)

1. `0x53B111-0x53B13A`: si `[0x5AC5C8]==0`, `0x538551([0x65D7C8])`; si `-1`
   retorna 0; si no, `[0x5AC5C8] = [0x57C384]`.
2. `0x53B140-0x53B14D`: si `[0x5AC5F0]!=0`, `0x53A81D`.
3. `0x53B15B-0x53B1AA` y `0x53B1BD-0x53B20B`: liberan cada puntero no nulo de
   los arrays `[0x5AC5CC]` (con `[0x5AC5D8]` entradas) y `[0x5AC5D0]` (con
   `[0x5AC5D4]` entradas).
4. `0x53B215-0x53B23A`: `memset(0x5867C8, 0, 0x25E00)` y
   `memset(0x585810, 0, 0xFA0)`.
5. `0x53B23D-0x53B25C`: `0x53B3CC(arg)`; si 0 → retorna 0;
   `0x53B29B(0x20, resultado)`; si 0 → retorna 0.
6. `0x53B26C-0x53B286`: `0x53A8A2(arg, 0, 0, resultado)`; si 0 → retorna 0.
7. `0x53B288-0x53B292`: `[0x5AC5F0]=1`; retorna 1.

`0x53A81D`, `0x53B3CC`, `0x53B29B` y `0x53A8A2` están en el mapa, pero su rol
interno no se ha leído aquí; solo se afirma el orden y el uso de sus valores de
retorno.

### 4.4 `0x00538CB0` — segunda creación (107 B, CONFIRMED)

| Paso | Dirección | Instrucción |
|---|---|---|
| creación | `0x538CB3-0x538CC5` | `DirectSoundCreate(NULL,&[0x5847E4],NULL)`; si falla retorna |
| nivel cooperativo | `0x538CC7-0x538CDD` | `push 1; call 0x537E9C; push eax; push [0x5847E4]; call [ecx+0x18]` |
| alias | `0x538CE6-0x538CEB` | `[0x57C39C] = [0x5847E4]` |
| limpieza | `0x538CF0-0x538D0A` | `memset(0x5846F0,0,0x2C)`, `memset(0x5846C8,0,0x21)` |
| bandera | `0x538D12` | `mov byte [0x5847E0], 1` |

Tres diferencias decisivas con `0x00538551`:

1. **No crea buffer primario** ni llama a `CreateSoundBuffer` ni a `SetFormat`.
2. **No comprueba** el `HRESULT` de `SetCooperativeLevel` (lo ignora).
3. La llamada a `SetCooperativeLevel` recibe **un solo argumento de pila**
   (`hwnd`): falta `dwLevel`. La firma COM exige dos argumentos tras `this`;
   tal como está, el `dwLevel` que lee la callee es basura de la pila.

Además usa globales propias (`0x5847E4`, `0x57C39C`, `0x5847E0`) que ningún
otro código lee como dispositivo activo.

### 4.5 `0x005380B3` — mezclador (466 B, CONFIRMED)

`GetCurrentPosition(+0x10)`, `Lock(+0x2C)` de `0x4000` B sobre `[0x57C374]`,
mezcla en la región bloqueada, `Stop(+0x48)` si procede y `Unlock(+0x4C)`.
Mantiene el cursor en `[0x57C37C]` y la bandera `[0x57C380]`. Es el consumidor
del buffer de 32 KiB que crea `0x00538731`.

## 5. Comparación de las dos creaciones

| Aspecto | `0x00538551` (viva) | `0x00538CB0` (sin llamantes) |
|---|---|---|
| `DirectSoundCreate` | sí, `0x53856D` | sí, `0x538CBC` |
| comprueba el resultado | sí (`0x538572`) | sí (`0x538CC1`) |
| `SetCooperativeLevel` | `hwnd, 3` (dos argumentos) | `hwnd` (falta `dwLevel`) |
| comprueba su `HRESULT` | sí | **no** |
| buffer primario | sí (`DSBCAPS_PRIMARYBUFFER`) | **no** |
| `SetFormat` del primario | sí | **no** |
| global del dispositivo | `[0x57C384]` | `[0x5847E4]` / alias `[0x57C39C]` |
| bandera de «listo» | — | `[0x5847E0]` |
| llamantes | `0x00538731`, `0x0053B10B` | `0x00538FD5` (0 llamantes) |
| alcanzable desde la entrada | sí, por llamadas directas | no consta |

Lo único que comparten es el thunk `0x005404F0` (`DSOUND.dll!DirectSoundCreate`)
y la estructura `0x0057C330`. El validador fija esa intersección: si un día
comparten algo más, la prueba falla y obliga a releer.

## 6. Qué NO se afirma

- **No se afirma que la ruta D sea código muerto.** La ausencia de llamantes
  directos no prueba nada: un `call` indirecto, un *callback*, una tabla de
  punteros o un `jmp` calculado son invisibles al grafo. Lo que se afirma es
  más débil y verificable: **no tiene ninguna arista entrante en `calls.tsv` y
  no aparece en ninguna tabla de punteros verificada**.
- **No se afirma el orden temporal** de las rutas A, B y C en un frame real.
  Solo se afirma que las aristas existen y que los sitios caen dentro del
  llamante.
- **No se afirma el contenido** de `0x5582E0` (WAVEFORMATEX del primario), ni el
  texto de `0x55811C`/`0x558124`/`0x55812C`/`0x558130`.
- **No se afirma** que `0x541060`/`0x540C20`/`0x540CA0`/`0x540DE0`/`0x540F90`/
  `0x5416E0`/`0x541950` sean `fopen`/`fclose`/`fread`/`ftell`/`fseek`/`malloc`/
  `HeapFree`: es una lectura de la forma de las llamadas (HYPOTHESIS).
- **No se afirma** que `0x0053FF0D` sea «la» ruta de voz: se lee el string
  `%s\audio\voiceuk\%s.sag` en S3 y se ve que llama al buffer secundario; el rol
  es HYPOTHESIS.

## 7. Por qué no hay evidencia dinámica

La imagen de referencia (1 557 504 B, MD5
`692b12825003417cc4a5a9d4db13eebe`) **ya no está en la máquina de análisis**.
La única build presente es un mod distinto:

| | Referencia | Build presente |
|---|---:|---:|
| Bytes | 1 557 504 | 1 559 040 |
| MD5 | `692b1282…` | `3198350e…` |
| Entrada | `0x00541B20` | `0x00541FA0` |

Una traza de esa build **no sería evidencia sobre el binario de referencia**:
cambia la entrada, cambia `.data` de dirección y el patrón de bytes de
`0x00538551` no aparece en ella. Por eso no se instrumentó: no había
condiciones razonables para observar *esta* ruta en *este* binario.

Consecuencia práctica: las tres rutas vivas tienen **estado estático** y
**predicción dinámica no observada**. Cualquier frase del tipo «el juego crea el
dispositivo al arrancar» es una predicción, no una medida.

## 8. Cómo reproducir

```bash
# 1. validar los datos declarativos sin binario ni ejecución
node --test tools/gexmap/test.mjs

# 2. revalidar los artefactos comprometidos
node tools/gexmap/cli.mjs check
node tools/gexmap/cli.mjs segments

# 3. consultar una función del mapa
node tools/gexmap/cli.mjs function 0x00538551
```

Con el ejecutable de referencia a mano, el mapa se regenera con:

```bash
node tools/gexmap/cli.mjs map --exe /ruta/GEX3D.EXE
```

Las pruebas de `audio.mjs` comprueban, entre otras cosas: que `0x00538551`
tiene exactamente dos llamantes y ambos están en una ruta declarada; que
`0x00538FD5` sigue sin llamantes; que `0x00538CB0` nunca declara
`CreateSoundBuffer` ni `SetFormat`; que viva y muerta solo comparten
`0x005404F0`; que ningún global nombra un escritor inexistente; y que ninguna
ruta declara hits o frames sin sesión.

## 9. Siguiente paso

1. Recuperar la imagen de referencia (o una copia con el mismo MD5) para poder
   instrumentar `0x00538551` y `0x00538731` con los mismos breakpoints de
   hardware de `tools/frame-trace`.
2. Con la traza: medir si `0x00538551` se ejecuta una vez (ruta A) o varias
   (rutas B/C con la guarda ya puesta), y si `0x00538731` se ejecuta por
   sonido o solo al arrancar.
3. Buscar la ruta de datos hacia `0x00538FD5` con un barrido de tablas de
   punteros en `.data`/`.rdata`; mientras no aparezca, la ruta D sigue siendo
   «existe, no se alcanza por llamada directa».

## 10. Lector del contenedor implementado (2026-10-31)

El primer componente del port nativo ya no es solo estatico: el parser
`0x0053899C` y el cargador `0x005388AC` estan transcritos en C11 en
`port/src/asset/sag.c` (API en `port/include/gex/asset/sag.h`), con 29 casos
de prueba y fuzz en `port/tests/test_sag.c` (69 305 checks, 0 fallos).

Esto convierte el tramo que esta seccion describe en un formato ejecutable:

- la cabecera `SLAG` / dword != `0xFFFFFFFF` / `WAVE`;
- el recorrido de chunks `tag` + `size`;
- `fmt ` -> `memset(dest, 0, 18)` + `min(size, 18)` + `seek(pos + size)`;
- `data` -> devuelve el tamano declarado y deja el cursor al payload.

Lo que **no** cambia: esto lee el contenedor, no reproduce audio. La creacion
del buffer secundario (`0x00538731` -> `0x00538551`) y del mezclador
`0x005380B3` siguen sin implementar. Ver `docs/SAG_CONTAINER.md` para el
formato, las desviaciones deliberadas y los limites.

El lector se prueba sin el juego y sin DirectSound:

```powershell
cd port
.\build.ps1 all
```
