# tools/gexmap — mapa estático de funciones (GEX3D.EXE EU)

Desensambla el ejecutable original y publica un mapa **medido** de funciones,
llamadas e imports. No reimplementa nada y no cambia el porcentaje de ByteMatch
(sigue 0,00 %). El resultado y sus límites están en
[`docs/FUNCTION_MAP.md`](../../docs/FUNCTION_MAP.md).

## Requisitos

- Node.js ≥ 20 (probado con 26.7.0).
- `llvm-objdump` u `objdump` con soporte Intel x86. Ruta explícita con
  `--objdump`, o en `PATH`, o en `GEX_OBJDUMP`.
- El `GEX3D.EXE` EU original (no se copia al repositorio; `.gitignore` excluye
  `*.exe`). Se verifica tamaño, MD5 y SHA-256 antes de escribir nada.

No requiere Ghidra, Java, REA ni Python.

## Uso

```bash
# Generar analysis/EU/{functions.tsv,calls.tsv,anchors.json,summary.json}
node tools/gexmap/cli.mjs map --exe /ruta/GEX3D.EXE

# Directorio de salida alternativo
node tools/gexmap/cli.mjs map --exe /ruta/GEX3D.EXE --out /tmp/gexmap

# Validar los artefactos comprometidos sin el binario
node tools/gexmap/cli.mjs check

# Consultar una función
node tools/gexmap/cli.mjs query --function 0x43e1ba

# Primera segmentación por subsistemas (HYPOTHESIS) + validación de evidencias
node tools/gexmap/cli.mjs segments
node tools/gexmap/cli.mjs segments --json

# Tests offline (sintéticos + invariantes de los artefactos + segmentación + frame + audio)
node --test tools/gexmap/test.mjs
```

`map` es determinista: dos ejecuciones sobre el mismo binario producen los
mismos hashes (comprobado). `check` recalcula el SHA-256 de los tres artefactos
y lo compara con el fijado en `summary.json`, además de validar recuentos,
solapes, clasificación, aristas y anclas.

`segments` no escribe nada: agrupa 65 inicios en siete subsistemas **HYPOTHESIS**
y recalcula cada evidencia (`I`/`T`/`A`/`G`/`O`) desde los artefactos; las capas
manuales (`S` string, `R` desensamblado) se declaran con su detalle. Falla si una
evidencia no se sostiene, si una dirección se repite o si la confianza declarada
no coincide con la derivada. El método y los resultados están en
[`docs/SUBSYSTEM_SEGMENTS.md`](../../docs/SUBSYSTEM_SEGMENTS.md).

`frame.mjs` no analiza nada: fija la traza dinámica del frame step
(`docs/FRAME_STEP_TRACE.md`) como datos —las cinco llamadas de `0x0043EA36`, los
25 sitios de llamada del update con sus recuentos medidos, el render, el bucle de
cine y 19 fichas de semántica mecánica— y expone `validateFrame(model)`, que las
contrasta contra `analysis/EU/`: cada sitio dentro de su llamante, arista
presente, `ret = site + 5`, ningún sitio con más frames que su sesión y toda
afirmación CONFIRMED con al menos dos líneas de evidencia con dirección. Se
valida sin binario ni ejecución, así que corre en CI; los recuentos se
reproducen con [`tools/frame-trace/`](../frame-trace/README.md).

`audio.mjs` tampoco analiza: fija las rutas de inicialización de DirectSound
(`docs/AUDIO_INIT_PATHS.md`) —14 globales con su único escritor, 20 llamadas por
vtable, 3 rutas alcanzables desde la entrada y 1 sin llamantes, más 19 fichas de
semántica mecánica— y expone `validateAudio(model)`. Comprueba que todo sitio de
ruta es una arista real dentro de su llamante; que `0x00538551` tiene
exactamente dos llamantes; que `0x00538FD5` sigue con cero y `0x00538CB0` con
uno; que viva y muerta solo comparten `0x005404F0`; y que ninguna ruta declara
hits o frames sin sesión. La ruta de voz se marca `partial` y el validador
**exige** que no empiece en la entrada.

## Qué significa cada columna

`functions.tsv`:

| Columna | Significado |
|---|---|
| `addr` | inicio (dirección virtual) |
| `size` | distancia al siguiente inicio: **cota superior**, incluye relleno/tablas |
| `kind` | `func` o `thunk` (stub `jmp dword ptr [IAT]`) |
| `basis` | `call`, `entry`, `text-start` (CONFIRMADO) · `prologue` (HIPÓTESIS) · `thunk` (estructural) |
| `symbol` | `FUN_xxxxxxxx` o `DLL!Función` para stubs |
| `callers` | funciones mapeadas que llaman a este inicio |
| `callees` | número de destinos directos distintos |
| `imports` | imports usados en la extensión, separados por `;` |

`calls.tsv`: `from`, `to`, `kind` (`call`/`tail`), `count` (cuántas veces).
`anchors.json`: las 24 direcciones citadas en la documentación y la función que
las contiene.

## Reglas de evidencia

- Un inicio es **CONFIRMADO** solo si lo alcanza una instrucción `call rel32`
  decodificada, el `AddressOfEntryPoint` del PE o el primer byte de `.text`.
  No se escanean bytes `E8`: eso produce destinos dentro de inmediatos y tablas.
- Los `push ebp; mov ebp, esp` fuera de esos inicios son **candidatos**
  (`basis=prologue`). Se publican aparte; nunca se suman a los confirmados.
- Los stubs de importación se separan para que sus imports no se atribuyan a la
  función anterior. Son estructura del enlazador, no código del juego.
- Ningún nombre `FUN_*` afirma qué hace la función. El mapa es estructura, no
  semántica.

## Ficheros

| Fichero | Papel |
|---|---|
| `cli.mjs` | comandos `map`, `check`, `query`, `segments`; anclas y reconciliación |
| `pe.mjs` | lector PE32: secciones, entrada, base de imagen, IAT |
| `disasm.mjs` | ejecuta objdump y parsea dirección, bytes y texto |
| `analyze.mjs` | reparación, inicios, stubs, extensiones, grafo, imports |
| `segments.mjs` | definiciones HYPOTHESIS, evidencia por miembro y validación |
| `frame.mjs` | traza dinámica del frame step como datos + `validateFrame()` |
| `audio.mjs` | rutas de inicialización de DirectSound + `validateAudio()` |
| `test.mjs` | tests offline e invariantes de los artefactos, segmentos, frame y audio |

## Límites

- Tamaños = cota superior, no tamaño real.
- Sin evidencia dinámica: no prueba que una función se ejecute.
- Sin límites de unidad de compilación: no sustituye al splitter ni al
  `symbols.txt`.
- Los 1 113 candidatos por prólogo siguen siendo hipótesis.
- La cobertura del 100 % es del *union* de extensiones, no prueba de que cada
  byte sea código.
