# RESEARCH — Fuentes y bibliografía anotada

Recopilación inicial (2026-10-08) de fuentes públicas. Son material de
investigación, **no** instrucciones ciegas. Todo está pendiente de contraste.

## S-01 — PCGamingWiki: "Gex: Enter the Gecko" ⭐ fuente base

- URL: <https://www.pcgamingwiki.com/wiki/Gex:_Enter_the_Gecko>
- Datos que aporta: ficha del port PC (desarrollo Crystal Dynamics, port PC
  LTI Gray Matter, Midway US / Ubisoft EU, septiembre 1998; título EU
  "Gex 3D: Enter the Gecko"), APIs (Direct3D 5, Glide), D3D nativo solo en
  versión US, instalación manual con `.reg`, parches F-01–F-04, caps de FPS
  (30 / 24 con parche D3D), CD-audio problemático, sin ratón, crash
  dgVoodoo2+vorpX, nota LAA, requisitos (Win 95/98/ME, P166, 32 MB, DX5).
- Confianza: media (wiki curada, pero sin verificar por nosotros).

## S-02 — Foro Zeus Software (nGlide): hilos de compatibilidad Gex

- URL: <https://www.zeus-software.com/forum/> (hilo "Nglide with ATi legacy cards")
- Datos que aporta: el juego pide 75 Hz; Voodoo2 real 512x384@60; "Gex2 works
  too fast even at 60fps, needs a 30fps cap"; exe capeado en la compat list;
  reportes de "wrong drawing" y "pure virtual function call".
- Confianza: media-alta en lo observacional (autor del wrapper).

## S-03 — tgames.fr: parche D3D (PAL) + FPS Limiter + configs Win10

- URLs citadas por PCGamingWiki:
  - Parche D3D: tgames.fr `patch-direct-3d-gex-3d-enter-the-gecko…`
  - FPS Limiter: tgames.fr `patch-3dfx-fps-limiter…`
  - Configs Win10: tgames.fr `gex3d_windows10.zip`
- Estado: localizados, **pendiente de descarga y análisis**.
- Confianza: desconocida (origen comunitario sin autoría clara).

## S-04 — Zeus Software: `gex2_patch.zip` (exe de reemplazo)

- URL: <https://www.zeus-software.com/files/nglide/gex2_patch.zip>
- Estado: localizado, **pendiente de descarga y diff binario**.
- Nota (Abandonware France): contiene `GEX3D.exe` que sobrescribe al original.

## S-05 — VOGONS: "Windows Game - Gex Enter The Gecko"

- URL: <https://www.vogons.org/viewtopic.php?t=40033>
- Datos que aporta: fix del códec Indeo para la intro (`ir32_32.dll` +
  registro `drivers.desc`); pantalla en negro con música de fondo.
- Confianza: media (contexto Wine, trasladable con cautela a Windows).

## S-06 — Reddit r/gex (reportes de usuarios)

- Hilos: "Gex enter the gecko on PC?" (2021), "Finally got Enter the Gecko
  to work on my PC! But there's an annoying problem…" (2021).
- Datos que aporta: música ausente; música que se corta tras cada nivel;
  problemas para pasar de la pantalla de título.
- Confianza: baja (reportes aislados, útil como lista de síntomas).

## S-07 — MyAbandonware: comentarios de la versión Windows

- URL: <https://www.myabandonware.com/game/gex-enter-the-gecko-dnl>
- Datos que aporta: problemas de detección del CD, `glide2x.dll` faltante,
  pantalla negra + crash, intro sin reproducir, volumen in-game sin efecto,
  anomalías de tamaño de ventana.
- Confianza: baja (reportes aislados).

## S-08 — Abandonware France: ficha + truco Vista/nGlide

- URL: <https://www.abandonware-france.org/ltf_abandon/ltf_jeu.php?id=1876>
- Datos que aporta: nGlide 0.99 (no 1.03) en Vista; procedimiento con
  `gex2_patch.zip`.
- Confianza: baja-media (datado, probablemente obsoleto).

## S-09 — patches-scrolls.de: entradas "patch for 3dfx PC" / "fix PC"

- URL: <https://www.patches-scrolls.de/patch/1826/7>
- Estado: **pendiente de identificar contenido y autoría**.

## S-10 — PCGW Community: demo oficial (Toon TV, 6.4 MB)

- URL: <https://community.pcgamingwiki.com/files/file/281-gex-enter-the-gecko-demo/>
- Datos que aporta: demo 3Dfx-only; espejo de archive.org (`swizzle_demu_GEX2`).
- Utilidad: posible binario de referencia legal y ligero para análisis inicial.
- Estado: **pendiente de descarga y análisis**.

## S-11 — Gex64Decomp (referencia N64, no PC)

- URLs: <https://github.com/matbourgon/gex64decomp>,
  <https://github.com/Tokatta007/Gex64Decomp>
- Utilidad: nombres/sistemas/lógica como referencia arquitectónica. No asumir
  identidad con el port PC.

## S-12 — Gex Trilogy (2025, Limited Run / Carbon Engine)

- Dato: reedición basada en versiones PlayStation vía emulación (lanzada
  junio 2025 en PC/PS/Xbox/Switch). Útil solo para comparar comportamiento;
  no comparte código con el port PC.

## S-13 — Internet Archive: imágenes de CD documentadas

- `gex-3d-enter-the-gecko-pc-pointsoft` (FR, Pointsoft, pistas CD-audio),
  `gex-enter-the-gecko-3dfx` (bundle Quantum3D Raven).
- Utilidad: evidencia de estructura del CD (pista datos + pistas audio) y de
  variantes regionales. **No descargar material propietario al repo.**

## Pendiente del mantenedor

- [ ] Enlaces y recursos adicionales anunciados (por recibir).
- [ ] Archivos originales del juego (por recibir; irán a Drive, nunca al repo).
