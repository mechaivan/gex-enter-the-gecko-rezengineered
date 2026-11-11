# S-2 — Visibilidad de cámara original (`0x00401C00`)

**Objeto:** ingeniería inversa **estática y de solo lectura** de la prueba de
visibilidad de cámara que usa el recorrido BSP de nivel en el ejecutable
original, de las rutinas que preparan sus datos y —**desde 2026-11-11**— de la
**validación dinámica** de esa cadena contra el proceso original en ejecución.

**Alcance:** documentar y validar. La visibilidad está implementada en
`port/`; este documento la contrasta con el original. **No** se modifica el
recorrido de `port/` más allá de lo que la validación demuestra, **no** se
crean buffers, shaders ni código D3D11, **no** se toca `C:\GEX_REZ` ni
ningún binario o asset. La validación dinámica se hizo sobre una **copia de
trabajo** aislada del juego, con puntos de interruptión de hardware y sin
parchear el ejecutable.

**Binario de referencia:** `C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE` (PE32 i386,
`imageBase = 0x00400000`), desensamblado completo con `llvm-objdump`.

**Referencias:** [`U1H_RESULTADOS.md`](U1H_RESULTADOS.md),
[`U1_CONSOLIDACION.md`](U1_CONSOLIDACION.md),
[`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md),
[`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md),
[`U1_DFX_RELOCATION_AUDIT.md`](U1_DFX_RELOCATION_AUDIT.md).

**Niveles de confianza usados en este documento:**

| Marca | Significado |
|---|---|
| **[A]** | Demostrado instrucción a instrucción en este build. |
| **[B]** | Deducido con evidencia sólida; algún detalle no cerrado. |
| **[C]** | Hipótesis o desconocido; se declara explícitamente. |

---

## 0. Resumen ejecutivo

1. `0x00401C00` es `void walk(const void *desc, const camera *cam, list *out)`,
   **cdecl**, 3 argumentos en pila. El **segundo** argumento es la cámara; el
   primero es un descriptor cuya primera dword es la raíz del BSP. **[A]**
2. La visibilidad **no** usa planos almacenados en `cam+0xC8/0xD4/0xD8`. Usa
   **cinco** filas de dos matrices `int16` de 3×3: `cam+0x88` (matriz **A**) y
   `cam+0xA8` (matriz **B**). Cada fila se aplica al centro del nodo y al origen
   de cámara por separado con `0x0046999C`. **[A]**
3. La prueba es una **esfera contra seis semiplanos** (near, far y cuatro
   laterales) con radio `r` y tolerancia `-r`. La fila 2 de B (`cam+0xB4..0xB8`)
   **no se usa** en ninguna comparación. **[A]**
4. El predicado de hoja del original es `(word[node+0x12] & 1) == 0`, no
   `& 2`. Para los tipos reales `1`/`2` ambos coinciden; no son equivalentes en
   general. **[A]**
5. Las matrices A y B son la matriz de orientación `M = *[cam+0xE8]` aplicada a
   los vectores locales de cámara construidos por `0x00402DC0`: eje de avance y
   cuatro normales de plano normalizadas de forma **aproximada**. **[A]/[B]**
6. `cam+0x5C` es un `int32` (`0x2EE0` = 12 000) que actúa como **distancia de
   corte lejana**. **[B]**
7. La pila del recorrido vive en `0x647740` con centinela `stack[0] =
   &stack[0]` y **sin comprobación de capacidad**. **[A]**
8. La cadena de orientación `0x46DD40 → 0x46E643 → 0x405C41` está reconstruida:
   rotaciones Euler **Z → Y → X** (pre-multiplicación) sobre `cam+0x30/0x32/0x34`
   (rama normal) o `cam+0x128/0x12A/0x12C` (rama alternativa si
   `[0x551644+0xa0] & 0x8`), **con los ángulos negados** (`0x46DE51`,
   `0x46DE67`, `0x46DE7D`), base `*[cam+0xF0]` o identidad si `cam+0x43C != 0`,
   cambio de ejes `M2 = 4096·[[1,0,0],[0,0,−1],[0,1,0]]` aplicado a las
   **columnas** (`O[r][c] = (M2 · column_c(R))[r]`), y
   `M = *[cam+0xE8] = transpose(O)`. A y B se construyen aplicando M a los
   vectores locales de `0x00402DC0`. **[A]**, confirmado dinámicamente
   (§14: el port reproduce bit a bit M, A, B, a_cam y b_cam del original).

---

## 1. `0x00401C00` — firma, convención y marco de pila

### 1.1 Firma

```
void __cdecl walk(const void *desc, const camera *cam, list *out);
```

Llamador único: `0x0043DECE`, dentro de `0x0043DC39`, justo antes de
`0x004269C2`. El llamador empuja, en este orden:

```
push [0x64FEA4]        ; out  = lista de hojas visibles
push level[+0x08]      ; cam  = cámara del nivel
push *[level+0x04]     ; desc = descriptor (su primera dword es la raíz)
call 0x00401C00
```

`desc[0]` es el puntero/offset a la raíz del BSP. `list` es
`[0x64FEA4]`: `dword[0]` = contador, `dword[1..]` = punteros a nodos, reservada
en `0x0043B047` con `0x2004` B (4 B de contador + `0x800` punteros). **[A]**

### 1.2 Prólogo y variables locales

```
00401C00: push ebp
00401C01: mov  ebp, esp
00401C03: sub  esp, 0x24
```

| Local | Valor | Uso |
|---|---|---|
| `[ebp-0x24]` | `0x006476C0` | matriz del plano (scratch, 16 B) |
| `[ebp-0x20]` | nodo actual | se recarga en cada iteración |
| `[ebp-0x1C]` | puntero de pila | arranca en `0x00647740` |
| `[ebp-0x18]` | `cam + 0x88` | base de la matriz **A** |
| `[ebp-0x14]` | `0x00647710` | resultado de la prueba de plano (12 B) |
| `[ebp-0x10]` | `0x00647720` | `{A·camPos, B·camPos}` (24 B) |
| `[ebp-0xC]` | `desc[0]` | raíz |
| `[ebp-0x8]` | `0x00647700` | `B·centro` (12 B) |
| `[ebp-0x4]` | `0x006476F0` | `A·centro` (12 B) |

Globales usadas:

| VA | Contenido |
|---|---|
| `0x00646BE0..0x00646BF0` | matriz 3×3 `int16` de trabajo (9 palabras) |
| `0x00646820/22/24` | vector `int16` de entrada (3 palabras) |
| `0x006474B0/B4/B8` | salida `int32` de `0x46999C` |
| `0x00647520/24/28` | vector `int32` de entrada del segundo bloque |
| `0x00646B80/84/88` | salida `int32` del segundo bloque |
| `0x00647740` | pila explícita del recorrido |

### 1.3 Pila explícita

Al entrar:

```
stack = 0x00647740
stack[0] = stack          ; centinela
sp = stack + 1            ; [ebp-0x1C]
*sp = desc[0]             ; raíz
out->count = 0
```

Bucle principal (`0x00401E31`):

```
if (*sp == sp) return;    ; pila vacía
node = *sp;
sp -= 4;
```

Al apilar un hijo: `sp += 4; *sp = hijo`. **No hay comparación con un límite
superior**: un árbol con más profundidad que el espacio reservado desborda
`0x647740` hacia arriba. **[A]**

---

## 2. Estructura de cámara: campos implicados

La cámara es un objeto opaco de al menos `0x43C` B. Solo se documentan los
campos que participan (directa o indirectamente) en la visibilidad.

