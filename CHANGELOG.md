# CHANGELOG

Formato: `YYYY-MM-DD — versión — descripción`.

## 2026-10-10 — 0.0.0 — Shim Glide v6: setvbuf comprobado (cota 2 KB condicional + evidencia separada)

- Hallazgo: el retorno de `setvbuf` NO se comprobaba — si el CRT
  rechazaba el búfer de 2 KB, la v5 afirmaba igual la cota: falso.
- Solución: `open_log` comprueba `setvbuf(...) == 0`
  (`buf_pinned`); rechazo ⇒ reenvío intacto + datos completos +
  tanda LOUDLY marcada (`buf=default-UNPINNED` + `gshim_error.txt`)
  y descartada por el criterio error-file existente (nunca 111).
  Cabecera gana campo `buf=2048|default-UNPINNED` (crash-safe).
- B14: doble de `setvbuf` en el harness (forzado por env,
  passthrough `RTLD_NEXT` si no): 6 checks (reenvío, error,
  cabecera, 501 filas, sin fail-fast). `buf=2048` afirmado en
  B1a/B6. Suites verdes 14+3+74+3 y 71 conductual.
- Docs honestos: cota 2 KB = propiedad ESPERADA pendiente de
  confirmación en Windows (no promesa portable); evidencia
  separada — (a) OBSERVADO ≤2048 B en Linux/glibc (B13),
  (b) MSVCRT esperado por contrato C89, V-2 lo confirma (footer
  `max` + A/B). Tripwires `test_docs` ampliados (banean
  absolutos portables).
- Sync: README v6 (Impacto condicional + fila de fallo + riesgo
  MSVCRT), MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (hitos, no conteos por auditoría).

## 2026-10-10 — 0.0.0 — Shim Glide v5: auditoría búfer CRT (fin del «cero syscalls»)

- Hallazgo: `fprintf` SÍ dispara escrituras implícitas al llenarse
  el búfer aunque no haya `fflush` (stdio estándar) — la v4 lo
  admitía como «residual» pero titulaba «cero syscalls»: falso y
  retirado de README + `gshim.c` (tripwire `test_docs`).
- Solución mínima: `setvbuf` 2 KB (C89 portable; MSVCRT y glibc
  lo respetan) ⇒ cada implícito ≤2 KB (~1–10 µs) con cadencia
  determinista (~1/100 filas) en ambos CRT; sin hilos ni
  bloqueos. Alternativas descartadas: búfer gigante (mata la
  forense de crash), sin búfer (syscall por fila), mmap/hilos
  (complejidad). Crash acotado a ~200 filas + cola de 64.
- `log_cost_us_max` SÍ captura los implícitos (ocurren dentro de
  la ventana medida) — documentadas 6 limitaciones (escalar, no
  transfiere entre máquinas, confla goteo+flush, sin finalize,
  sin NOLOG, page-cache/AV).
- Pruebas: B13 traza write() a nivel syscall (ptrace propio;
  LD_PRELOAD descartado: no ve escrituras intra-libc): máx
  exacto 2048 B con y sin finalize; 40 implícitos en tanda
  crash sin ningún `fflush` (prueba del punto 2). Suites verdes
  14+3+69+3 y 63 conductual. Sin Windows/Gex (V-x pendientes).
- Sync: README v5 (timing honesto + método), MODERNIZATION M-13,
  PROJECT_STATE, REVERSE_ENGINEERING, tools/README. Panel intacto.

## 2026-10-10 — 0.0.0 — Shim Glide v4: capacidad, I-O en path y marcador NOLOG

- Auditoría final pre-build (sin ejecutar Gex/compilar en Windows
  ni tocar instalaciones; dinámica NO declarada): (1) anillo a
  262144 stop-on-full (nunca reutiliza) + marcador `# overflow`
  + `fflush` una sola vez + nota error; `overflow=1` descarta la
  tanda siempre; análisis de capacidad (típico <5K, patológico
  <150K/60 s; footer `pending_calls` para re-dimensionar).
  (2) I/O en path: fuera el `fflush` cada 4096 (picos ~ms en el
  frame); goteo de 64 filas al búfer CRT sin syscalls por diseño;
  coste residual (~µs, 1/64 filas + flush CRT raros) medido por
  footer `max` + A/B NOLOG; crash acotado (~centenas de filas).
  Muestreo pending 1/1024→1/4096 (cambios siempre exactos).
  (3) Marcador NOLOG: fallos init/end audibles (error + tanda
  inválida para A/B); reenvío intacto.
- Nueva suite CONDUCTUAL (`tests/behavior`, 57 checks verdes):
  el `gshim.c` real compilado contra fakes Win32 (QPC/env/
  resolución programables): exactitud filas/footer/muestreo,
  tope + marcador, NOLOG, marcador roto, 111×4 + sin QPC,
  kill -9, doble shutdown, log bloqueado, DETACH no-op,
  fast-path. Estructurales 14+3+65 (10 pines v4 nuevos).
  Conductual ≠ validación Windows (V-0/V-1/V-1b/V-2 pendientes).
- Sync: README v4 (capacidad + timing + riesgos abiertos),
  MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (auditoría ≠ hito).

## 2026-10-10 — 0.0.0 — Shim Glide v3: revisión final de riesgos pre-build

- Revisión punto por punto (sin ejecutar Gex ni tocar
  instalaciones/registro; validación dinámica NO declarada):
  (1) `DllMain` pasa a solo-ATTACH (DETACH no-op: cero I/O bajo
  loader-lock; riesgo deadlock evaluado); volcado final en hook
  `grGlideShutdown` (hilo del juego, idempotente) + incremental
  cada 4096. (2) `GSHIM_NOLOG=1` aislado: `gshim_log.csv` con un
  único punto de apertura (`open_log`, call-sites guardados);
  señal = `gshim_nolog.marker` (init+end, contadores A/B).
  (3) Política fail-fast documentada: sin QPC o símbolo
  irresoluble → `gshim_error.txt` + exit 111, nunca retornos
  ficticios; log no abrible → reenvío intacto + gap visible.
  (4) Fuera la inferencia por transitividad: hipótesis explícita
  + V-1b (volcado DIRECTO de exports de la DLL real renombrada).
- `.def`: 35 reenvíos + 3 código (Shutdown se une a Swap/Pending).
  Suite verde 14+3+55 + sintaxis; `test_init` cubre las 4
  propiedades (estructural, no conductual). Footer con `by=` +
  fila `X`; fallback `grSstWinClose` pendiente explícito de V-2
  (footer tras salida normal).
- Sync: README v2→v3 (política fallos + V-1b + formato),
  MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (revisión ≠ hito).

## 2026-10-10 — 0.0.0 — Auditoría pre-ejecución del shim Glide (v2 + suite)

- Auditoría estática de `tools/glide-shim` (sin ejecutar Gex ni
  tocar instalaciones): 6 defectos encontrados y corregidos —
  D-01 exports de wrappers dependientes del auto-export (ahora
  `dllexport` + `.def` explícito), D-02 `LoadLibrary`/fopen en
  `DllMain` (ahora `DllMain` mínimo + init perezoso con guarda),
  D-03 `fflush` por swap (ahora anillo RAM 64K + volcado
  incremental), D-04 overflow `tick*1e6` (ahora ticks crudos +
  conversión offline), D-05 sin guarda de arquitectura (ahora
  `#error` si no-i386), D-06 carrera en init (Interlocked).
