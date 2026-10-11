# glide-shim — Proxy `glide2x.dll` de instrumentación (Fase A)

## Qué hace

Sustituto reversible de `glide2x.dll` que reenvía las 38 imports Glide
del exe EU a la DLL real (renombrada) y registra `grBufferSwap`
(emisión) + `grBufferNumPending` (profundidad: todos los cambios +
1/4096 periódico) con ticks QPC crudos en `gshim_log.csv`. **No altera
ninguna llamada.** El exe queda intacto (md5 verificable antes/después).

Diseño v6 (auditoría setvbuf 2026-10-10): anillo 262144 filas en
RAM con stop-on-full (nunca reutiliza; desbordar = alarma + tanda
descartada); `DllMain` solo-ATTACH; volcado final en hook
`grGlideShutdown` + goteo de 64 filas (sin `fflush` explícito;
implícitos acotados a 2 KB vía `setvbuf` COMPROBADO — rechazo =
tanda marcada y descartada, § Impacto);
fallos fail-fast (nunca retornos ficticios); `GSHIM_NOLOG=1` sin tocar
el log y con fallos de marcador audibles; auto-coste medido (footer).

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
`#error`.) En el PC mantenedor, usar `./build-win32.sh` (shell
MSYS2 MINGW32; verifica toolchain + registra evidencia).

## Política de fallos (fail fast, never fake)

| Situación | Comportamiento | Reversible |
|---|---|---|
| Sin QPC / algún símbolo (Swap/Pending/Shutdown) irresoluble | `gshim_error.txt` + salida inmediata código 111. El juego NUNCA sigue con retornos ficticios | Sí: quitar el shim (mismo § Reversión) |
| Anillo lleno (262144 filas) | Marcador `# overflow … RUN INVALID` + `fflush` (una vez) + nota de error; footer `overflow=1`; el fichero conserva un prefijo contiguo pero **la tanda SE DESCARTA SIEMPRE** | N/A (re-diseñar tanda con `pending_calls` del footer) |
| `setvbuf` rechazado (raro: CRT sin búfer propio) | Reenvío intacto, datos completos, pero cota 2 KB INVÁLIDA: error anotado + cabecera `buf=default-UNPINNED`. **Tanda descartada** (mismo criterio error-file); nunca salida 111 ni cota afirmada a ciegas | Sí |
| `gshim_log.csv` no abrible | El reenvío sigue intacto; error anotado + reintento en finalize. El gap (log ausente/corto + error) es visible; el juego no se ve afectado | Sí |
| Marcador NOLOG no escribible (init o end) | Reenvío intacto; error anotado. La tanda NOLOG es INVÁLIDA para A/B (modo o salida limpia no demostrables sin sus 2 líneas) | Sí |
| Salida sin `grGlideShutdown` / crash | Filas de goteo en disco (pérdida acotada, ver § Impacto); footer ausente (visible). Sin footer no hay auto-coste de esa tanda | N/A (datos parciales honestos) |
| Modo NOLOG + fallo crítico | Igual que logging (el reenvío también debe funcionar en NOLOG) | Sí |

Nota: si el juego arranca, el loader ya resolvió los 35 forwarders,
luego el fallo de resolución es una rama de defensa-en-profundidad,
no el caso esperado. Sin UI (determinista, apto para tandas).

## Capacidad (por qué 262144 + 1/4096)

Tanda prevista 60 s: ~1500 filas `S` + filas `P` (cambios + llamadas
totales/4096). Típico (<100K llamadas spin/s) ⇒ <5K filas (margen
~50×). Patológico (spin continuo a ~5M llamadas/s) ⇒ ~150K < 262K.
Desbordar en una tanda prevista solo es posible con un spin
patológico — por eso es una alarma que descarta la tanda, no un
evento rutinario. Si ocurre, el footer trae `pending_calls` exacto
para re-dimensionar con datos (las llamadas/s reales se conocen en
V-2, que es lo que se mide). Las filas volcadas NO se liberan en
RAM: el anillo es un prefijo contiguo write-once.

## Validar ANTES de instalar (PC mantenedor, obligatorio)

V-0. Identidad de la DLL real (pin Hito 1 = payload nGlide 2.10) — SUPERADO 2026-10-10 (salida real PC, ver TESTING):

