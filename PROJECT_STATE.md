# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE started (Etapa A documented in Phase 1; bounded static E-3/M-13 + Línea A/B executed read-only 2026-10-10 under explicit authorization - no Ghidra/decomp; static function map + call graph + per-function imports on the EU exe 2026-10-25, `docs/FUNCTION_MAP.md`; first evidence-based subsystem segmentation 2026-10-30, `docs/SUBSYSTEM_SEGMENTS.md`; dynamic frame-step trace with hardware breakpoints 2026-10-30, `docs/FRAME_STEP_TRACE.md`; static DirectSound init routes 2026-10-31, `docs/AUDIO_INIT_PATHS.md`)
**Implementation:** Started 2026-10-31 — native port skeleton in `port/` (C11, no dependencies): reader for the `SLAG` audio container (`0x0053899C` + loader `0x005388AC`), 29 test cases + fuzz, 69 305 checks, `docs/SAG_CONTAINER.md`; **audio route** implemented the same day (`0x00538285` IMA ADPCM decoder + `0x005380B3` streaming pump + `0x00538731` player) with an injectable backend, 60 052 + 144 checks, `docs/AUDIO_MIXER_RE.md`; **audible audio** added 2026-11-01 (I-2b): real DirectSound backend (`dsound.h`/`-ldsound`, no new dependency) and the frame-loop scheduler, 215 + 50 checks including the device suite, `docs/AUDIO_MIXER_RE.md` §16. **No playable build**: Hito P / MAIN unchanged; these are components, not the port
**Testing:** Hito 1 + 5ª–9ª sesión (2026-10-09, PC mantenedor; nGlide 2.10, ≈25 FPS Steam, wrapper = nGlide 2.10 CONFIRMADO por payload-hash, imports solo-sistema/backend dinámico; A2 46%, global 20%; resto pendiente) + smoke in-game H4a (2026-10-11, PC mantenedor: arranque real hasta menú; registro restaurado)
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-11-01

## Ampliación de objetivos (2026-10-08, solo documentación)

- Se ha ampliado el conjunto de objetivos futuros: FA-01…FA-15 (fundamentos),
  M-01…M-26 (modernización), X-01 (deferred) y X-02 (long-term research).
  Ver MODERNIZATION_GOALS.md.
- **Ninguna feature está implementada.** Todas están PROPOSED / TO INVESTIGATE
  (X-01/X-02 DEFERRED).
- La prioridad actual es la Fase 1 / Research (Fase 0 cerrada 2026-10-08).
- No se debe saltar a implementación: la documentación precede a cualquier
  modificación técnica.
- Orden: ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL.
- 2026-10-08: +M-24 localización (PROPOSED, P3, sin fase) y +X-02 posible
  rewrite Rust/Bevy (DEFERRED, long-term research, fuera de implementación).
- 2026-10-09: +M-25 voice pack selection UK/USA (PROPOSED, P3, sin fase).
- 2026-10-09: +M-26 skippable intro cinematic (PROPOSED, P3, sin fase).

## Current objective

> Understand the original PC version of Gex: Enter the Gecko and existing
> community fixes before implementing any modifications.
> Ruta corta (2026-10-10): Hito R → Hito K → Hito P (= MAIN 100%) →
> Olas 1–3. Ver ROADMAP.
>
> Cambio de fase (2026-10-31): se abre una **vía de implementación nativa** en
> paralelo a la investigación. El objetivo ya no es solo entender el binario,
> sino tener componentes reales, probados sin el juego, que se puedan integrar
> después. El primer componente es el lector del contenedor de audio `SLAG`
> (`docs/SAG_CONTAINER.md`, código en `port/`), seguido el mismo día por la ruta
> de audio completa —decodificador IMA ADPCM, bomba de streaming y reproductor—
> (`docs/AUDIO_MIXER_RE.md`). Esta vía **no** promociona Hito P ni MAIN: el panel
> sigue midiendo la ruta de mods/parches.
>
> 2026-11-01 (I-2b): la ruta de audio ya es **audible** en el port. Un *backend*
> DirectSound real (la API del original, sin dependencias nuevas) sustituye al
> WAV como salida y `gex_audio_sched` reproduce el contrato de los llamantes de
> `0x005380B3` en el frame loop. Verificado en el dispositivo de esta máquina
> (cursor a 44 100 B/s, contenido leído por `Lock` idéntico al decodificado);
> la audición humana no la puede comprobar el agente.

## Vía de implementación nativa (abierta 2026-10-31)

Decisión del mantenedor: empezar a implementar sin esperar al cierre de la
Fase 3, eligiendo piezas cuyo comportamiento ya está demostrado por
instrucción. Condiciones que debe cumplir cada pieza:

