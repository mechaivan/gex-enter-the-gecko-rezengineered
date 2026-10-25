# FUNCTION_MAP — mapa estático de funciones (EU)

> **Nivel de evidencia:** mixto y separado por columna. Los inicios marcados
> `call`, `entry` y `text-start` están **CONFIRMADOS** por el desensamblado
> (una instrucción `call` directa, el punto de entrada PE o el primer byte de
> `.text`). Los marcados `prologue` son **HIPÓTESIS** estructurales. Los marcados
> `thunk` son stubs de importación del enlazador, también estructurales.
> Ningún nombre `FUN_*` afirma qué hace una función.
>
> Fecha de generación: **2026-10-25**. Binario: `GEX3D.EXE` EU, MD5
> `692b12825003417cc4a5a9d4db13eebe`, SHA-256
> `d0932549613f9c08fb895c5f8a6461ce044e1863b4cba42da4715c2b41e73886`.
> Reproducible con `tools/gexmap/` (ver §9).

## 1. Qué es

Un mapa **medido** del código del ejecutable original:

- inicios de función y su extensión (cota superior),
- grafo de llamadas directas entre funciones mapeadas,
- imports que usa cada función,
- reconciliación con las 1 647 entradas que publica el informe de ByteMatch,
- anclas documentales del proyecto, comprobadas contra el mapa.

No es una decompilación, no reconstruye el código fuente, no demuestra qué
ejecuta cada función y **no cambia el porcentaje de ByteMatch** (sigue 0,00 %:
no hay ni una función reimplementada).

## 2. Por qué hacía falta

`docs/BYTEMATCH.md` declaraba PENDIENTE el *splitter* y el `symbols.txt`, y a la
vez afirmaba que su lista de 1 647 funciones «no puede inventar una». Ambas cosas
necesitaban una comprobación mecánica. El mapa la aporta: desensambla el binario
completo y comprueba, dirección por dirección, cuáles de esas entradas son
realmente un inicio de instrucción y un destino de llamada.

Resultado medido (§7): **199 de las 1 647 entradas no son inicios confirmados**,
y **104 ni siquiera caen en un límite de instrucción**. La lista del informe es
un punto de partida útil, pero no una lista de funciones demostradas.

## 3. Método

### 3.1 Lectura del PE

`tools/gexmap/pe.mjs` lee solo lo que el binario declara: cabecera PE32, base de
imagen, punto de entrada, secciones e IAT. No hay heurísticas ni firmas. El
ejecutable se verifica por tamaño, MD5 y SHA-256 antes de escribir nada; si no
coincide, `map` se detiene.

### 3.2 Desensamblado y reparación

El decodificador es `llvm-objdump -d --x86-asm-syntax=intel` (versión usada:
LLVM 22.1.8). Se conserva la columna de bytes crudos de objdump, de modo que
cada instrucción tiene su longitud real y no una longitud inferida por la
distancia a la siguiente.

Un barrido lineal se desincroniza cuando encuentra tablas de salto embebidas en
`.text`: decodifica los datos como instrucciones hasta que vuelve a cuadrar por
casualidad. El mapa lo detecta sin adivinar: si un `call` directo apunta a una
dirección que el barrido no decodificó, vuelve a desensamblar **desde ese
destino** en una ventana de 4 KiB (`--start-address`) y sustituye esa región.
En este binario hay **24 regiones reparadas** y **0 llamadas sin resolver**.

Las 24 ventanas empiezan en:

```
0x4046CC 0x40ADAC 0x41044F 0x412F8B 0x41366F 0x421CB0 0x42500A 0x42D18E
0x43572F 0x440123 0x44023B 0x4409B7 0x463BE5 0x476B4A 0x47782F 0x4E1FD1
0x4F0F9F 0x50983F 0x50BC9C 0x52B495 0x52D112 0x52D507 0x53D62C 0x549720
```

La lista completa, con su `stop` y número de instrucciones, está en
`analysis/EU/summary.json` → `repairs`. Una reparación es una mejora local: no
demuestra que el resto del barrido sea código y no datos.

### 3.3 Inicios confirmados (`basis=call|entry|text-start`)

Se acepta como inicio únicamente:

- el destino de una instrucción `call rel32` decodificada dentro de `.text`
  (`basis=call`),
- el `AddressOfEntryPoint` del PE (`basis=entry`),
- el primer byte de `.text` (`basis=text-start`).

