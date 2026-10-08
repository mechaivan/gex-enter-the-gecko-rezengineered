# KNOWN_ISSUES — Mapa inicial de problemas conocidos

> **Nivel de evidencia global: NO VERIFICADO.**
> Todos los problemas de este documento provienen de fuentes secundarias
> (PCGamingWiki, foros, reportes de usuarios) recensadas el 2026-10-08.
> Ninguno ha sido reproducido ni confirmado por este proyecto todavía.
> Ver [RESEARCH.md](RESEARCH.md) para las fuentes.

Formato por problema: identificador, descripción, fuente(s), estado.

## Rendimiento / timing

### I-01 — La velocidad del juego depende del framerate

- **Descripción:** en sistemas rápidos el juego corre demasiado deprisa; se
  considera necesario un límite de 30 FPS para que sea jugable. Incluso a
  60 FPS iría demasiado rápido (afirmación del autor de nGlide).
- **Fuentes:** foro Zeus Software (nGlide); parche "3DFX FPS Limiter".
- **Hipótesis (no confirmada):** lógica/timing ligada al número de frames.
- **Estado:** DESCONOCIDO (pendiente de reproducción y localización en el exe).

### I-02 — Límite de FPS distinto según versión/parche

- **Descripción:** PCGamingWiki indica cap de 30 FPS normal y 24 FPS con el
  parche D3D no oficial.
- **Fuente:** PCGamingWiki (tabla de vídeo).
- **Estado:** DESCONOCIDO.

## Render / vídeo

### I-03 — La versión europea no soporta Direct3D de fábrica

- **Descripción:** solo la versión US trae soporte Direct3D integrado; la
  versión EU sería exclusiva Glide/3Dfx y necesita parche no oficial o wrapper.
- **Fuente:** PCGamingWiki.
- **Estado:** DESCONOCIDO.

### I-04 — Pantalla negra + cuelgue/bloqueo al arrancar

- **Descripción:** varios usuarios reportan pantalla en negro y bloqueo al
  iniciar el juego en sistemas modernos.
- **Fuentes:** MyAbandonware (comentarios), Reddit r/gex.
- **Estado:** DESCONOCIDO (podrían ser varias causas distintas).

### I-05 — Errores `glide2x.dll` ausente

- **Descripción:** el juego exige Glide; sin tarjeta 3Dfx o wrapper, error de
  DLL faltante.
- **Fuente:** MyAbandonware (comentarios).
- **Estado:** DESCONOCIDO (esperable por diseño original; pendiente de
  confirmar dependencias del exe).

### I-06 — Resolución/tamaño de ventana anómalos

- **Descripción:** un usuario reporta ventana más pequeña que varía según la
  escena, en modo borderless.
- **Fuente:** MyAbandonware (comentarios).
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
- **Fuentes:** PCGamingWiki, Reddit r/gex, MyAbandonware.
- **Estado:** DESCONOCIDO.

### I-12 — La música se corta tras completar cada nivel

- **Descripción:** un usuario reporta que debe reiniciar el juego tras cada
  nivel para recuperar la música.
- **Fuente:** Reddit r/gex.
- **Estado:** DESCONOCIDO (un solo reporte).

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
- **Estado:** DESCONOCIDO.

## Instalación / arranque

### I-15 — El instalador original no funciona en Windows moderno

- **Descripción:** necesaria instalación manual (copiar carpeta `gex2` +
  claves de registro + ejecutar como administrador).
- **Fuente:** PCGamingWiki (procedimiento de instalación manual).
- **Claves implicadas:** `HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00`
  (`Version`, `InstallDir`, `CDDriveName`).
- **Estado:** DESCONOCIDO.

### I-16 — Comprobación de CD ("valid gex 2 disk")

- **Descripción:** el juego exige el CD; problemas de detección con ISOs
  montadas.
- **Fuente:** MyAbandonware (comentarios).
- **Estado:** DESCONOCIDO.

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
- **Estado:** DESCONOCIDO.

## Memoria / estabilidad

### I-20 — Posible necesidad del flag LAA (Large Address Aware)

- **Descripción:** nota genérica de PCGamingWiki para ejecutables 32-bit.
- **Fuente:** PCGamingWiki.
- **Estado:** DESCONOCIDO (nota genérica, no específica de Gex).

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