| Offset | Tipo | Valor / contenido | Escritor | Lector | Conf. |
|---|---|---|---|---|---|
| `+0x00/+0x02/+0x04` | `int16` ×3 | posición de cámara (mundo) | juego / animación | `0x401C00`, `0x405C41`, `0x46DD40` | **[A]** |
| `+0x30/+0x32/+0x34` | `int16` ×3 | ángulos Euler (rama normal) | juego | `0x46DD40` | **[B]** |
| `+0x40` | `int16` | `0x140` = 320; distancia focal local | init `0x404A03…` | `0x402DC0` (como `f`) | **[A]** |
| `+0x44` | `int16` | `0x200` = 512; **no** lo lee `0x402DC0` | init `0x404A03…` | otros consumidores | **[C]** |
| `+0x48` | `int32` | `0x180` = 384; ancho base | init `0x404A03…` | `0x402DC0` (como `w`) | **[A]** |
| `+0x5C` | `int32` | `0x2EE0` = 12 000; **corte lejano** | init `0x404AD2` | `0x401C00` | **[B]** |
| `+0x60/+0x62/+0x64` | `int16` ×3 | `(0, 0, 0x1000)` eje de avance local | `0x402DC0` | `0x405C41` | **[A]** |
| `+0x68/+0x6A/+0x6C` | `int16` ×3 | normal local nº 1 (abajo) | `0x402DC0` | `0x405C41` | **[A]** |
| `+0x70/+0x72/+0x74` | `int16` ×3 | normal local nº 2 (izquierda) | `0x402DC0` | `0x405C41` | **[A]** |
| `+0x78/+0x7A/+0x7C` | `int16` ×3 | normal local nº 3 (derecha) | `0x402DC0` | `0x405C41` | **[A]** |
| `+0x80/+0x82/+0x84` | `int16` ×3 | normal local nº 4 (arriba) | `0x402DC0` | `0x405C41` | **[A]** |
| `+0x88..+0x98` | `int16` ×9 | matriz **A** (filas: avance, abajo, izquierda) | `0x405C41` | `0x401C00` | **[A]** |
| `+0x9A..+0xA6` | — | relleno / no usado por la visibilidad | — | — | **[B]** |
| `+0xA8..+0xB2` | `int16` ×6 | filas 0–1 de **B** (derecha, arriba) | `0x405C41` | `0x401C00` | **[A]** |
| `+0xB4/+0xB6/+0xB8` | `int16` ×3 | fila 2 de **B** | **no localizado** | `0x405C41` (solo para `+0xD4`) | **[C]** |
| `+0xC8/+0xCC/+0xD0` | `int32` ×3 | `A · camPos` | `0x405C41` | **no** lo lee `0x401C00` | **[A]** |
| `+0xD4/+0xD8/+0xDC` | `int32` ×3 | `B · camPos` | `0x405C41` | **no** lo lee `0x401C00` | **[A]** |
| `+0xE0` | ptr | orientación intermedia | `0x46DD40` | `0x46E643` | **[B]** |
| `+0xE4` | ptr | orientación intermedia `O` | `0x46DD40` | `0x46E643` | **[B]** |
| `+0xE8` | ptr | matriz **M** (3×3 `int16`) | `0x46E643` | `0x405C41` | **[A]** |
| `+0xF0` | ptr | orientación base (identidad si `cam+0x43C != 0`) | juego | `0x46DD40` | **[B]** |
| `+0x104` | `int32` | `0x1000` | init `0x404A03…` | no localizado | **[C]** |
| `+0x14/+0x18/+0x1C` | `int32` ×3 | `O · (−camPos)` (traslación de O) | `0x46DD40` | no leído por `0x401C00` | **[A]** |
| `+0xA4/+0xA6/+0xA8` | `int16` ×3 | `−camPos` (vector traslación para `0x46DD40`) | — | `0x46DD40` | **[A]** |
| `+0x130/+0x132/+0x134` | `int16` ×3 | copia de `cam+0x00/0x02/0x04` (posición) | `0x46DD40` | `0x46DD40` | **[A]** |
| `+0x128/+0x12A/+0x12C` | `int16` ×3 | ángulos Euler (rama alternativa de `0x46DD40`) | juego | `0x46DD40` | **[B]** |
| `+0x490/+0x492/+0x494` | `int16` ×3 | factores de escala por columna (Q12) | no localizado | `0x46DD40` | **[C]** |
| `+0x43C` | `int32` | bandera de modo de orientación | juego | `0x46DD40` | **[B]** |

**Tamaño real de los bloques:** `cam+0x88` y `cam+0xA8` son **9 `int16` cada
uno** (18 B por matriz), no «7 `int16`». El hueco entre ambas va de `+0x9A` a
`+0xA7`. **[A]**

**Matrices A y B son independientes**, no dos mitades de un mismo array: A ocupa
`+0x88..+0x98`, B ocupa `+0xA8..+0xB8`, con `+0x9A..+0xA6` sin uso.

---

## 3. `0x0046999C` — transformación 3×3 `int16` con `sar 12`

### 3.1 Firma y contrato

```
void transform3(void);   /* sin argumentos: trabaja sobre globales */
```

Entradas implícitas:

- matriz `int16` en `0x00646BE0..0x00646BF0` (fila `r`, columna `c` en
  `0x646BE0 + 2*(3r + c)`);
- vector `int16` en `0x00646820/22/24`.

Salidas:

- **bloque 1:** `int32` en `0x006474B0/B4/B8`;
- **bloque 2:** `int32` en `0x00646B80/84/88`.

### 3.2 Matemática exacta

Sea `m[r][c]` la matriz `int16` (con signo) y `v = (vx, vy, vz)` el vector
`int16` (con signo). El primer bloque calcula, en aritmética de 32 bits:

```
o0 = (m[0][0]*vx + m[0][1]*vy + m[0][2]*vz) sar 12   -> 0x6474B0
o1 = (m[1][0]*vx + m[1][1]*vy + m[1][2]*vz) sar 12   -> 0x6474B4
o2 = (m[2][0]*vx + m[2][1]*vy + m[2][2]*vz) sar 12   -> 0x6474B8
```

El segundo bloque repite **la misma** matriz con el vector `int32`
`0x00647520/24/28` y escribe `0x00646B80/84/88`:

```
p0 = (m[0][0]*u0 + m[0][1]*u1 + m[0][2]*u2) sar 12   -> 0x646B80
p1 = (m[1][0]*u0 + m[1][1]*u1 + m[1][2]*u2) sar 12   -> 0x646B84
p2 = (m[2][0]*u0 + m[2][1]*u1 + m[2][2]*u2) sar 12   -> 0x646B88
```

Detalles verificados:

- todas las entradas de la matriz y del vector `int16` se leen con `movsx`
  (signo); el vector `int32` del segundo bloque, con `imul` de 32 bits;
- el desplazamiento es `sar 12` (aritmético, redondeo hacia **−∞**), no `idiv`;
- **no hay saturación** ni `clamp` en ninguna de las seis salidas;
- **no hay traslación**: no se suma ningún vector de origen;
- no se comprueba desbordamiento: la suma de tres productos `int16×int16`
  puede superar `2^31` en teoría (con valores de mundo reales no se observa);
- `0x0046999C` **no** modifica `0x646BE0` ni `0x646820`;
- `0x00469B02` es **otra** rutina (suma `0x646BB4` y satura); no se usa en la
  visibilidad.

### 3.3 Atajo útil

Como `m` es la matriz de orientación en Q12 (`4096` = 1.0), la transformación
equivale a `(M_q12 · v) >> 12`. Si `M` fuese una rotación exacta, el resultado
sería la rotación de `v` en unidades de mundo. Con los valores reales la
matriz es una aproximación (ver §5.4).

---

## 4. Matrices A y B: construcción (`0x00405C41`)

### 4.1 Firma

```
void build_visibility_matrices(camera *cam);
```

Llamada desde la cadena de orientación (`0x43E568`) después de `0x46E643`.
**[A]**

### 4.2 Secuencia exacta

Para cada transformación, `0x405C41` copia las **9 palabras** de
`M = *[cam+0xE8]` (offsets `+0x00, +0x02, +0x04, +0x06, +0x08, +0x0A, +0x0C,
+0x0E, +0x10`) a `0x646BE0..0x646BF0`, carga el vector en `0x646820`, llama a
`0x46999C` y guarda el resultado **truncado a palabra**:

| # | Vector de entrada | Destino | Filas |
|---|---|---|---|
| 1 | `cam+0x60/+0x62/+0x64` (avance) | `cam+0x88/+0x8A/+0x8C` | A fila 0 |
| 2 | `cam+0x68/+0x6A/+0x6C` | `cam+0x8E/+0x90/+0x92` | A fila 1 |
| 3 | `cam+0x70/+0x72/+0x74` | `cam+0x94/+0x96/+0x98` | A fila 2 |
| 4 | `cam+0x78/+0x7A/+0x7C` | `cam+0xA8/+0xAA/+0xAC` | B fila 0 |
| 5 | `cam+0x80/+0x82/+0x84` | `cam+0xAE/+0xB0/+0xB2` | B fila 1 |

Los cinco stores son `mov word ptr [...]`, es decir **truncan** el `int32` de
`0x46999C` a `int16` (sin saturación; un valor fuera de rango se corrompe).
**[A]**

Después:

| # | Matriz | Vector | Destino | Tipo |
|---|---|---|---|---|
| 6 | **A** (`cam+0x88..0x98`) | `cam+0x00/+0x02/+0x04` (posición) | `cam+0xC8/+0xCC/+0xD0` | `int32` |
| 7 | **B** (`cam+0xA8..0xB8`, 9 palabras) | `cam+0x00/+0x02/+0x04` | `cam+0xD4/+0xD8/+0xDC` | `int32` |

Los pasos 6 y 7 escriben **dwords** (sin truncar). **[A]**

### 4.3 Consecuencia

- A está **completa** (3 filas) y se construye a partir de tres vectores
  locales.
- B recibe escritura en sus filas 0 y 1 desde `0x402DC0`; su **fila 2**
  (`cam+0xB4..0xB8`) solo aparece como **entrada** del paso 7. No se ha
  localizado ningún escritor de `cam+0xB4..0xB8` en las rutas de cámara
  (`0x401C00`, `0x402DC0`, `0x405C41`, `0x46DD40`, `0x46E643`). Las escrituras
  `mov word ptr [...+0xB4]` localizadas (`0x491DF5`, `0x4922A2`) operan sobre
  **otro** objeto, no sobre la cámara. **[A]/[C]**
- `cam+0xC8` y `cam+0xD4` (y sus vecinos) **no los lee** `0x401C00`: son
  resultados que otros consumidores pueden usar, pero no la visibilidad del
  recorrido.

### 4.4 Orientación de las filas

Con `0x46999C` como `M · v >> 12`, las filas de A/B son la matriz `M` aplicada
a los vectores locales. Es decir, **A/B no son la matriz de vista**: son
**normales de plano en espacio de mundo**, una por fila. **[A]/[B]**

