# TESTING — Metodología de pruebas

> Estado: **Hito 1 ejecutado (2026-10-09, PC del mantenedor) + resto de
> la matriz pendiente** (Fase 2 sin iniciar). Detalle en «Hito 1» más abajo.

## Principio

Ninguna solución se considera terminada porque "funcione una vez".
Siempre comparar:

**Original vs Fix existente vs REZengineered**

## Qué comprobar (por cambio)

- Estabilidad (arranques repetidos, sesiones largas, cambios de nivel).
- Rendimiento (FPS medidos, frame pacing).
- Comportamiento (velocidad de juego, física, timing).
- Compatibilidad (versiones US/EU/demo, GPUs, wrappers).
- Controles, gráficos, audio, resolución.
- Regresiones (re-ejecutar tests de cambios anteriores).

## Protocolo de registro (futuro)

Cada test registra: fecha, versión del juego (hash — referencia EU en
[docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md)),
OS/GPU/driver, wrapper y versión, fix aplicado, pasos, FPS medidos,
resultado observado y artefactos (logs/vídeo). Matriz de resultados en
[COMPATIBILITY.md](COMPATIBILITY.md).

## Protocolo de prueba reproducible (2026-10-09; base del Hito 1)

Base obligatoria: copia de trabajo de la **EU v1.00.000 con Glide**
(inventario [docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md));
US/D3D/parches solo como comparadores, nunca como referencia de «correcto».

- **Objetivo y alcance:** una pregunta por prueba (área FA + issue I-xx si
  aplica); qué queda fuera, explícito.
- **Entorno a registrar:** SO + arquitectura + build, CPU/RAM, GPU + driver
  exacto, wrapper Glide (nombre/versión/config) o HW 3Dfx, refresco usado.
- **Procedencia y hashes:** origen de cada fichero (CD EU, imagen, fix) +
  MD5/SHA-256 verificados contra inventario antes de probar.
- **Estado de instalación:** método (F-05 u otro), claves de registro
  creadas/valores, unidad CD (física/imagen/letra; TOC preservada o no),
  `glide2x.dll` en uso (origen/versión).
- **Pasos y veredicto:** pasos numerados reproducibles; resultado esperado
  (declarado antes); resultado observado (hechos); éxito/fallo según
  criterio escrito antes de ejecutar.
- **Artefactos:** capturas (arranque, menús, HUD, fallo), logs (juego,
  wrapper, monitor de registro/sistema), vídeo si hay timing implicado;
  FPS medidos + método de medida.
- **Fallos:** cada fallo registra síntoma, contexto completo, frecuencia
  (siempre/a veces/una vez) y repetición paso a paso; **observación**
  (visto) separada de **hipótesis** (causa propuesta) e **inferencia**.
- **Repetición:** rehacer desde estado limpio documentado (misma copia o
  copia fresca con igual hash); anotar qué cambió si no reproduce.
- **Custodia:** originales de Drive intactos (nunca probar sobre ellos);
  copias de trabajo aisladas por prueba; ningún byte del juego en el repo;
  re-verificar hashes al cerrar la prueba.
- **Limitaciones moderno-vs-época:** cada resultado anota qué parte del
  entorno es moderna (emulación MCI/DSound, wrappers, GPU, SO) y qué
  exigiría HW/SW de época; sin época, «correcto» queda provisional.

## Matriz de pruebas (priorizada, FA-04…FA-15)

> Fila FA-11/FA-12 ejecutada (Hito 1, 2026-10-09); resto sin ejecutar.
> Sin nuevos IDs (trazabilidad por área FA + nombre).
> FA-01…FA-03 fuera de la matriz runtime (inventario estático + diffs
> Etapa D). Prioridad: P0 = puerta de entrada · P1 = sistemas · P2 =
> complementarias. Resultados futuros → [COMPATIBILITY.md](COMPATIBILITY.md).

| Área FA | Prueba | Requisitos | Dependencias | Datos a capturar | Moderno / época | Riesgos | Prioridad |
|---|---|---|---|---|---|---|---|
| FA-11/FA-12 | Instalación + primer arranque EU (F-05) ✅ Hito 1 | Windows + contenido CD EU + unidad/imagen | Ninguna (puerta de entrada) | Claves creadas, mensajes CD, ¿arranca? | Moderno: instalador roto → F-05 manual; época: instalador real | I-15/I-16/I-17; Hito 1: arranca (ver abajo) | P0 |
| FA-12 | Detección CD: físico vs imagen vs ausente | Unidad óptica y/o imagen con TOC preservada | Instalación (fila anterior) | Mensaje exacto, letra/unidad aceptada, primer lector | Moderno: MCI degradado; época: lector real | Imágenes sin TOC/subcanales falsean | P0 |
| FA-11 | Registro: lecturas, guardado, admin | Monitor de registro/sistema (a elegir, p. ej. Sysinternals) | Instalación | Claves tocadas, imprescindible vs config, ubicación `GEX2*.GEX`, causa admin | Moderno sí | HKLM vs portable; I-17 | P1 |
| FA-09 | CD-audio por nivel + cambios de nivel | Audio funcional + CD/imagen con 16 pistas | Instalación + detección CD | Mapeo pista↔nivel, cortes (I-12), loops, volumen | Moderno: MCI/DSound emulados; época: referencia | I-11/I-12; 15-vs-16 abierto | P1 |
| FA-05 | FPS + velocidad baseline | Contador FPS + referencia visual | Instalación | FPS, frame pacing, velocidad observada | Moderno bajo wrapper (no referencia); época: Voodoo = referencia | Sin FA-15 no se valida «correcto» | P1 |
| FA-04 | Timing: velocidad vs refresco/V-Sync | Control de refresco (60/75 Hz si posible) | FPS baseline | Velocidad a distintos Hz/con V-Sync | Moderno parcial (modos limitados); época: 75 Hz pedido | I-01/I-02/I-07 | P1 |
| FA-13 | Modos de vídeo + viewport/HUD | Enumeración de modos (wrapper/log) | Instalación | Modos ofrecidos, resolución real, HUD por modo | Moderno: wrapper escala; época: 512x384@60 ref | I-06 (contexto wrapper) | P1 |
| FA-10 | Intro + cinemáticas | Arranque funcional | Instalación | ¿Reproducen? AV-sync, códec observable (sin asumir Indeo) | Moderno sí; época: referencia | I-14; F-06 débil | P1 |
| FA-06 | Teclado + joystick + ratón | Teclado, mando, observación en menús/juego | Instalación | Mapeo real, detección mando, ausencia ratón | Moderno: mandos ≠ época; época: joystick WinMM | I-18/I-19 | P1 |
| FA-08 | SFX + voces UK + volúmenes | Audio funcional | Instalación | SFX por nivel, disparadores de voz, efecto volumen (I-13) | Moderno: DSound emulado | I-13 (un solo reporte) | P1 |
| FA-14 | Fullscreen/ventana/foco/Alt+Tab | — | Instalación | Modo real, Alt+Tab, pérdida de foco | Moderno sí (+wrappers); época: exclusividad ref | I-06 | P2 |
| FA-07 | Cámara: controles y comportamiento | Juego jugable | Instalación | Controles reales PC, seguimiento, colisiones | Ambas (época ideal); sin evidencia previa | Sin base documental | P2 |
| FA-05 | F-03 antes/después (diff funcional) | Binario F-03 (E-1 completo; E-2 listo E-2.3, pendiente ejecución) | FPS baseline + acceso | FPS/velocidad exe F-03 vs original | Moderno (declarado Win98–XP: limitación) | Alcance declarado estrecho | P2 |
| FA-15 | Referencias «correcto» per sistema + matriz HW | Resultados de las filas anteriores | Todas (continua) | Tabla de referencia por sistema; matriz HW (Ola 3) | Gap época: sin HW propio salvo mantenedor | No inventar época | P1 |

## Prueba P-F03 — A/B exe EU F-03 vs original (rama B ejecutada 2026-10-09; ABA completo pendiente)

> Plan redactado 2026-10-09. NO ejecutado. Ningún paso corre sin
> autorización explícita del mantenedor (A1–A4). Caracterización
> off-label, NO intento de fix: un limitador no puede subir los 25 FPS.

