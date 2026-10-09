# ROADMAP

Progresión del proyecto, ordenada por complejidad y dependencias:

```text
ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL
```

Los objetivos de modernización viven en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md)
(M-01…M-25, todos PROPOSED; X-01/X-02 DEFERRED). Este roadmap puede cambiar
si la investigación demuestra que es necesario.

## Fase 0 — Planning / Documentation ✅ (cerrada 2026-10-08)

- [x] Definir objetivos y principios del proyecto.
- [x] Recopilar recursos públicos iniciales.
- [x] Preparar estructura del repositorio y documentación base.
- [x] Fijar jerarquía de referencias y taxonomía (ORIGINAL/F/T/R).
- [x] Definir objetivos futuros M-01…M-24 + FA-01…FA-15 (PROPOSED) + X-01/X-02.
- [x] Cerrar la recopilación de material público pendiente (S-01…S-19).
- Entorno RE y máquina de testing → movidos a Fase 1 (pendientes).

## Fase 1 — Research & Original PC Documentation 🟡 (en curso)

- [x] Inventariar archivos originales (hashes, versiones, regiones):
  edición EU v1.00.000 completa —
  [docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md).
  Variantes US/demo/parcheadas: pendientes.
- [~] Documentar la versión PC: ejecutable identificado (`GEX3D.EXE`,
  Glide exclusivo, rama `3dfx\release_europe`), dependencias
  (Glide/WinMM/DSound; sin D3D ni DirectInput en EU), claves de
  registro, CD-audio (TOC 1+16). Dinámico: pendiente.
- [~] Analizar fixes existentes: Lote 1 (2026-10-09) con descripciones
  F-01…F-12 verificadas en fuente + revisión cruzada + S-14 (2026-10-09);
  diffs binarios en Fase 2.
- [~] Lista de problemas conocidos con niveles de evidencia (evidencia
  estática propia añadida; ningún issue reproducido todavía).
- [x] Registrar propuesta M-25 (voice pack UK/USA, PROPOSED, P3, sin fase)
  + fuente S-20 (reparto vocal) — solo documentación (2026-10-09).
- [~] Cubrir las áreas FA-01…FA-15 a nivel documental: matriz de
  cobertura (2026-10-09); dinámica y RE pendientes (Fase 2+).
- [ ] Probar `tools/setup-re-env.sh` (Ghidra+JDK) en máquina sin restricciones.
- [ ] Definir máquina Windows de testing + protocolo de captura.
- [x] Inventariar S-14 (setup package): metadatos + hashes + inventario
  (22 entradas) + README leído (2026-10-09). Binarios → Fase 2.

## Fase 2 — Reverse Engineering ⬜ (no iniciada)

- [x] Análisis estático del ejecutable (imports, strings, secciones) —
  adelantado en Fase 1 como documentación (ver inventario §4.1).
- Proyecto Ghidra + naming inicial de funciones/sistemas.
- Dependencias: confirmar en dinámico (estático Fase 1 en EU: Glide ✅,
  WinMM ✅, DSound ✅; Direct3D ❌, DirectInput ❌; Indeo ?).
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
- X-02 Posible rewrite Rust/Bevy: LONG-TERM RESEARCH, prioridad muy baja;
  no es una fase de implementación (ver MODERNIZATION_GOALS.md).
- Nada de esta fase compite con Windows ni con el objetivo principal.

---

Leyenda: ✅ completada · 🟡 en curso · ⬜ pendiente
