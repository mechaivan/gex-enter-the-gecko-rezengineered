# S3 — Secuencia de cámara hasta el menú principal (Gex: Enter the Gecko)

> Ámbito: el comportamiento **del original** desde un arranque en frío hasta
> que el menú principal queda visible y estable sobre **MAP7**, medido con
> instrumentación de solo lectura (breakpoints de hardware + volcado de memoria
> por `ReadProcessMemory`) sobre una **copia de trabajo** del juego.
>
> Fecha de la captura: **2026-11-12**. Binario: `GEX3D.EXE` EU
> (`md5 692b12825003417cc4a5a9d4db13eebe`, el mismo de las capturas
> anteriores). Driver: **nGlide 2.10** (software, D3D9).

---

## 1. Objetivo y alcance

Reconstruir la secuencia de entrada al menú principal para poder reproducirla
en el port D3D11. Este documento **no implementa** nada en DX11: describe lo
medido, lo demuestra y lo que queda abierto.

Preguntas que se responden aquí:

1. ¿Qué ocurre entre el arranque en frío y el menú?
2. ¿Cómo se mueve la cámara durante esa transición y cómo termina?
3. ¿De dónde sale ese movimiento (rutina, secuencia predefinida, interpolación,
   temporización, cinemática de vídeo)?
4. ¿Qué datos hay que llevar al port?

---

## 2. Metodología

### 2.1 Aislamiento

- Copia de trabajo en `%TEMP%\opencode\gexrun` (ejecutable + `glide2x.dll` +
  `glide2x_gex_real.dll` + `LEVEL/` + `MOVIE/` + `AUDIO/` + `FONT.3DF` +
  `GEX.DFX`/`GEX.VFX`).
- **`C:\GEX_REZ` no se escribe nunca**: solo se lee.
- El ejecutable **no se modifica ni se parchea**: `md5` idéntico al original
  antes y después de cada captura.

### 2.2 Instrumentación

El trazador del propio repo (`tools/frame-trace/tracer.cs`): depurador de
Windows con **breakpoints de hardware** (`Dr0..Dr3`, sin tocar un byte del
código) y volcado de memoria **de solo lectura** (`ReadProcessMemory`).

| Opción | Uso en esta investigación |
|---|---|
| `hw:<dirs>` | hasta 4 direcciones de ejecución |
| `dumpabs:<base>:<offs>` | dwords de una dirección absoluta (bloque único por impacto) |
| `dumplimit:N` | número de bloques volcados **en total** (limitación del trazador) |

**Limitaciones del trazador, relevantes aquí:**

- El límite de volcados es **global**, no por impacto: con `dumplimit:N` solo
  los `N` primeros impactos se volcan. Por eso las series largas se capturan
  con **un solo** breakpoint (el update por frame) y el resto de direcciones
  se mide en sesiones aparte.
- Cada impacto cuesta una excepción de depuración + una línea de log: el juego
  baja de ritmo. Se midió el coste (§ 3.3): con un impacto por frame la tasa
  real se mantiene (~25 fps), con volcado por frame también.

### 2.3 Qué NO se ha hecho

- No se ha capturado el framebuffer del juego (la ventana devuelve negro), así
  que **no hay verificación píxel a píxel**.
- No se ha modificado el ejecutable, el driver ni los assets.

---

## 3. Lo observado

### 3.1 El arranque: tres películas y luego nada (sin entrada)

Sin teclas, el juego **no llega nunca** a la ruta 3D. Con breakpoints en la
cadena S4 (cine) y en el update por frame:

| t (ms) | Función | Qué es |
|---:|---|---|
| 3 667 | `0x00538017` | wrapper de cine (el que llama `WinMain`) |
| 3 782 | `0x0053A5FB` | bucle de película |
| 15 079 | `0x0053A5FB` | bucle de película |
| 25 811 | `0x00538017` | wrapper de cine |
| 25 826 | `0x0053A5FB` | bucle de película |

En 70 s **no se ejecutan ni el update por frame (`0x0043E1BA`) ni la emisión
de geometría (`0x004269C2`)**. El juego espera en las películas (la intro)
hasta que el jugador pulsa algo.

⟹ El arranque es: **cine (S4) → (entrada del jugador) → ruta 3D**.

### 3.2 La ruta 3D empieza y la cámara sale volando

