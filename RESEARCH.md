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

Las fuentes son material de investigación, **no** instrucciones ciegas.
Recopilación inicial: 2026-10-08. El contraste propio comenzó en Fase 1
(inventario + análisis estático de los originales EU — ver
[docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md));
lo ya verificado se indica en § «Áreas de investigación futura».

## S-01 — PCGamingWiki: "Gex: Enter the Gecko" ⭐ referencia pública principal PC

- URL: <https://www.pcgamingwiki.com/wiki/Gex:_Enter_the_Gecko>
- Datos que aporta: ficha del port PC (desarrollo Crystal Dynamics, port PC
  LTI Gray Matter, Midway US / Ubisoft EU, septiembre 1998; título EU
  "Gex 3D: Enter the Gecko"), APIs (Direct3D 5, Glide), D3D nativo solo en
  versión US, instalación manual con `.reg`, parches F-01–F-04, caps de FPS
  (30 / 24 con parche D3D), CD-audio problemático, sin ratón, crash
  dgVoodoo2+vorpX, nota LAA, requisitos (Win 95/98/ME, P166, 32 MB, DX5).
- Lote 1 (2026-10-09): página releída íntegra. Datos nuevos: sección FPS
  Limiter (enlaza F-03), enlace alternativo a F-01 + `gex3d_windows10.zip`,
  save location VACÍA (desconocida), sin ratón, Red Book puede fallar.
- Confianza: media-alta como hub procedural (wiki curada; parte manual +
  `.reg` + admin ejecutados en Hito 1, sin F-01). Lote 2 (2026-10-09):
  relectura íntegra sin cambios; citas verbatim confirmadas (FPS limiter,
  caps 30/24, VSync vía nGlide, sin ratón, Red Book).

## S-02 — Foro Zeus Software (nGlide): hilos de compatibilidad Gex

- URL: <https://www.zeus-software.com/forum/> (hilo "Nglide with ATi legacy cards")
- Datos que aporta: el juego pide 75 Hz; Voodoo2 real 512x384@60; "Gex2 works
  too fast even at 60fps, needs a 30fps cap" (cita del problema que motiva el
  parche; cap en binario sin confirmar — ver F-04); reportes de
  "wrong drawing" y "pure virtual function call".
- Lote 1 (2026-10-09): fila «Gex 2: Enter The Gecko» verificada verbatim
  («replace 'gex3d.exe' and install patch» → `gex2_patch.zip`, UN solo
  parche: base del veredicto F-04). nGlide vigente 2.10 (Win XP–11;
  Glide 2.11/2.60/3.10; 0.99 y 1.03 aún hospedadas); FAQ: «too fast» →
  activar V-Sync. Hilo «No music in Gex 2» (t=743, 13 posts, 2014→2024)
  leído íntegro: Zeus pide un wrapper winmm; `_inmm.dll` daría música +
  loop (1 reporte 2024); bugs presentes también en Voodoo real.
- Confianza: media-alta en lo observacional (autor del wrapper).
- Instalador/configurador (fuentes públicas, 2026-10-09, base de A2.2):
  el instalador coloca su `glide2x.dll` + `nglide_config.exe` en el dir.
  de sistema (SysWOW64 en Win64); backend vídeo DirectX; configurator
  2.10: backend, resolución, aspecto, refresco, VSync (On por defecto),
  gamma, splash 3dfx; ajustes globales. Confianza media (guías
  comunitarias consistentes). <https://steamcommunity.com/sharedfiles/filedetails/?id=127637434>
  <https://dosbox-x.com/wiki/Guide:Setting-up-3dfx-Voodoo-in-DOSBox%E2%80%90X>
  <https://steamcommunity.com/app/38450/discussions/0/595161733884157883/>

## S-03 — tgames.fr: hub de parches Gex 3D (Tgames) ⭐

- Hilos leídos íntegros en Lote 1 (2026-10-09), autor Tgames (admin, activo
  desde 2008); descargas solo para miembros (contenido no inspeccionado):
  - F-01 D3D (PAL): `…/patch-patch-direct-3d-gex-3d-enter-the-gecko-windows-7-8-10-t12116.html` (OP + 6 respuestas).
  - F-03 FPS Limiter: `…/patch-patch-3dfx-fps-limiter-gex-3d-enter-the-gecko-t12190.html` (OP + «Merci!»).
  - F-10 music handler: `…/patch-gex-3d-pc-support-des-musiques-sous-windows-7-8-10-t12122.html` (3 posts).
  - F-11 NO-CD D3D: `…/no-cd-no-cd-gex-3d-enter-the-gecko-version-direct-3d-t12117.html` (solo OP).
  - F-12 debug tool: `…/trainer-gex-3d-enter-the-gecko-direct-3d-cheats-debug-t12121.html` (solo OP).
  - Base URL nuevo foro: `https://www.tgames.fr/pc/progs-pc/`.
- Re-verificación t12190 (2026-10-09, prep. P-F03): 2 posts, sin cambios
  de contenido; adjunto solo-miembros (sin URL directa pública; registro
  gratuito requerido); sin nº versión/hashes; README verbatim en F-03.
- Citados pero no inspeccionados: `gex3d_windows10.zip` (configs Win10/11),
  `GEX3DFX_PatchD3D_V1.2.zip`, `gex3d_d3d_debugtool.zip` (nombres vistos en
  hilos/snippets; no descargados).
- t12123 («Full Game…», repack de juego completo): FUERA DE ALCANCE, no
  consultado.
