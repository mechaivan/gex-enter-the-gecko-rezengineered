# KNOWN_ISSUES — Mapa inicial de problemas conocidos

> **Nivel de evidencia global: PRIMERAS OBSERVACIONES PROPIAS (Hito 1,
> 2026-10-09).**
> La mayoría de problemas provienen de fuentes secundarias (PCGamingWiki,
> foros, reportes de usuarios) recensadas el 2026-10-08. El Hito 1
> ([TESTING.md](TESTING.md)) aporta la primera ejecución propia: confirma
> parcialmente I-15/I-16, no reproduce I-11 en esa config y añade
> observaciones propias (I-21, I-22). Varios issues tienen evidencia
> estática propia (Fase 1) anotada en cada issue.
> Ver [RESEARCH.md](RESEARCH.md) para las fuentes y
> [docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md)
> para la evidencia propia.

Formato por problema: identificador, descripción, fuente(s), estado.

## Rendimiento / timing

### I-01 — La velocidad del juego depende del framerate

- **Descripción:** en sistemas rápidos el juego corre demasiado deprisa; se
  considera necesario un límite de 30 FPS para que sea jugable. Incluso a
  60 FPS iría demasiado rápido (afirmación del autor de nGlide).
- **Fuentes:** foro Zeus Software (nGlide); parche "3DFX FPS Limiter".
- **Lote 1 (2026-10-09):** F-03 verificado (2×GEX3D.EXE EU/US, solo sistemas
  3DFX + Win 98–XP); F-04 sin pieza separada (probable duplicado de F-02);
  caps PCGW 30/24 re-verificados.
- **Hipótesis (no confirmada):** lógica/timing ligada al número de frames.
- **5ª sesión (2026-10-09):** ≈25 FPS estables (método: Steam, 6ª); causa sin atribuir.
- **6ª sesión (2026-10-09):** A/B VSync On/Off ⇒ ≈25 en ambos (nulo): VSync solo DESCARTADO; líder: límite propio del juego.
- **Lote 2:** F-03 = limitador anti-too-fast de época (FPS objetivo sin
  declarar), NO desbloqueador; nulo como fix de los 25 estables;
  referencia RE para localizar timing/límite.
- **P-F03 (prep. 2026-10-09):** protocolo A/B (exe EU F-03 vs original)
  preparado en TESTING.md, pendiente de autorización (A1–A4);
  caracterización off-label, no intento de fix.
- **Estado:** DESCONOCIDO (pendiente de reproducción y localización en el exe).

### I-02 — Límite de FPS distinto según versión/parche

- **Descripción:** PCGamingWiki indica cap de 30 FPS normal y 24 FPS con el
  parche D3D no oficial.
- **Fuente:** PCGamingWiki (tabla de vídeo).
- **Lote 1 (2026-10-09):** re-verificado en la página (sin cambios).
- **5ª–6ª sesión (2026-10-09):** ≈25 FPS estables (método: Steam); nuevo dato para la tabla de caps; persiste con VSync off.
- **Lote 2:** caps PCGW 30/24 re-verificados verbatim; 25 no coincide con
  ningún cap documentado; PAL⇒25 no establecido (línea E).
- **Estado:** DESCONOCIDO.

## Render / vídeo

### I-03 — La versión europea no soporta Direct3D de fábrica

- **Descripción:** solo la versión US trae soporte Direct3D integrado; la
  versión EU sería exclusiva Glide/3Dfx y necesita parche no oficial o wrapper.
- **Fuente:** PCGamingWiki.
- **Evidencia Fase 1 (CONFIRMED):** el exe EU v1.00.000 no importa ninguna API
  D3D (38 imports `glide2x`). La afirmación sobre EU queda confirmada a nivel
  binario; la parte US sigue pendiente de binario.
- **Estado:** DESCONOCIDO (issue no reproducido; premisa EU confirmada).

### I-04 — Pantalla negra + cuelgue/bloqueo al arrancar

- **Descripción:** varios usuarios reportan pantalla en negro y bloqueo al
  iniciar el juego en sistemas modernos.
- **Fuentes:** MyAbandonware (comentarios), Reddit r/gex.
- **Estado:** DESCONOCIDO (podrían ser varias causas distintas).

