# ROADMAP

Evolución del proyecto por fases. Este roadmap puede cambiar si la
investigación demuestra que es necesario.

## Fase 0 — Planning ✅ (en curso)

- [x] Definir objetivos y principios del proyecto.
- [x] Recopilar recursos públicos iniciales.
- [x] Preparar estructura del repositorio y documentación base.
- [ ] Preparar almacenamiento (Drive) y flujo de trabajo.
- [ ] Preparar herramientas y entorno de reverse engineering.
- [ ] Recibir material del mantenedor (enlaces, archivos).

## Fase 1 — Research & Documentation ⬜

- Recopilar y contrastar toda la información disponible.
- Analizar documentación y fixes existentes (qué cambian técnicamente).
- Inventariar archivos originales (hashes, versiones, regiones).
- Establecer la lista de problemas conocidos con niveles de evidencia.
- Documentar la versión PC: ejecutables, dependencias, registro, CD-audio.

## Fase 2 — Reverse Engineering ⬜

- Análisis estático del ejecutable (imports, strings, secciones).
- Proyecto Ghidra + naming inicial de funciones/sistemas.
- Análisis de dependencias (Glide, Direct3D 5, WinMM, Indeo…).
- Localización de sistemas: timing/FPS, render, audio, input, CD-check.
- Comparación binaria: original vs ejecutables parcheados.
- Confirmar causas raíz de cada problema (o refutar hipótesis).

## Fase 3 — Technical Planning ⬜

- Determinar soluciones candidatas por problema.
- Evaluar alternativas (nativo vs wrapper vs parche binario).
- Priorizar cambios por impacto/riesgo.
- Definir arquitectura de la implementación (loader, DLL, patches…).

## Fase 4 — Implementation ⬜

- Implementar solo soluciones verificadas y documentadas.
- Un cambio = un commit lógico + documentación + justificación.

## Fase 5 — Testing ⬜

- Comparar **Original vs Fix existente vs REZengineered**.
- Estabilidad, rendimiento, comportamiento, compatibilidad, regresiones.
- Documentar todos los resultados.

## Fase 6 — Documentation & Release ⬜

- Documentación final, builds, instrucciones y releases.
- Guía para que el usuario aporte sus propios archivos originales.

---

Leyenda: ✅ completada · 🟡 en curso · ⬜ pendiente
