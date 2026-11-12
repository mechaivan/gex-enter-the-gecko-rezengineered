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
| `watch:<addr>` | breakpoint de **escritura** de 4 B en una dirección absoluta |
| `watchdyn:<func>:<arg>:<off>[:<len>]` | breakpoint de **escritura** en `[arg + off]`, donde `arg` se resuelve en el **primer impacto** de `<func>` (para punteros del heap que cambian entre ejecuciones); `len` = 1, 2 o 4 B |
| `probe:<addr>` | dword leído en **cada** impacto de watchpoint (para correlacionar valores) |
| `dumpabs:<base>:<offs>` | dwords de una dirección absoluta (bloque único por impacto) |
| `dumpindraw:<arg>:<ptroff>:<len>` | bloque contiguo de `len` bytes tras **desreferenciar** `arg+ptroff` (así se capturó el array incrustado en el objeto) |
| `dumpstack:<offs>` | dwords en `esp+off` (admite offsets negativos por desbordamiento: `0xFFFFFFF8` = `esp-8`) |
| `dumplimit:N` | número de bloques volcados **en total** (limitación del trazador) |

**Limitaciones del trazador, relevantes aquí:**

- El límite de volcados es **global**, no por impacto: con `dumplimit:N` solo
  los `N` primeros impactos se volcan. Por eso las series largas se capturan
  con **un solo** breakpoint (el update por frame) y el resto de direcciones
  se mide en sesiones aparte.
- Cada impacto cuesta una excepción de depuración + una línea de log: el juego
  baja de ritmo. Se midió el coste (§ 3.3): con un impacto por frame la tasa
  real se mantiene (~25 fps), con volcado por frame también.
- En un breakpoint **a mitad de función**, `esp` no apunta a la dirección de
  retorno: los campos `ret`/`a0`/`a1`/`a2` del log quedan **desplazados uno**
  (el `ret` real es `a0`, el argumento 0 es `a1`, etc.). Hay que tenerlo en
  cuenta al leer los logs.
- Los watchpoints de escritura disparan como `SINGLE_STEP` con el bit de `DR6`:
  el **EIP registrado es la instrucción siguiente** a la que escribe.

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

### 4.1 La cámara copia: no calcula la trayectoria (§ 4.8-4.14)


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

> **Superado (2026-11-13).** La rutina de actualización **sí** está identificada
> (`0x411B8F` → `0x411B94` → `0x53E98C`, § 4.8-4.9), la representación de los
> datos también (registros de **32 B** incrustados en el objeto, § 4.10) y la
> interpolación está cerrada y verificada (§ 4.10-4.13). Se conserva el párrafo
> como registro de lo que se sabía en esa fecha.

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

> **Superado (2026-11-13).** Aquí se decía «registros de 0x200 B»: `0x200` es
> la **alineación** de la imagen del `.DFX`, no el tamaño de un registro (§ 4.5).
> El interpolador ya está identificado: registros de **32 B** y **Hermite
> cúbica** en Q15 (§ 4.10), verificado contra las 428 muestras (§ 4.13). No se
> etiqueta como Catmull-Rom porque el criterio de las tangentes no se ha
> establecido.

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

> **Superado (2026-11-13).** Los puntos intermedios **sí** están en la imagen,
> pero como **registros de control de 32 B**, no como posiciones muestreadas:
> el frame 200 es una interpolación entre dos de esos registros, de modo que su
> tripelete no tiene por qué aparecer literal. La ubicación se obtuvo siguiendo
> la **cadena real de punteros** del contenedor, no por búsqueda de bytes
> (siguiente apartado).

**La cadena que lleva a los 26 registros (medida, no escaneada).** La imagen
compactada de `level\map7.dfx` (con `image_offset = 0x19800`, calculado como
`((count + 0x200) & ~0x1FF) * 4`) contiene la cadena:

| Paso | Offset en la imagen | Contenido | Resultado |
|---|---:|---|---|
| Header de nivel | `0x5FC` | `4F 0 → 0x95EC`, `4F 1 → 0x961C`, `0E` | dos referencias de actor |
| Actor (slot 0) | `0x95EC` | `+0x28` | entidad `0xC00C` |
| Entidad | `0xC00C` | `+0x00` → objeto; `+0x10` → nodo (ceros en fichero) | objeto `0xC02C` |
| Objeto | `0xC02C` | `+0x00` → registros; `+0x04` = 26; `+0x06` = 0; `+0x07` = 1 | array `0xC034` |
| Array | `0xC034` | 26 × 32 B = 832 B | 25 segmentos + centinela |

Los 832 B son **idénticos byte a byte** al volcado runtime del array en
`0x193E6F80` (capturado con el juego en marcha): el fichero contiene el array
ya construido, sin transformación. La ruta de 29 registros de la imagen
`0xA5EC` (cabecera `0xA5CC`) coincide en los registros 0..23, pero su registro
24 final difiere (`D=18`, tangentes `(1822, -299, -117)` frente al centinela);
no es la ruta que usa la cámara de MAP7.

> **Límite.** El significado del opcode `0x4F` del header y el criterio que
> hace que el slot 0 sea la cámara (y el slot 1, una spline hermana de 26
> registros en `0xC394`) **no** están identificados. Lo que está demostrado es
> la ubicación determinista por la cadena y que sus 832 B son los del array en
> memoria.

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

> **Corrección (2026-11-13).** Este apartado atribuía el avance al contador de
> `+0x46`. Medido después sobre el propio nodo (§ 4.11), el avance lo produce
> `0x53F252` con **+1 en la fracción de 15 bits por llamada**, y `0x53E98C`
> —quien la llama— se ejecuta **una vez por frame** desde el update de cámara.
> El contador de `+0x46` **no** es la fracción ni el índice: coincide con la
> fracción en 4 de 607 escrituras y se reinicia de forma irregular (§ 4.12). La
> conclusión práctica (unidad = frames) **no cambia**; el mecanismo sí.

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

> **Superado (2026-11-13).** La representación **sí** está decodificada: son
> **registros de 32 B** (punto, dos tangentes y la duración `D`) incrustados en
> el objeto del mundo, y la interpolación es una **Hermite cúbica** en Q15
> (§ 4.10).
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
> (§ 4.8-4.14). También quedan resueltos después quién avanza el nodo
> (`0x53F252`, § 4.11) y la curva exacta de la interpolación (Hermite en Q15,
> verificada 428/428, § 4.13). Lo que **sigue** sin identificar es el criterio
> con que se calcularon las tangentes guardadas en cada registro.

### 4.9 La cadena que produce ese puntero


```
0x411B8F  ...                      ; bloque del update de cámara
0x411B94  calll 0x53e98c           ; -> puntero a la posición del frame
0x53E997  calll 0x53f252           ; AVANZA el nodo (índice y fracción)
0x53E9B0  calll 0x53ef00           ; EVALÚA la trayectoria en 0x005AC608
0x53E9BC  movl $0x5ac608, %eax     ; devuelve el buffer
0x411BB5  ... 0x411BD9             ; la cámara copia 3 int16 a cam+0/+2/+4
```

**Medido** con un breakpoint de ejecución en `0x53E98C` que registra sus
argumentos (§ 2.2), en una sesión completa de MAP7:

| Dato | Valor |
|---|---|
| Llamadas con el nodo de la cámara | **607** |
| … desde `0x00411B94` (update de cámara) | **607** |
| … desde `0x004043A8` (arranque, ms 25 352) | 1 (además de las 607) |
| Llamadas que llegan a evaluar (`0x53E9BC`) | **428** = los 428 frames del vuelo |
| Llamadas que no evalúan | 179 (frames posteriores al final: `0x53EF00` devuelve 0) |

⟹ **Una llamada por frame**, desde el bloque `0x411B8F` del update de cámara.
No hay ningún otro llamante que toque el nodo de la cámara durante el vuelo
(`0x53E98C` es un accesor genérico usado por ~20 sitios: NPCs, objetos).

**Los dos nodos de la cámara** (medidos en una sesión; son direcciones del
**heap** y cambian entre ejecuciones):

