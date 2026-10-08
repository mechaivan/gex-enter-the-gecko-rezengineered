# RESEARCH — Fuentes y jerarquía de referencias

## 0. Jerarquía de referencias (decisión del proyecto, 2026-10-08)

1. **Fuente de verdad: el Gex: Enter the Gecko original para PC.**
   El comportamiento a preservar es el del port PC en su contexto. Toda
   decisión de implementación se juzga contra él.
2. **Referencias secundarias (otras versiones de Gex):** N64 (Gex64Decomp),
   guías PS1 de speedrun, etc. Solo sirven para distinguir *"comportamiento
   original de la familia Gex"* de *"problemas específicos del port de PC"*.
   **No se debe intentar reproducir el comportamiento de otras versiones.**
3. **Gex Trilogy (2025) NO forma parte de las referencias del proyecto.**
   Se mantiene únicamente como contexto histórico (S-12).

Las fuentes son material de investigación, **no** instrucciones ciegas. Todo
está pendiente de contraste. Recopilación inicial: 2026-10-08.

## S-01 — PCGamingWiki: "Gex: Enter the Gecko" ⭐ referencia pública principal PC

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

## S-11 — Gex64Decomp (REFERENCIA SECUNDARIA — no es código PC)

- URLs: <https://github.com/matbourgon/gex64decomp>
  (fork: <https://github.com/Tokatta007/Gex64Decomp>)
- Qué es: decompilación WIP de *Gex 64* (N64, MIPS) con splat + decomp.me;
  activa (211 commits, último sep 2026). Requiere ROM USA propia (`gex64.z64`).
  flags `-O2`, ficheros mips1/mips3; esquema de nombres aún provisional.
- Utilidad: nombres, sistemas, lógica como **referencia arquitectónica** para
  distinguir comportamiento de la familia Gex vs problemas del port PC.
- Advertencia: **NO es el código de la versión PC** (port distinto:
  LTI Gray Matter; otra plataforma y CPU). Nunca fuente de verdad.
- Estado: catalogado como referencia secundaria.

## S-12 — Gex Trilogy (2025) — CONTEXTO, no referencia

- Por decisión del proyecto (2026-10-08), Gex Trilogy (Limited Run / Carbon
  Engine, basado en versiones PlayStation vía emulación) **no** es referencia.
  Se conserva esta nota solo como contexto histórico.

## S-13 — Internet Archive: imágenes de CD documentadas

- `gex-3d-enter-the-gecko-pc-pointsoft` (FR, Pointsoft, pistas CD-audio),
  `gex-enter-the-gecko-3dfx` (bundle Quantum3D Raven).
- Utilidad: evidencia de estructura del CD (pista datos + pistas audio) y de
  variantes regionales. **No descargar material propietario al repo.**

## S-14 — speedrun.com: "PC Version Setup Package" (Mysticore) ⭐

- URL: <https://www.speedrun.com/gex2/resources/e3dsk>
- Qué es: paquete (Drive, 25-01-2022, ~411 MB: `Gex 2 PC (Patches & Tools).zip`)
  que "incluye todo lo necesario para que la versión PC NTSC funcione en
  sistemas modernos sin el disco físico", con README de instrucciones.
- Utilidad: procedimiento real de setup usado por speedrunners (versión,
  ejecutable, fixes, configuración). Análisis pendiente.
- Copia de trabajo: `Drive → REZengineered/research/` (copiado 2026-10-08
  desde el enlace público; uso privado de investigación, no redistribuir).
- Pendiente: extraer README + inventario de parches/herramientas incluidas
  (requiere descarga en máquina con espacio; 411 MB superan este sandbox).
- Confianza: media-alta (mismo autor modera el leaderboard PC).

## S-15 — speedrun.com: PS1 Any% Guide + diferencias de versión (secundaria)

- URL: <https://www.speedrun.com/gex2/guides/le6ak>
- Qué es: guía de mecánicas/rutas basada en PS1; indica que "muchas cosas
  aplican a PC, no todo" y remite a diferencias de versión (N64/PC).
- Utilidad secundaria: entender mecánicas originales vs particularidades PC.
- Estado: pendiente de lectura detallada.

## S-16 — DxWrapper (elishacloud) — herramienta de referencia (open source)

- URL: <https://github.com/elishacloud/dxwrapper>
- Qué es: wrapper open-source de DLLs DirectX para Win10/11: convierte
  DirectDraw/D3D 1–7 → D3D9 (Dd7to9, cubre **D3D5**), D3D8 → D9, DirectInput
  1–7 → 8, hooks de DirectSound; permite cargar `.asi`; hack de resolución
  legacy; modo ventana; FPS counter.
- Utilidad: (a) ayuda de testing/compatibilidad para la ruta D3D del juego;
  (b) **código abierto para estudiar** cómo se interceptan/solucionan APIs
  legacy (no copiar a ciegas: entender y decidir solución propia).
- Estado: catalogado; probar en Fase 5 contra ruta D3D si aplica.

## S-17 — dgVoodoo2 (dege-diosg) — herramienta de referencia (freeware, NO OSS)

- URLs: <https://github.com/dege-diosg/dgVoodoo2> · <https://dgvoodoo2.com/>
- Qué es: wrapper freeware (no open-source) Glide/DirectDraw/D3D3–9 → D3D11/12.
  Instalación por DLLs junto al exe + `dgVoodooCpl.exe`.
- Utilidad: alternativa de testing para rutas Glide y D3D; comparar
  comportamiento nGlide vs dgVoodoo2 vs nativo.
- Nota: PCGamingWiki reporta crash dgVoodoo2 + vorpX (I-10, sin verificar).
- Estado: catalogado; probar en Fase 5.

## S-18 — REA (morluto/rea) — herramienta RE del proyecto (MIT)

- URL: <https://github.com/morluto/rea>
- Qué es: CLI + servidor MCP open-source (MIT, Node 22+) que da a agentes IA
  una vía uniforme para inspeccionar software sin fuente: decompilación de
  binarios nativos vía Hopper o Ghidra aportado (12.1.x), análisis estático
  JS/Electron, evidencias y limitaciones por conclusión. Setup:
  `npx rea-agents setup`; diagnóstico por proveedor:
  `rea doctor --provider ghidra --json`.
- Uso en el proyecto: capa de orquestación/evidencia del RE; Ghidra como motor
  profundo. CLI 5.0.0 ya instalado en sandbox, sin backend nativo (ver TOOLKIT).
- Estado: adoptado como herramienta oficial del proyecto.

## Pendiente del mantenedor

- [x] Enlaces principales recibidos (PCGamingWiki, REA, Gex64Decomp como
  secundaria, speedrun/setup). Jerarquía de referencias fijada.
- [ ] Archivos originales del juego (irán a Drive `originals/`, nunca al repo).
- [ ] Extraer de S-14 (setup package): README + lista de parches/herramientas
  + hashes de ejecutables incluidos.
