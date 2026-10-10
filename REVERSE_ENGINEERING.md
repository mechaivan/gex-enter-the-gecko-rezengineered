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
      (ver Avance abajo); «30 FPS correcto» aún sin explicar del todo;
      acoplamiento pendiente de prueba dinámica (E-2 bloqueado).
- [ ] Render (I-03–I-10): ruta Glide única en EU (sin selección D3D en el
      binario); modos de vídeo; petición de 75 Hz; paths de resolución.
      Selección Glide/D3D solo en US/F-01 (pendiente de binario).
- [ ] Audio/CD (I-11–I-13): MCI/WinMM/CD-audio; qué ocurre al cambiar de nivel;
      voces `VOICEUK/` vs `voice/` + mecanismo de selección (base futura M-25).
- [ ] FMV (I-14): reproductor de intro; dependencia Indeo (hipótesis débil
      tras Lote 1: sugerencia sin validar en hilo VOGONS no resuelto).
      Base futura de M-26 (intro omisible, QoL, P3).
- [ ] Instalación/CD-check (I-15–I-17): lecturas de registro; detección de CD.
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
> | `0x43E5DD`/`0x43E5E3` (en `0x43E1BA` ← `0x43EA36` ← `0x463AAA`, estado 2) | Sello por frame: `elapsed=now-t0`, `/16.666` (doble `0x54C060` VERIFICADO) → `_ftol` → `[obj+0x10C]`; `[obj+0x104]++` | Disasm (único lector t0; `0x43E1BA`/`0x43EA36` INDEPENDIENTES verificadas) | t0 `0x57B5FC` | Lectores `+0x10C`: NINGUNO en ámbito acotado |
> | `0x43E1BA` (`0x43E1BA`–`0x43E63C`, 1 llamador) | Update por frame: ~20 subsistemas + doble-buffer `+0x14` + sello final | Disasm completo | — | Rol de cada subsistema (mov/anim/cám/fís/input/cine sin mapear) |
> | `0x43F80D` (pre-update; 4 llamadores) | OSD «DEMO MODE» (`0x55029C/2A8`); si `+0x104==0` → `ds:0x57B63C=0` (1er frame) | Disasm + strings | Contador `+0x104` | `ds:0x57B63C` SIN lectores (2 escritores) = muerto/vestigial |
> | `0x43DC3A` (en update si `+0x4E38==0`; +llamador `0x424B0D`) | Instala callbacks render + envía 2 listas (pasa contador a `0x473238`) + drena `grBufferNumPending` | Disasm | Contador `+0x104` | `0x473238` (mipmaps/texturas) IGNORA el contador (sin `[ebp+0x10]`) |
> | `0x43EA51`/`0x43EA5E` (cola `0x43EA36`) | Espera `grBufferNumPending→0` + `grBufferSwap(3)`; luego no-op `0x46300C` | Disasm + IAT Glide | Presentación | — |
> | `0x53FCB7` (post-frame) | Housekeeping: puerta `0x54038E`, pump `0x5380B3`/`0x5380A6`, cuenta atrás 375 frames `ds:0x5AF3E0`→`0x53FD31` | Disasm | — | — |
> | `0x4622D7` (en `0x461FBD` ← init `0x43B1FE`) | Present durante init: `grBufferSwap(2)` + `+0x104` de OTRO objeto (`ds:0x551644`) | Disasm | — | Rol exacto (inferencia: pantalla carga) |
> | `0x424C7F` / `0x537E6E` | `grBufferSwap(1)` (ruta `0x424C0F`; wrapper cine `0x537E69`) | Disasm | — | Roles exactos |
> | `0x418FF8` (fn2 registro «tvend» `@0x55336C`; tabla 60 regs `@0x5531C8`–`@0x553678`) | Handler pantalla TV (IDs «remred__»/«etvbutn_»); escribe `[obj+0x10C]=0x10` (`0x4190D5`); sib `0x418C52` escribe `+0x104` | Disasm + barrido byte-exacto: 0 llamadores directos; 0 refs estáticas a la tabla (108/110 fns huérfanas; 2 markey solo intra-familia) | Tabla pantallas (ajena a gameplay) | Cobertura dinámica (¿runtime la alcanza?) — requiere E-2 |
> | `0x412F8B` (← `0x43E516`) | Cuenta atrás por frame (`+0x4BC−−`) + print = lógica frame-acoplada | Disasm resync | — | — |
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
> **Throttle presentación (spec pública Glide 2.2/3.0 Ref Manual):**
> `swap_interval` = retraces verticales a esperar (60 Hz+3 → máx 20
> FPS). Juego: gameplay=3, init=2, resto=1. A 75 Hz (tasa pedida,
> FA-05): 75/3 = **25 FPS máx** = coincide con ≈25 estables Hito 1
> (INFERENCIA fuerte, no prueba: tasa real en runtime sin confirmar).
>
> **Sabemos (estático):** init t0 + resets; sello 16.666 + contador por
> frame; orden serie update→swap→casa; 4 puntos de present (3/2/1/1);
> consumidores efectivos (check 1er frame; cuenta atrás `0x412F8B`;
> cuenta 375) y falsos (flag sin lectores; contador ignorado por
> `0x473238`); `+0x10C` sin lectores en ámbito; `+0x14` doble-buffer.
>
> **NO podemos demostrar estáticamente:** qué subsistema mueve cada
> cosa (mov/anim/cám/fís/input/cine); si el cociente `+0x10C` se lee
> fuera del ámbito (dataflow global pendiente; `0x418FF8` DESCARTADO:
> campos propios, ver fila); tasa real de refresco en runtime;
> mecanismo F-03 (build distinta, no tocada); acoplamiento
> cuantitativo (E-2); cobertura runtime de la tabla de pantallas.
>
> **Próximo paso mínimo y seguro (sin intervención binaria):**
> E-2 dinámico cuando I-23 lo permita (confirmar sim-por-swap +
> cobertura tabla pantallas); el campo `+0x10C` (frames fraccionales
> desde t0) ya computado es la entrada natural de un futuro
> paso-fijo (diseño Fase 3, no ahora).

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
