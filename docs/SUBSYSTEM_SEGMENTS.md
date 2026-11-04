# SUBSYSTEM_SEGMENTS — primera segmentación por subsistemas (EU)

> **Nivel de evidencia:** los **nombres de subsistema son HYPOTHESIS**. Los
> inicios `basis=call` están **CONFIRMADOS** por el desensamblado; los
> `basis=prologue` son **HIPÓTESIS estructurales** y se marcan como tales. Las
> aristas, los imports y las anclas son **CONFIRMADOS** contra los artefactos.
> Ningún nombre afirma semántica original: el binario está *stripped* y no se ha
> recuperado ningún símbolo.
>
> **Fecha de publicación:** 2026-10-30. **Artefactos de entrada:** mapa
> estático generado el 2026-10-25. **Binario:** `GEX3D.EXE` EU, MD5
> `692b12825003417cc4a5a9d4db13eebe`, SHA-256
> `d0932549613f9c08fb895c5f8a6461ce044e1863b4cba42da4715c2b41e73886`.
> **Entrada:** `analysis/EU/{functions,calls}.tsv`, `anchors.json`, `summary.json`.
> **Reproducible con:** `node tools/gexmap/cli.mjs segments`.
>
> Este documento no cambia ByteMatch: sigue **0,00 %**. No es el *splitter* ni un
> `symbols.txt`, y no clasifica las 2 655 filas del mapa.

## 1. Qué es y qué no es

Es una **primera rebanada defendible**: 65 inicios de función agrupados en siete
grupos, cada uno con la evidencia observable que lo sostiene, su confianza y lo
que el mapa **no** permite afirmar. No es una partición del programa: hay 20
aristas directas entre miembros de grupos distintos, y muchas funciones quedan
fuera a propósito.

Lo que **sí** demuestra:

- qué inicios existen y quién llama a quién (aristas directas del desensamblado);
- qué imports alcanza cada función, directa o transitivamente;
- qué anclas documentales caen dentro de qué función;
- qué miembros están conectados por una cadena de llamadas directas.

Lo que **no** demuestra:

- que una función se ejecute, ni en qué orden (no hay traza dinámica);
- qué hace por dentro (no hay decompilación ni tipos);
- que dos funciones con la misma API pertenezcan a la misma unidad de compilación;
- el nombre original de nada.

## 2. Método

### 2.1 Capas de evidencia

Cada código se recalcula desde los artefactos salvo `S` y `R`, que son lecturas
manuales del binario original y quedan registradas con su detalle.

| Código | Significado | ¿Automático? |
|---|---|---|
| `I` | import de la familia del segmento referenciado dentro de la extensión de la función | sí (`functions.tsv`) |
| `T` | llamada directa a un stub `jmp [IAT]` de la familia del segmento | sí (`calls.tsv`) |
| `A` | ancla documental dentro de la función | sí (`anchors.json`) |
| `G` | distancia 1 —o 2 a través de un miembro del mismo segmento— a un ancla o a una función con `I`/`T` | sí (grafo) |
| `O` | ≥4 callees directos compartidos con una función anclada | sí (grafo) |
| `S` | referencia a un string localizado en `.rdata`, dentro de la extensión | manual, con dirección y texto |
| `R` | secuencia de llamadas leída en desensamblado crudo | manual, con nota |

`W` no es una evidencia sino un hecho del grafo: **alcanzable desde `WinMain`
(`0x00463278`) solo con aristas directas**. `W=no` **no** significa código
muerto: los *callbacks* (ventana, diálogo, temporizadores), las llamadas
indirectas y el despacho por datos son invisibles a esta medida. Por eso hay
procedimientos de diálogo con 0 llamadores directos.

### 2.2 Confianza derivada

La confianza **no se declara a mano**: se calcula y el validador rechaza
cualquier discrepancia con la declarada.

| Nivel | Condición |
|---|---|
| **ALTA** | `I` o `T` de la familia + `W=sí` + inicio no meramente `prologue` |
| **MEDIA** | ancla, string, solapamiento `O`, `I`/`T` que `WinMain` no alcanza por llamadas directas, o `G` a distancia 1 |
| **BAJA** | `basis=prologue`, o nada más allá de `G` a distancia 2 / `W` |

Consecuencia deliberada: un candidato por prólogo **nunca** es ALTA aunque tenga
la API delante, porque el inicio en sí no está confirmado; y una función con API
pero sin camino directo desde `WinMain` baja a MEDIA.

### 2.3 Validación

`segments` recompone cada miembro desde `analysis/EU/` y falla si:

- una dirección no existe, es un stub o está repetida;
- una evidencia declarada no se sostiene;
- una familia declarada por el segmento no aparece en ningún miembro;
- la confianza declarada no coincide con la derivada.

