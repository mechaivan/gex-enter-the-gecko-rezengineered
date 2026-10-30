# Traza dinámica del frame step (EU)

> Estado: **medido en ejecución** el 2026-10-30 sobre el ejecutable original EU.
> Ámbito: el ciclo de frame `0x0043EA36`, sus cinco llamadas, los 25 sitios de
> llamada del update `0x0043E1BA` y la semántica mecánica demostrable de 19 de
> sus 24 destinos.
> Datos declarativos y validables: `tools/gexmap/frame.mjs`.
> Herramientas: `tools/frame-trace/`.

## 1. Qué se ha hecho

`docs/SUBSYSTEM_SEGMENTS.md` §8 dejaba como primer paso «trazar el frame step en
ejecución». Este documento lo cierra. No se ha inferido del grafo de llamadas: se
ha ejecutado el juego, se ha instrumentado con breakpoints de hardware y se ha
contado cada llamada por frame. Después se ha leído el desensamblado de cada
destino para separar **qué hace el código** (CONFIRMED) de **para qué sirve en el
juego** (HYPOTHESIS).

Resultado en una línea: el frame step es entrada → update → drenaje de la cola →
`swap(3)` → hoja, el periodo es 40 ms (25 FPS), y **23 de los 24 callees del
update se ejecutan exactamente una vez por frame**; el único que no se ejecuta es
el *forwarder* `0x0047344B`.

## 2. Método

### 2.1 Entorno

| Elemento | Valor |
|---|---|
| Ejecutable | `GEX3D.EXE` EU, 1 557 504 B, MD5 `692b12825003417cc4a5a9d4db13eebe` |
| Base de imagen | `0x00400000` (sin reubicación en las sesiones) |
| Copia de trabajo | `…\gex-run\GEX3D.EXE`, mismo MD5, con `LEVEL`, `MOVIE`, `AUDIO` |
| Datos del juego | registro `HKLM\SOFTWARE\WOW6432Node\Crystal Dynamics\Gex2\1.00`, `InstallDir` apuntando a la copia |
| Glide | nGlide (el juego no arranca sin `glide2x.dll`) |
| Host | Windows x64, proceso objetivo WOW64 (32 bits) |

Nunca se modificó la instalación original: se trabajó sobre una copia y el
registro se exportó antes de tocarlo.

### 2.2 Instrumentación

Se escribió un depurador mínimo (`tools/frame-trace/tracer.cs`, C# sobre
`WaitForDebugEvent`) que arranca el proceso con `DEBUG_ONLY_THIS_PROCESS` y usa
**breakpoints de hardware** (`Dr0`–`Dr3`, `Dr7` con `L0..L3` y `RW=00`,
`LEN=00`). Al saltar, lee `Wow64GetThreadContext`, registra `Eip`, `Esp`, la
dirección de retorno y los tres primeros argumentos, activa `RF` (`EFLAGS` bit
16) para que la instrucción se ejecute una vez antes de volver a disparar, y
continúa.

**Por qué no `INT3`.** El primer trazador parcheaba `0xCC` + single-step. El
juego muere antes del primer frame de juego con `0xC000041D` y `0xC0000005`
dentro de sus *callbacks* de tiempo: el reentro constante en el depurador rompe
el temporizador. Los breakpoints de hardware no tocan ni un byte y el juego corre
normal. El precio es **cuatro direcciones por sesión**, así que las tablas se
construyen encadenando sesiones.

**Ancla de frame.** Cada sesión incluye `0x0043E1BA` (entrada del update). Se
llama exactamente una vez por frame, así que el flujo de hits se parte en frames
con denominador exacto: `frames/total` no es una estimación.

### 2.3 Navegación

Las teclas se inyectan con `PostMessage` a la ventana del juego, descartando la
`D3DProxyWindow` que crea nGlide. `Escape` (`0x1B`) salta las FMV: el bucle de
cine `0x00537BBB` solo cuenta `WM_KEYDOWN` de `VK_ESCAPE` y clics, no `Enter`.
Después, `Enter`/flechas/espacio para menú y nivel.