| Nodo | Objeto | Llamante | Salida | Papel |
|---|---|---|---|---|
| `0x1942AF68` | `0x1942AF78` | `0x411B94` | `0x005AC608` | **posición** (3 int16) |
| `0x1942B2D0` | `0x1942B2E0` | `0x411C8C` | `0x005AC600` | **rotación** (cuaternio, 8 B) |

`0x005AC608` es un **buffer de trabajo compartido**: cualquier objeto que use
`0x53E98C` para posición escribe ahí (medido: dos nodos distintos lo escribieron
en el mismo frame). Funciona porque la cámara copia el contenido **inmediatamente
después** de la llamada. Por eso el port no debe tratarlo como almacenamiento
persistente.

### 4.10 El registro de 32 B y el interpolador `0x53DEB0`

> **Corrección de una interpretación anterior.** § 4.9 decía que la variante
> activa era de **20 B (10 int16)**. El desensamblado completo de `0x53EF00`
> muestra que el tamaño depende de la bandera `obj[+6]`, y que **la cámara usa la
> de 32 B**:

```asm
0x53EF2D  movb 0x6(%eax), %cl        ; obj[+6] = variante
0x53EF81  cmpl $0x1, %edx            ; ¿== 1?
0x53EF8C  imull $0xa, %ecx, %ecx     ; sí:  índice * 10   (registros de 10 B)
0x53EFA3  shll $0x5, %eax            ; no:  índice * 32   (registros de 32 B)
```

| `obj[+6]` | Registro | Ruta | Uso medido |
|---|---|---|---|
| `1` | 10 B | `0x43158D` → `0x431036` (producto escalar de 4 int16, `>>12`, recorte a `0x1000`) | **rotación** (cuaternio) |
| `≠ 1` | **32 B** | `0x53DEB0` (Hermite cúbica) | **posición** de la cámara |

**Layout del registro de 32 B** (leído del array capturado, § 4.13):

| Offset | Tipo | Contenido |
|---|---|---|
| `+0x00` | int16 | **`D`**: duración del segmento **en frames** |
| `+0x02` `+0x04` `+0x06` | int16 ×3 | **punto** del registro (x, y, z) |
| `+0x08` `+0x0C` `+0x10` | int32 ×3 | **tangente de salida** (x, y, z) |
| `+0x14` `+0x18` `+0x1C` | int32 ×3 | **tangente de entrada** (x, y, z) |

El **array de registros está incrustado en el objeto**: `obj[+0]` apunta a
`obj+8`, y `a = array + idx*32`, `b = a + 32`. `obj[+4]` es el número de
registros (**26** para la cámara: índices `0..25`; el recorte posterior lo
confirma, porque fija `nodo[0] = obj[4] - 1 = 25`).

El **último registro (25) es un centinela**: `D = 0`, punto = encuadre final
`(2243, 487, 196)` y tangentes a cero. Con `D = 0` la interpolación no divide
(salvo con `frac = 0`, que copia el punto) y el avance devuelve 0, así que la
secuencia **termina sola** en ese registro (§ 4.11).

**El parámetro `t`** (en `0x53EF00`, con `S = [0x0054C1B0]`):

```asm
0x53F03C  movw 0x2(%ecx), %ax        ; nodo[+2] = flags | fracción
0x53F040  shrw %ax                   ; >> 1
0x53F043  andw $0x7fff, %ax          ; fracción de 15 bits
0x53F04C  movl 0x54c1b0, %ecx        ; S  (medido en runtime: S = 15)
0x53F052  shll %cl, %eax             ; fracción << S
0x53F055  idivl -0x10(%ebp)          ; / D   (división con signo)
```

⟹ `t = (frac15 << S) / D`, es decir **`t` en Q15**: `0` en el registro actual,
`32768` en el siguiente. Si `D == 0` o `frac15 == 0` no se interpola: se copian
los tres `int16` del registro actual (`0x53F0B5..0x53F0FE`).

**La interpolación** (`0x53DEB0(out, t, a, b)`, aritmética entera, `S = 15`):

```
t2  = (t * t)  >> S
t3  = (t2 * t) >> S
h00 = 2*t3 - 3*t2 + (1 << S)
h01 = 3*t2 - 2*t3
h10 = t3 - 2*t2 + t
h11 = t3 - t2

out[k] = (int16) ( ( P_a[k]*h00 + P_b[k]*h01
                   + T_a[k]*h10 + T_b[k]*h11 ) >> S )      k = 0,1,2
```