- Confianza: media-alta en lo descriptivo (autor identificado y prolífico;
  binarios sin verificar por nosotros).

## S-04 — Zeus Software: `gex2_patch.zip` (exe de reemplazo)

- URL: <https://www.zeus-software.com/files/nglide/gex2_patch.zip>
- Lote 1 (2026-10-09): fila de compatibilidad + procedimiento AF verificados
  (payload = `GEX3D.exe` de reemplazo). **Pendiente de descarga y diff
  binario** (incluye verificar si ya capa a 30 FPS: veredicto F-04).
- Nota (Abandonware France): contiene `GEX3D.exe` que sobrescribe al original.

## S-05 — VOGONS: "Windows Game - Gex Enter The Gecko" (hilo NO resuelto)

- URL: <https://www.vogons.org/viewtopic.php?t=40033>
- Datos que aporta: fix del códec Indeo para la intro (`ir32_32.dll` +
  registro `drivers.desc`); pantalla en negro con música de fondo.
- Lote 1 (2026-10-09): 24 respuestas (2014-07-15→18) leídas íntegras.
  Resultado: NO RESUELTO (reportero atascado en `wine: command not found`;
  Jorpho escéptico desde el inicio + aviso sobre binarios; Dominus → WineHQ).
- Confianza: baja (evidencia débil, fuente única, contexto Wine/OSX).

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
- Lote 2 — extras (metadatos, sin descargar): «Official Patch» EN 609 KB
  + «Patch to run the game in Glide-mode using the nGlide emulator -
  unpack into the game folder, replacing .exe file and run the .bat
  file» EN 570 KB (posible derivado F-02, sin identificar; ver F-02).
  Testimonios débiles adyacentes (I-13/I-14/I-12): volumen sin efecto,
  intro sin reproducir, detección CD.
- Confianza: baja (reportes aislados).

## S-08 — Abandonware France: ficha + truco Vista/nGlide

- URL: <https://www.abandonware-france.org/ltf_abandon/ltf_jeu.php?id=1876>
- Datos que aporta: nGlide 0.99 (no 1.03) en Vista; procedimiento con
  `gex2_patch.zip`.
- Lote 1 (2026-10-09): ficha (id=1876) + truco «Fonctionnement sous Vista»
  leídos verbatim. Edición EU FR (Ubi Soft + Pointsoft + Proein ES);
  Technique: 3DFX obligatoria, música CD exige PRIMER lector, bug de
  repetición por nivel; voces Gould (US) / Phillips (EU). Hilo antiguo del
  foro AF («[RELEASE] Patch Direct 3D… V1.0 by Tgames», 2018): URL muerta
  tras migración a vBulletin 6 — solo snippets de buscador (nombres de
  ficheros, NOPs, moonjump); NO verificados de primera mano.
- Lote 2 — liens/telechargements (leído 2026-10-09, metadatos): espejo
  «Patch de transformation 3DFX→Direct3D» (356 Ko, UK, «mis à jour par
  Tgames»: «plus besoin de carte 3DFX ni de wrapper Glide») + «Version
  automatique» (198775 Ko: «patché et configuré pour Windows modernes»)
  + «Version CD-ROM» US (508542 Ko: «doublages refaits», compatible
  Direct3D) + «Version CD-ROM» FR (409885 Ko: «doublages britanniques»,
  3DFX/wrappers). Créditos: Tgames (D3D, Patch). Sin hashes; no descargar.
- Confianza: media en lo leído (datado pero verbatim).

## S-09 — patches-scrolls.de: entradas "patch for 3dfx PC" / "fix PC"

- URL: <https://www.patches-scrolls.de/patch/1826/7>
- Lote 1 (2026-10-09): página releída (2 chunks): «Gex II», 16.08.13,
  «Editiert von: nobody», sin descripciones ni descargas visibles.
- Estado: **contenido y autoría UNKNOWN**.

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
- Utilidad: evidencia de estructura del CD y de variantes regionales (la
  estructura 1+16 del CD EU ya está confirmada con originales propios,
  Fase 1). **No descargar material propietario al repo.**

## S-14 — speedrun.com: "PC Version Setup Package" (Mysticore) ⭐

- URL: <https://www.speedrun.com/gex2/resources/e3dsk>
- **CONFIRMADO (metadatos Drive autorizados, solo lectura, 2026-10-09):**
  fichero público `Gex 2 PC (Patches & Tools).zip`
  (ID `1XvJa18j-82iBX1To4VUyuqsfp50xwVYf`, enlazado desde la página),
  `application/x-zip-compressed`, **411301184 bytes** (= 411.3 MB decimales
  = 392.2 MiB), creado 2022-01-25T09:51:02Z. MD5
  `4a3aed9dde7152de6597b2e8b53cf467`, SHA-256
  `60d9e430394ab026e4642d29769d8a4fbf1e8690d3d0bb3c932be5b1b7e1a36b`.
  Copia de trabajo `…[ORIGINAL DESCARGADO 2026-10-08].zip` en carpeta
  `research`: mismo tamaño y mismos hashes → **bit-idéntica al original
  público**. Discrepancia 411 vs ~393 resuelta con evidencia: redondeo en
  unidades distintas del mismo fichero. (Identidad del propietario visible
  en metadatos; no registrada por privacidad.)
- **Re-verificación 2026-10-09 (2ª comprobación, solo metadatos):** ambas
  copias accesibles, no trashed, sin modificar desde la 1ª comprobación;
  MD5/SHA-256 coinciden con lo documentado.