- **Pregunta (1):** ¿el `GEX3D.EXE` EU de F-03 cambia FPS, velocidad de
  simulación, estabilidad, controles o niveles frente al original EU
  v1.00.000 + nGlide 2.10 en Win11? Fuera de alcance: mecanismo interno
  (Fase 2), otras ediciones/wrappers, cualquier toque al original.
- **Base:** FA-05 + I-01/I-02. Comparador A (original) vs B (F-03 EU),
  orden ABA (A1→B→A2). Sin referencia «correcto» (sin época).
- **Pre-condiciones (solo lectura):** Hito 1 reproducible; config nGlide
  transcrita Y CONGELADA (sin tocar VSync ni refresco); refresco de
  escritorio anotado; contador Steam operativo; imagen CD como `D:`
  (TOC anotada); copia fresca con MD5 verificado
  (`692b12825003417cc4a5a9d4db13eebe`).
- **Autorizaciones necesarias:** A1 test off-label (aceptar R1–R6 del
  informe 2026-10-09) · A2 registro gratuito tgames + descarga SOLO del
  exe EU (+ notas visibles del adjunto) · A3 snapshot estático pre-run
  (tamaño, MD5/SHA-256, recurso de versión, lista de imports; sin
  desensamblar, sin ejecutar) + scan AV · A4 ejecución en copia B.
- **Copias:** A = control (exe original intacto); B = clon de A con SOLO
  el exe sustituido por el EU de F-03 (backup del original con hash
  antes). Instalación original: NUNCA tocada. Registro: `InstallDir`
  conmuta A↔B entre series (HKLM global): registrar cada cambio y
  restaurar valores Hito 1 al cerrar.
- **Series (misma escena/punto de nivel, mismo método Steam):**
  1. Snapshot pre: hashes A/B, cfg nGlide, refresco, `InstallDir`.
  2. A1: 3 arranques (¿arranca? ¿intro? ¿menú?) + 60 s escena fija
     (FPS mín/med/máx) + travesía fija cronometrada + entrar a 2
     niveles + spot-check teclado.
  3. B: idéntico a A1 (conmutar `InstallDir`, anotar).
  4. A2: idéntico (control de deriva; conmutar de vuelta).
  5. Cierre: re-verificar hashes, restaurar registro, decidir
     conservar (Drive, etiquetado) o borrar B.
- **Métricas por serie:** FPS Steam (mín/med/máx, escena fija 60 s) ·
  velocidad sim (cronómetro travesía fija + percepción) · estabilidad
  (arranques OK/3, transiciones OK, cuelgues) · controles (teclado:
  mover/saltar/atacar/cámara/pausa) · niveles (¿entran? ¿música? ¿HUD?).
- **Criterio de éxito (pre-declarado):** procedimiento completo con datos
  utilizables en las 3 series. Interpretación:
  | B observado | Lectura |
  |---|---|
  | FPS↓ y/o sim más lenta | Limitador activo aquí → señal RE (timing), NO fix |
  | B ≈ A (≈25) | No concluyente (entorno off-label o suelo independiente) |
  | B no arranca / cuelga | Incompatible Win11+nGlide (coherente con alcance); revertir; negativo valioso |
  | FPS↑ en B | Inesperado: re-verificar identidad del exe antes de concluir |
- **Reversión:** borrar B (o restaurar exe original + re-hash MD5) +
  registro a valores Hito 1 + verificación final de que A arranca.
- **Registro:** hoja P-F03 (fecha, hashes exe A/B, entorno completo,
  tablas A1/B/A2, observaciones SEPARADAS de hipótesis). Resultados →
  COMPATIBILITY.md + I-01/I-02 (solo con datos).

## Ejecución P-F03/B (2026-10-09, PC mantenedor) — PARCIAL, rama B

> Primer resultado real del exe EU F-03. NO es el ABA completo:
> faltan A2 + hoja de métricas + snapshot E-1. Datos = observación
> del mantenedor (Steam para FPS), sin instrumentar.

- **A (baseline previo):** ≈25 FPS estables (Steam, 5ª–6ª).
- **B (exe EU F-03):** 42–44 FPS en menú Y en juego; cinemáticas ≈15
  (sin cambio vs ~15 percibidos); animaciones + velocidad general
  ACELERADAS; música y SFX a velocidad aparentemente normal.
- **Lectura:** rama «FPS↑ en B» de la tabla P-F03 (inesperada) →
  identidad CONFIRMADA (E-1.1: sha256 match 2026-10-09).
  Apoya I-01 (sim sigue a FPS); audio-rate independiente (observado,
  triggers sin probar); nº 42–44 no redondo ⇒ techo del sistema, no
  cap diseñado (inferencia).

## E-1 — Captura estática F-03 (EJECUTADO 2026-10-09, solo lectura)

> Ejecutado en Arena con descargas verificadas (supera a los scripts
> de abajo, que se conservan como referencia reproducible). Sin
> ejecutar nada; originales/registro/instalación intactos.

- **E-1.1 identidad:** `Get-FileHash` (MD5+SHA256) del exe probado en
  copia B → debe ser md5 `3198350eb398db9a64771842d7781b4b`,
  sha256 `7a9b68516867db5460b6ec0d152841742192b1bb47198d2c7b0357d7aa3e8254`.
  Si difiere: STOP (identidad rota). **OK 2026-10-09** (match exacto).
- **E-1.2 PE original-vs-EU:** script imports de 8ª (cambiar `$p` a
  cada exe) + versión/arquitectura:
```powershell
foreach($p in 'C:\RUTA\GEX3D_orig_EU.exe','C:\RUTA\GEX3D_EU_F03.exe'){
 $v=[System.Diagnostics.FileVersionInfo]::GetVersionInfo($p)
 $p+' | FileVer='+$v.FileVersion+' ProdVer='+$v.ProductVersion
 $x=[System.IO.File]::ReadAllBytes($p); $pe=[System.BitConverter]::ToInt32($x,0x3C)
 'Machine='+[System.BitConverter]::ToUInt16($x,$pe+4).ToString('X4')+' TimeDateStamp=0x'+[System.BitConverter]::ToUInt32($x,$pe+8).ToString('X8')}
```
- **E-1.3 resumen byte-diff** (solo números, sin subir bytes):
```powershell
$a=[System.IO.File]::ReadAllBytes('C:\RUTA\GEX3D_orig_EU.exe')
$b=[System.IO.File]::ReadAllBytes('C:\RUTA\GEX3D_EU_F03.exe')
'lenA='+$a.Length+' lenB='+$b.Length
$n=[Math]::Min($a.Length,$b.Length); $d=0; $first=-1; $last=-1
for($i=0;$i-lt$n;$i++){if($a[$i]-ne$b[$i]){$d++;if($first-lt0){$first=$i}$last=$i}}
'difieren='+$d+' de '+$n+' primero=0x'+$first.ToString('X')+' ultimo=0x'+$last.ToString('X')}
```
- **Criterio:** E-1.1 OK + imports/versión/arch de ambos + conteo diff.
- **Resultado (2026-10-09):** descargas md5+sha256 OK (3/3); PE x86
  GUI ×3, links 05-18/06-29/06-24; EU = build distinta (82.9% ≠);
  imports sin nueva API timing (+`mciGetErrorStringA` solo); timing
  = `Sleep`+`GetTickCount`; path `_demo` en EU/US; EU≠US (67.3% ≠).
  Detalle: S-26 + F-03. **E-1.1 OK (2026-10-09):** paste sha256 =
  bytes Drive EU (triple: paste + metadato + hash local re-descarga).

## E-2 — Micro-test sim-vs-FPS (AUTORIZADO 2026-10-10; LISTO PARA EJECUTAR 2026-10-10 — método E-2.3)

> Diseñado para distinguir H2 (throttle eliminado → render-bound)
> de H3 (timer reprogramado). 1 variable cada vez; copias A/B.

- **Variables:** exe A/B; luego en B: escena simple↔compleja; luego
  VSync on/off (nGlide). **Constantes:** resto cfg nGlide, InstallDir
  conmutado+registrado, CD `D:`, nivel/punto, Steam.