con `P_a = a+2/+4/+6`, `P_b = b+2/+4/+6`, `T_a = a+8/+0xC/+0x10`,
`T_b = b+0x14/+0x18/+0x1C`. Los términos de punto son `int16` con extensión de
signo (`movswl`); los de tangente son **int32 completos** (`movl`). El resultado
se **trunca a int16** (`movw %dx`), sin saturación.

Los cuatro pesos son la base de Hermite cúbica. **No se etiqueta la curva como
«Catmull-Rom»**: lo medido es la base de Hermite y sus tangentes tal como están
guardadas en el registro; el criterio con que el exportador calculó esas
tangentes no se ha establecido.

### 4.11 El avance del nodo: `0x53F252`

Desensamblada y verificada (`0x53F252..0x53F3C1`). `0x53F252(obj, nodo)`:

```
ret = 0
if (obj == 0 || nodo == 0) return 0
variant = (obj[+6] == 1)
if (nodo[+0] >= obj[+4]) return 0              ; índice fuera de rango
ret = 1
D = (variant ? arr[nodo[+0]*10] : arr[nodo[+0]*32])   ; int16 de +0
0x53E0CB(obj, nodo, 0)                         ; atiende el bit 0 del nodo
frac = (((nodo[+2] >> 1) & 0x7FFF) + 1) & 0x7FFF
nodo[+2] = (nodo[+2] & 1) | (frac << 1)        ; <-- escritura 0x53F312
if (frac < D) return 1                         ; sigue en el mismo segmento
nodo[+0] = nodo[+0] + 1                        ; <-- escritura 0x53F33F
nodo[+2] = nodo[+2] & 1                        ; <-- escritura 0x53F350 (frac = 0)
if (nodo[+0] < obj[+4] - 1) return 1           ; segmento siguiente normal
if (nodo[+0] > obj[+4] - 1) ret = 0            ; se pasó: no se evalúa
if (obj[+7] & 2) || (obj[+7] & 4) nodo[+0] = 0
else                              nodo[+0] = obj[+4] - 1   ; <-- 0x53F3B8
return ret
```

El recorte **solo** actúa en el último índice, y es lo que explica el final:

- **Último segmento (`idx = 24`, `D = 18`)**: al llegar a `frac = 18` hace
  `idx = 25`, `frac = 0`, y como `25 >= obj[4]-1 = 25` entra en el recorte; no
  se ha pasado (`25 > 25` es falso), así que devuelve 1 y **evalúa** el registro
  25 con `frac = 0` → el encuadre final `(2243, 487, 196)`.
- **Registro 25 = centinela**: su `D` es **0** y sus tangentes son cero (medido
  en el array). A partir de ahí, cada frame escribe `frac = 1`, `1 < 0` es
  falso, incrementa a 26, el recorte lo devuelve a 25 y **devuelve 0**: no se
  evalúa. La cámara se queda con el último valor. Eso son las 179 llamadas que
  no evalúan y las 180 escrituras de recorte.

Escritores exactos (medidos con watchpoints de 2 B sobre `nodo+0` y `nodo+2`):

| EIP (instrucción siguiente) | Campo | Veces (sesión) |
|---|---|---:|
| `0x0053F316` | fracción (`nodo+2`) | 607 |
| `0x0053F342` | índice (`nodo+0`) | 204 |
| `0x0053F354` | fracción = 0 | 204 |
| `0x0053F3BB` | índice recortado | 180 |

⟹ El índice se escribe **204** veces: **25** son las transiciones reales
(`0 → 25`) y el resto son los intentos posteriores al final, que el recorte
devuelve a **25** (180 escrituras de recorte). La fracción se reinicia **204**
veces por la misma razón. Por eso la posición **deja de cambiar** aunque la
función se siga llamando cada frame.

**Duraciones medidas** de los 25 segmentos (`D` de los registros `0..24`):

