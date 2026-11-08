# U-1 — Auditoría definitiva de la carga y relocalización DFX (2026-11-08)

**Objeto:** reconstrucción verificable del contenedor `.DFX`, de la rutina de
relocalización `0x004735C0`, del cargador/compactador `0x00461E0B` y de la
variante `0x00461ECA`, a partir de los bytes reales de los assets y del
desensamblado del ejecutable original.

**Binario de referencia:** `C:\GEX_REZ\E2_A_ORIGINAL\GEX3D.EXE`
(PE32 i386, 1 557 504 B, EU v1.00.000; `imageBase = 0x00400000`).

**Assets verificados (solo lectura):**

| Fichero | Ruta | Tamaño |
|---|---|---:|
| `AZTEC16.DFX` | `C:\GEX_REZ\E2_A_ORIGINAL\LEVEL\AZTEC16.DFX` | 544 586 B |
| `GEX.DFX` | `C:\GEX_REZ\E2_A_ORIGINAL\GEX.DFX` | 146 008 B |

**Modo:** análisis estático *read-only*. Sin ejecución del juego, sin ventanas,
sin breakpoints, sin emulador, sin instalar herramientas y sin modificar
`C:\GEX_REZ`, el port ni los assets. El desensamblado utilizado es el volcado
completo `gex_full.asm` (en `%TEMP%\opencode`, no versionado); no se sube ningún
binario.

**Criterio de evidencia:** una afirmación es *demostrada* solo si (a) se
reproduce con los bytes reales de los assets, o (b) se lee en instrucciones del
ejecutable con semántica inequívoca. Las inferencias se marcan como tales.

---

## A. Formato binario

```
+0x00  u32 count                 ; número de entradas de la tabla
+0x04  u32 offsets[count]        ; posiciones de DWORD, relativas a la imagen
+...    relleno hasta 4*aligned  ; relleno a cero
4*aligned: imagen                ; imagen relocalizable
```

### A.1 Fórmula de alineación

```
aligned = (count + 0x200) & ~0x1FF      ; en DWORDs (no en bytes)
imageBase = 4 * aligned                 ; en bytes
```

En el binario, `0x00461E47..0x00461E5B` implementa exactamente el redondeo a
múltiplo de 512:

```
461e47  add eax,0x200
461e4c  cdq
461e4d  and edx,0x1ff
461e53  add eax,edx
461e55  sar eax,9
461e58  shl eax,9
461e5b  mov [ebp-0x20],eax        ; aligned
```

Para valores positivos equivale a `(count + 0x200) & ~0x1FF`.

### A.2 Relleno

El hueco entre el final de la tabla (`4 + 4*count`) y la imagen (`4*aligned`) se
verificó **byte a byte**: es relleno íntegramente a cero en ambos archivos
(1 204 B en `AZTEC16`, 756 B en `GEX`).

### A.3 Offsets

Los `count` DWORD de la tabla son **posiciones de DWORD relativas a la imagen**:

* siempre múltiplos de 4;
* siempre `< tamaño de imagen`;
* el DWORD situado en `imagen + offset` es, a su vez, un desplazamiento
  relativo dentro de la imagen (no una dirección absoluta ni una VA del
  ejecutable).

La tabla **no** describe objetos: describe **posiciones de punteros**. Tras
cargar, la tabla se destruye.

### A.4 Nivel frente a modelo

El contenedor es común, pero la **cabecera de la imagen** no lo es:

| Aspecto | DFX de nivel | DFX de modelo (`GEX.DFX`) |
|---|---|---|
| `image[0]` | descriptor relocalizado | `0x2E0`, **no** relocalizado; apunta a ceros |
| Cadena de regiones | cierra exactamente | no existe |
| Uso | BSP + geometría + objetos | modelo/personaje con tabla de frames |
| Nombre | `image+0x8C` → cadena de nivel | `image+0x8C` → no es cadena de nivel |

No debe generalizarse el descriptor BSP a todos los `.DFX`.

---

## B. Validación de los dos archivos

### B.1 AZTEC16.DFX

| Dato | Valor |
|---|---|
| Tamaño exacto | **544 586 B** |
| Primer DWORD (`count`) | **16 082** = `0x00003ED2` (bytes `D2 3E 00 00`) |
| `aligned` | **16 384 DWORDs** = `0x4000` |
| Inicio de imagen `4*aligned` | **65 536** = `0x10000` |
| Tamaño lógico de imagen | **479 050 B** |
| Copia tras redondeo del cargador | **479 052 B** |
| Fin de tabla `4 + 4*count` | **64 332** |
| Relleno tabla→imagen | **1 204 B, todos cero** |

Primeros 16 bytes de la imagen en `0x10000`:

```
3C 54 00 00  C8 0A 00 00  02 00 00 00  E4 15 05 00
= 0x0000543C  0x00000AC8  0x00000002  0x000515E4
```

Primeras ocho entradas de relocalización:

| # | offset | DWORD en `imagen+offset` |
|---:|---:|---:|
| 0 | `0x0` | `0x543C` |
| 1 | `0x4` | `0xAC8` |
| 2 | `0xC` | `0x515E4` |
| 3 | `0x14` | `0x938` |
| 4 | `0x3C` | `0x4E94` |
| 5 | `0x54` | `0xB54` |
| 6 | `0x5C` | `0x2664` |
| 7 | `0x74` | `0x138` |

Comprobaciones sobre las 16 082 entradas:

* offsets no alineados a 4: **0**
* offsets fuera de `[0, 479 050)`: **0** (máximo `0x74F24` = 479 012)
* DWORD apuntados fuera de la imagen: **0**
* DWORD apuntados iguales a cero: **0**
* rango de los DWORD apuntados: **[224, 479 020]** ⊂ `[0, 479 050)`