Resultado actual: **0 problemas**, 65 miembros, 20 aristas entre segmentos.

## 3. Resumen

| ID | Hipótesis de subsistema | Miembros | ALTA | MEDIA | BAJA | Familias de API | Anclas dentro |
|---|---|---:|---:|---:|---:|---|---:|
| S1 | paso de frame y tiempo | 8 | 4 | 3 | 1 | KERNEL32, GLIDE2X | 8 |
| S2 | render Glide | 10 | 10 | 0 | 0 | GLIDE2X | 3 |
| S3 | audio, mezcla y voz | 9 | 4 | 5 | 0 | DSOUND, WINMM | 3 |
| S4 | cinemáticas y bucle de película | 12 | 5 | 7 | 0 | GLIDE2X, USER32 | 3 |
| S5 | entrada (teclado y joystick) | 9 | 4 | 4 | 1 | USER32, WINMM | 0 |
| S6 | shell Win32 (ventana, mensajes y arranque) | 11 | 5 | 3 | 3 | USER32, GDI32, GLIDE2X, KERNEL32 | 2 |
| S7 | configuración en el registro | 6 | 3 | 0 | 3 | ADVAPI32 | 1 |
| **Total** | | **65** | **35** | **22** | **8** | | **20 de 24** |

Los 65 miembros son el **2,58 %** de los 2 523 inicios de juego (2 655 filas
incluyen los 132 stubs). 20 de las 24 anclas documentales caen dentro de un
miembro; las cuatro restantes son la entrada PE, el handler de pantalla TV
`0x00418FF8` (0 llamadores), y dos funciones que quedan fuera de la rebanada
(`0x00423AFE`, `0x00471E5F`).

## 4. Detalle por segmento

Cada tabla es la salida validada de `node tools/gexmap/cli.mjs segments`. La
columna **W** es la alcanzabilidad por llamadas directas desde `WinMain`; la
columna **Evidencia** usa los códigos de §2.1.

### S1 — HYPOTHESIS: paso de frame y tiempo

8 miembros · ALTA 4 · MEDIA 3 · BAJA 1 · familias: KERNEL32, GLIDE2X

| Dirección | Bytes | Base | Llamadores | W | Evidencia | Confianza | Papel observado |
|---|---:|---|---:|---|---|---|---|
| `0x0043EA36` | 58 | call | 1 | sí | A G I T | ALTA | frame step: input -> update -> espera de cola -> swap(3) |
| `0x0043E1BA` | 1155 | call | 1 | sí | A G I | ALTA | update por frame; sella elapsed/16.666 en +0x10C |
| `0x0043B11D` | 2512 | call | 1 | sí | A G I | ALTA | init temporal por nivel; t0 = GetTickCount |
| `0x00412F8B` | 1764 | call | 1 | sí | A G | MEDIA | cuenta atras por frame (+0x4BC--); sin import directo |
| `0x00461FBD` | 875 | call | 1 | sí | A G I T | ALTA | bucle alternativo con swap(2) durante init |
| `0x0046300C` | 5 | call | 2 | sí | G | MEDIA | hoja de 5 B llamada por los dos bucles; rol sin determinar |
| `0x004519DE` | 136 | call | 1 | sí | A | MEDIA | delta -> tabla -> WORD; ancla R2 en +46 |
| `0x00424A93` | 380 | prologue | 0 | no | G O | BAJA | candidato por prólogo, 0 llamadores; comparte 8 callees con el update (variante no llamada) |

Sostén: el frame step confirmado llama a entrada, update, espera y swap; el
update y la init leen `GetTickCount`; las anclas M-13 caen dentro de estos
inicios. El **rol** de `0x0046300C`, `0x00412F8B` y `0x004519DE` es una
hipótesis de posición, no una semántica leída. `0x00424A93` no lo llama nadie en
el grafo directo: se publica como variante **no** ejecutada, no como código vivo.

### S2 — HYPOTHESIS: render Glide

10 miembros · ALTA 10 · MEDIA 0 · BAJA 0 · familias: GLIDE2X