- **INVENTARIO (2026-10-09, carpeta `S-14_extracted` subida por el
  mantenedor; 22 entradas = 3 carpetas + 19 ficheros; nombres+metadatos,
  binarios NO leídos):** raíz: `README.txt` (2131 B, md5
  `14e8d1a7ea0afd778bc2d087a8450e50`) + `Game Files/` + 3 instaladores
  (`nGlide210_setup.exe` 3301587 B, `WinCDEmu-4.1.exe` 1576544 B,
  `_inmm238.exe` 311653 B, 2006); `Game Files/`: `gex3d.exe` «parcheado»
  (1665536 B, 2013-10-04, md5 `0a65f3ada9bef8c842e52cf0a7987f67`) +
  `music/` con `track01–15.wav` (15/15, 431409348 B total, mtime uniforme
  1998-03-28). Total extraído ≈ 438 MB vs ZIP 411.3 MB (ratio 0.94,
  coherente). Sin duplicados, sin rutas sospechosas, sin zips anidados.
  4 .exe = CAUTELA (nunca ejecutados ni descargados). Resto de md5,
  recuperables vía Drive API.
- **README LEÍDO (`README.txt`, texto completo, 2026-10-09):** declara 3
  fases: (1) instalar WinCDEmu + montar imagen `Gex3DD3D.ccd` (URL Drive
  externa, NO inspeccionada) + SETUP.EXE; (2) copiar `Game Files/` (exe
  parcheado + `music/`) sobre la instalación + instalar nGlide 2.10;
  (3) música vía `_inmm` (player DirectShow + `_inmm.ini` con los WAV,
  guardado en el juego). Rutas `…\Crystal Dynamics\gex23d` (= D3D).
  Resultado declarado: jugar con `gex3d.exe` sin disco. Procedimiento NO
  ejecutado ni verificado por el proyecto.
- **DECLARADO POR LA FUENTE (página releída 2026-10-09, sin verificar):**
  «Includes everything you need to get the NTSC PC version to run on modern
  systems, without needing the physical disc. Simply follow the instructions
  in the README.» Recurso tipo «Patch», actualizado hace ~4 años por
  Mysticore (moderador de gex2, verificado en la página). La página NO
  publica contenidos, README, hashes ni tamaño.
- **DESCONOCIDO:** contenido/comportamiento de los 4 .exe y audio real de
  los WAV (inventariados por nombre+metadatos, contenido NO leído);
  eficacia y seguridad del procedimiento (no ejecutado); procedencia del
  exe 2013; correspondencia track01–15 ↔ pistas CD (15 WAV frente a 16
  CD-DA en nuestro EU); aplicabilidad a EU.
- **SIGUIENTE PASO (Fase 2, con autorización):** diff binario del
  `gex3d.exe` 2013 vs original; resto, solo lectura. Nada más pendiente
  en Fase 1 para S-14.
- **BLOQUEO ANTERIOR SUPERADO (índice):** el listado se obtuvo vía la
  carpeta `S-14_extracted` subida por el mantenedor (2026-10-09); la
  limitación técnica del entorno (sin lectura parcial de zips) persiste.
- **AUTENTICIDAD (9ª sesión, hashes públicos):** md5 `cd30d314…1a20` +
  SHA1 `81762943…5097` (Drive/Falcon) = hashes publicados por Zeus para
  `nGlide210_setup.exe` (foro t=557) ⇒ instalador OFICIAL nGlide 2.10
  (confianza alta). Ver S-22 (payload).
- Copia de trabajo: `Drive → REZengineered/research/` (2026-10-08; uso
  privado de investigación, no redistribuir).
- Utilidad futura: procedimiento real de setup usado por speedrunners
  (versión, ejecutable, fixes, configuración).
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
- Utilidad: (a) ayuda de testing/compatibilidad para la ruta D3D (versión
  US / parche F-01: el binario EU inventariado no tiene ruta D3D);
  (b) **código abierto para estudiar** cómo se interceptan/solucionan APIs
  legacy (no copiar a ciegas: entender y decidir solución propia).
- Estado: catalogado; probar en Fase 5 donde aplique.

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

## S-19 — Wrappers/herramientas genéricas, ronda 2 (2026-10-08)

- **DDrawCompat** (<https://github.com/narzoul/DDrawCompat>): wrapper
  DirectDraw/D3D1–7, open source (0BSD), activo. Vía PCGW Community + GOG
  forums. → T-04.
- **DxWnd** (ghotik): hooker/ventanado genérico; mirror GitHub estancado
  (2017), upstream actual pendiente de localizar. → T-05.
- **WineD3D for Windows** (fdossena.com): DX1–7 sobre OpenGL; mención en guía
  Steam de wrappers, pendiente de verificación directa. → T-06.
- Guías consultadas: Steam "DirectX Wrappers/Emulation" (dgVoodoo per-DX),
  insertmorecoins DxWrapper 2026 (config Dd7to9/Dinputto8/ventana).

## S-20 — Reparto vocal UK/USA (Wikipedia, VGFacts, BTVA) — REFERENCE general

- URLs: <https://en.wikipedia.org/wiki/Gex:_Enter_the_Gecko> (verificada
  2026-10-09: infobox + Plot + Development) ·
  <https://www.vgfacts.com/game/gexenterthegecko/trivia-846/> (snippet) ·
  BTVA (snippet).
