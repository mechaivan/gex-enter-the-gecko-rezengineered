# Frame loop nativo (I-3)

> Estado: **implementado y probado sin el juego** el 2026-11-01. Ejecuta el
> núcleo temporal, el orden de frame reconstruido y el planificador de audio de
> I-2b. **No** ejecuta lógica del juego ni renderiza nada: `pre_frame`, `update`
> y `present` son callbacks inyectados. I-4 conecta ventana, entrada y renderer
> nulo en el callback de presentación.
>
> Evidencia de partida: `docs/FRAME_STEP_TRACE.md` (traza dinámica),
> `docs/M13_DECOUPLE_PROPOSAL.md` §4 (acumulador), `docs/AUDIO_MIXER_RE.md`
> §16 (planificador).

## 1. Qué se ha construido

| Pieza | Fichero | Origen |
| --- | --- | --- |
| Acumulador de paso fijo | `port/include/gex/core/loop_core.h`, `port/src/core/loop_core.c` | `tools/m13-core/m13_core.c`, movido tal cual (prefijo `gex_loop_core_`) |
| Frame loop | `port/include/gex/core/frame.h`, `port/src/core/frame.c` | nuevo; orden de `0x0043EA36` |
| Pruebas deterministas | `port/tests/test_frame.c` | nuevo; 365 checks |
| Demo ejecutable | `port/tools/framedemo.c` | nuevo; reloj guionizado |
| Compatibilidad del prototipo | `tools/m13-core/m13_core.h` (shim) | traduce `m13_*` → `gex_loop_core_*` |

No se ha reescrito el acumulador: se ha **movido** al port y el harness del
prototipo compila ahora esa misma fuente. Hay una sola implementación de la
matemática del bucle en el repositorio.

## 2. Arquitectura

```text
gex_frame_run(now_us)                      una pasada del frame loop
│
├─ pre_frame        ← 0x0043F80D           entrada / preparación del frame
│
├─ gex_loop_core_iterate(now_us)           el acumulador M-13 (dueño del reloj)
│    │  dt = clamp(now - t_prev, 250 ms)
│    │  acc += dt
│    ├─ update × N   ← 0x0043E1BA          N = floor(acc/step), máx. 5
│    └─ present trampoline                 exactamente una vez por pasada
│         ├─ gex_audio_sched_tick()        ← 0x005380B3, una vez por frame
│         └─ present ← 0x0043DC3A + 0x0054053E + 0x005404F6
│
└─ contadores / estado
```

El reloj lo posee `gex_loop_core`: `gex_frame` nunca calcula `dt` por su cuenta.
Eso garantiza que el frame loop y el harness del prototipo no puedan discrepar.

**Sin hilos, sin `malloc`.** Todo el estado vive en `gex_frame` (y en el
`gex_audio_sched` que el llamante aporta). El bombeo de audio corre en el hilo
del llamante, igual que en el original.

## 3. Relación con `tools/m13-core`

- `m13_core.c` se ha movido a `port/src/core/loop_core.c` con `git mv` y solo
  cambia el prefijo de los símbolos (`m13_init` → `gex_loop_core_init`,
  `m13_iterate` → `gex_loop_core_iterate`). La semántica es idéntica.
- `tools/m13-core/m13_core.h` es ahora un shim de compatibilidad: incluye la
  cabecera del port y define `m13_config_t`, `m13_state_t`, `m13_init` y
  `m13_iterate` como alias. `test_m13_core.c` **no se ha tocado**: sus 120
  checks siguen siendo los mismos y ahora prueban el código que corre en el
  port.
- `tools/m13-core/Makefile` compila `test_m13_core.c` +
  `../../port/src/core/loop_core.c` con `-I../../port/include`.
- Se añade una función que el prototipo no tenía: `gex_loop_core_rebase()`,
  que re-ancla `t_prev` y vacía `acc`. La usa `gex_frame_resume()` para que una
  pausa no se convierta en una ráfaga de pasos de recuperación. No altera el
  contrato de `init`/`iterate`.

## 4. Orden de callbacks y correspondencia con la traza

Traza medida (`docs/FRAME_STEP_TRACE.md` §3.1) y su equivalente en el port:

| # | Original | Port | Estado |
| --- | --- | --- | --- |
| 1 | `0x0043EA36` frame step | `gex_frame_run()` | implementado |
| 2 | `0x0043F80D` entrada del frame | hook `pre_frame` | callback de prueba |
| 3 | `0x0043E1BA` update | hook `update`, 1..N veces | callback de prueba |
| 4 | `0x0043DC3A` render (condicional) | dentro de `present` | **no implementado** (I-4) |
| 5 | `0x0054053E` drenaje de cola | dentro de `present` | **no implementado** (I-4) |
| 6 | `0x005404F6` swap(3) | dentro de `present` | **no implementado** (I-4) |
| 7 | `0x0046300C` hoja posterior | — | **no modelada** (semántica sin identificar) |
| — | `0x005380B3` bombeo de audio | `gex_audio_sched_tick()` | implementado (I-2b) |

La hoja `0x0046300C` (5 bytes, posterior al swap) no se modela y no se le
inventa semántica. Si I-4 la identifica, entra como cuarto hook *después* de
`present`, sin reinterpretar el actual.

El orden observable en las pruebas es `pre → update → audio → present`; el
tick de audio se cuenta dentro del callback de presentación, así que la prueba
comprueba que ya ocurrió cuando `present` corre (`ticks_at_present == n`).

## 5. Punto de integración del scheduler de audio

`gex_audio_sched_tick(&sched)` se llama **una vez por frame presentado**,
dentro de la fase de presentación y **antes** del hook `present`:

```c
static void frame_present(void *ctx)      /* src/core/frame.c */
{
    if (f->audio != NULL) {
        const gex_audio_status status = gex_audio_sched_tick(f->audio);
        ...
    }
    f->hooks.present(...);
}
```

Consecuencias, todas probadas:

- una pasada con 3 updates de recuperación hace **un** tick, no tres: el
  original bombea por pasada del frame loop (`0x00463205`, `0x0053A5FB`,
  `0x0053FCB7`), no por update;
- si el frame se aborta antes (pre/update), **no hay tick**;
- si el tick falla, no se llama a `present` y el estado exacto queda en
  `f->last_audio`;
- `f->audio` es opcional: sin planificador, `audio_ticks == 0` y no se inventa
  ninguno.

El frame loop **no** abre, cierra, pausa ni reanuda el stream por su cuenta: el
`gex_audio_stream` y su `gex_audio_sched` son del llamante. `gex_frame_pause()`
sí delega en `gex_audio_sched_pause()` (que congela el cursor del dispositivo),
y `gex_frame_resume()` en `gex_audio_sched_resume()`.

## 6. Tiempo: 25 FPS, paso lógico y unidades

Dos relojes distintos que **no** se confunden:

| Concepto | Valor | Evidencia |
| --- | --- | --- |
| Periodo de **presentación** original | 40 ms (25 FPS, p50 de 1 723 intervalos) | `FRAME_STEP_TRACE.md` §3.2 |
| Cuantía del **sello del update** | 16,666 ms (60 Hz) | `FRAME_STEP_TRACE.md` §3.3: `(GetTickCount()-inicio)/16.666` |
| Clamp anti-espiral | 250 ms | propuesta M-13 §4 |
| Tope de recuperación | 5 pasos | propuesta M-13 §4 |

Configuraciones que envía el port:

- `gex_frame_config_original()` — `step_us = 40000`, `original_mode = 1`.
  En modo 1:1 el paso no se usa (se consume exactamente un update por present y
  `acc` se vacía), así que 40000 es solo documental: es el testigo de
  compatibilidad (ORG-1).
- `gex_frame_config_decoupled()` — `step_us = 16667`, `original_mode = 0`.

**Por qué 16667 y no 40000 en modo desacoplado.** 40 ms es una propiedad del
swap bloqueante, no de la simulación: el update ya calcula su sello en unidades
de 60 Hz (constante `16.666` leída del PE), así que 60 Hz es la granularidad
que el propio update habla. Elegir 40 ms «porque el original presenta a 25 FPS»
ataría la simulación a la presentación, que es justo lo que M-13 quiere
separar. Aun así, el valor definitivo de `STEP` sigue **pendiente de E-2**
(`M13_DECOUPLE_PROPOSAL.md` §8): los presets son una decisión documentada, no
una constante recuperada.

El reloj es inyectado (`now_us`, microsegundos, monótono esperado). No se lee
`GetTickCount` ni `QueryPerformanceCounter` en ninguna parte del núcleo ni del
frame loop: la lectura del reloj real será responsabilidad de I-4.

## 7. Política de catch-up, descartes, pausas y errores

- **Catch-up:** `floor(acc/step)` updates, acotado a `max_steps` (5). El
  acumulador conserva el resto fraccional exacto (p. ej. 60 ms a 16,667 ms son
  3 pasos y 9 999 µs de acarreo).