---

## 5. Vectores locales de cámara (`0x00402DC0`)

### 5.1 Firma y entradas

```
void build_local_frame(camera *cam);
```

Llamada desde `0x00403A35` y `0x0043B5D9`. No llama a ninguna otra función (es
autocontenida). **[A]**

Entradas:

- `w = cam+0x48` (`int32`, `0x180`);
- `f = (int16)cam+0x40` (`0x140`);
- `cam+0x44` **no se lee**.

Derivados:

```
half = trunc(w / 2)      = 0xC0  = 192
a    = trunc(w / 3) * 2  = 0x100 = 256
b    = half              = 192
f    = 0x140             = 320
```

`trunc` es división entera de C (`idiv`, hacia cero), no `sar`. **[A]**

### 5.2 Rayos de esquina

```
v1 = (-a, -b, f)
v2 = ( a, -b, f)
v3 = (-a,  b, f)
v4 = ( a,  b, f)
```

### 5.3 Productos vectoriales

| Destino | Cálculo | Resultado simbólico | `sar 12` |
|---|---|---|---|
| `cam+0x60..0x64` | constante | `(0, 0, 0x1000)` | — |
| `cam+0x68..0x6C` | `v1 × v2` | `(0, +2af, +2ab)` | `(0, +40, +24)` |
| `cam+0x70..0x74` | `v3 × v1` | `(+2bf, 0, +2ab)` | `(+30, 0, +24)` |
| `cam+0x78..0x7C` | `v2 × v4` | `(−2bf, 0, +2ab)` | `(−30, 0, +24)` |
| `cam+0x80..0x84` | `v4 × v3` | `(0, −2af, +2ab)` | `(0, −40, +24)` |

Los valores numéricos son los de `w = 0x180`, `f = 0x140`, **después de
`sar 12` y antes de la normalización**. El producto vectorial se calcula en
`int32` (dos `imul` + `sub`), se desplaza con `sar 12` y se **trunca a `int16`**
al almacenarlo; solo entonces se aplica §5.4. Con esos mismos valores, las
normales finales almacenadas son:

| Destino | Norma final (Q12) |
|---|---|
| `cam+0x68..0x6C` | `(0, 3523, 2114)` |
| `cam+0x70..0x74` | `(3321, 0, 2656)` |
| `cam+0x78..0x7C` | `(-3321, 0, 2656)` |
| `cam+0x80..0x84` | `(0, -3523, 2114)` |

**[A]**

Semántica geométrica **[B]**:

- `cam+0x60` = eje de avance local (`+Z`);
- `cam+0x68` = normal del plano inferior (contiene `v1` y `v2`);
- `cam+0x70` = normal del plano izquierdo (contiene `v3` y `v1`);
- `cam+0x78` = normal del plano derecho (contiene `v2` y `v4`);
- `cam+0x80` = normal del plano superior (contiene `v4` y `v3`).

La asignación «abajo/izquierda/derecha/arriba» depende de la convención de ejes
del motor; lo **demostrado** es qué par de rayos genera cada normal.

### 5.4 Normalización aproximada

Cada una de las cuatro normales se normaliza con el mismo bloque:

```
max, mid, min = ordenar_desc(|gx|, |gy|, |gz|)     ; red XOR-swap
approx = (30*max + 12*mid + 9*min) / 8            ; cdq/and/add/sar 3
if (approx != 0) {
    gx = (gx << 14) / approx                       ; idiv (trunc)
    gy = (gy << 14) / approx
    gz = (gz << 14) / approx
}
```

Detalles verificados:

- la ordenación es un **XOR-swap** de tres elementos, no una comparación
  estable;
- el cociente es `idiv` (truncamiento hacia cero), no `sar`;
- si `approx == 0` (vector nulo) **no se toca** el vector: queda `(0,0,0)`;
- **no hay saturación** del resultado;
- `<< 14` puede desbordar para componentes grandes; con los valores reales no
  ocurre.

La fórmula apunta a `approx ≈ 4·|g|` para luego escalar por `2^14` y obtener
Q12 (`4096 ≈ 1.0`). El error de magnitud depende de la distribución:

| Caso | `approx/|g|` | `|n|` resultante | Error |
|---|---|---|---|
| una componente | 3.75 | ≈ 4369 | +6.7 % |
| dos iguales | ≈ 3.82 | ≈ 4290 | +4.7 % |
| tres iguales | 3.68 | ≈ 4452 | +8.7 % |

Es decir, **la normalización no es exacta**: la magnitud queda entre ~4200 y
~4450 en lugar de 4096 (hasta ≈ 9 % de error). Esto importa para la
comparación con `-r`: la tolerancia efectiva del radio no es exactamente `r`.
Se documenta como comportamiento original, no como error del port. **[A]**

`cam+0x44` (`0x200`) no interviene en esta construcción. **[A]**

---

## 6. Cadena de orientación `0x46DD40` → `0x46E643`

### 6.1 `0x0046DD40(camera *cam)` — matriz de orientación O

Construye la orientación de la cámara y la escribe en `*[cam+0xE4]` (por
columnas) y `*[cam+0xE0]`.

**Paso 0 — selección de ángulos (líneas `0x46dd7e..0x46dd8e`).**
Se lee el byte global `[0x00551644 + 0xa0]`:
- si `([0x551644+0xa0] & 0x8) != 0` → **rama alternativa**: ángulos
  `cam+0x128/0x12A/0x12C`.
- si `== 0` → **rama normal**: ángulos `cam+0x30/0x32/0x34`. **[A]**

**Paso 1 — matriz base (líneas `0x46dd94..0x46de3c`).**
- Si `cam+0x43C != 0` (`0x46dd97`), la base es la **identidad** (`0x464eca`).
  **[B]**
- Si `cam+0x43C == 0`, la base es `*[cam+0xF0]` (9 palabras, copiadas a
  `ebp-0x54`). **[A]**

**Paso 2 — rotaciones Euler (líneas `0x46de4a..0x46df3d`).**
Se aplican **tres rotaciones por pre-multiplicación** sobre la matriz
acumulativa `ebp-0x54`, pasando los ángulos **sin negar**:

| Paso | Función | Eje | Ángulo (rama normal) | Ángulo (rama alternativa) |
|---|---|---|---|---|
| 1 | `0x0046CD38` | **Z** | `(int16)cam+0x34` | `(int16)cam+0x12C` |
| 2 | `0x0046C3A4` | **Y** | `(int16)cam+0x32` | `(int16)cam+0x12A` |
| 3 | `0x0046BA10` | **X** | `(int16)cam+0x30` | `(int16)cam+0x128` |

Cada función recibe `(matriz_ptr, ángulo)` y hace
`matriz = Rot_eje(ángulo) · matriz` (pre-multiplicación), con
`RotZ(θ)=[[c,-s,0],[s,c,0],[0,0,1]]`,
`RotY(θ)=[[c,0,s],[0,1,0],[-s,0,c]]` y
`RotX(θ)=[[1,0,0],[0,c,-s],[0,s,c]]`, donde `s=sin(θ)`, `c=cos(θ)`. El ángulo
está en unidades Q12 de giro (`0x1000` = una vuelta completa). Las secuencias
`neg` que aparecen en `0x46CD38`/`0x46C3A4`/`0x46BA10` pertenecen a la
reducción de cuadrante del **lookup** de seno/coseno, no a una negación del
ángulo por parte del builder. Las tablas globales son `0x00646C20` (sin) y
`0x00645C20` (cos) (BSS, inicializadas en runtime). **[A]**

**Ejes verificados por la matriz que cada función construye:**
- `0x46CD38` → fila 2 = `(0, 0, 0x1000)` → **eje Z**. **[A]**
- `0x46C3A4` → fila 1 = `(0, 0x1000, 0)` → **eje Y**. **[A]**
- `0x46BA10` → fila 0 = `(0x1000, 0, 0)` → **eje X**. **[A]**

Producto acumulado:

```
R = RotX(cam+0x30) · RotY(cam+0x32) · RotZ(cam+0x34) · base              (rama normal)
R = RotX(cam+0x128) · RotY(cam+0x12A) · RotZ(cam+0x12C) · base           (rama alternativa)
```

Es un orden Euler **Z → Y → X** (aplicadas Z primero, X última), con los
ángulos **crudos**. **[A]**

**Paso 3 — cambio de ejes M2 (líneas `0x46dfa2..0x46e0a4`).**
Se construye y aplica por columnas:

```
M2 = 4096 · [ 1  0  0 ]
            [ 0  0 -1 ]
            [ 0  1  0 ]
```

`O_col_k = M2 · R_col_k` para cada columna k, usando `0x004698B0`
(`o = (M · v) >> 12`, Q12). Por tanto:

```
O = M2 · R
```

`O` se escribe **por columnas** en `*[cam+0xE4]` (offsets 0/6/C = columna 0,
2/8/E = columna 1, 4/A/10 = columna 2). **[A]**

**Paso 4 — traslación de O (líneas `0x46e4c6..0x46e63c`).**
Se calcula `O · (−camPos)` con `0x0046999C` (matriz `*[cam+0xE4]`, vector
`−(cam+0x00/0x02/0x04)`), resultado `int32` sin truncar, guardado en
`cam+0xE4 + 0x14/0x18/0x1C` (traslación de O) y también en `cam+0x14/0x18/0x1C`.
**[A]**