1. semántica completa del original (sin bucles internos sin leer);
2. dependencias de plataforma nulas o mínimas;
3. prueba posible sin el juego completo ni assets originales;
4. consumidor identificado en el motor.

Estado:

| Pieza | Original | Código | Prueba |
| --- | --- | --- | --- |
| Lector `SLAG` (formato + payload `data`) | `0x0053899C`, `0x005388AC`, `0x00538B01`, `0x00538409`, `0x00538A99`, `0x00538ACA` | `port/src/asset/sag.c` | `port/tests/test_sag.c` (69 305 checks, 0 fallos) |
| Decodificador IMA ADPCM de 4 bits | `0x00538285`, tablas `0x00558138`/`0x00558178` | `port/src/audio/adpcm.c` | `port/tests/test_adpcm.c` (60 052 checks, 0 fallos) |
| Bomba de streaming + reproductor | `0x005380B3`, `0x00538731`, `0x00538409`, `0x005384A3`, `0x005384ED`, `0x00538040/68/75/A6` | `port/src/audio/stream.c` | `port/tests/test_audio.c` (215 checks, 0 fallos) |
| *Backend* de salida inyectable | `IDirectSoundBuffer` (Lock/Unlock/Play/Stop/GetCurrentPosition) | `gex_audio_backend` | captura en memoria + WAV (`port/tools/sagplay.c`) |
| *Backend* de dispositivo DirectSound | `0x00538551` (dispositivo), `CreateSoundBuffer(0x8000, 0xE0)`, `GetCurrentPosition`, `SetVolume` `0x0053886c` | `port/src/audio/backend_dsound.c` | `port/tests/test_device.c` (50 checks, 0 fallos, se omite sin tarjeta) |
| Programación del bombeo (frame loop) | llamantes `0x00463205`, `0x0053A5FB`, `0x0053FCB7`; prefill `0x00538842` | `port/src/audio/sched.c` | `port/tests/test_audio.c` (planificador) |

Límites de esta vía, explícitos: no hay ficheros `.sag` reales disponibles y la
equivalencia con el original es estática (transcripción instrucción a
instrucción). La salida a hardware **sí** está implementada y medida desde
I-2b (2026-11-01): `test_device` y `sagplay --device` entregan PCM a un
dispositivo DirectSound real y comprueban que el cursor avanza a la frecuencia
del flujo. Lo que **no** está verificado es la audición humana: el agente no
tiene oídos, así que la confirmación por oído queda para el mantenedor con
`sagplay --device`. Ver `docs/SAG_CONTAINER.md` §6/§9 y
`docs/AUDIO_MIXER_RE.md` §11/§14/§16.

## Checklist de la Fase 0 ✅ (cerrada 2026-10-08)

- [x] Definir objetivos y principios (README, ROADMAP).
- [x] Crear estructura inicial del repositorio.
- [x] Recopilación inicial de fuentes públicas (PCGamingWiki, foros, proyectos).
- [x] Mapa inicial de problemas conocidos (sin verificar — ver KNOWN_ISSUES.md).
- [x] Catálogo inicial de fixes comunitarios (sin analizar técnicamente).
- [x] Plan de reverse engineering (ver REVERSE_ENGINEERING.md).
- [x] Inventario de herramientas disponibles en este entorno.
- [x] Recibir y revisar los enlaces/recursos pendientes del mantenedor
  (PCGamingWiki, REA, Gex64Decomp-secundaria, speedrun/setup — jerarquía
  fijada en RESEARCH.md §0).
- [x] Crear estructura de carpetas en Drive (originales, backups, análisis…).
- [x] Recibir archivos originales del mantenedor → inventariados en Fase 1
  (ver `docs/ORIGINAL_ARTIFACT_INVENTORY.md`).

## Checklist de la Fase 1 🟡 (en curso)

- [x] Inventariar archivos originales: edición EU v1.00.000 completa
  (1585 ficheros, hashes, TOC, instalador + ejecutable identificados).
  Variantes US/demo/parcheadas: pendientes.
- [~] Documentar la versión PC (ejecutables, dependencias, registro,
  CD-audio): estático hecho; dinámico pendiente.
- [~] Problemas conocidos con evidencia: estática propia añadida;
  primeras observaciones propias (Hito 1: I-15/I-16 parciales,
  I-11 no reproducido, I-21/I-22/I-23 nuevos).