```
20, 13, 10, 10, 15, 10, 15, 14, 25, 39, 24, 26, 23, 17, 15, 18, 14, 13, 14,
15, 15, 15, 15, 15, 18          → suma exacta = 428
```

La suma **coincide exactamente** con los 428 frames medidos por el otro camino
(breakpoints en el update por frame). Es una confirmación independiente del
modelo de avance.

### 4.12 El contador `0x0064711E` no gobierna el nodo (resultado negativo)

`0x006466D8 + 0x46 = 0x0064711E`. Se ha comprobado **correlacionando valores**,
no por suposición: en 607 escrituras de la fracción, el contador **coincide con
la fracción en 4** y no coincide con el índice nunca. Se comporta así:

- suma **1 por frame**, pero **se reinicia** en puntos irregulares (medido en
  `(idx, frac)` = (1,13), (5,9), (8,5), (9,13), (10,3), (11,12), (12,19),
  (14,12), (16,8), (18,10), (20,14), (22,13), (24,16), (25,1)…);
- llega a valores **> 31** (máximo medido: 50), así que no es una fracción de
  5 bits ni un índice;
- tras el final del vuelo sigue subiendo y reiniciándose, mientras el nodo está
  fijo en `idx = 25`.

⟹ **No es la fracción, no es el índice y no participa en el avance.** Su
significado exacto queda **sin identificar**; lo que sí está demostrado es que
el nodo lo mueve `0x53F252` (§ 4.11), no este contador.

### 4.13 Verificación contra las 428 muestras: 428/428

Se reimplementó la fórmula en Python con **aritmética entera exacta** (réplicas
de `imull`, `sarl`, `idivl`, truncado a `int16`) y se comparó con las 428
muestras publicadas en `analysis/menu_camera_map7.tsv`, partiendo solo del array
capturado del proceso:

| Comprobación | Resultado |
|---|---|
| Duraciones `D` leídas del array → suma | **428** (igual que los frames medidos) |
| Muestras comparadas | 428 |
| Coincidencias **exactas** (los 3 ejes, sin tolerancia) | **428 / 428** |
| Valor de `S` | `15`, leído de `[0x0054C1B0]` en runtime |
| Primer frame calculado | `(2341, 4071, 3074)` = el capturado en `0x5AC608` |
| Último frame (427) | `idx = 25`, `frac = 0`, `(2243, 487, 196)` |

También se cruzó con la **salida viva** del proceso (breakpoint en `0x53E9BC`,
volcado de `0x005AC608`): la primera muestra capturada
`0x0FE70925 / 0x0C020FE7 / 0x00000C02` = `(2341, 4071, 3074)` es exactamente el
frame 0 calculado.

⟹ **El algoritmo de la posición está cerrado**: dado el array, la posición de
cada frame se reproduce bit a bit.

**Reproducible**: `analysis/verify_menu_camera_interp.py` contiene la
reimplementación (sin datos del juego incrustados) y compara contra el TSV:

```
python analysis/verify_menu_camera_interp.py <trace_arr.log> --ptr 0x<array>
```

### 4.14 Resumen del mecanismo (lo que se sabe y lo que no)


| Pregunta | Respuesta | Estado |
|---|---|---|
| ¿Quién escribe la posición? | `0x411BBB` / `0x411BCC` / `0x411BDD` | medido |
| ¿Quién escribe la orientación? | `0x004122E8` / `0x4122F6` / `0x00412304` | medido |
| ¿Cada cuánto? | posición: 1 por frame durante el vuelo (427 escrituras = frames 1..428); orientación: 1 por frame siempre | medido |
| ¿De dónde salen los valores? | de un **array de registros de 32 B incrustado en el objeto** (`obj+8`), indexado por el nodo | medido |
| ¿Cómo se combinan? | **Hermite cúbica** en Q15 entre el registro actual y el siguiente, con `t = (frac15 << 15) / D` | medido y verificado 428/428 |
| ¿Quién avanza el nodo? | **`0x53F252`**, llamada desde `0x53E997` (dentro de `0x53E98C`), una vez por frame desde `0x411B94` | medido |
| ¿Cuánto avanza? | **+1** en la fracción de 15 bits; al llegar a `D`, `idx++` y `frac = 0` | medido |
| ¿Qué es `D`? | `int16` en `array[idx*32 + 0]`: la **duración en frames** del segmento | medido |
| ¿El contador de `0x0064711E` gobierna la selección? | **no**: no coincide con la fracción ni con el índice, y no participa en el avance | medido (negativo) |
| ¿Qué es `obj[+6]`? | la variante del registro: `1` → 10 B (rotación), otro → 32 B (posición) | medido |
| ¿Cómo se calculan las tangentes del registro? | **sin identificar** (se leen tal cual del array) | pendiente |
| ¿Dónde están los 26 registros en el `.DFX`? | cadena header `0x5FC` → actor `0x95EC` → `+0x28` → entidad `0xC00C` → objeto `0xC02C` → array `0xC034` (§ 4.5) | medido |
| ¿Está reproducido en el port? | **sí**: módulo `gex/camera_path.h`, aritmética entera, 428/428 exactos (§ 6) | implementado |


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

