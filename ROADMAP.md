# ROADMAP

Progresión del proyecto, ordenada por complejidad y dependencias:

```text
ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL
```

Los objetivos de modernización viven en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md)
(M-01…M-26, todos PROPOSED; X-01/X-02 DEFERRED). Este roadmap puede cambiar
si la investigación demuestra que es necesario.

> Ruta corta (2026-10-10): **Hitos R → K → P + Olas 1–3**.
> Las antiguas Fases 4–14 se fusionan abajo; los IDs M-xx/FA-xx NO cambian.
> Hito P = MAIN 100% del panel (sin crear otra definición).

## Fase 0 — Planning / Documentation ✅ (cerrada 2026-10-08)

- [x] Definir objetivos y principios del proyecto.
- [x] Recopilar recursos públicos iniciales.
- [x] Preparar estructura del repositorio y documentación base.
- [x] Fijar jerarquía de referencias y taxonomía (ORIGINAL/F/T/R).
- [x] Definir objetivos futuros M-01…M-24 + FA-01…FA-15 (PROPOSED) + X-01/X-02.
- [x] Cerrar la recopilación de material público pendiente (S-01…S-19).
- Entorno RE y máquina de testing → movidos a Fase 1 (pendientes).

## Fase 1 — Research & Original PC Documentation 🟡 (en curso)

Completado: inventario EU, Hito 1, catálogos F/S (Lotes 1–2, S-14),
E-1/E-1.1, matrices FA/testing. **Catálogos cerrados: no ampliar
F-xx/S-xx sin necesidad de una implementación concreta.**

- [ ] Specs Hito 1 (build/GPU/driver/refresco) — 1 registro.
- [x] Método A/B decidido: E-2.3 (conmutación InstallDir por copia;
  I-23 causa identificada 2026-10-10).
- [ ] E-2 (Tests 1–2; Test 3 condicional) — LISTO (E-2.3),
  pendiente ejecución PC mantenedor.
- [ ] Probar `tools/setup-re-env.sh` en máquina sin restricciones.
- [ ] Cierre Fase 1 → autorización Fase 2.

## Fase 2 — Reverse Engineering mínimo ⬜ (no iniciada)

Solo Hito K (orden: REVERSE_ENGINEERING.md §3):
- [x] Estático adelantado en Fase 1 (inventario §4.1 + E-1 + E-3/M-13 timing).
- Proyecto Ghidra + naming (`WinMain`, game-loop). Mapa estático
  2026-10-25: `tools/gexmap` identifica inicios confirmados, grafo de
  llamadas e imports, y `WinMain` por la arista desde la entrada PE;
  Ghidra, decompilación y nombres originales siguen pendientes
  (`docs/FUNCTION_MAP.md`). Primera segmentación por subsistemas
  2026-10-30: 65 inicios, 7 grupos HYPOTHESIS, evidencia y confianza
  validadas (`docs/SUBSYSTEM_SEGMENTS.md`); no asigna roles. Traza
  dinámica del frame step 2026-10-30 (breakpoints de hardware, 2 326
  frames): orden del frame y 25 sitios del update medidos, 19 destinos
  con semántica mecánica (`docs/FRAME_STEP_TRACE.md`). Rutas de inicialización de DirectSound
  2026-10-31 (estático, sin traza): `0x00538551` es el único creador vivo
  (2 llamantes conectados a la entrada) y `0x00538FD5` -> `0x00538CB0`
  sigue sin llamantes (`docs/AUDIO_INIT_PATHS.md`). Ruta de audio
  reconstruida el mismo día: `0x00538285` es un decodificador IMA ADPCM
  de 4 bits, `0x005380B3` una bomba de streaming y `0x00538731` el
  reproductor (`docs/AUDIO_MIXER_RE.md`); ya no queda ningún bucle de
  audio sin leer.
- C mínima: timing FA-04/05, ruta/registro FA-11 (I-23 causa
  identificada estática 2026-10-10; dinámica pendiente), init FA-03.
