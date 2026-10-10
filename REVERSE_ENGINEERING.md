# REVERSE_ENGINEERING — Plan y metodología

> Estado: **Etapa A completada como documentación (Fase 1, 2026-10-08)** —
> binarios EU inventariados + análisis estático básico registrado en
> [docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md).
> RE profundo (Etapas B+) **no iniciado** (bloqueado hasta cambio de fase).

## 1. Principios

- REA como capa de orquestación/evidencia; Ghidra (u otra herramienta
  apropiada) como motor de análisis profundo.
- Todo hallazgo se etiqueta: **Confirmado / Inferido / Experimental /
  Desconocido / Limitación**. Ninguna hipótesis se presenta como hecho.
- Trabajar siempre sobre **copias**; originales intactos y verificados por hash.
- Priorizar sistemas que expliquen problemas de KNOWN_ISSUES.md.

## 2. Objetivos del análisis

1. Inventario de binarios: ejecutables, DLLs, versiones, regiones, hashes
   (SHA-256), fechas, entry points.
2. Dependencias: imports/exports (Fase 1 en EU: Glide ✅, WinMM ✅, DSound ✅;
   Direct3D ❌, DirectInput ❌; Indeo pendiente).
3. Superficie de configuración: strings del registro (`Gex2`, `InstallDir`,
   `CDDriveName`), ficheros de config, argumentos CLI.
4. Sistemas internos: timing/game-loop, render (Glide vs D3D), audio/CD-audio,
   input, CD-check, FMV/intro, gestión de memoria.
5. Diffs binarios: original vs `gex2_patch.zip` vs FPS Limiter vs parche D3D.
6. Mapa función↔problema para cada issue de KNOWN_ISSUES.md.

## 3. Plan por etapas

> Orden de ruta (2026-10-10): B → C mínima (timing → ruta/registro →
> init render) → D mínima (F-02/F-03). Resto pospuesto (ROADMAP Hito K).

### Etapa A — Inventario (sin desensamblar) ✅ (Fase 1, ver inventario §2–§4)

- [x] Recibir y hashear originales (SHA-256) + registrar procedencia.
- [x] `file`, headers PE (máquina, subsistema, timestamp, secciones).
- [x] Tablas de imports/exports y DLLs implícitas.
- [x] Extracción de strings (rutas, claves de registro, mensajes, DX/Glide).
- [~] Comparar variantes: parcial (E-1: ORIG vs EU/US F-03 + imports,
  2026-10-09); US-retail/demo pendientes.

### Etapa B — Proyecto Ghidra + naming inicial

- [ ] Crear proyecto (fuera del repo; artefactos grandes a Drive).
- [ ] Auto-análisis, identificar `WinMain`, game-loop, init/shutdown.
- [ ] Nombrar imports clave y callbacks (ventana, timer, audio).
- [ ] Documentar convenciones en este fichero.

### Etapa C — Sistemas (uno por uno, con engineering log)

- [~] Timing/FPS (I-01, I-02): E-3/M-13 estático acotado 2026-10-10
      (objdump, sin Ghidra/dinámico): sleeps/GetTickCount/contadores/
      divisor 16.666 localizados + flujo temporal gameplay reconstruido
      (ver Avance abajo); + Línea B (lectores `+0x10C` R1/R2, alias
      obj, 75 Hz pedido confirmado en call-site `0x46324D`);
      «30 FPS correcto» aún sin explicar del todo; acoplamiento
      pendiente de prueba dinámica (E-2 listo E-2.3,
      ejecución pendiente).
- [ ] Render (I-03–I-10): ruta Glide única en EU (sin selección D3D en el
      binario); modos de vídeo; petición de 75 Hz CONFIRMADA
      (call-site `0x46324D`: 512x384@75Hz; cine 640x480@60Hz
      `0x537EA6`, Línea B); paths de resolución.
      Selección Glide/D3D solo en US/F-01 (pendiente de binario).
- [~] Audio/CD (I-11–I-13): MCI/WinMM/CD-audio; qué ocurre al cambiar de nivel;
      voces: ruta `<InstallDir>\audio\voiceuk\<n>.sag` CONFIRMADA en
      código 2026-10-10 (`0x53FF0D` + fmt `0x55A708`); housekeeping voz
      `0x53FCB7` decodificada (pump `0x5380B3`/`0x5380A6`, scheduler
      375 frames, array add/remove, reset); tabla `0x5ACBE4` = filas
      por voz (R2 WORD = canal voz, hipótesis fuerte → audio
      wall-clock obligatorio, valida M-13 §4). Selección UK-vs-US
      pendiente (base futura M-25).