El escalado por columnas con `cam+0x490/0x492/0x494` (líneas `0x46e340..0x46e448`)
se aplica a `*[cam+0xE0]` con `imul` + `sar 0xc`; su consumidor no se ha
cerrado y no afecta a `M = *[cam+0xE8]`. **[C]**

### 6.2 `0x0046E643(dst, src)` — matriz final M

```
void orientation_finalize(matrix *dst, const matrix *src);
```

Llamada desde `0x0043E54C` con `dst = *[cam+0xE8]`, `src = *[cam+0xE4]`.
El llamador pone previamente `src->word[+0x12] = 0` (`0x43e539`), por lo que
**no** se toma la rama de ortogonalización (`0x46E898`). **[A]**

1. `dst->rot = transpose(src->rot)` (9 palabras). **[A]**
2. `t = −(src->tx, src->ty, src->tz)`. **[A]**
3. `dst->rot` se copia a `0x646BE0`; `t` a `0x647520/24/28`; `0x0046999C`
   calcula `dst->rot · t >> 12` en `0x646B80/84/88`. **[A]**
4. `dst->tx/ty/tz = 0x646B80/84/88`. **[A]**

Por tanto la matriz rotacional de M es:

```
M = transpose(O) = transpose(M2 · R) = R^T · M2^T
```

con

```
M2^T = 4096 · [ 1  0  0 ]
              [ 0  0  1 ]
              [ 0 -1  0 ]
```

`M = *[cam+0xE8]` es una matriz 3×3 `int16` en Q12. **[A]**

La traslación de M se descarta para la visibilidad: `0x405C41` solo lee las 9
palabras rotacionales de `*[cam+0xE8]`. **[A]**

### 6.3 `0x004698B0` — rotar un vector por la matriz global

```
void rot_vector(void);   /* lee 0x646BE0 (matriz) y 0x6474E0 (vector),
                            escribe 0x6474F0/4/8 */
```

Usado por `0x46DD40` para aplicar M2 a las columnas de R. Matemática:

```
o[r] = (m[r][0]*v[0] + m[r][1]*v[1] + m[r][2]*v[2]) sar 12   (r = 0,1,2)
```

Idéntica a `0x0046999C` pero con los globales `0x6474E0/24/28` (entrada) y
`0x6474F0/4/8` (salida). `sar 12`, sin saturación. **[A]**

### 6.4 Cadena completa en el frame

```
0x43E521  eax = level+0x8                  ; cam
0x43E525  call 0x46DD40(cam)               ; O = M2·R en *[cam+0xE4],
                                             traslación en cam+0xE4+0x14/18/1c
0x43E539  word[*[cam+0xE4] + 0x12] = 0     ; desactiva ortogonalización
0x43E54B  push *[cam+0xE4]                 ; src
0x43E552  push *[cam+0xE8]                 ; dst
0x43E559  call 0x46E643(dst, src)          ; M = *[cam+0xE8] = transpose(O)
0x43E568  call 0x405C41(cam)               ; A, B, cam+0xC8 = A·camPos,
                                             cam+0xD4 = B·camPos
```

Después de esta cadena, `cam+0x88..0x98` (A) y `cam+0xA8..0xB2` (B filas 0–1)
están listas para `0x401C00`. **[B]**

---

## 7. La prueba de visibilidad

### 7.1 Preparación (una vez por recorrido)

```
A_cam[3] = transform3(A, camPos)      ; -> 0x647720/24/28
B_cam[3] = transform3(B, camPos)      ; -> 0x64772C/30/34
plane[0..4] = camPos                   ; -> 0x6476C0 (solo palabras 0,2,4)
```

`A_cam` y `B_cam` son `int32`. `plane` es una matriz `int16` de 16 B cuyo único
renglón distinto de cero es el primero (`cam.x, cam.y, cam.z`); las filas 1 y 2
no se inicializan en `0x401C00`. **[A]**

### 7.2 Por nodo

Sea `c = (word[node+0x00], word[node+0x02], word[node+0x04])` el centro
`int16` con signo, `r = (uint16)word[node+0x06]` el radio **sin signo**, y

```
T_A = transform3(A, c)
T_B = transform3(B, c)
```

Las **cinco** filas se aplican así (el índice `i` es la fila de la matriz y el
componente de `T`):

| # | Expresión | Comparación que **descarta** | Semántica |
|---|---|---|---|
| 1 | `T_A.x − A_cam.x` | `<= −r` | near |
| 2 | `T_A.x − A_cam.x` | `>= cam+0x5C + r` | far |
| 3 | `T_A.y − A_cam.y` | `<= −r` | lateral A1 |
| 4 | `T_A.z − A_cam.z` | `<= −r` | lateral A2 |
| 5 | `T_B.x − B_cam.x` | `<= −r` | lateral B0 |
| 6 | `T_B.y − B_cam.y` | `<= −r` | lateral B1 |

Notas exactas:

- `T_A` se calcula **una vez**; cada componente se resta en el momento de su
  comparación. El original **no** calcula `A·(c − camPos)`.
- El radio se lee con `xor edx,edx; mov dx,[node+6]`, es decir **unsigned
  `int16`** (0…65535), y se niega con `neg edx`.
- `cam+0x5C` es `int32`; `cam+0x5C + r` se evalúa en 32 bits.
- `T_A.x` se escribe de vuelta en `0x6476F0` tras restarle `A_cam.x` (detalle sin
  efecto en el resto de pruebas).
- `B` fila 2 (`cam+0xB4..0xB8`) **no se compara nunca**.
- La prueba es **conservadora**: rechaza solo si la esfera está completamente
  fuera del semiplano (`distancia <= −r`). No hay recorte de polígonos en esta
  función.

### 7.3 Fórmula equivalente

```
visible(c, r) =
      (T_A.x − A_cam.x) >  −r
   && (T_A.x − A_cam.x) <   cam+0x5C + r
   && (T_A.y − A_cam.y) >  −r
   && (T_A.z − A_cam.z) >  −r
   && (T_B.x − B_cam.x) >  −r
   && (T_B.y − B_cam.y) >  −r

con  T_A = (A · c) sar 12 ,  A_cam = (A · camPos) sar 12
     T_B = (B · c) sar 12 ,  B_cam = (B · camPos) sar 12
```

El volumen es la intersección de **seis semiplanos**: dos del eje de avance
(near/far) y cuatro laterales. Los cinco renglones de matriz implicados son A0,
A1, A2, B0 y B1; A0 se usa dos veces. **[A]**

### 7.4 Clasificación del nodo y orden de apilado

```
tipo = (int16)word[node+0x12]
if ((tipo & 1) == 0) {
    if (out->count < 0x800) {
        out->items[out->count] = node;
        out->count++;
    }
} else {
    d = ((camPos · N) sar 12) − (int32)node[+0x14]
    if (d >= 0) {
        if (node[+0x1C]) push(node[+0x1C]);
        if (node[+0x18]) push(node[+0x18]);
    } else {
        if (node[+0x18]) push(node[+0x18]);
        if (node[+0x1C]) push(node[+0x1C]);
    }
}
```

- **Hoja:** `(tipo & 1) == 0`. Los tipos reales son `1` (interno) y `2`
  (hoja); con ellos `(tipo & 1) == 0` y `(tipo & 2) != 0` coinciden. **[A]**
- La lista se anexa **sin comprobar el tipo otra vez**: si un nodo con
  `tipo & 1 == 0` pasa la visibilidad, entra. **[A]**
- **Orden de visita:** si `d >= 0` se visita primero `+0x18`; si `d < 0`, primero
  `+0x1C`. El hijo que queda arriba en la pila es el que se visita antes. **[A]**
- **Prueba de plano:** `N = (int16)word[node+0x0C..0x10]` (Q12) y
  `node[+0x14]` es `int32`. La matriz `plane` solo aporta su primer renglón
  (`camPos`), y de `0x46999C` solo se usa la salida `.x`; por tanto

```
d = ((cam.x*N.x + cam.y*N.y + cam.z*N.z) sar 12) − node.distance
```

  Las filas 1 y 2 de `plane` y las salidas `.y/.z` **no influyen** en la
  decisión. **[A]**

### 7.5 El descriptor y la lista

- `desc[0]` es la raíz (`int32`); `desc` es `*[level+0x04]`.
- `out` es `[0x64FEA4]`; capacidad fija `0x800`. Si se supera, las hojas
  adicionales **se descartan en silencio** (no hay error ni reasignación).
- `out->count` se pone a `0` al entrar; no se limpia el resto del array.

---

## 8. Pseudocódigo C fiel