Campos clave:

* `imagen+0` = `0x543C` → descriptor.
* `imagen+0x543C` = `0x44A44` → raíz BSP.
* `imagen+0x8C` = `0xE0` → nombre del nivel.

Pertenencia a la tabla de los punteros del descriptor (`desc = 0x543C`):
`0x543C`, `0x5450` (`+0x14`), `0x5460` (`+0x24`), `0x5464` (`+0x28`),
`0x5468` (`+0x2C`), `0x546C` (`+0x30`) y `0x5470` (`+0x34`) están **todos** en la
tabla, igual que `0x8C` y `0x0`.

### B.2 GEX.DFX

| Dato | Valor |
|---|---|
| Tamaño exacto | **146 008 B** |
| Primer DWORD (`count`) | **322** = `0x00000142` (bytes `42 01 00 00`) |
| `aligned` | **512 DWORDs** |
| Inicio de imagen `4*aligned` | **2 048** = `0x800` |
| Tamaño de imagen | **143 960 B** |
| Copia tras redondeo del cargador | **143 960 B** |
| Fin de tabla `4 + 4*count` | **1 292** |
| Relleno tabla→imagen | **756 B, todos cero** |

Primeros 16 bytes de la imagen en `0x800`:

```
E0 02 00 00  00 00 00 00  01 00 35 00  4C 00 00 00
= 0x000002E0  0x00000000  0x00350001  0x0000004C
```

Primeras ocho entradas de relocalización:

| # | offset | DWORD en `imagen+offset` |
|---:|---:|---:|
| 0 | `0xC` | `0x4C` |
| 1 | `0x10` | `0x50` |
| 2 | `0x1C` | `0x1F8` |
| 3 | `0x20` | `0x40` |
| 4 | `0x24` | `0x34` |
| 5 | `0x2C` | `0x124` |
| 6 | `0x4C` | `0xC5C` |
| 7 | `0x50` | `0x4020` |

Comprobaciones sobre las 322 entradas:

* offsets no alineados a 4: **0**
* offsets fuera de `[0, 143 960)`: **0** (máximo `0x20808` = 133 128)
* DWORD apuntados fuera de la imagen: **0**
* rango de los DWORD apuntados: **[52, 133 700]** ⊂ `[0, 143 960)`

Pertenencia a la tabla:

| Offset | ¿En tabla? | Observación |
|---|---:|---|
| `0x0` | **no** | `image[0] = 0x2E0` no se relocaliza; el DWORD en `image+0x2E0` es `0` |
| `0x2C` | sí | puntero a tabla de frames (`0x124`) |
| `0x30` | no | contador `8` |
| `0x34` | no | cadena `"gex______"` |
| `0x8C` | sí | puntero a nombre; valor `0x10138` |

Es decir: en `GEX.DFX` **no** se relocaliza `image[0]`, sí `image+0x8C` y
`image+0x2C`; no se relocalizan el contador ni la cadena. Esto confirma la
observación de U-1h §12.

---

## C. Funciones del ejecutable

### C.1 `0x00461B90` — lectura completa del fichero

**Argumento:** `[ebp+8]` = puntero a la ruta (una sola cadena).
**Devuelve:** `[ebp-4]` = buffer con el fichero completo.

Secuencia verificada:

```
461ba0  push 0x5515cc                 ; modo de apertura
461ba5  mov  eax,[ebp+8]              ; ruta
461ba8  push eax
461ba9  call 0x541060                 ; apertura
461bb1  mov  [ebp-0xc],eax            ; f = descriptor de fichero
461bcb  push 2 ; push 0 ; push f
461bd3  call 0x540f90                 ; posicionar al final
461bdf  call 0x540de0                 ; tamaño del fichero
461be7  mov  [ebp-8],eax
461bee  call 0x464fb3                 ; asignar(tamaño) -> buffer
461bf6  mov  [ebp-4],eax
461bf9  push 0 ; push 0 ; push f
461c01  call 0x540f90                 ; posicionar al inicio
461c09  push f ; push 1 ; push tamaño ; push buffer
461c17  call 0x540ca0                 ; leer(buffer, tamaño, 1, f)
461c23  call 0x540c20                 ; cerrar
461c2b  mov  eax,[ebp-4] ; ret
```

**Demostrado:** lee el fichero completo, reserva con `0x464FB3`, lee `tamaño`
bytes y devuelve el buffer. La asignación deja en `[0x647FAC]` el tamaño
**redondeado a múltiplo de 4** (véase C.4).

**Inferencia (no demostrada):** los thunks `0x541060`, `0x540F90`, `0x540DE0`,
`0x540CA0` y `0x540C20` corresponden a apertura, posicionamiento, consulta de
tamaño, lectura y cierre. La identidad exacta no se ha resuelto por tabla de
imports; se infiere de la secuencia de argumentos y del flujo.

### C.2 `0x00461E0B` — cargador, relocalización y compactación

**Argumento:** `[ebp+8]` = ruta del `.DFX`.
**Devuelve:** `[ebp-0x1c]` = buffer `B` con la imagen compactada en `B+0`.

Estructura local en `[ebp-0x14]`:

```
struct { u32 count; u32 *offsets; }   ; container
```

Secuencia:

```
461e1b  call 0x461b90              ; B = leer(ruta)
461e23  mov  [ebp-4],eax           ; B
461e26  mov  edx,[ebp-4]
461e29  mov  [ebp-0x1c],edx        ; copia de B (valor de retorno)
461e2c  mov  eax,[ebp-4]
461e2f  add  eax,4
461e35  mov  [container+4],eax     ; container.offsets = B+4
461e3e  mov  ecx,[B]               ; count
461e40  mov  [container+0],ecx     ; container.count = count
461e45  mov  eax,[container]       ; recarga count en EAX
461e47  add  eax,0x200
461e4c  cdq ; and edx,0x1ff ; add eax,edx
461e55  sar  eax,9 ; shl eax,9
461e5b  mov  [ebp-0x20],eax        ; aligned
461e5e  mov  eax,[ebp-4]
461e61  push eax                   ; arg3 = B  (delta)
461e62  mov  ecx,[ebp-0x20]        ; ECX = aligned  ← recarga clave
461e65  mov  edx,[ebp-4]           ; EDX = B
461e68  lea  eax,[edx+4*ecx]       ; imageBase = B + 4*aligned
461e6b  push eax                   ; arg2 = imageBase
461e6c  mov  ecx,[ebp-0xc]
461e6f  push ecx                   ; arg1 = container
461e70  call 0x4735c0              ; relocalizar(container, imageBase, B)
```

Compactación:

```
461e78  mov  edx,[0x647fac]        ; tamaño de la última asignación
461e7e  sar  edx,2                 ; /4
461e84  mov  eax,[ebp-0x18]
461e87  sub  eax,[ebp-0x20]        ; (tamaño/4) - aligned
461e8d  lea  edx,[B+4*eax]         ; end = B + 4*(tamaño/4 - aligned)
461e93  cmp  dst,end ; jae fin
461ea4  mov  edx,[dst + 4*aligned] ; DWORD de origen
461ea7  mov  [dst],edx             ; copia a destino
461eac  add  dst,4
461eb2  jmp  0x461e93              ; bucle hacia delante
461eb4  mov  ecx,[ebp-0x20]
461eb7  shl  ecx,2                 ; 4*aligned
461ebb  call 0x46506b              ; liberar(4*aligned)
461ec3  mov  eax,[ebp-0x1c]        ; devuelve B
```

Observaciones demostradas:

* origen de la copia: `B + 4*aligned` (imagen antes de compactar);
* destino: **`B` (offset 0)**, no offset 4 ni la posición de la tabla;
* copia manual de DWORD hacia delante (`dst < src`, solapamiento seguro);
* longitud: `4 * (roundUp4(tamaño)/4 − aligned)` bytes;
* después libera `4*aligned` y devuelve `B`.

#### C.2.1 Corrección de transcripción: flujo de `ECX` en `0x00461E68`

En una transcripción previa se afirmó que `ECX` contenía `count` al llegar a
`0x00461E68`. Eso es **incorrecto**: entre `0x00461E3E` (`ECX = count`) y
`0x00461E68` hay instrucciones que reutilizan registros. En particular:

```
461e40  mov [edx],ecx          ; guarda count; ECX deja de ser necesario
461e45  mov eax,[edx]          ; count pasa a EAX para el cálculo
...
461e62  mov ecx,[ebp-0x20]     ; ECX = aligned   ← instrucción omitida antes
461e65  mov edx,[ebp-0x4]      ; EDX = B
461e68  lea eax,[edx+4*ecx]    ; EAX = B + 4*aligned
```

Por tanto, `imageBase = B + 4*aligned`; el cálculo es correcto. La corrección es
de **transcripción**, no de lógica: faltaba documentar `0x00461E62`.

### C.3 `0x004735C0` — relocalizador

**Firma real (`cdecl`):** `void reloc(container *c, void *imageBase, u32 delta)`.

* `arg1` = `container { u32 count; u32 *offsets; }` (`[ebp+8]`).
* `arg2` = `imageBase = B + 4*aligned` (`[ebp+0xc]`), base para **localizar**
  cada DWORD que se modifica.
* `arg3` = `B` (`[ebp+0x10]`), **delta** que se suma.

```
4735c9  mov ecx,[eax]              ; count
4735ce  mov eax,[edx+4]            ; offsets
4735d1  lea ecx,[eax+4*ecx]        ; end = offsets + 4*count
4735dd  mov [ebp-0xc],eax          ; p = offsets
4735eb  cmp p,end ; jae fin
4735f3  mov eax,[p]                ; off
4735f8  cdq ; and edx,3 ; add eax,edx ; sar eax,2
473601  mov ecx,[ebp+0xc]          ; imageBase
473604  lea edx,[ecx+4*eax]        ; addr = imageBase + off
47360a  mov eax,[addr]
47360f  add eax,[ebp+0x10]         ; valor += delta
473615  mov [addr],eax             ; *addr += delta
473617  jmp 0x4735e2               ; p += 4
```

El `cdq/and/sar` es una división con signo entre 4; como todos los offsets son
múltiplos de 4 positivos, equivale a `imageBase + off`. Recorre exactamente
`count` entradas.

### C.4 `0x00464FB3` — asignador

```
464fb7  mov eax,[ebp+8]
464fba  add eax,3 ; sar eax,2 ; shl eax,2   ; redondeo a múltiplo de 4
464fcc  base[idx] -> valor devuelto (ebp-4)
464fe2  base[idx] += size
464ffe  remaining[idx] -= size
46501b  si remaining[idx] < 0 -> error (no retorna buffer)
46505e  [0x647FAC] = size                    ; tamaño de la última asignación
```

Es un asignador tipo *arena* (puntero incremental). `[0x647FAC]` no es «el
tamaño del fichero» por definición: es el **tamaño de la última asignación**. En
esta ruta coincide con el tamaño del fichero porque `0x461B90` realiza una sola
asignación.