| Dirección | Bytes | Base | Llamadores | W | Evidencia | Confianza | Papel observado |
|---|---:|---|---:|---|---|---|---|
| `0x0043DC3A` | 1408 | call | 2 | sí | A G I T | ALTA | envio de render + drenaje de cola (grBufferNumPending) |
| `0x0046558A` | 17068 | call | 1 | sí | G I T | ALTA | mayor consumidor Glide: 10 APIs, 17 068 B |
| `0x00461C32` | 473 | call | 1 | sí | G I T | ALTA | descarga de mipmaps de textura |
| `0x004329EF` | 462 | call | 1 | sí | G I T | ALTA | carga de textura 3DF (gu3dfLoad/gu3dfGetInfo) |
| `0x00432BBD` | 532 | call | 1 | sí | G I T | ALTA | triángulos + estado de color/alpha |
| `0x00432DD1` | 264 | call | 1 | sí | G I T | ALTA | triángulos + estado de color/alpha (variante corta) |
| `0x00424C0F` | 121 | call | 2 | sí | A G I T | ALTA | swap(1) de la ruta de arranque; ancla en +112 |
| `0x00424D82` | 648 | call | 1 | sí | G I T | ALTA | grSstQueryHardware: sonda de hardware Glide |
| `0x00473238` | 504 | call | 1 | sí | G I T | ALTA | descarga de mipmap y de nivel parcial |
| `0x00469836` | 122 | call | 3 | sí | G I T | ALTA | grTexNCCTable: tabla de paleta |

Sostén: son las únicas funciones de juego que llaman directamente a Glide, y
cada una declara la API concreta. `0x0046558A` concentra `_grDrawTriangle`,
`_grTexSource`, `_grAlphaCombine`, `_grConstantColorValue4`, `_grChromakeyMode`,
`_grDrawLine`, `_grDrawPlanarPolygonVertexList`, `_grTexDownloadTable`, entre
otras. Que sean «render» es la hipótesis; **qué** dibuja cada una no se afirma.

### S3 — HYPOTHESIS: audio, mezcla y voz

9 miembros · ALTA 4 · MEDIA 5 · BAJA 0 · familias: DSOUND, WINMM

| Dirección | Bytes | Base | Llamadores | W | Evidencia | Confianza | Papel observado |
|---|---:|---|---:|---|---|---|---|
| `0x00538551` | 467 | call | 2 | sí | G I T | ALTA | **único** DirectSoundCreate vivo: dispositivo + buffer primario (`DSBCAPS_PRIMARYBUFFER`) |
| `0x00538CB0` | 107 | call | 1 | no | G I T | MEDIA | segundo DirectSoundCreate; su único llamante (`0x00538FD5`) no tiene llamantes; no crea buffer primario |
| `0x0053B10B` | 400 | call | 1 | sí | G | MEDIA | init del sonido: llama a `0x00538551` una sola vez y pone `[0x5AC5F0]=1` |
| `0x0053FCB7` | 122 | call | 4 | sí | A G | MEDIA | housekeeping de voz (ancla); no alcanza APIs del sistema |
| `0x0053FF0D` | 297 | call | 1 | sí | A G S | MEDIA | ruta `<InstallDir>\audio\voiceuk\%s.sag` (string verificado) |
| `0x0053FE80` | 141 | call | 2 | sí | A G | MEDIA | snapshot del cociente (ancla R1 en +84); llama a la ruta de voz |
| `0x0053D6C1` | 348 | call | 1 | sí | G I | ALTA | auxGetDevCaps/auxGetNumDevs/auxGetVolume |
| `0x0053DE1A` | 105 | call | 5 | sí | G I | ALTA | auxSetVolume |
| `0x0053D822` | 320 | call | 2 | sí | G I | ALTA | auxGetVolume + mciSendCommandA (frontera audio/cine) |

Sostén: dos rutas independientes. DirectSound aparece en `0x00538551` y en
`0x00538CB0`; WinMM aparece en el control de volumen auxiliar (`0x0053D6C1`,
`0x0053DE1A`) y en MCI (`0x0053D822`). La voz se sostiene en el ancla M-13 de
`0x0053FCB7`, en el string de `0x0053FF0D` y en la llamada de `0x0053FE80`.

El detalle de las cadenas está en
[`docs/AUDIO_INIT_PATHS.md`](AUDIO_INIT_PATHS.md): `0x00538551` tiene
exactamente dos llamantes (`0x00538731`, que crea el único buffer secundario de
streaming, y `0x0053B10B`, el envoltorio de init) y los dos están conectados a
la entrada por llamadas directas. `0x00538731` no es miembro de S3 (el mapa lo
deja fuera de la tabla), pero es quien cierra la cadena.

Incertidumbre: `0x0053FF0D` y `0x0053FCB7` **no alcanzan ninguna API del
sistema** ni a profundidad 12; su asignación es documental y de grafo, no de
imports. `0x0053D822` es la frontera audio/cine: usa MCI, que también aparece en
la cadena de película. No se separa aquí porque su volumen auxiliar es audio.

### S4 — HYPOTHESIS: cinemáticas y bucle de película

12 miembros · ALTA 5 · MEDIA 7 · BAJA 0 · familias: GLIDE2X, USER32

