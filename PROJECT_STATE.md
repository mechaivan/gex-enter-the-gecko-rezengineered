# PROJECT_STATE — Estado del proyecto

**Project:** Gex: Enter the Gecko — REZengineered
**Phase:** 0 — Planning / Research
**Version:** 0.0.0
**Reverse Engineering:** Not started
**Implementation:** Not started (blocked until research is sufficient)
**Testing:** Not started
**Last updated:** 2026-10-08

## Current objective

> Understand the original PC version of Gex: Enter the Gecko and existing
> community fixes before implementing any modifications.

## Checklist de la Fase 0

- [x] Definir objetivos y principios (README, ROADMAP).
- [x] Crear estructura inicial del repositorio.
- [x] Recopilación inicial de fuentes públicas (PCGamingWiki, foros, proyectos).
- [x] Mapa inicial de problemas conocidos (sin verificar — ver KNOWN_ISSUES.md).
- [x] Catálogo inicial de fixes comunitarios (sin analizar técnicamente).
- [x] Plan de reverse engineering (ver REVERSE_ENGINEERING.md).
- [x] Inventario de herramientas disponibles en este entorno.
- [ ] Recibir y revisar los enlaces/recursos pendientes del mantenedor.
- [ ] Inventariar archivos originales proporcionados (hashes, versiones).
- [ ] Crear estructura de carpetas en Drive (originales, backups, análisis…).
- [ ] Preparar/validar entorno de RE (Ghidra o alternativa + proyecto).
- [ ] Definir entorno de testing en Windows.

## Limitaciones actuales

- **Sin archivos originales todavía:** no se ha proporcionado ningún ejecutable
  ni archivo del juego. Todo lo documentado proviene de fuentes secundarias.
- **Sin entorno Windows de pruebas:** este entorno (Linux) solo permite análisis
  estático; la reproducción de problemas requiere un PC con Windows.
- **Sin Ghidra/JDK/Rizin en el sandbox:** imposible descargarlos aquí
  (egress restringido: sin apt, sin release-assets de GitHub, sin headers
  `-dev`). Documentado y verificado en `docs/TOOLKIT.md`. Ghidra se usará en
  una máquina sin restricciones (`tools/setup-re-env.sh` listo, sin probar).
- **REA sin backend nativo aquí:** CLI instalado pero `rea doctor` indica
  host no soportado y falta de Ghidra/Hopper.

## Criterio de salida de la Fase 0

Pasar a Fase 1 (Research & Documentation profunda) cuando:

1. Los recursos pendientes del mantenedor estén revisados.
2. Exista al menos un ejecutable original inventariado (hash + versión).
3. El entorno de análisis estático esté operativo.
