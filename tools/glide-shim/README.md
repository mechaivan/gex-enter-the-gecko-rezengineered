# glide-shim — Proxy `glide2x.dll` de instrumentación (Fase A)

## Qué hace

Sustituto reversible de `glide2x.dll` que reenvía las 38 imports Glide
del exe EU a la DLL real (renombrada) y registra `grBufferSwap`
(emisión) + `grBufferNumPending` (profundidad, muestreada por cambio +
1/1024) con ticks QPC crudos en `gshim_log.csv`. **No altera ninguna
llamada.** El exe queda intacto (md5 verificable antes/después).

Diseño v2 (auditoría 2026-10-10): sin I/O por frame (anillo en RAM +
volcado incremental cada 4096 filas + cola en detach), sin división por
fila (conversión offline exacta con el `qpf` de cabecera), `DllMain`
mínimo (resolución perezosa fuera del loader-lock), auto-coste medido
(footer) y modo `GSHIM_NOLOG=1` (pass-through puro para A/B).

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

V-1. Exports del shim compilado (exactamente los 38):

```bat
i686-w64-mingw32-objdump -p glide2x.dll > exports.txt
```

`exports.txt` debe listar las 38 decoradas: las 36 de `gshim.def`
+ `_grBufferSwap@4` + `_grBufferNumPending@0`, sin más ni menos
(alternativa: `dumpbin /exports`, o Dependencies.exe). Si la cuenta
no es 38 exacta o falta alguna: NO instalar (reportar + `exports.txt`).
Las 36 deben figurar como forwarders a `glide2x_gex_real.*`.

V-2. Smoke `NOLOG` (instalado según § Procedimiento, juego 10 s):

```bat
set GSHIM_NOLOG=1
```

Juego indistinguible; `gshim_log.csv` con marcador `nolog=1`;
`gshim_error.txt` NO debe existir. Si existe: leerlo, reportar,
revertir. (Valida reenvío + carga sin el logger.)

## Procedimiento (copia de trabajo)

1. Punto de restauración: `md5 GEX3D.EXE` (= pin repo `692b1282…`);
   copia de seguridad de `glide2x.dll` original fuera de la carpeta.
2. V-0 + V-1 (arriba). Renombrar en la copia: `glide2x.dll` →
   `glide2x_gex_real.dll`.
3. Copiar la `glide2x.dll` compilada a la carpeta. Borrar
   `gshim_log.csv` / `gshim_error.txt` previos si existen.
4. V-2 (smoke NOLOG). Luego tanda real: `set GSHIM_NOLOG=` (vacío),
   jugar 60 s (escena simple + escena compleja), salir. Verificar:
   `gshim_log.csv` existe y crece; juego indistinguible (test de
   instrumentación, NO equivalencia); leer el footer (§ Impacto).
5. Reversión: borrar `glide2x.dll` (shim) + `gshim_log.csv` +
   `gshim_error.txt` si existe, renombrar `glide2x_gex_real.dll` →
   `glide2x.dll`; re-verificar md5 del exe; arranque Hito 1 repetible.

## Impacto en timing (cómo medirlo, no asumirlo)

- Por diseño: el path medido hace 2 QPC + stores en RAM (sin I/O,
  sin división); volcado incremental cada ~3 min + cola en detach.
- Footer de cada tanda (`# end ...`): `rows`, `overflow` (=0
  esperado), `log_cost_us_sum/max` = coste propio del logger.
  Criterio provisional: `max` documentado en el reporte; si `max`
  sale del orden de µs o el juego va distinto, DESCARTAR tanda.
- Protocolo A/B: misma escena con `GSHIM_NOLOG=1` vs con log →
  FPS iguales dentro del ruido = impacto despreciable. El modo
  NOLOG (V-2) ya aísla el coste del reenvío del coste del logger.

## Formato del log

```text
# gshim 2 (audit 2026-10-10) qpf=<ticks/s> nolog=0
# seq,tick_raw,event,arg
1,123456789,S,3
2,123457101,P,0
...
# end rows=<n> overflow=0 swaps=<n> pending_calls=<n> log_cost_us_sum=<..> max=<..> nolog=0 init_failed=0
```

`S` = swap (arg = `swap_interval`), `P` = pending muestreado.
Conversión offline exacta: `t_us = tick_raw * 1000000 / qpf`
(p. ej. Python; ojo: modo texto Windows ⇒ `\r\n`).

`gshim_error.txt` solo aparece si algo falló (init/log); su
ausencia es parte del criterio de aceptación.

## Criterios aceptar/descartar

- ACEPTAR: V-0/V-1/V-2 OK; log con swaps (tasa = el valor real que
  sea); footer `overflow=0 init_failed=0`; juego indistinguible;
  md5 exe intacto; reversión limpia.
- DESCARTAR (no usar el log): V-x falla / `gshim_error.txt` existe /
  el juego va distinto con el shim → reportar + revertir.

## Pruebas automáticas (sin Windows)

```bash
./tests/run_tests.sh   # .def 38/38 + aridades SDK + estructura init + sintaxis C
```

`test_def` (cobertura .def vs IAT), `test_api` (4·nparams=@N vs SDK),
`test_init` (estructural: DllMain mínimo, init perezoso, guarda 32-bit).
Verde 2026-10-10 (12+3+21 checks + `gcc -fsyntax-only` con stub).

## Estado y evidencia que FALTA (no declarar compatibilidad)

Verificado: IAT 38/38 (0 ordinales, 0 sin decorar; exe PE32/i386) +
aridades 38/38 contra SDK Glide 2.x (`FX_CALL=__stdcall`) + suite
verde + `DllMain` mínimo + guarda 32-bit. Por transitividad (el exe
corre contra nGlide en Hito 1), la DLL real exporta los 38 nombres.
FALTA (requiere PC Windows): build MinGW-32 + `objdump/dumpbin`
mecánico (V-1) + carga dinámica real (V-2). **Compatibilidad plena
NO confirmada hasta V-1/V-2.**