| Dirección | Bytes | Base | Llamadores | W | Evidencia | Confianza | Papel observado |
|---|---:|---|---:|---|---|---|---|
| `0x00537BB0` | 11 | call | 2 | sí | A G | MEDIA | wrapper de cine (ancla E-3): GetTickCount |
| `0x00537BBB` | 272 | call | 1 | sí | G I | ALTA | bucle de mensajes de cine: BeginPaint/EndPaint/PeekMessageA |
| `0x00537D70` | 249 | call | 1 | sí | G I T | ALTA | volcado a framebuffer lineal (grLfbWriteRegion) |
| `0x00537E69` | 17 | call | 1 | sí | A G I T | ALTA | swap(1) del wrapper de cine (ancla en +5) |
| `0x00537E7A` | 34 | call | 1 | sí | G | MEDIA | LFB + swap encadenados; sin import directo |
| `0x00537EA6` | 43 | call | 1 | sí | A G I T | ALTA | grSstWinOpen 640x480@60Hz para cine (ancla) |
| `0x00537ED1` | 326 | call | 1 | sí | G I T | ALTA | entrada de cine: GetTickCount + grGlideGetState |
| `0x00538017` | 41 | call | 1 | sí | G | MEDIA | envoltorio de 41 B llamado desde WinMain |
| `0x0053A5A2` | 89 | call | 1 | sí | G | MEDIA | prepara el bucle: audio, estado Glide y 0x0053A5FB |
| `0x0053A5FB` | 348 | call | 1 | sí | G | MEDIA | bucle de película: dibujo, swap, mensajes, tick |
| `0x0053A757` | 129 | call | 1 | sí | G | MEDIA | paso auxiliar del bucle; vuelve a 0x00537BB0 |
| `0x0053A43A` | 326 | call | 2 | sí | G R | MEDIA | función recursiva invocada desde el bucle de película; sin API alcanzable |

Sostén: cadena verificada `WinMain (0x00463278) -> 0x00538017 -> 0x00537ED1 ->
0x0053A5A2 -> 0x0053A5FB`; el bucle pinta con LFB, hace swap, bombea mensajes y
avanza por tick. `0x00537EA6` abre una superficie Glide de 640x480, distinta de
la de 512x384 del juego (`0x0046324D`), lo que encaja con una ruta de vídeo
aparte; sigue siendo **hipótesis de posición**.

Incertidumbre: el segmento declara solo Glide y USER32 como familias directas.
WinMM y DirectSound se alcanzan de forma indirecta (profundidad 3–6) por la
música de la película; no se acreditan como API directa del segmento.

### S5 — HYPOTHESIS: entrada (teclado y joystick)

9 miembros · ALTA 4 · MEDIA 4 · BAJA 1 · familias: USER32, WINMM

| Dirección | Bytes | Base | Llamadores | W | Evidencia | Confianza | Papel observado |
|---|---:|---|---:|---|---|---|---|
| `0x0043F80D` | 178 | call | 4 | sí | G | MEDIA | paso de entrada del frame; sin import directo |
| `0x0043EA70` | 1019 | call | 1 | sí | G | BAJA | 1019 B llamada por el paso de entrada; sin API ni ancla |
| `0x0043F51D` | 737 | call | 2 | sí | G I | ALTA | joyGetPosEx (sondeo de joystick) |
| `0x0043EE6B` | 440 | call | 1 | sí | G I | ALTA | joyGetDevCapsA + joyGetPosEx (capacidades) |
| `0x004403D6` | 686 | call | 1 | no | G I | MEDIA | VkKeyScanA; sin camino directo desde WinMain (callback/indirecta) |
| `0x0043FD44` | 455 | call | 1 | sí | I | ALTA | VkKeyScanA |
| `0x0043FB8D` | 439 | call | 1 | sí | I | ALTA | VkKeyScanA |
| `0x00440CD1` | 168 | call | 2 | no | G I | MEDIA | VkKeyScanA; sin camino directo desde WinMain |
| `0x00440D79` | 256 | call | 1 | no | I | MEDIA | VkKeyScanA; sin camino directo desde WinMain |

Sostén: el frame step confirmado llama a `0x0043F80D` antes del update. De ahí
cuelgan el sondeo de joystick (`joyGetPosEx`, `joyGetDevCapsA`) y las
conversiones de tecla con `VkKeyScanA`. `0x0043FD44` y `0x0043FB8D` son
alcanzables desde `WinMain`; los otros tres usos de `VkKeyScanA` no.