- Hechos: Gex = Dana Gould (NA), Leslie Phillips (EU), Mitsuo Senda (JP).
  Gould escribió citas/chistes y grabó «over 700 voice-overs»; Phillips =
  variación «muy distinta, más culta y refinada» (VGFacts). N64: voces
  reducidas (~100 samples) y cutscenes omitidas (solo consola, no aplica
  a PC).
- Alcance: fuentes GENERALES (artículo liderado por PS1), NO PC-específicas.
  Confirmación PC: S-08 (AF: Gould US / Phillips EU en ficha PC) + S-03
  (Tgames: carpetas `voice` US / `voiceuk` EU) + inventario §8 (400 `.SAG`
  UK en `AUDIO/VOICEUK/`).
- UNKNOWN (base de M-25): contenido/cobertura del set USA de PC;
  paralelismo frase a frase; separación voces FMV vs in-game; mecanismo de
  selección del exe; formato `.SAG`.
- Confianza: media como referencia de reparto; nula para detalles PC-USA.

## S-21 — Identidad de `3dfxSpl2.dll` (splash Glide 2.x) — REFERENCE

- Fuentes (2026-10-09): r/3dfx («3dfxSpl2.DLL - Splash screen for Glide 2.x
  games») + VOGONS («3dfxspl2 is for Glide 2x games»; quitarlas elimina el
  splash). Dos fuentes comunitarias independientes coinciden.
- Uso en el proyecto: explica el splash 3dfx del arranque (Hito 1, punto 8)
  y la `3dfxSpl2.dll` cargada (2ª sesión). Por sí sola no dice nada sobre
  el renderer activo.
- Confianza: media-alta como identidad del fichero; nula para versiones
  concretas de `glide2x` (ficha + fecha/firma en TESTING 4ª sesión:
  Banshee 2.61.00.0658 — corr. 7ª, fecha mostrada 2019-09-15 sin campo
  identificado, sin firma visible, SHA-256; wrapper = nGlide 2.10, 9ª).
- Corroboración propia (3ª sesión): la `3dfxSpl2.dll` cargada declara
  `3dfx Splash Screen` 1.0.0.4 — coherente con esta identidad (splash,
  no renderer).

## S-22 — Corpus público de `glide2x.dll` (fichas y hashes) — REFERENCE

- Fuentes (2026-10-09, fichas leídas, nada descargado):
  <https://www.dllme.com/dll/files/glide2x> (15 ficheros) +
  <https://www.pconlife.com/viewfileinfo/glide2x-dll/>.
- Dato objetivo: dllme hospeda NUESTRO hash antiguo
  (`7cbd095872e821b54cd6fa03f76aa22073271567175069c53ebb2e73b0299aab`,
  1.6 MB, MD5 `f59d9780abe6bcb89433bdad4c8c5d59`, subido mar-2024)
  etiquetado «3Dfx, Glide for Voodoo Banshee/Voodoo3/Velocity,
  2.61.00.0658» ⇒ (a) la DLL pre-nGlide es copia de circulación
  pública; (b) la cadena «2.60.0.658» de TESTING 3ª sesión queda EN
  DISPUTA (extracción automática vs transcripción humana) ⇒ el
  «cambio de versión» 3ª→5ª sesión NO está confirmado (relectura
  posible; hash nuevo pendiente).
- `2.61.00.0658` indexada (1.2–1.3 MB, MD5 distintos): la cadena de
  la ficha 5ª existe en familia 3dfx pública. Sin hash propio: no
  discrimina vendedor.
- Hábito de marca: las `glide2x.dll` de nGlide v1.02/v1.05 declaran
  ProductName «nGlide vX.XX» (pconlife). nGlide 2.10: metadatos
  propios SIN referencia ⇒ un fichero con metadatos puramente 3dfx
  NO es atribuible a nGlide por defecto.
- dgVoodoo declara versiones propias (2.60.0.0 / producto 2.8.3.2):
  distinguible por metadatos cuando se tengan.
- nGlide anterior a Hito 1 NO excluido (instalaciones previas
  UNKNOWN): el fichero antiguo ya renderizaba ⇒ el traductor ya
  estaba en la ruta; la 5ª «instalación» pudo ser reinstalación.
- Uso: corpus de comparación para el futuro hash de
  `SysWOW64\glide2x.dll` (A2.3). Etiquetas de estos sitios: confianza
  baja; hashes: objetivos.
- RESOLUCIÓN 7ª sesión (PC): hash idéntico ⇒ sin sustitución; relectura
  `2.61.00.0658` ⇒ disputa cerrada (3ª: error transcripción). Recurso
  ES+EN probable (mismo hash, dos idiomas).
- Docs oficiales nGlide (zeus-software.com/downloads/nglide, leídos
  2026-10-09): backend Direct3D+Vulkan; Glide 2.60 API; instalador sin
  política documentada de copia de seguridad/sobrescritura/versiones;
  FAQ confirma VSync/splash en configurador. Foro t=560: nGlide SÍ
  aparece en Agregar/quitar; desinstalación manual = borrar glide*.dll
  (sin copias mencionadas).
- Coexistencia configurador+fichero antiguo, explicaciones: (1) el
  instalador omitió un fichero preexistente (solo documentado para
  versiones nGlide previas: fuente estrecha); (2) 7cbd es el payload
  de nGlide 2.x (sin referencia; v1.x marcaba «nGlide»); (3) sustituyó
  y algo restauró (sin fuente). Fecha de instalación (registro,
  lectura) discrimina (1)/(2) de forma fuerte.