- [~] Analizar fixes existentes: Lote 1 (2026-10-09) con descripciones
  F-01…F-12 verificadas en fuente + revisión cruzada + S-14 (2026-10-09);
  diffs binarios pendientes (Fase 2); Lote 2 (2026-10-09): F-13 oficial
  + F-01/F-03/F-10 profundizados (S-23; binarios pendientes).
- [x] Cubrir FA-01…FA-15 a nivel documental: matriz de cobertura
  (2026-10-09); cobertura exhaustiva CERRADA 2026-10-10 (resto por
  implementación, ROADMAP R→K→P).
- [x] Inventariar S-14 (setup package speedrun): metadatos + hashes +
  inventario nominal (22 entradas) + README leído (2026-10-09, carpeta
  `S-14_extracted`, solo lectura). Análisis binario → Fase 2.
- [~] Preparar/validar entorno de RE (Ghidra o alternativa + proyecto).
  Alternativa estática operativa desde 2026-10-25: `tools/gexmap` +
  `docs/FUNCTION_MAP.md` (2 655 inicios, 5 097 aristas, 132 stubs de import,
  1 113 candidatos por prólogo = HYPOTHESIS). Primera segmentación por
  subsistemas 2026-10-30: 65 inicios en 7 grupos HYPOTHESIS, con evidencia y
  confianza validadas (`docs/SUBSYSTEM_SEGMENTS.md`; 35 ALTA / 22 MEDIA /
  8 BAJA). Traza dinámica del frame step 2026-10-30: 2 326 frames
  instrumentados con breakpoints de hardware, 25 sitios del update medidos
  (23 de 24 callees 1 vez/frame), 19 destinos con semántica mecánica
  (`docs/FRAME_STEP_TRACE.md`, `tools/gexmap/frame.mjs`, `tools/frame-trace/`).
  Rutas de inicialización de DirectSound 2026-10-31 (estático): tres cadenas
  alcanzables desde la entrada (`0x00538551` con dos llamantes) y una cuarta
  sin llamantes (`0x00538FD5` -> `0x00538CB0`); datos en `tools/gexmap/audio.mjs`,
  análisis en `docs/AUDIO_INIT_PATHS.md`. No hay traza: la imagen de referencia
  ya no está en la máquina de análisis.
  Ghidra y decompilación siguen pendientes; el mapa es prerrequisito
  del splitter, no un sustituto.
- [~] Definir entorno de testing en Windows: protocolo + matriz FA +
  evaluación de alternativas + requisitos/checklist (TESTING.md,
  2026-10-09); Hito 1 ejecutado en PC del mantenedor (Win11 64-bit;
  build/GPU/driver pendientes). Protocolo P-F03 (A/B F-03) preparado
  (A1–A4); rama B autorizada y ejecutada parcial
  (42–44 FPS + speedup, observación 2026-10-09); E-1 ejecutado
  (E-1.1 identidad confirmada sha256); E-2 LISTO (método E-2.3
  reparado 2026-10-10, ejecución pendiente PC mantenedor);
  E-3 ejecutado (2026-10-10, estático solo-lectura, sin
  ejecutar/modificar) + M-13 flujo temporal estático (TESTING,
  REVERSE_ENGINEERING) + I-23 causa identificada + R-audio
  confirmado + `tools/m13-core` verificado (120 checks) +
  `tools/glide-shim` v6 setvbuf-comprobado (suites verdes 2026-10-10:
  14+3+74+3+3+17+11 + 71 conductual; V-0 superado PC; suite portable;
  V-1 superado PC 2026-10-10 (GCC 16.1.0, 38/38, F-2 evaluado benigno);
  F-3 H4a verificado PC (mismatch-alias + sin dllexport = 38);
  integrado en árbol, V-1 H4a superado PC (38/38; log y SHA
  archivados 2026-10-11 en `Reports/H4a/`); V-1b superado PC
  2026-10-10
  (cruce vs DLL real 38/38); V-2 SUPERADO PC 2026-10-11 (sobre
  `0c22556`: `W32HARNESS: 14 passed, 0 failed` + `ALL PASS` +
  `SUITE: ALL PASS` + `V2_RC=0`; log SHA-256 `3ff4672a…`);
  P-W5 SUPERADO PC 2026-10-11 (probe aislado de resolución de
  exports: 3/3 hooks → shim, 35/35 forwarders → `glide2x_gex_real`,
  exit 0; sin llamadas y sin Gex); evidencias P-W5 ya archivadas en
  `REZengineered/Reports/` (log SHA-256 `c630759a…`, recomputado
  2026-10-11 sobre la copia archivada: coincide; +
  binario/fuente/lista/informe). Evidencias V-1 H4a/V-1b,
  matrix2 y V-2 archivadas 2026-10-11 en
  `REZengineered/Reports/{H4a,V1b,Matrix2,V2}/` (completo:
  también el `exports_shim.txt` del run H4a, 2026-10-11 —
  SHA-256 `9b9170a6…` —, nunca mezclado con el de V-1b).
  Smoke in-game H4a SUPERADO PC 2026-10-11 (arranque real hasta
  menú, copia aislada, salida 0; registro restaurado). Pendiente:
  tanda instrumentada in-game (60 s; footer/overflow) y estabilidad
  en partida.)