Incertidumbre: `0x0043EA70` (1 019 B) no tiene API, ancla ni solapamiento
suficiente; se queda en **BAJA** y solo se afirma que cuelga del paso de
entrada. Los tres `VkKeyScanA` sin camino directo podrían ser *callbacks* o una
ruta alternativa; el mapa no lo decide.

### S6 — HYPOTHESIS: shell Win32 (ventana, mensajes y arranque)

11 miembros · ALTA 5 · MEDIA 3 · BAJA 3 · familias: USER32, GDI32, GLIDE2X, KERNEL32

| Dirección | Bytes | Base | Llamadores | W | Evidencia | Confianza | Papel observado |
|---|---:|---|---:|---|---|---|---|
| `0x00463278` | 2413 | call | 1 | sí | A G I T | ALTA | WinMain: init Glide, ventana, bucle y registro |
| `0x0046324D` | 43 | call | 1 | sí | A G I T | ALTA | grSstWinOpen 512x384@75Hz (ancla) |
| `0x00463011` | 128 | call | 7 | sí | G I T | ALTA | apagado: DestroyWindow + MessageBoxA + grGlideShutdown |
| `0x00463091` | 238 | call | 2 | sí | G I | ALTA | registro de clase y CreateWindowExA |
| `0x0046317F` | 80 | prologue | 0 | no | G I | BAJA | candidato por prólogo: Sleep/DispatchMessageA/PeekMessageA (0 llamadores) |
| `0x00463C4E` | 407 | prologue | 0 | no | G I | BAJA | candidato por prólogo: DialogBoxParamA/DefWindowProcA (0 llamadores) |
| `0x00463DE5` | 91 | prologue | 0 | no | I | BAJA | candidato por prólogo: EndDialog (0 llamadores) |
| `0x00463205` | 72 | call | 2 | sí | G I | ALTA | WaitMessage + ruta de MCI/ventana |
| `0x00463BE5` | 15 | call | 1 | sí | G | MEDIA | 15 B entre el registro de clase y WinMain; rol sin determinar |
| `0x00463BF4` | 45 | call | 1 | no | G I T | MEDIA | CloseWindow + ShowCursor + grSstControl; solo alcanzable desde el diálogo |
| `0x00463C21` | 45 | call | 1 | no | G I T | MEDIA | BringWindowToTop + ShowCursor + grSstControl; solo desde el diálogo |

Sostén: `WinMain` (alcanzado desde la entrada PE, arista confirmada) inicializa
Glide, registra la clase, crea la ventana (`CreateWindowExA`), bombea mensajes y
apaga (`DestroyWindow`, `MessageBoxA`, `grGlideShutdown`). Los tres candidatos
por prólogo tienen APIs de shell pero **cero llamadores directos**: encajan con
procedimientos de ventana/diálogo invocados por USER32, no con código muerto
demostrado. `0x00463BF4` y `0x00463C21` solo se alcanzan desde el diálogo.

Incertidumbre: `0x00463BE5` (15 B) no tiene API; su papel es desconocido.

### S7 — HYPOTHESIS: configuración en el registro

6 miembros · ALTA 3 · MEDIA 0 · BAJA 3 · familias: ADVAPI32

| Dirección | Bytes | Base | Llamadores | W | Evidencia | Confianza | Papel observado |
|---|---:|---|---:|---|---|---|---|
| `0x00442EEC` | 49 | call | 4 | sí | G I | ALTA | RegCreateKeyA |
| `0x00442F1D` | 36 | call | 4 | sí | G I | ALTA | RegCloseKey |
| `0x00442F41` | 84 | call | 1 | sí | A G I | ALTA | RegQueryValueExA: InstallDir + CDDriveName (ancla) |
| `0x00442F95` | 56 | prologue | 0 | no | G I | BAJA | candidato por prólogo: RegSetValueExA (0 llamadores) |
| `0x00442FCD` | 103 | prologue | 0 | no | G I | BAJA | candidato por prólogo: RegQueryValueExA (0 llamadores) |
| `0x00443034` | 55 | prologue | 0 | no | G I | BAJA | candidato por prólogo: RegSetValueExA (0 llamadores) |

Sostén: son los seis únicos usos de ADVAPI32 en el mapa. `WinMain` llama a
`0x00442F41`, que consulta `InstallDir` y `CDDriveName` (strings verificados
dentro de su extensión) y usa `RegCreateKeyA`/`RegCloseKey`. Los tres candidatos
por prólogo escriben o leen el registro pero no tienen llamadores directos: se
publican como **HYPOTHESIS**, no como rutas activas.

## 5. Recorrido verificado del frame

Leído en desensamblado crudo **y confirmado en ejecución** (2026-10-30; método,
recuentos y evidencia en `docs/FRAME_STEP_TRACE.md`):