### I-05 — Errores `glide2x.dll` ausente

- **Descripción:** el juego exige Glide; sin tarjeta 3Dfx o wrapper, error de
  DLL faltante.
- **Fuente:** MyAbandonware (comentarios).
- **Estado:** DESCONOCIDO (esperable por diseño original; dependencias
  confirmadas en Fase 1: 38 funciones `glide2x` importadas).

### I-06 — Resolución/tamaño de ventana anómalos

- **Descripción:** un usuario reporta ventana más pequeña que varía según la
  escena, en modo borderless.
- **Fuente:** MyAbandonware (comentarios).
- **Hito 1 (2026-10-09):** caso distinto (pantalla completa propia con
  bandas/HUD, ver I-21); este reporte borderless sigue sin reproducir.
- **Estado:** DESCONOCIDO (podría ser artefacto de wrapper/config).

### I-07 — Petición de refresco a 75 Hz

- **Descripción:** el juego solicitaría modo 75 Hz; el hardware Voodoo2 real
  usa 512x384@60 Hz.
- **Fuente:** foro Zeus Software (nGlide).
- **Estado:** DESCONOCIDO.

### I-08 — "Pure virtual function call" (versión parcheada Glide2)

- **Descripción:** cuelgue con ese error usando la versión parcheada con el
  renderer Glide2 bajo nGlide; en Voodoo2 real funcionaría.
- **Fuente:** foro Zeus Software (nGlide).
- **Estado:** DESCONOCIDO.

### I-09 — Regresiones de dibujado entre versiones de nGlide

- **Descripción:** "wrong drawing" en nGlide 1.00/1.01 según reportes del foro.
- **Fuente:** foro Zeus Software.
- **Estado:** DESCONOCIDO (podría ser bug del wrapper, no del juego).

### I-10 — Incompatibilidad dgVoodoo2 (Glide) + vorpX

- **Descripción:** PCGamingWiki reporta crash combinando dgVoodoo2 y vorpX.
- **Fuente:** PCGamingWiki (tabla VR).
- **Estado:** DESCONOCIDO.

## Audio

### I-11 — Sin música / CD-audio no suena

- **Descripción:** la música Red Book CD no suena correctamente en Windows
  moderno; varios usuarios reportan ausencia total de audio o de música.
- **Fuentes:** PCGamingWiki, Reddit r/gex, MyAbandonware, foro Zeus (t=743),
  Abandonware France, Tgames (F-10).
- **Evidencia Fase 1:** base confirmada (16 pistas CD-DA en la TOC +
  `mciSendCommandA` importado). El fallo en Windows moderno sigue sin
  reproducir.
- **Lote 1 (2026-10-09):** Zeus (autor nGlide, 2015): haría falta un wrapper
  winmm; algunos bugs audio existen también en Voodoo real. Un reporte 2024:
  `_inmm.dll` daría música + loop. AF: el disco debe estar en el PRIMER
  lector óptico. F-10 es el handler comunitario (D3D/WAV). README S-14
  corrobora `_inmm` (método DirectShow + `_inmm.ini` con los WAV) como
  solución adoptada por speedrunners (DECLARADO POR LA FUENTE, no
  verificado funcionalmente).
- **Hito 1 (2026-10-09):** NO reproducido en esa config: música y SFX
  funcionan (imagen montada como `D:`). Pendiente: cambios de nivel,
  loops, volumen, otros hardwares.
- **Estado:** DESCONOCIDO (no reproducido en Hito 1; reportes externos
  intactos).

### I-12 — La música se corta tras completar cada nivel

- **Descripción:** un usuario reporta que debe reiniciar el juego tras cada
  nivel para recuperar la música.
- **Fuente:** Reddit r/gex.
- **Reporte adyacente (Lote 1):** Abandonware France documenta que la música
  suena una sola vez por nivel y no se repite salvo abrir el menú de pausa
  (matiz distinto: sin reinicio). Zeus t=743: la pausa reinicia la música
  pero no restaura el SFX «1UP».
- **Estado:** DESCONOCIDO (reportes adyacentes, sin reproducir).

### I-13 — Controles de volumen del juego sin efecto