### C.5 `0x0046506B` — liberación / rollback

```
465071  and al,-4                 ; size &= ~3
465083  sub base[idx],size        ; base -= size
46509f  add remaining[idx],size   ; remaining += size
```

No libera con `free` de la libc: revierte el puntero del arena. Tras compactar,
el bloque útil queda `[B, B + copyLen)`.

### C.6 `0x0043B2A9` — almacenamiento en `level[+4]`

```
43b294  mov  eax,[ebp+8]       ; ruta del .DFX
43b298  call 0x461e0b          ; eax = B (imagen compactada)
43b2a0  mov  [ebp-4],eax
43b2a3  mov  ecx,[ebp+0x10]    ; struct level
43b2a6  mov  edx,[ebp-4]       ; EDX = B
43b2a9  mov  [ecx+4],edx       ; level[+4] = B
43b2ac  mov  eax,[ebp+0x10]
43b2af  movsx ecx,word[eax+0x4E38]
43b2b6  cmp  ecx,2
43b2c3  mov  eax,[edx+0x8C]    ; image+0x8C ya es puntero absoluto
43b2ca  call 0x5407e0          ; strcpy(0x54FA90, image+0x8C)
```

`0x0043B11D` es la **entrada de la función** que contiene esta llamada; el
almacenamiento real está en `0x0043B2A9`.

### C.7 `0x004650B0` / `0x004650FD` — ámbitos del arena

`0x00464FB3` no reserva memoria del sistema: avanza un puntero dentro de una
pila de ámbitos. El estado vive en dos arrays indexados por
`[0x647EA8]` (*profundidad*):

```
[0x647EAC + 8*d]   base[d]        ; puntero incremental del ámbito d
[0x647EB0 + 8*d]   remaining[d]   ; bytes disponibles del ámbito d
```

```
4650B0  if (depth >= 0x20) return
        base[depth+1]      = base[depth]        ; 0x4650C7..0x4650D3
        remaining[depth+1] = remaining[depth]   ; 0x4650E0..0x4650EC
        depth++                                  ; 0x4650EE..0x4650F6

4650FD  if (depth != 0) depth--                  ; 0x465100..0x465114
```

`0x4650B0` **abre un ámbito** clonando base y resto en el nivel siguiente;
`0x4650FD` **lo cierra** bajando la profundidad. Como las asignaciones del
ámbito hijo modifican `base[depth+1]`/`remaining[depth+1]` y no los del padre,
al cerrar el ámbito **todo lo asignado dentro se descarta de golpe**: el padre
recupera su puntero incremental intacto, sin llamar a `0x46506B`.

Nota: `0x4650FD` **no** restaura `[0x647FAC]` (el «tamaño de la última
asignación»); ese global queda con el último valor escrito por `0x464FB3` o por
quien lo modifique.

---

## D. Algoritmo exacto

Pseudocódigo fiel al binario:

```c
/* 1. Lectura del fichero completo (0x00461B90) */
B = load_file(path);              /* buffer con count + tabla + relleno + imagen */

/* 2. Cabecera y alineación (0x00461E0B) */
count   = *(u32 *)B;
offsets = (u32 *)(B + 4);
aligned = ((count + 0x200) >> 9) << 9;      /* == (count + 0x200) & ~0x1FF, en DWORDs */

/* 3. Localización de la imagen original */
imageBase = B + 4 * aligned;

/* 4. Relocalización de los DWORD enumerados (0x004735C0) */
for (i = 0; i < count; i++) {
    u32 off  = offsets[i];
    u32 *dst = (u32 *)(imageBase + off);

    /* 5. Suma de B a los valores relativos */
    *dst += B;
}

/* 6. Compactación desde B + 4*aligned hasta B */
copyLen = 4 * ((roundUp4(last_alloc_size) / 4) - aligned);
src = imageBase;
dst = B;
for (k = 0; k < copyLen / 4; k++) {
    dst[k] = src[k];              /* bucle manual, hacia delante */
}

/* 7. Liberación de 4*aligned (0x0046506B) */
release(4 * aligned);

/* 8. Devolución de B (0x00461EC3) */
return B;

/* 9. Almacenamiento en level[+4] (0x0043B2A9) */
level[4] = B;
```

### D.1 Por qué `imageBase` localiza y `B` es el delta

Antes de compactar, la imagen está en `imageBase = B + 4*aligned`. La tabla
contiene **posiciones relativas a la imagen**, así que el DWORD a modificar está
en `imageBase + offset`. Eso explica el segundo argumento.

Cada DWORD contiene un valor `v` que es un desplazamiento **relativo a la
imagen**. Tras la compactación, la imagen pasa a residir en `B`; el valor
correcto es `B + v`. Por eso el delta que se suma es `B`, no `imageBase` y no
cero. La suma se hace *antes* de mover los datos, pero el valor resultante ya es
el de la ubicación final.

### D.2 Redondeo de la asignación y los dos bytes extra de AZTEC16

`0x00464FB3` redondea el tamaño a múltiplo de 4 y lo guarda en `[0x647FAC]`.
`0x00461E0B` calcula la longitud de copia con ese valor redondeado:

| Archivo | `roundUp4(tamaño)` | `4*aligned` | `copyLen` | Imagen lógica |
|---|---:|---:|---:|---:|
| AZTEC16 | 544 588 | 65 536 | **479 052** | 479 050 |
| GEX | 146 008 | 2 048 | **143 960** | 143 960 |

En `AZTEC16` se copian **2 bytes adicionales** (padding de redondeo). Permanecen
dentro de la asignación de 544 588 B; no se lee fuera del bloque reservado. El
tamaño lógico de la imagen sigue siendo 479 050 B.