```c
/* Resumen fiel de 0x00401C00. Los tipos reproducen el original:
   int16 con signo para posiciones/normales, uint16 para el radio,
   int32 para distancias y resultados de transformación. */

typedef struct { int16_t w[9]; } mat3_q12;   /* row-major, 4096 = 1.0 */

static void transform3(const mat3_q12 *m, const int16_t v[3], int32_t out[3])
{
    for (int r = 0; r < 3; r++)
        out[r] = (m->w[3*r+0] * (int32_t)v[0] +
                  m->w[3*r+1] * (int32_t)v[1] +
                  m->w[3*r+2] * (int32_t)v[2]) >> 12;   /* sar 12 */
}

typedef struct {
    int16_t  center[3];      /* +0x00 */
    uint16_t radius;         /* +0x06, unsigned */
    int16_t  normal[3];      /* +0x0C, Q12 */
    int16_t  type;           /* +0x12 */
    int32_t  distance;       /* +0x14 */
    uint32_t child_lo;       /* +0x18 */
    uint32_t child_hi;       /* +0x1C */
} bsp_node;

typedef struct {
    int32_t count;
    const bsp_node *item[0x800];
} visible_list;

void walk(const void *desc, const camera *cam, visible_list *out)
{
    const mat3_q12 *A = (const mat3_q12 *)((const uint8_t *)cam + 0x88);
    const mat3_q12 *B = (const mat3_q12 *)((const uint8_t *)cam + 0xA8);
    const int16_t  *cam_pos = (const int16_t *)cam;      /* +0x00 */
    const int32_t   far     = *(const int32_t *)((const uint8_t *)cam + 0x5C);

    int32_t A_cam[3], B_cam[3];
    int32_t T_A[3], T_B[3];
    int16_t plane[9] = {0};

    /* pila explícita: centinela + nodos */
    uintptr_t stack[4096];
    uintptr_t *sp = &stack[1];

    transform3(A, cam_pos, A_cam);
    transform3(B, cam_pos, B_cam);
    plane[0] = cam_pos[0];
    plane[1] = cam_pos[1];
    plane[2] = cam_pos[2];

    stack[0] = (uintptr_t)&stack[0];
    *sp = *(const uint32_t *)desc;          /* desc[0] = raíz */
    out->count = 0;

    while (*(uintptr_t *)sp != (uintptr_t)sp) {
        const bsp_node *n = (const bsp_node *)*sp;
        --sp;

        const int32_t r = (int32_t)n->radius;   /* unsigned */

        transform3(A, n->center, T_A);
        if (T_A[0] - A_cam[0] <= -r) continue;
        if (T_A[0] - A_cam[0] >= far + r) continue;
        if (T_A[1] - A_cam[1] <= -r) continue;
        if (T_A[2] - A_cam[2] <= -r) continue;

        transform3(B, n->center, T_B);
        if (T_B[0] - B_cam[0] <= -r) continue;
        if (T_B[1] - B_cam[1] <= -r) continue;

        if ((n->type & 1) == 0) {               /* hoja */
            if (out->count < 0x800) {
                out->item[out->count] = n;
                out->count++;
            }
            continue;
        }

        /* prueba de plano: solo el renglón 0 de `plane` contribuye */
        int32_t d = (plane[0] * (int32_t)n->normal[0] +
                     plane[1] * (int32_t)n->normal[1] +
                     plane[2] * (int32_t)n->normal[2]) >> 12;
        d -= n->distance;

        if (d >= 0) {
            if (n->child_hi) *++sp = n->child_hi;   /* +0x1C */
            if (n->child_lo) *++sp = n->child_lo;   /* +0x18 */
        } else {
            if (n->child_lo) *++sp = n->child_lo;
            if (n->child_hi) *++sp = n->child_hi;
        }
    }
}
```

**Diferencias deliberadas del pseudocódigo respecto al original (no son
fidelidad, son legibilidad):**

- `stack[4096]` es una cota ilustrativa; el original **no** tiene cota.
- `plane` se declara como 9 palabras; el original usa 16 B en `0x6476C0`.
- Los `continue` reproducen los saltos a `0x402232`.

---

## 9. Incógnitas y límites

| Punto | Estado |
|---|---|
| `cam+0xB4/+0xB6/+0xB8` (B fila 2) | Sin escritor localizado en rutas de cámara. Alimenta `cam+0xD4`, que `0x401C00` no usa. **[C]** |
| Semántica de `cam+0x40` y `cam+0x44` | `+0x40` se usa como distancia focal `f`; `+0x44` no interviene en `0x402DC0`. Su consumidor real (p. ej. `0x427DA2`) no se ha trazado. **[C]** |
| Orden exacto de las rotaciones Euler en `0x46DD40` | **Demostrado:** Z → Y → X (pre-multiplicación, ángulos crudos) sobre `cam+0x30/0x32/0x34` (rama normal) o `cam+0x128/0x12A/0x12C` (rama alternativa si `[0x551644+0xa0] & 0x8`); base `*[cam+0xF0]` o identidad si `cam+0x43C != 0`. §6.1. **[A]** |
| Escalado por columna `cam+0x490/+0x492/+0x494` | Se aplica a `*[cam+0xE0]` con `imul` + `sar 0xc` en `0x46DD40`; su consumidor y significado no se han localizado. No afecta a `M = *[cam+0xE8]`. **[C]** |
| Consumidor de `cam+0x14/+0x18/+0x1C` | `0x46DD40` escribe `O·(−camPos)` allí; no se ha localizado quién lo lee. **[C]** |
| Consumidor de `cam+0x43C` | Actúa como bandera para forzar base identidad en `0x46DD40`; quién la escribe no se ha trazado. **[C]** |
| Sentido «front/back» y convención de ejes | Lo demostrado es la **desigualdad** (`> -r`, `< far + r`), no la etiqueta. Las etiquetas «abajo/izquierda/derecha/arriba» son **[B]**. |
| Escala real de la normalización | Aproximada (§5.4); error de hasta ~9 %. No se corrige: es el comportamiento original. **[A]** |
| `0x6476C0` compartido con colisión | `0x41A203` escribe palabras 0–0xA del mismo buffer. No afecta a la decisión porque solo se usa la salida `.x` (renglón 0), que `0x401C00` reescribe. **[A]** |
| Desbordamiento de la pila `0x647740` | No hay comprobación. Un árbol más profundo que el espacio disponible corrompe memoria. No se ha medido el espacio reservado. **[A]/[C]** |
| Radio como `uint16` | Se usa sin signo; un radio `> 0x7FFF` es legal en el código aunque no se ha observado en datos. **[A]** |

> **Consumidor de `cam+0xE0` localizado (2026-11-10).** La entrada «escalado por
> columna» deja de estar sin consumidor conocido: `0x004269C2` (emisión de hojas
> BSP) copia `*[cam+0xE0]` —3×3 `int16` en Q12 en `+0x00..+0x10` y traslación
> `int32` en `+0x14/+0x18/+0x1C`— a `0x646BE0..0x646BF0` y
> `0x646BB4/B8/BC` antes de proyectar con `0x46A1D0` (42267-42313). Es, por
> tanto, la matriz **de proyección/emisión**, distinta de `M = *[cam+0xE8]`
> que usa la visibilidad. La relación entre ambas no se ha trazado; el
> significado del escalado por columna sigue **[C]**. La ruta de emisión está
> en [`S2_BSP_TRAVERSAL.md`](S2_BSP_TRAVERSAL.md) §5.5.

---

## 10. Discrepancias con la documentación previa

| Documento | Afirmación previa | Estado tras esta ronda |
|---|---|---|
| `U1H_RESULTADOS.md` §6.1 | «`cam+0x88` (7 `int16`)» | **Incorrecto:** A son 9 `int16` (`+0x88..+0x98`) y existe una segunda matriz B de 9 `int16` (`+0xA8..+0xB8`). |
| `U1H_RESULTADOS.md` §6.1 | «planos laterales (`cam+0xC8`, `cam+0xD4`, `cam+0xD8`)» | **Incorrecto:** `+0xC8` y `+0xD4` son `A·camPos` y `B·camPos` (`int32`); `0x401C00` no los lee. Los laterales son filas de A y B. |
| `U1H_RESULTADOS.md` §6.1 | «hoja (`word[+0x12] & 2`)» | **Engañoso:** el predicado real es `(word[+0x12] & 1) == 0`. Coincide con `& 2` solo porque los tipos son 1/2. |
| `U1H_RESULTADOS.md` §6.1 | «comparaciones con `-radio`, `radio + cam[+0x5C]` y los planos laterales» | Se precisa: 6 comparaciones (near, far y 4 laterales) sobre 5 renglones; B fila 2 nunca se usa. |
| `S2_BSP_TRAVERSAL.md` §2 | «`+0x12` … bit 1 = hoja» | El bit probado es el 0 (`& 1 == 0`). El bit 1 distingue el tipo 2 en los datos, pero no es el que prueba el código. |
| `S2_BSP_TRAVERSAL.md` §3 | «Nodo interno apila `+0x18` y `+0x1C` si no son nulos» | Falta el orden: depende de `d` (plano). Se corrige en §7.4 de este documento. |
| `S2_BSP_TRAVERSAL.md` §4 | «matriz de vista desde `cam+0x88` (7 `int16`), `cam+0x5C`, planos en `cam+0xC8/+0xD4/+0xD8`» | Sustituido por el modelo A/B de §4–§7. |
| `U1_CONSOLIDACION.md` §4.1 | «Nodo BSP … tipo `+0x12` = 1/2» | Correcto; se añade que el predicado es `& 1 == 0` y que la lista no revalida el tipo. |
| `docs/D3D11_FIRST_SCENE_PLAN.md` §3.2 | «Recorrido BSP — Clipping por viewport (implementar después)» | El original **no** hace clipping en `0x401C00`: hace *culling* de esfera contra 6 semiplanos. El recorte por coordenadas proyectadas está en `0x4269C2` (emisión). |
| `docs/D3D11_FIRST_SCENE_PLAN.md` §4.2 | «El algoritmo BSP original no incluye clipping explícito» | **Correcto**; se mantiene, pero conviene distinguirlo del *culling* de esfera. |

