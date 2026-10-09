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
  `.reg` + admin ejecutados en Hito 1, sin F-01).

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

## S-03 — tgames.fr: hub de parches Gex 3D (Tgames) ⭐

- Hilos leídos íntegros en Lote 1 (2026-10-09), autor Tgames (admin, activo
  desde 2008); descargas solo para miembros (contenido no inspeccionado):
  - F-01 D3D (PAL): `…/patch-patch-direct-3d-gex-3d-enter-the-gecko-windows-7-8-10-t12116.html` (OP + 6 respuestas).
  - F-03 FPS Limiter: `…/patch-patch-3dfx-fps-limiter-gex-3d-enter-the-gecko-t12190.html` (OP + «Merci!»).
  - F-10 music handler: `…/patch-gex-3d-pc-support-des-musiques-sous-windows-7-8-10-t12122.html` (3 posts).
  - F-11 NO-CD D3D: `…/no-cd-no-cd-gex-3d-enter-the-gecko-version-direct-3d-t12117.html` (solo OP).
  - F-12 debug tool: `…/trainer-gex-3d-enter-the-gecko-direct-3d-cheats-debug-t12121.html` (solo OP).
  - Base URL nuevo foro: `https://www.tgames.fr/pc/progs-pc/`.
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
  concretas de `glide2x` (ficha registrada en TESTING 3ª sesión:
  Banshee 2.60.0.658 + SHA-256; originalidad/wrapper pendientes).
- Corroboración propia (3ª sesión): la `3dfxSpl2.dll` cargada declara
  `3dfx Splash Screen` 1.0.0.4 — coherente con esta identidad (splash,
  no renderer).

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

- Origen de la `glide2x.dll` cargada: original, redistribuida, modificada
  o wrapper con metadatos 3dfx (ficha en TESTING.md 3ª sesión; UNKNOWN).
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