Con el guion de teclas `1B,1B,1B,0D,0D` (Esc a los 8, 16 y 24 s; Enter a los
32 y 40 s), el update por frame y la emisión de geometría arrancan a la vez
hacia los **25,0 s**: la ruta 3D (el nivel MAP7) entra en pantalla.

**Y la cámara se mueve desde el primer frame**, sin que ninguna tecla lo
provoque: el vuelo dura **428 frames** (17,08 s medidos; § 3.3) y termina
justo en el frame 428.

### 3.3 La tasa de frames real

Sesión ligera (breakpoints en el update y en la emisión, **sin** volcados):
20,2–24,8 updates por segundo. La sesión con volcado por frame da 24,7
muestras/s. ⟹ **el coste del trazador es despreciable** y los 428 frames del
vuelo equivalen a **≈ 17,1 s** de reloj real.

### 3.4 La serie de cámara

Una muestra por frame en el update por frame, volcando el objeto de cámara
`cam = 0x0065CD80` (posición `+0x00/+0x02/+0x04`, Euler `+0x30/+0x32/+0x34`,
matrices A/B, planos, punteros y escalas). 1 728 muestras en 70 s.

| frame | t (ms) | X | Y | Z | Euler X | Euler Z |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 2341 | 4071 | 3074 | -397 | 2802 |
| 1 | 37 | 2365 | 4101 | 3066 | -397 | 2796 |
| 2 | 77 | 2404 | 4149 | 3052 | -397 | 2784 |
| 3 | 118 | 2456 | 4213 | 3033 | -396 | 2769 |
| 4 | 158 | 2520 | 4291 | 3009 | -393 | 2750 |
| 5 | 198 | 2595 | 4381 | 2982 | -392 | 2729 |
| 6 | 238 | 2681 | 4482 | 2952 | -390 | 2701 |
| 7 | 278 | 2774 | 4592 | 2918 | -387 | 2673 |
| 8 | 319 | 2875 | 4708 | 2882 | -382 | 2642 |
| 9 | 357 | 2982 | 4829 | 2844 | -376 | 2607 |
| 10 | 399 | 3094 | 4953 | 2804 | -371 | 2572 |
| 11 | 438 | 3210 | 5078 | 2764 | -364 | 2537 |
| 20 | 799 | 4275 | 5990 | 2409 | -278 | 2253 |
| 40 | 1 599 | 7808 | 5543 | 1267 | -99 | 1460 |
| 80 | 3 199 | 4421 | 1831 | 462 | -10 | 1429 |
| 120 | 4 800 | -1256 | -1218 | 1766 | -262 | 2142 |
| 160 | 6 400 | 1153 | -4492 | 911 | -127 | 2585 |
| 200 | 8 002 | 4498 | -8346 | 737 | -33 | 2769 |
| 240 | 9 602 | 7761 | -5743 | -244 | 75 | 10 |
| 280 | 11 202 | 8533 | -774 | 1174 | -21 | -72 |
| 320 | 12 802 | 6086 | 4506 | 1276 | 5 | 877 |
| 360 | 14 402 | 1316 | 4088 | 640 | -77 | 1310 |
| 400 | 16 002 | -737 | 827 | 384 | -2 | 2786 |
| 419 | 16 762 | 1782 | 556 | 229 | -18 | 2889 |
| 420 | 16 802 | 1879 | 541 | 222 | -20 | 2881 |
| 421 | 16 842 | 1967 | 528 | 216 | -23 | 2872 |
| 422 | 16 882 | 2045 | 516 | 210 | -24 | 2864 |
| 423 | 16 922 | 2113 | 506 | 205 | -25 | 2856 |
| 424 | 16 962 | 2167 | 498 | 201 | -26 | 2851 |
| 425 | 17 002 | 2208 | 492 | 198 | -28 | 2846 |
| 426 | 17 042 | 2234 | 488 | 196 | -29 | 2844 |
| **427** | **17 082** | **2243** | **487** | **196** | **-29** | **2842** |

A partir del frame 428 la cámara **no vuelve a cambiar**: las 1 300 muestras
siguientes (hasta 95 s) son idénticas.

- **Punto de partida**: `(2341, 4071, 3074)`, Euler `(-397, 0, 2802)` — el
  mismo encuadre de la ruta 3D capturado antes
  ([`S2_CAMERA_VISIBILITY.md`](S2_CAMERA_VISIBILITY.md) § 14.2). Es el
  **encuadre por defecto del nivel**.