- D mínima: diffs F-02/F-03 vs original (pistas timing).
- Resto de C/D POSPUESTO hasta que una implementación lo pida.

## Fase 3 — Technical Planning ⬜ (puerta del prototipo)

- Soluciones candidatas (solo Hito K) + método (loader/DLL/parche —
  sin presuponer; se decide aquí con evidencia).
- Insumo disponible: `docs/M13_DECOUPLE_PROPOSAL.md` (propuesta M-13;
  no sustituye la decisión de método ni promociona estados).
- Arquitectura mínima para Hito P; resto pospuesto.
- Promover a PLANNED solo los FA/M-xx implicados en Hito P.

## Hitos R → K → P + Olas 1–3 (2026-10-10; sustituyen a Fases 4–14)

### Hito R — Base de ejecución reproducible
- [x] Hito 1 (arranca + jugable Win11+nGlide).
- [ ] Specs + E-2 (método E-2.3 listo; ejecución pendiente).
- Criterio: cualquiera repite arranque y A/B con la hoja TESTING.

### Hito K — Conocimiento mínimo (Fase 2 mínima)
- Criterio: sabemos DÓNDE intervenir (timing, ruta, init) + Fase 3
  decide el método. Nada más se investiga sin pedirlo una
  implementación.

### Hito P — Primer prototipo (= MAIN 100% del panel)
- Una intervención mínima, reversible y documentada sobre copia:
  arranca, cambio observable, resto idéntico, sin regresión.
- Criterio: MAIN-1…MAIN-10 completados (docs/PROJECT_STATUS.md §4.9).

### Ola 1 — Desacoplamiento + base (tras Hito P)
M-13 (+ M-14 solo tras verificar M-13) + enablers mínimos
(M-01/M-02/M-03, M-21 como referencia viva). M-13 va primero porque
sus FA (FA-04/05) salen del camino crítico de Hito K.

### Ola 2 — Display e input
M-04/M-05, M-15…M-17, M-06…M-12. Requieren FA-06/07/13/14; orden por
dependencia real, no por número.

### Ola 3 — Integración y resto
M-18/M-19/M-20, M-22…M-26. X-01/X-02 siguen DEFERRED.

## Vía de implementación nativa (abierta 2026-10-31)

Ruta paralela a Hitos R/K/P, decidida por el mantenedor: implementar ya las
piezas cuyo comportamiento está demostrado por instrucción, sin esperar a la
Fase 3. El port vive en `port/` y cada pieza trae su prueba ejecutable sin el
juego. **No** altera el panel: Hito P y MAIN siguen midiendo la ruta de
mods/parches.

- [x] **I-1 — Lector del contenedor de audio `SLAG`.** Parser `0x0053899C` +
  carga `0x005388AC` transcritos a C11 (`port/src/asset/sag.c`), 29 casos y
  fuzz, 69 305 checks. Doc: `docs/SAG_CONTAINER.md`.
- [x] **I-2 — Ruta de audio (`SLAG` → PCM).** El «mezclador» `0x005380B3`
  resultó ser una **bomba de streaming** y `0x00538285` un **decodificador IMA
  ADPCM de 4 bits**; no hay mezcla de voces en esta ruta. Implementados
  `port/src/audio/adpcm.c` (`0x00538285` + tablas `0x00558138`/`0x00558178`) y
  `port/src/audio/stream.c` (buffer `0x8000`/`0x4000`, bomba `0x005380B3`,
  reproductor `0x00538731`) con *backend* inyectable; 60 052 + 144 checks y demo
  `port/tools/sagplay.c` (tono sintético → `build/tone.wav`). Doc:
  `docs/AUDIO_MIXER_RE.md`. Backend de dispositivo y bucle de bombeo → I-2b.