- **Métricas:** travesía fija cronometrada ×3 (A y B) + FPS
  mín/med/máx + ciclos de anim periódica en 10 s (A y B) + FPS B
  escena simple vs compleja.
- **Decisión:** t_B/t_A ≈ 25/43 ⇒ sim frame-acoplada (H1/H2 ✓);
  FPS B varía con escena ⇒ render-bound sin timer (H2 ✓) vs FPS B
  fijo ⇒ timer (H3 ✓); VSync mueve B ⇒ acoplado a presentación.
- **Reversión:** borrar copia B; registro a Hito 1; nGlide
  restaurado + retorno verificado.
- **Pre-requisito E-2.0 (2026-10-10, OK):** backup original
  (`...\exe original\GEX3D.EXE`) md5 = pin repo/Drive
  `692b1282…` (match exacto; aviso de discrepancia = solo
  mayúsculas de PowerShell, sin error de transcripción).
  Fuente de copia A verificada; el método sin registro quedó
  invalidado (E-2.2) y reparado (E-2.3).
- **E-2.1 copias A/B (2026-10-10, OK):** `E2_A_ORIGINAL` md5 match
  (original), `E2_B_F03EU` sha256 match (F-03 EU); principal
  conservada; lanzamiento sin registro (desde carpeta propia,
  InstallDir Hito 1 intacto). Mediciones NO iniciadas (ver E-2.2).