- **Descarte:** si se alcanza el tope y aún queda tiempo, `acc` se vacía y
  `core.discards_total++`. Es observable, nunca silencioso.
- **Delta excesivo:** `dt` se recorta a `max_dt_us` (250 ms) *antes* de entrar
  en el acumulador. Un parón de 10 s se comporta igual que uno de 250 ms.
- **Reloj invertido:** la muestra se ignora, `t_prev` no se mueve, el frame se
  presenta igualmente (una vez) y `run` devuelve `GEX_FRAME_WARN_CLOCK`
  (positivo, recuperable). Es la misma política que el núcleo del prototipo.
- **Pausa:** `gex_frame_pause()` marca `paused`, pausa el scheduler y hace que
  `run` no ejecute ninguna fase. `gex_frame_resume(now)` reancla el reloj y
  vacía `acc`: la duración de la pausa no genera pasos de recuperación.
  Ambas son idempotentes.
- **Errores:** un callback que devuelve distinto de cero aborta el frame. Las
  fases posteriores no se ejecutan (ni el scheduler ni `present`). `run`
  devuelve el `GEX_FRAME_ERR_*` concreto y, tras el error, vacía `acc` y
  reancla el reloj en `now_us` para que el paso fallido no se repita. El
  llamante decide si cierra el stream: el frame loop no cierra recursos ajenos.
- **Cierre:** `gex_frame_shutdown()` suelta hooks y puntero al scheduler, marca
  el frame como no inicializado y es idempotente. `run` sobre un frame cerrado
  devuelve `GEX_FRAME_ERR_STATE`. `gex_frame_init()` vuelve a empezar con todos
  los contadores a cero.

## 8. API pública y ciclo de vida

```c
gex_frame_config cfg = gex_frame_config_original();   /* o _decoupled() */
gex_frame_hooks hooks = { pre, update, present, ctx };
gex_frame f;
gex_frame_init(&f, &cfg, &hooks, &sched /* o NULL */, t0_us);
while (running) {
    gex_frame_status st = gex_frame_run(&f, clock_us());
    if (st < 0) { /* decidir: cerrar stream, salir, ... */ }
}
gex_frame_pause(&f);            /* opcional */
gex_frame_resume(&f, clock_us());
gex_frame_shutdown(&f);
```

Contadores de solo lectura: `frames`, `steps` (updates *entrados*: un update
que falla cuenta como entrado), `pre_frames`, `audio_ticks`, `aborted`,
`clock_backwards`, `last_steps`, `last_status`, `last_audio`, más los del
núcleo (`core.steps_total`, `core.presents_total`, `core.discards_total`,
`core.max_n`).

## 9. Pruebas y resultados exactos

```powershell
cd port
.\build.ps1 clean
.\build.ps1 all        # 69 305 + 60 052 + 215 + 365 + 50 checks, 0 fallos
.\build\test_frame.exe
.\build\framedemo.exe --frames 4
.\build\framedemo.exe --decoupled --frames 3 --dt 60000 --audio
```

```powershell
cd tools\m13-core
clang -std=c11 -Wall -Wextra -Wpedantic -O2 -I..\..\port\include `
    -o ..\..\port\build\test_m13_core.exe test_m13_core.c `
    ..\..\port\src\core\loop_core.c
..\..\port\build\test_m13_core.exe
```

Salida medida en esta máquina (Windows 11, clang 22.1.8,
`-std=c11 -O2 -Wall -Wextra -Wpedantic`, **sin avisos**):

```text
69305 checks, 0 failures
60052 checks, 0 failures
215 checks, 0 failures
365 checks, 0 failures
device checks: 50, 0 failures
120 checks, 0 failures        <- tools/m13-core contra el núcleo del port
```

`framedemo --frames 4` (modo original 1:1, reloj guionizado de 40 ms):

```text
config:  original 1:1, step=40000 us, max_dt=250000 us, max_steps=5
frame  0: now=   40000 us  steps=1  present
frame  1: now=   80000 us  steps=1  present
frame  2: now=  120000 us  steps=1  present
frame  3: now=  160000 us  steps=1  present
totals:  frames=4 steps=4 presents=4 audio_ticks=0 discards=0 aborted=0 backwards=0
```

`framedemo --decoupled --frames 3 --dt 60000 --audio` (catch-up + audio):

```text
config:  decoupled, step=16667 us, max_dt=250000 us, max_steps=5
frame  0: now=   60000 us  steps=3  present
frame  1: now=  120000 us  steps=4  present
frame  2: now=  180000 us  steps=3  present
totals:  frames=3 steps=10 presents=3 audio_ticks=3 discards=0 aborted=0 backwards=0
audio:   3 halves submitted, 0 stops, finished=0
```