```powershell
Get-FileHash .\glide2x.dll -Algorithm SHA256
# debe ser 7cbd095872e821b54cd6fa03f76aa22073271567175069c53ebb2e73b0299aab
Get-FileHash .\glide2x.dll -Algorithm MD5
# debe ser f59d9780abe6bcb89433bdad4c8c5d59
(Get-Item .\glide2x.dll).Length  # debe ser 1630208
```

Si el hash NO coincide: parar (DLL distinta a la auditada; reportar
versión/tamaño/hashes y no instalar).

Resultado 2026-10-10 (PC mantenedor): PASS — tamaño 1630208 B,
SHA-256/MD5 exactos, PE32/i386, copia de trabajo idéntica.
Nota: el payload declara metadatos estilo 3dfx (`Glide for Voodoo
Banshee…`, 2.61.00.0658); es lo esperado — la atribución a nGlide
2.10 viene de la cadena instalador→payload→hash de la 9ª sesión
(RESEARCH S-22), no de los metadatos.

V-1. Build Win32 + exports del shim (exactamente los 38 = 35
reenvíos + 3 código) — SUPERADO 2026-10-10 (salida real PC, ver
TESTING). Reproducible vía `build-win32.sh` (shell
MSYS2 MINGW32, toolchain `i686-w64-mingw32`):

```bash
./build-win32.sh [copia-real-verificada.dll]
```

Compila con la línea fijada (`-m32 -shared -O2`), registra
versión del compilador + sha256 de `glide2x.dll`, vuelca
`exports_shim.txt` (evidencia humana) y ejecuta el verificador
`tests/check_exports.py`: siempre en modo shim-only (38 del shim
por bytes) y, si se pasa la copia real, el cruce V-1b completo
(real con las 38, incluidos los 3 interceptores).
Deben aparecer las 38 decoradas (`_grBufferSwap@4`,
`_grBufferNumPending@0`, `_grGlideShutdown@0` resueltos en local;
las 35 restantes como forwarders a `glide2x_gex_real.*`), sin más
ni menos. Si la cuenta no es 38 exacta o falta alguna: NO
instalar (reportar + `exports_shim.txt`).

Resultado 2026-10-10 (PC mantenedor, MSYS2 MINGW32, copia
temporal aislada): PASS, exit 0 — GCC 16.1.0, `V-1 BUILD: PASS`,
DLL PE32/i386 temporal (SHA-256 `c6286811…7dea6df4`, NO en el
repo); 38/38 exactas, 3 código = interceptadas, 35 forwarders
mismo nombre. Hallazgo F-2 — EVALUADO 2026-10-10: benigno y
justificado (`-Wcast-function-type`, cast `GetProcAddress` →
`void (__stdcall *)(int32_t)`; firma/SDK/`@4`/V-1b coinciden).
Fuentes primarias: `gshim.c:311-317`, firmas SDK, `.def`,
V-1b; secundaria: typedef `FARPROC` (corroboración web). Cast
SIN cambios (supresión local solo ante `-Werror` real).
Límite: estático; la ruta dinámica del arnés quedó cubierta por
V-2 (contra fake staged); contra la DLL real sigue pendiente
(P-W5/in-game).

F-3 (2026-10-11, H4a VERIFICADO en PC): `.def` mismatch-alias
`"dec"=X@N` + C SIN dllexport = 38 exactas, 0 extras
(build rc=0, `CHECK_EXPORTS: PASS`). dllexport emitía twins
`X@N` (dedup solo por nombre); sospecha del mantenedor
CONFIRMADA. Integrado en árbol; V-1 H4a superado PC (38/38,
log/sha pendientes de archivar). V-2 bloqueada por 2 fallos del
harness (rutas child_load + cmp ausente); fix en árbol.
Re-run V-2 (a827647): 6/14 — check de bind antes de la 1.ª
llamada (el shim enlaza perezoso por diseño); fix en árbol.
Re-run V-2 (002fd6a): 11/14 — W03 off-by-one 38/37 en scan,
W05/W10 STRICT-99 con IO bloqueada (finalize sí corrió); fix
en árbol. Re-run V-2 (85d72d2): harness 14/14 ALL PASS, suite
FAIL única por fakelogic (vehículo Linux-only <dlfcn.h>/-ldl +
stub redefiniendo __stdcall); fix en árbol. Re-run V-2 (2a8e6da):
harness 14/14; suite roja por OSError EBUSY al limpiar el scratch
de fakelogic (TemporaryDirectory en MSYS2); fix en árbol.
Re-run V-2 (0c22556): **SUPERADO** — `W32HARNESS: 14 passed, 0
failed` + `W32HARNESS: ALL PASS`, `SUITE: ALL PASS`, `V2_RC=0`
(log SHA-256 `3ff4672a…4c3d824ea`); `SKIP behaviour` en MINGW32
= esperado (etapa Linux-host). V-1 PASS en la misma corrida.