---

## E. Correcciones históricas

Una investigación posterior a U-1h produjo cifras incompatibles con los bytes.
Estas son las correcciones, con su causa verificable:

| Afirmación errónea | Valor real | Por qué era incorrecta |
|---|---|---|
| `AZTEC16` `count = 131` | **16 082** (`0x3ED2`) | El primer DWORD del fichero es `0x3ED2`; 131 no aparece como cabecera |
| Imagen `AZTEC16` en offset 528 | **65 536** (`0x10000`) | 528 = `4 + 131*4`; es el **fin de tabla con un count equivocado**, no el inicio de imagen |
| Imagen `GEX` en offset 1 292 | **2 048** (`0x800`) | 1 292 = `4 + 322*4`; es el **fin de tabla**, no el inicio de imagen |
| `aligned = 2 048 B` para ambos | AZTEC 65 536 B; GEX 2 048 B | La fórmula coincide solo en GEX; en AZTEC `aligned = 16 384` DWORDs |
| Imagen `GEX` de 143 360 B | **143 960 B** | `146 008 − 2 048 = 143 960` |
| `delta = 0` | **`delta = B`** | El llamador empuja `B` como tercer argumento (`0x00461E61`) |
| Compactación hacia offset 4 | **hacia offset 0 (`B`)** | El bucle escribe el primer DWORD de la imagen en `B[0]` |

**Causa común:** se confundió el **final de la tabla** (`4 + 4*count`) con el
**inicio real de la imagen** (`4*aligned`), y en `AZTEC16` se usó además un
`count` equivocado. No es una interpretación alternativa del formato: contradice
los bytes de cabecera y la aritmética del cargador.

---

## F. Limitaciones pendientes

Estas cuestiones quedan **expresamente abiertas**:

1. **Identidad exacta de los thunks de E/S** (`0x541060`, `0x540F90`,
   `0x540DE0`, `0x540CA0`, `0x540C20`). Se infieren por argumentos y flujo; no se
   ha resuelto la tabla de imports.
2. **Generalización de `[0x647FAC]`.** Es «tamaño de la última asignación», no un
   campo del formato. En esta ruta coincide con el tamaño del fichero porque hay
   una sola asignación; otras rutas podrían no cumplirlo.
3. **Propósito de la variante `0x00461ECA`.** Su cuerpo está analizado en §H y
   su diferencia con `0x00461E0B` es **demostrada**. Lo que sigue abierto es
   *para qué existía*: no tiene ningún llamador estático en este build, de modo
   que no se puede observar ni el origen de `dest` ni el uso del resultado.
   Dentro de esa incógnita quedan tres preguntas concretas: (a) qué significa
   la constante `0x801FC000`; (b) por qué la copia recorta exactamente
   `0x200` DWORDs; (c) si el destino externo pretendía ser una dirección fija
   de otro mapa de memoria. No se afirma ninguna respuesta.
4. **Direcciones absolutas reales.** El análisis estático fija la relación
   `B + offset`, pero el valor de `B` solo se conoce en ejecución.
5. **Formatos de modelo.** `GEX.DFX` usa el mismo contenedor, pero su cabecera y
   su recorrido **no están descodificados**. La ingeniería inversa de *todos* los
   formatos `.DFX` **no** está completa.
6. **Referencias calculadas.** La ausencia de llamadores de `0x00461ECA` se
   demuestra para referencias literales (`call`/`jmp` directos y DWORD con la
   VA). No se puede descartar por completo una llamada cuya dirección se
   construya en tiempo de ejecución; no se ha localizado ninguna secuencia que
   lo haga.

---

## G. Documentos relacionados

* [`U1G_RESULTADOS.md`](U1G_RESULTADOS.md) — primera descripción correcta de la
  cadena `0x004735C0(&reloc, p + 4*aligned, p)`.
* [`U1H_RESULTADOS.md`](U1H_RESULTADOS.md) — contenedor, descriptor y flujo 3D;
  §2.2 corregida por esta auditoría.
* [`U1_CONSOLIDACION.md`](U1_CONSOLIDACION.md) — consolidación de U-1.
* [`docs/U1_GEOMETRY_RECOVERY_PLAN.md`](docs/U1_GEOMETRY_RECOVERY_PLAN.md) —
  plan de recuperación de geometría.
* [`docs/D3D11_FIRST_SCENE_PLAN.md`](docs/D3D11_FIRST_SCENE_PLAN.md) — plan del
  primer escenario D3D11 (S-1 **no** implementado).

---

## H. Variante `0x00461ECA`

**Estado:** analizada instrucción a instrucción el 2026-11-08. Es una **función
independiente**, no una continuación de `0x00461E0B` ni una rama interna suya.

### H.1 Localización y forma

| Dato | Valor |
|---|---|
| Inicio | `0x00461ECA` |
| Última instrucción | `0x00461FBC` (`ret`) |
| Longitud | `0xF3` = 243 B |
| Función previa | termina en `0x00461EC9` (`ret`) |
| Función siguiente | empieza en `0x00461FBD` (prólogo propio) |

No hay caída de flujo desde la función anterior ni hacia la siguiente: es un
cuerpo aislado entre dos `ret`.

Prólogo y epílogo:

```
461eca  push ebp
461ecb  mov  ebp,esp
461ecd  sub  esp,0x20          ; 8 locales
...
461fb9  mov  esp,ebp
461fbb  pop  ebp
461fbc  ret                    ; sin operando -> limpia el llamador
```