### 2.4 Validación

```bash
node tools/gexmap/cli.mjs segments          # la segmentación sigue cuadrando
node tools/gexmap/cli.mjs check
node --test tools/gexmap/test.mjs           # 16 pruebas, incluidas 4 del frame
```

`frame.mjs` comprueba contra `analysis/EU/`: que cada sitio cae dentro del rango
de su llamante, que existe la arista, que `ret = site + 5`, que ningún sitio
declara más frames que la sesión y que ninguna afirmación CONFIRMED tiene menos
de dos líneas de evidencia con dirección.

## 3. Recorrido medido del frame

### 3.1 Orden

En 1 723 de 1 724 frames de una sesión larga (y en 1 103 de 1 104 en otra
independiente) el orden fue idéntico:

```text
0x00463AAF  WinMain
  └─ 0x0043EA36  frame step                          ret 0x00463AAF  arg 0x00648900
       ├─ 0x0043F80D  entrada del frame               ret 0x0043EA42  arg 0x00648900
       ├─ 0x0043E1BA  update por frame                ret 0x0043EA4E  arg 0x00648900
       │    └─ 0x0043DC3A  render                     ret 0x0043E5C7  arg 0x00648900
       │         └─ 0x0054053E  _grBufferNumPending@0 ret 0x0043E0C5
       ├─ 0x0054053E  _grBufferNumPending@0 (bucle)   ret 0x0043EA56
       ├─ 0x005404F6  _grBufferSwap@4  arg 3          ret 0x0043EA63
       └─ 0x0046300C  hoja de 5 B                     ret 0x0043EA68
```

Dos matices que el desensamblado añade y la traza confirma:

- El render **no** es incondicional. `0x0043E5B3-0x0043E5C7` llama a `0x0043DC3A`
  solo si `WORD [obj+0x4E38] == 0`; si no, hace `[obj+0x14] = 1 - [obj+0x14]`.
  En las 2 326 frames observadas `+0x4E38` valía 0 y el render se ejecutó
  siempre, pero la rama existe.
- El drenaje de cola aparece **dos veces** por frame: una dentro del render
  (`0x0043E0C5`) y otra en el bucle del frame step (`0x0043EA56`). El bucle
  repite `0x0043EA51` mientras `_grBufferNumPending@0` devuelva distinto de 0.

### 3.2 Tiempo

Con 1 723 intervalos entre inicios de frame: mínimo 37 ms, **p50 = 40 ms**,
p90 = 41 ms, máximo 246 ms (una pausa puntual). 40 ms ≈ **25 FPS**.

Desglose dentro de un frame típico (traceH, frame a `+0 ms`):

| Instante | Qué ocurre |
|---:|---|
| +0 ms | `0x0043EA36`; entrada y update en el mismo milisegundo |
| +1 ms | el render drena la cola (`0x0054053E` desde `0x0043E0C5`) |
| +14 ms | el frame step drena otra vez (`0x0054053E` desde `0x0043EA56`) |
| +14 ms | `_grBufferSwap@4(3)` |
| +37 ms | vuelve el swap; `0x0046300C` |

El `swap` bloquea ≈23 ms de los 40. El trabajo de juego (entrada + update +
render) ocupa ≈14 ms.

### 3.3 Lo que el update deja en el objeto global

Al final del update, `0x0043E5DD-0x0043E633`:

```text
call [0x0065F32C]            ; GetTickCount
sub  eax, [0x0057B5FC]       ; delta desde el inicio del nivel
mov  [ebp-0x10], eax         ; (con [ebp-0xC] = 0 -> entero de 64 bits)
fild qword [ebp-0x10]
fdiv qword [0x0054C060]      ; constante doble = 16.666 (leída del PE)
call 0x00540950              ; __ftol
mov  [obj+0x10C], eax        ; tiempo de nivel en unidades de 60 Hz
...
add  eax, 1
mov  [obj+0x104], eax        ; contador de frames
...
and  eax, 0xF7FFFFFF
mov  [obj+0xA0], eax         ; limpia el bit 0x08000000
```