No se escanean bytes `E8` en crudo: un `E8` dentro de un inmediato o de una
tabla de salto no es una llamada. Esa es exactamente la diferencia que separa
este mapa del informe de ByteMatch (§7).

### 3.4 Candidatos por prólogo (`basis=prologue`, HIPÓTESIS)

Fuera de los inicios confirmados, se marca como **candidato** todo
`push ebp; mov ebp, esp` que (a) esté en un límite de instrucción y (b) venga
inmediatamente después de un terminador de flujo (`ret`, `jmp`, `int3`…). Es la
forma de encontrar funciones a las que solo se llega por tabla o por llamada
indirecta.

En este binario hay **1 113 candidatos**. No se suman a los confirmados: se
publican aparte y con `basis=prologue`. Un prólogo puede ser una función real o
el comienzo de una secuencia que el barrido interpretó como código; el mapa no
lo decide.

### 3.5 Stubs de importación (`basis=thunk`)

El enlazador deja al final de `.text` bloques de stubs de 6 bytes:

```asm
jmp dword ptr [0x0065F334]   ; slot de la IAT
```

Sin separarlos, el bloque entero se absorbía en la función anterior y sus
imports se atribuían a código que nunca los llama. El mapa los detecta cuando el
`jmp [IAT]` (a) está en un límite de instrucción, (b) sigue a un terminador y
(c) su slot pertenece a la IAT leída del PE. Quedan como `kind=thunk` y
`basis=thunk` (o `basis=call` si además son destino de una llamada directa).

Caso concreto que motivó la corrección: `0x0054038E` es una función real de
54 B que termina en `ret` en `0x005403C3`. Los stubs empiezan en `0x005403C4`.
Antes de la corrección, `0x0054038E` absorbía 300 B de stubs (354 B de extensión
frente a 54 B reales) y aparecía con **50 imports** que en realidad pertenecían al
bloque; ahora tiene **0 imports directos** y los stubs son filas propias.

### 3.6 Extensiones y grafo

La extensión de cada función es la distancia hasta el siguiente inicio
confirmado o candidato (la última llega al final de `.text`). Es una **cota
superior**: incluye relleno, tablas y stubs intercalados si los hay.

Para cada función se recorre su extensión y se registra:

- `call rel32` a un inicio mapeado → arista `call` (con contador),
- `jmp rel32` a un inicio mapeado → arista `tail`,
- `call dword ptr [IAT]` / `jmp dword ptr [IAT]` → import directo,
- `jmp [IAT]` en el primer byte → la fila es un `thunk`.

Que una función llame a otra no dice nada sobre el significado de ninguna de
las dos; el grafo es estructura, no semántica.

Los imports se atribuyen a **toda la extensión**, no solo al cuerpo real: si una
función no se detectó como inicio, la anterior puede heredar sus imports. Es
otra consecuencia de que la extensión sea una cota superior.

### 3.7 Anclas documentales

`tools/gexmap/cli.mjs` contiene 24 direcciones citadas en
`REVERSE_ENGINEERING.md` / `TESTING.md` (game loop, voz, registro, render,
entrada PE, `WinMain`). Para cada una, el mapa calcula qué función la contiene y
el desplazamiento. Resultado actual: **13 en el inicio exacto de una función, 11
dentro de una función y 0 fuera de todo el mapa**. La tabla está en
`analysis/EU/anchors.json`.

Esta comprobación no valida lo que la documentación *afirma* de cada ancla; solo
que la dirección existe y cae donde el documento dice.

## 4. Uso

```powershell
# Generar el mapa (requiere el .EXE original; nunca se copia al repo)
node tools/gexmap/cli.mjs map --exe C:\ruta\GEX3D.EXE --objdump C:\ruta\llvm-objdump.exe

# Validar los artefactos comprometidos, sin binario (lo que puede correr el CI)
node tools/gexmap/cli.mjs check

# Consultar una función: tamaño, llamadores, llamadas, imports directos y transitivos
node tools/gexmap/cli.mjs query --function 0x43e1ba

# Tests offline (sintéticos + invariantes de los artefactos)
node --test tools/gexmap/test.mjs
```

`map` escribe en `analysis/EU/` por defecto (`--out` lo cambia). El binario no se
copia ni se sube: `.gitignore` excluye `*.exe`.

## 5. Artefactos