`ret` sin operando (y no `ret 8`) implica **`cdecl`**: el llamador limpia los
argumentos. El retorno va en `EAX`.

### H.2 Firma real

```c
void *dfx_load_at(const char *path /*[ebp+0x8]*/, void *dest /*[ebp+0xC]*/);
```

* `arg1` = ruta del `.DFX` (`[ebp+8]`).
* `arg2` = **puntero de destino externo** (`[ebp+0xC]`). El valor `-1`
  (`0xFFFFFFFF`) es un **centinela**: significa «calcula el destino tú».
* Retorno (`EAX`) = el `dest` **original**, no el puntero avanzado.

Reparto de locales (32 B reservados):

| Local | Uso |
|---|---|
| `[ebp-0x4]` | `B`, y después puntero de lectura (se reutiliza) |
| `[ebp-0x8]` | puntero al contenedor (`&[ebp-0x10]`) |
| `[ebp-0xC]` | `container.offsets` |
| `[ebp-0x10]` | `container.count` |
| `[ebp-0x14]` | `N` (límite en DWORDs) |
| `[ebp-0x18]` | `end = B + 4*N` |
| `[ebp-0x1C]` | `dest` original (valor de retorno) |
| `[ebp-0x20]` | `aligned` (DWORDs) |

El contenedor tiene **exactamente el mismo layout** que en `0x00461E0B`.

### H.3 Cuerpo, instrucción a instrucción

```
461ed0  lea  eax,[ebp-0x10]
461ed3  mov  [ebp-0x8],eax          ; &container
461ed6  call 0x4650b0               ; abre ámbito de arena (sin argumentos)
461edb  mov  ecx,[ebp+0x8]
461ede  push ecx
461edf  call 0x461b90               ; B = leer_fichero_completo(path)
461ee4  add  esp,0x4
461ee7  mov  [ebp-0x4],eax          ; B

461eea  cmp  dword[ebp+0xc],-1      ; ¿dest == -1?
461eee  jne  0x461efe
461ef0  mov  edx,0x801fc000
461ef5  sub  edx,dword[0x647fac]    ; edx = 0x801FC000 - roundUp4(tamaño)
461efb  mov  [ebp+0xc],edx          ; reescribe el propio argumento

461efe  mov  eax,[ebp-0x4]
461f01  add  eax,0x4
461f04  mov  ecx,[ebp-0x8]
461f07  mov  [ecx+0x4],eax          ; offsets = B + 4
461f0a  mov  edx,[ebp-0x8]
461f0d  mov  eax,[ebp-0x4]
461f10  mov  ecx,[eax]
461f12  mov  [edx],ecx              ; count = B[0]

461f14  mov  edx,[ebp-0x8]
461f17  mov  eax,[edx]              ; count
461f19  add  eax,0x200
461f1e  cdq
461f1f  and  edx,0x1ff
461f25  add  eax,edx
461f27  sar  eax,0x9
461f2a  shl  eax,0x9
461f2d  mov  [ebp-0x20],eax         ; aligned = (count+0x200) & ~0x1FF

461f30  mov  eax,[ebp+0xc]
461f33  push eax                    ; arg3 = delta = dest
461f34  mov  ecx,[ebp-0x20]
461f37  mov  edx,[ebp-0x4]
461f3a  lea  eax,[edx+4*ecx]        ; imageBase = B + 4*aligned
461f3d  push eax                    ; arg2 = imageBase
461f3e  mov  ecx,[ebp-0x8]
461f41  push ecx                    ; arg1 = &container
461f42  call 0x4735c0               ; relocalizar(container, imageBase, dest)
461f47  add  esp,0xc

461f4a  mov  edx,[0x647fac]         ; roundUp4(tamaño del fichero)
461f50  add  edx,0x3
461f53  sar  edx,0x2                ; ceil(tamaño/4)
461f56  sub  edx,0x200              ; N = ceil(tamaño/4) - 0x200
461f5c  mov  [ebp-0x14],edx
461f5f  mov  eax,[ebp-0x14]
461f62  mov  ecx,[ebp-0x4]
461f65  lea  edx,[ecx+4*eax]
461f68  mov  [ebp-0x18],edx         ; end = B + 4*N
461f6b  mov  eax,[ebp-0x14]
461f6e  sub  eax,[ebp-0x20]
461f71  shl  eax,0x2
461f74  mov  [0x647fac],eax         ; [0x647FAC] = 4*(N - aligned)

461f79  mov  ecx,[ebp+0xc]
461f7c  mov  [ebp-0x1c],ecx         ; guarda dest original
461f7f  mov  edx,[ebp-0x20]
461f82  mov  eax,[ebp-0x4]
461f85  lea  ecx,[eax+4*edx]
461f88  mov  [ebp-0x4],ecx          ; lectura = B + 4*aligned

461f8b  mov  edx,[ebp-0x4]
461f8e  cmp  edx,[ebp-0x18]
461f91  jae  0x461fb1               ; while (src < end)
461f93  mov  eax,[ebp+0xc]
461f96  mov  ecx,[ebp-0x4]
461f99  mov  edx,[ecx]
461f9b  mov  [eax],edx              ; *dest = *src
461f9d  mov  eax,[ebp+0xc]
461fa0  add  eax,0x4
461fa3  mov  [ebp+0xc],eax          ; dest++
461fa6  mov  ecx,[ebp-0x4]
461fa9  add  ecx,0x4
461fac  mov  [ebp-0x4],ecx          ; src++
461faf  jmp  0x461f8b

461fb1  call 0x4650fd               ; cierra ámbito de arena (sin argumentos)
461fb6  mov  eax,[ebp-0x1c]         ; retorna dest original
461fb9  mov  esp,ebp
461fbb  pop  ebp
461fbc  ret
```