- [x] **I-2b — Audio audible en el port.** *Backend* de dispositivo con
  DirectSound (la API del original: `dsound.h`/`-ldsound`, sin dependencias
  nuevas; SDL2/SDL3 no está instalado y habría supuesto vendorizar código),
  `port/src/audio/backend_dsound.c` + `port/include/gex/audio/backend_dsound.h`,
  y programación del bombeo con `port/src/audio/sched.c` +
  `port/include/gex/audio/sched.h` (el contrato de los llamantes de
  `0x005380B3`: una llamada por frame tras el update). Volumen de `0x0053886c`
  incluido. Pruebas: `test_audio` 215 checks y `test_device` 50 checks (esta
  última se auto-omite sin tarjeta), con el cursor avanzando a 44 100 B/s y el
  contenido de la mitad leído por `Lock` idéntico al decodificado. Doc:
  `docs/AUDIO_MIXER_RE.md` §16. **Límite:** la audición humana no la verifica
  el agente; `sagplay --device` queda para el mantenedor. El *gate* del cursor
  se respeta (56 diferimientos medidos en una ejecución real).
- [x] **I-3 — Núcleo de tiempo / frame step.** El acumulador de paso fijo se
  movió de `tools/m13-core` al port (`port/src/core/loop_core.c`, una sola
  implementación; el harness del prototipo compila esa fuente y conserva sus
  120 checks) y `port/src/core/frame.c` implementa `gex_frame_run()` con el
  orden medido `pre_frame → update × N → gex_audio_sched_tick → present`, reloj
  inyectado, catch-up acotado (5 pasos, clamp 250 ms), descartes
  contabilizados, reloj invertido recuperado, pausas/reanudación y política de
  errores explícita (incluida corrección: los errores de pre_frame limpian
  acumulador y reanclan reloj, fijada por la regresión T6b). Preset Original
  1:1 (40000 µs, un update por present) y preset desacoplado (16667 µs = 60 Hz,
  la cuantía del sello `16.666` del update). Pruebas: `test_frame` 410 checks
  + `framedemo`; sin ventana, entrada ni render (los tres callbacks son stubs).
  Doc: `docs/FRAME_LOOP_PORT.md`.
- [x] **I-4 — Ventana + entrada + renderer nulo.** Primer proceso nativo:
  `gexloop.exe` abre una ventana Win32 real (clase `GexNativeLoopWindow`,
  960×540), drena mensajes (`WM_KEYDOWN`/`WM_KEYUP`/`WM_KILLFOCUS`/`WM_CLOSE`),
  mantiene estado de teclado con flancos y libera todo al perder foco, sale con
  Escape o al cerrar, y conduce `gex_frame_run()` con
  `QueryPerformanceCounter`. Paso lógico 60 Hz (16 667 µs) y presentación
  objetivo 120 Hz (8 333 µs) con `MsgWaitForMultipleObjectsEx` y
  `timeBeginPeriod(1)` restaurado al salir; medido ~105–108 presents/s,
  59,5–60,0 pasos/s, 0,85–1,93 % de CPU y 0 errores. `present` es un renderer
  nulo que no dibuja; `pre_frame`/`update` son stubs. La evidencia retira
  `0x0043E719` como candidato de entrada (es una máquina de estados de acción
  dentro del update); la ruta de entrada del original es S5 (HYPOTHESIS) y el
  port usa mensajes Win32 como decisión explícita. Pruebas: 826 + 117 + 1 009
  checks nuevos y prueba real de ventana (Escape y `WM_CLOSE`). Doc:
  `docs/WINDOW_INPUT_PORT.md`.