- **E-2.2 hallazgo (2026-10-10, BLOQUEANTE método):** renombrar
  `<DIR_PRINCIPAL>\` impide el arranque (error `Cannot load font
  <DIR_PRINCIPAL>\font3.dff`); copias A/B también fallan desde sus
  exes. Método sin registro invalidado; mediciones no iniciadas.
  Ver I-23. Origen IDENTIFICADO estáticamente 2026-10-10
  (cadena InstallDir→`font.3df`); confirmación dinámica pendiente (E-2.3).
- **E-2.3 método reparado (2026-10-10, LISTO PARA EJECUTAR, PC
  mantenedor):** cada copia A/B se lanza con su propio `InstallDir`
  registrado (WOW64: usar `reg … /reg:32`): (1) punto de
  restauración: `reg export HKLM\SOFTWARE\Crystal Dynamics\Gex2`
  + md5 del exe (= pin repo); (2) `reg add …\Gex2\1.00 /v InstallDir
  /d <carpeta-copia>` antes de cada tanda + CD `D:` presente;
  (3) arrancar desde la carpeta de la copia, tanda A (travesía ×3 +
  FPS + ciclos anim 10 s), conmutar registro, tanda B, escena
  simple↔compleja, VSync on/off; (4) reversión: registro a Hito 1,
  borrar copia B, retorno verificado. ACEPTAR: A/B arrancan y miden
  (I-23 confirmado dinámicamente si el fallo sigue la ruta
  registrada); DESCARTAR: cualquier arranque fuera de su InstallDir
  registrado. Micro-item: 1 dir listing (`font.3df` vs `font3.dff`).

## E-3 — Superficie timing + contexto candidato 1 (EJECUTADO 2026-10-10, solo lectura)

> Análisis estático acotado del exe EU original en Arena (descarga
> Drive→sandbox verificada md5 `692b1282…`, objdump/binutils;
> copia eliminada después). Sin ejecutar ni modificar nada
> (exe/DLL/registro/instalación intactos). E-2 listo para ejecutar
> (método E-2.3; ver I-23).
> Flujo temporal gameplay (M-13): REVERSE_ENGINEERING.md Etapa C.

- **Superficie timing (hechos):** únicas APIs = `Sleep`+`GetTickCount`
  (1 call real `Sleep(10)` en `0x4631C1`, loop idle; 7 calls
  `GetTickCount`; stubs muertos `0x540418`/`0x5403CA`); init t0
  `0x43B36D`→`ds:0x57B5FC`; cálculo `elapsed/16.666` (`0x43E5DD`,
  doble `0x54C060` VERIFICADO) + contador frames; wrapper `0x537BB0`;
  busy-waits 4000 ms (`0x4637F0`) / 0 (`0x463808`); debounce
  10 ms `0x53B776`. 3 strings ancla: fmt Timer `0x54D0EC`←`0x406B3A`,
  `grSstQuery-fail` `0x54F664`←`0x424DB1`, spline-error `0x54D310`←`0x411DD8`.
- **Candidatos:** (1) `push 0xFA0` en `0x4637E4` (bytes `68 A0 0F 00 00`
  @offset `0x62BE4`, VERIFICADO); (2) `Sleep(10→1)` en idle (no
  gameplay). La rama `16.666` es la más relevante para M-13 pero NO
  primer parche (riesgo float-math a ciegas).
- **Contexto candidato 1 (hechos):** `0x463278` = WinMain (init +
  máquina de 9 estados, jump table `0x463BC1`); estado 6 = rama cine:
  compone `<InstallDir>\movie\<nombre>` desde tabla de **12 slots**
  en `0x551668` (subs 0–0xB: crylogo, logo, intro, outro, scans,
  clak1–4, bandai, midway, ubisoft); abre pares `.sag`/`.jam` modo
  `'r'` (`0x537ED1`); SOLO `sub==1` (`\movie\logo`) toma
  `0x538017(buf,0,4000)`; resto `(buf,1,0)` sin espera. Post-espera:
  `0x4650FD`, estado 6, transiciones sub (0→0xB, 0xA→2, 0xB→2, 2→1,
  3→check bits `0x27`→sub 4 ó estado 9, resto→9); estado 3 = init t0
  (`0x43B11D`); estado 2 = loop juego. Secuencia arranque INFERIDA:
  8→6 (crylogo→ubisoft→intro→logo+4 s)→9→3→2.
- **Nota tabla 12 vs inventario (hecho + pregunta abierta):** el disco
  EU trae 9 pares (FA-10, ficheros); el código referencia 12 (añade
  clak4, bandai, midway). Diferencia código↔disco sin explicar;
  FA-10 intacto (inventario de ficheros, no de código).
- **Evidencia vs inferencia:** direcciones/ramas/tabla/modos `'r'` =
  evidencia (disasm); propósito «hold splash logo ~4 s» = INFERENCIA
  fuerte (no se descarta decode/sonido solapado — incógnita
  principal); orden FMVs = inferencia; nombres sprintf/open/refcount
  = inferencia (comportamiento evidenciado).
- **Clasificación:** pausa funcional (hold splash/FMV). NO carga de
  nivel, NO init motor, NO fade (sin rampas; `0x647EA8` = anidamiento).
- **Riesgos test 4000→1000 ms (futuro, NO autorizado):** corte
  FMV/sonido si decodifica durante el hold; posible race init;
  alcance SOLO splash logo; cero efecto M-13 (gameplay = estado 2).
- **Veredicto:** APTO para considerar micro-test reversible en copia
  aislada como ejercicio de metodología A/B; NO como avance M-13.
  Sin intervención binaria; pendiente autorización expresa.

## Hito 1 — Primera prueba funcional (2026-10-09, PC del mantenedor) ✅ EJECUTADA

> Primer hito funcional confirmado del proyecto: instalación manual F-05
> (sin paso F-01) + arranque + nivel jugable en Windows 11 64-bit, con
> ejecutable EU original inalterado (MD5 verificado). Renderer:
> wrapper = nGlide 2.10 CONFIRMADO (payload-hash, 9ª sesión); API efectiva pendiente.
> FPS ≈25 estables (5ª–6ª sesión, método Steam); A/B VSync nulo (6ª);
> sin logs ni capturas archivadas todavía.

1. **Entorno (CONFIRMADO, parcial):** Windows 11 de 64 bits. Build de
   Windows, CPU/RAM, GPU + driver, refresco y layout de monitores:
   PENDIENTES de registrar (parte del siguiente paso recomendado).
2. **Instalación (CONFIRMADO):** copia manual de `GEX2/` EU a
   `<DIR_PRINCIPAL>` (532 ficheros); `GEX3D.EXE` 1557504 B, MD5
   `692b12825003417cc4a5a9d4db13eebe` (coincide con inventario §4).
   Exe inalterado (sin parches); nGlide no instalado *durante* esta
   prueba (instalaciones anteriores: UNKNOWN).
3. **Registro (CONFIRMADO en esta config):** `...\Gex2\1.00` con
   `Version`=DWORD 2, `InstallDir`=`<DIR_PRINCIPAL>`,
   `CDDriveName`=`D`. Imprescindible-vs-config y causa admin:
   pendientes (FA-11).
4. **CD (CONFIRMADO en esta config):** imagen CloneCD original montada
   como `D:`; el juego arranca sin error de CD (FA-12). TOC de la
   imagen montada: no re-verificada en esta prueba.
5. **Resultado (CONFIRMADO):** `GEX3D.EXE` arranca, entra en un nivel y
   permite mover al personaje; música y efectos funcionan en esta
   config (I-11 NO reproducido aquí; I-12 pendiente de más niveles).
6. **Problemas observados (CONFIRMADO como observación, causa UNKNOWN):**
   resolución baja 4:3 (esperable de la época, modo exacto sin medir);
   bandas negras arriba/izquierda con centrado sin confirmar; HUD
   ligeramente descolocado; iconos del escritorio desplazados al
   segundo monitor durante el juego (reversible al cerrar) → I-21, I-22.
7. **Hallazgo Glide (2ª sesión: `glide2x` CARGADA):** `glide2x.dll` cargada
   desde `C:\WINDOWS\SYSTEM32\` (= fichero físico `SysWOW64\glide2x.dll`
   por redirección WOW64; coherente con el observado de 1630208 B — el
   hash registrado en 3ª sesión). `3dfxSpl2.dll` también cargada (splash 3dfx, S-21).
   `glide.dll`/`glide3x.dll`: carga sin confirmar. Procedencia e identidad
   de la `glide2x` (wrapper moderno vs época): pendientes entonces (FA-02/FA-03; resuelto 9ª: nGlide 2.10).
8. **Pantalla 3DFX (2ª sesión: mecanismo identificado):** la muestra
   `3dfxSpl2.dll` (biblioteca splash de Glide 2.x, S-21), cargada en el
   proceso. Por sí sola sigue sin demostrar el renderer activo (eso lo
   indica `glide2x`, ver nota 2ª sesión).
9. **Custodia (CONFIRMADO):** originales de Drive intactos; prueba sobre
   copia aislada; ningún byte del juego en el repo; ningún dato personal
   del PC registrado salvo lo listado aquí.

### Nota post-Hito 1 (2026-10-09): enumeración 64-bit vacía = artefacto WOW64

- **Evidencia (CONFIRMADO):** con el juego en marcha, `(Get-Process GEX3D)`
  desde PowerShell de 64-bit devuelve 7 módulos (`GEX3D.EXE`, `ntdll.dll`,
  `wow64.dll`, `wow64base.dll`, `wow64win.dll`, `wow64con.dll`,
  `wow64cpu.dll`); el filtro gráfico
  (`glide|3dfx|d3d|ddraw|dxgi|opengl|vulkan|…`) no devuelve nada.
- **Explicación (comportamiento documentado de WOW64, no del juego):** un
  observador de 64-bit solo ve el lado de 64-bit del proceso emulado (la
  capa WOW64); las DLL reales de 32-bit (`glide2x`, `winmm`, `dsound`…)
  son invisibles por esta vía. Es un artefacto de medida: **no dice nada
  sobre el renderer**, ni a favor ni en contra de Glide. La composición
  exacta del set (`wow64base`/`wow64con` incluidos) depende de la build de
  Windows y no es diagnóstica.
- **Siguiente paso (EJECUTADO 2026-10-09):** consulta repetida desde
  PowerShell de 32-bit → ver nota «2ª sesión» (`glide2x` cargada).

### Nota post-Hito 1 (2026-10-09, 2ª sesión): `glide2x` cargada + parpadeos

- **Renderer (CONFIRMADO carga; hipótesis sólida):** con el juego en marcha,
  `glide2x.dll` CARGADA desde `C:\WINDOWS\SYSTEM32\glide2x.dll` (= físico
  `SysWOW64\glide2x.dll` por redirección WOW64; coherente con el fichero
  observado de 1630208 B — hash registrado en 3ª sesión). `3dfxSpl2.dll` también
  cargada (splash Glide 2.x, S-21). Ruta Glide = hipótesis muy sólida;
  ficha registrada en 3ª sesión; originalidad/wrapper pendientes entonces (resuelto 9ª: nGlide 2.10).
- **Acompañantes (OBSERVADO, rol UNKNOWN):** `ddraw.dll`, `d3d9.dll`,
  `dxgi.dll` y `atidx9loader32.dll` presentes en el proceso. Su presencia
  NO demuestra que sean el renderer activo (driver, appcompat, …).
  `atidx9loader32` sugiere GPU AMD (HYPOTHESIS por prefijo; modelo/driver
  pendientes). Exe intacto (MD5 re-confirmado).
- **Pantalla (OBSERVADO):** el 2º monitor parpadea tras el splash 3dfx, al
  empezar las cinemáticas (Ubisoft + juego) y al pasar al menú principal;
  4:3 + bandas arriba/izquierda + HUD desalineado de nuevo → I-21/I-22.
- **Siguiente paso (EJECUTADO 2026-10-09):** ficha del fichero registrada
  → ver nota «3ª sesión».

### Nota post-Hito 1 (2026-10-09, 3ª sesión): ficha Glide registrada

- **Ficha `glide2x.dll` (CONFIRMADO, solo lectura):**
  `SysWOW64\glide2x.dll` (1630208 B) — Descripción `3Dfx Interactive, Inc.
  Glide DLL`; Producto `Glide para Voodoo Banshee` (transcripción aproximada;
  nombre exacto en 4ª sesión); Versión de producto `2.60.0.658` (disputa RESUELTA en 7ª: relectura PC `2.61.00.0658`; 3ª fue error de transcripción); SHA-256
  `7cbd095872e821b54cd6fa03f76aa22073271567175069c53ebb2e73b0299aab`.
  Presente en los módulos de `GEX3D.EXE` en ejecución.
- **Ficha `3dfxSpl2.dll` (CONFIRMADO):** Descripción y Producto `3dfx
  Splash Screen`, versión `1.0.0.4`; también cargada en el proceso.
  Biblioteca de pantalla de inicio (S-21); NO es prueba del renderer.
- **Interpretación (rigurosa):** la ficha es COMPATIBLE con una
  implementación Glide de 3dfx, pero metadatos + hash por sí solos NO
  demuestran si el fichero es original, redistribuido o modificado, ni
  si interviene un wrapper. Ruta Glide = hipótesis muy sólida (no hecho
  cerrado). I-21/I-22 siguen pendientes, sin causa atribuida.
- **Siguiente paso (EJECUTADO 2026-10-09):** fechas/firma consultadas
  → ver nota «4ª sesión».

### Nota post-Hito 1 (2026-10-09, 4ª sesión): fecha mostrada + sin firma

- **Producto exacto (CONFIRMADO):** `Glide® for Voodoo Banshee®` (nombre
  mostrado en Propiedades; corrige la transcripción aproximada de la 3ª
  sesión). Resto de la ficha intacto (2.61.00.0658 — corr. 7ª; SHA-256 verificado).
- **Fecha mostrada en Propiedades (CONFIRMADO como dato mostrado):**
  domingo 2019-09-15 00:54:48. Campo específico (creación/modificación)
  NO identificado: no atribuir a ninguno.
- **Firma digital (CONFIRMADO ausencia observada):** ninguna pestaña de
  firmas digitales ni firma visible en las propiedades consultadas.
- **Interpretación (documentada):** metadatos compatibles con Glide de 3dfx
  / Voodoo Banshee; fecha + ausencia de firma NO determinan si la DLL es
  original, redistribuida, modificada o de un paquete posterior; el hash
  identifica el contenido, no su procedencia o autenticidad; la carga es
  pista relevante, no demuestra la API de renderizado ni descarta un
  wrapper. `3dfxSpl2.dll` = pantalla de inicio (S-21), no prueba de
  renderer. I-21/I-22 pendientes, sin causa definitiva.
- **Siguiente diagnóstico (DEFINIDO 5ª, EJECUTADO 6ª: nulo):** A/B VSync
  nGlide (ver 6ª sesión). Metadatos/hash 3ª–4ª sesión archivan la ficha
  anterior; la ficha nueva (2.61.00.0658) necesita tamaño+hash propios.

### Nota post-Hito 1 (2026-10-09, 5ª sesión): nGlide + 25 FPS + DLL 2.61

- **Entorno (CAMBIO VERIFICADO):** nGlide 2.10 instalado en Windows (en
  Hito 1–4ª sesión no lo estaba *durante* las pruebas). Comparabilidad
  con sesiones anteriores: LIMITADA (config distinta).
- **`glide2x` cargada (OBSERVADO, ficha nueva):** versión mostrada
  `2.61.00.0658`, metadatos 3Dfx Voodoo Banshee/Voodoo3 — cadena
  MISMO fichero que 3ª sesión (hash idéntico, 7ª): NO hubo sustitución
  (1630208 B, SHA-256 `7cbd…9aab`; tamaño/hash resueltos en 7ª).
- **Wrapper (CONCLUSIÓN SÍ → A2.2):** nGlide instalado + su instalador
  coloca su `glide2x.dll` en SysWOW64 (S-02, triple fuente) + el juego
  carga desde SysWOW64 + render funcional en HW moderno sin 3dfx (un
  driver de época no podría operar aquí) ⇒ wrapper=SÍ. Vendedor
  «nGlide» PROBABLE entonces (7ª–8ª: sin demostrar; 9ª: CONFIRMADO por payload-hash, A2.3 L ✓; ver S-22).
- **FPS (OBSERVADO, método pendiente):** ≈25 estables. Método de medida
  NO registrado ⇒ cifra provisional. Sin logs/vídeo.
- **nGlide (OBSERVADO, nulos):** `Aspect correction` no mejora I-21;
  `Refresh rate: By desktop` sin mejora perceptible. Nulo ≠ prueba de
  ruta (un bypass daría el mismo nulo). 1 prueba cada uno.
- **Modo vídeo (OBSERVADO):** un único parpadeo al iniciar, compatible
  con un cambio de modo. Refresco de escritorio: pendiente.
- **Herramientas (LIMITACIÓN CONFIRMADA):** MSI Afterburner/RTSS ⇒
  negro + cierre (inyección incompatible con esta ruta); contador de
  Steam inservible en aquel intento (en 6ª sí aportó lecturas en nivel).
  NO proponer overlays con inyección. Checklist #7: Steam válido para
  FPS en nivel; pacing preciso pendiente (conteo slow-mo).
- **Siguiente experimento (DEFINIDO 5ª, EJECUTADO 6ª: nulo):** A/B
  de `Vertical synchronization` en nGlide Configurator (On→Off→On):
  si los FPS saltan ⇒ 25 impuesto por sincronización/presentación;
  si persisten ⇒ límite propio del juego (y podría cerrar «API
  efectiva» si el ajuste mueve el render). Prerrequisitos: ajustes
  actuales anotados, refresco de escritorio, hash/tamaño DLL, método
  FPS. Reversión: volver a On + verificar retorno a ≈25.

### Nota post-Hito 1 (2026-10-09, 6ª sesión): A/B VSync nulo + intros ≈15

- **A/B VSync (CONFIRMADO, 1 variable):** config de referencia, solo
  `Vertical synchronization` On→Off→On. Nivel: ≈25 estables en AMBOS
  estados, sin diferencia visual relevante. Método: contador Steam
  (cierra la laguna de método de 5ª para FPS en nivel). Sin tocar
  originales ni registro.
- **Interpretación (rigurosa):** VSync por sí solo DESCARTADO como
  causa del 25. No demuestra la causa real. Hipótesis líder: límite
  propio del juego (~40 ms); alternativas: otro factor de
  sincronización/presentación (menos probable tras el nulo).
  «API efectiva» (A2.2) sigue abierta (el ajuste no movió el render).
- **Intros (OBSERVACIÓN usuario, NO medida):** secuencias Ubisoft,
  Crystal Dynamics y logo Gex —percibidas como vídeos— a ~15 FPS
  ESTIMADOS visualmente. Formato/frecuencia reales: SIN CONFIRMAR
  (repo: pares `MOVIE/` UBISOFT/CRYLOGO/LOGO+INTRO…, inventario §7;
  `.JAM`=vídeo/`.SAG`=audio solo hipótesis por tamaños; magias sin
  muestrear, §14.3). I-14 NO reproducida aquí (las intros SÍ se ven).
- **Modo vídeo (OBSERVADO):** el parpadeo del 2º monitor es UN único
  cambio de modo/resolución al arrancar, no parpadeo continuo (I-22).
  Refresco de escritorio: sigue pendiente.
- **Siguiente paso (PROPUESTO, sin cambios):** medida precisa de FPS +
  pacing con vídeo slow-mo del móvil (120/240 fps) + lecturas de solo
  lectura pendientes (refresco escritorio; hash DLL resuelto en 7ª). Pacing
  regular a 25.00 ⇒ temporización propia; irregular ⇒ otra causa.

### Nota post-Hito 1 (2026-10-09, 7ª sesión): hash idéntico + metadatos completos

- **Fichero (CONFIRMADO):** `SysWOW64\glide2x.dll`, SHA-256 idéntico al
  de 3ª sesión (`7cbd…9aab`), 1630208 B ⇒ MISMO fichero, NO hubo
  sustitución entre sesiones. Versión releída `2.61.00.0658` ⇒ disputa
  3ª RESUELTA (transcripción errónea).
- **Metadatos (OBSERVADO):** Company `3Dfx Interactive, Inc.`;
  ProductName «Glide para Voodoo Banshee/Voodoo3» (ES) vs «Glide® for
  Voodoo Banshee®» (EN, 4ª sesión) sobre el MISMO hash ⇒ hipótesis
  líder: recurso de versión multilingüe (sin confirmar hasta enumerar
  bloques). Archivo 3dfx en ambos idiomas; nada dice «nGlide».
- **nGlide (CONFIRMADO presencia, uso SIN DEMOSTRAR):**
  `nglide_config.exe` existe en SysWOW64 ⇒ instalador ejecutado. Pero
  el fichero cargado es el 7cbd preexistente ⇒ instalador NO sustituyó
  (omisión) O 7cbd es su propio payload (sin referencia) ⇒ coexistencia
  sin resolver (S-22). Configurador instalado ≠ wrapper en uso por Gex.
- **Interpretación (rigurosa):** wrapper=SÍ intacto; vendedor
  INDETERMINADO entonces (9ª: resuelto — nGlide 2.10; ver S-22). `d3d9.dll` en proceso
  sigue sin probar nada (compatible con backend D3D oficial de nGlide,
  nunca probatorio; criterio 2ª sesión intacto).
- **Siguiente paso (PROPUESTO, solo lectura):** registro de
  desinstalación nGlide (DisplayVersion/InstallDate) + versión de
  `nglide_config.exe` + lectura de ajustes sin cambiar nada. Instalado
  tras 4ª sesión ⇒ instalador no sustituyó ⇒ nGlide puenteado;
  instalado antes de Hito 1 ⇒ payload-propio posible. Slow-mo/FPS
  aplazados hasta aclarar la ruta.

### Nota post-Hito 1 (2026-10-09, 8ª sesión): registro sin fechas + payload pendiente

- **Registro nGlide (CONFIRMADO):** DisplayName `nGlide 2.10`,
  DisplayVersion `2.10`; `InstallDate` e `InstallLocation` VACÍOS ⇒
  vía temporal INCONCLUSA (sin inferir fechas). Configurador:
  FileVersion/ProductVersion `2.10`, ProductName `nGlide Configurator`
  (CONFIRMADO).
- **DLL (RECONFIRMADO ×3):** mismo SHA-256 7cbd…, 1630208 B, metadatos
  3Dfx `2.61.00.0658` ⇒ continuidad intacta.
- **Instalador S-14 (metadatos Drive, solo lectura):**
  `nGlide210_setup.exe` en `S-14_extracted`, 3301587 B (coherente con
  3.14–3.15 MB del corpus), md5 `cd30d314c3f1470cef1a35300fda1a20`,
  SHA-256 `3cfcd03a923386c36685a772d24797fb78762cfbe63fe5676756091cf27da7a4`.
  Inspección del payload BLOQUEADA desde aquí (binario 3.3 MB no
  transitable; sandbox sin salida a Drive) ⇒ comparación de hash
  pendiente entonces (9ª: hash obtenido vía informe Falcon, sin inspección
  local); .exe ni ejecutado ni descargado. Corpus: sin referencia
  pública del payload nGlide 2.10 (negativo).
- **Atribución:** SIN DEMOSTRAR entonces (9ª: DEMOSTRADO nGlide 2.10 por
  payload-hash). Ni nombre, ni configurador, ni d3d9 atribuían por sí solos.
- **Siguiente paso ÚNICO (solo lectura, sin instalar nada):** lista de
  imports PE de la DLL cargada (pegar en PowerShell, juego cerrado;
  algoritmo validado contra PE sintético; EJECUTADO 9ª):

```powershell
$p='C:\Windows\SysWOW64\glide2x.dll'
$b=[System.IO.File]::ReadAllBytes($p)
$pe=[System.BitConverter]::ToInt32($b,0x3C)
if([System.Text.Encoding]::ASCII.GetString($b,$pe,2)-ne'PE'){'NO-PE';exit}
'Maquina: '+[System.BitConverter]::ToUInt16($b,$pe+4).ToString('X4')
$magic=[System.BitConverter]::ToUInt16($b,$pe+24)
$dd=$pe+24+$(if($magic-eq0x20b){112}else{96})+8
$impRVA=[System.BitConverter]::ToInt32($b,$dd)
$nSec=[System.BitConverter]::ToUInt16($b,$pe+6)
$optSize=[System.BitConverter]::ToUInt16($b,$pe+20)
$secOff=$pe+24+$optSize
$secs=@()
for($i=0;$i-lt$nSec;$i++){$secs+=[pscustomobject]@{VA=[System.BitConverter]::ToInt32($b,$secOff+$i*40+12);SZ=[System.BitConverter]::ToInt32($b,$secOff+$i*40+8);Raw=[System.BitConverter]::ToInt32($b,$secOff+$i*40+20)}}
function r2f($r){foreach($s in $secs){if($r-ge$s.VA-and$r-lt($s.VA+$s.SZ)){return $s.Raw+($r-$s.VA)}}return -1}
function cstr($f){$e=$f;while($e-lt$b.Length-and$b[$e]-ne0){$e++};[System.Text.Encoding]::ASCII.GetString($b,$f,($e-$f))}
$d=r2f $impRVA
if($d-lt0){'SIN-TABLA-IMPORTS';exit}
$k=0
while($true){$o=[System.BitConverter]::ToInt32($b,$d);$t=[System.BitConverter]::ToInt32($b,$d+16);$n=[System.BitConverter]::ToInt32($b,$d+12);if($o-eq0-and$t-eq0-and$n-eq0){break}$f=r2f $n;if($f-ge0){cstr $f}$d+=20;$k++;if($k-gt64){'TRUNCADO-64';break}}
```

  Solo sistema (+3dfxSpl2) y nada D3D/Vulkan ⇒ estilo época (muere
  payload-nGlide); d3d9/d3d11/dxgi/vulkan-1 ⇒ estilo wrapper (muere
  driver clásico; nGlide exigiría referencia). CORRECCIÓN 9ª: dicotomía
  SUPERADA — payload nGlide 2.10 confirmado CON imports solo-sistema ⇒
  backend dinámico (la ausencia estática nunca descartó wrapper). Ajustes
  del configurador: aún pendientes de transcribir.

### Nota post-Hito 1 (2026-10-09, 9ª sesión): imports + atribución nGlide 2.10

- **Imports PE (CONFIRMADO, solo lectura):** `SysWOW64\glide2x.dll` =
  PE `014C` (x86); imports estáticos: `KERNEL32`, `USER32`, `GDI32`,
  `ADVAPI32`, `WINMM`, `VERSION` (6 DLL de sistema). Sin imports
  estáticos de Direct3D, DXGI ni Vulkan.
- **Interpretación imports (rigurosa):** por SÍ SOLOS son compatibles
  con driver de época Y con wrapper de backend dinámico (LoadLibrary):
  no prueban originalidad 3dfx ni descartan wrapper. Solo muere la
  sub-hipótesis «backend enlazado estáticamente». La dicotomía de 8ª
  era falsa (corregida arriba): verificar > concluir.
- **Atribución (CONFIRMADO 9ª — cadena de hashes):** instalador S-14 =
  oficial nGlide 2.10 (md5/SHA1 publicados por Zeus, t=557) = muestra
  Falcon 3cfcd03a…; su drop `glide2x.dll` = SHA-256 7cbd… = DLL cargada
  (1630208 B, md5 f59d9780… idem) ⇒ la DLL cargada ES el payload de
  nGlide 2.10 ⇒ Gex usa nGlide 2.10. Detalle + tabla de drops: S-22.
  Abierta: cadena de adquisición pre-Hito 1 (A2.3 M).
- **Corolarios:** metadatos 3dfx 2.61.00.0658 + ES/EN + circulación
  dllme = marca de fábrica del payload nGlide (no de driver época);
  backend D3D/Vulkan oficial ⇒ carga dinámica (imports mudos);
  «instalación» 5ª = re-drop del mismo fichero (o idéntico omitido).
- **Siguiente paso ÚNICO:** medida precisa FPS + pacing (slow-mo móvil
  + refresco escritorio; diferido 6ª hasta aclarar la ruta — ACLARADA).
  Opcionales (no el paso): hash `3dfxSpl2.dll`/configurador, ajustes.

## Evaluación de alternativas de entorno (2026-10-09; Hito 1 = vía A)

> Hito 1 (2026-10-09): primera config observada funcionando (vía A:
> Win11 64-bit + F-05 manual + imagen como D:, sin parches). Veredicto
> general de compatibilidad: pendiente (una sola prueba; ficha + fecha y
> firma registradas — 4ª sesión; wrapper = nGlide 2.10, 9ª).
> Base: EU v1.00.000 Glide (inventario §4.1: 38 `glide2x`, MCI, DSound,
> WinMM; instalador 16-bit roto en moderno → F-05).

### A. Windows moderno + wrapper Glide (PC físico)

- **Ventajas:** HW real y disponible; drivers actuales; captura (FPS,
  vídeo, logs) con herramientas modernas; instalable vía F-05 + wrapper.
- **Limitaciones:** MCI/CD-audio degradado (I-11); DSound emulado; modos
  y refrescos limitados por GPU/monitor modernos (75 Hz incierto);
  instalador 16-bit inútil; UAC/admin (I-17); DWM/Alt+Tab ≠ época.
- **Requisitos:** Win 10/11 64-bit exacto (versión+build); GPU+driver
  exactos; wrapper (nombre+versión+config); unidad óptica o imagen con
  TOC; herramientas de captura.
- **Riesgos:** atribuir al juego artefactos del wrapper/SO/GPU; Hito 1
  ejecutado una sola vez (ficha + fecha/firma — 4ª sesión;
  wrapper = nGlide 2.10, 9ª); «correcto» de época
  inalcanzable aquí.
- **Sirve para:** P0 (instalación, detección CD lógica, registro,
  guardado, admin) + P1 observación (FPS relativo, modos modernos,
  input moderno, intro/SFX audibilidad). **No sirve para:** referencia
  «correcto» (FA-15), timing/FPS absolutos, CD-audio MCI original.

### B. Máquina virtual Windows (viabilidad parcial)

- **Ventajas:** snapshots y estado limpio reproducible; aislamiento;
  varios SO huésped (XP/7/10) en un host; instalación/registro
  repetibles.
- **Limitaciones:** GPU virtual o passthrough (doble emulación Glide→
  wrapper→GPU virtual); reloj virtualizado (timing/FPS no fiables);
  CD-audio/MCI en huésped degradado; refrescos virtuales; input con
  latencia.
- **Requisitos:** hipervisor+versión, huésped exacto, guest additions,
  GPU virtual vs passthrough documentado, política de snapshots.
- **Riesgos:** artefactos de virtualización confundidos con juego.
- **Veredicto:** viable para P0 (instalación/registro/detección lógica)
  y observación gruesa; **no válida** para FA-04/FA-05 absolutos,
  FA-13 referencia ni FA-15. No equivalente a A ni a C.

### C. Hardware histórico 3dfx (referencia de época)

- **Ventajas:** única referencia válida de «correcto»: Voodoo real,
  MCI/CD-audio original, refrescos de época, DSound HW, joystick WinMM
  época, instalador 16-bit funcional (Win95/98).
- **Limitaciones:** disponibilidad (no consta HW en proyecto; decisión
  del mantenedor); fragilidad; captura externa obligatoria (VGA);
  medición FPS por vídeo/conteo (sin overlays modernos); drivers época.
- **Requisitos (abiertos):** PC época o compatible, Voodoo (modelo a
  decidir), Win95/98 + DX época, unidad CD, joystick, capturadora.
  Sin modelo concreto: no consta en documentación.
- **Riesgos:** HW único/frágil, coste, tiempo.
- **Sirve para:** FA-15 (definir «correcto»), timing/FPS referencia,
  CD-audio referencia, render referencia, instalador original.

### Separar SO / wrapper / hardware

Variar un factor cada vez (mismo juego+wrapper en 2 SO; mismo SO+juego
con 2 wrappers; mismo todo en 2 GPUs); matriz de combinaciones
registrada; cada resultado con tupla completa (protocolo); contraste
contra época cuando exista.

## Requisitos mínimos: checklist previo (2026-10-09)

> Clasificación: IMP = imprescindible (toda prueba) · REC = recomendable ·
> ESP = específico de pruebas. Sin especs mínimas inventadas del juego.

| # | Requisito | Clase | Pruebas |
|---|---|---|---|
| 1 | Windows versión + arquitectura + build | IMP | Todas |
| 2 | CPU + RAM | IMP | Todas |
| 3 | GPU + driver exacto | IMP | Todas |
| 4 | Wrapper Glide + versión + config, o 3dfx modelo+driver | IMP | Todas |
| 5 | Método instalación + hashes verificados | IMP | Todas |
| 6 | Unidad CD física (modelo) o imagen + herramienta montaje + TOC verificada | IMP | FA-09/FA-12; resto REC |
| 7 | Contador FPS + método | IMP | FA-04/FA-05; resto REC |
| 8 | Captura pantalla/vídeo + logs + monitor registro | IMP | Todas (monitor: FA-11 IMP) |
| 9 | Copias aisladas + verificación de hash | IMP | Todas |
| 10 | Refrescos disponibles (lista EDID) | REC | FA-04/FA-05/FA-13 |
| 11 | Mando (modelo/conexión) | REC (ESP época: FA-06) | FA-06 |
| 12 | Capturadora externa | ESP época (C) | FA-15 + referencia |
| 13 | Snapshots / estado limpio repetible | REC (IMP si VM) | Todas en B |
| 14 | HW época (PC+Voodoo+Win95/98+CD+joystick) | ESP (C) | FA-15 + referencias |

## Propuesta: MVP y referencia ideal (2026-10-09)

- **MVP para empezar (A):** PC físico Windows 10/11 64-bit + wrapper Glide
  documentado (nGlide 2.10, estándar de facto; dgVoodoo2 para contraste) +
  imagen CD con TOC verificada + captura FPS/vídeo/logs + copias aisladas.
  Permite P0 + P1 observación. Sin HW concreto (no consta).
- **Referencia ideal (C):** HW época 3dfx + Win95/98 + CD físico + joystick
  + capturadora externa. Permite FA-15 y validación «correcto». Modelos a
  decidir por el mantenedor.
- **Hechos confirmados:** base EU-Glide estática (inventario); instalador
  roto en moderno (F-05); rutas MCI/DSound estáticas; wrappers catalogados
  (T-01…T-06) sin probar; Hito 1: F-05 manual + arranque + nivel jugable
  en Win11 64-bit (4ª sesión: ficha + fecha/firma; wrapper pendiente).
- **Recomendaciones:** empezar por A; usar B para repetición P0; reservar C
  para referencia; contrastar 2 wrappers antes de concluir render/timing.
- **Incógnitas:** specs del PC del Hito 1 (build/GPU AMD?/driver); TOC de
  la imagen montada; refrescos soportados; causa admin; origen de la
  `glide2x` (ficha nueva 2.61.00.0658 + nGlide; hash/tamaño pendientes; wrapper=SÍ);
  resto de valores `.reg` (1 vez OK).
- **Decisiones del mantenedor:** completar specs + captura; VM sí/no;
  HW época sí/no (+modelos); herramientas de captura; momento de Fase 2.

## Categorías futuras de prueba

Cuando haya implementación, cubrir por cambio:

- Original behavior · Compatibility · Timing · Rendering
- Resolution · Aspect ratio · Camera
- Input · Controllers · Audio · FMV
- Alt+Tab · Multi-monitor · DPI · Configuration
- Original Mode · Modern Mode · Enhanced Visuals

Principio aplicable: **Original → Fix existente → REZengineered**.

## V-0 + V-1 + V-1b superados (2026-10-10)

- **V-0 (EJECUTADO en PC mantenedor, salida real): PASS.**
  `SysWOW64\glide2x.dll`: 1630208 B, SHA-256 `7cbd…9aab`, MD5
  `f59d9780…`, PE32/i386, `Glide for Voodoo Banshee, Voodoo3, &
  Velocity` 2.61.00.0658; copia `gex-v0-work` idéntica (ambos
  hashes). Metadatos 3dfx = lo esperado (marca del payload nGlide
  2.10, 9ª/S-22), no discrepancia.
- **V-1 (EJECUTADO en PC mantenedor 2026-10-10, MSYS2 MINGW32,
  copia temporal aislada): PASS, exit 0.** GCC 16.1.0, target
  `i686-w64-mingw32`; `V-1 BUILD: PASS`; DLL PE32/i386 temporal
  (SHA-256
  `c62868115001ebfb0d303ad5df6bc49a557e8adf342f0b19df89080c7dea6df4`,
  NO incorporada al repo); veredicto `check_exports.py` modo
  shim-only (lógica auto-probada `test_exports` 17/17): 38/38
  exactas (0 adicionales), 3 de código = interceptadas,
  35 forwarders a `glide2x_gex_real` mismo nombre. Hallazgo F-2 —
  EVALUADO 2026-10-10: benigno y justificado (warning
  `-Wcast-function-type` en `gshim.c:312`, cast de `GetProcAddress`
  a `void (__stdcall *)(int32_t)` para `_grBufferSwap@4`): lint
  determinista sobre el patrón prescrito por la API; firma
  corroborada (SDK `1 4`, `.def`, V-1b 38/38 en DLL real); sin
  defecto. Cast SIN cambios; supresión local solo si llega
  `-Werror` con necesidad real. Límite: estático; invocación
  dinámica pendiente (V-2/P-W5). Estado: evaluado, no corregido;
  código intacto.
- **V-1b (EJECUTADO en PC mantenedor 2026-10-10, MSYS2 MINGW32,
  copia temporal aislada): PASS, exit 0.** `build-win32.sh` con
  copia real verificada como argumento (copia temporal, sin
  instalar, sin Gex): ambas PE32/i386; `check_exports.py` cruce
  completo — 38/38 esperadas (0 adicionales), 3 interceptadas y
  35 forwarders coinciden; shim temporal SHA-256
  `1f234c0a05b851b1d365c7efb1b3945fd21befafebc88cd5c2cb7c43ccb98217`
  (NO incorporado al repo). Criterios superados: arquitectura +
  presencia exacta + resolución de forwarders. Limitaciones: cruce
  estático de tablas de exports; NO prueba carga en proceso, NO
  prueba Gex ni integración en ejecución. F-2 evaluado 2026-10-10
  (benigno y justificado, no corregido); V-2 no iniciado.

## Diagnóstico suite en PC Windows (2026-10-10, sin tocar producción)

- **Síntomas (PC mantenedor, Git Bash):** `run_tests.sh` no
  encontraba python (alias de Store) y c-syntax/behaviour fallaban
  con gcc de 64 bits.
- **Dictamen:** entorno, no regresión. `gshim.c` bloquea 64-bit
  Windows por diseño (`#error` solo bajo `_WIN32` no-x86); la
  suite, en cambio, fallaba donde debía SKIPear y fijaba
  `python3` sin alternativa: hueco real de robustez, corregido.