Las correcciones se registran aquí y en las notas de los documentos afectados;
**no** se reescribe el histórico de `U1H_RESULTADOS.md` ni de
`U1_CONSOLIDACION.md`.

---

## 11. Implementación en `port/` (2026-11-09)

La cadena de orientación y la prueba de visibilidad están implementadas en
`port/`:

- `port/include/gex/camera.h` — API pública (estructura `gex_camera`, tablas
  trigonométricas, rotaciones, construcción de A/B, prueba de visibilidad).
- `port/src/camera/camera.c` — implementación: `gex_cam_init_tables()`,
  `gex_cam_build_orientation()` (0x46DD40), `gex_cam_orientation_finalize()`
  (0x46E643), `gex_cam_build_local_frame()` (0x402DC0),
  `gex_cam_build_visibility_matrices()` (0x405C41), `gex_cam_update()`,
  `gex_cam_sphere_visible()`, `gex_cam_plane_distance()`.
- `port/tests/test_camera.c` — 889 comprobaciones deterministas: valores
  exactos de las tablas, matrices de rotación exactas por eje (ángulos
  positivos, negativos y con vuelta), composición Z→Y→X, rama alternativa y
  base, valores exactos del marco local, cableado M/A/B con matrices no
  triviales, `a_cam`/`b_cam`, preservación de B fila 2, límites de los seis
  semiplanos, radio unsigned, orden de hijos, predicado de hoja, aritmética de
  `gex_cam_plane_distance()` con wrap de 32 bits (productos y sumas fuera de
  rango, `sar 12` negativo, resta de `distance` con wrap) y comparación con un
  modelo de referencia x86 independiente sobre casos deterministas.
- `port/tests/test_bsp.c` — 37 comprobaciones, 0 fallos; AZTEC16 = 815 hojas.
  Integrado en `build.ps1 test` y en el objetivo `test` del `Makefile` con
  `-DGEX_BSP_TEST_FAULT_INJECT`, `src/asset/dfx.c`, `src/asset/sag.c` y
  `-lm`. Incluye la reinterpretación de la distancia cruda (`0x14`) con el bit
  alto activo (`0x80000000` → `INT32_MIN`) y el orden de hijos resultante.
- `port/src/bsp/bsp.c` — `gex_bsp_walk()` acepta ahora `cam_ctx` opcional: cuando
  se proporciona, aplica la prueba de esfera contra 6 semiplanos y el orden de
  apilado basado en `d = ((camPos·N) >> 12) − distance`. Predicado de hoja
  corregido a `(tipo & 1) == 0`.

Todas las pruebas pasan con ASan/UBSan sin comportamiento indefinido.

### Correcciones tras auditoría (2026-11-10)

La revisión estática del commit `a3f433e` contra el desensamblado encontró
cinco defectos en `port/src/camera/camera.c`; todos corregidos:

| Defecto | Corrección |
|---|---|
| `build_visibility_matrices()` calculaba `a_cam` y `b_cam` con **M** en lugar de **A** y **B**. | `a_cam = transform3(A, pos)`, `b_cam = transform3(B, pos)`, con B completo (incluida su fila 2). |
| `sphere_visible()` calculaba `T_A` y `T_B` con **M**, y volvía a transformar `pos` con M. | `T_A = transform3(A, c)`, `T_B = transform3(B, c)`; se leen `cam->a_cam` / `cam->b_cam`. |
| `build_local_frame()` truncaba los productos vectoriales a `int16` **antes** de `sar 12`. | Producto en `int32`, `sar 12`, luego truncado a `int16`; después normalización. |
| La normalización ordenaba los **valores con signo**, permutando las componentes de salida. | Se ordenan solo las **magnitudes** (`|g|`) para `max/mid/min`; cada componente conserva su posición. |
| `tab_sin`/`reduce_angle` negaban el ángulo (`tab_sin(θ) = −sin θ`) y el builder volvía a negar los ángulos. | `tab_sin(θ)` y `tab_cos(θ)` usan las identidades de cuadrante correctas sobre la tabla de seno; el builder pasa los ángulos crudos. |

Además, `test_bsp.c` no se compilaba en `build.ps1`/`Makefile` y `test_camera`
no enlazaba `src/bsp/bsp.c`; ambas reglas están corregidas. La prueba BSP
sintética de orden de hijos se desacopló de la geometría del frustum (centros
en el origen), para que verifique solo el orden de apilado.

Resultados: `build.ps1 test` completo pasa; `test_camera` = **889 checks, 0
fallos**; `test_bsp` = **37 checks, 0 fallos** (AZTEC16 = 815 hojas); ambas
baterías también pasan con `clang -fsanitize=address,undefined` y con `gcc`.

La aproximación de las tablas trigonométricas sigue siendo una limitación
declarada: los valores se generan como `round(4096·sin/cos(2π·i/4096))` y **no**
se ha demostrado equivalencia bit a bit con las tablas retail.

### Auditoría aritmética: wrap de 32 bits (2026-11-10, segunda corrección)

La misma revisión encontró que la aritmética de 32 bits todavía dependía de
comportamiento indefinido o definido por la implementación en varios puntos.
El original x86 opera con registros de 32 bits que **envuelven** (`imul`/`add`/
`sub`/`sar`); C no permite ese desbordamiento en tipos con signo. Correcciones:

| Punto | Antes | Ahora |
|---|---|---|
| `gex_cam_plane_distance()` | Suma de tres productos en `int32_t` (desbordamiento signed) y `>> 12` sobre un valor con signo. | Acumulador `uint32_t` (wrap módulo 2³²), `sar12()` sobre los bits del registro y resta de `distance` con wrap explícito. |
| `sar12()` | `v >> 12` sobre `int32_t` negativo (definido por la implementación). | Desplazamiento sobre `uint32_t` más máscara `0xFFF00000` cuando el bit 31 está activo; replica `sar` exacto hacia −∞. |
| `gex_cam_sphere_visible()` | `T_A[i] − a_cam[i]`, `−r` y `far + r` en signed `int32_t`. | `sub_wrap()`/`add_wrap()` con wrap de 32 bits sobre los bits del registro. |
| Conversiones `int32_t`↔`uint32_t` e `int16_t` | Casts de implementación definida (`(int32_t)acc`, `(int16_t)t`, `(int16_t)(-s)`). | `bits_i32()`, `bits_i16()` y `low_i16()` copian los bits con `memcpy`; `neg_i16()` reproduce el `neg` de 16 bits con wrap. |
| `bsp.c`: lectura de la distancia `+0x14` | `read_u32(..., (uint32_t *)&distance)`: violación de aliasing estricto y conversión de implementación definida. | Se lee en un `uint32_t` local y se reinterpreta con `bits_i32()`. Los centros y normales (`int16`) se reinterpretan igual con `bits_i16()`. |

Ninguna operación del camino de cámara o de recorrido depende ya de
desbordamiento con signo, de `>>` sobre negativos ni de conversiones entre
tipos de distinto signo no controladas. Las pruebas nuevas comparan
`gex_cam_plane_distance()` y `gex_cam_sphere_visible()` con un modelo de
referencia x86 escrito de forma independiente (512 y 256 configuraciones
deterministas respectivamente, con sesgo hacia los límites de `int16`), además
de casos fijos: suma superior a `INT32_MAX`, suma inferior a `INT32_MIN`,
resultado negativo antes de `sar 12`, `0 − INT32_MIN`, `±5 − INT32_MAX` y
`far + radio` con `far = INT32_MAX`.

### Decisiones de implementación

| Decisión | Motivación |
|---|---|
| Tablas `sin`/`cos` generadas matemáticamente en Q12 | Las tablas originales están en BSS (no en el PE); el código de inicialización en runtime no se localizó estáticamente. Se reproducen como `round(sin/cos(2π·i/4096)·4096)`. |
| `cam->use_alt_angles` en lugar de dirección `0x551644+0xa0` | La dirección global es específica del proceso original; el port expone un campo de struct en su lugar. |
| `cam->m` embebido (no puntero) | Evita punteros colgantes; la matriz M se copia directamente en la estructura de cámara. |
| B fila 2 (`cam+0xB4..0xB8`) | El original la **lee** para `b_cam` pero no se le localizó escritor en la ruta de cámara. El port **no la escribe**: preserva lo que el llamador dejó en `mat_b[6..8]`. Si la cámara se inicializa a cero, es determinista y vale 0. Nunca se compara en la visibilidad. |
| `cam+0xC8`/`cam+0xD4` (`A·camPos` / `B·camPos`) | Se calculan y se usan como origen de los semiplanos (`A_cam`/`B_cam`), igual que `0x401C00`. |
| Ángulos crudos en `gex_cam_build_orientation` | El builder original no niega los ángulos; las `neg` del desensamblado pertenecen al lookup trigonométrico. |
| Pila dinámica mediante `grow_frames` | El original no tiene cota; el port la mantiene para seguridad. |

## 12. Qué falta para implementar (fuera de alcance de esta ronda)

1. Validar el número de hojas visibles en AZTEC16 con una cámara concreta; sin
   traza dinámica del original no se puede contrastar el resultado exacto.
