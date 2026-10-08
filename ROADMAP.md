# ROADMAP

Progresión del proyecto, ordenada por complejidad y dependencias:

```text
ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL
```

Los objetivos de modernización viven en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md)
(M-01…M-23, todos PROPOSED). Este roadmap puede cambiar si la investigación
demuestra que es necesario.

## Fase 0 — Planning / Documentation 🟡 (en curso)

- [x] Definir objetivos y principios del proyecto.
- [x] Recopilar recursos públicos iniciales.
- [x] Preparar estructura del repositorio y documentación base.
- [x] Fijar jerarquía de referencias y taxonomía (ORIGINAL/F/T/R).
- [x] Definir objetivos futuros M-01…M-23 + FA-01…FA-15 (PROPOSED).
- [ ] Cerrar la recopilación de material público pendiente.
- [ ] Probar `tools/setup-re-env.sh` (Ghidra+JDK) en máquina sin restricciones.
- [ ] Definir máquina Windows de testing + protocolo de captura.

## Fase 1 — Research & Original PC Documentation ⬜

- Recopilar y contrastar toda la información disponible.
- Analizar fixes existentes (qué cambian técnicamente).
- Inventariar archivos originales (hashes, versiones, regiones).
- Lista de problemas conocidos con niveles de evidencia.
- Documentar la versión PC: ejecutables, dependencias, registro, CD-audio.
- Cubrir las áreas FA-01…FA-15 a nivel documental.

## Fase 2 — Reverse Engineering ⬜

- Análisis estático del ejecutable (imports, strings, secciones).
- Proyecto Ghidra + naming inicial de funciones/sistemas.
- Dependencias (Glide, Direct3D 5, WinMM, DirectInput, Indeo…).
- Sistemas: timing/FPS, render, audio, input, cámara, CD-check.
- Comparación binaria: original vs ejecutables parcheados.
- Confirmar causas raíz (o refutar hipótesis).
- Detalle en [REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md).

## Fase 3 — Technical Planning ⬜

- Soluciones candidatas por problema.
- Alternativas (nativo vs wrapper vs parche binario).
- Priorización por impacto/riesgo.
- Arquitectura de implementación (loader, DLL, patches…).
- Promover objetivos M-xx de PROPOSED a PLANNED solo con evidencia.

## Fase 4 — Foundation / Compatibility ⬜

- Estabilizar la base: arranque, instalación, dependencias, renderer
  original funcional en Windows moderno.
- Cerrar FA-01…FA-15 con verificación.
- Sin features modernas todavía: solo entender y estabilizar.

## Fase 5 — Low-risk Modernization ⬜

- M-01 Modern Configuration System.
- M-02 Portable Configuration.
- M-03 Reduce / Remove Registry Dependency.
- M-04 Modern Resolution Selection.
- M-05 Borderless Windowed.

## Fase 6 — Modern Input ⬜

- M-06 Modern Input System (+ investigación FA-06/FA-07 previa).
- M-07 XInput · M-08 DualShock/DualSense.
- M-09 Button Remapping · M-10 Analog Deadzones · M-11 Vibration.
- M-12 Modern Camera Control (hipótesis; requiere FA-07).

## Fase 7 — Timing / Render Decoupling ⬜

- M-13 Modern Render Refresh Rates (desacoplar render de simulación).
- M-14 Unlimited Render Mode (experimental).
- Requiere FA-04/FA-05 completamente entendidos.

## Fase 8 — Widescreen / Resolution / Camera ⬜

- M-15 Native 16:9 (real, no estirado).
- M-16 Ultrawide 21:9 / 32:9 (después de 16:9).
- M-17 Adaptive FOV / Camera for Widescreen.

## Fase 9 — Windows Modern Integration ⬜

- M-18 Robust Alt+Tab.
- M-19 Multi-monitor / DPI Awareness (meta de compatibilidad).

## Fase 10 — In-Game Configuration ⬜

- M-20 In-Game Options Menu (Video, Audio, Controls, Advanced).
- Integra los sistemas de las fases 5–9; no eliminar el launcher
  externo sin evidencia.

## Fase 11 — Original Mode / Modern Mode ⬜

- M-21 Original Mode (referencia viva del comportamiento PC).
- M-22 Modern Mode (agrupa mejoras; sin modificar gameplay).

## Fase 12 — Optional Enhanced Visuals ⬜

- M-23 Optional Enhanced Visuals / HD Texture Pack.
- Opcional, desactivable, fuera de Original Mode.

## Fase 13 — Extended Compatibility / Hardware Matrix ⬜

- Rellenar la matriz de hardware con resultados reales.
- Cobertura: GPUs, Windows, drivers, renderers, resoluciones, refrescos,
  controllers. Ver [COMPATIBILITY.md](COMPATIBILITY.md).

## Fase 14 — Experimental / Deferred ⬜

- X-01 Linux / Proton / Steam Deck: DEFERRED, baja prioridad.
- No compite con Windows ni con el objetivo principal.

---

Leyenda: ✅ completada · 🟡 en curso · ⬜ pendiente