- 8ª sesión (PC + Drive, solo lectura): registro nGlide 2.10 sin fechas
  ⇒ temporal inconcluso; configurador triple-2.10; DLL reconfirmada.
  Instalador fijado (S-14_extracted: 3301587 B, md5 cd30d314…1a20,
  SHA-256 3cfcd03a…7a7a; tamaño coherente con 3.14–3.15 MB del corpus)
  pero payload sin inspeccionar (bloqueo técnico, no de permiso) ⇒
  hipótesis (1)/(2) vivas entonces. Sin referencia pública del payload
  (negativo 8ª; 9ª: informe Falcon la aporta — ver abajo). Discriminador
  ejecutado: imports PE (9ª).
- 9ª sesión — INSTALADOR OFICIAL (hashes vendor): Zeus (foro t=557,
  post 2020-04-10) publica para `nGlide210_setup.exe` MD5
  `cd30d314c3f1470cef1a35300fda1a20` + SHA1
  `81762943ca942b25bea37645123a19a9a3545097` + CRC32 `703d395e` ⇒ S-14
  (mismos md5/SHA-256…) = OFICIAL (confianza alta). Blog independiente
  cita el mismo SHA-256 (corroboración débil).
- 9ª sesión — PAYLOAD (Falcon Sandbox, muestra 3cfcd03a… = S-14 bit a
  bit; detonada por terceros 2020-08-10, veredicto 0/100 limpio):
  instalador NSIS (TrID 94.6%) que suelta en `%WINDIR%\System32`
  (huésped Win7-32; en Win64 → SysWOW64, S-02): `glide.dll`
  (1536000 B), **`glide2x.dll` (1630208 B, MD5 `f59d9780…`,
  SHA-256 `7cbd…9aab` = NUESTRA DLL)**, `glide3x.dll` (1732608 B),
  `3DfxSpl[2].dll` (1105408 B c/u), `nglide_config.exe` (348160 B),
  `nglide_uninst.exe`, readme + accesos. Tabla de hashes abajo. ⇒
  **ATRIBUCIÓN CONFIRMADA: la DLL cargada ES nGlide 2.10** (escenario
  (2) GANA; (1)/(3) muertos para este fichero). A2.3 «Atribución
  origen (L)» ✓. Abierta: adquisición pre-Hito 1 (instalación previa
  vs copia circulante).
- 9ª sesión — IMPORTS (PC): x86 + 6 DLL sistema, sin D3D/DXGI/Vulkan
  estáticos ⇒ nGlide 2.10 enlaza backend dinámico (LoadLibrary).
  Metadatos 3dfx/ES-EN/dllme = marca del payload, no de época.
- Drops `nGlide210_setup.exe` (Falcon, 2020-08-10; huésped Win7-32):
  - `glide2x.dll` 1630208 B — MD5 `f59d9780abe6bcb89433bdad4c8c5d59` —
    SHA-256 `7cbd…9aab` (completo en TESTING 3ª).
  - `glide.dll` 1536000 B — MD5 `d1ad25821fe5b92b66697569f09d0f4c` —
    SHA-256 `3e1bcd94fc30311bebd21b916ed38d30e540a06dfd658cc8dfcdbe510a8587a4`.
  - `glide3x.dll` 1732608 B — MD5 `c3680e912fc7e84f141cbb700425da68` —
    SHA-256 `dd765740d52367e23965d718b5ebd3d56ec72e2985608b4c4c4e233a8d87fdc6`.
  - `3DfxSpl.dll` 1105408 B — MD5 `eff462cab8dab3a45e88b2622bfa7496` —
    SHA-256 `1ee8b2b4963b95f600ac683907778304f75c559aa6831a3f203efafc8d67d45a`.
  - `3DfxSpl2.dll` 1105408 B — MD5 `08a1b06fe2fee5a1e3b33f1d71b84705` —
    SHA-256 `262c70749ac24b4d3691e39767d3e01b5b4957b9b82768186e5faa58f395ceba`.
  - `nglide_config.exe` 348160 B — MD5 `b3013435b3332e1b4ee23240551088e1` —
    SHA-256 `c145622c72a262a80257ba00b9aa234e004ab0ab8fb0625bfff7454c42b7dc67`.
  - `nglide_uninst.exe` 70537 B — MD5 `21121597d281b89fe24a00beabcc3d81` —
    SHA-256 `fa5b760cfb4e3dafdebbbea36789dd22ae704cf44b1e39caa01432813c1ef6a9`.
  - `nglide_readme.txt` 24929 B — MD5 `101e59351307196e7ad44940460317ee` —
    SHA-256 `098acd1c5ed5f2fa4de519374df628a95b82fac1cfd1d991d4de19cce4769fb9`.
  (SHA1 de cada drop, en el informe Falcon.)

## S-23 — Parche oficial 3dfx + «Generic Update» (1999, Midway/US) — REFERENCE

- Fuentes (2026-10-09, Lote 2; leídas, nada descargado):
  3dfxzone.it objid=1004 (ficha + readme verbatim) + Patches Scrolls
  archivo-1998 + MyAbandonware extras + soggi.org («coming someday»).
- Ficha 3dfxzone (verbatim): «3dfx Support Patch updates Gex… adding
  3dfx support via Glide APIs. Copy manually gex23Dfx.exe from
  “Gex-Enter-the-Gecko_3dfx_Patch” into …gex23d. If still problems,
  try generic patch in “Gex-Enter-the-Gecko_Generic_Patch”.» 1.18 MB.