```text
WinMain 0x00463278
  └─ 0x0043EA36  frame step (58 B)                     ret 0x00463AAF
       ├─ call 0x0043F80D   entrada del frame            (S5)  ret 0x0043EA42
       ├─ call 0x0043E1BA   update por frame (24 callees) (S1)  ret 0x0043EA4E
       │    └─ call 0x0043DC3A  render (si WORD [obj+0x4E38]==0) ret 0x0043E5C7
       │         └─ call 0x0054053E  _grBufferNumPending@0       ret 0x0043E0C5
       ├─ call 0x0054053E   _grBufferNumPending@0  ─┐            ret 0x0043EA56
       │    (bucle: espera a que la cola baje)       │
       ├─ push 3; call 0x005404F6  _grBufferSwap@4  ─┘            ret 0x0043EA63
       ├─ call 0x0046300C   hoja de 5 B                           ret 0x0043EA68
       └─ ret
```

Los stubs son `jmp dword ptr [IAT]` reales: `0x005404F6 -> [0x0065F4C8]` y
`0x0054053E -> [0x0065F4F0]`. El ancla documental `0x0043EA51` describía solo la
espera y el swap; el inicio real del frame step es `0x0043EA36` y incluye la
entrada, el update y la hoja final.

La traza fija tres hechos que el desensamblado por sí solo no daba:

- **Orden y frecuencia.** 1 723 de 1 724 frames repiten exactamente la secuencia
  anterior. Periodo p50 = **40 ms (25 FPS)**; el `swap` bloquea ≈23 ms y el
  trabajo de juego (entrada + update + render) ≈14 ms.
- **El render es condicional.** `0x0043E5B3` solo llama a `0x0043DC3A` si
  `WORD [obj+0x4E38] == 0`; si no, invierte `[obj+0x14]`. En la muestra siempre
  se tomó, pero la rama existe y la tabla de S2 no debe leerlo como incondicional.
- **El drenaje de cola aparece dos veces por frame**: dentro del render
  (`0x0043E0C5`) y en el bucle del frame step (`0x0043EA56`).

De los 24 callees directos del update, **23 se ejecutan exactamente una vez por
frame**; el único que no se ejecutó es el *forwarder* `0x0047344B` (sitio
`0x0043E31C`, 0/606). `0x0045E5C5` es el único destino con dos sitios de llamada
en el mismo frame. El detalle por sitio y la semántica mecánica demostrable de 19
destinos están en `docs/FRAME_STEP_TRACE.md` y en `tools/gexmap/frame.mjs`.

Cadena de cine (S4), también cruda:

```text
WinMain
  └─ 0x00538017
       └─ 0x00537ED1  (GetTickCount, grGlideGetState)
            └─ 0x0053A5A2
                 └─ 0x0053A5FB  bucle
                      ├─ 0x00537BBB  BeginPaint/EndPaint/PeekMessageA
                      ├─ 0x00537E7A -> 0x00537D70 (LFB) + 0x00537E69 (swap)
                      ├─ 0x00537BB0  tick
                      ├─ 0x0053A757
                      └─ 0x0053A43A  (recursiva)
```

Cadena de audio (S3), detalle en `docs/AUDIO_INIT_PATHS.md`:

```text
A) arranque
0x00541B20 -> 0x00463278 (WinMain) -> 0x0053D822 -> 0x0053B10B -> 0x00538551
                                                     (auxGetVolume + mciSendCommandA)
  0x00538551: DirectSoundCreate -> [0x0057C384]
              SetCooperativeLevel(+0x18, hwnd, 3)
              CreateSoundBuffer(+0x0C) -> [0x0057C378]  (DSBCAPS_PRIMARYBUFFER)
              SetFormat(+0x38, 0x005582E0)

B) reproduccion por nombre
0x00463278 -> 0x00538017 -> 0x00537ED1 -> 0x0053A5A2 -> 0x00538731 -> 0x00538551
  0x00538731: Stop(+0x48)/Release(+0x08) del anterior, carga el fichero (0x005388AC),
              parser de cabecera (0x0053899C), CreateSoundBuffer(+0x0C) -> [0x0057C374]
              (dwFlags=0xE0, 0x8000 B), Play(+0x30, 0, 0, 1), SetVolume(+0x3C)
  0x005380B3: mezclador GetCurrentPosition(+0x10) / Lock(+0x2C, 0x4000) / Unlock(+0x4C)

C) voz (prefijo parcial)
0x0053FF0D -> 0x00538731 -> 0x00538551

D) segunda creacion, sin llamantes
0x00538FD5 (0 llamantes) -> 0x00538CB0
  0x00538CB0: DirectSoundCreate -> [0x005847E4]; SetCooperativeLevel(+0x18) con un
              solo argumento de pila; sin buffer primario; [0x005847E0]=1

0x0053FE80 (R1) -> 0x0053FF0D  "%s\audio\voiceuk\%s.sag"
```