- **Punto de llegada** (estable): `(2243, 487, 196)`, Euler `(-29, 0, 2842)`,
  `w = 384`, corte lejano `8500`. Es el **encuadre del menú**.
- **La trayectoria** no es una línea: la cámara sube hasta `Y = 6503`, baja
  hasta `Y = -8346` y vuelve: un **vuelo largo en bucle** sobre el nivel.

### 3.5 Otros campos que cambian durante el vuelo

| Campo | Durante el vuelo | Estable |
|---|---|---|
| `cam+0x5C` (corte lejano) | 12 000 → 8 500 | 8 500 |
| `cam+0x48` (ancho base) | 384 | 384 |
| `cam+0xE0/E4/E8/F0` (punteros de orientación) | fijos: `0x0065CD60`, `0x0057B508`, `0x0057B528`, `0x0057B548` | los mismos |
| `cam+0x43C` (modo de orientación) | 0 (base identidad) | 0 |
| `cam+0x490/492/494` (escalas) | `0x10001000`, `0x10001000`, `0x1000` | los mismos |
| `cam+0x128/12C` (Euler de la rama alternativa) | 0 | 0 |
| `cam+0x130/134` (copia de la posición) | siguen a la posición | los mismos |

El branch de la rama alternativa (`cam+0x128`) está **a cero** durante toda la
secuencia: el juego usa la rama normal (Euler en `cam+0x30`).

---

## 4. El mecanismo

### 4.1 La cámara copia: no calcula la trayectoria (§ 4.8-4.10)


El escritor de la posición de la cámara está en la función que empieza en
`0x0043B11D` (el update del mundo):

```asm
0x43B56B  movl 0x54d054, %eax        ; eax = cámara (0x0065CD80)
0x43B570  movl -0x4(%ebp), %ecx       ; ecx = controlador de cámara
0x43B573  movw 0x28(%ecx), %dx       ; controlador[+0x28]
0x43B577  movw %dx, (%eax)           ; cámara[+0x00] = posición X
0x43B582  movw 0x2a(%ecx), %dx
0x43B586  movw %dx, 0x2(%eax)        ; cámara[+0x02] = posición Y
0x43B592  movw 0x2c(%ecx), %dx
0x43B596  movw %dx, 0x4(%eax)        ; cámara[+0x04] = posición Z
0x43B59A  ...                       ; y una segunda copia en cam+0x118/0x11A/0x11C
0x43B5D3  calll 0x402dc0             ; reconstruye las matrices de la cámara
```

El controlador se obtiene en `0x0043B294` (dentro de la función que empieza en
`0x0043B11D`):

```asm
0x43B294  movl 0x8(%ebp), %eax     ; el mundo/la escena
0x43B297  pushl %eax
0x43B298  calll 0x461e0b           ; -> puntero al nodo de cámara
0x43B2A0  movl %eax, -0x4(%ebp)   ; controlador
```