- [x] **I-5 — Investigación y diseño del renderer nativo (sin implementar).**
  Hito de investigación: consolida la evidencia del render original (Glide
  exclusivo, 38 imports censados; `grSstWinOpen` 512×384@75 Hz con 2 buffers
  de color; render condicional `0x0043DC3A` + drenaje doble + `grBufferSwap(3)`
  que bloquea ~23 ms; triángulos en pantalla con `w`, texturas `.3df`/NCC) y
  evalúa D3D 11 vs OpenGL vs capa Glide vs rasterizador software contra siete
  criterios del proyecto. **Recomendación: Direct3D 11** (cabeceras y librerías
  verificadas en ambos toolchains hoy, WARP para pruebas sin GPU, cero
  descargas), condicionada a R-1 (crear dispositivo) como primer paso del hito
  de implementación; OpenGL y el rasterizador C11 quedan como alternativas con
  el mismo contrato. Se define el contrato `gex_renderer` (vtable interna,
  `gex_null_renderer` sigue vivo para las pruebas) y el plan R-1…R-8. **No se
  implementó nada**: sin `d3d11` compilado, sin geometría, sin cambios en
  `frame.c`/`loop_core.c`/audio, sin dependencias ni build. Doc:
  `docs/RENDERER_ARCHITECTURE_RESEARCH.md`.
- [x] **R-1 — Dispositivo Direct3D 11 (hardware y WARP).** Primera pieza del
  renderer real, sin swapchain ni dibujo: `gex_d3d11_device`
  (`port/src/render/d3d11_device.c`, Windows-only) crea el dispositivo con
  `D3D11CreateDevice` en modo `HARDWARE` o `WARP`, registra el feature level
  obtenido (lista explícita 11_1→9_1), entrega el contexto inmediato y libera
  todo COM en `shutdown` idempotente; fallo = struct a cero. **Medido en esta
  máquina: hardware y WARP creados a FL 11_1**; bits reservados →
  `E_INVALIDARG` (0x80070057) con cierre limpio; capa debug ausente →
  `DXGI_ERROR_SDK_COMPONENT_MISSING` (0x887A002D), hallazgo documentado.
  Verificación de fugas conductual (referencia manual + `Release` final a 0)
  porque el runtime mantiene referencias internas al device. Prueba:
  `test_d3d11_device` 205 checks, 0 fallos con **Clang 22.1.8 y GCC 16.1.0**;
  sonda `d3d11probe.exe`; batería completa 132 189 checks, 0 fallos. La
  decisión D3D11 de I-5 queda confirmada. Doc: `docs/D3D11_DEVICE.md`.

Orden justificado: I-2 cerró el subsistema ya empezado; I-2b lo hace audible
(backend DirectSound + planificador, 2026-11-01); I-3 conecta ese planificador
al primer bucle temporal nativo (2026-11-01); I-4 cierra el primer proceso
ejecutable con ventana, teclado y reloj real, sin renderizado (2026-11-02); I-5
investiga y diseña el renderer real sin implementarlo (2026-11-02); R-1 verifica
la decisión con el dispositivo D3D11 real, hardware y WARP, sin swapchain ni
dibujo (2026-11-02). La
llamada `gex_audio_sched_tick(&sched)` ya está dentro del frame loop, una vez
por frame presentado y tras el update (`port/src/core/frame.c`); I-4 rellena
`pre_frame`/`update`/`present` con stubs y un renderer nulo, y lee el reloj real.
El renderizado y la lógica de juego quedan para el hito siguiente (no iniciado);
I-4 no los adelanta.

## Publicación pública (hitos transversales ⬜ — no iniciados)

> Secuencia de publicación futura, en orden de prioridad actual. Nada
> completado; no publicar hasta cerrar la revisión de distribución.

- [ ] Documentación y diagnóstico: registro de compatibilidad + pruebas.
- [ ] Diseño técnico: método de modernización documentado (Fase 3+), sin
  distribuir materiales protegidos.
- [ ] Prototipo interno: comprobar con copia legítima, originales intactos
  cuando el diseño lo permita.
- [ ] Pruebas y recuperación: instalación, funcionamiento, copias,
  restauración, desinstalación.
- [ ] Revisión de distribución: repo, historial, dependencias, licencias y
  método de modificación (ver LICENSE.md §5–§6).
- [ ] Preparación pública: avisos, instrucciones y docs antes de anunciar
  en GitHub, foros o Discord.

---

Leyenda: ✅ completada · 🟡 en curso · ⬜ pendiente