- **Ajuste mínimo:** `tests/env.sh` (python portable verificado
  por ejecución; etapas de host gated a Linux + gcc nativo con
  SKIP ruidoso; c-syntax clasifica guard=SKIP, resto=FAIL) +
  `test_env` 8/8 hermético (más prueba extremo a extremo
  simulando Store+py+mingw64: suite verde con SKIPs).
  `build-win32.sh` revisado sin defectos. Producción intacta
  (`gshim.c`/`.def` sin tocar). Nota posterior al diagnóstico:
  V-1 superada en PC 2026-10-10 (MSYS2 MINGW32, copia temporal
  aislada; GCC 16.1.0, PE32/i386, 38/38); V-1b superada PC
  (cruce vs DLL real 38/38); F-2 EVALUADO (benigno y
  justificado; ver V-1), código sin modificar.
- **Codificación (mismo PC):** `test_docs.py` reventaba con
  `UnicodeDecodeError` (byte 0x81): leía con la codificación del
  locale (cp1252) dos ficheros UTF-8. Ajuste: `encoding='utf-8'`
  explícito + regresión hermética `test_docs_encoding` 3/3 que
  simula el locale cp1252. El resto pasaba porque `gshim.c`/`.def`
  no contienen bytes indefinidos en cp1252.

## Arnés nativo Win32 W00–W11 (2026-10-10, sin Gex ni DLL real)