- Readme v1.0 (verbatim, SOLO instalación): requiere tarjeta 3Dfx +
  instalación TÍPICA (mínima no vale); dir. por defecto `…\gex23d`;
  mover `gex23Dfx.exe`; atajos Start Menu; LEGAL ©1999 Crystal
  Dynamics, distribuido por Midway (→ lado US, 1999, NO 1998).
- Secundarios: «3D card update 607K, mostly Voodoo Rush fixes»
  (Patches Scrolls); «Official Patch» EN 609 KB + parche nGlide
  exe+.bat 570 KB (MAW extras). 607≈609 coherente (unidades/redondeo;
  identidad sin probar); 1.18 MB ≈ suma de dos mitades (hipótesis).
- NEGATIVOS documentados: nombre `GX2PATCH.ZIP` NO localizado en
  ninguna fuente consultada; versiones origen/destino UNKNOWN; lista
  de errores UNKNOWN (readme sin changelog); contenido del «Generic
  Patch» UNKNOWN; hashes no publicados; limitador FPS: SIN EVIDENCIA.
- Relación EU v1.00.000: SIN EVIDENCIA de compatibilidad (apunta a
  base `gex23d`/D3D; la EU es Glide-nativa → mecánicamente N/A).
  Utilidad: referencia de variantes US (exe-por-renderer: PATRÓN
  reutilizable como idea) + comparador RE futuro.
- Confianza: alta en existencia/contenido-descrito (readme primario);
  media en alcance (blurb secundario); nula en detalles no declarados.

## S-24 — Lote herramientas GitHub + upscale Mega (2026-10-09) — HUB

Método: API GitHub (metadatos + README + árbol + commits + releases;
sin clonar/compilar/ejecutar/descargar binarios). Fuentes pequeñas
leídas para evaluar formatos (vfx.rs, main.rs, file.h,
glideconstants.h). Wiki unLoKable leída (Home + SND-and-SMP). Mega:
fetch HTTP 500 (host fuera de la lista permitida) → sin verificación.

### H-01 — gex2-tools (SK83RJOSH): extractor `.VFX`→PNG, PC

- URL: <https://github.com/SK83RJOSH/gex2-tools> — «Tools for working
  with Gex: Enter The Gecko (PC)». Rust 2021 v0.1.0 (anyhow, binrw
  0.11.2, bitflags, image 0.24.6). 2023-06-17→23, 2 commits, muerto
  desde entonces. 0 releases, 1 estrella. SIN README, SIN LICENCIA.
- Hace (fuente leída, ~7.5 KB): CLI que filtra `.vfx`, parsea
  `File{texture_count, textures[]}`, descomprime cada textura y vuelca
  `{i}_{formato}.png` (RGBA8) en `<nivel>/`. Formatos: RGB8A1=1
  (1 B/px; brightness+rgb_0/rgb_1; workaround bug encoder), R7G6B5A1=11
  y ARGB4=12 (2 B/px); dims desde tamaño+aspecto.
- Cruce PC: `.VFX` = `LEVEL/*.VFX` del inventario §5 (36 pares).
  Match de formato CONFIRMADO; ejecución pendiente (Fase 2+). Código
  NO reutilizable sin licencia (pedir al autor).
- Confianza: alta (diseño PC + formato coincidente); media en
  corrección del decode (sin ejecutar).

### H-02 — Gex3DViewer (MatBourgon): visor/exportador niveles PC (WIP)

- URL: <https://github.com/MatBourgon/Gex3DViewer> — «Gex 3D Level
  Viewer». C++20/CMake; 2024-09-14→2025-04-07; 527 ficheros (mayoría
  vendored: GLFW/glad/glm/imgui); 5 releases 0.1→0.5 (sep 2024–ene
  2025, 1 asset c/u); 3 estrellas. SIN LICENCIA.
- README verbatim: «The engine is currently set up to work with the
  PC build and its files» — abre `.dfx` + `.vfx` parejo; WASD+ratón,
  wireframe. Build Windows (`lib/glfw/*.lib` + `runcmake.bat`).
- Capacidades (estructura+commits): mapreader (36 KB), script (17 KB),
  componentes (Path/ProxSig/QMark/Script/Timer/FlyBox/TV), visor 3D
  OpenGL+imgui, **exportación de modelos + texture sheets + nivel +
  skybox**, lector paths/rotaciones, `glideconstants.h` (enums
  GrAspectRatio/GrLOD/GrTextureFormat Glide reales).
- Cruce PC: .DFX/.VFX §5 + consts Glide coherentes con EU-Glide.
  Match CONFIRMADO; ejecución pendiente. Código NO reutilizable.
- Confianza: alta como utilidad PC candidata; media-baja en cobertura
  (WIP: «Unsure where this lands, but it runs»).

### R-03 — GexPSXLZSS (MatBourgon): LZSS del BIGFILE, PS1

- URL: <https://github.com/MatBourgon/GexPSXLZSS> — compresor/
  descompresor LZSS de ficheros de `BIGFILE.dat` de Gex 2 (el
  contenedor NO incluido: hay que parsear la cabecera aparte). C++,
  README completo, `test.cpp`, 3 releases (1.0, 1.0-dll Win x64,
  1.1; ene 2024), retoque README may 2026. SIN LICENCIA.