Es decir: `+0x10C = (GetTickCount() − inicio_nivel) / 16,666`, `+0x104` es un
contador monótono de frames y `+0xA0` pierde el bit `0x08000000` cada frame. La
constante `16.666` se ha verificado leyendo el doble en `0x0054C060` del
ejecutable original.

Al principio del update, `0x0043E27D-0x0043E288` hace
`memset(obj+0x78, 0, 0x28)`: limpia 40 B de un bloque de trabajo cada frame.

## 4. Los 25 sitios de llamada del update

Medidos con el ancla `0x0043E1BA`. `frames` es el número de frames con al menos
un hit; `sesión` es el número de frames capturados en esa sesión. En las
sesiones m1–m7 la captura termina justo después del ancla y el último frame queda
abierto (de ahí `n-1/n`); descontando ese frame de cola, **todos** aparecen una
vez por frame completo. m8 termina después del último sitio y da `n/n`.

| Sitio (`call`) | Destino | Bytes | Llamadas | Frames | Sesión | Por frame |
|---|---|---:|---:|---:|---:|---|
| `0x0043E288` | `0x00540650` | 96 | 603 | 603 | 603 | 1,00 |
| `0x0043E2E9` | `0x00465530` | 90 | 599 | 599 | 600 | 1,00 |
| `0x0043E2F5` | `0x0043E719` | 339 | 599 | 599 | 600 | 1,00 |
| `0x0043E31C` | `0x0047344B` | 17 | **0** | **0** | 606 | **0,00** |
| `0x0043E328` | `0x00413D73` | 181 | 604 | 604 | 605 | 1,00 |
| `0x0043E36F` | `0x00436F6F` | 317 | 604 | 604 | 605 | 1,00 |
| `0x0043E396` | `0x0053F610` | 33 | 605 | 605 | 606 | 1,00 |
| `0x0043E3AC` | `0x0053F631` | 1 375 | 606 | 606 | 607 | 1,00 |
| `0x0043E3D5` | `0x0045EAC4` | 1 454 | 606 | 606 | 607 | 1,00 |
| `0x0043E3E6` | `0x0045E5C5` | 186 | 604 | 604 | 605 | 1,00 |
| `0x0043E3F2` | `0x005401CD` | 251 | 606 | 606 | 607 | 1,00 |
| `0x0043E418` | `0x0046FACE` | 840 | 606 | 606 | 607 | 1,00 |
| `0x0043E46B` | `0x00537862` | 690 | 602 | 602 | 603 | 1,00 |
| `0x0043E486` | `0x0041F714` | 456 | 606 | 606 | 607 | 1,00 |
| `0x0043E49E` | `0x00421668` | 136 | 599 | 599 | 600 | 1,00 |
| `0x0043E4AA` | `0x00460BF7` | 665 | 606 | 606 | 607 | 1,00 |
| `0x0043E4C5` | `0x00473471` | 17 | 605 | 605 | 606 | 1,00 |
| `0x0043E4F0` | `0x0045E5C5` | 186 | 604 | 604 | 605 | 1,00 |
| `0x0043E516` | `0x00412F8B` | 1 764 | 606 | 606 | 607 | 1,00 |
| `0x0043E525` | `0x0046DD40` | 2 307 | 602 | 602 | 603 | 1,00 |
| `0x0043E559` | `0x0046E643` | 492 | 603 | 603 | 603 | 1,00 |
| `0x0043E568` | `0x00405C41` | 1 867 | 606 | 606 | 607 | 1,00 |
| `0x0043E581` | `0x00421E5A` | 448 | 602 | 602 | 603 | 1,00 |
| `0x0043E5C2` | `0x0043DC3A` | 1 408 | 603 | 603 | 603 | 1,00 |
| `0x0043E5FF` | `0x00540950` | 48 | 606 | 606 | 607 | 1,00 |

Notas de la tabla:

- **24 destinos distintos, 25 sitios**: `0x0045E5C5` se llama dos veces por frame,
  desde `0x0043E3E6` y desde `0x0043E4F0`, y una tercera desde el render
  (`0x0043DE9D`).
