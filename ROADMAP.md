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
  `docs/AUDIO_MIXER_RE.md`. Pendiente: backend de dispositivo y bucle de bombeo.
- [ ] **I-2b — Audio audible en el port.** Implementar un *backend* de
  dispositivo (WASAPI/ALSA/SDL) con `submit`/`stop`/`cursor`, programar la bomba
  cada media mitad y exponer el volumen de `[0x5582F4]`. Criterio: el tono
  sintético suena sin DirectSound y el *gate* del cursor se respeta.
- [ ] **I-3 — Núcleo de tiempo / frame step.** El acumulador de paso fijo ya
  está prototipado en `tools/m13-core`; falta conectarlo al orden de frame
  medido en `docs/FRAME_STEP_TRACE.md` (pre-update → update + sello → swap) y
  probarlo con reloj inyectado. Criterio: la secuencia de llamadas por frame
  coincide con la traza, sin render.
- [ ] **I-4 — Ventana + entrada + renderer nulo.** Abrir ventana, leer teclado
  y ejecutar el frame step con un renderer que no dibuja (`0x0043E719` es el
  candidato de entrada). Criterio: proceso que corre, responde a teclas y no
  crashea; es el primer «prototipo ejecutable» aunque no muestre el juego.

Orden justificado: I-2 cerró el subsistema ya empezado; I-2b lo hace audible;
I-3 e I-4 son los requisitos mínimos de cualquier bucle jugable y no dependen de
assets.

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