2. Integración con la emisión de polígonos (`0x4269C2`) y el renderer D3D11.
3. Declarar S-2 completo.

## 13. Tercera matriz: la de emisión `cam+0xE0` (2026-11-10)

La cadena documentada en §6 produce **tres** matrices, no dos. `cam+0xE4`,
`cam+0xE8` y `cam+0xE0` son punteros a buffers distintos:

| Campo | Tipo real | Escritor | Contenido |
|---|---|---|---|
| `cam+0xE0` | `ptr` → 0x20 B | `0x46DD40` | matriz de **emisión**: 9 `int16` Q12 row-major + traslación `int32[3]` |
| `cam+0xE4` | `ptr` → 18 B | `0x46DD40` | `O` = `M2·R` por columnas |
| `cam+0xE8` | `ptr` → 18 B | `0x46E643` | `M = transpose(O)` (visibilidad) |

`0x46DD40` escribe `+0xE0` y `+0xE4`; `0x46E643` deriva `+0xE8` de `+0xE4`.
Comparten la rotación de partida, pero `+0xE0` aplica además el cambio de ejes,
el escalado por columna (`cam+0x490/0x492/0x494`) y la traslación
`-camPos` transformada; `+0xE8` no tiene traslación.

Formato de la matriz de emisión (offsets dentro del buffer apuntado):

| Offset | Tipo | Fila |
|---|---|---|
| `+0x00/+0x02/+0x04` | `int16` | 0 → x de pantalla |
| `+0x06/+0x08/+0x0A` | `int16` | 1 → y de pantalla |
| `+0x0C/+0x0E/+0x10` | `int16` | 2 → profundidad |
| `+0x14/+0x18/+0x1C` | `int32` | traslación x/y/z |

Consecuencias para el puerto:

- `port/include/gex/camera.h` **no** modela `+0xE0`: define
  `GEX_CAM_PTR_O 0xE4`, `GEX_CAM_PTR_M 0xE8` y `gex_camera.m` (M embebida).
  `gex_camera.m` **no** puede usarse como matriz de emisión: le falta el cambio
  de ejes, el escalado por columna y la traslación, y su orientación es la
  transpuesta.
- El futuro `gex_level_emit` necesita un **contexto de emisión** propio
  (`gex_emit_mat3` + traslación), construido a partir de la misma rotación pero
  sin reutilizar `gex_camera.m`. Contrato propuesto en
  [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) §11.
- Dos consumidores del original copian la matriz a los globales
  `0x646BE0..0x646BF0`/`0x646BB4..0x646BBC` con una diferencia: `0x4269C2`
  (BSP) copia la traslación **tal cual** y `0x4013A0` (modelos) la copia
  **`sar 4`** porque los vértices de modelo están en Q4. El puerto debe
  mantener esa distinción en el contexto de emisión, no dentro de la cámara.

---

## 14. Validación dinámica contra el ejecutable original (2026-11-11)

Esta sección cierra (parcialmente) la validación de S-2 comparando la cadena de
cámara del port con el **proceso original en ejecución**, no solo con el
desensamblado. Todo lo que sigue está respaldado por lectura de memoria del
proceso vivo; el original **no** se modifica ni se parchea.

### 14.1 Método

- **Trazador propio** (`tools/frame-trace/tracer.cs` y su extensión temporal con
  volcado de memoria): puntos de interruptión de **hardware** (`Dr0..Dr3`),
  `ReadProcessMemory` para volcar estructuras, y guiones de teclas para
  navegar los menús hasta la ruta 3D. Cero cambios en el binario.
- **Copia de trabajo aislada** del juego (ejecutable original + `glide2x`
  real + `LEVEL`/`AUDIO`), fuera de `C:\GEX_REZ`, que solo se lee.
- **AZTEC16 como nivel de referencia**: el original construye
  `<base>\level\map7.dfx` (cadena en `0x551900`, `sprintf` en `0x463403`).
  Se copias `AZTEC16.DFX/VFX` como `MAP7.DFX/MAP7.VFX` en el `InstallDir`
  del registro para que la ruta 3D use ese nivel.
- **Ruta 3D alcanzada**: el guion de teclas documentado lleva a la misión 3D;
  a partir de ahí el original ejecuta, por frame, **1** recorrido BSP
  (`0x401C00`), **1** emisión (`0x4269C2`) y ~**1517** polígonos dibujados
  (219 998 draws en 145 frames, breakpoint en `0x46558A`).

### 14.2 Estado de cámara capturado (frame 1)

Cámara del original en `0x0065CD80`; matrices en `0x57B508` (O),
`0x57B528` (M) y `0x57B548` (base).

| Campo | Valor capturado |
|---|---|
| `+0x00` posición | `0x0925, 0x0FE7, 0x0C02` (2341, 4071, 3074) |
| `+0x30..0x34` Euler | `0xFE73, 0x0000, 0x0AF2` (−397, 0, 2802) |
| `+0x40/+0x44/+0x48` | `0x0140` / `0x0200` / `0x0180` |
| `+0x5C` corte lejano | `0x2134` (8500) |
| `+0x60..0x84` marco local | `(0,0,4096) (0,3523,2114) (3321,0,2656) (−3321,0,2656) (0,−3523,2114)` |
| `+0x88` matriz A | `3074 −1352 −2343 / −258 113 −4099 / 657 −3917 −1520` |
| `+0xA8` matriz B | `3329 2162 −1520 / 3430 −1509 1679 / 0 0 0` |
| `+0xC8` a_cam | `−1346 −3112 −4659` |
| `+0xD4` b_cam | `2910 1720 0` |
| `+0xE8` matriz M | `−1648 −2144 3074 / −3749 943 −1352 / 0 −3359 −2343` |
| `+0xF0` base | identidad (`4096` en 0, 4 y 8) |

La disposición de los offsets coincidía con la de `port/include/gex/camera.h`;
los **valores**, no.

### 14.3 Tres defectos reales encontrados y corregidos

1. **Los ángulos Euler se niegan.** En `0x46DE51`, `0x46DE67` y `0x46DE7D`
   el original hace `movswl 0x34(%edx), %eax; negl %eax` (y lo mismo en
   `+0x32` y `+0x30`) antes de llamar a las rotaciones Z, Y y X; la rama
   alternativa (`0x46DECA`, `0x46DEE3`, `0x46DF06`) niega igual sus ángulos
   (`+0x12C`, `+0x12A`, `+0x128`). El port usaba los ángulos crudos.
   *Corrección:* `gex_cam_build_orientation()` negate los tres ángulos.
2. **`M2` se aplica a las columnas, no a las filas.** `0x46E038..0x46E11D`
   copia una **fila** de la matriz de trabajo, la transforma con
   `0x4698B0` usando `M2 = 4096·[[1,0,0],[0,0,−1],[0,1,0]]` en `0x646BE0` y
   escribe el resultado en la **columna** destino. Como la matriz de trabajo
   del original es la transpuesta del producto del port, el efecto sobre `R`
   es `O[r][c] = (M2 · column_c(R))[r]`. El port aplicaba `M2` a las filas
   (matriz distinta). *Corrección:* `gex_cam_apply_m2()` reescrito.
3. **La tabla trigonométrica trunca, no redondea.** La tabla de seno del
   original (`0x00646C20`, media vuelta) volcada del proceso vivo coincide
   con `trunc(4096·sin(2πi/4096))` en las **1024** entradas vivas y
   difiere del valor redondeado en **502** de ellas.
   *Corrección:* `gex_cam_init_tables()` trunca.

### 14.4 Comparación bit a bit (port vs. original)

Con la cámara capturada y la base identidad:

| Magnitud | Port (tras la corrección) | Original | Resultado |
|---|---|---|---|
| marco local (15 valores) | idéntico | idéntico | ✅ |
| M (9 × `int16`) | `−1648 −2144 3074 / −3749 943 −1352 / 0 −3359 −2343` | igual | ✅ |
| A (9 × `int16`) | `3074 −1352 −2343 / −258 113 −4099 / 657 −3917 −1520` | igual | ✅ |
| B (9 × `int16`) | `3329 2162 −1520 / 3430 −1509 1679 / 0 0 0` | igual | ✅ |
| a_cam | `−1346 −3112 −4659` | igual | ✅ |
| b_cam | `2910 1720 0` | igual | ✅ |

Además, la fórmula de visibilidad se comprobó contra el desensamblado de la
prueba de hoja del original (`0x401F19..0x401F54`): `d0 = (A·centro) − a_cam`
se rechaza si `d0 <= −r` o `d0 >= far + r`, con `r` leído como `uint16` en
`node+0x06`; el port implementa exactamente eso.

### 14.5 Regresión automatizada

`port/tests/test_camera.c` incorpora ahora:

- `test_dynamic_reference_camera()`: reproduce la cámara capturada arriba y
  exige igualdad **exacta** de marco local, M, A, B, a_cam y b_cam.
- `test_table_truncation()`: fija los valores truncados de la tabla.
- `test_composition_zyx()`: valores dorados de la cadena corregida.

La suite completa del port (20 binarios) pasa con `-O0`, `-O2`, ASan y UBSan.

### 14.6 El nivel de referencia: `map7.dfx`