W00–W11. Arnés nativo Win32 (conducta SIN Gex ni DLL real, tras
V-1): `./tests/win32/run-harness.sh` carga la DLL compilada en
dirs temporales contra una fake staged al lado (14 escenarios:
init, reenvío×3, NOLOG, footer, doble shutdown, resolución×4,
overflow, bloqueos, prebound, DETACH; repo intacto verificado).
Ver TESTING «Arnés nativo Win32» (comandos, tabla, pendientes
P-W1…P-W6, hallazgo F-1 `%llu`/MSVCRT). Evidencia PC pendiente.

V-1b. Exports de la DLL REAL renombrada (evidencia DIRECTA; los
exports reales NO se infieren de los imports del exe):

```bat
i686-w64-mingw32-objdump -p glide2x_gex_real.dll > exports_real.txt
```

Las 38 decoradas deben estar exportadas (si falta alguna: NO
instalar; reportar + `exports_real.txt`). Veredicto automático:
`python3 tests/check_exports.py glide2x.dll <copia-real>
tests/expected_iat.txt` (lee ambas tablas de exports y exige
arquitectura + resolución de forwarders; su lógica está
auto-probada en `test_exports`, pero el veredicto real exige los
artefactos Windows). Hipótesis de trabajo (CONFIRMADA en este
paso, V-1b 2026-10-10, 38/38): la DLL nGlide 2.10 exporta los 38
nombres que el exe importa — el Hito 1 solo prueba que esa
combinación cargó entonces.

**Estado V-1b: SUPERADO 2026-10-10 (PC mantenedor, MSYS2 MINGW32,
copia temporal aislada).** PASS, exit 0: ambas PE32/i386; cruce
38/38 (0 adicionales), 3 interceptadas + 35 forwarders OK; shim
temporal SHA-256 `1f234c0a…ccb98217` (NO en el repo).
Limitaciones: cruce estático de exports; NO prueba carga en
proceso, Gex ni integración en ejecución (ver TESTING).

**Reconciliación 2026-10-11:** el registro recuperado del PC
coincide (PASS, rc=0, real 38/38) con los criterios documentados
→ se MANTIENE el PASS (hecho histórico intacto). Falta de archivo
concreta, sin reconstruir nada: los artefactos `v1b.log`,
`exports_shim.txt` y `exports_real.txt` nunca estuvieron en git
(`.gitignore` excluye `*.log`) → pendientes en
`REZengineered/Reports/` (Drive), junto al log+SHA de V-1 H4a y
el resto de `matrix2.log`.

V-2. Smoke `NOLOG` (instalado según § Procedimiento, juego 10 s):

> Nota de nombres: «V-2» en este apartado = paso **in-game**
> (juego instalado), aún PENDIENTE. El gate «V-2» del repo =
> arnés+suite en PC (superado 2026-10-11) — no confundir: el
> arnés valida la DLL contra una fake staged, NO que el shim esté
> listo para instalar.

```bat
set GSHIM_NOLOG=1
```

Juego indistinguible; `gshim_nolog.marker` con sus 2 líneas
(init + end con contadores); `gshim_log.csv` NO debe existir;
`gshim_error.txt` NO debe existir. Marcador incompleto o error ⇒
tanda NOLOG inválida: no usar para A/B; reportar, revertir.
(Valida reenvío + carga + salida limpia sin el logger.)

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
   footer `# end` (prueba de finalize vía shutdown) y `overflow=0`;
   juego indistinguible (test de instrumentación, NO equivalencia).
   Sin footer ⇒ el juego no llamó a shutdown en esa salida (ver
   § Formato; reportarlo: decide el fallback `grSstWinClose`,
   pendiente de esta evidencia).
