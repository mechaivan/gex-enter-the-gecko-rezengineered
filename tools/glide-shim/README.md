# glide-shim — Proxy `glide2x.dll` de instrumentación (Fase A)

## Qué hace

Sustituto reversible de `glide2x.dll` que reenvía las 38 imports Glide
del exe EU a la DLL real (renombrada) y registra `grBufferSwap`
(emisión) + `grBufferNumPending` (profundidad, muestreada por cambio +
1/1024) con ticks QPC crudos en `gshim_log.csv`. **No altera ninguna
llamada.** El exe queda intacto (md5 verificable antes/después).

Diseño v3 (revisión de riesgos 2026-10-10): `DllMain` solo-ATTACH
(DETACH no-op: cero I/O bajo loader-lock); volcado final en hook
`grGlideShutdown` (hilo del juego) + persistencia incremental cada
4096 filas; sin I/O por frame (anillo RAM) ni división por fila
(conversión offline exacta); fallos fail-fast (nunca retornos
ficticios); modo `GSHIM_NOLOG=1` sin tocar `gshim_log.csv`;
auto-coste medido (footer).

Evidencia que produce (1ª dinámica real del proyecto): tasa real de
presents/s (S-01), intervalos swap usados por escena, profundidad de
cola, dependencia escena (input H2/H3), ancla STEP. Sin tocar registro
ni exe.

## Requisitos

PC Windows + MinGW 32-bit (`i686-w64-mingw32-gcc`, build `-m32`
obligatorio: las decoraciones stdcall `_nombre@N` son x86-32).
Copia de trabajo del juego (NO el original).

## Construir

```bat
i686-w64-mingw32-gcc -m32 -shared -O2 -o glide2x.dll gshim.c gshim.def
```

Debe compilar sin errores. (Un build x86-64 lo rechaza `gshim.c` con
`#error`.)

## Política de fallos (fail fast, never fake)

| Situación | Comportamiento | Reversible |
|---|---|---|
| Sin QPC / algún símbolo (Swap/Pending/Shutdown) irresoluble | `gshim_error.txt` + salida inmediata código 111. El juego NUNCA sigue con retornos ficticios | Sí: quitar el shim (mismo § Reversión) |
| `gshim_log.csv` no abrible | El reenvío sigue intacto; error anotado + reintento en finalize. El gap (log ausente/corto + error) es visible; el juego no se ve afectado | Sí |
| Salida sin `grGlideShutdown` / crash | Filas incrementales en disco; footer ausente (visible, ver § Formato). Sin footer no hay auto-coste de esa tanda | N/A (datos parciales honestos) |
| Modo NOLOG + fallo | Igual que arriba (el reenvío también debe funcionar en NOLOG) | Sí |

Nota: si el juego arranca, el loader ya resolvió los 35 forwarders,
luego el fallo de resolución es una rama de defensa-en-profundidad,
no el caso esperado. Sin UI (determinista, apto para tandas).

## Validar ANTES de instalar (PC mantenedor, obligatorio)

V-0. Identidad de la DLL real (pin Hito 1 = payload nGlide 2.10):

```powershell
Get-FileHash .\glide2x.dll -Algorithm SHA256
# debe ser 7cbd095872e821b54cd6fa03f76aa22073271567175069c53ebb2e73b0299aab
Get-FileHash .\glide2x.dll -Algorithm MD5
# debe ser f59d9780abe6bcb89433bdad4c8c5d59
(Get-Item .\glide2x.dll).Length  # debe ser 1630208
```

Si el hash NO coincide: parar (DLL distinta a la auditada; reportar
versión/tamaño/hashes y no instalar).

V-1. Exports del shim compilado (exactamente los 38 = 35 reenvíos +
3 código):

```bat
i686-w64-mingw32-objdump -p glide2x.dll > exports_shim.txt
```

Deben aparecer las 38 decoradas (`_grBufferSwap@4`,
`_grBufferNumPending@0`, `_grGlideShutdown@0` resueltos en local; las
35 restantes como forwarders a `glide2x_gex_real.*`), sin más ni
menos (alternativa: `dumpbin /exports`, o Dependencies.exe). Si la
cuenta no es 38 exacta o falta alguna: NO instalar (reportar +
`exports_shim.txt`).

V-1b. Exports de la DLL REAL renombrada (evidencia DIRECTA; los
exports reales NO se infieren de los imports del exe):

```bat
i686-w64-mingw32-objdump -p glide2x_gex_real.dll > exports_real.txt
```

Las 38 decoradas deben estar exportadas (si falta alguna: NO
instalar; reportar + `exports_real.txt`). Hipótesis de trabajo (SIN
verificar hasta este paso): la DLL nGlide 2.10 exporta los 38
nombres que el exe importa — el Hito 1 solo prueba que esa
combinación cargó entonces.

V-2. Smoke `NOLOG` (instalado según § Procedimiento, juego 10 s):

```bat
set GSHIM_NOLOG=1
```

Juego indistinguible; `gshim_nolog.marker` con líneas init+end
(contadores para A/B); `gshim_log.csv` NO debe existir ni crecer;
`gshim_error.txt` NO debe existir. Si existe: leerlo, reportar,
revertir. (Valida reenvío + carga + salida limpia sin el logger.)