- [x] Referencia externa documentada (2026-10-11): reconstrucción de
  Soul Reaver PS1 (`fmil95/soul-re`, MIT, commit `59d1ee8…`)
  analizada como referencia comparativa del motor (candidato de
  linaje «Gex 2»); informe en
  `docs/research/SOUL_RE_ENGINE_REFERENCE.md` (solo documentación;
  sin verificación en el exe PC; sin cambios de código ni binarios).
- [x] Referencia externa documentada (2026-10-25): decompilación de
  **Gex (1996) para Windows** (`Hydroloxed/gexdecomp`, sin licencia;
  `main` @ `8ba80780`) como referencia secundaria de la familia Gex
  (RESEARCH S-27); sin código copiado, sin binarios y sin trasladar
  direcciones a `GEX3D.EXE`.

## Limitaciones actuales

- **Originales inventariados; copia de trabajo verificada (fuera del
  repo):** `originals/` en Drive (volcado CloneCD + contenido del CD)
  catalogado en `docs/ORIGINAL_ARTIFACT_INVENTORY.md` con hashes;
  copia de trabajo del exe EU en scratch del sandbox (md5
  `692b1282…` verificado; NADA en el repo). Análisis binario
  profundo AUTORIZADO 2026-10-10 (pívot: modificar código,
  investigar dinámicamente incl. I-23, experimentos controlados;
  originales intactos + hashes + copias + restore points).
- **Windows de pruebas parcial:** este entorno (Linux) solo permite análisis
  estático; el PC del mantenedor (Win11 64-bit) ejecutó el Hito 1; build,
  GPU/driver y herramientas de captura siguen pendientes de registrar.
- **Sin Ghidra/JDK/Rizin en el sandbox:** imposible descargarlos aquí
  (egress restringido: sin apt, sin release-assets de GitHub, sin headers
  `-dev`). Documentado y verificado en `docs/TOOLKIT.md`. Ghidra se usará en
  una máquina sin restricciones (`tools/setup-re-env.sh` listo, sin probar).
  El mapa estático de `tools/gexmap` se construyó sin Ghidra (objdump); no hay
  decompilación ni tipos.
- **Mapa estático sin semántica:** los 1 113 inicios por prólogo son hipótesis;
  los tamaños son cotas superiores; el 100 % de cobertura es del union de
  extensiones. La segmentación por subsistemas (2026-10-30) es una primera
  rebanada **HYPOTHESIS** de 65 inicios: 8 son BAJA (7 prólogos), `W=no` no
  prueba código muerto y los nombres no son símbolos originales. No sustituye al
  splitter ni al `symbols.txt` y no cambia el 0,00 % de ByteMatch.
- **REA sin backend nativo:** REA es capa de orquestación/evidencia y necesita
  un proveedor externo (Ghidra/IDA/Hopper) para análisis nativo. En el sandbox
  el CLI se instaló pero `rea doctor` da `healthy:false`. **Ningún binario de
  GEX ha sido analizado con REA.** Estado del host Windows y plan de
  habilitación (pendiente de autorización): `docs/REA_WINDOWS_ENABLEMENT.md`.
- **Audio audible sin confirmación por oído:** el *backend* DirectSound entrega
  PCM al dispositivo y el cursor avanza a la frecuencia del flujo, pero el
  agente no puede escuchar la salida. La confirmación auditiva es del
  mantenedor (`port\build\sagplay.exe --device`). Además el backend es sólo
  Windows y no hay `.sag` reales: todo se valida con fixtures sintéticos.

## Criterio de salida de la Fase 0 (cumplido 2026-10-08, histórico)

1. [x] Los recursos pendientes del mantenedor están revisados.
2. [x] Existe al menos un ejecutable original inventariado (hash + versión):
   `GEX3D.EXE` EU v1.00.000.
3. [~] El entorno de análisis estático está operativo (sandbox; Ghidra
   pendiente en máquina sin restricciones).

Fase 1 iniciada por decisión del mantenedor (2026-10-08). El análisis
binario profundo (Ghidra/decompilación) sigue bloqueado hasta que el
mantenedor indique el cambio de fase.