5. Reversión: borrar `glide2x.dll` (shim) + `gshim_log.csv` +
   `gshim_error.txt` + `gshim_nolog.marker` si existen, renombrar
   `glide2x_gex_real.dll` → `glide2x.dll`; re-verificar md5 del
   exe; arranque Hito 1 repetible.

## Impacto en timing (cómo medirlo, no asumirlo)

- Por diseño: el path medido hace 2 QPC + stores en RAM + (1 de
  cada ~64 filas) un goteo de 64 `fprintf` al búfer CRT. Sin
  `fflush` explícito en el path válido. PERO cualquier `fprintf`
  puede disparar una escritura implícita al llenarse el búfer
  (stdio estándar, sin excepción: titular ausencia total de
  syscalls —como hacía la v4— sería falso).
  Cota 2 KB = propiedad ESPERADA de implementación (pendiente de
  confirmación en Windows, punto (b)); jamás se afirma a ciegas:
  el retorno de `setvbuf` SE COMPRUEBA, y si el CRT la rechaza la
  tanda queda marcada (`buf=default-UNPINNED` + error) y se
  descarta. Evidencia separada: (a) OBSERVADO en Linux/glibc a
  nivel syscall (B13: máximo exacto 2048 B, con y sin finalize);
  (b) en Windows/MSVCRT es esperada por contrato C89, pero solo
  V-2 la confirma empíricamente (footer `max` de cada tanda real
  + A/B; el criterio provisional —abajo— decide cada tanda con
  datos).
- `log_cost_us_max`: SÍ captura los `flush` implícitos (ocurren
  dentro de la ventana medida de alguna llamada): es el peor caso
  observado de la tanda. Limitaciones: es un escalar (sin
  distribución; la media sale de `sum`/filas offline); no se
  transfiere entre máquinas/tandas (page-cache, AV, disco);
  confla goteo+flush (conservador, sobre-atribuye); no cubre el
  `fclose` de finalize (fuera del path por diseño); en NOLOG no
  existe (el A/B aísla el reenvío).
- Crash: en disco queda todo menos la cola (<64 filas de anillo
  + ≤2 KB del búfer ≈ ~200 filas; probado: ≥4700/5000 tras
  kill -9).
- Criterio provisional: `max` documentado en el reporte; si `max`
  sale del orden de µs–decenas de µs o el juego va distinto,
  DESCARTAR tanda.

## Formato del log

```text
# gshim 6 (setvbuf audit 2026-10-10) qpf=<ticks/s> (init) buf=2048
# seq,tick_raw,event,arg
1,123456789,S,3
2,123457101,P,0
...
1523,124001337,X,0
# end rows=1523 overflow=0 swaps=1490 pending_calls=88120 log_cost_us_sum=312.4 max=41.7 by=shutdown
```

`S` = swap (arg = `swap_interval`), `P` = pending (todos los
cambios + 1/4096), `X` = marcador shutdown. Conversión offline
exacta: `t_us = tick_raw * 1000000 / qpf` (p. ej. Python; ojo:
modo texto Windows ⇒ `\r\n`). Sin línea `# end` ⇒ finalize no
corrió (salida sin shutdown o crash): filas válidas hasta el
último goteo, sin auto-coste. Con `# overflow … RUN INVALID` ⇒
tanda descartada aunque haya footer.

`gshim_nolog.marker` (solo NOLOG, 2 líneas o tanda inválida):

```text
gshim 6 (setvbuf audit 2026-10-10) nolog=1 qpf=<ticks/s>
end swaps=<n> pending_calls=<n> by=shutdown
```

Cabecera `buf=2048` = cota vigente y comprobada;
`buf=default-UNPINNED` = `setvbuf` rechazado (descartar tanda).
`gshim_error.txt` solo aparece si algo falló; su ausencia es parte
del criterio de aceptación.

## Criterios aceptar/descartar

- ACEPTAR: V-0/V-1/V-1b/V-2 OK; log con swaps (tasa = el valor
  real que sea) + footer `overflow=0 by=shutdown`; NOLOG con
  marcador de 2 líneas; juego indistinguible; md5 exe intacto;
  reversión limpia.
- DESCARTAR (no usar el log): V-x falla / `gshim_error.txt` existe /
  `overflow=1` o marcador `# overflow` / salida código 111 / el
  juego va distinto con el shim → reportar + revertir.

## Pruebas automáticas (suite multi-etapa)