- **Descripción:** cambiar el volumen in-game no haría nada (un reporte).
- **Fuente:** MyAbandonware (comentarios).
- **Estado:** DESCONOCIDO (un solo reporte).

## Vídeo FMV / intro

### I-14 — La intro no se reproduce

- **Descripción:** la cinemática inicial no se muestra en sistemas modernos.
- **Fuentes:** MyAbandonware, VOGONS.
- **Hipótesis (no confirmada):** falta el códec Indeo (ir32_32.dll) o su
  registro en `drivers.desc`.
- **Lote 1 (2026-10-09):** hilo VOGONS leído íntegro → NO resuelto; la
  sugerencia Indeo queda como evidencia débil de fuente única (contexto
  Wine/OSX, sin validar). Reporte adyacente distinto (Zeus t=743, Win7):
  la intro SÍ se reproduce pero el audio se desincroniza (logo Midway y
  escena de Gex) + música ausente.
- **6ª sesión (2026-10-09, equipo real):** las intros SÍ se reproducen
  aquí (I-14 NO reproducida en esta config): secuencias Ubisoft/Crystal
  Dynamics/logo Gex percibidas a ~15 FPS ESTIMADOS (NO medido).
  Formato/frecuencia sin confirmar (repo: `MOVIE/` §7, magias §14.3).
- **Estado:** DESCONOCIDO.

### I-15 — El instalador original no funciona en Windows moderno

- **Descripción:** necesaria instalación manual (copiar carpeta `gex2` +
  claves de registro + ejecutar como administrador).
- **Fuente:** PCGamingWiki (procedimiento de instalación manual).
- **Claves implicadas:** `HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00`
  (`Version`, `InstallDir`, `CDDriveName`).
- **Evidencia Fase 1:** instalador identificado (InstallShield 5.x, stub NE
  16-bit) — coherente con la rotura en Windows moderno (HYPOTHESIS, sin
  probar). Claves `...\Gex2\1.00` confirmadas en strings del exe.
- **Hito 1 (2026-10-09):** instalación manual CONFIRMADA funcionando en
  Win11 64-bit con `Version`=2, `InstallDir`=`C:\GEX_REZ\GEX2`,
  `CDDriveName`=`D` (sin paso F-01). Instalador original en moderno:
  sigue sin probar (HYPOTHESIS intacta).
- **Estado:** PARCIALMENTE CONFIRMADO (manual funciona 1 vez; instalador
  roto sin probar; causa admin pendiente).

### I-16 — Comprobación de CD ("valid gex 2 disk")

- **Descripción:** el juego exige el CD; problemas de detección con ISOs
  montadas.
- **Fuente:** MyAbandonware (comentarios).
- **Lote 1 (2026-10-09):** F-01 incluye No-CD (PAL); F-11 es el No-CD
  standalone (versión D3D, posible US). Mecanismo exacto UNKNOWN (indicio
  «NOPs» del autor, sin verificar). Sin CD no hay música (F-11) → cadena
  con F-10.
- **Evidencia Fase 1:** mensajes de CD-check presentes en strings del exe
  («A valid Gex: Enter The Gecko CD was not found.»).
- **Hito 1 (2026-10-09):** detección CONFIRMADA funcionando 1 vez con
  imagen CloneCD montada como `D:` (arranque sin error de CD).
  Mecanismo del check: sigue UNKNOWN.
- **Estado:** PARCIALMENTE CONFIRMADO (detección con imagen funciona 1
  vez; mecanismo UNKNOWN; otras letras/unidades sin probar).

### I-17 — Requiere ejecución como administrador

- **Descripción:** PCGamingWiki indica ejecutar `gex3d.exe` como admin.
- **Hipótesis (no confirmada):** escritura en HKLM o acceso a paths protegidos.
- **Fuente:** PCGamingWiki.
- **Estado:** DESCONOCIDO.

## Input

### I-18 — Sin soporte de ratón (al menos en menús)

- **Descripción:** PCGamingWiki indica "No mouse support".
- **Fuente:** PCGamingWiki (tabla de input).
- **Estado:** DESCONOCIDO (podría ser comportamiento original, no un bug).

### I-19 — Estado del soporte de mando desconocido

