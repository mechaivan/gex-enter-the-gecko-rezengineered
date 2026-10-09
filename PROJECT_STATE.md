# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 1 — Research & Original PC Documentation (Fase 0 cerrada 2026-10-08)
**Version:** 0.0.0
**Original artifact inventory:** COMPLETE (2026-10-08, EU v1.00.000)
**Reverse Engineering:** Deep RE not started (Etapa A documented in Phase 1)
**Implementation:** Not started (blocked until research is sufficient)
**Testing:** Not started
**Sync rule:** [docs/REPO_SYNC_RULE.md](docs/REPO_SYNC_RULE.md)
**Last updated:** 2026-10-08

## Ampliación de objetivos (2026-10-08, solo documentación)

- Se ha ampliado el conjunto de objetivos futuros: FA-01…FA-15 (fundamentos),
  M-01…M-24 (modernización), X-01 (deferred) y X-02 (long-term research).
  Ver MODERNIZATION_GOALS.md.
- **Ninguna feature está implementada.** Todas están PROPOSED / TO INVESTIGATE
  (X-01/X-02 DEFERRED).
- La prioridad actual es la Fase 1 / Research (Fase 0 cerrada 2026-10-08).
- No se debe saltar a implementación: la documentación precede a cualquier
  modificación técnica.
- Orden: ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL.
- 2026-10-08: +M-24 localización (PROPOSED, P3, sin fase) y +X-02 posible
  rewrite Rust/Bevy (DEFERRED, long-term research, fuera de implementación).

## Current objective

> Understand the original PC version of Gex: Enter the Gecko and existing
> community fixes before implementing any modifications.

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
- [~] Problemas conocidos con evidencia: estática propia añadida; ningún
  issue reproducido todavía.
- [~] Analizar fixes existentes: Lote 1 (2026-10-09) con descripciones
  F-01…F-12 verificadas en fuente; diffs binarios pendientes (Fase 2).
- [ ] Cubrir FA-01…FA-15 a nivel documental.
- [ ] Inventariar S-14 (setup package speedrun): README + parches + hashes
  (cuando se indique; no mover datos al sandbox).
- [ ] Preparar/validar entorno de RE (Ghidra o alternativa + proyecto).
- [ ] Definir entorno de testing en Windows.

## Limitaciones actuales

- **Originales inventariados, sin copias locales:** `originals/` en Drive
  (volcado CloneCD + contenido del CD) catalogado en
  `docs/ORIGINAL_ARTIFACT_INVENTORY.md` con hashes y análisis estático de
  solo lectura; no queda ninguna copia en el sandbox ni en el repo.
  El análisis binario profundo sigue bloqueado hasta que el mantenedor
  indique el cambio de fase.
- **Sin entorno Windows de pruebas:** este entorno (Linux) solo permite análisis
  estático; la reproducción de problemas requiere un PC con Windows.
- **Sin Ghidra/JDK/Rizin en el sandbox:** imposible descargarlos aquí
  (egress restringido: sin apt, sin release-assets de GitHub, sin headers
  `-dev`). Documentado y verificado en `docs/TOOLKIT.md`. Ghidra se usará en
  una máquina sin restricciones (`tools/setup-re-env.sh` listo, sin probar).
- **REA sin backend nativo aquí:** CLI instalado pero `rea doctor` indica
  host no soportado y falta de Ghidra/Hopper.

## Criterio de salida de la Fase 0 (cumplido 2026-10-08, histórico)

1. [x] Los recursos pendientes del mantenedor están revisados.
2. [x] Existe al menos un ejecutable original inventariado (hash + versión):
   `GEX3D.EXE` EU v1.00.000.
3. [~] El entorno de análisis estático está operativo (sandbox; Ghidra
   pendiente en máquina sin restricciones).

Fase 1 iniciada por decisión del mantenedor (2026-10-08). El análisis
binario profundo (Ghidra/decompilación) sigue bloqueado hasta que el
mantenedor indique el cambio de fase.
