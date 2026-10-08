# PATCH_ANALYSIS — Catálogo y análisis de fixes existentes

> **Nivel de evidencia global: CATALOGADO, NO ANALIZADO.**
> Este documento lista las soluciones comunitarias encontradas el 2026-10-08.
> De ninguna se conoce todavía **qué cambia técnicamente**; ese análisis es
> trabajo de las Fases 1–2. No descargar ni aplicar nada ciegamente:
> primero determinar qué hace cada fix y por qué funciona.

## F-01 — Unofficial Direct3D patch (PAL) — tgames.fr

- **Problema que dice solucionar:** añade render Direct3D a versiones no-US
  (I-03).
- **Origen:** tgames.fr — "3DFX to Direct 3D Patch (PAL) for Nvidia/ATI cards
  (Windows XP)".
- **Archivos asociados:** parche + `gex3d_windows10.zip` (ficheros de config
  necesarios en Windows 10+).
- **Efectos conocidos (sin verificar):** cap de FPS pasaría a 24.
- **Pendiente:** conseguir copia, identificar ejecutables afectados, diff
  binario original vs parcheado, APIs utilizadas.
- **Estado:** DESCONOCIDO.

## F-02 — nGlide + `gex2_patch.zip` — Zeus Software

- **Problema que dice solucionar:** compatibilidad Glide en GPUs modernas (I-05).
- **Origen:** zeus-software.com — wrapper nGlide + parche específico Gex2.
- **Archivos asociados:** `gex2_patch.zip` contendría un `GEX3D.exe` de
  reemplazo (según Abandonware France: se extrae y sobrescribe).
- **Notas de compatibilidad (foro Zeus):**
  - El juego pide 75 Hz; Voodoo2 real usa 512x384@60.
  - "Gex2 works too fast even at 60fps. It needs a 30fps cap" → existe un
    ejecutable capeado en la compatibility list de nGlide.
  - Reportes de "wrong drawing" y de "pure virtual function call" con la
    versión parcheada Glide2.
- **Pendiente:** diff del exe reemplazado, determinar qué renderer paths toca.
- **Estado:** DESCONOCIDO.

## F-03 — 3DFX FPS Limiter (EU/US) — tgames.fr

- **Problema que dice solucionar:** velocidad excesiva en sistemas rápidos
  (I-01).
- **Origen:** tgames.fr — "patch-3dfx-fps-limiter".
- **Efectos conocidos (sin verificar):** limitaría FPS; "runs smooth and at
  normal speed".
- **Pendiente:** determinar mecanismo (¿sleep/busy-wait? ¿hook? ¿exe
  reemplazado? ¿a qué FPS limita?).
- **Estado:** DESCONOCIDO.

## F-04 — Ejecutable capeado a 30 FPS — nGlide compatibility list

- **Problema que dice solucionar:** velocidad excesiva (I-01).
- **Origen:** lista de compatibilidad de nGlide (mencionado en su foro).
- **Pendiente:** localizarlo, compararlo con el original y con F-03.
- **Estado:** DESCONOCIDO.

## F-05 — Instalación manual + `.reg` + modo administrador

- **Problema que dice solucionar:** instalador roto en Windows moderno (I-15).
- **Origen:** PCGamingWiki.
- **Contenido:** copiar carpeta `gex2`; crear claves
  `HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00` (`Version=2`, `InstallDir`,
  `CDDriveName`); ejecutar `gex3d.exe` como administrador; aplicar F-01.
- **Pendiente:** confirmar qué lee el exe del registro (análisis estático de
  strings/imports + dinámico).
- **Estado:** DESCONOCIDO.

## F-06 — Fix del códec Indeo (intro) — VOGONS

- **Problema que dice solucionar:** intro no se reproduce (I-14).
- **Origen:** hilo VOGONS; contexto Wine/Linux pero aplicable a Windows.
- **Contenido:** colocar `ir32_32.dll` junto al exe y registrar descripciones
  en `HKLM\...\drivers.desc` ("Indeo® Video R3.2"…).
- **Pendiente:** confirmar que la intro usa Indeo y qué API la reproduce.
- **Estado:** DESCONOCIDO.

## F-07 — Versiones específicas de nGlide (ej. 0.99 en Vista)

- **Problema que dice solucionar:** arranque en Vista (nGlide 1.03 no
  soportada; 0.99 funcionaría).
- **Origen:** Abandonware France.
- **Pendiente:** verificar; probablemente obsoleto, pero documenta que el
  comportamiento depende del wrapper.
- **Estado:** DESCONOCIDO.

## F-08 — Parches de patches-scrolls.de ("patch for 3dfx PC", "fix PC")

- **Problema que dicen solucionar:** sin información todavía.
- **Origen:** patches-scrolls.de, entradas del 16.08.13 para "Gex II".
- **Pendiente:** identificar contenido y autoría.
- **Estado:** DESCONOCIDO.

## Proyectos relacionados (no son fixes de la versión PC)

## R-01 — Gex64Decomp (MatBourgon / Tokatta007)

- **Qué es:** decompilación WIP de *Gex 64* (N64, MIPS) con splat/decomp.me.
- **Utilidad potencial:** nombres, sistemas, lógica, estructuras como
  **referencia** arquitectónica.
- **Advertencia:** NO asumir identidad con la versión PC (distinto port:
  LTI Gray Matter; distinta plataforma y CPU).
- **Estado:** catalogado como referencia.

## R-02 — Gex Trilogy (2025, Limited Run Games, Carbon Engine)

- **Qué es:** reedición basada en las versiones **PlayStation** vía emulación.
- **Utilidad potencial:** comparación de comportamiento audiovisual original.
- **Advertencia:** no es la versión PC ni comparte su código; no sirve como
  fuente de implementación.
- **Estado:** catalogado como referencia.

## Plantilla de análisis (usar en Fases 1–2)

Para cada fix documentar: nombre, autor, fecha, problema, comportamiento
antes/después, archivos y ejecutables afectados, DLLs, offsets, funciones,
instrucciones modificadas, hooks, wrappers, APIs, dependencias,
limitaciones, compatibilidad y efectos secundarios. Comparar siempre
**Original → Fix comunitario → REZengineered**.