- **Descripción:** no hay información clara sobre DirectInput/XInput.
- **Evidencia parcial Fase 1:** joystick vía WinMM (`joyGetPosEx`), sin
  imports a DirectInput en el binario EU. Comportamiento real pendiente.
- **Lote 2 (F-10):** soporte Xbox 360 comunitario = EXPERIMENTAL, solo
  movimiento con stick izquierdo, inestable, cadena D3D solamente;
  cerrado/sin fuente (no reutilizable). Original WinMM sin caracterizar.
- **Estado:** DESCONOCIDO.

## Memoria / estabilidad

### I-20 — Posible necesidad del flag LAA (Large Address Aware)

- **Descripción:** nota genérica de PCGamingWiki para ejecutables 32-bit.
- **Fuente:** PCGamingWiki.
- **Estado:** DESCONOCIDO (nota genérica, no específica de Gex).

## Primeras observaciones propias (Hito 1, 2026-10-09)

### I-21 — Bandas negras descentradas + HUD descolocado (observación propia)

- **Descripción:** en la única prueba propia (Win11, pantalla completa):
  resolución baja 4:3 con bandas negras arriba y a la izquierda (centrado
  sin confirmar) y elementos del HUD ligeramente descolocados.
- **Fuente:** Hito 1 ([TESTING.md](TESTING.md)), observación directa.
- **Hipótesis (no confirmadas):** modo de vídeo de época escalado por
  wrapper/GPU/monitor; viewport del juego no centrado en el modo
  elegido. Renderer: `glide2x` cargada, ficha + fecha/firma (4ª sesión);
  wrapper = nGlide 2.10 (atribuido 9ª); causa definitiva sin atribuir.
- **2ª sesión (2026-10-09):** repetido (4:3, bandas arriba/izquierda, HUD
  desalineado) con `glide2x.dll` cargada. Causa sigue UNKNOWN.
- **5ª sesión (2026-10-09):** `Aspect correction` de nGlide no mejora bandas/HUD (nulo, 1 prueba, config con nGlide).
- **Estado:** OBSERVADO 3 veces (causa UNKNOWN; modo/resolución sin medir).

### I-22 — Pantalla completa desplaza iconos al segundo monitor (propia)

- **Descripción:** durante el juego, los iconos del escritorio del monitor
  principal se desplazaron temporalmente al segundo monitor; al cerrar el
  juego, el comportamiento se revirtió.
- **Fuente:** Hito 1 ([TESTING.md](TESTING.md)), observación directa.
- **Hipótesis (no confirmada):** cambio a modo exclusivo de baja
  resolución en multimonitor (clásico de fullscreen exclusivo).
- **2ª sesión (2026-10-09):** el 2º monitor parpadea tras el splash 3dfx,
  al empezar las cinemáticas (Ubisoft + juego) y al pasar al menú
  principal (patrón de re-modos en cada transición de vídeo). Causa sigue
  UNKNOWN; layout/resoluciones/refrescos sin registrar.
- **5ª sesión (2026-10-09):** un único parpadeo al iniciar (compatible con un cambio de modo); config distinta (nGlide) ⇒ comparabilidad limitada.
- **6ª sesión (2026-10-09):** UN único cambio de modo/resolución al
  arrancar, no parpadeo continuo (refina 5ª; modo sin medir).
- **Estado:** OBSERVADO 4 veces (causa UNKNOWN).

## Relación con objetivos (sin duplicar)

Las features futuras viven en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md),
no aquí. Mapeo orientativo issue → objetivo de investigación:

| Issue | Objetivo relacionado |
|---|---|
| I-01, I-02 (timing/FPS) | FA-04, FA-05 → M-13, M-14 |
| I-03 (D3D en EU) | FA-03 → M-04 |
| I-04…I-10 (render/arranque) | FA-03, FA-13, FA-14 |
| I-11…I-13 (audio) | FA-08, FA-09 |
| I-14 (intro) | FA-10 |
| I-15…I-17 (instalación/CD/admin) | FA-11, FA-12 → M-01…M-03 |
| I-18, I-19 (input) | FA-06 → M-06…M-11 |
| I-20 (LAA) | FA-01 |
| I-21 (viewport/HUD, propio) | FA-13 → M-04 |
| I-22 (multimonitor, propio) | FA-14 → M-05, M-19 |