- `0x00540950` (`__ftol`) es el callee con más tráfico del juego: en la
  sesión m3 acumuló **10 623 llamadas en 607 frames (≈17,5 por frame)** sumando
  todos sus llamantes (`0x4641DE`, `0x46B8A6`, ocho sitios de
  `0x46D78F`–`0x46D924`, …). Desde el update se llama una sola vez, al final.
- `0x00540650` (`memset`) tiene 32 llamadores mapeados y en m8 acumuló **1 584
  llamadas en 603 frames (2,63 por frame)**; los dos mayores son `0x0043FA9B`
  (840, dentro de la entrada del frame `0x0043F80D`) y el sitio del update
  `0x0043E28D` (603).

## 5. Semántica demostrada (CONFIRMED)

Solo mecánica de código, con la instrucción que la sostiene. Ninguna de estas
fichas afirma para qué sirve el dato en el juego.

| Dirección | Qué hace el código | Evidencia |
|---|---|---|
| `0x00540650` | `memset(dst, byte, count)`; devuelve el byte, no `dst` | `rep stosd` en `0x540691`, bucle de bytes en `0x540697` |
| `0x00540950` | `__ftol`: `double` de la FPU → entero 64 bits con truncamiento | `fnstcw`/`fldcw` con `or ah,0xC`, `fistp qword` |
| `0x00465530` | inicializa una lista sobre un array de nodos de 8 B (`+4` = anterior, `-1` en el primero) | `lea edx,[ecx+8*eax-8]`, bucle `[nodo+4]=nodo-8` |
| `0x00421668` | recorre 16 cadenas (índices 1,3,…,31, paso 8 desde `+0x10`) y llama `0x421283` por nodo | `i=1; i+=2; cmp i,0x1F`, `[ecx+8*eax+0x10]` |
| `0x0043E719` | máquina de estados sobre `WORD [obj+0x4E3E]`: 0→4, 6 nada, 7→`0x423A6A` y 0, otro→`0x43E63D`; bits de `+0xA0`/`+0x6FC0`/`+0x28+4`; fija `0x100` en `[obj+0xC]+0x10` | `cmp ecx,4/6/7` en `0x43E757-0x43E7BC`, `or ch,0x1` en `0x43E85C` |
| `0x0043E63D` | cancelación: estado 4→`0x422140`, estado = 0, limpia el bit `0x100` | `cmp ecx,4`, `mov word [obj+0x4E3E],0`, `and dh,-0x2` |
| `0x0047344B` | *forwarder* de 17 B → `0x452368` | `push [ebp+8]; call 0x452368; pop ebp; ret` |
| `0x00473471` | *forwarder* de 17 B → `0x455F1A` | `push [ebp+8]; call 0x455F1A; pop ebp; ret` |
| `0x0053F610` | acumulador 24.8: `[0x5AC610] += arg; [0x5AC614] = [0x5AC610] >> 8`. Llamado con `0x100`, suma **1,0 por frame** | `add eax,[ebp+8]`, `sar ecx,8`, `push 0x100` en `0x43E391` |
| `0x0053F631` | recorre registros de `0x2C` B desde `[arg+0xC]`; salta `[rec]&2`; `[rec+4]++`; `0x40223B(rec+8,arg2)`; si ≠0, `[rec]\|=1`; índice de frame = `((WORD[rec+0xA] + [0x5AC614]) % [[arg+0xC]+0x14]) * 6` | `imul eax,eax,0x2C` en `0x53F647`, `idiv` en `0x53F8C7`, `imul ecx,edx,6` en `0x53F8D7` |
| `0x0045E5C5` | dos pasadas sobre la misma lista: la 1.ª llama `0x45E480(arg,nodo,arg2)` por nodo con `+0x10&0x20` y `+0x10&0x400` limpio (o limpia `0x20` si tiene `0x400`); la 2.ª llama `0x45E265` por nodo con `0x20` | `and edx,0x400`/`and ecx,0x20` en `0x45E5E9-0x45E60C`, segundo bucle en `0x45E63A` |
| `0x00460BF7` | prueba de banderas por nodo (`0x40`, `0x200`, `0x8000000`, `0x10000000`, `+0xFC&0x1000`, `[0x5555DC]`, `[+0xF4]==5`) y copia tres `WORD` desde `[nodo+0x2C]+0x34/0x38/0x3C` o desde `[nodo+0x48]` | `0x460C19-0x460C87` (pruebas), `0x460C9D-0x460CFF` (copias) |
| `0x0041F714` | recorrido **por pares** de cadenas de nodos: 16 cadenas `0x10..0x1F` si `WORD [[0x551644]+0x6F36] != 1`, y dos emparejamientos sobre las tablas `0x54D4E0` y `0x54D520`/`0x54D500`; cada par va a `0x41F5C1` | `i=0x10..0x1F` en `0x41F737`, `[4*i+0x54D4E0]` en `0x41F79E`, `[4*eax+0x54D520]` en `0x41F862` |
| `0x00436F6F` | tres recorridos de listas: propaga `0x4`→`0x1` y limpia `0x2` en `[nodo+8]`; marca `[[nodo+0x14]+8]\|=1` y llama `[nodo+0x10]`; llama `0x461A36`/`0x461A00` por nodo sin `0x1` | `0x436F9C-0x436FEA`, `0x437013-0x437039`, `0x437073-0x437088` |
| `0x0046FACE` | asigna entradas de `0x20` B de un pool (`[arg+0x4C10]`…`[arg+0x4C14]`), rellena 8 `WORD` con `0x1000`, calcula un ángulo con dos llamadas a `0x463E40` (`+0x400`/`+0xC00`), aproxima distancia con `(30*max + 12*min) >> 5` y enlaza en `[arg+0x2C]` | `0x46FBA2-0x46FBB1` (pool), `0x46FC61-0x46FC72` (distancia), `0x46FD63-0x46FD75` (enlace) |
| `0x00421E5A` | elige bloque de transformación (`arg+0x28`, `arg+0x50`, global `0x64FE80`) según `[0x551644]+0x4C94&0x10` y `+0xA0&0x40000`, y despacha por `WORD +0x4E3E` a `0x42276C`/`0x422740`/`0x42216E`/`0x424191`/`0x423A6F` | `0x421E6C-0x421EA5` (selección), `0x421EC2-0x421FD4` (despacho) |
| `0x0046E643` | copia `WORD` de un registro temporal: si `WORD [arg2+0x12]==1`, `0x46E898(&tmp,arg2)` y `tmp[0]→[arg1]`, `tmp[2]→[arg1+6]`; otra rama llama `0x46999C` | `cmp ecx,1` en `0x46E650`, `0x46E65D`, `0x46E668-0x46E676` |