y `0x00461E0B` llama a `0x00461B90`, que **carga un fichero entero** en
memoria (`fopen` `0x00541060`, `fseek` `0x00540F90`, `ftell` `0x00540DE0`,
`malloc` `0x00464FB3`, `fread` `0x00540CA0`, `fclose` `0x00540C20`) y después
**compacta** el `.DFX`: mueve la **imagen** (la región que sigue a la tabla de
relocalización, en `aligned*4`, con `aligned = (count + 0x200) & ~0x1FF`) al
principio del buffer y devuelve ese puntero. Esto **ya estaba documentado** en
[`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md) § C.6, que llama a
ese resultado «B (imagen compactada)» y lo almacena en `level[+4]`.

> **Corrección (2026-11-13).** Lo publicado el 2026-11-12 («un array de
> registros de **0x200 B** indexado por un contador enmascarado con `0x1FF`) era
> **incorrecto**: `0x200` es la **alineación** de la imagen del `.DFX`, no un
> tamaño de registro, y `0x1FF` es la máscara de esa alineación, no un índice
> de nodo. La aritmética es exactamente la que ya usa el parser del port
> (`GEX_DFX_ALIGN_ADD` / `GEX_DFX_ALIGN_MASK`, `port/src/asset/dfx.c`).

⟹ El material de la trayectoria vive en **datos del juego** (la imagen de
un `.DFX`), no en código ni en una constante. El nodo tiene la posición en
`+0x28/+0x2A/+0x2C`.

⟹ Solo está demostrada la copia de posición y orientación en la **colocación
inicial** (`0x43B56B..0x43B5CC`). No es la rutina que actualiza la cámara durante
los 428 frames. La representación de los datos y la rutina de actualización
siguen sin identificar; no podemos afirmar todavía dónde ni cómo se interpola.
El port ya dispone de la imagen compactada de `level\\map7.dfx`, así que no
necesita implementar E/S nueva para investigar esos datos.

### 4.2 La trayectoria NO es una interpolación de dos puntos


Ajuste polinomial de las 428 muestras con `t = frame/427`:

| Ajuste | `X` | `Y` | `Z` | Euler X | Euler Z |
|---|---:|---:|---:|---:|---:|
| grado 3 | 5 929 | 7 081 | 1 495 | 234 | 2 044 |
| grado 5 | 2 639 | 3 181 | 1 274 | 188 | 2 260 |
| grado 7 | 2 848 | 1 816 | 1 010 | 162 | 2 231 |

(residuo máximo en unidades del juego; el rango de cada eje es de 3 361 a
15 497). Los residuos son del mismo orden que el propio recorrido: esto
permite descartar un polinomio de grado bajo como descripción de la trayectoria,
pero **no identifica por sí solo el interpolador ni demuestra una spline**.
El controlador obtiene datos de un fichero organizado en registros de 0x200 B;
la hipótesis de varios puntos de control es compatible con esa estructura, pero
queda pendiente decodificar el nodo y seguir cómo se actualiza antes de afirmar
si usa Catmull-Rom, otra spline o una secuencia de muestras.

### 4.3 El perfil de velocidad


| Tramo (frames) | `|Δ|` medio por frame |
|---|---:|
| 0–19 | 140,9 (arrancando desde 39,2) |
| 20–39 | 204,0 (máximo 255,9) |
| 40–419 | 111–181 (el recorrido curva) |
| 420–426 | 52,7 (frenada: 89 → 41 → 26 → 9 → 0) |

⟹ **aceleración inicial, crucero y frenada final hasta el alto total**.
El perfil es compatible con una interpolación suave (por ejemplo, Catmull-Rom
o equivalente), pero la velocidad observada no basta para identificar el
algoritmo ni el número de puntos de control.

### 4.4 Descartado: no es una cinemática de vídeo


- El **S4** (cine) se ejecuta **antes** de la ruta 3D (3,7 / 15,1 / 25,8 s) y
  es un bucle **bloqueante** con su propio `grSstWinOpen` de 640×480
  ([`docs/SUBSYSTEM_SEGMENTS.md`](docs/SUBSYSTEM_SEGMENTS.md) § S4).
- La cámara que se mueve es **`cam = 0x0065CD80`**, la del **nivel** (la misma
  que consume el renderer del port y que se capturó en la ruta 3D), y se mueve
  **dentro** de la ruta 3D, con el nivel dibujándose.
- El movimiento ocurre en la ruta 3D (update por frame + emisión de
  geometría activos) ⟹ **no es la reproducción de una película**.

⟹ El mecanismo demostrado es: **un controlador de cámara del mundo alimenta
la posición y orientación que la cámara copia por frame**, usando datos
procedentes de un fichero. La estructura exacta de esos datos y el algoritmo
que genera cada estado siguen pendientes de decodificación.

---

### 4.5 El fichero: cuál es y cómo se localiza


Medido con el trazador (breakpoint en `0x00461B90` y volcado de la cadena de
la ruta; `dumpstr` no consume el límite de volcados, así que sale **todas** las
rutas que abre el juego):

| t (ms) | Ruta abierta | Llamante |
|---:|---|---|
| 1 696 | `…\GEX2\gex.dfx` | `0x00461E20` (dentro de `0x461E0B`) |
| 25 764 | `…\GEX2\level\map7.vfx` | `0x00461C41` (dentro de `0x461C32`, la tabla de texturas) |
| 25 766 | `…\GEX2\level\map7.dfx` | `0x00461E20` (dentro de `0x461E0B`) |

⟹ **El control de cámara del menú viene de `level\map7.dfx`**: el `.DFX`
del nivel, cargado en el **mismo instante** (25,766 s) en que la cámara empieza
a volar (25,96 s). `gex.dfx` es el `.DFX` global (cargado al arrancar, con el
menú) y `map7.vfx` es el contenedor de texturas del nivel (`0x461C32` construye
con él la tabla de texturas de `0x006488F4`).

⟹ Consecuencia práctica: **el material de la trayectoria es el mismo
`.DFX` que el port ya lee** (`gex_dfx_container.image`, la región compactada
que el parser del port ya extrae). No hace falta E/S nuevo: basta decodificar
dentro de la imagen.

### 4.6 El nodo de cámara: `+0x28` y el encuadre final del menú


- El offset `+0x28` de la copia (`0x43B573`) coincide con la **primera
  entrada** de la imagen: sus palabras `+0x28/+0x2A/+0x2C` valen
  `(2795, 77, 268)`, que es exactamente la posición de cámara de la **primera
  muestra** de la serie publicada (antes del vuelo).
- El **encuadre final del menú `(2243, 487, 196)` aparece como literal en esa
  imagen**, en el offset `0xC356` de la imagen (una sola ocurrencia).

⟹ El encuadre al que llega el menú **es un dato del nivel**, no una
constante del código. Y el nodo tiene la posición en `+0x28` (int16 ×3).

**Lo que NO está en la imagen**: los puntos intermedios de la trayectoria. El
punto medido en el frame 200, `(4498, -8346, 737)`, **no** aparece como
tripelete de int16 en la imagen. ⟹ La representación **no** es una lista de
posiciones muestreadas (queda por identificar si son puntos de control con
otra codificación, o un movimiento paramétrico).

### 4.7 La temporización: **por frame**, no por reloj


Estado de ejecución capturado por frame (volcado del bloque global
`0x006466D8`, el estado de la ruta 3D):

| Offset | Tipo | Comportamiento medido |
|---|---|---|
| `+0x46` | int16 | **contador**: suma **1 en cada frame** (1 150 de 1 189 pasos consecutivos; los 39 restantes son reinicios por cambio de escena) |
| `+0x10` / `+0x12` | int16 ×2 | par de valores que **alterna** entre dos durante el vuelo (p. ej. `-29680` y `-20080`) |
| `+0x14` | int16 | valores del mismo orden, con variaciones pequeñas |
| `+0x16` | int16 | constante `100` durante el vuelo |

⟹ **El avance de la trayectoria es por frame**, no por un reloj: el
contador de `+0x46` acompaña 1:1 a los frames y no hay `GetTickCount` ni un
delta de tiempo en el avance. ⟹ **Consecuencia para el port**: los **428
frames** del vuelo son la unidad, y a 60 fps la misma secuencia duraría ≈ 7,1 s,
no 17,1 s. El port debe reproducir **el número de frames**, no la duración en
segundos.

El papel de `+0x10/+0x12/+0x14` (los valores que alternan) **no** está
establecido: se han medido, pero no se ha identificado qué representan ni cómo
participan en el cálculo.

### 4.8 El escritor de la cámara, localizado con watchpoints de escritura


**Extensión del trazador (2026-11-13).** `tools/frame-trace/tracer.cs` solo
sabía poner breakpoints de **ejecución**. Se le ha añadido una opción
**`watch:<addr>`**: un breakpoint de **escritura** de 4 bytes en una dirección
absoluta (registro de depuración con `R/W=01`, `LEN=11`). Al dispararse,
registra la **instrucción que escribe** (EIP), el valor anterior y el nuevo, los
registros y la dirección de retorno. Es una opción aditiva: el comportamiento
anterior (breakpoints de ejecución, volcados, guiones de teclas) no cambia.

(residuo máximo en unidades del juego; el rango de cada eje es de 3 361 a
15 497). Los residuos son del mismo orden que el propio recorrido: esto
permite descartar un polinomio de grado bajo como descripción de la trayectoria,
pero **no identifica por sí solo el interpolador ni demuestra una spline**.
El controlador obtiene los datos de la imagen compactada de `level\\map7.dfx`.
El valor `0x200` describe la alineación de esa imagen en el contenedor, no el
tamaño de un registro. La representación de la trayectoria sigue pendiente de
decodificación; no sabemos todavía si contiene puntos de control, coeficientes,
una secuencia transformada u otra estructura.
**Qué escribe la cámara durante el vuelo.** Con `watch:` sobre
`cam+0x00/+0x04/+0x30/+0x34` (posición X/Y, Z y Euler X/Y, Z) en una sesión
completa de MAP7:

| Escritor (EIP) | Campo | Veces | Ventana |
|---|---|---:|---|
| `0x00411BBB` | posición X (`cam+0x00`) | **427** | 26 417 – 43 483 ms |
| `0x00411BCC` | posición Y (`cam+0x02`) | **427** | ídem |
| `0x00411BDD` | posición Z (`cam+0x04`) | **427** | ídem |
| `0x004122E8` | Euler X (`cam+0x30`) | 563 | 25 950 – 48 888 ms |
| `0x004122F6` | Euler Y (`cam+0x32`) | 563 | ídem |
| `0x00412304` | Euler Z (`cam+0x34`) | 563 | ídem |

⟹ **La posición se escribe 427 veces: una por frame, del frame 1 al 428**, y
**deja de escribirse** al terminar el vuelo (la cámara queda fija en su
encuadre final). La orientación se escribe **una vez por frame durante toda la
sesión** (563 escrituras), incluso después del vuelo.

**El bloque que copia la posición** (`0x411B55..0x411BD9`, dentro de la función
que empieza en `0x00411B00`):

```asm
0x411BAC  movl 0x8(%ebp), %ecx        ; el objeto del mundo
0x411BAF  movl 0x8(%ecx), %edx        ; edx = &cam[+0x00]  (0x0065CD80)
0x411BB2  movl -0xc(%ebp), %eax       ; eax = puntero devuelto por 0x53E98C
0x411BB5  movw (%eax), %cx            ; lee p[0]
0x411BB8  movw %cx, (%edx)            ; cam.pos.x = p[0]      <-- escritura
0x411BC4  movw 0x2(%ecx), %dx         ; p[1]
0x411BC8  movw %dx, 0x2(%eax)         ; cam.pos.y = p[1]
0x411BD5  movw 0x4(%edx), %ax
0x411BD9  movw %ax, 0x4(%ecx)         ; cam.pos.z = p[2]
```

Es decir: **la cámara no calcula nada, copia tres `int16` de un puntero**.

> **Actualizado (2026-11-13).** El párrafo anterior decía que la rutina
> que actualiza la cámara durante los 428 frames no estaba identificada: **esa
> limitación queda superada** por lo medido con breakpoints de escritura
> (§ 4.8-4.10). Lo que **sigue** sin identificar es quién avanza el nodo de la
> trayectoria, la curva exacta de la interpolación y si el contador de
> `0x006466D8+0x46` gobierna la selección de datos.

### 4.9 La cadena que produce ese puntero


```
0x411B94  calll 0x53e98c          ; -> puntero a la posición actual
0x53E98C  calll 0x53f252         ; comprobación previa
0x53E9B0  calll 0x53ef00         ; rellena el buffer global 0x5AC608
0x53E9BC  movl $0x5ac608, %eax   ; y lo devuelve
```

- El **buffer global `0x005AC608`** es la posición del frame (tres `int16`).
  Capturado con watchpoint: lo escribe `0x0053DF72` / `0x0053DFB1` (y la
  tercera palabra), que están dentro de **`0x0053DEB0`**: la función de
  interpolación.
- `0x53EF00` es el **navegador de la trayectoria**:

```asm
0x53EF41  movswl 0x4(%ecx), %edx      ; obj[+4] = número de registros
0x53EF48  cmpl %edx, %eax             ; ¿índice >= número?
0x53EF65  movw 0x2(%edx), %ax         ; nodo[+2] = flags | fracción
0x53EF6C  andw $0x1, %ax             ; bit 0 = "valor directo"
0x53EF89  imull $0xa, %ecx, %ecx      ; índice * 10
0x53EF94  movswl (%eax,%ecx), %ecx    ; array[índice * 10]
0x53EFA3  shll $0x5, %eax            ; índice * 32  (la otra variante)
```

⟹ La estructura que consulta es:

| Elemento | Contenido |
|---|---|
| `obj[+0]` | puntero al **array de registros** (dentro de la imagen del `.DFX`) |
| `obj[+4]` | número de registros (`int16`) |
| `obj[+6]` | bandera de variante: `0` → registros de **32 `int16` (64 B)**, `1` → registros de **10 `int16` (20 B)** |
| nodo `+0` | **índice** del registro actual (`int16`) |
| nodo `+2` | bit 0 = «usar el valor directo»; bits 1-15 = **fracción** de interpolación |

Y la interpolación (`0x53DEB0`, la variante de 20 B, que es la que usa este
juego) calcula **tres pesos polinómicos** a partir de la fracción `t` (con el
desplazamiento en punto fijo leído de `[0x0054C1B0]`) y los aplica a los
registros actual y siguiente. La forma cerrada exacta de esos pesos **no está
verificada** (lectura parcial del desensamblado): queda pendiente.

En runtime (sesión de captura): `obj = 0x193C5F78` y el nodo en `0x193C5F68`.
Son direcciones del **heap** y **cambian entre ejecuciones**, así que no se
pueden fijar a priori; por eso el nodo se localizó con un breakpoint de
ejecución en `0x53E98C` (que registra sus argumentos) y no con un watchpoint
directo.

### 4.10 Resumen del mecanismo (lo que se sabe y lo que no)


| Pregunta | Respuesta | Estado |
|---|---|---|
| ¿Quién escribe la posición? | `0x411BBB` / `0x411BCC` / `0x411BDD` | medido |
| ¿Quién escribe la orientación? | `0x004122E8` / `0x4122F6` / `0x00412304` | medido |
| ¿Cada cuánto? | posición: 1 por frame durante el vuelo (427 escrituras = frames 1..428); orientación: 1 por frame siempre | medido |
| ¿De dónde salen los valores? | de un **array de registros** en la imagen del `.DFX`, indexado por el nodo | medido |
| ¿Cómo se combinan? | interpolación entre el registro actual y el siguiente, con una fracción de 15 bits en el nodo | medido (mecánica); **la curva exacta no** |
| ¿Quién avanza el nodo? | **no identificado** (el nodo está en el heap y su escritor no se ha capturado) | pendiente |
| ¿El contador de `0x006466D8+0x46` gobierna la selección? | no demostrado: acompaña 1:1 a los frames, pero no se ha capturado el nodo en el mismo instante | pendiente |

## 5. Relación con el menú

Lo que **sí** queda medido:

| Observación | Dato |
|---|---|
| La ruta 3D (el nivel MAP7) entra a la vez que empieza el vuelo | update y emisión activos desde ~25,0 s; el vuelo arranca en el frame 1 |
| El vuelo **no** lo provoca ninguna tecla | el guion solo sirve para saltar las películas; el vuelo arranca y acaba solo |
| El vuelo **termina** en un alto total | los últimos 7 frames frenan (89 → 41 → 26 → 9 → 0) y la cámara no vuelve a moverse en 1 300 frames siguientes |
| El menú **no** se dibuja por las rutas 2D | 0 llamadas a `0x00432BBD` (glifo de fuente) y 0 a `0x00432DD1` (rectángulo) en los 650 frames de la ruta 3D |
| El número de llamadas de estado **no** marca el menú | varía 317–2 353 por frame **según la visibilidad** (la cámara ve distinto nivel en cada punto del vuelo): 1 252 al entrar, ~1 700–2 100 en el centro del vuelo, ~500–700 cuando la cámara pasa por zonas con menos geometría |

⟹ **Lo medido**: el menú se apoya en MAP7 como fondo y la cámara llega a su
encuadre definitivo al final del vuelo (frame 428), donde se queda quieta.

**Lo que NO se ha aislado**: el instante exacto en que los **elementos** del
menú se hacen visibles. Las rutas 2D no se usan en la ruta 3D (0 llamadas), el
recuento de llamadas de estado sigue a la visibilidad y no al menú, y no hay
captura de píxeles (la ventana devuelve negro). Queda como pendiente con el
método disponible (identificar el estado de menú en el mundo de
`0x00648900`/el flujo de comandos, o capturar la ventana con otro método).

**Búsqueda de un indicador de «menú preparado» (resultado negativo).** Se
volcó por frame el bloque del mundo `0x00648900` (0x200 B, 925 frames) buscando
un campo que cambiara **una sola vez** en el frame 428 (el final del vuelo) y
se quedara fijo: los campos que varían son el estado de la cámara
(posición, orientación, planos), la proyección y **contadores por frame**
(p. ej. `+0x104`, que suma 1 en cada frame), y **ninguno** hace un único cambio
en el frame 428 ⟹ el indicador del menú no está en ese bloque (o no es un
único cambio de valor). Queda pendiente.

**Hipótesis (no demostrada)**: el vuelo de cámara **es** la animación de
entrada del menú (el juego entra en la ruta 3D, la cámara vuela hasta el
punto de vista del menú y ahí se queda). El indicio es que el vuelo arranca
solo en el primer frame de la ruta 3D y que su encuadre final es estable
indefinidamente; falta identificar el estado de menú para confirmarlo.

---

## 6. Diseño para el port (DX11)

Con los datos medidos, el port necesita:

| Dato | Valor | Estado |
|---|---|---|
| Encuadre de partida | `(2341, 4071, 3074)`, Euler `(-397, 0, 2802)` | medido |
| Encuadre final | `(2243, 487, 196)`, Euler `(-29, 0, 2842)` | medido |
| Duración | **428 frames** (≈ 17,1 s a ~25 fps) | medido |
| Unidad de avance | **frames**, no segundos (§ 4.7) | medido |
| Origen de los datos | **la imagen de `level\map7.dfx`** (§ 4.5), que el port ya parsea | identificado |
| Encuadre final como dato | literal en la imagen del `.DFX`, offset `0xC356` | medido |
| Posición en el nodo | `+0x28` (int16 ×3) | medido |
| Forma | recorrido curvo; el navegador selecciona registros y el interpolador combina el actual y el siguiente mediante una fracción de 15 bits | mecanismo identificado; curva exacta pendiente |
| Origen de los datos | controlador del mundo `0x006487E0` y array de registros en la imagen de `level\\map7.dfx` | identificado |

**Lo que falta para poder implementarlo fielmente:**

1. **Quién avanza el nodo** de la trayectoria (el nodo vive en el heap; todavía
   no se ha capturado la instrucción que modifica su índice/fracción).
2. La **curva exacta** de `0x53DEB0`: se ha confirmado la interpolación entre
   registros y el uso de una fracción de 15 bits, pero falta completar el
   desensamblado y verificar la fórmula de los tres pesos polinómicos.
3. Si el contador de `0x006466D8+0x46` gobierna la **selección** de registros
   o solo acompaña al avance del nodo.
4. El significado de los campos `+0x10/+0x12/+0x14` del estado en
   `0x006466D8`.
5. El **estado de menú** que dispara el vuelo y el momento exacto en que sus
   elementos quedan visibles/preparados.

La rutina que copia la posición durante el vuelo **ya está localizada**:
`0x411BBB` / `0x411BCC` / `0x411BDD` escriben X/Y/Z, y la cadena
`0x53E98C → 0x53EF00 → 0x53DEB0` produce los tres valores. Por tanto, no
queda pendiente localizar el escritor de cámara ni sustituirlo por una
aproximación: queda reconstruir el avance del nodo y la matemática exacta
del interpolador.

---

## 7. Límites

- Todo está medido sobre **nGlide 2.10** (software, D3D9): el frame rate, y
  por tanto la duración en segundos, depende del driver. En el Voodoo real la
  secuencia iría más rápido (más fps).
- El tiempo del juego se ve afectado por el coste del trazador (medido: ~25
  fps con y sin volcado ⟹ el coste es despreciable).
- **No hay verificación píxel a píxel**: la ventana del juego devuelve negro.
- El encuadre del menú se ha medido **con el nivel cargado**; no se ha
  medido el menú con otro nivel.

---

## 8. Siguiente tarea

1. Capturar quién **modifica el nodo del heap** (índice y fracción) y
   relacionar esa escritura con el update de cada frame y con
   `0x006466D8+0x46`.
2. Completar el desensamblado de `0x53DEB0` y verificar la fórmula exacta de
   los tres pesos polinómicos para la variante de registros de 10 `int16`.
3. Identificar el estado que hace visibles/preparados los elementos del menú
   al terminar el vuelo.
4. Con esos datos, implementar el algoritmo original en DX11 y validarlo
   frente a la secuencia medida de 428 frames; no sustituirlo por una curva
   aproximada ni por muestras inventadas.
