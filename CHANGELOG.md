# CHANGELOG

Formato: `YYYY-MM-DD — versión — descripción`.

## 2026-10-08 — 0.0.0 — Regla permanente de sincronización + revisión global

- Nueva regla permanente `docs/REPO_SYNC_RULE.md`: cada cambio relevante
  exige revisión global (README/ROADMAP/PROJECT_STATE/CHANGELOG/objetivos/
  issues/RE/compatibilidad/testing), corrección de contradicciones y
  checklist final antes del commit. Sin avances de fase unilaterales.
- Primera aplicación: Fase 0 cerrada, Fase 1 en curso en README, ROADMAP
  y PROJECT_STATE (antes decían Fase 0); ROADMAP Fase 1 con inventario
  completado y entorno/S-14 pendientes; Fase 2 anota el estático
  adelantado sin darse por iniciada.
- Evidencia Fase 1 propagada sin convertir hipótesis en hechos:
  REVERSE_ENGINEERING (Etapa A ✅, B+ bloqueadas), RESEARCH (nueva
  sección «Verificado por el proyecto», originales recibidos),
  KNOWN_ISSUES (notas estáticas en I-03/I-05/I-11/I-15/I-16/I-19, ningún
  issue reproducido), COMPATIBILITY (fila EU confirmada), PATCH_ANALYSIS
  (T-01/T-04: D3D = US/F-01, no EU), MODERNIZATION_GOALS (M-07: sin
  DirectInput en EU; avance parcial FA), TOOLKIT (Etapa A ejecutada),
  TESTING/BUILD/CREDITS (punteros, fase, crédito Mysticore).
- Historial preservado: entradas anteriores intactas.

## 2026-10-08 — 0.0.0 — Fase 1: inventario de artefactos originales (solo docs)

- Nuevo `docs/ORIGINAL_ARTIFACT_INVENTORY.md`: inventario completo de
  `REZengineered/originals/` (1585 ficheros): `disc_image/` (volcado
  CloneCD: hashes, TOC 1+16 pistas, aritmética de sectores) y
  `original_install/` (raíz 19+2, `GEX2/`, `LEVEL/` 72, `AUDIO/` 37,
  `VOICEUK/` 400, `MOVIE/` 18, `DIRECTX/` 85 + `DRIVERS/` 5×189).
- Identificados con evidencia: instalador InstallShield 5.x (stub NE +
  CABs `ISc(` v4), `GEX3D.EXE` (PE32, Glide exclusivo, rama
  `3dfx\release_europe`), edición europea v1.00.000 (`SETUP.INI` +
  `DATA.TAG`), DirectX 4.05.01.1600, CD mixto masterizado ≥1998-05-19.
- Conciliación CD→instalación vía manifiesto de `DATA1.CAB` (100%):
  400/400 voces, 72/72 niveles, 37/37 TADs, 18/18 vídeos.
- Verificación: 30 descargas temporales con SHA-256 30/30, eliminadas
  tras el análisis; el repo no contiene bytes del juego original.
- PROJECT_STATE: checklist y limitaciones actualizados. STOP Fase 1.

## 2026-10-08 — 0.0.0 — Nuevas ideas M-24 (localización) y X-02 (Rust/Bevy)

- M-24 Localization (PROPOSED, P3): estudio futuro de localización de
  textos (English original + Español + otros); solo textos, sin doblaje;
  English = source of truth; Trilogy no reutilizable ni referencia.
- X-02 Posible rewrite Rust/Bevy (DEFERRED, long-term research, prioridad
  muy baja): solo registrar la posibilidad lejana; no es fase de
  implementación; no abandona el engine original.
- ROADMAP Fase 14 menciona X-02 como long-term (sin fase de implementación).
- Nada implementado; prioridades actuales intactas.

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