## 6. Semántica HYPOTHESIS (mecánica descrita, propósito no probado)

| Dirección | Lectura | Qué falta para subir a CONFIRMED |
|---|---|---|
| `0x00413D73` | si `[arg+0x50]` tiene `0x4000` llama `0x537B14`; si tiene `0xF0`, `0x537B65` y `0x53E98C(handle, +0x10)` para `[obj+0x480]`/`[obj+0x484]` | identificar qué son esos dos handles y qué hace `0x53E98C` con ellos |
| `0x005401CD` | dispara un id de la tabla `WORD 0x55A340` indexada por el byte `+0x6F71` (si `<0x1E` y `!= -1`) y otro por nodo con `[nodo+0x10]&0x200`; `0x540036` → `0x53FE80` (ruta `%s\audio\voiceuk\%s.sag`) | confirmar que la tabla es de cues de voz y no de otro recurso |

Los demás destinos siguen sin semántica asignada y así se declaran: `0x00405C41`,
`0x00412F8B`, `0x0045EAC4`, `0x0046DD40`, `0x00537862`.

## 7. Resultados negativos

1. **`0x0047344B` no se ejecuta.** El sitio `0x0043E31C` existe en el código y la
   arista está en el mapa, pero en 606 frames de gameplay no se tomó ni una vez.
   No se afirma que sea código muerto (puede depender de un modo o de una rama no
   alcanzada en este nivel); se afirma que **no se ejecutó** en la sesión.