### H.4 Pseudocódigo

```c
void *dfx_load_at(const char *path, void *dest)   /* cdecl */
{
    struct { u32 count; u32 *offsets; } c;        /* [ebp-0x10] / [ebp-0xC] */
    arena_scope_push();                            /* 0x4650B0 */

    u32 *B = load_file(path);                      /* 0x461B90 */

    if (dest == (void *)-1)
        dest = (void *)(0x801FC000u - (u32)last_alloc_size);  /* [0x647FAC] */

    c.offsets = B + 1;                             /* B + 4 bytes */
    c.count   = B[0];
    aligned   = (c.count + 0x200) & ~0x1FFu;       /* DWORDs */

    reloc(&c, (u8 *)B + 4 * aligned, (u32)dest);   /* 0x4735C0 */

    u32 N   = ((last_alloc_size + 3) >> 2) - 0x200; /* DWORDs */
    u32 *end = B + N;

    last_alloc_size = 4 * (N - aligned);            /* [0x647FAC] */

    u32 *ret = dest;
    for (u32 *src = B + aligned; src < end; src++, dest++)
        *(u32 *)dest = *src;                       /* copia manual, 4 B */

    arena_scope_pop();                              /* 0x4650FD */
    return ret;
}
```

### H.5 Diferencias con `0x00461E0B`

| Aspecto | `0x00461E0B` (principal) | `0x00461ECA` (variante) | Clasificación |
|---|---|---|---|
| Argumentos | 1: `path` | 2: `path`, `dest` | **CONFIRMADA** |
| Convención | `cdecl`, retorno en `EAX` | `cdecl`, retorno en `EAX` | **CONFIRMADA** |
| Lectura | `0x461B90(path)` | `0x461B90(path)` | **CONFIRMADA** |
| Ámbito de arena | no abre ninguno | `0x4650B0` al entrar / `0x4650FD` al salir | **CONFIRMADA** |
| Cabecera | `count = B[0]`, offsets en `B+4` | idéntica | **CONFIRMADA** |
| Alineación | `(count+0x200) & ~0x1FF` DWORDs | instrucciones equivalentes (`0x461F19..0x461F2D`) | **CONFIRMADA** |
| Imagen origen | `B + 4*aligned` | `B + 4*aligned` | **CONFIRMADA** |
| Relocalizador | `0x4735C0` | `0x4735C0` | **CONFIRMADA** |
| Delta | `B` | **`dest`** | **CONFIRMADA** |
| Destino de la copia | `B` (in situ, compacta) | `dest` externo (no compacta) | **CONFIRMADA** |
| Longitud copiada | `4*(roundUp4(tamaño)/4 − aligned)` | `4*(N − aligned)`, `N = (roundUp4(tamaño)+3)>>2 − 0x200` | **CONFIRMADA** |
| Fin de copia | fin lógico de la imagen (+ *padding*) | **2 048 B antes** (`0x200` DWORDs) | **CONFIRMADA** |
| `[0x647FAC]` al salir | intacto (`roundUp4(tamaño)`) | **sobrescrito** con `4*(N − aligned)` | **CONFIRMADA** |
| Liberación | `0x46506B(4*aligned)` | **ninguna**: `0x4650FD` descarta todo el ámbito | **CONFIRMADA** |
| Retorno | `B` (buffer del arena) | `dest` original | **CONFIRMADA** |
| Llamadores | 2 (`0x43B298`, `0x46371D`) | **ninguno localizado** | **CONFIRMADA** (en este build) |
| Uso del resultado | `level[+4]`, `[0x5555DC]+0x18` | no observable | **NO DEMOSTRADA** |
| Propósito | cargar el `.DFX` en memoria del arena | ¿carga en dirección fija? | **NO DEMOSTRADA** |

**Conclusión de la comparación:** comparten **lectura, cabecera, alineación,
localización de la imagen y relocalizador**; difieren en **destino, delta,
longitud, liberación y valor de retorno**. No es «el mismo cargador con un flag»:
son dos rutinas con contratos distintos.

### H.6 La copia truncada, cuantificada

`N = ceil(tamaño/4) − 0x200` DWORDs; `end = B + 4*N`; la copia empieza en
`B + 4*aligned`. Por tanto:

```
longitud copiada = 4 * (N − aligned)
                 = 4*(roundUp4(tamaño)/4 − 0x200 − aligned)
                 = longitud_del_cargador_principal − 2 048 B
```

| Dato | `AZTEC16.DFX` | `GEX.DFX` |
|---|---:|---:|
| Tamaño del fichero | 544 586 | 146 008 |
| `roundUp4(tamaño)` = `[0x647FAC]` | 544 588 | 146 008 |
| `(roundUp4(tamaño)+3)>>2` = `ceil(tamaño/4)` | 136 147 | 36 502 |
| `aligned` (DWORDs) | 16 384 | 512 |
| Inicio de imagen `4*aligned` | 65 536 | 2 048 |
| `N = ceil/4 − 0x200` | 135 635 | 35 990 |
| `end = 4*N` | 542 540 | 143 960 |
| **Longitud copiada** | **477 004** | **141 912** |
| Imagen lógica (`tamaño − 4*aligned`) | 479 050 | 143 960 |
| Bytes de imagen no copiados | **2 046** | **2 048** |
| `dest` por defecto (`0x801FC000 − roundUp4(tamaño)`) | `0x801770B4` | `0x801D85A8` |
| Fin de la copia (`dest + longitud`) | `0x801EB800` | `0x801FB000` |