- Verificación nueva (más allá de IAT vs `.def`): exe PE32/i386
  re-confirmado, 38 imports Glide nombrados (0 ordinales, 0 sin
  decorar); aridades 38/38 contra SDK Glide 2.x
  (`sezero/glide`, `FX_CALL=__stdcall`; rama clásica en los 3
  prototipos duales). Sin MinGW/Wine/DLL real en sandbox
  (verificado ausente): build + dumpbin + dinámica quedan como
  validación V-1/V-2 en PC mantenedor (pins hash DLL Hito 1 en
  README). Compatibilidad plena NO confirmada.
- Nueva suite `./tests/run_tests.sh` (verde: 12+3+21 checks +
  `gcc -fsyntax-only`): `test_def` (38/38), `test_api`
  (4·nparams=@N), `test_init` (estructural, no conductual).
  README v2: construir + validar (V-0/V-1/V-2) + impacto timing
  (footer auto-coste + A/B `GSHIM_NOLOG`) + reversión.
- Sync: MODERNIZATION M-13, PROJECT_STATE, REVERSE_ENGINEERING,
  tools/README. Panel intacto (auditoría ≠ hito).

## 2026-10-10 — 0.0.0 — Pívot: I-23 identificada, R-audio confirmado, núcleo M-13 verificado, shim Glide (fuente)

- Autorización expresa (pívot 2026-10-10): modificar código,
  investigar dinámicamente (incl. origen I-23) y experimentos
  controlados; originales intactos + hashes + copias + restore
  points. Análisis sobre copia trabajo (md5 `692b1282…`, fuera
  del repo); nada redistribuido.
- I-23 causa IDENTIFICADA (estática): WinMain `0x463281–95` lee
  `InstallDir`/`CDDriveName` vía `0x442F41`
  (`HKLM\…\Gex2\1.00`, REG_SZ); `0x4329EF` compone
  `<InstallDir>\font.3df`, fallo → fatal `0x463011`. E-2 método
  REPARADO: E-2.3 (conmutación InstallDir por copia, `/reg:32`,
  restore point + criterios) LISTO PARA EJECUTAR en PC
  mantenedor; confirmación dinámica pendiente. Micro-item:
  `font.3df` vs `font3.dff` (1 dir listing).
- R-audio CONFIRMADO: fmt `0x55A708` (`<InstallDir>\audio\
  voiceuk\<n>.sag`, `0x53FF0D`) + `VOICEUK/` 400 `.SAG`;
  `0x53FCB7` decode completo (pump + scheduler 375 + array
  voces); tabla `0x5ACBE4` filas por voz (R2 WORD = canal voz,
  hipótesis fuerte) → audio wall-clock obligatorio, valida
  propuesta §4. `0x43F80D`: 3 llamadores efectivos
  (frame/init/cine) + `0x4D5763` muerto. IAT Glide: 38 imports
  con slots (`0x65F45C–0x65F4F0`).
- Nuevo `tools/m13-core` (núcleo §4 C11 + harness): 120 checks,
  0 fallos (gcc Linux 2026-10-10); NO integrado, M-13 PROPOSED.
- Nuevo `tools/glide-shim` (proxy instrumentación Fase A):
  fuente C + `.def` 36/36 verificado contra IAT; sin compilar
  ni ejecutar (sin MinGW/Wine en sandbox; requiere PC Windows).
- Sync: TESTING E-2/E-2.3, KNOWN I-23/I-01, MODERN FA-04/08/11 +
  M-13, REVERSE Etapa C + avance, ROADMAP método/E-2/Hito R,
  STATE Fase 1 + limitaciones, PATCH, RESEARCH, propuesta
  (§4 estado, §10 puerta). Panel intacto (ningún hito
  completado: E-2 listo ≠ ejecutado; causa I-23 ≠ fix).

## 2026-10-10 — 0.0.0 — M-13: revisión crítica Rev.1 de la propuesta (sin implementar)

- `docs/M13_DECOUPLE_PROPOSAL.md` Rev.1: 8 hallazgos aplicados —
  fronteras por-campo (`0x43DC3A`/`0x53FCB7`), fases A/B (FPS-1
  original inejecutable con throttle intacto), A no acelera
  (drenaje en el exe), tolerancias como placeholders + procedimiento
  k×σ_baseline (Hito R/MAIN-8), Q pared-absoluta aclarada, R-catchup
  como divergencia declarada, ORG-1 + C′ + REV-1/NREG-1/PASO-1
  corregidos, typo Original Mode. S-01…S-06 clasificados (P/N);
  nada refutado. Veredicto: MANTENER con REVISIÓN.
  M-13 PROPOSED; FA/M/hitos intactos; E-2 bloqueado; panel intacto.

## 2026-10-10 — 0.0.0 — M-13: propuesta de prototipo reversible sim/render (diseño)

- Nuevo `docs/M13_DECOUPLE_PROPOSAL.md` (diseño, NO implementación):
  arquitectura SIM/PRESENT + acumulador de paso fijo, pseudocódigo,
  puntos de intervención A/B/C (método lo decide Fase 3), riesgos
  R-* + supuestos S-*, criterios REV/SIM/FPS/PASO/NREG (umbrales
  propuestos), puerta de autorización (E-2 + Hito K + Fase 3).
  M-13 sigue PROPOSED; FA/M intactos; causa del límite de FPS no
  resuelta. Panel intacto (documentar no completa hitos MAIN).

## 2026-10-10 — 0.0.0 — Línea A+B: refs Glide/desacoplado + lectores `+0x10C` y 75 Hz

- Línea B (estático solo-lectura, exe EU md5 `692b1282…`, copia
  eliminada; nada ejecutado/modificado): lectores del cociente
  `+0x10C` LOCALIZADOS — R1 `0x53FED4` (snapshot→`ds:0x5ACBD4`,
  1W/1R) y R2 `0x451A0C` (delta→tabla `0x5ACBE5`→WORD; flags modo
  `ds:0x5ACBC4`∈{0,3} + `ds:0x5ACBC3`); alias `ds:0x551640`=
  `ds:0x551644`→`0x648900`; snapshot/restore contador `ds:0x65CD30`;
  `grSstWinOpen` juego=512x384@75 Hz pedidos (`0x46324D`), cine=
  640x480@60 Hz (`0x537EA6`); 25 FPS reforzado (sigue hipótesis:
  tasa real runtime sin confirmar); boceto desacoplado (diseño
  solo, sin implementar). Detalle: REVERSE_ENGINEERING Etapa C.
- Línea A (solo lectura; clones eliminados): catálogo S-26 —
  sezero/glide (impl `grBufferSwap`/`grBufferNumPending` minada),
  soul-re (Gex-2-engine: `GAMELOOP_DoTimeProcess` + `decoupleGame`,
  PSX/VBL), TRX (patrón paso-fijo+interpolación, ref. diseño),
  KAIN2-PC (débil: D3D, sin Glide, 2023). Detalle: RESEARCH S-26.
- FA-04 sigue PARCIAL (semántica tabla + dinámica E-2 pendientes).
  Panel intacto (sin hito).