La cámara capturada pertenece a la ruta 3D del nivel que carga el juego, que
**no** es `AZTEC16.DFX` sino `C:\GEX_REZ\GEX2\level\map7.dfx` (ruta
construida con el `sprintf` de `0x00463403`; capturada en vivo con
`dumpstr` sobre el argumento de ruta del cargador `0x00461E0B`). El recorrido
BSP con esa cámara y ese nivel reproduce el **conteo y la lista completa de
hojas visibles** del original en 18/18 frames (§9 de
`S2_BSP_TRAVERSAL.md`).

## 15. Integración de la cámara original en el renderer (2026-11-11)

La cadena de cámara validada en §14 se ha integrado en el renderer de
Direct3D 11: el escenario `map7.dfx` se dibuja ahora con la cámara real del
juego, sin matriz sintética ni encuadre automático.

### 15.1 De la cámara a la matriz de emisión (`cam+0xE0`)

`0x46DD40` escribe en el bloque de 0x20 B al que apunta `cam+0xE0` la
matriz que consume la proyección. La construcción exacta (verificada contra
el desensamblado y contra la captura del proceso vivo) es:

| Paso | Dirección | Operación |
|---|---|---|
| 1 | `0x46DFA2..0x46E11D` | `O = M2 · R` (el mismo cambio de ejes de §6.1) |
| 2 | `0x46E2A3..0x46E2E8` | fila 0: `(v << 9) / 0x140` (`idiv`, trunca hacia cero) |
| 3 | `0x46E2EC..0x46E33C` | fila 1: `(v * 0x180) / 0xF0` (`idiv`, trunca hacia cero) |
| 4 | `0x46E33C` | fila 2: sin escalar |
| 5 | `0x46E340..0x46E448` | cada fila escalada por `cam+0x490`/`+0x492`/`+0x494` (`imul` + `sar 12`) |
| 6 | `0x46E4C6..0x46E51D` | traslación `= E · (−camPos)` (`sar 12`), en `+0x14/+0x18/+0x1C` |

Los pasos 2 y 3 son los factores **1,6** que el original aplica al pasar de
la orientación de la cámara a la de pantalla; con `cam+0x490/492/494 =
0x1000` (el caso de MAP7) el paso 5 es la identidad.

Con la cámara de referencia del frame 0 el port reconstruye **bit a bit** la
matriz y la traslación capturadas del proceso vivo:

```text
capturado en 0x65CD60   m = [-2636 -5998 0] [-3430 1508 -5374] [3074 -1352 -2343]
                        t = (7467, 4494, 1345)
port (gex_cam_build_emit_matrix)
                        m = [-2636 -5998 0] [-3430 1508 -5374] [3074 -1352 -2343]
                        t = (7467, 4494, 1345)          ✔
```

Implementación: `gex_cam_build_emit_matrix()` en `port/src/camera/camera.c`
(contrato en `port/include/gex/camera.h`).

### 15.2 Estado de cámara de referencia (MAP7, frame 0)

Capturado del objeto de cámara del GEX3D.EXE (`0x0065CD80`) con el
breakpoint de hardware en `0x004269C2`:

| Campo | Dirección | Valor |
|---|---|---|
| posición | `cam+0x00..0x05` | (2341, 4071, 3074) |
| Euler | `cam+0x30..0x35` | (-397, 0, 2802) |
| focal | `cam+0x40` | 0x140 (320) |
| ancho | `cam+0x48` | 0x180 (384) |
| corte lejano | `cam+0x5C` | 8500 |
| escala por fila | `cam+0x490/492/494` | 0x1000, 0x1000, 0x1000 |
| base identidad | `cam+0x43C` | 0 |

Los parámetros de proyección (centro x 256, centro y 192 y plano cercano 160)
los fija el juego una vez en `0x4636DD..0x4636FB` y no dependen de la
cámara.

### 15.3 Qué cambia en el port

- `port/src/camera/camera.c`: `gex_cam_build_emit_matrix()` (matriz de emisión
  y traslación) y los campos `row_scale[3]`, `proj_cx`, `proj_cy` y
  `near_z` de `gex_camera`. El campo `far` se renombra a `far_plane` porque
  `far` es una macro de los encabezados de Windows y rompía la compilación
  del módulo Win32.
- `port/src/scene/scene_level.c`: `gex_scene_reference_camera()` (el estado
  de §15.2) y `gex_scene_level_load_camera()`, que recorre el BSP **con la
  prueba de esfera de la cámara** (`gex_cam_sphere_visible`) y emite con la
  matriz de `cam+0xE0`, en **una sola pasada** (sin encuadre automático).
- `port/src/app/i4_main_win32.c`: con `--level` se usa la cámara original;
  `--diagnostic-view` conserva la vista sintética con encuadre automático
  como opción independiente.

### 15.4 Resultado con MAP7

Comparado con lo que el GEX3D.EXE original deja para el frame 0 (las
referencias están en `port/tests/ref_emit_frames.h` y
`port/tests/ref_map7_frames.h`):

| Magnitud | Original | Port |
|---|---|---|
| Hojas visibles | 307 | 307 (mismas hojas, mismo orden) |
| Matriz de emisión | `[-2636 -5998 0] [-3430 1508 -5374] [3074 -1352 -2343]` | idéntica bit a bit |
| Traslación | (7467, 4494, 1345) | idéntica bit a bit |
| Triángulos aceptados | 1123 | 1123 |
| Orden de dibujo | 1123 triángulos | **1123/1123 en la misma posición** |

### 15.5 Ejecución real

Con GPU hardware (D3D11 11_1) y ventana Win32 de 960×540:

```text
renderer: Direct3D 11 (hardware)
textures: 4 decodificadas de 'C:\GEX_REZ\GEX2\level\map7.vfx'
geometry: level 'C:\GEX_REZ\GEX2\level\map7.dfx'
  level: entries=25620 vertices=9606 polygons=16292 leaves=307
  emitted: triangles=1123 (fog passes=0) discarded by depth=0
  view: ORIGINAL GAME CAMERA (cam+0xE0)
    emit matrix: [-2636 -5998 0] [-3430 1508 -5374] [3074 -1352 -2343]
    translation: (7467, 4494, 1345)
    projection: focal=320 cx=256 cy=192 near_z=160
final: t=0.67s presents=60 fps=90.0 ... exit: clean, frame errors=0
```

La captura de esa ventana (no se sube al repositorio) muestra la geometría
de MAP7 con texturas desde el punto de vista de la cámara del juego; con
`--diagnostic-view` se ve el nivel entero desde fuera, que es lo propio de
la vista sintética.

### 15.6 El color de niebla de la cámara

La niebla del escenario mezcla el color del vértice hacia un color destino
que el juego mantiene en `[0x6468A0]` (B), `[0x647574]` (G) y `[0x647560]`
(R) y que actualiza **por objeto** durante la emisión (`0x43DE14` y
`0x43DE4D`). Para MAP7 en el frame 0 el valor general es
`B=0x14, G=0x14, R=0x5A`, que es el que usa el port (`scene_camera_view`).
Con él, los 1123 colores por vértice del port coinciden con los del original.
El valor por objeto queda pendiente: su origen en el `.DFX` (la estructura
que lee `0x43DE14`, con los bytes de color en `+0x40` y `+0x48`) todavía no
está identificado.

### 15.7 Lo que sigue abierto

- **El vuelo de cámara**: la cámara del juego avanza frame a frame por el
  escenario (18 frames capturados en `ref_map7_frames.h`). El port usa el
  estado del frame 0; reproducir el vuelo requires la lógica de movimiento
  del original, que no está reconstruida.
- **La LUT de niebla**: el original mezcla la niebla por vértice con su tabla
  `[0x64FEA8]`, que genera `0x461895` a partir de dos profundidades del
  nivel. El port usa la fórmula analítica equivalente con los valores A y B
  capturados: **solo afecta al color**, no a la geometría ni a la posición.
- **El color de niebla** (`[0x6474C4]`) lo anima el juego frame a frame; el
  port usa el valor capturado del frame 0.
- **Los materiales y el blending por polígono** siguen fuera de alcance.

### 14.7 Lo que sigue abierto (no cerrado)

La cadena de cámara queda **cerrada**: reproduce bit a bit el marco local, M,
A, B, `a_cam` y `b_cam` del original, y con ella el recorrido BSP reproduce
**18/18 frames** con la lista completa de hojas idéntica (§9 de
`S2_BSP_TRAVERSAL.md`).

La validación de la emisión (2026-11-11, §9.5 de `S2_BSP_TRAVERSAL.md`) uso
ademas la matriz de `0x65CD60`, que es la `O` de esta cámara escalada por 1,6
en las filas 0 y 1: el port reproduce ese contexto **bit a bit**. La emisión de
nivel ha quedado validada contra el original **triángulo a triángulo**
(conjunto, orden de dibujo y fases de subdivisión). La cámara no se ha tocado:
ninguna de las correcciones de la emission afecta a la cadena validada aquí.

Lo que **queda abierto** es la comparación cuantitativa de la **emisión de
geometría** (polígonos y triángulos por frame): el port emite sobre un
contexto de emisión reconstruido estáticamente, no sobre una captura del
original. El contrato de emisión (`docs/D3D11_FIRST_SCENE_PLAN.md` §11) **no
se da por validado** hasta esa comparación.

---