La trayectoria ya **no** es un objetivo de investigación: es un dato validado y
un módulo portable probado. Lo que queda es **conectarlo** al renderer D3D11,
que es una tanda aparte.

| Dato | Valor | Estado |
|---|---|---|
| Encuadre de partida | `(2341, 4071, 3074)`, Euler `(-397, 0, 2802)` | medido |
| Encuadre final | `(2243, 487, 196)`, Euler `(-29, 0, 2842)` | medido |
| Duración | **428 frames** (≈ 17,1 s a ~25 fps) | medido |
| Unidad de avance | **frames**, no segundos (§ 4.7) | medido |
| Origen de los datos | imagen de `level\map7.dfx`, cadena header `0x5FC` → actor `0x95EC` → entidad `0xC00C` → objeto `0xC02C` → array `0xC034` (§ 4.5) | identificado |
| Encuadre final como dato | literal en la imagen del `.DFX`, offset `0xC356` (§ 4.6) | medido |
| Posición en el nodo | `+0x28` (int16 ×3) (§ 4.6) | medido |
| Forma | **Hermite cúbica en Q15** sobre registros de 32 B, 25 segmentos de `D` frames (§ 4.10) | medido y verificado 428/428 |
| Avance | `+1` en la fracción de 15 bits por frame; `idx++` al llegar a `D` (§ 4.11) | medido |
| Estado en el port | módulo `gex/camera_path.h` + pruebas exactas | implementado |

### 6.1 El módulo portable

| Fichero | Papel |
|---|---|
| `port/include/gex/camera_path.h` | API: `gex_camera_path_key`, `gex_camera_path`, `gex_camera_path_node` |
| `port/src/camera/camera_path.c` | implementación entera de las tres rutinas |
| `port/tests/test_camera_path.c` | 1437 comprobaciones exactas |
| `port/tests/ref_menu_camera_path.h` | 26 registros + 428 posiciones (generado) |
| `analysis/gen_menu_camera_path_ref.py` | genera la referencia siguiendo la cadena del DFX |

El contrato es explícito: el módulo **recibe registros ya validados** (no
parsea el `.DFX`). La cadena de § 4.5 se usa en el generador de la referencia,
no en el runtime del port; así el módulo no depende del formato del contenedor.

```c
gex_camera_path_key keys[26];      /* D, punto, tangente de salida y de entrada */
gex_camera_path path = { keys, 26, 0, 1 };
gex_camera_path_node node;

gex_camera_path_init_node(&node);  /* index = 0, fracción = 0 */
for (frame = 0; frame < 428; frame++) {
    int16_t position[3];
    if (gex_camera_path_step(&path, &node, position))
        camera_set_position(position);   /* conexión pendiente (tanda siguiente) */
}
```

- `gex_camera_path_advance` reproduce `0x53F252`: suma 1 a la fracción de 15
  bits (preservando el bit de modo 0), cambia de segmento al llegar a `D`, y
  devuelve 0 al alcanzar el centinela.
- `gex_camera_path_eval` reproduce `0x53EF00`: copia directa si `D == 0` o
  `frac == 0`; si no, `t = (frac << 15) / D` con **división con signo** y llama
  a la Hermite.