| Fichero | Contenido | Filas |
|---|---|---:|
| `analysis/EU/functions.tsv` | `addr`, `size`, `kind` (`func`/`thunk`), `basis`, `symbol`, `callers`, `callees`, `imports` | 2 655 |
| `analysis/EU/calls.tsv` | `from`, `to`, `kind` (`call`/`tail`), `count` | 5 097 |
| `analysis/EU/anchors.json` | anclas documentales y función que las contiene | 24 |
| `analysis/EU/summary.json` | hashes, recuentos, reparaciones, reconciliación, límites y SHA-256 de los otros tres | — |

Los SHA-256 de `functions.tsv`, `calls.tsv` y `anchors.json` están fijados dentro
de `summary.json`, y `check` los vuelve a calcular. Así un artefacto editado a
mano no pasa la validación.

## 6. Cifras verificadas

| Medida | Valor |
|---|---:|
| `.text` | `0x00401000`–`0x0054BADE` (1 354 462 B virtuales) |
| Instrucciones decodificadas | 391 487 |
| Regiones reparadas | 24 |
| Inicios totales | **2 655** |
| — por `call` directa | 1 448 |
| — por punto de entrada PE | 1 |
| — por primer byte de `.text` | 1 |
| — por prólogo (HIPÓTESIS) | 1 113 |
| — stubs de importación (`basis=thunk`) | 92 |
| Stubs de importación totales (`kind=thunk`) | **132** |
| Funciones no-stub (`gameFunctions`, incluye los 1 113 candidatos) | 2 523 |
| Aristas directas | **5 097** (5 095 `call` + 2 `tail`) |
| Llamadas directas sin resolver | 0 |
| Funciones que referencian algún import | 218 (86 `func` + 132 `thunk`) |
| Imports distintos referenciados | 132 |
| Funciones sin llamador directo mapeado | 1 112 (1 111 candidatos por prólogo + `0x00401000`) |
| Cobertura de `.text` por extensiones | 100,000 % (por construcción, no prueba de código) |

Validación estructural que sí aporta información: el punto de entrada
`0x00541B20` (416 B) llama directamente a `0x00463278`, la dirección que la
documentación identifica como `WinMain`. El mapa no lo sabía de antemano; la
arista sale del desensamblado.

Segunda validación cruzada: las **40** funciones de la unidad `game/imports` del
informe coinciden una a una con los **40** stubs que el mapa marca `kind=thunk`
con `basis=call`. Los otros 92 stubs no aparecen en el informe porque no son
destino de una llamada directa.

## 7. Reconciliación con ByteMatch (medida)

El informe vigente (`report/EU/report.json`) lista 1 647 entradas. Contraste con
el mapa:

| Categoría | Entradas |
|---|---:|
| Inicio confirmado del mapa (`call`/`entry`/`text-start`/`thunk`) | **1 448** |
| Inicio por prólogo | 0 |
| Límite de instrucción, pero no inicio | 95 |
| Ni siquiera límite de instrucción | **104** |
| **Total del informe** | **1 647** |

Lectura honesta:

- **1 448 entradas son correctas**: son destinos de `call` reales.
- **95 entradas** caen en un límite de instrucción pero no son un inicio
  demostrado. Podrían ser funciones alcanzadas de forma indirecta; el mapa no
  las confirma ni las descarta.
- **104 entradas son imposibles como inicio de función**: una dirección que cae
  en medio de una instrucción no puede ser el primer byte de una función. Son
  falsos positivos del barrido de bytes `E8` (bytes `E8` dentro de inmediatos,
  tablas de salto o datos).
- **94 inicios confirmados no están en el informe**: los 92 stubs que no son
  destino de una llamada directa, el punto de entrada PE y el primer byte de
  `.text`. Los 1 448 destinos de `call` reales sí están. El informe es incompleto
  para los stubs y para los dos inicios estructurales, pero no pierde ninguna
  llamada directa.

Consecuencia: la frase «cada entrada listada es llamada de verdad por el
binario» era falsa y se ha corregido en `docs/BYTEMATCH.md`. El total de 1 647
sigue siendo válido como **lo que publica el informe**, no como recuento de
funciones demostradas. El mapa da **1 542 inicios confirmados** (1 448 `call` +
1 `entry` + 1 `text-start` + 92 `thunk`) y **1 113 candidatos** adicionales.

## 8. Ejemplo trabajado: `0x0043E1BA` (update por frame)

Consulta reproducible:

```powershell
node tools/gexmap/cli.mjs query --function 0x43e1ba
```