Los pasos por frame (3, 4, 3) muestran el acarreo fraccional: 180 ms / 16,667 ms
= 10 pasos repartidos en 3 presentaciones.

`test_frame` cubre, con reloj inyectado y backend de audio simulado:

| Prueba | Qué fija |
| --- | --- |
| T1 original | orden `pre → update → audio → present`, 1 update y 1 present por frame, 1 tick por frame |
| T2 catch-up | 60 ms ⇒ 3 updates, 1 present, 1 tick, acarreo 9 999 µs |
| T3 descarte | 250 ms ⇒ 5 pasos, 1 descarte, `acc = 0` |
| T4 clamp | 10 s ⇒ mismo resultado que 250 ms; `t_prev` avanza |
| T5 reloj invertido | 0 updates, 1 present, `WARN_CLOCK`, `t_prev` intacto |
| T6 error en pre-frame | 0 updates, 0 ticks, 0 presents |
| T7 error en update | 2 updates entrados, 0 ticks, 0 presents, `acc = 0` |
| T8 error de audio | `ERR_AUDIO` + `last_audio = ERR_BACKEND`, sin present, recuperación sin repetición |
| T9 error en present | el tick ya ocurrió, `frames` no aumenta |
| T10 pausa/reanudación | ninguna fase mientras dura; 20 ms tras reanudar ⇒ 1 paso, 0 descartes |
| T11 ciclo de vida | rechazo sin inicializar, config inválida, cierre idempotente, reinicialización |
| T12 independencia | 400 ms a 25 Hz y a 200 Hz ⇒ los mismos 23 pasos; 10 y 80 ticks |

## 10. Qué es real y qué es de prueba

- **Real:** el reloj inyectado, el acumulador (el mismo del prototipo), el
  orden de fases, la llamada al planificador de audio de I-2b sobre un
  `gex_audio_stream` real, el clamp, el catch-up, los descartes, el reloj
  invertido, la pausa y el ciclo de vida.
- **De prueba:** `pre_frame`, `update` y `present` son stubs que graban
  llamadas. `framedemo` imprime; no hay lógica del juego, ni ventana, ni
  entrada, ni renderer, ni swap.
- **No afirmado:** que el juego original se ejecute, que exista un build
  jugable o que el frame loop reproduzca el trabajo de 14 ms medido dentro del
  frame original. Lo que se reproduce es la *secuencia* y el *tiempo lógico*,
  no el contenido.

## 11. Discrepancias original ↔ port

| ID | Original | Port | Motivo |
| --- | --- | --- | --- |
| F1 | el frame step drena la cola en bucle (`0x0043EA51`) y el swap bloquea ~23 ms | `present` es un callback único; no hay drenaje ni swap | no existe renderer todavía (I-4) |
| F2 | `0x0046300C` tras el swap | no modelada | semántica sin identificar; no se inventa |
| F3 | el update calcula `(GetTickCount()-inicio)/16.666` dentro de sí mismo | el reloj se inyecta al bucle; el sello no se implementa | no hay objeto de juego; el sello pertenece al update real (I-4+) |
| F4 | el juego es *present-bound*: un frame lento ralentiza la simulación | modo desacoplado con catch-up y descarte | objetivo de M-13; es opcional y `original_mode` es el preset por defecto |
| F5 | no hay manejo de errores de DirectSound en el frame loop | un error de audio aborta el frame y vacía el acumulador | política del port, explícita y probada |
| F6 | no hay pausa del frame loop | `gex_frame_pause/resume` | herramienta del port; reancla el reloj |

## 12. Límites y pendientes para I-4

- **No hay ventana, entrada ni renderer.** `present` es un stub; I-4 conecta
  ahí el renderer nulo y el swap.
- **No hay lectura de reloj real.** Falta el proveedor de `now_us` (p. ej.
  `QueryPerformanceCounter` en Windows) con la misma interfaz inyectable.
- **El render condicional (`[obj+0x4E38]`) no se modela**: pertenece al update
  original, no al bucle.
- **El drenaje de cola del swap** (`_grBufferNumPending`) no se modela: sin
  Glide no hay cola.
- **`STEP` definitivo pendiente de E-2.** Los presets son una decisión
  documentada, no una constante recuperada.
- **Sin hilos y sin mezcla multi-voz**, igual que I-2b.
- **Sin `.sag` reales**: el audio de las pruebas y de la demo es sintético.
