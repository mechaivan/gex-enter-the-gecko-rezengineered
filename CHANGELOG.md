# CHANGELOG

Formato: `YYYY-MM-DD — versión — descripción`.

## 2026-10-09 — 0.0.0 — Evaluación de entornos Windows (sin probar)

- TESTING.md: alternativas A (Win moderno+wrapper), B (VM parcial), C
  (HW época referencia) + checklist 14 requisitos (IMP/REC/ESP) + MVP
  (A) y referencia ideal (C). Ninguna config declarada compatible.
- Sync: TOOLKIT §5.2 [~], PROJECT_STATE/ROADMAP (máquina pendiente,
  faltan datos HW). Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Protocolo de pruebas + matriz FA (Windows, sin ejecutar)

- TESTING.md: protocolo reproducible (entorno, hashes, instalación,
  pasos/veredicto, artefactos, fallos, custodia, limitaciones
  moderno-vs-época) + matriz priorizada FA-04…FA-15 (14 pruebas, sin
  nuevos IDs). Base EU-Glide; US/D3D/parches solo comparadores.
- ROADMAP `M-01…M-24` revisado: correcto como está (describe la Fase 0
  cerrada 2026-10-08; M-25 vive en Fase 1). Sin cambios; M-25 PROPOSED.
- Sync: PROJECT_STATE/ROADMAP (testing [~], máquina pendiente).
- Fase 1 en curso, Fase 2 sin iniciar. Nada ejecutado.

## 2026-10-09 — 0.0.0 — Matriz documental FA-01…FA-15 (sin RE)

- MODERNIZATION_GOALS: nueva sección de cobertura documental (15 filas:
  conocido, evidencia+fuente, tipo, relevancia EU, falta, siguiente
  acción, estado). 4 BASE DOCUMENTADA (FA-01/02/03/09), 8 PARCIAL, 3
  PENDIENTE (FA-04/07/15). Tabla FA intacta (todo TO INVESTIGATE).
- RESEARCH: patrón de guardado `GEX2%d%d%c.GEX` (estático) aflorado a la
  lista verificada (ubicación UNKNOWN). Sin nuevos IDs, sin binarios.
- Sync: PROJECT_STATE/ROADMAP (FA documental [~], dinámica → Fase 2+).
- Fase 1 en curso (no cerrada), Fase 2 sin iniciar, M-25 PROPOSED.

## 2026-10-09 — 0.0.0 — Revisión cruzada F-01…F-12 + S-14 (solo documental)

- PATCH_ANALYSIS: nueva sección de revisión cruzada (tabla de seguimiento
  F-01…F-12, duplicidades, análisis S-14 en 5 puntos, prioridades
  documentales). Sin fusiones, sin nuevos IDs, sin binarios.
- Correcciones mínimas: RESEARCH S-02 («exe capeado» → cita motivadora,
  veredicto F-04); PATCH F-01 (ambigüedad `voice`/`voiceuk` explicitada).
  F-05/F-06/F-07/F-12 ya cumplían (sin cambios); historial intacto.
- Sync: PROJECT_STATE/ROADMAP (revisión cruzada anotada en fixes [~]).
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Duplicidad
  confirmada: ninguna; probable: F-04↔F-02 (descriptiva, pendiente binario).

## 2026-10-09 — 0.0.0 — S-14: inventario nominal + README leído (carpeta extraída)

- `S-14_extracted` (subida por el mantenedor): 22 entradas inventariadas
  solo por nombre+metadatos (README, Game Files con exe 2013 + 15 WAV, 3
  instaladores); README.txt leído íntegro (montar Gex3DD3D.ccd, copiar
  exe+music, nGlide, _inmm DirectShow, sin disco). Nada ejecutado.
- Clasificación: inventario+README CONFIRMADOS a nivel documental;
  procedimiento NTSC-D3D DECLARADO (no verificado); binarios/WAV/EU
  aplicabilidad DESCONOCIDOS. Sin nuevos IDs.
- Sync: RESEARCH (S-14), PATCH_ANALYSIS (F-09), PROJECT_STATE/ROADMAP
  (S-14 [x], binarios→Fase 2), KNOWN_ISSUES (I-11 _inmm), COMPATIBILITY
  (fila D3D), REVERSE_ENGINEERING (Etapa D: exe 2013 candidato).
- Fase 1 en curso, Fase 2 sin iniciar, M-25 PROPOSED. Historial intacto.

