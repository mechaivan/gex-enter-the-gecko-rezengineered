# REVERSE_ENGINEERING — Plan y metodología

> Estado: **Etapa A completada como documentación (Fase 1, 2026-10-08)** —
> binarios EU inventariados + análisis estático básico registrado en
> [docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md).
> RE profundo (Etapas B+) **no iniciado** (bloqueado hasta cambio de fase).

## 1. Principios

- REA como capa de orquestación/evidencia; Ghidra (u otra herramienta
  apropiada) como motor de análisis profundo.
- Todo hallazgo se etiqueta: **Confirmado / Inferido / Experimental /
  Desconocido / Limitación**. Ninguna hipótesis se presenta como hecho.
- Trabajar siempre sobre **copias**; originales intactos y verificados por hash.
- Priorizar sistemas que expliquen problemas de KNOWN_ISSUES.md.

## 2. Objetivos del análisis

1. Inventario de binarios: ejecutables, DLLs, versiones, regiones, hashes
   (SHA-256), fechas, entry points.
2. Dependencias: imports/exports (Fase 1 en EU: Glide ✅, WinMM ✅, DSound ✅;
   Direct3D ❌, DirectInput ❌; Indeo pendiente).
3. Superficie de configuración: strings del registro (`Gex2`, `InstallDir`,
   `CDDriveName`), ficheros de config, argumentos CLI.
4. Sistemas internos: timing/game-loop, render (Glide vs D3D), audio/CD-audio,
   input, CD-check, FMV/intro, gestión de memoria.
5. Diffs binarios: original vs `gex2_patch.zip` vs FPS Limiter vs parche D3D.
6. Mapa función↔problema para cada issue de KNOWN_ISSUES.md.

## 3. Plan por etapas

### Etapa A — Inventario (sin desensamblar) ✅ (Fase 1, ver inventario §2–§4)

- [x] Recibir y hashear originales (SHA-256) + registrar procedencia.
- [x] `file`, headers PE (máquina, subsistema, timestamp, secciones).
- [x] Tablas de imports/exports y DLLs implícitas.
- [x] Extracción de strings (rutas, claves de registro, mensajes, DX/Glide).
- [ ] Comparar variantes disponibles (US/EU/demo/exes parcheados).

### Etapa B — Proyecto Ghidra + naming inicial

- [ ] Crear proyecto (fuera del repo; artefactos grandes a Drive).
- [ ] Auto-análisis, identificar `WinMain`, game-loop, init/shutdown.
- [ ] Nombrar imports clave y callbacks (ventana, timer, audio).
- [ ] Documentar convenciones en este fichero.

### Etapa C — Sistemas (uno por uno, con engineering log)

- [ ] Timing/FPS (I-01, I-02): buscar sleeps, `timeGetTime`, contadores,
      divisores de frame; explicar por qué 30 FPS es "correcto".
- [ ] Render (I-03–I-10): ruta Glide única en EU (sin selección D3D en el
      binario); modos de vídeo; petición de 75 Hz; paths de resolución.
      Selección Glide/D3D solo en US/F-01 (pendiente de binario).
- [ ] Audio/CD (I-11–I-13): MCI/WinMM/CD-audio; qué ocurre al cambiar de nivel.
- [ ] FMV (I-14): reproductor de intro; dependencia Indeo.
- [ ] Instalación/CD-check (I-15–I-17): lecturas de registro; detección de CD.
- [ ] Input (I-18–I-19): teclado + joystick WinMM (sin imports DirectInput
      en EU); confirmar ausencia de ratón.

### Etapa D — Diffs de parches comunitarios

- [ ] Por cada fix (F-01…F-08): qué bytes/funciones cambian y qué efecto tienen.
- [ ] Tabla comparativa **Original → Fix → Hipótesis de causa → Propuesta**.

## 4. Herramientas

Ver [docs/TOOLKIT.md](docs/TOOLKIT.md). Resumen: binutils disponibles en este
entorno; Ghidra + Java pendientes de instalar; REA (CLI/MCP, trae su propio
backend vía Hopper o Ghidra aportado) pendiente de evaluar; testing dinámico
requiere Windows (limitación actual).

## 5. Registro

Cada sistema/problema relevante genera una entrada de engineering log según
[docs/ENGINEERING_LOG_TEMPLATE.md](docs/ENGINEERING_LOG_TEMPLATE.md), que se
archivará en `docs/engineering-log/` cuando exista.

## 6. Relación con MODERNIZATION_GOALS

- Las Etapas A–C cubren FA-01…FA-15 (fundamentos). Sin ellas, ningún M-xx
  puede pasar de PROPOSED a PLANNED.
- La Etapa D (diffs de parches) alimenta directamente FA-03/FA-05/FA-10/FA-11
  y los objetivos M-04, M-13, M-01…M-03 (ver PATCH_ANALYSIS.md § Relación).
- El orden de implementación vive en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md)
  y [ROADMAP.md](ROADMAP.md): este documento define el *cómo* del análisis,
  no el *cuándo* de las features.