- **Qué es:** `tests/win32/` — `harness.c` (padre lanza 1 hijo por
  escenario en dirs temporales), `fakereal.c` (doble de
  `glide2x_gex_real.dll` con conteos en vivo + STRICT:
  finalize-antes-de-reenviar o exit 99), `fakereal_full.def` (las
  3 variantes sin-1-export se derivan con `grep -v`),
  `build-harness.sh` + `run-harness.sh` (etapa 9 de la suite).
- **Decisión de diseño (sin hooks):** la costura de test es la
  propia ruta beside-shim de `do_init`: el hijo carga la DLL de
  PRODUCCIÓN (copia temporal de un `build-win32.sh` fresco; el
  fichero del repo jamás se carga y se compara por bytes tras
  correr) por ruta ABSOLUTA con la fake staged al lado. Loader,
  `GetProcAddress` y `DllMain` corren de verdad en Windows;
  `gshim.c`/`gshim.def`/`build-win32.sh` NO cambian (cero riesgo
  de regresión de producción). Los puntos de inyección `#ifdef`
  se descartaron por cubrir MENOS (falsearían el loader) a cambio
  de tocar producción; si algún día se quiere inyectar fallos de
  QPC/setvbuf en Win32, se añaden entonces (ver P-W2/P-W4).