- La Hermite (`0x53DEB0`) usa **solo enteros**: productos de 32 bits con
  *wrap*, `sar 15`, y truncado final a `int16`. **No hay coma flotante en
  ninguna parte del módulo** (la coincidencia exacta de los 428 frames es la
  prueba).
- `gex_camera_path_validate` fija el contrato de los registros: al menos dos,
  variante 0, duraciones no negativas y centinela (`D == 0`) al final.
- La **variante 1** (registros de 10 B, rotación) se **rechaza** en lugar de
  aproximarla; el último registro con `D > 0` también se rechaza para no leer
  fuera del array.

### 6.2 Validación (sin tolerancias)

`port/tests/test_camera_path.c` compara **valor a valor**, eje por eje:

- 26 registros; 25 duraciones que suman **428**; centinela con `D = 0`.
- Las **428 posiciones** medidas del original, frame a frame: **428/428**.
- Primera muestra `(2341, 4071, 3074)`; última `(2243, 487, 196)`.
- Tras el centinela **no** se produce muestra ni se vuelve a escribir la
  posición (se comprueba con un valor centinela en el destino).
- Fronteras de segmento (los frames 19/20 y 32), bit de modo con `frac = 16384`
  (media Hermite calculada a mano), flags de *wrap* `0x02`/`0x04`, argumentos
  inválidos, índices fuera de rango y variante 1.

Resultados: `port/build.ps1 test` → **1437 comprobaciones, 0 fallos** (y el
resto de suites sin regresión); `port/run_sanitizers.ps1` → **O0/O2/asan/ubsan,
24/24 OK**.

> **Comportamiento medido que no hay que «arreglar».** Con los flags de *wrap*
> activos, el frame que alcanza el centinela **reinicia el índice a 0 en el
> mismo frame**, de modo que ese frame copia el registro 0 y no el centinela.
> El módulo lo reproduce; el significado de esos bits no está establecido.

### 6.3 Lo que el módulo NO hace

1. **No parsea el `.DFX`**: recibe los registros validados. Leerlos del
   contenedor en el runtime es una decisión de integración, no del módulo.
2. **No modela la rotación**: la variante de 10 B / cuaternio / Euler sigue
   pendiente (§ 8).
3. **No conoce el estado de menú** ni cuándo aparecen sus elementos (§ 5).
4. **No toca el renderer**: ni emisión, ni texturas, ni orden de dibujo, ni la
   cámara estática ya integrada. La conexión es la tarea siguiente.

**Lo que sigue pendiente (no bloquea el módulo):**

1. El **criterio de las tangentes**: se leen tal cual del registro; cómo las
   calculó el exportador no se ha establecido (no se etiqueta la curva como
   Catmull-Rom ni Bézier).
2. El significado del opcode `0x4F` del header y el criterio de selección del
   slot 0 como cámara (y el papel de la spline hermana de `0xC394`).
3. El papel de los campos `+0x10/+0x12/+0x14` del estado en `0x006466D8`.
4. El **estado de menú** que dispara el vuelo y el momento exacto en que el
   menú queda estable.
5. La ruta de **rotación** (registros de 10 B) y su conversión a Euler.

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

1. **Conectar el módulo al renderer D3D11** (tanda siguiente): llamar a
   `gex_camera_path_step` una vez por frame desde el update de la escena y
   escribir la posición en el nodo de cámara que ya consume la matriz `cam+0xE0`,
   sin tocar emisión, texturas ni orden de dibujo. Comprobar que la escena
   avanza 428 frames y que el encuadre final coincide con el ya integrado.
2. Cerrar la ruta de **rotación** (registros de 10 B → cuaternio → Euler).
3. Identificar el **estado de menú** que dispara el vuelo y el instante en que
   los elementos quedan visibles (ruta 2D o flujo de comandos).
4. Establecer el **criterio de las tangentes** del exportador (sin él no se
   etiqueta la curva).
5. Identificar el opcode `0x4F` del header y el papel de la spline hermana de
   `0xC394`.

La comparación frame a frame contra `analysis/menu_camera_map7.tsv` ya está
cerrada en el módulo (428/428); lo que falta es la comparación **contra el
renderer**, que depende del punto 1.