| Dato | Valor |
|---|---|
| Extensión | `0x0043E1BA` + 1 155 B (hasta el siguiente inicio, `0x0043E63D`) |
| Llamador directo | `0x0043EA36` (1) |
| Llamadas directas | 24 |
| Imports directos | `KERNEL32.dll!GetTickCount` |
| Imports transitivos | 40, hasta profundidad 9 |

Los 24 callees directos, con tamaño, llamadores mapeados e imports transitivos:

| Callee | Bytes | Llamadores | Imports transitivos |
|---|---:|---:|---:|
| `0x00405C41` | 1 867 | 2 | 0 |
| `0x00412F8B` | 1 764 | 1 | 16 |
| `0x00413D73` | 181 | 1 | 1 |
| `0x0041F714` | 456 | 1 | 1 |
| `0x00421668` | 136 | 1 | 0 |
| `0x00421E5A` | 448 | 1 | 24 |
| `0x00436F6F` | 317 | 1 | 0 |
| `0x0043DC3A` | 1 408 | 2 | 39 |
| `0x0043E719` | 339 | 1 | 18 |
| `0x0045E5C5` | 186 | 3 | 1 |
| `0x0045EAC4` | 1 454 | 1 | 2 |
| `0x00460BF7` | 665 | 2 | 1 |
| `0x00465530` | 90 | 3 | 0 |
| `0x0046DD40` | 2 307 | 2 | 0 |
| `0x0046E643` | 492 | 16 | 1 |
| `0x0046FACE` | 840 | 2 | 1 |
| `0x0047344B` | 17 | 1 | 21 |
| `0x00473471` | 17 | 1 | 20 |
| `0x00537862` | 690 | 1 | 19 |
| `0x0053F610` | 33 | 1 | 0 |
| `0x0053F631` | 1 375 | 1 | 0 |
| `0x005401CD` | 251 | 1 | 16 |
| `0x00540650` | 96 | 32 | 0 |
| `0x00540950` | 48 | 6 | 0 |

Lo que esto **sí** demuestra: el update llama a 24 funciones y, a través de
ellas, puede alcanzar 40 imports distintos, entre ellos Glide, WinMM y
`DirectSoundCreate`. Lo que **no** demuestra: qué subsistema es cada callee, en
qué orden se ejecutan ni con qué datos. Eso sigue siendo trabajo de RE
(comprobación dinámica o lectura guiada), no del mapa.

## 9. Reproducir y mantener

Requisitos: Node.js ≥ 20 (probado con 26.7.0) y `llvm-objdump` u `objdump`.
No requiere Ghidra, Java, REA ni Python.

```bash
node tools/gexmap/cli.mjs map --exe /ruta/GEX3D.EXE
node tools/gexmap/cli.mjs check
node --test tools/gexmap/test.mjs
```

Determinismo comprobado: dos ejecuciones completas sobre el mismo binario
produjeron **hashes idénticos** de `functions.tsv`, `calls.tsv` y `anchors.json`,
y `summary.json` idéntico (incluidos los SHA-256 que fija para los otros tres).

Los artefactos se regeneran cuando cambie el método o la lista de anclas. Si
cambia el binario, `map` falla antes de escribir: las cifras de este documento
solo valen para el SHA-256 indicado arriba.

## 10. Límites y siguiente paso

- **Tamaños = cota superior** (distancia al siguiente inicio), no el tamaño real.
- **Prólogos = hipótesis**. 1 113 candidatos; ninguno promovido.
- **Cobertura 100 %** es del *union* de extensiones, no prueba de que cada byte
  sea código.
- **Sin evidencia dinámica**: el mapa no prueba que una función se ejecute.
- **Sin límites de unidad de compilación**: no sustituye al *splitter* ni al
  `symbols.txt`; da los inicios, el grafo y los imports que el splitter
  necesitará.
- **Sin nombres originales**: `FUN_xxxxxxxx` es una etiqueta derivada de la
  dirección, no un símbolo recuperado (el binario está *stripped*).

Siguiente paso recomendado: usar los 1 448 inicios confirmados y el grafo para
proponer una primera segmentación por subsistemas **con evidencia** (por
ejemplo, agrupando por imports transitivos y por llamadores del update), y
contrastarla con las anclas ya documentadas. Cualquier nombre de subsistema que
se publique debe seguir marcado como HYPOTHESIS hasta que una prueba —estática
guiada o dinámica— lo demuestre.