- **Aislamiento:** todo en `%TEMP%\gshim_w32_<pid>\` (CWD por
  hijo; las 3 salidas del shim son relativas al CWD). El hijo
  verifica `GetModuleFileName` del shim y de la fake contra las
  rutas staged (una DLL ajena solo puede dar FAIL). En el dir de
  build jamás existe una fake con nombre cargable (lo aserta el
  build; protege W07). Supuesto A-1: el PATH no debe contener
  otra `glide2x_gex_real.dll`; si la hubiera, W07 falla ruidoso
  (nunca falso PASS). `git status` debe quedar intacto (se
  comprueba).
- **Comandos exactos (MSYS2 MINGW32; cualquier shell Windows
  vale si el toolchain i686 está en PATH):**
  `pacman -S mingw-w64-i686-gcc mingw-w64-i686-python`
  (una vez); `cd tools/glide-shim`;
  `./tests/win32/run-harness.sh` (construye + chequea exports +
  corre los 14 escenarios) o `./tests/run_tests.sh` (suite
  completa, etapa 9 incluida). Build manual + inspección:
  `./tests/win32/build-harness.sh /tmp/w32build` (imprime el dir;
  warnings C en `build.log`, hay que leerlos).
- **Resultado esperado:** `W32HARNESS BUILD: PASS` +
  `W32HARNESS: 14 passed, 0 failed` + `W32HARNESS: ALL PASS`,
  repo intacto (2× PASS de integridad). Cada escenario imprime
  `PASS <id>` o `FAIL <id>: <motivo concreto>`; códigos: 0
  PASS, 1 FAIL, 111 fail_fast probado (solo W06/W07), 42 crash
  sin shutdown (solo W08), 99 orden violated (siempre FAIL).
  Timeout 180 s/hijo (kill + FAIL).
- **Conducta (14 hijos, solo Windows):** W00 loader+aislamiento
  (rutas staged, DETACH no cambia el csv); W01 exactitud
  (3000/5000/arg3, 3003 filas, 2 P cambio+1/4096, footer exacto,
  cabecera `buf=2048`, ticks dígitos no-decrecientes — DICTAMINA
  F-1 y el pinning MSVCRT); W02 alterno (8001 filas); W03
  overflow (300K swaps, tope 262144 + marcador único +
  `overflow=1` + nota error); W04 NOLOG (csv ausente, marcador
  2 líneas, reenvío intacto); W05 marcador bloqueado (falla
  ruidoso, reenvío intacto); W06×3 resolución (`cannot resolve`
  nombra el símbolo, exit 111); W07 sin fake (exit 111);
  W08 sin shutdown (exit 42, ≥4700/5000 filas, cabecera
  `buf=2048`, sin footer — cota MSVCRT de pérdida); W09 doble
  shutdown (1 footer, 1 X, fake ve 2); W10 log bloqueado
  (degrada ruidoso, reenvío intacto); W11 prebound (fast-path;
  aserción adaptada: conducta idéntica, el «LoadLibrary jamás
  llamado» de B12 no es observable sin hooks).
- **Compilación/exports (NO conducta, etiquetado aparte):**
  `build-harness.sh` paso 3 — shim staged == 38 esperados
  (`check_exports.py --shim-only`, mismo veredicto V-1),
  fakes con exactamente su set + `w32harness.exe` PE32/i386
  (parse inline). En modo compile-only (i686 sin Windows) esto
  PASSa y la conducta SKIPea ruidoso; sin toolchain, SKIP todo.
- **PENDIENTES explícitos (no superados):** P-W1 fallo de
  escritura a mitad de stream (sin inyección de fallos FS
  fiable en Windows stock; la apertura cubierta por W10);
  P-W2 fallo QPC (QPC real; `fail_fast` probado por W06/W07);
  P-W3 prueba B13-equivalente a nivel syscall (necesita
  ptrace/strace; cota MSVCRT por construcción + W01/W08);
  P-W4 setvbuf-rechazado en MSVCRT (CRT no interponible; rama
  cubierta por B14 en Linux; W01 afirma el positivo);
  P-W5 conducta de los 35 forwarders (exige DLL real;
  territorio V-1b/V-2); P-W6 compilación MinGW-32 real +
  corrida de los 14 (este sandbox no tiene MinGW ni Windows:
  C warning-clean vs stub /tmp, parsers ensayados 22/22 con
  los bytes exactos de `harness.c`, suite anfitriona verde y
  SKIP ruidoso verificados aquí; la evidencia Win32 la da el PC).
- **Hallazgo F-1 (revisión, SIN corregir — fuera de alcance):**
  `gshim.c` formatea ticks/contadores con `%llu`/`%lld`, que
  MSVCRT no soporta de forma fiable (gcc avisa `-Wformat`;
  [1](https://sourceforge.net/p/mingw-w64/mailman/mingw-w64-public/thread/20120411101049.GA4263@glandium.org/)).
  En Linux (glibc) es correcto; en el PC, W01 falla ruidoso
  mostrando la fila si MSVCRT lo mangla. Propuesta (requiere
  decisión: cambia conducta Windows de basura-a-correcto):
  `-D__USE_MINGW_ANSI_STDIO=1` en `build-win32.sh`, cero
  cambios de fuente. No se toca producción en este turno.
