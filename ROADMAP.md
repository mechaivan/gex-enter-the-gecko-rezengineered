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
- [ ] Decisión método A/B pese a I-23 (mantenedor).
- [ ] E-2 (Tests 1–2; Test 3 condicional) — BLOQUEADO hasta decidir A/B.
- [ ] Probar `tools/setup-re-env.sh` en máquina sin restricciones.
- [ ] Cierre Fase 1 → autorización Fase 2.

## Fase 2 — Reverse Engineering mínimo ⬜ (no iniciada)

Solo Hito K (orden: REVERSE_ENGINEERING.md §3):
- [x] Estático adelantado en Fase 1 (inventario §4.1 + E-1).
- Proyecto Ghidra + naming (`WinMain`, game-loop).
- C mínima: timing FA-04/05, ruta/registro FA-11 (I-23), init FA-03.
- D mínima: diffs F-02/F-03 vs original (pistas timing).
- Resto de C/D POSPUESTO hasta que una implementación lo pida.

## Fase 3 — Technical Planning ⬜ (puerta del prototipo)

- Soluciones candidatas (solo Hito K) + método (loader/DLL/parche —
  sin presuponer; se decide aquí con evidencia).
- Arquitectura mínima para Hito P; resto pospuesto.
- Promover a PLANNED solo los FA/M-xx implicados en Hito P.

## Hitos R → K → P + Olas 1–3 (2026-10-10; sustituyen a Fases 4–14)

### Hito R — Base de ejecución reproducible
- [x] Hito 1 (arranca + jugable Win11+nGlide).
- [ ] Specs + método A/B + E-2 (ver Fase 1 restante).
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
