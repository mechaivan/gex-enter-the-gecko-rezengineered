# CHANGELOG

Formato: `YYYY-MM-DD — versión — descripción`.

## 2026-10-08 — 0.0.0 — Objetivos M-01…M-23 y roadmap por fases (solo docs)

- Creado MODERNIZATION_GOALS.md: FA-01…FA-15, M-01…M-23, X-01 (DEFERRED);
  todo PROPOSED / TO INVESTIGATE; jerarquía de implementación.
- ROADMAP reordenado en fases 0–14 (entender → base → sencillo →
  dependiente → complejo → experimental).
- RESEARCH: áreas de investigación futura (reportado vs hipótesis vs futuro).
- COMPATIBILITY: dimensiones futuras + matriz hardware (vacía) + Linux/Wine
  como secundario (X-01 DEFERRED).
- TESTING: categorías futuras; KNOWN_ISSUES: mapeo a objetivos (sin nuevos
  issues); PATCH_ANALYSIS: mapeo fixes → objetivos; REVERSE_ENGINEERING:
  vínculo con FA/M; README: enlace a goals.
- Nada implementado; ninguna afirmación convertida en hecho.

## 2026-10-08 — 0.0.0 — Taxonomía estricta + Fase 0 estricta + wrappers ronda 2

- README: eliminada la explicación del juego de palabras del nombre (queda
  solo el nombre del proyecto). Logo IA pendiente (no crear gráficos).
- PATCH_ANALYSIS: taxonomía §0 (ORIGINAL / F-fix / T-wrapper / R-referencia)
  + regla explícita: nada catalogado se asume solución final.
- Nuevos T-04 (DDrawCompat), T-05 (DxWnd, upstream por confirmar), T-06
  (WineD3D for Windows); fuente S-19.
- S-14: catalogado como material de investigación; prohibido moverlo al
  sandbox o modificarlo. Originales y análisis binario pospuestos por
  decisión del mantenedor (PROJECT_STATE, RESEARCH).

## 2026-10-08 — 0.0.0 — Referencias del mantenedor + jerarquía + herramientas

- Jerarquía fijada: PC original = fuente de verdad; otras versiones solo
  secundarias; Gex Trilogy fuera de referencias (README, RESEARCH §0).
- RESEARCH: S-11/S-12/S-18 enriquecidos (Gex64Decomp, Trilogy-demoted, REA);
  nuevas S-14 (PC Version Setup Package speedrun), S-15 (guía PS1),
  S-16 (DxWrapper), S-17 (dgVoodoo2).
- PATCH_ANALYSIS: F-09 (setup package) + sección herramientas T-01..T-03.
- Copia privada de S-14 en `Drive → REZengineered/research/` (411 MB,
  pendiente de extraer README e inventario).

## 2026-10-08 — 0.0.0 — Entorno RE sandbox operativo

- `tools/setup-sandbox-re.sh`: venv (`pefile`, `capstone`, `yara-python`) +
  REA CLI 5.0.0 en `/opt/rea-toolkit`. Verificado con self-test.
- `tools/setup-re-env.sh`: script (sin probar) para entorno completo
  Ghidra 12.1.4 + JDK 21 en máquinas con internet completo.
- Verificado e imposible en sandbox: JDK/Ghidra/Rizin/radare2 (documentado
  el porqué en `docs/TOOLKIT.md`); `rea doctor` confirma falta de backend.
- Estructura Drive `REZengineered/{originals,backups,analysis,builds,research}`.
- Actualizados PROJECT_STATE, TOOLKIT, tools/README.

## 2026-10-08 — 0.0.0 — Inicialización Fase 0

- Creación de la estructura inicial del repositorio.
- Documentos base: README, PROJECT_STATE, ROADMAP, RESEARCH, KNOWN_ISSUES,
  PATCH_ANALYSIS, COMPATIBILITY, REVERSE_ENGINEERING, BUILD, TESTING,
  CREDITS, LICENSE, CHANGELOG.
- Guías: `docs/TOOLKIT.md`, `docs/ENGINEERING_LOG_TEMPLATE.md`.
- `.gitignore` orientado a no redistribuir material propietario.
- Recopilación inicial de fuentes públicas (PCGamingWiki, foros, proyectos).
- Mapa inicial de problemas conocidos (todos **sin verificar**).
- Catálogo inicial de fixes comunitarios (todos **sin analizar técnicamente**).
- Sin modificaciones del juego (correcto según la Fase 0).