## 2026-10-09 — 0.0.0 — S-14: inventario interno bloqueado (solo metadatos)

- Acceso + hashes re-verificados (2ª comprobación, Drive solo lectura):
  ambas copias intactas (MD5/SHA-256 coinciden, no trashed, sin modificar).
- Índice ZIP + README: BLOQUEADOS en este entorno con evidencia —
  `read_file_text` falla (25 MB cap vs 411301184 bytes) y `download_file`
  solo ofrece bytes completos o URL no consumible; copia completa al
  workspace prohibida. Sin descarga, extracción ni ejecución.
- Sync: RESEARCH (S-14: re-verificación + bloqueo), PATCH_ANALYSIS (F-09),
  PROJECT_STATE/ROADMAP (S-14 [~] con bloqueo). Contenido sigue
  DESCONOCIDO; Fase 1 en curso, Fase 2 sin iniciar.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Precisión F-12 + inspección documental S-14

- Bloque A: F-12 ya no presenta el menú debug como confirmado
  (PATCH_ANALYSIS: «Qué es (DECLARADO POR LA FUENTE)» + «Valoración» con
  atribución explícita; COMPATIBILITY y REVERSE_ENGINEERING suavizados
  igual). A2 verificado sin cambios: Last updated 2026-10-09, M-25
  PROPOSED, Fase 1 en curso, Fase 2 sin iniciar.
- Bloque B: S-14 con metadatos Drive autorizados (solo lectura): 411301184
  bytes, MD5/SHA-256 registrados, copia `research/` bit-idéntica;
  discrepancia 411/~393 resuelta con evidencia (unidades). Página releída:
  cita corregida («Includes…»), README no publicado por separado.
  Contenido sigue DESCONOCIDO; S-14 sigue investigación pendiente.
- Sync: RESEARCH (S-14 reestructurado + pendiente), PATCH_ANALYSIS (F-12,
  F-09), PROJECT_STATE/ROADMAP (S-14 [~]). Sin binarios, sin descargas al
  sandbox, sin ejecución, sin fase avanzada.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Nueva propuesta M-25: selección de voces UK/USA

- MODERNIZATION_GOALS: +M-25 Voice Pack Selection (UK/USA), PROPOSED, P3
  provisional, Nivel 10 (Audio/Voces), sin fase (post-11, TBD); refs
  cruzadas M-20 (Audio) y M-24 (textos, no confundir); mapa actualizado.
- RESEARCH: nueva S-20 (reparto Gould/Phillips: Wikipedia verificada +
  VGFacts/BTVA; general, no PC-específica) + UNKNOWNs base de M-25.
- Sync: ROADMAP (M-01…M-25, item Fase 1), PROJECT_STATE (ampliación +
  Last updated), REVERSE_ENGINEERING (Etapa C audio/voces), COMPATIBILITY
  (nota variante regional), PATCH_ANALYSIS (F-01/F-03 → M-25), CREDITS.
- Sin implementación, sin binarios tocados, sin fase avanzada.
- Historial preservado: entradas anteriores intactas.

## 2026-10-09 — 0.0.0 — Lote 1: investigación documental de fixes (F-01…F-12)

- PATCH_ANALYSIS: F-01…F-09 re-investigados en sus fuentes (hilos leídos
  íntegros, citas verbatim) + F-10 (music handler), F-11 (NO-CD D3D) y F-12
  (debug tool, confirma menú debug en build D3D) catalogados. F-04 queda
  como probable duplicado de F-02 (sin pieza separada); F-06 no validado
  (hilo VOGONS no resuelto); t12123 (repack full-game) fuera de alcance.
- RESEARCH: S-01/S-02/S-03/S-04/S-05/S-08/S-09/S-14 re-verificadas (S-02
  absorbe el hilo Zeus «No music in Gex 2»: winmm/MCI + `_inmm.dll`).
- KNOWN_ISSUES: notas Lote 1 en I-01/I-11/I-12/I-14/I-16 (I-14 debilitada a
  hipótesis débil; I-11/I-12 con nuevos reportes adyacentes).
- COMPATIBILITY (fila «versión D3D»), REVERSE_ENGINEERING (Etapa D:
  F-01…F-12), PROJECT_STATE/ROADMAP (Fase 1 [~] en fixes) sincronizados.
- Historial preservado: entradas anteriores intactas.

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