Consecuencia sobre la tabla de relocalización (verificada entrada a entrada):

| Dato | `AZTEC16.DFX` | `GEX.DFX` |
|---|---:|---:|
| Entradas cuyo valor apunta más allá de la copia | **121** | **0** |
| — posición **dentro** de la copia → puntero colgante | **22** | 0 |
| — posición **fuera** de la copia → no se copia | **99** | 0 |
| Valor máximo apuntado | 479 020 | 133 700 |

Los últimos 2 048 B de ambos ficheros **no son relleno**:

| Asset | Bytes no nulos en los últimos 2 048 B | Primero | Último |
|---|---:|---:|---:|
| `AZTEC16.DFX` | 1 272 | 542 538 | 544 585 |
| `GEX.DFX` | 978 | 144 008 | 146 006 |

**Lectura de estas cifras (CONFIRMADA aritméticamente):** aplicada a
`AZTEC16.DFX`, la variante produciría una imagen mutilada: 99 posiciones de
puntero desaparecen y 22 punteros copiados apuntan a memoria **más allá del
final de la copia**. Aplicada a `GEX.DFX`, no pierde ningún puntero, pero
igualmente descarta 2 048 B de datos no nulos.

**No demostrado:** que esa pérdida fuera intencionada. No hay ningún llamador
que permita observar el resultado.

### H.7 El centinela `dest == -1` y la constante `0x801FC000`

La aritmética es inequívoca:

```
dest = 0x801FC000 − roundUp4(tamaño_del_fichero)
  =>  dest + roundUp4(tamaño_del_fichero) = 0x801FC000
```

Es decir: con el centinela, el **buffer completo del fichero termina exactamente
en `0x801FC000`**. En el binario, `0x801FC000` aparece **una sola vez**
(`0x461EF0`), está fuera de la imagen del ejecutable
(`0x00400000 + 0x261000 = 0x00661000`) y no tiene ningún otro consumidor.

| Afirmación | Clasificación |
|---|---|
| `dest + roundUp4(tamaño) = 0x801FC000` con el centinela | **CONFIRMADA** (aritmética de `0x461EF0..0x461EFB`) |
| `0x801FC000 = 0x80000000 + 0x1FC000`, 16 KB por debajo del techo de un mapa de 2 MB; patrón de «cargar al final de la RAM» | **INFERIDA** |
| Que proceda de la versión PlayStation y se conservara sin uso en el build PC | **NO DEMOSTRADA** |
| Que el recorte de `0x200` DWORDs sea una reserva de pila/scratch de ese mapa | **NO DEMOSTRADA** |

### H.8 Búsqueda de llamadores

| Comprobación | Resultado |
|---|---|
| `call 0x00461ECA` / `jmp 0x00461ECA` en el desensamblado completo | **0** (el único salto en el rango es interno: `0x461FAF → 0x461F8B`) |
| DWORD con la VA `CA 1E 46 00` en los 1 557 504 B del `.EXE` | **0** apariciones |
| Sección `.reloc` en el PE | **no existe** (5 secciones: `.text`, `.rdata`, `.data`, `.idata`, `.rsrc`) |
| Exports (`exportRVA`/`exportSize`) | `0` / `0` |
| Caída de flujo desde `0x00461EC9` | imposible: es un `ret` |

Como el ejecutable **no tiene tabla de reubicación**, cualquier puntero a código
debe aparecer como VA literal en el fichero; no aparece. Por tanto:

> **CONFIRMADA:** `0x00461ECA` es código **no referenciado** en este build.
> **NO DEMOSTRADA:** su ausencia de uso en otras versiones o regiones del juego.

Esto explica que no se pueda responder con evidencia estática a «¿qué hace el
llamador con el resultado?»: **no hay llamador**.

### H.9 Impacto en S-1 (parser del contenedor `.DFX`)

| Pregunta | Respuesta | Clasificación |
|---|---|---|
| ¿El formato binario es idéntico? | Sí: misma cabecera, misma fórmula de alineación, misma imagen, mismo relocalizador | **CONFIRMADA** |
| ¿Cambia el procesamiento posterior? | Sí, pero **después** del parseo: destino externo, sin compactación in situ, sin liberación parcial | **CONFIRMADA** |
| ¿El parser necesita parámetros nuevos? | No. `count` + tabla + imagen bastan; `dest` solo afecta a la colocación | **CONFIRMADA** |
| ¿El parser puede seguir siendo independiente de la relocalización? | Sí. Es lo recomendable: parsear/validar, y relocalizar como paso aparte con delta explícito | **CONFIRMADA** |
| ¿Hay que soportar dos rutas de carga? | No para S-1: la variante **no se usa** en este build y su copia truncada rompe `AZTEC16` | **INFERIDA** (fuerte) |
| ¿Sirve para niveles, modelos u otros recursos? | No se puede determinar: no hay llamador. El contenedor es común, así que no distingue nivel de modelo | **NO DEMOSTRADA** |

Recomendación para S-1 (sin implementar nada ahora):

1. Modelar el contenedor como **datos**: `count`, tabla de offsets, imagen.
2. Exponer la relocalización como operación con **`delta` explícito**, no
   cableada a `B`. Así la ruta principal (`delta = B`) y la variante
   (`delta = dest`) son el mismo código con distinto argumento.
3. **No** reproducir el recorte de `0x200` DWORDs ni el centinela
   `0x801FC000`: son comportamiento de una ruta sin uso y producen punteros
   colgantes en `AZTEC16`.
4. La colocación de la imagen en un buffer destino (compactada o no) es una
   decisión del consumidor, no del parser.