## 2026-10-10 — 0.0.0 — Atribución estática `0x418FF8`: fn2 «tvend», ajena a M-13

- Puntero-dato único en `.data` `0x553378` (slot fn2 del registro
  «tvend» `@0x55336C`, tabla 60 registros `@0x5531C8`–`@0x553678`
  {idA, idB, fn1, fn2, fn3}: pantallas/entidades × mundos).
  Barrido byte-exacto: 0 refs estáticas a la tabla; 0 llamadores
  directos (108/110 fns; 2 markey solo intra-familia lvltv→markey,
  raíz huérfana). `0x418FF8` escribe `[obj+0x10C]=0x10`
  (`0x4190D5`): campos propios, NO el cociente/contador timing.
  Impacto M-13: ninguno. Falta cobertura dinámica (E-2/I-23).
  REVERSE_ENGINEERING fila + FA-04 actualizados. Nada ejecutado/
  modificado; copia eliminada. Panel intacto (sin hito).

## 2026-10-10 — 0.0.0 — E-3 documentado + M-13 estático: flujo temporal gameplay

- TESTING E-3 (nuevo, ejecutado solo-lectura 2026-10-10): superficie
  timing EU (Sleep+GetTickCount, t0 `0x43B36D`, `elapsed/16.666`
  `0x43E5DD`); candidatos (1) `push 0xFA0` `0x4637E4`, (2) Sleep idle;
  contexto candidato 1 = hold splash `\movie\logo` (estado 6, solo
  sub==1; tabla 12 slots `0x551668` vs 9 pares en disco — pregunta
  abierta); veredicto APTO reversible sin beneficio M-13. Nada
  ejecutado/modificado; copia eliminada; E-2 sigue BLOQUEADO (I-23).
- REVERSE_ENGINEERING Etapa C timing [~]: flujo temporal gameplay
  (init estado 3 → pre/update/sello → swap(3) → housekeeping, serie
  sin separación sim/render); throttle = `swap_interval` Glide
  (spec pública; 75/3 = 25 FPS, consistente Hito 1 — inferencia);
  consumidores mapeados (flag 1er frame sin lectores; `0x473238`
  ignora contador; `+0x10C` sin lectores en ámbito; `0x418FF8` no
  atribuible); sabido vs no-demostrable + próximo paso mínimo.
  FA-04 → PARCIAL; I-01 + M-13 apuntan. Panel intacto (acoplamiento
  sin prueba dinámica; A3.3 sigue 0/21).

## 2026-10-10 — 0.0.0 — Simplificación estratégica: ruta R→K→P + Olas 1–3

- ROADMAP: Fases 4–14 fusionadas en Hitos R/K/P + Olas 1–3 (IDs
  M/FA intactos); Hito P = MAIN 100% (sin tercera definición);
  Fase 1 recortada a restante real; catálogos F/S cerrados salvo
  necesidad de implementación.
- Clasificación: RUTA = FA-01/04/05/11, I-01/I-23, F-05/F-03/F-02,
  M-13+enablers; resto POSPUESTO (sin IDs eliminados). RE
  priorizado (B→C mínima→D mínima); Etapa A variantes [~] (E-1).
- Sync: E-2 BLOQUEADO (I-23) en STATE/TESTING/RESEARCH; P-F03
  cabecera factual; COMPAT matriz futura comprimida; README ruta
  + objetivos 3–4 acotados. Panel intacto (reorganización, §6).

## 2026-10-10 — 0.0.0 — I-23: dependencia de ruta fija (método E-2 bloqueado)

