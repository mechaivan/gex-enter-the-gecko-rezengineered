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

## F-09 — PC Version Setup Package (Mysticore, speedrun.com, 2022-01-25)

- **Problema que dice solucionar:** setup completo de la versión PC NTSC en
  sistemas modernos sin disco físico.
- **Origen:** <https://www.speedrun.com/gex2/resources/e3dsk> →
  `Gex 2 PC (Patches & Tools).zip` (~411 MB).
- **Copia de trabajo:** `Drive → REZengineered/research/` (privada).
- **Pendiente:** extraer README + inventario (parches, tools, versiones,
  ejecutables, hashes). Por tamaño, hacerlo en máquina con espacio.
- **Estado:** DESCONOCIDO (contenido sin inspeccionar).

## Herramientas de compatibilidad (no son fixes del juego)

Se catalogan como ayudas de testing/estudio. El proyecto decidirá en Fase 3
qué papel juega cada una; la prioridad son soluciones nativas y fundamentadas.

## T-01 — DxWrapper (elishacloud, open source)

- **Qué es:** DDraw/D3D1–7 → D3D9, D3D8 → D9, DInput1–7 → 8, hooks DirectSound,
  loader `.asi`, resolution hack legacy, modo ventana.
- **Interés:** cubre D3D5 (API del juego); código abierto para estudiar
  intercepción de APIs legacy.
- **Fuente:** S-16. **Estado:** catalogado, sin probar.

## T-02 — dgVoodoo2 (dege-diosg, freeware, código cerrado)

- **Qué es:** Glide/DirectDraw/D3D3–9 → D3D11/12.
- **Interés:** comparar rutas Glide y D3D bajo wrappers distintos.
- **Fuente:** S-17. **Estado:** catalogado, sin probar.

## T-03 — nGlide (Zeus Software, freeware, código cerrado)

- **Qué es:** wrapper Glide → Direct3D moderno; estándar de facto para Gex2.
- **Interés:** ruta Glide en GPUs modernas; exe capeado a 30 FPS (F-04).
- **Fuente:** S-02/S-04. **Estado:** catalogado, sin probar.

## Proyectos relacionados (no son fixes de la versión PC)

## R-01 — Gex64Decomp (MatBourgon / Tokatta007) — REFERENCIA SECUNDARIA

- **Qué es:** decompilación WIP de *Gex 64* (N64, MIPS) con splat/decomp.me.
- **Utilidad potencial:** nombres, sistemas, lógica, estructuras como
  **referencia** arquitectónica para distinguir comportamiento de la familia
  Gex de problemas del port PC. **NO reproducir su comportamiento;
  NO es código PC.**
- **Advertencia:** NO asumir identidad con la versión PC (distinto port:
  LTI Gray Matter; distinta plataforma y CPU).
- **Estado:** catalogado como referencia secundaria (fuente S-11).

## R-02 — Gex Trilogy (2025) — NO ES REFERENCIA (contexto)

- Por decisión del proyecto (2026-10-08), Gex Trilogy (Limited Run / Carbon
  Engine, emulación de versiones PlayStation) **no** forma parte de las
  referencias. Se conserva esta nota solo como contexto histórico.

## Plantilla de análisis (usar en Fases 1–2)

Para cada fix documentar: nombre, autor, fecha, problema, comportamiento
antes/después, archivos y ejecutables afectados, DLLs, offsets, funciones,
instrucciones modificadas, hooks, wrappers, APIs, dependencias,
limitaciones, compatibilidad y efectos secundarios. Comparar siempre
**Original → Fix comunitario → REZengineered**.