- [ ] FMV (I-14): reproductor de intro; dependencia Indeo (hipótesis débil
      tras Lote 1: sugerencia sin validar en hilo VOGONS no resuelto).
      Base futura de M-26 (intro omisible, QoL, P3).
- [~] Instalación/CD-check (I-15–I-17): lecturas de registro DECODIFICADAS
      2026-10-10 (`0x442F41`: `HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00`,
      `InstallDir`+`CDDriveName`, exige REG_SZ; WinMain `0x463281–95`;
      font `<InstallDir>\font.3df` en `0x4329EF` → causa I-23);
      detección de CD (`X:\` + error frontal si falta).
- [ ] Input (I-18–I-19): teclado + joystick WinMM (sin imports DirectInput
      en EU); confirmar ausencia de ratón.
- [ ] Menú debug (F-12, Lote 1): localizar el presunto debug menu de
      desarrolladores en la build D3D (la fuente afirma acceso con F1 vía
      memory patch; sin verificar por el proyecto) — superficie RE potencial.

> **Avance E-3/M-13 — flujo temporal gameplay (2026-10-10, estático
> acotado, solo lectura; exe EU md5 `692b1282…`, copia eliminada;
> nada ejecutado/modificado).** Método y candidatos: TESTING E-3.
> Nota metodología: `objdump` lineal desincroniza junto a
> `0x412F8B` (tabla de datos); resincronizado con `--start-address`.
>
> | Dirección | Función observada | Evidencia | Dependencias | Dudas |
> |---|---|---|---|---|
> | `0x43B36D`/`0x43B373` (en `0x43B11D` ← `0x463A29`, estado 3) | Init temporal (1×/nivel): `[obj+0x10C]=0`, `t0=GetTickCount()`→`ds:0x57B5FC`, `[obj+0x108]=0`, `[obj+0x104]=0` | Disasm (único escritor t0) | obj=`ds:0x551640` | — |
> | `0x43E5DD`/`0x43E5E3` (en `0x43E1BA` ← `0x43EA36` ← `0x463AAA`, estado 2) | Sello por frame: `elapsed=now-t0`, `/16.666` (doble `0x54C060` VERIFICADO) → `_ftol` → `[obj+0x10C]`; `[obj+0x104]++` | Disasm (único lector t0; `0x43E1BA`/`0x43EA36` INDEPENDIENTES verificadas) | t0 `0x57B5FC` | Lectores `+0x10C`: ver filas R1/R2 (Línea B; «ninguno» SUPERSEDE) |
> | `0x43E1BA` (`0x43E1BA`–`0x43E63C`, 1 llamador) | Update por frame: ~20 subsistemas + doble-buffer `+0x14` + sello final | Disasm completo | — | Rol de cada subsistema (mov/anim/cám/fís/input/cine sin mapear) |
> | `0x43F80D` (pre-update; 3 efectivos + 1 muerto) | OSD «DEMO MODE» (`0x55029C/2A8`); si `+0x104==0` → `ds:0x57B63C=0` (1er frame) | Disasm + strings + barrido llamadores 2026-10-10 | Contador `+0x104` | `ds:0x57B63C` SIN lectores (2 escritores) = muerto/vestigial; llamadores: frame `0x43EA3D`, init `0x462B35`, cine `0x537BBB`; `0x4D5763` muerto (0 llamadores) |
> | `0x43DC3A` (en update si `+0x4E38==0`; +llamador `0x424B0D`) | Instala callbacks render + envía 2 listas (pasa contador a `0x473238`) + drena `grBufferNumPending` | Disasm | Contador `+0x104` | `0x473238` (mipmaps/texturas) IGNORA el contador (sin `[ebp+0x10]`) |
> | `0x43EA51`/`0x43EA5E` (cola `0x43EA36`) | Espera `grBufferNumPending→0` + `grBufferSwap(3)`; luego no-op `0x46300C` | Disasm + IAT Glide | Presentación | — |
> | `0x53FCB7` (post-frame; decode completo 2026-10-10) | Housekeeping VOZ: puerta `0x54038E`, pump `0x5380B3`/`0x5380A6`, cuenta 375 `ds:0x5AF3E0`→scheduler `0x53FD31`, array voces add/remove, reset `0x53FE1A` | Disasm `0x53FCB7–7F` | Reloj pared (NO tocar en M-13) | — |
> | `0x4622D7` (en `0x461FBD` ← init `0x43B1FE`) | Present durante init: `grBufferSwap(2)` + `+0x104` de OTRO objeto (`ds:0x551644`) | Disasm | — | Rol exacto (inferencia: pantalla carga) |
> | `0x424C7F` / `0x537E6E` | `grBufferSwap(1)` (ruta `0x424C0F`; wrapper cine `0x537E69`) | Disasm | — | Roles exactos |
> | `0x418FF8` (fn2 registro «tvend» `@0x55336C`; tabla 60 regs `@0x5531C8`–`@0x553678`) | Handler pantalla TV (IDs «remred__»/«etvbutn_»); escribe `[obj+0x10C]=0x10` (`0x4190D5`); sib `0x418C52` escribe `+0x104` | Disasm + barrido byte-exacto: 0 llamadores directos; 0 refs estáticas a la tabla (108/110 fns huérfanas; 2 markey solo intra-familia) | Tabla pantallas (ajena a gameplay) | Cobertura dinámica (¿runtime la alcanza?) — requiere E-2 |
> | `0x412F8B` (← `0x43E516`) | Cuenta atrás por frame (`+0x4BC−−`) + print = lógica frame-acoplada | Disasm resync | — | — |
> | `ds:0x551640`=`ds:0x551644`→`0x648900` (Línea B) | MISMO objeto: init estático idéntico (BSS ceros); 0 stores estáticos a ninguno → alias válido estáticamente | Bytes `.data` + grep stores (37 refs `0x551640`, 0 escrituras; `0x551644` ~100s lecturas, 0 escrituras) | obj timing | Re-apunte runtime no excluible estáticamente |
> | `0x46324D` / `0x537EA6` (Línea B) | `grSstWinOpen`: juego=(512x384, 75 Hz, 2 col, 0 aux); cine=(640x480, 60 Hz, …) | Disasm pushes + enums `sst1vid.h` sst1/cvg/h3 (coinciden) en sezero/glide | Presentación | Tasa REAL runtime (driver) sin confirmar |
> | R1 `0x53FED4` (←`0x53FE80`←`0x540036`/`0x540301`←`0x5401CD` frame; cond.) | Lee cociente `[0x551644+0x10C]` → snapshot `ds:0x5ACBD4` (1W/1R cerrado) | Disasm + cadena llamadores + grep refs | Cociente `+0x10C`, flags modo | Condiciones de disparo en runtime |
> | R2 `0x451A0C` (←`0x4519DE`←`0x452368`←`0x47344B`←`0x43E31B` frame; cond.) | `delta=((Q−snap)>>1)+1`, clamp `[0,lim−1]`, `tabla[0x5ACBE5+delta]<<2` → WORD `[[ebp+0x10]]+0x10` (gates off → 0) | Disasm `0x4519DE`–`0x451A65` | Q, snap, modo `0x5ACBC4==3`, flag `0x5ACBC3` | Semántica fina + obj 3er arg (HIPÓTESIS FUERTE 2026-10-10: tabla `0x5ACBE4` = filas por voz, WORD = canal voz — sin probar) |
> | `ds:0x5ACBC4`/`ds:0x5ACBC3` (Línea B) | Modo {0,3} (writers solo `0x53FCxx`; 17 cmps incl. frame `0x423C59`,`0x43D85D`,`0x4639C1`) + byte flag (1@`0x540324`/0@`0x54033C`) | Grep refs exhaustivo | R1/R2 | Significado modo 3 vs 0 |
> | `0x423CFE`→`ds:0x65CD30`→`0x423EB8` (Línea B) | Snapshot/restore contador `+0x104` vía BSS (1W/1R cerrado) | Disasm | Contador | Momento/condición restore |
> | `0x4721B6`, `0x43DCEE` (Línea B) | Lectores contador extra: `+0x104`+WORD→arg (`0x4721B6`); 2ª lectura→2ª llamada `0x473238` (arg muerto ×2) | Disasm | Contador | — |
> | `0x53FF0D` + fmt `0x55A708` (2026-10-10) | Ruta voz `<InstallDir>\audio\voiceuk\<n>.sag`; `AUDIO/VOICEUK/` 400 `.SAG` en disco (inventario §6/§8) | Strings + disasm + inventario | InstallDir (I-23) | Selección UK-vs-US |
>
> Flujo temporal (gameplay, estado 2; SERIE, sin hilos observados):
>
> ```text
> [Estado 3, 1x/nivel] 0x463A29 -> 0x43B11D: +0x10C=0, t0=ahora, +0x108=0, +0x104=0
>   -> estado 2 --+
> [Por frame]     | 0x463AAA -> 0x43EA36(ds:0x551640):
>   pre-update    |   0x43F80D (DEMO-MODE; 1er frame si +0x104==0; 0x43EA70/0x43F51D/0x43F94F)
>   update+sello  |   0x43E1BA (~20 subsistemas; 0x43DC3A si +0x4E38==0;
>   |             |    sello 16.666->+0x10C, +0x104++)
>   cociente R1/R2|   R1: Q->snap ds:0x5ACBD4 (0x53FE80, cond. modo);
>   |             |    R2: delta=(Q-snap)>>1+1->tabla->WORD (0x4519DE, cond.)
>   presentacion  |   spin grBufferNumPending->0; grBufferSwap(3); no-op 0x46300C
>   housekeeping  |   0x53FCB7 (pump + cuenta 375)
>                 +-> repetir (sim+render comparten flujo; sin 2o dominio temporal)
> ```
>
> **Separación sim/render:** NO observada. Update (incluye envío render
> `0x43DC3A`) → drenaje cola → swap → casa, todo serie. La sim avanza
> 1× por swap presentado (acoplamiento serie evidenciado;
> cuantificación exacta pendiente de dinámica).
>
> **Throttle presentación (spec Glide 2.2/3.0 + IMPL `gglide.c` sezero/glide):**
> `swap_interval` = retraces a esperar (60 Hz+3 → máx 20 FPS);
> `grBufferSwap` encola y retorna (drena interno solo si >6
> pendientes); el freno real es el spin-a-cero del frame siguiente
> (`0x43EA51`): la sim no avanza hasta retirar el swap (3 retraces).
> Juego: gameplay=3, init=2, resto=1. 75 Hz/512x384 PEDIDOS
> confirmados en call-site `0x46324D` (enums `sst1vid.h`): 75/3 =
> **25 FPS máx** = coincide con ≈25 estables Hito 1 (INFERENCIA
> reforzada, no prueba: tasa real en runtime sin confirmar).
>
> **Sabemos (estático):** init t0 + resets; sello 16.666 + contador por
> frame; orden serie update→swap→casa; 4 puntos de present (3/2/1/1);
> consumidores efectivos (check 1er frame; cuenta atrás `0x412F8B`;
> cuenta 375; R1 snapshot cociente; R2 delta→tabla→WORD) y falsos
> (flag sin lectores; contador ignorado por `0x473238` ×2);
> alias `0x551640`=`0x551644`; snapshot/restore contador
> (`ds:0x65CD30`); modos `ds:0x5ACBC4`∈{0,3}; `+0x14` doble-buffer;
> causa I-23 (cadena InstallDir→`font.3df`); dominio R-audio
> (ruta voz + housekeeping `0x53FCB7` + tabla `0x5ACBE4`);
> llamadores `0x43F80D` (frame/init/cine + 1 muerto); IAT Glide
> 38 imports con slots (`0x65F45C–0x65F4F0`).
>
> **NO podemos demostrar estáticamente:** qué subsistema mueve cada
> cosa (mov/anim/cám/fís/input/cine); semántica fina de la tabla
> `0x5ACBE4` + identidad del 3er arg de R2 (hipótesis fuerte: filas
> por voz — sin probar);
> refs computadas en runtime (alias/re-apuntes); tasa real de
> refresco en runtime; mecanismo F-03 (build distinta, no tocada);
> acoplamiento cuantitativo (E-2.3 pendiente ejecución); cobertura
> runtime de la tabla
> de pantallas (`0x418FF8` DESCARTADO estáticamente: campos
> propios, ver fila).
>
> **Próximo paso mínimo y seguro (sin intervención binaria):**
> E-2.3 en PC mantenedor (I-23 causa identificada, método reparado;
> confirmar sim-por-swap + cobertura tabla pantallas + disparo
> R1/R2); el campo `+0x10C`
> ya computado es la entrada natural de un futuro paso-fijo —
> pero R2 lo CONSUME (delta→tabla), así que el diseño debe
> preservar esa ruta (ver boceto).
>
> **Boceto desacoplado MÍNIMO (PROPUESTA de diseño, NO implementar;
> desbloqueo FPS prohibido regla 21):** (a) conservar productor
> Q=`elapsed/16.666` por paso-sim; (b) acumulador punto-fijo
> (patrón soul-re `GAMELOOP_DoTimeProcess`: `timeMult`→`while(>=
> unidad){paso-sim}`) con N pasos por present (patrón TRX
> `PhaseExecutor`: `nframes=WaitTick()`→N×`Control()`→`Draw()`
> + interpolado opcional); (c) snapshot Q por present (rol R1) y
> delta R2 derivado del tiempo acumulado (NO del contador de
> swaps); (d) throttle de presentación intacto hasta Origin Mode
> verificado. **Riesgos:** semántica tabla `0x5ACBE5` desconocida
> (¿pitch audio? ¿anim?); ruta R1/R2 solo modo 3; args muertos a
> `0x473238`; `Swap(3)`+drain-a-cero = latencia 3 retraces/frame.
> **Incógnitas:** tasa real runtime; disparo R1/R2; orden
> subsistemas. **Primer paso reversible:** shim instrumentación
> `tools/glide-shim` v6 (setvbuf-comprobado 2026-10-10, suites verdes
> estructural + conductual + traza syscall + setvbuf-forzado; V-0
> superado PC, V-1 superado PC (38/38; F-2 pendiente); V-1b
> pendiente; arnés W00–W11 preparado, pendiente PC) +
> E-2.3; núcleo acumulador verificado en harness
> (`tools/m13-core`, 120 checks); el desacoplado
> se implementa en el PROTOTIPO Fase 1+ (Hito P), no en el exe.
>
> **Línea A (refs externas, RESEARCH S-26):** estudiar
> `soul-re/src/Game/GAMELOOP.c` (familia Gex-2: gating por
> `gameFramePassed` + flag `decoupleGame`) y
> `TRX/src/trx/game/{clock,phase}` (paso-fijo + interpolación);
> `sezero/glide` ya minado (semántica swap); KAIN2-PC no (sin
> Glide, obsoleto). Sin suponer motor compartido.
> Insumo Fase 3: propuesta completa en
> `docs/M13_DECOUPLE_PROPOSAL.md` (2026-10-10; diseño, no implementar).

### Etapa D — Diffs de parches comunitarios

- [ ] Por cada fix (F-01…F-12, según tipo; F-04 pendiente de fusión con
      F-02): qué bytes/funciones cambian y qué efecto tienen.
- [ ] Exe parcheado S-14 (2013, NTSC-D3D, md5
      `0a65f3ada9bef8c842e52cf0a7987f67`, solo metadatos): candidato a
      diff vs original (Fase 2).
- [ ] Tabla comparativa **Original → Fix → Hipótesis de causa → Propuesta**.

## 4. Herramientas

Ver [docs/TOOLKIT.md](docs/TOOLKIT.md). Resumen: binutils disponibles en este
entorno; Ghidra + Java pendientes de instalar; REA (CLI/MCP, trae su propio
backend vía Hopper o Ghidra aportado) pendiente de evaluar; testing dinámico
requiere Windows (limitación actual).

## 5. Registro

Cada sistema/problema relevante genera una entrada de engineering log según
[docs/ENGINEERING_LOG_TEMPLATE.md](docs/ENGINEERING_LOG_TEMPLATE.md), que se
archivará en `docs/engineering-log/` cuando exista.

## 6. Relación con MODERNIZATION_GOALS

- Las Etapas A–C cubren FA-01…FA-15 (fundamentos). Sin ellas, ningún M-xx
  puede pasar de PROPOSED a PLANNED.
- La Etapa D (diffs de parches) alimenta directamente FA-03/FA-05/FA-10/FA-11
  y los objetivos M-04, M-13, M-01…M-03 (ver PATCH_ANALYSIS.md § Relación).
- El orden de implementación vive en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md)
  y [ROADMAP.md](ROADMAP.md): este documento define el *cómo* del análisis,
  no el *cuándo* de las features.