- Observación reproducida 3/3 (principal + copias A/B): renombrar
  `C:\GEX_REZ\GEX2\` impide arrancar (`Cannot load font
  C:\GEX_REZ\GEX2\font3.dff`). Método sin registro invalidado;
  mediciones no iniciadas. Origen sin investigar (orden expresa).
  KNOWN I-23 (nuevo) + TESTING E-2.2 + mapeo FA-11. Panel intacto.

## 2026-10-10 — 0.0.0 — E-2.1: copias A/B verificadas (hashes match)

- `E2_A_ORIGINAL` = original (md5), `E2_B_F03EU` = F-03 EU (sha256);
  principal conservada. Lanzamiento sin registro (carpeta propia).
  Mediciones pendientes de orden de inicio. Panel intacto.

## 2026-10-10 — 0.0.0 — E-2.0: backup original verificado (falsa discrepancia MD5)

- Paste mantenedor = pin repo = metadato Drive
  (`692b1282…`, match exacto case-insensitive): la diferencia era
  solo mayúsculas de PowerShell; docs correctos, sin corrección.
  Fuente de copia A verificada. TESTING E-2.0. Panel intacto.

## 2026-10-09 — 0.0.0 — E-1.1: identidad exe probado CONFIRMADA (sha256 match)

- Paste mantenedor (`C:\GEX_REZ\GEX2\GEX3D.EXE`) =
  `7a9b6851…a3e8254`, match exacto vs bytes Drive EU (re-descarga
  + hash local + metadato; md5 `3198350e…` coherente). P-F03/B
  atribuido al exe analizado (build 1998-06-29); E-1 completo.
  Pin sha256 completo registrado en TESTING E-1.1. Staging
  eliminado; repo sin binarios. E-2 pendiente. Panel intacto.

## 2026-10-09 — 0.0.0 — E-1: F03-EU es build junio-1998, sin nueva API timing

- Descargas Drive→Arena verificadas (md5+sha256 3/3); análisis
  solo-lectura local (pefile+binutils); copias eliminadas después.
- RESEARCH S-26 (nuevo): huellas PE (links 05-18/06-29/06-24, x86
  GUI, mismo CRT); EU = build distinta (82.9% bytes ≠, 1/381
  bloques =); imports sin nueva API timing (+mciGetErrorStringA;
  Sleep+GetTickCount únicos); path `_demo` EU/US; EU≠US (67.3% ≠).
- PATCH F-03 + TESTING E-1 (ejecutado; scripts superados pero
  conservados) + KNOWN I-01 (mecanismo interno/de build) + STATE.
  E-1.1 (hash exe probado) pendiente mantenedor; E-2 pendiente.
  Panel intacto (sin hito).

## 2026-10-09 — 0.0.0 — P-F03/B: binarios F-03 + rama B (42–44 + speedup)

- RESEARCH S-25 (nuevo): `F-03_FPS_Limiter/` en Drive (EU 1559040 B
  md5 3198350e… mtime 1998-07-08; US 1665536 B md5 5dde47d8… mtime
  1998-06-24; TXT 682 B = README OP salvo URL corta); original EU
  re-confirmado; bytes inaccesibles desde aquí (egress bloqueado).
- PATCH F-03: tabla de binarios + TXT + stamps de era; US ≠ exe 2013
  S-14 (mismo tamaño, distinto hash). P-F03/B parcial: 42–44 FPS
  menú+juego, cinemáticas ≈15, sim acelerada, audio normal (rama
  «FPS↑» inesperada → E-1.1 identidad).
- TESTING: sección P-F03/B + E-1 (captura estática solo-lectura con
  scripts) + E-2 (micro-test sim-vs-FPS) pendientes de autorización;
  matriz actualizada. KNOWN I-01 (OBSERVADO, mecanismo pendiente) +
  I-02 (dato 42–44), STATE, COMPAT Notas. Panel intacto (sin hito).

## 2026-10-09 — 0.0.0 — Lote tools: H-01/H-02 + R-03/R-04 + upscale (S-24)

- RESEARCH S-24 (nuevo, hub): gex2-tools (extractor .VFX→PNG PC, sin
  licencia), Gex3DViewer (visor/exportador PC WIP, sin licencia),
  GexPSXLZSS (LZSS BIGFILE PS1, sin licencia), unLoKable (suite
  audio CD, MIT; 0 correspondencias PC EU) + upscale Mega «Screen
  Titles» (nada verificable: HTTP 500). API GitHub, sin clonar/
  compilar/ejecutar/descargar.
- PATCH_ANALYSIS: nueva categoría H-xx (herramienta específica Gex
  PC) con H-01/H-02 + R-03/R-04 (referencias secundarias) + filas
  de relación + taxonomía. M-23: candidatos externos anotados.
- CREDITS: SK83RJOSH + SalsaGal; MatBourgon ampliado. Panel intacto
  (investigación, sin hitos).

## 2026-10-09 — 0.0.0 — P-F03: prueba A/B del limitador F-03 preparada

- Enlace exacto verificado: PCGW «3DFX FPS Limiter EU/US patch» →
  tgames t12190 (URL legacy + canónica www, `patch-patch` vivo);
  adjunto SOLO-miembros (sin URL directa pública; registro gratuito
  requerido para revelarlo).
- F-03: README del autor archivado verbatim; negativos: sin nº de
  versión, sin nombre/tamaño de archivo, sin hashes, sin FPS objetivo.
  Detector `voiceuk`=EU cruzado con inventario §§6/8 (match nominal);
  entorno Win11+nGlide fuera del alcance declarado (solo 3Dfx HW +
  Win98–XP); evidencia funcional publicada: nula.
- TESTING.md: protocolo P-F03 (A/B ABA en copias aisladas, métricas,
  criterios pre-declarados, reversión) PENDIENTE DE AUTORIZACIÓN
  (A1–A4). Sin descargas, sin ejecución, sin modificaciones.
  Panel intacto (plan, sin hitos).

## 2026-10-09 — 0.0.0 — Lote 2: parches oficiales/comunitarios, FPS, música, mandos

- RESEARCH S-23 (nuevo): parche oficial 3dfx + Generic Update (3dfxzone
  1.18 MB, readme v1.0 verbatim ©1999 Midway/US; añade `gex23Dfx.exe`
  a base `gex23d`; genérico sin describir; `GX2PATCH.ZIP` no localizado;
  sin changelog/FPS/EU). S-01/S-07/S-08 ampliados (PCGW re-verificado,
  MAW extras 609/570 KB, AF liens: espejo F-01 356 Ko + packs).
- PATCH_ANALYSIS: F-13 oficial (nuevo) + F-01 (espejo AF) + F-03 (FPS
  objetivo sin declarar; limitador≠desbloqueador) + F-10 (verbatims
  Xbox/música; cerrado, no reutilizable). Taxonomía F-xx ampliada.
- KNOWN I-01/I-02/I-19 + PROJECT_STATE actualizados. 25 FPS: ningún cap
  documentado coincide (30/24); PAL⇒25 no establecido; R-1 vigente.
  Panel intacto (investigación, sin hitos).

## 2026-10-09 — 0.0.0 — 9ª sesión: atribución nGlide 2.10 CONFIRMADA + panel v3.3

- TESTING 9ª (equipo real, solo lectura): imports PE x86 = 6 DLL sistema
  (KERNEL32/USER32/GDI32/ADVAPI32/WINMM/VERSION), sin D3D/DXGI/Vulkan
  estáticos ⇒ por sí solos compatibles con época Y wrapper dinámico
  (dicotomía 8ª corregida: era falsa).
- Atribución CONFIRMADA por cadena de hashes (sin ejecutar nada):
  instalador S-14 = oficial nGlide 2.10 (md5/SHA1 de Zeus, t=557) =
  muestra Falcon 3cfcd03a…; su drop `glide2x.dll` = SHA-256 7cbd… =
  DLL cargada ⇒ Gex usa nGlide 2.10 (backend dinámico). RESEARCH S-22:
  autenticidad + tabla de drops + resolución escenarios.
- Panel v3.3 (solo datos): A2.3 «Atribución origen (L)» ✓ → A2 37→46%
  (25/54), global 18→20% (57/285); MAIN 40% intacto; corrige cabecera
  §4.4 (stale 15/54). Fila v3.3→(este commit): registrar en siguiente.

## 2026-10-09 — 0.0.0 — 8ª sesión: registro sin fechas, payload pendiente, imports propuestos

- TESTING 8ª (equipo real + Drive, solo lectura): nGlide 2.10 en
  registro pero InstallDate/InstallLocation vacíos ⇒ vía temporal
  inconclusa (sin inferencias); configurador 2.10 triple-confirmado;
  DLL 7cbd reconfirmada (continuidad ×3).
- Instalador `nGlide210_setup.exe` fijado en `S-14_extracted` (3301587 B,
  md5/SHA-256; coherente con corpus) pero payload sin inspeccionar:
  bloqueo técnico (binario no transitable, sandbox sin salida a Drive),
  no de permiso; .exe ni ejecutado ni descargado. Sin referencia
  pública del payload (negativo documentado).
- Siguiente paso único: lista de imports PE de la DLL (script PowerShell
  incluido, algoritmo validado; sin instalar nada). Atribución a nGlide
  2.10: SIN DEMOSTRAR. Panel intacto.

## 2026-10-09 — 0.0.0 — 7ª sesión: hash idéntico, sin sustitución, vendedor indeterminado

- TESTING 7ª (equipo real, solo lectura): `SysWOW64\glide2x.dll` =
  mismo hash 7cbd… (1630208 B) ⇒ sin sustitución; relectura 2.61.00.0658
  (disputa 3ª cerrada: error transcripción); ProductName ES vs EN en
  igual hash (recurso multilingüe probable); `nglide_config.exe` existe
  ⇒ instalador ejecutado, uso por Gex sin demostrar.
- RESEARCH S-22: resolución + docs oficiales nGlide (backend D3D+Vulkan,
  Glide 2.60, sin política de sobrescritura documentada; foro: aparece
  en Agregar/quitar) + 3 escenarios de coexistencia. d3d9 sigue sin
  probar nada.
- Siguiente paso mínimo: registro desinstalación (versión/fecha) +
  versión y lectura del configurador. Sin cambios de hitos/%: panel
  intacto («¿nGlide?» sigue vigente).

## 2026-10-09 — 0.0.0 — Atribución Glide: nGlide sin demostrar + corpus S-22

- Análisis de atribución (sin nuevos datos del PC): demostrado = carga
  de `SysWOW64\glide2x.dll` + wrapper=SÍ; que la implementación sea
  nGlide 2.10 NO demostrado (metadatos 3dfx vs marca nGlide; 3 nulos;
  sustitución sin confirmar). Panel: «nGlide probable» → «¿nGlide?»
  (solo redacción, sin cifras; wrapper=SÍ intacto).
- RESEARCH S-22 (corpus público): hash antiguo 7cbd… circula (dllme,
  1.6 MB, MD5 f59d9780…); dllme lo etiqueta 2.61.00.0658 ⇒ cadena
  2.60.0.658 de 3ª sesión EN DISPUTA; nGlide v1.x declara ProductName
  propio. TESTING 3ª/5ª anotados.
- Siguiente paso mínimo (solo lectura, PC): hash+metadatos completos
  de la DLL + presencia/lectura del configurador. Sin slow-mo/FPS aún.

## 2026-10-09 — 0.0.0 — 6ª sesión: A/B VSync nulo + intros ≈15 (estimado)

- TESTING 6ª sesión (equipo real): A/B VSync On/Off ⇒ ≈25 FPS en ambos
  (método: Steam; cierra laguna de método); VSync solo DESCARTADO como
  causa; líder: límite propio del juego. Intros percibidas ~15 FPS
  (NO medido; formato/frecuencia sin confirmar). Parpadeo = 1 cambio
  de modo. Sin tocar originales ni registro.
- KNOWN (I-01/I-02/I-14/I-22), COMPATIBILITY, PROJECT_STATE actualizados;
  §4.10/Trazabilidad A2.2 al día; registro v3.2→6114f0b. RESEARCH sin
  cambios (revisado; sin fuentes nuevas).
- Sin cambios de hitos ni porcentajes (resultado nulo, completa ningún
  criterio): dashboard intacto (v3.2).

## 2026-10-09 — 0.0.0 — 5ª sesión: nGlide 2.10 + 25 FPS + wrapper=SÍ (A2.2)

- TESTING 5ª sesión (equipo real): nGlide 2.10 instalado; `glide2x`
  2.61.00.0658 (ficha nueva; hash/tamaño pendientes); ≈25 FPS estables
  (método pendiente); Aspect/Refresh nulos; un parpadeo; RTSS/Steam
  inservibles (limitación de tooling). Experimento A/B VSync definido,
  pendiente de autorización.
- Wrapper=SÍ (A2.2 L ✓): instalador nGlide→SysWOW64 + render en HW
  moderno sin 3dfx. A2 28→37%, global 16→18% (52/285). MAIN intacto
  (40%). API efectiva y procedencia siguen abiertas.
- Panel v3.2 (solo datos): SVG + MD sincronizados; siguiente hito =
  causa 25 FPS (FA-05, I-01/I-02). KNOWN (I-01/I-02/I-21/I-22),
  COMPATIBILITY, PROJECT_STATE y RESEARCH (S-02) actualizados.

## 2026-10-09 — 0.0.0 — Regla permanente: dashboard solo con avance real

- `docs/PROJECT_STATUS.md` §6: nueva regla permanente — el SVG es el
  dashboard oficial; solo se actualiza ante cambio real y verificable
  (hito con evidencia, % justificado, cambio de estado, nuevo hito,
  corrección); sin avance real, SVG y MD intactos, sin commits vacíos;
  diseño aprobado conservado; informe por sesión.
- `docs/REPO_SYNC_RULE.md`: referencia cruzada a la regla.
- SVG intacto (sigue v3.1); sin cambios de cifras, metodología o diseño.

## 2026-10-09 — 0.0.0 — Panel de progreso v3.1: ajustes visuales

- `docs/PROJECT_STATUS.svg` v3.1 (760×700, misma tarjeta): «40%» MAIN
  reducido de 124 a 68px; secundarias ampliadas (resumen 12px, áreas
  26/9.5/9px, stats 15px, pie 9.5px); banda ámbar 100% sustituida por
  la etiqueta `VERSIÓN JUGABLE` bajo la barra; espacios redistribuidos.
  Composición, colores y estructura intactos. Cifras idénticas.
- `docs/PROJECT_STATUS.md` v3.1: geometría y registro actualizados; el
  significado completo del 100% MAIN (leyenda + avisos) permanece en
  §4.9. Metodología, evidencias y alcance sin cambios (ref `e9e846c`).
- README sin cambios. Sin preview raster (entorno sin renderizador;
  auditoría estática).

## 2026-10-09 — 0.0.0 — Panel de progreso v3.0: rediseño visual tarjeta

- `docs/PROJECT_STATUS.svg` v3.0 (760×700): tarjeta negra/morada estilo
  HUD retro — «40%» gigante + barra MAIN ancha con ticks 25/50/75,
  6/12 hitos y 14/35 puntos; leyenda ámbar `100% MAIN = PRIMERA
  VERSIÓN JUGABLE PREPARADA PARA PRUEBAS` + aviso de no distribución;
  global 16% secundario; 5 mini-tarjetas de área; resumen compacto.
  Sin scripts ni remotos; decoración mínima.
- `docs/PROJECT_STATUS.md` v3.0: misma metodología, cifras, evidencias
  y alcance MAIN (ref `e9e846c`); solo cambian resumen literal,
  geometría documentada y registro. Cifras idénticas a v2.0.
- README sin cambios (enlace y descripción siguen vigentes).
  Sin preview raster (entorno sin renderizador; auditoría estática).

## 2026-10-09 — 0.0.0 — Panel de progreso v2.0: héroe MAIN + jerarquía mejorada

- `docs/PROJECT_STATUS.svg` v2.0 (760×960): héroe MAIN (40%, 14/35 PTS,
  6/12 hitos, EN CURSO) con leyenda `100% MAIN = PRIMERA VERSIÓN JUGABLE
  PREPARADA PARA PRUEBAS` y aviso de no distribución; global 16% (47/285)
  independiente; 5 áreas con nombres claros y estados texto+color; resumen
  CONSEGUIDO/EN INVESTIGACIÓN/SIGUIENTE HITO. Estilo HUD negro/morado.
- `docs/PROJECT_STATUS.md` v2.0: alcance MAIN verificable en 12 hitos
  (6 completados con evidencia: inventario/tests/Hito 1/Fase 1; 6
  pendientes incl. API gráfica y wrapper); distingue jugable vs
  distribuible vs completo; procedimiento conjunto SVG+MD.
- README: aclaración MAIN vs global. Cifras de áreas sin cambios.
  Sin preview raster (entorno sin renderizador; auditoría estática).

## 2026-10-09 — 0.0.0 — Panel SVG de progreso + metodología (PROJECT_STATUS v1.0)

- Nuevo `docs/PROJECT_STATUS.svg`: panel de diagnóstico retrofuturista
  (morado/negro, estático, sin scripts ni remotos) con 5 áreas + global.
- Nuevo `docs/PROJECT_STATUS.md`: fuente de verdad — metodología de hitos
  ponderados (XS1/S2/M3/L5/XL8), estados verificables, fórmulas, exclusiones
  N/A documentadas, evidencias y procedimiento de actualización Arena.
- Cifras iniciales v1.0: global 16% (47/285 PTS); A1 32%, A2 28%, A3 0%,
  A4 0%, A5 33%. Trazables a FA/M-xx, TESTING, KNOWN_ISSUES y BUILD/LICENSE.
- README: sección «Panel de progreso» + fila en la tabla de docs.
- Fase 1 en curso; RE no iniciada; exe intacto. Sin preview raster (entorno
  sin renderizador SVG; validado por auditoría estática).

## 2026-10-09 — 0.0.0 — Hito 1 (4ª sesión): fecha/firma Glide + plan de publicación

- TESTING.md: nota 4ª sesión — producto exacto `Glide® for Voodoo Banshee®`
  (precisión de transcripción); fecha mostrada en Propiedades 2019-09-15
  00:54:48 (campo sin identificar, no atribuir); sin pestaña/firma digital
  visible. Fecha + ausencia de firma NO determinan procedencia; hash
  identifica contenido, no autenticidad; carga = pista, no prueba de API
  ni descarte de wrapper. I-21/I-22 sin causa definitiva.
- Publicación futura (planificación, nada implementado): BUILD.md (requisitos
  de distribución BYO + validación local), LICENSE.md §5 (evaluación
  jurídica por método, 6 métodos, pendiente) y §6 (lista de revisión:
  licencias, avisos, dependencias, historial, privacidad, docs), ROADMAP
  (6 hitos transversales, todos pendientes).
- README: resumen inglés corregido (`*Gex: Enter the Gecko* — REZengineered`).
- RESEARCH/COMPAT/KNOWN/GOALS sincronizados (4ª sesión). Fase 1 en curso;
  RE no iniciada; exe intacto. Sin autorización legal declarada.

## 2026-10-09 — 0.0.0 — Hito 1 (3ª sesión): ficha Glide Banshee 2.60 + SHA-256

- TESTING.md: nota 3ª sesión — `SysWOW64\glide2x.dll` (1630208 B):
  `3Dfx Interactive, Inc. Glide DLL`, producto `Glide para Voodoo
  Banshee` (®), versión 2.60.0.658, SHA-256 registrado; `3dfxSpl2.dll`
  (`3dfx Splash Screen` 1.0.0.4). Ambas cargadas en `GEX3D.EXE`.
  Interpretación: compatible con Glide 3dfx, pero NO prueba
  originalidad ni descarta wrapper. I-21/I-22 sin causa atribuida.
- RESEARCH (S-21 corroborado + hipótesis de origen), COMPATIBILITY
  (ficha en nota Hito 1), KNOWN_ISSUES (I-21: ficha registrada),
  GOALS (FA-02/FA-03: ficha sí, origen/wrapper UNKNOWN).
- Siguiente paso único: fechas + firmas digitales del fichero (solo
  lectura). Fase 1 en curso, Fase 2 sin iniciar, RE no iniciado.

## 2026-10-09 — 0.0.0 — Hito 1 (2ª sesión): glide2x cargada + M-26 intro omisible

- TESTING.md: nota 2ª sesión — `glide2x.dll` CARGADA (SYSTEM32 = físico
  SysWOW64 por redirección WOW64) + `3dfxSpl2.dll` (splash Glide 2.x,
  S-21); ruta Glide = hipótesis muy sólida, identidad pendiente.
  `ddraw/d3d9/dxgi/atidx9loader32` presentes, rol UNKNOWN (no implican
  renderer); GPU AMD probable. Puntos 7–8 actualizados; parpadeos
  multimonitor en transiciones de vídeo registrados.
- KNOWN_ISSUES: I-21/I-22 observados 2 veces (sin nuevos IDs); mapeo
  ampliado (I-21→M-04, I-22→M-05/M-19).
- M-26 Skippable Intro (PROPOSED, P3, sin fase): GOALS (Nivel 11) +
  mapa, ROADMAP (M-01…M-26 + bullet Fase 1), PROJECT_STATE, RE (FMV),
  RESEARCH (S-21 splash), CREDITS (r/3dfx).
- Siguiente paso único: ficha del fichero Glide físico (Detalles +
  `certutil` SHA256, solo lectura). Fase 1 en curso, Fase 2 sin iniciar.

## 2026-10-09 — 0.0.0 — Hito 1: enumeración 64-bit vacía = artefacto WOW64 (nota + paso)

- TESTING.md: nota post-Hito 1 — `(Get-Process GEX3D).Modules` desde
  PowerShell 64-bit solo muestra la capa WOW64 (7 módulos: exe + `ntdll`
  + `wow64*`); el filtro gráfico vacío es un artefacto de medida y no
  dice nada sobre el renderer (ni a favor ni en contra de Glide).
- Siguiente paso único: repetir la consulta desde PowerShell de 32-bit
  (`SysWOW64...powershell.exe`), solo lectura, sin instalar nada.
- Sin cambios de fase ni de estados FA: FA-02/FA-03 siguen UNKNOWN.
  Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Hito 1: primera prueba funcional EU en Win11 (F-05 sin F-01)

- TESTING.md: Hito 1 ejecutado en PC del mantenedor (Win11 64-bit):
  instalación manual `C:\GEX_REZ\GEX2` (532 ficheros, MD5 del exe
  verificado), `.reg` (`Version`=2, `InstallDir`, `CDDriveName`=`D`),
  imagen CloneCD montada como `D:`; arranca, entra en nivel, movimiento +
  música/SFX OK. Exe inalterado (sin parches); nGlide no instalado
  durante la prueba. Renderer UNKNOWN (splash 3DFX no probatorio;
  `glide*.dll` en SysWOW64 de procedencia desconocida, carga sin confirmar).
- KNOWN_ISSUES: I-15/I-16 parcialmente confirmados (1 config); I-11 no
  reproducido aquí; I-06 intacto + nuevos I-21 (bandas/HUD) e I-22
  (iconos multimonitor) como observaciones propias; cabecera actualizada.
- COMPATIBILITY (fila Win10/11 + nota Hito 1), PATCH_ANALYSIS (F-05
  ejecutado sin F-01), RESEARCH (S-01), MODERNIZATION_GOALS (nota runtime
  FA, estados sin cambiar), TOOLKIT (Windows parcial), README /
  PROJECT_STATE / ROADMAP (testing: Hito 1) sincronizados.
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Siguiente paso:
  identificar la DLL gráfica cargada con PowerShell (solo lectura).

## 2026-10-09 — 0.0.0 — Evaluación de entornos Windows (sin probar)

- TESTING.md: alternativas A (Win moderno+wrapper), B (VM parcial), C
  (HW época referencia) + checklist 14 requisitos (IMP/REC/ESP) + MVP
  (A) y referencia ideal (C). Ninguna config declarada compatible.
- Sync: TOOLKIT §5.2 [~], PROJECT_STATE/ROADMAP (máquina pendiente,
  faltan datos HW). Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Protocolo de pruebas + matriz FA (Windows, sin ejecutar)

- TESTING.md: protocolo reproducible (entorno, hashes, instalación,
  pasos/veredicto, artefactos, fallos, custodia, limitaciones
  moderno-vs-época) + matriz priorizada FA-04…FA-15 (14 pruebas, sin
  nuevos IDs). Base EU-Glide; US/D3D/parches solo comparadores.
- ROADMAP `M-01…M-24` revisado: correcto como está (describe la Fase 0
  cerrada 2026-10-08; M-25 vive en Fase 1). Sin cambios; M-25 PROPOSED.
- Sync: PROJECT_STATE/ROADMAP (testing [~], máquina pendiente).
- Fase 1 en curso, Fase 2 sin iniciar. Nada ejecutado.

## 2026-10-09 — 0.0.0 — Matriz documental FA-01…FA-15 (sin RE)

- MODERNIZATION_GOALS: nueva sección de cobertura documental (15 filas:
  conocido, evidencia+fuente, tipo, relevancia EU, falta, siguiente
  acción, estado). 4 BASE DOCUMENTADA (FA-01/02/03/09), 8 PARCIAL, 3
  PENDIENTE (FA-04/07/15). Tabla FA intacta (todo TO INVESTIGATE).
- RESEARCH: patrón de guardado `GEX2%d%d%c.GEX` (estático) aflorado a la
  lista verificada (ubicación UNKNOWN). Sin nuevos IDs, sin binarios.
- Sync: PROJECT_STATE/ROADMAP (FA documental [~], dinámica → Fase 2+).
- Fase 1 en curso (no cerrada), Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Revisión cruzada F-01…F-12 + S-14 (solo documental)

- PATCH_ANALYSIS: nueva sección de revisión cruzada (tabla de seguimiento
  F-01…F-12, duplicidades, análisis S-14 en 5 puntos, prioridades
  documentales). Sin fusiones, sin nuevos IDs, sin binarios.
- Correcciones mínimas: RESEARCH S-02 («exe capeado» → cita motivadora,
  veredicto F-04); PATCH F-01 (ambigüedad `voice`/`voiceuk` explicitada).
  F-05/F-06/F-07/F-12 ya cumplían (sin cambios); historial intacto.
- Sync: PROJECT_STATE/ROADMAP (revisión cruzada anotada en fixes [~]).
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Duplicidad
  confirmada: ninguna; probable: F-04↔F-02 (descriptiva, pendiente binario).

## 2026-10-09 — 0.0.0 — S-14: inventario nominal + README leído (carpeta extraída)

- `S-14_extracted` (subida por el mantenedor): 22 entradas inventariadas
  solo por nombre+metadatos (README, Game Files con exe 2013 + 15 WAV, 3
  instaladores); README.txt leído íntegro (montar Gex3DD3D.ccd, copiar
  exe+music, nGlide, _inmm DirectShow, sin disco). Nada ejecutado.
- Clasificación: inventario+README CONFIRMADOS a nivel documental;
  procedimiento NTSC-D3D DECLARADO (no verificado); binarios/WAV/EU
  aplicabilidad DESCONOCIDOS. Sin nuevos IDs.
- Sync: RESEARCH (S-14), PATCH_ANALYSIS (F-09), PROJECT_STATE/ROADMAP
  (S-14 [x], binarios→Fase 2), KNOWN_ISSUES (I-11 _inmm), COMPATIBILITY
  (fila D3D), REVERSE_ENGINEERING (Etapa D: exe 2013 candidato).
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Historial intacto.

## 2026-10-09 — 0.0.0 — S-14: inventario interno bloqueado (solo metadatos)

- Acceso + hashes re-verificados (2ª comprobación, Drive solo lectura):
  ambas copias intactas (MD5/SHA-256 coinciden, no trashed, sin modificar).
- Índice ZIP + README: BLOQUEADOS en este entorno con evidencia —
  `read_file_text` falla (25 MB cap vs 411301184 bytes) y `download_file`
  solo ofrece bytes completos o URL no consumible; copia completa al
  workspace prohibida. Sin descarga, extracción ni ejecución.
- Sync: RESEARCH (S-14: re-verificación + bloqueo), PATCH_ANALYSIS (F-09),
  PROJECT_STATE/ROADMAP (S-14 [~] con bloqueo). Contenido sigue
  DESCONOCIDO; Fase 1 en curso, Fase 2 sin iniciar.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Precisión F-12 + inspección documental S-14

- Bloque A: F-12 ya no presenta el menú debug como confirmado
  (PATCH_ANALYSIS: «Qué es (DECLARADO POR LA FUENTE)» + «Valoración» con
  atribución explícita; COMPATIBILITY y REVERSE_ENGINEERING suavizados
  igual). A2 verificado sin cambios: Last updated 2026-10-09, M-25
  PROPOSED, Fase 1 en curso, Fase 2 sin iniciar.
- Bloque B: S-14 con metadatos Drive autorizados (solo lectura): 411301184
  bytes, MD5/SHA-256 registrados, copia `research/` bit-idéntica;
  discrepancia 411/~393 resuelta con evidencia (unidades). Página releída:
  cita corregida («Includes…»), README no publicado por separado.
  Contenido sigue DESCONOCIDO; S-14 sigue investigación pendiente.
- Sync: RESEARCH (S-14 reestructurado + pendiente), PATCH_ANALYSIS (F-12,
  F-09), PROJECT_STATE/ROADMAP (S-14 [~]). Sin binarios, sin descargas al
  sandbox, sin ejecución, sin fase avanzada.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Nueva propuesta M-25: selección de voces UK/USA

- MODERNIZATION_GOALS: +M-25 Voice Pack Selection (UK/USA), PROPOSED, P3
  provisional, Nivel 10 (Audio/Voces), sin fase (post-11, TBD); refs
  cruzadas M-20 (Audio) y M-24 (textos, no confundir); mapa actualizado.
- RESEARCH: nueva S-20 (reparto Gould/Phillips: Wikipedia verificada +
  VGFacts/BTVA; general, no PC-específica) + UNKNOWNs base de M-25.
- Sync: ROADMAP (M-01…M-25, item Fase 1), PROJECT_STATE (ampliación +
  Last updated), REVERSE_ENGINEERING (Etapa C audio/voces), COMPATIBILITY
  (nota variante regional), PATCH_ANALYSIS (F-01/F-03 → M-25), CREDITS.
- Sin implementación, sin binarios tocados, sin fase avanzada.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Lote 1: investigación documental de fixes (F-01…F-12)

- PATCH_ANALYSIS: F-01…F-09 re-investigados en sus fuentes (hilos leídos
  íntegros, citas verbatim) + F-10 (music handler), F-11 (NO-CD D3D) y F-12
  (debug tool, confirma menú debug en build D3D) catalogados. F-04 queda
  como probable duplicado de F-02 (sin pieza separada); F-06 no validado
  (hilo VOGONS no resuelto); t12123 (repack full-game) fuera de alcance.
- RESEARCH: S-01/S-02/S-03/S-04/S-05/S-08/S-09/S-14 re-verificadas (S-02
  absorbe el hilo Zeus «No music in Gex 2»: winmm/MCI + `_inmm.dll`).
- KNOWN_ISSUES: notas Lote 1 en I-01/I-11/I-12/I-14/I-16 (I-14 debilitada a
  hipótesis débil; I-11/I-12 con nuevos reportes adyacentes).
- COMPATIBILITY (fila «versión D3D»), REVERSE_ENGINEERING (Etapa D:
  F-01…F-12), PROJECT_STATE/ROADMAP (Fase 1 [~] en fixes) sincronizados.
- Historial preservado: entradas anteriores intactas.

## 2026-10-08 — 0.0.0 — Regla permanente de sincronización + revisión global

- Nueva regla permanente `docs/REPO_SYNC_RULE.md`: cada cambio relevante
  exige revisión global (README/ROADMAP/PROJECT_STATE/CHANGELOG/objetivos/
  issues/RE/compatibilidad/testing), corrección de contradicciones y
  checklist final antes del commit. Sin avances de fase unilaterales.
- Primera aplicación: Fase 0 cerrada, Fase 1 en curso en README, ROADMAP
  y PROJECT_STATE (antes decían Fase 0); ROADMAP Fase 1 con inventario
  completado y entorno/S-14 pendientes; Fase 2 anota el estático
  adelantado sin darse por iniciada.
- Evidencia Fase 1 propagada sin convertir hipótesis en hechos:
  REVERSE_ENGINEERING (Etapa A ✅, B+ bloqueadas), RESEARCH (nueva
  sección «Verificado por el proyecto», originales recibidos),
  KNOWN_ISSUES (notas estáticas en I-03/I-05/I-11/I-15/I-16/I-19, ningún
  issue reproducido), COMPATIBILITY (fila EU confirmada), PATCH_ANALYSIS
  (T-01/T-04: D3D = US/F-01, no EU), MODERNIZATION_GOALS (M-07: sin
  DirectInput en EU; avance parcial FA), TOOLKIT (Etapa A ejecutada),
  TESTING/BUILD/CREDITS (punteros, fase, crédito Mysticore).
- Historial preservado: entradas anteriores intactas.

## 2026-10-08 — 0.0.0 — Fase 1: inventario de artefactos originales (solo docs)

- Nuevo `docs/ORIGINAL_ARTIFACT_INVENTORY.md`: inventario completo de
  `REZengineered/originals/` (1585 ficheros): `disc_image/` (volcado
  CloneCD: hashes, TOC 1+16 pistas, aritmética de sectores) y
  `original_install/` (raíz 19+2, `GEX2/`, `LEVEL/` 72, `AUDIO/` 37,
  `VOICEUK/` 400, `MOVIE/` 18, `DIRECTX/` 85 + `DRIVERS/` 5×189).
- Identificados con evidencia: instalador InstallShield 5.x (stub NE +
  CABs `ISc(` v4), `GEX3D.EXE` (PE32, Glide exclusivo, rama
  `3dfx\release_europe`), edición europea v1.00.000 (`SETUP.INI` +
  `DATA.TAG`), DirectX 4.05.01.1600, CD mixto masterizado ≥1998-05-19.
- Conciliación CD→instalación vía manifiesto de `DATA1.CAB` (100%):
  400/400 voces, 72/72 niveles, 37/37 TADs, 18/18 vídeos.
- Verificación: 30 descargas temporales con SHA-256 30/30, eliminadas
  tras el análisis; el repo no contiene bytes del juego original.
- PROJECT_STATE: checklist y limitaciones actualizados. STOP Fase 1.

## 2026-10-08 — 0.0.0 — Nuevas ideas M-24 (localización) y X-02 (Rust/Bevy)

- M-24 Localization (PROPOSED, P3): estudio futuro de localización de
  textos (English original + Español + otros); solo textos, sin doblaje;
  English = source of truth; Trilogy no reutilizable ni referencia.
- X-02 Posible rewrite Rust/Bevy (DEFERRED, long-term research, prioridad
  muy baja): solo registrar la posibilidad lejana; no es fase de
  implementación; no abandona el engine original.
- ROADMAP Fase 14 menciona X-02 como long-term (sin fase de implementación).
- Nada implementado; prioridades actuales intactas.

## 2026-10-08 — 0.0.0 — Objetivos M-01…M-23 y roadmap por fases (solo docs)

- Creado MODERNIZATION_GOALS.md: FA-01…FA-15, M-01…M-23, X-01 (DEFERRED);
  todo PROPOSED / TO INVESTIGATE; jerarquía de implementación.
- ROADMAP reordenado en fases 0–14 (entender → base → sencillo →
  dependiente → complejo → experimental).
- RESEARCH: áreas de investigación futura (reportado vs hipótesis vs futuro).
- COMPATIBILITY: dimensiones futuras + matriz hardware (vacía) + Linux/Wine
  como secundario (X-01 DEFERRED).
- TESTING: categorías futuras; KNOWN_ISSUES: mapeo a objetivos (sin nuevos
  issues); PATCH_ANALYSIS: mapeo fixes → objetivos; REVERSE_ENGINEERING:
  vínculo con FA/M; README: enlace a goals.
- Nada implementado; ninguna afirmación convertida en hecho.

## 2026-10-08 — 0.0.0 — Taxonomía estricta + Fase 0 estricta + wrappers ronda 2

- README: eliminada la explicación del juego de palabras del nombre (queda
  solo el nombre del proyecto). Logo IA pendiente (no crear gráficos).
- PATCH_ANALYSIS: taxonomía §0 (ORIGINAL / F-fix / T-wrapper / R-referencia)
  + regla explícita: nada catalogado se asume solución final.
- Nuevos T-04 (DDrawCompat), T-05 (DxWnd, upstream por confirmar), T-06
  (WineD3D for Windows); fuente S-19.
- S-14: catalogado como material de investigación; prohibido moverlo al
  sandbox o modificarlo. Originales y análisis binario pospuestos por
  decisión del mantenedor (PROJECT_STATE, RESEARCH).

## 2026-10-08 — 0.0.0 — Referencias del mantenedor + jerarquía + herramientas

- Jerarquía fijada: PC original = fuente de verdad; otras versiones solo
  secundarias; Gex Trilogy fuera de referencias (README, RESEARCH §0).
- RESEARCH: S-11/S-12/S-18 enriquecidos (Gex64Decomp, Trilogy-demoted, REA);
  nuevas S-14 (PC Version Setup Package speedrun), S-15 (guía PS1),
  S-16 (DxWrapper), S-17 (dgVoodoo2).
- PATCH_ANALYSIS: F-09 (setup package) + sección herramientas T-01..T-03.
- Copia privada de S-14 en `Drive → REZengineered/research/` (411 MB,
  pendiente de extraer README e inventario).

## 2026-10-08 — 0.0.0 — Entorno RE sandbox operativo

- `tools/setup-sandbox-re.sh`: venv (`pefile`, `capstone`, `yara-python`) +
  REA CLI 5.0.0 en `/opt/rea-toolkit`. Verificado con self-test.
- `tools/setup-re-env.sh`: script (sin probar) para entorno completo
  Ghidra 12.1.4 + JDK 21 en máquinas con internet completo.
- Verificado e imposible en sandbox: JDK/Ghidra/Rizin/radare2 (documentado
  el porqué en `docs/TOOLKIT.md`); `rea doctor` confirma falta de backend.
- Estructura Drive `REZengineered/{originals,backups,analysis,builds,research}`.
- Actualizados PROJECT_STATE, TOOLKIT, tools/README.

## 2026-10-08 — 0.0.0 — Inicialización Fase 0

- Creación de la estructura inicial del repositorio.
- Documentos base: README, PROJECT_STATE, ROADMAP, RESEARCH, KNOWN_ISSUES,
  PATCH_ANALYSIS, COMPATIBILITY, REVERSE_ENGINEERING, BUILD, TESTING,
  CREDITS, LICENSE, CHANGELOG.
- Guías: `docs/TOOLKIT.md`, `docs/ENGINEERING_LOG_TEMPLATE.md`.
- `.gitignore` orientado a no redistribuir material propietario.
- Recopilación inicial de fuentes públicas (PCGamingWiki, foros, proyectos).
- Mapa inicial de problemas conocidos (todos **sin verificar**).
- Catálogo inicial de fixes comunitarios (todos **sin analizar técnicamente**).
- Sin modificaciones del juego (correcto según la Fase 0).