- Plataforma: PS1 (nombre PSX + `BIGFILE.dat`; PC EU: 0 `bigfile`).
  Utilidad: algoritmo de referencia si un contenedor PC resultara
  LZSS (sin evidencia hoy). NO es código PC.
- Confianza: alta (PS1); nula aplicabilidad PC directa.

### R-04 — unLoKable (SalsaGal): suite audio Crystal Dynamics (MIT)

- URL: <https://github.com/SalsaGal/unLoKable> — «A suite for Crystal
  Dynamics audio formats». Rust workspace: 15 tools (adsheader,
  adsloopfind, adsunloop, cds2seq, demul, demus, desnd, msqsplit,
  seqrepeat, sf2panlaw, vabfine, vabsmp, vagheader, vagsanitizer,
  vagunloop) + `core`; tests; releases 0.2.0→1.0.0 (jul 2024–ene
  2025); 25 estrellas. **Licencia MIT** (única reutilizable).
- Formatos (README+wiki íntegros): .SND/.SMP (juegos CD 1994–2000,
  «mostly PlayStation»; revisiones soul-reaver/prototype/**gex** =
  «early games such as Gex», magia `DNSa`; secuencias `QSMa`/`QESa`),
  .MUS/.SAM (2000–2007; PS2/Xbox/PC), .MUL (2003–2007, multiplexado);
  salidas: SEQ→MIDI, ADS/VAG, VAB→SF2/DLS, WAV/FLAC/…. demus `--pc`
  (PCM16, defecto) vs `--console` (VAG); desnd `-f gex`.
- Cruce PC EU: **0 correspondencias** (ninguna extensión en PC; audio
  PC = 37 .TAD + 400 .SAG + 9 pares .SAG/.JAM + CD-DA vía MCI).
  demus-PC cubre juegos 2000–2007, NO Gex 2 PC (1998, LTI Gray
  Matter). NO asumir compatibilidad.
- Valor: referencia metodológica (bancos/secuencias/loops) + magias
  para test barato Fase 2+ sobre .TAD/.SAG + cadena de preservación
  reutilizable si un stream PC resultara compatible.
- Confianza: alta (alcance documentado); hipótesis .TAD↔SMP sin base.

### Mega — upscale «Screen Titles» (sin verificar)

- URL: `https://mega.nz/file/3FdVyIRY#…` (id opaco, sin nombre de
  fichero). Fetch desde aquí: HTTP 500 (host no permitido).
- Verificable hoy: NADA (contenido/formato/dims/nº imágenes/licencia/
  permiso/autor: UNKNOWN). NO descargado, NO incorporado.
- Pendiente (lado mantenedor, M-23): nombre+tamaño del fichero,
  licencia/permiso del autor, correspondencia vs texturas .VFX
  extraídas (requiere H-01/H-02 validados), dimensiones/UV/
  transparencia/paleta/canales/formato, diseño pack HD opcional
  separado + reversible. Sin evidencia no hay compatibilidad.

## S-25 — Binarios F-03 en Drive + ejecución P-F03/B (2026-10-09)

- Carpeta `F-03_FPS_Limiter` (id `1hWPmFa9eDfQvMi7DXAupP0SJvW4BsdzW`,
  subida 2026-10-09): `EU/GEX3D.EXE` (`1VtRO2i18KdXy_sYq8lKxsYCmWAgZfmOe`),
  `US/GEX3D.EXE` (`1x6J78EE8C-bTM35zcnpeLs7AQTOjZX1v`),
  `Gex3DFX_Update.txt` (682 B, mtime 2025-08-20 = fecha OP).
- Hashes (Drive): EU 1559040 B md5 `3198350eb398db9a64771842d7781b4b`
  sha256 `7a9b6851…a3e8254`, mtime 1998-07-08; US 1665536 B md5
  `5dde47d87e2797a14e4ed8253443f96c` sha256 `22fdbe65…c0891d`, mtime
  1998-06-24. Original EU re-confirmado (md5 `692b1282…`).
- TXT verbatim = README del OP salvo `https://tgames.fr` (OP:
  `https://www.tgames.fr`). EU +1536 B vs original; US mismo tamaño
  que exe 2013 S-14 pero distinto hash (artefactos distintos).
- P-F03/B (observación mantenedor): B = 42–44 FPS menú+juego,
  cinemáticas ≈15, sim acelerada, audio normal. Rama «FPS↑»
  (inesperada) → identidad CONFIRMADA (E-1.1). Detalle: F-03,
  TESTING P-F03/B; E-1 completo; E-2 pendiente de autorización.
- Límite técnico: bytes inaccesibles desde aquí (egress Google
  bloqueado; download inline inviable ~2 MB base64) → estático
  profundo vía scripts solo-lectura lado mantenedor (E-1).
- Confianza: alta (metadatos+hashes Drive, TXT íntegro); resultado
  funcional = observación sin instrumentar.

## S-26 — E-1: estático comparativo F-03 (ejecutado 2026-10-09)

Método: descargas Drive→Arena verificadas por hash (md5+sha256 OK
las 3); análisis solo-lectura local (pefile 2024.8.26, binutils);
copias eliminadas tras el análisis. TXT md5 `2fff411e…3cd4d5`.

- **Huellas PE (los 3: x86 GUI, 5 secciones, sin versión, sin Rich,
  sin delay/bound, mismo CRT):** link ORIG 1998-05-18, F03-EU
  1998-06-29, F03-US 1998-06-24; entries distintos; stacks iguales.
- **F03-EU = BUILD DISTINTA, no parche:** 82.9% bytes difieren
  (55016 runs), 1/381 bloques 4KB idénticos, 5/5 secciones md5≠
  (.text +1024, .data +512). EUvsUS: 67.3% difieren, 0 bloques
  iguales → dos builds de época separadas, una por región.
- **Imports: SIN nueva API de timing.** Sets ORIG↔EU idénticos salvo
  +`mciGetErrorStringA` (WINMM, diagnóstico MCI, no timing). Timing
  en los 3 = `Sleep`+`GetTickCount` y nada más (sin QPC ni mm-timers).
  Glide 38/38/38 idénticos; US también Glide-only (sin D3D); US sin
  `GetDiskFreeSpaceA` (trivia). Total ORIG: 132 funcs.
- **Strings:** `_demo` solo EU/US (path defecto `…\\Gex23dfx_demo`;
  ORIG: `…\\Gex23dfx`); `%s\\GEX2…` añadido en EU/US; «DEMO MODE»×2
  en los 3; 0 strings FPS; kfusam ORIG==EU, US extendido (01–14).
- **Hipótesis líder (no probada):** exes F-03 de línea demo/build
  junio-1998; el «límite» es conductual-emergente, no parche
  quirúrgico. Localización a nivel código → Fase 2.
- **E-1.1 OK (2026-10-09):** sha256 paste = bytes Drive EU
  (match exacto triple). E-2 pendiente de autorización.
- Confianza: alta (bytes verificados, herramientas estándar).

## Áreas de investigación futura (separación estricta)

### Verificado por el proyecto (Fase 1, 2026-10-08)

Evidencia estática propia sobre los originales EU (ver
[docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md)):

- Edición europea v1.00.000 (`SETUP.INI` + `DATA.TAG` + PDB `release_europe`).
- EU Glide-exclusiva: 38 imports a `glide2x.dll`, 0 a DirectDraw/Direct3D.
- Claves `HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00` en strings del exe.
- Música en pistas CD: TOC 1 datos + 16 CD-DA, `mciSendCommandA` importado.
- Joystick vía WinMM (`joyGetPosEx`); sin imports a DirectInput en EU.
- Instalador InstallShield 5.x (stub NE 16-bit + CABs `ISc(` v4).
- Patrón de nombre de guardado `GEX2%d%d%c.GEX` en strings del exe EU
  (ubicación/directorio UNKNOWN; PCGW «save location» vacía).
- `glide2x.dll` cargada (7cbd…) = payload de nGlide 2.10 (drop-hash del
  instalador oficial verificado; imports solo-sistema ⇒ backend
  dinámico; S-22, 9ª sesión 2026-10-09).

### Reportado por fuentes secundarias (pendiente de verificación propia)

Nada de esto está confirmado por el proyecto; son hechos bien atestiguados
que habrá que verificar contra el juego real (Fase 1–2):

- Port PC por LTI Gray Matter, septiembre 1998 (Midway US / Ubisoft EU).
- D3D 5 nativo en versión US (en EU, ausente de fábrica — ver arriba).
- Caps de FPS reportados: 30 (base) / 24 (con parche D3D).
- Requisitos Win 95/98/ME, P166, 32 MB, DX5.
- Reparto vocal: Dana Gould (NA) / Leslie Phillips (EU) — atestiguado a
  nivel juego (S-20); set USA de PC sin verificar (base de M-25).

### Hipótesis abiertas (NO confirmadas)

- Lógica/timing acoplados al framerate (I-01).
- Intro en códec Indeo (I-14) — hipótesis DÉBIL tras Lote 1 (hilo VOGONS no
  resuelto, fuente única sin validar); escritura HKLM causa de I-17.
- Cámara PC similar a PS1 (L1/R1): **sin evidencia en PC**.
- Formatos de assets: `FONT.3DF` identificado (textura 3dfx `.3df`);
  `.DFX/.VFX/.SAG/.JAM/.TAD` pendientes (UNKNOWN-PENDING).

### Investigación futura por área → objetivo

| Área | Alimenta a |
|---|---|
| PC camera control, camera architecture | FA-07, M-12, M-17 |
| Controller input, gamepad APIs | FA-06, M-06…M-11 |
| Game loop, render loop, refresh rate | FA-04, FA-05, M-13, M-14 |
| Resolution handling, aspect ratio, viewport, FOV | FA-13, M-04, M-15…M-17 |
| Fullscreen/window creation, focus, Alt+Tab | FA-14, M-05, M-18 |
| DPI, multi-monitor | M-19 |
| Configuration storage, registry usage, launcher behavior | FA-11, M-01…M-03, M-20 |
| Texture formats, asset loading, texture replacement | M-23 |
| Audio configuration, CD audio behavior | FA-08, FA-09, M-20 |

## Pendiente del mantenedor

- [x] Enlaces principales recibidos (PCGamingWiki, REA, Gex64Decomp como
  secundaria, speedrun/setup). Jerarquía de referencias fijada.
- [x] Archivos originales del juego: recibidos y custodiados en Drive
  `originals/` (nunca en el repo); inventariados en Fase 1 (2026-10-08).
  El análisis binario profundo sigue bloqueado hasta que el mantenedor
  indique el cambio de fase.
- [x] S-14: metadatos + hashes + inventario nominal + README leído
  (2026-10-09, `S-14_extracted`, solo lectura). Diff binario → Fase 2.