2. **La rama de cancelación no se toma.** `0x0043E63D` (llamada desde
   `0x0043E719` cuando el estado no es 0/6/7), `0x00423AFE` y `0x00423A6A` dieron
   **0 hits en 611 frames** con el ancla incluida. En gameplay normal el estado
   `WORD +0x4E3E` se queda en los valores que no disparan esas ramas.
3. **`INT3` + single-step no es viable con este binario.** Produce
   `0xC000041D`/`0xC0000005` antes del primer frame de juego. No es un fallo del
   juego: es la instrumentación. Queda registrado para que nadie lo reintente
   como si fuera un problema del ejecutable.
4. **El render es condicional, aunque en la muestra siempre se tomó.** La traza
   no prueba que se llame siempre; prueba que se llamó en todas las frames
   observadas con `+0x4E38 == 0`.
5. **`0x0046300C` sigue sin rol.** La traza confirma que se ejecuta una vez por
   frame, al final, después del `swap`; no dice qué hace. Su cuerpo es de 5 B.
6. **Una sesión, un nivel, un wrapper.** Todo lo medido es un nivel concreto bajo
   nGlide. Las frecuencias por frame son sólidas (2 300+ frames, dos sesiones
   independientes con el mismo resultado); los tiempos absolutos dependen del
   wrapper.

## 8. Límites

- **Cuatro breakpoints por sesión**: las tablas se encadenan; cada fila declara
  su sesión para que el denominador sea auditable.
- **Solo se observa lo que se ejecuta**: una rama no tomada no deja rastro, y eso
  es un resultado, no un fallo.
- **El tamaño de las funciones es una cota superior** (distancia al siguiente
  inicio), igual que en el resto del mapa.
- **Sin nombres originales**: «frame step», «render», «update» son etiquetas
  descriptivas.
- **No se instrumentó el hilo de audio ni los *callbacks* de DirectSound**: las
  conclusiones son del hilo principal.

## 9. Cómo reproducir

```bat
:: 1. compilar el trazador (sin binarios en el repo)
%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /platform:x64 ^
  /optimize+ /out:tracer.exe tools\frame-trace\tracer.cs

:: 2. una sesión: ancla + tres sitios del update
tracer.exe C:\gex-run\GEX3D.EXE C:\gex-run trace.log 45000 ^
  hw:43e1ba,540650,465530,43e719 1B,1B,1B,1B,0D,0D,28,0D,0D,20,0D

:: 3. resumir por frame
node tools\frame-trace\summarize.mjs trace.log

:: 4. validar los datos publicados (sin binario ni ejecución)
node --test tools\gexmap\test.mjs
```

Los logs crudos no se publican: contienen rutas locales y no son necesarios para
verificar nada, porque `frame.mjs` fija los recuentos y las pruebas los validan.

## 10. Qué cambia en la segmentación

- **S1 deja de ser solo HYPOTHESIS de posición.** El orden de las cinco llamadas
  del frame step y la frecuencia de los 25 sitios del update están medidos. La
  *pertenencia* de cada función a S1 sigue siendo una agrupación; su *ejecución*
  ya no es una suposición.
- **El negativo «sin evidencia dinámica» de §8 desaparece** para el frame step.
- **`0x0043E719`** (antes «candidato a mover entre S3 y S5») queda como máquina de
  estados de acción: no es entrada ni audio.
- **`0x0046300C`** sigue sin rol, pero ya se sabe que se ejecuta al final de cada
  frame, después del `swap`.
- **`0x00412F8B`** se ejecuta una vez por frame; su lectura como «cuenta atrás»
  no se ha confirmado en esta sesión.

`docs/PROJECT_STATUS.svg` no se toca: mide el avance de MAIN (versión jugable),
no hitos de ingeniería inversa.