## Procedimiento (copia de trabajo)

1. Punto de restauración: `md5 GEX3D.EXE` (= pin repo `692b1282…`);
   copia de seguridad de `glide2x.dll` original fuera de la carpeta.
2. V-0. Renombrar en la copia: `glide2x.dll` →
   `glide2x_gex_real.dll`. V-1 + V-1b.
3. Copiar la `glide2x.dll` compilada a la carpeta. Borrar
   `gshim_log.csv` / `gshim_error.txt` / `gshim_nolog.marker`
   previos si existen.
4. V-2 (smoke NOLOG, verifica también salida limpia). Luego tanda
   real: `set GSHIM_NOLOG=` (vacío), jugar 60 s (escena simple +
   escena compleja), salir normal. Verificar: `gshim_log.csv` con
   footer `# end` (prueba de finalize vía shutdown); juego
   indistinguible (test de instrumentación, NO equivalencia).
   Sin footer ⇒ el juego no llamó a shutdown en esa salida (ver
   § Formato; reportarlo: decide el fallback `grSstWinClose`,
   pendiente de esta evidencia).
5. Reversión: borrar `glide2x.dll` (shim) + `gshim_log.csv` +
   `gshim_error.txt` + `gshim_nolog.marker` si existen, renombrar
   `glide2x_gex_real.dll` → `glide2x.dll`; re-verificar md5 del
   exe; arranque Hito 1 repetible.

## Impacto en timing (cómo medirlo, no asumirlo)

- Por diseño: el path medido hace 2 QPC + stores en RAM (sin I/O,
  sin división); volcado incremental cada ~3 min + finalize en
  shutdown (fuera del gameplay medido).
- Footer de cada tanda (`# end ...`): `rows`, `overflow` (=0
  esperado), `log_cost_us_sum/max` = coste propio del logger,
  `by=shutdown`.
  Criterio provisional: `max` documentado en el reporte; si `max`
  sale del orden de µs o el juego va distinto, DESCARTAR tanda.
- Protocolo A/B: misma escena con `GSHIM_NOLOG=1` (contadores en
  `.marker`) vs con log → FPS iguales dentro del ruido = impacto
  despreciable. El modo NOLOG aísla el coste del reenvío del
  coste del logger (cero I/O de log en NOLOG).

## Formato del log

```text
# gshim 3 (risk-review 2026-10-10) qpf=<ticks/s> (init)
# seq,tick_raw,event,arg
1,123456789,S,3
2,123457101,P,0
...
1523,124001337,X,0
# end rows=1523 overflow=0 swaps=1490 pending_calls=88120 log_cost_us_sum=312.4 max=41.7 by=shutdown
```

`S` = swap (arg = `swap_interval`), `P` = pending muestreado,
`X` = marcador shutdown (incluido en rows). Conversión offline
exacta: `t_us = tick_raw * 1000000 / qpf` (p. ej. Python; ojo:
modo texto Windows ⇒ `\r\n`). Sin línea `# end` ⇒ finalize no
corrió (salida sin shutdown o crash): filas válidas hasta el
último volcado incremental, sin auto-coste.

`gshim_nolog.marker` (solo NOLOG):

```text
gshim 3 (risk-review 2026-10-10) nolog=1 qpf=<ticks/s>
end swaps=<n> pending_calls=<n> by=shutdown
```

`gshim_error.txt` solo aparece si algo falló; su ausencia es parte
del criterio de aceptación.

## Criterios aceptar/descartar

- ACEPTAR: V-0/V-1/V-1b/V-2 OK; log con swaps (tasa = el valor
  real que sea) + footer `overflow=0 by=shutdown`; juego
  indistinguible; md5 exe intacto; reversión limpia.
- DESCARTAR (no usar el log): V-x falla / `gshim_error.txt` existe /
  salida código 111 / el juego va distinto con el shim → reportar
  + revertir.

## Pruebas automáticas (sin Windows)

```bash
./tests/run_tests.sh   # .def 38/38 + aridades SDK + estructura init/exit + sintaxis C
```

`test_def` (cobertura .def vs IAT + hook shutdown), `test_api`
(4·nparams=@N vs SDK), `test_init` (estructural: DllMain sin DETACH,
fail-fast, choke NOLOG, finalize idempotente). Verdes 2026-10-10
(14+3+55 checks + `gcc -fsyntax-only` con stub). Estructurales:
NO prueban conducta en Windows (eso es V-1/V-1b/V-2).

## Estado y evidencia que FALTA (no declarar compatibilidad)

Verificado: IAT 38/38 (0 ordinales, 0 sin decorar; exe PE32/i386) +
aridades 38/38 contra SDK Glide 2.x (`FX_CALL=__stdcall`) + suite
verde + `DllMain` solo-ATTACH + guarda 32-bit + fail-fast. Los
exports de la DLL concreta se verifican en V-1b (evidencia
directa); la carga real, en V-2. Pendiente explícito de Windows:
build MinGW-32 + V-0/V-1/V-1b/V-2 + confirmar footer tras salida
normal (decide el fallback `grSstWinClose`). **Compatibilidad
plena y validación dinámica NO declaradas.**