Las rutas A, B y C existen y son alcanzables por llamadas directas desde la
entrada; **ninguna se ha observado en ejecución** (ver
`docs/AUDIO_INIT_PATHS.md` §7).

Cadena de entrada (S5):

```text
0x0043EA36 -> 0x0043F80D
   ├─ 0x0043F51D  joyGetPosEx
   ├─ 0x0043EA70  (1019 B, sin API)
   ├─ 0x0043F94F -> 0x0043FD44 / 0x0043FB8D  (VkKeyScanA)
   ├─ 0x004328F1
   └─ 0x004334C8
```

## 6. Resultados negativos (lo que no se puede afirmar)

Estos negativos son parte del resultado: acotan la hipótesis y evitan convertir
una agrupación plausible en un hecho.

1. **El update no se puede descomponer por imports.** De sus 24 callees
   directos, solo 2 alcanzan una API del sistema a profundidad ≤3:
   `0x0043DC3A` (Glide@1) y `0x0043E719` (WinMM@3). **18 de los 24 no alcanzan
   ninguna API ni a profundidad 12**; los otros 4 la alcanzan a profundidad 4–8.
   Por eso 22 de los 24 siguen sin subsistema asignado. El mapa dice *quién
   llama a quién*, no *qué hace*.

   Los imports siguen sin clasificarlos, pero la lectura guiada más la traza
   dinámica ya dan **semántica mecánica demostrable a 19 de los 24** (17
   CONFIRMED y 2 HYPOTHESIS); los 5 restantes —`0x00405C41`, `0x00412F8B`,
   `0x0045EAC4`, `0x0046DD40`, `0x00537862`— siguen declarados sin semántica.
   Ver `docs/FRAME_STEP_TRACE.md` §5 y §6. Ninguno de esos 19 se ha movido de
   segmento: la semántica es mecánica, no una asignación de subsistema.
2. **`0x0053FCB7` (housekeeping de voz) no alcanza ninguna API** a profundidad
   12. Su pertenencia a audio se sostiene por el ancla M-13 y por sus cuatro
   llamadores (`0x00423AFE`, `0x00424191`, `0x00463278`, `0x0052B220`), no por
   imports. Es MEDIA, no ALTA.
3. **El segundo `DirectSoundCreate` no se alcanza por llamada directa.** `0x00538CB0`
   solo se alcanza a través de `0x00538FD5`, un candidato por prólogo con **0
   llamadores**; no hay camino de llamadas directas desde `WinMain`. La ruta
   activa es `0x00538551`, con **dos** llamantes (`0x00538731` y `0x0053B10B`)
   y ambos conectados a la entrada. Las dos creaciones solo comparten el thunk
   `0x005404F0` y la estructura `0x0057C330`; la de `0x00538CB0` no crea buffer
   primario, no comprueba el `HRESULT` de `SetCooperativeLevel` y le pasa **un
   solo argumento de pila** (falta `dwLevel`). No se afirma que sea código
   muerto (puede haber llamada indirecta), pero tampoco se presenta como la
   ruta activa. Ver `docs/AUDIO_INIT_PATHS.md` §3.4, §4.4 y §5.
4. **La clave de registro no está referenciada desde `.text`.** El string
   `SOFTWARE\Crystal Dynamics\Gex2\1.00` (VA `0x0055044C`) tiene **una sola**
   referencia de 4 bytes en todo el fichero, en `.rdata` (offset `0x14EC48`), no
   dentro de ninguna función mapeada. Por tanto no se atribuye a `0x00442F41`
   ni a `0x00442F95` sin más evidencia. `InstallDir` y `CDDriveName` sí están
   dentro de `0x00463278` (WinMain).
5. **`W=no` no es código muerto.** Solo 840 filas del mapa son alcanzables desde
   `WinMain` por llamadas directas. Los procedimientos de ventana y diálogo se
   invocan por puntero desde USER32; los *callbacks* y el despacho por datos
   quedan fuera del grafo. Tres usos de `VkKeyScanA`
   (`0x004403D6`, `0x00440CD1`, `0x00440D79`) están en esa situación.
6. **La segmentación no es una partición.** Hay 20 aristas directas entre
   miembros de grupos distintos; el acoplamiento es real, no un artefacto:

| Desde | Hacia | Pares |
|---|---|---|
| S6 (shell) | S1, S2, S3, S4, S5, S7 | 12 aristas (WinMain arranca todo) |
| S1 (frame) | S2, S5 | 4 aristas |
| S2 (render) | S6 | 2 aristas |
| S4 (cine) | S5 | 1 arista |
| S1 (variante) | S2 | 1 arista |