```bash
./tests/run_tests.sh   # estáticas + conductuales; verde = todo pasa
```

Estructurales (propiedades del código, NO conducta): `test_def`
14 (38/38 `.def` vs IAT + hook shutdown), `test_api` 3
(4·nparams=@N vs SDK), `test_init` 74 (DllMain solo-ATTACH,
fail-fast, choke NOLOG, finalize, capacidad/I-O v4, búfer CRT
v5, setvbuf comprobado v6), `test_docs` 3 (tripwires honestidad), `test_exports` 17
(verificador V-1/V-1b auto-probado vs fixtures PE), `test_env` 11
(detección de entorno: python/python3/py, gating de host,
gating win32 build/run, clasificación c-syntax),
`gcc -fsyntax-only`
con stub. Verdes 2026-10-10.

Conductuales (el `gshim.c` REAL compilado contra fakes Win32
funcionales; afirman ficheros/cuentas/orden/códigos/syscalls):
71 checks en 14 escenarios (B1a/b exactitud incl. muestreo 1/4096,
B2 tope 262144 + marcador, B3 NOLOG, B4 marcador roto, B5 111×4,
B6 kill -9, B7 sin QPC, B8 doble shutdown, B10 log bloqueado,
B11 DETACH no-op, B12 fast-path, B13 traza write ≤2 KB +
implícitos sin `fflush`, B14 setvbuf forzado a fallar). Verdes
2026-10-10 (Linux).

En PC Windows (Git Bash): la suite resuelve python vía
`python3`/`python`/`py` (solo acepta el que ejecuta de verdad; el
alias de Microsoft Store se descarta) y las etapas de host
(c-syntax/behaviour) se SKIPean fuera de Linux con motivo
explícito — nunca validan el build Win32 (`build-win32.sh`).

La etapa 9 (`win32-harness`) compila y ejecuta el arnés nativo
W00–W11 solo con Windows + MinGW-32 (ver § Validar ANTES de
instalar); sin ellos, SKIP ruidoso.

Ni las estructurales ni las conductuales sustituyen a Windows:
V-0 superado 2026-10-10 (identidad + copia, PC mantenedor);
V-1 superado 2026-10-10 (build + 38/38, PC mantenedor; F-2
evaluado benigno); V-1b superado 2026-10-10 (cruce vs DLL real);
V-2 superado 2026-10-11 (PC mantenedor, sobre `0c22556`: arnés
14/14 + `SUITE: ALL PASS` + `V2_RC=0`; log sha256 `3ff4672a…`).

## Estado, riesgos abiertos y evidencia que FALTA

Verificado: IAT 38/38 + aridades SDK + suites verde (estructural
14+3+74+3+3+17+11 y conductual 71) + `DllMain` solo-ATTACH + fail-fast +
choke NOLOG + `setvbuf` comprobado (cota 2 KB esperada; observada
≤2048 B en Linux; en MSVCRT, aceptación de `setvbuf` confirmada por
W01 y pérdida acotada por W08 en PC 2026-10-11, V-2). Riesgos ABIERTOS (no bloquean el build, condicionan
el uso): (1) el juego podría salir sin `grGlideShutdown` ⇒ sin
footer (lo decide la tanda real in-game; fallback WinClose pendiente); (2) tasa de
llamadas spin real desconocida hasta la tanda real in-game (márgenes calculados,
alarma lista); (3) latencia concreta de implícitos en Windows
solo medible allí (footer `max` + A/B por tanda; page-cache/AV
pueden moverla); (4) efectividad exacta de la cota 2 KB en MSVCRT:
por contrato C89 + W01/W08 en PC (aceptación y pérdida acotada);
la igualdad exacta no se mide sin hooks (B13 solo observa glibc) y
no se afirma como promesa
portable. Los exports de la DLL concreta se verifican
en V-1b (evidencia directa); la carga real en el juego, en el
procedimiento in-game. Pendiente explícito: procedimiento
in-game (instalación en copia + smoke NOLOG + tanda real:
requiere ejecutar el juego) y P-W5 (35 forwarders vs DLL real).
El arnés W00–W11 y la suite ya tienen evidencia PC 2026-10-11
(V-2 superado).
**Compatibilidad plena NO declarada; la validación dinámica del
arnés (fake) está superada (V-2); la in-game sigue sin declarar.**