En concreto: `0x0043EA36 -> 0x0043F80D` (frame→entrada),
`0x0043E1BA -> 0x0043DC3A` (update→render),
`0x00463278 -> 0x0043EA36` (WinMain→frame),
`0x00463278 -> 0x00538017` (WinMain→cine) y
`0x00537BBB -> 0x0043F80D` (cine→entrada).

## 7. Cómo reproducir y validar

```bash
# Recalcular la segmentación y validar cada miembro/evidencia/confianza
node tools/gexmap/cli.mjs segments

# Misma validación, salida legible por máquina
node tools/gexmap/cli.mjs segments --json

# Invariantes de los artefactos de entrada (sin binario)
node tools/gexmap/cli.mjs check
node --test tools/gexmap/test.mjs
```

`segments` no escribe ficheros y no necesita el ejecutable: solo lee
`analysis/EU/`. Si los artefactos cambian (nuevo binario, nuevo método), la
validación falla antes de publicar una tabla incoherente.

## 8. Límites y siguientes pasos

Límites que siguen en pie:

- **La evidencia dinámica cubre el frame, no la segmentación entera.** El
  2026-10-30 se instrumentó el juego y se midió el frame step, el update y el
  render (`docs/FRAME_STEP_TRACE.md`): 2 326 frames, 25 sitios de llamada del
  update, 23 de 24 callees ejecutados exactamente una vez por frame. Eso valida
  *orden y frecuencia* de S1/S2, no la pertenencia de los 65 miembros: los otros
  58 siguen agrupados por grafo de llamadas, imports y anclas documentales, sin
  observación en ejecución.
- **La traza es de un nivel, bajo nGlide.** Frecuencias por frame sólidas; los
  tiempos absolutos (40 ms de periodo, 23 ms de swap) dependen del wrapper.
- **Tamaños = cota superior** (distancia al siguiente inicio).
- **Prólogos = HYPOTHESIS.** Los 8 miembros BAJA incluyen 7 candidatos por
  prólogo y una función confirmada sin API ni ancla (`0x0043EA70`); ninguno se
  promueve.
- **Sin unidades de compilación.** El *splitter* y el `symbols.txt` siguen
  pendientes; esta agrupación no los sustituye.
- **Sin nombres originales.** «frame step», «render», «audio» son etiquetas
  descriptivas de la evidencia, no símbolos recuperados.

Siguientes pasos concretos, en orden de coste/beneficio:

1. ~~Trazar el frame step en ejecución~~ **Hecho** (2026-10-30): orden de las
   cinco llamadas, 25 sitios del update medidos, `0x0046300C` localizado al final
   del frame tras el `swap`. Ver `docs/FRAME_STEP_TRACE.md`.
2. ~~Lectura guiada de `0x0043E719`~~ **Hecho**: es una máquina de estados sobre
   `WORD [obj+0x4E3E]` (0→4, 6 nada, 7→`0x423A6A`, otro→`0x43E63D`) con bits de
   entrada; no es ni entrada pura ni audio. Su sitio en S1 se mantiene y la
   hipótesis «candidato a mover entre S3 y S5» se retira.
3. ~~Resolver la rama de `0x00538FD5`/`0x00538CB0`~~ **Hecho (estático)**: la
   cadena existe pero no tiene llamantes; `0x00538551` es el único creador vivo
   del dispositivo y sus dos llamantes están conectados a la entrada. No se
   promueve a «código muerto» ni se da por cerrada S3: falta la ejecución y el
   barrido de tablas de punteros. Ver `docs/AUDIO_INIT_PATHS.md`. Siguiente
   paso concreto: recuperar la imagen de referencia (MD5 `692b1282…`) para
   instrumentar `0x00538551`/`0x00538731` con `tools/frame-trace`.
4. **Semántica de los 5 callees del update aún sin asignar**: `0x00405C41`
   (1 867 B), `0x00412F8B` (1 764 B), `0x0045EAC4` (1 454 B), `0x0046DD40`
   (2 307 B) y `0x00537862` (690 B). Entre los cuatro primeros está la mayor
   parte de la lógica del frame que sigue sin explicar.
5. **No clasificar el resto de las 2 458 funciones de juego** sin una prueba
   por grupo; repetir este método (evidencia + confianza + negativo) en la
   siguiente rebanada, no por dirección.

Cualquier promoción de un nombre de `HYPOTHESIS` a `CONFIRMED` exige una prueba
nueva —estática guiada o dinámica— que se registre en este documento y en
`CHANGELOG.md`.
