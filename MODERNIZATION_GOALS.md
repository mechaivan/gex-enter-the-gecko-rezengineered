# MODERNIZATION_GOALS — Objetivos futuros de modernización

> **Documento de referencia principal** de los objetivos de modernización.
> Todo lo aquí listado está en estado **PROPOSED / TO INVESTIGATE** salvo que
> se indique lo contrario. **Nada está implementado. Nada se asume posible
> hasta investigarlo.** La fuente de verdad sigue siendo el Gex PC original.

## Filosofía

```text
DOCUMENT / COLLECT → ANALYZE → UNDERSTAND → PLAN → IMPLEMENT → TEST → DOCUMENT
```

Y dentro de la futura implementación:

```text
BASE / FUNDAMENTOS → MEJORAS SENCILLAS → MODERNIZACIÓN → SISTEMAS COMPLEJOS → EXPERIMENTAL / OPCIONAL
```

No se empieza por 16:9, ultrawide ni texture packs. Primero base sólida y
comprensión completa del funcionamiento original de PC.

## Leyenda

**Estados:** `PROPOSED` · `TO INVESTIGATE` · `RESEARCHED` · `PLANNED` ·
`IMPLEMENTED` · `TESTING` · `VERIFIED` · `DEFERRED` · `REJECTED`

**Prioridad:** `P0` fundacional (Fase A, no es feature) · `P1` alta ·
`P2` media · `P3` baja / experimental.

**Complejidad:** estimación preliminar `XS / S / M / L / XL`, **a revisar
tras la investigación**. Ninguna estimación es un compromiso.

**Original Mode:** ¿puede existir este objetivo sin romper la preservación?
**Modern Mode:** ¿pertenece este objetivo a Modern Mode?

---

## Ruta corta al prototipo (2026-10-10; IDs intactos)

- **RUTA:** FA-01, FA-04, FA-05, FA-11 (+ FA-02/FA-03 confirmación) →
  Hito K → Hito P. M-13 en Ola 1 (sus FA salen del camino crítico).
- **POSPUESTOS:** FA-06…FA-10, FA-12(profundo)…FA-15; M-04…M-12,
  M-14…M-20, M-22…M-26 (Olas 2–3, por dependencias reales).
- **ENABLERS Ola 1:** M-01/M-02/M-03, M-21 (referencia viva).
- Regla: ningún M-xx avanza sin sus FA; ningún FA se reabre sin
  implementación que lo pida. Detalle: ROADMAP (R→K→P).

# FASE A — FUNDAMENTOS Y COMPATIBILIDAD (P0, no son features)

Base del proyecto: **preservar y entender el Gex original de PC**. Todo M-xx
depende, directa o indirectamente, de esta fase. Detalle del cómo en
[REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md); problemas en
[KNOWN_ISSUES.md](KNOWN_ISSUES.md).

| ID | Área fundacional | Se investiga en | Estado |
|---|---|---|---|
| FA-01 | Ejecutable original (variantes, hashes, versiones) | RE Etapa A | TO INVESTIGATE |
| FA-02 | Dependencias (DLLs, Glide, D3D5, WinMM, Indeo…) | RE Etapa A–B | TO INVESTIGATE |
| FA-03 | Renderer original (rutas Glide/D3D, selección) | RE Etapa C | TO INVESTIGATE |
| FA-04 | Timing / game loop | RE Etapa C | TO INVESTIGATE |
| FA-05 | FPS y lógica de juego (acoplamiento) | RE Etapa C (I-01, I-02) | TO INVESTIGATE |
| FA-06 | Input original (teclado, mando, ausencia de ratón) | RE Etapa C (I-18, I-19) | TO INVESTIGATE |
| FA-07 | Cámara original (comportamiento PC, no asumir PS1) | RE Etapa C | TO INVESTIGATE |
| FA-08 | Audio (efectos, voces, volúmenes) | RE Etapa C (I-13) | TO INVESTIGATE |
| FA-09 | CD Audio (Red Book, cambios de nivel) | RE Etapa C (I-11, I-12) | TO INVESTIGATE |
| FA-10 | FMV / Indeo (intro) | RE Etapa C (I-14) | TO INVESTIGATE |
| FA-11 | Registry (claves, valores, imprescindibles vs config) | RE Etapa C (I-15) | TO INVESTIGATE |
| FA-12 | CD check (detección de disco) | RE Etapa C (I-16) | TO INVESTIGATE |
| FA-13 | Resolución original (modos, viewport, HUD) | RE Etapa C | TO INVESTIGATE |
| FA-14 | Fullscreen / ventana (creación, foco, Alt+Tab) | RE Etapa C | TO INVESTIGATE |
| FA-15 | Comportamiento de referencia en hardware/software de época | TESTING + COMPATIBILITY | TO INVESTIGATE |

**Avance de Fase 1 (2026-10-08, parcial — estados sin cambiar):** evidencia
estática propia en [docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md)
para FA-01 (exe EU inventariado), FA-02 (dependencias: Glide/WinMM/DSound;
sin D3D/DirectInput), FA-03 (ruta Glide única en EU), FA-09 (TOC 1+16,
MCI), FA-11 (claves `Gex2\1.00` en strings) y FA-12 (mensajes de
CD-check). Las preguntas profundas de cada FA siguen TO INVESTIGATE.
Hito 1 (2026-10-09, ver TESTING.md): primera evidencia runtime para FA-11
(valores `.reg` funcionan 1 vez), FA-12 (imagen `D:` aceptada), FA-09
(música/SFX audibles) y FA-14 (observación multimonitor); FA-02/FA-03:
`glide2x` cargada + ficha + fecha/firma (4ª sesión), origen/wrapper UNKNOWN.
Estados FA sin cambiar.

### Cobertura documental FA-01…FA-15 (2026-10-09, Fase 1)

> Matriz documental: qué respalda cada área con la evidencia disponible hoy.
> No sustituye al análisis dinámico/RE: la columna Estado de la tabla FA
> sigue TO INVESTIGATE en las 15 áreas hasta cerrar sus preguntas profundas.
> Estados documentales: BASE DOCUMENTADA (base estática sólida en EU;
> dinámica pendiente) · PARCIAL (evidencia incompleta) · PENDIENTE (sin
> evidencia aprovechable). Reglas: estático ≠ comportamiento confirmado; lo
> secundario/D3D/US/parches no se extrapola a EU-Glide.

| ID y tema | Conocido | Evidencia y fuente | Tipo | Relevancia EU-Glide | Falta | Siguiente acción | Estado |
|---|---|---|---|---|---|---|---|
| FA-01 — Ejecutable, variantes, hashes, versiones | EU `GEX3D.EXE` v1.00.000: 1557504 B, MD5/SHA-256, PE32 i386 GUI, link 1998-05-18, rama `3dfx\release_europe`; `Gex2-Europe` + `1.00.000`; instalador despliega exacto `GEX2/` | Inventario §4.1, §3.2, §10, §11–§13 (30/30 SHA) | Original estática | Directa (artefacto de referencia) | Variantes US/demo/parcheadas; parseo formal CAB; `data3.ca0`; etiqueta volumen | Registrar hashes de variantes al recibirlas (sin analizar); §14 → Fase 2 | BASE DOCUMENTADA |
| FA-02 — Dependencias y bibliotecas | 38 imports `glide2x`; WINMM (`joyGetPosEx`, `aux*`, `mciSendCommandA`); DSOUND; K32/U32/GDI32/ADVAPI32; 0 D3D/DDraw/DInput; «requires Glide 2.1+»; CD trae DX 4.05.01.1600 + DRIVERS Win95 (contexto, no dependencia probada) | Inventario §4.1, §9 (`DXVER.INF`), §11 | Original estática | Directa | Uso real en dinámico; Indeo (sin evidencia estática en ningún sentido) | Confirmación dinámica en Windows (Olas); Indeo vía FA-10 | BASE DOCUMENTADA |
| FA-03 — Renderer y rutas Glide/D3D | EU Glide exclusiva en estático; llamadas cubren init/shutdown, `grSstWinOpen`, triángulos, `grBufferSwap`, texturas, fog, alpha, LFB; D3D5 nativo US declarado (PCGW, sin verificar); F-01 añade D3D a PAL (declarado) | Inventario §4.1; S-01; F-01 | Original estática (EU) + pública/declarada (US/F-01) | Directa (vía EU); US/D3D no extrapolable | Comportamiento en ejecución (modos, 75 Hz, resolución); selección US; mecanismo F-01 | Etapa C render; diffs Etapa D | BASE DOCUMENTADA |
| FA-04 — Timing y game loop | Flujo temporal gameplay estático (E-3/M-13 + Línea B 2026-10-10): init t0 `0x43B36D`, sello `elapsed/16.666`→`+0x10C` + contador `+0x104` por frame (`0x43E5DD`), sim+render en serie con `grBufferSwap(3)`; lectores `+0x10C` LOCALIZADOS (R1 snapshot `ds:0x5ACBD4`, R2 delta→tabla `0x5ACBE5`→WORD, flags `ds:0x5ACBC4`∈{0,3}); 75 Hz/512x384 pedidos confirmados (`0x46324D`); acoplamiento serie evidenciado, cuantitativo pendiente | REVERSE_ENGINEERING Etapa C + TESTING E-3 (disasm acotado, nada ejecutado) | Original estática (parcial) | Crítica (preservar comportamiento; base M-13/M-14) | Prueba dinámica acoplamiento (E-2 listo E-2.3, ejecución pendiente); semántica fina tabla `0x5ACBE4` (hipótesis fuerte: filas por voz) + obj 3er arg R2; mapeo subsistemas; tasa real runtime | E-2 dinámico (E-2.3) cuando ejecute (+ cobertura tabla pantallas, disparo R1/R2) | PARCIAL |
| FA-05 — FPS y acoplamiento lógica | «too fast even at 60fps, needs 30fps cap» (Zeus); caps PCGW 30 base / 24 parche D3D; F-03 declarado (3DFX+Win98–XP, mecanismo y objetivo sin declarar); F-04 sin resolver; FAQ nGlide: V-Sync si «too fast»; petición 75 Hz | S-02, S-01, F-03, F-04, T-03 | Pública + declarada | Directa (velocidad EU), sin observación EU | FPS reales EU (época/moderno); mecanismo acoplamiento; mecanismo F-03; F-04 | Medir en Windows (referencia + moderno); diffs | PARCIAL |
| FA-06 — Entrada: teclado, mando, ratón | `joyGetPosEx` (WinMM) en estático; 0 imports DirectInput; PCGW «No mouse support»; AF recomienda Xbox 360 (contexto moderno, no evidencia original); F-10 stick experimental inestable (D3D) | Inventario §4.1; S-01; F-07; F-10 | Original estática (parcial) + pública | Directa | Teclado, confirmación ausencia ratón, comportamiento/mapeo joystick | Tests dinámicos en Windows; Etapa C input | PARCIAL |
| FA-07 — Cámara original de PC | Nada en PC; L1/R1 estilo PS1 sin evidencia PC | RESEARCH (hipótesis sin evidencia) | Desconocido | Directa (base M-12/M-17) | Todo comportamiento de cámara PC | Observar en Windows; Etapa C | PENDIENTE |
| FA-08 — SFX, voces, volúmenes | 37 `.TAD` (25 contenidos, 7 grupos idénticos; bancos por nivel por nombres — inferencia); `VOICEUK/` 400 `.SAG` UK (prefijos por nivel/tipo — inferencia); DSOUND + volúmenes `aux*`; manifiesto instala todo (§10); ruta voz CONFIRMADA en código 2026-10-10 (`0x53FF0D` + fmt `0x55A708`: `<InstallDir>\audio\voiceuk\<n>.sag`; housekeeping voz `0x53FCB7` decodificada) | Inventario §4.1, §6, §8, §10; I-13; S-20; M-25 | Original estática (activos) + pública + inferencia (nombres) | Directa | Formatos `.TAD`/`.SAG` (magias); volúmenes (I-13); mapeo/selección voces; set USA | Magias §14.3 (Fase 2); tests dinámicos audio | PARCIAL |
| FA-09 — CD Audio Red Book y niveles | TOC 1 datos + 16 CD-DA (pistas 2–17; pista 17 ~4 s sin pregap — peculiaridad documentada, sin concluir); audio ≈40:36; `mciSendCommandA` → MCI; I-11/I-12 + adyacentes AF/Zeus; F-10 y `_inmm` S-14 (declarados, contexto D3D/NTSC, no evidencia EU) | Inventario §2.2, §4.1; I-11, I-12; F-10; S-14 | Original estática (base) + pública | Directa | Mapeo pista↔nivel; cambios de nivel; loops/volumen; repro moderna; correspondencia 15-vs-16 | Tests CD-audio en Windows; duraciones vs WAV S-14 (Fase 2+) | BASE DOCUMENTADA |
| FA-10 — FMV e Indeo | `MOVIE/` 9 pares `.SAG`/`.JAM` (LOGO, UBISOFT, INTRO, OUTRO, SCANS, CLAK1–3, CRYLOGO); hipótesis `.JAM`=vídeo/`.SAG`=audio por tamaños (sin confirmar); magias sin muestrear; F-06 débil (VOGONS no resuelto, Wine/OSX); adyacente Zeus: intro sí reproduce en Win7 con desync | Inventario §7, §11; F-06; S-02 t=743 | Original estática (inventario) + pública (débil) | Directa | Reproductor/códec real; magias; intro en EU | Muestras §14.3; Etapa C FMV; test intro | PARCIAL |
| FA-11 — Registro y valores | Claves `…\Gex2\1.00` + ruta defecto `…\Gex23dfx` en strings; destino defecto `\gex23dfx` en `SETUP.INS`; patrón guardado `GEX2%d%d%c.GEX` (ubicación UNKNOWN); `.reg` F-05 declarado (`Version`=2, `InstallDir`, `CDDriveName`); F-10 usa `InstallDir`; I-17 (hipótesis HKLM); uso real CONFIRMADO estático 2026-10-10 (`0x442F41`: `InstallDir`+`CDDriveName` desde `HKLM\…\Gex2\1.00`; `font.3df` desde InstallDir = causa I-23) | Inventario §4.1, §3.1; F-05; F-10; I-15/I-17 | Original estática (claves) + declarada (valores) | Directa | Valores/uso reales; imprescindible vs config; ubicación guardado; causa admin | Observación dinámica registro en Windows; reproducir F-05 | PARCIAL |
| FA-12 — Comprobación del CD | Mensajes CD-check en strings («A valid Gex… CD was not found.»); I-16; No-CD declarados F-01 (PAL), F-11 (posible US, NOPs sin verificar), exe 2013 S-14 (NTSC); AF exige primer lector (adyacente) | Inventario §4.1; I-16; F-01; F-11; F-09 | Original estática (mensajes) + declarada | Directa | Lógica/ubicación del check; método identificación disco; dinámica | Etapa C CD-check; diffs Etapa D | PARCIAL |
| FA-13 — Resolución, viewport, HUD | Secundaria: Voodoo2 real 512x384@60, petición 75 Hz (Zeus); adyacentes: `FONT.3DF` (fuente 3dfx) y `GEX.VFX` (rampa paleta/gamma, formato UNKNOWN); I-06 anomalía borderless (contexto wrapper) | S-02; inventario §4.2; I-06 | Pública (débil) + estática adyacente | Directa (modos originales; base M-04) | Lista de modos; viewport/HUD; rutas de resolución en exe | Enumeración dinámica en Windows; Etapa C render | PARCIAL (mínima) |
| FA-14 — Fullscreen, ventana, foco, Alt+Tab | `grSstWinOpen` (creación ventana vía Glide) + subsistema GUI en estático; comportamiento desconocido | Inventario §4.1 | Original estática (mínima) | Directa | Exclusividad fullscreen; ventana posible; foco/Alt+Tab | Tests dinámicos en Windows; Etapa C | PARCIAL (mínima) |
| FA-15 — Referencia en HW/SW de época | Sin pruebas propias; indicios secundarios dispersos: Voodoo2 512x384@60, bugs audio también en Voodoo real (Zeus), Vista+nGlide0.99 (AF, wrapper, no HW original); comportamiento «correcto» per sistema sin definir | S-02, S-08; COMPATIBILITY (notas) | Pública (indicios) + desconocido (pruebas) | Total (define «correcto») | Definir referencias per sistema; resultados HW/SW época | Máquina Windows + protocolo captura (Fase 1); matriz HW (Ola 3) | PENDIENTE |

**Lagunas cerrables solo con documentación:** descripciones de hilos tgames
citados no leídos (`gex3d_windows10.zip`, `GEX3DFX_PatchD3D_V1.2.zip`;
requieren membresía; sin descargar binarios); contenido de F-08 si
reaparece. **Resto → Fase 2+:** toda dinámica, diffs binarios, magias de
formato, `data3.ca0`/`unshield` y tests en Windows.

---

# NIVEL 1 — MEJORAS MODERNAS DE BAJO RIESGO

## M-01 — Modern Configuration System

- **Categoría:** Nivel 1 — Bajo riesgo · Configuración
- **Descripción:** Configurador moderno para vídeo, audio, controles,
  renderer, resolución, fullscreen/windowed/borderless y compatibilidad.
- **Prioridad:** P1 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** FA-01, FA-03, FA-06, FA-08, FA-11, FA-13, FA-14
- **Investigar antes:** qué opciones existen originalmente; cuáles dependen
  del launcher/configuración externa; cómo y dónde se almacenan.
- **Riesgos / incógnitas:** launcher externo desconocido; formato de
  almacenamiento desconocido (TO INVESTIGATE).
- **Relación con comportamiento original:** no debe alterar gameplay; solo
  exponer configuración de forma moderna.
- **Compatible con Original Mode:** Sí · **Pertenece a Modern Mode:** Sí
- **Notas:** base para M-02, M-03 y M-20.

## M-02 — Portable Configuration

- **Categoría:** Nivel 1 — Bajo riesgo · Configuración
- **Descripción:** Si es técnicamente seguro: configuración junto al
  ejecutable, ficheros portables, instalación movible sin perder ajustes.
- **Prioridad:** P1 · **Complejidad estimada:** S · **Estado:** PROPOSED
- **Dependencias:** M-01, FA-11
- **Investigar antes:** qué lee/escribe el exe fuera de su carpeta
  (registro, paths absolutos, CDDriveName).
- **Riesgos / incógnitas:** paths absolutos cableados; dependencias de
  registro no documentadas.
- **Relación con comportamiento original:** neutro si solo cambia el
  almacenamiento de ajustes.
- **Compatible con Original Mode:** Sí · **Pertenece a Modern Mode:** Sí
- **Notas:** no asumir todavía el formato final.

## M-03 — Reduce / Remove Registry Dependency

- **Categoría:** Nivel 1 — Bajo riesgo · Configuración
- **Descripción:** Investigar claves de registro de Gex; determinar qué
  guarda cada una, cuáles son imprescindibles, cuáles son solo configuración
  y cuáles pueden sustituirse por ficheros locales. Eliminar dependencias
  solo cuando esté demostrado que no rompe compatibilidad.
- **Prioridad:** P1 · **Complejidad estimada:** S · **Estado:** PROPOSED
- **Dependencias:** FA-11, FA-01
- **Investigar antes:** inventario completo de claves/valores y su uso real.
- **Riesgos / incógnitas:** claves leídas en arranque con paths esperados;
  permisos HKLM (posible causa de I-17).
- **Relación con comportamiento original:** debe ser transparente para el juego.
- **Compatible con Original Mode:** Sí · **Pertenece a Modern Mode:** Sí
- **Notas:** prerrequisito natural de M-02.

## M-04 — Modern Resolution Selection

- **Categoría:** Nivel 1 — Bajo riesgo · Display
- **Descripción:** Permitir seleccionar resoluciones modernas (720p, 1080p,
  1440p, 4K) si el renderer lo admite.
- **Prioridad:** P1 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** FA-03, FA-13, FA-14
- **Investigar antes:** manejo interno de resolución, viewport, HUD y
  render targets; límites del renderer original.
- **Riesgos / incógnitas:** HUD/menús acoplados a resolución fija;
  render targets con tamaño asumido.
- **Relación con comportamiento original:** Original Mode conserva
  resoluciones de época; la selección moderna vive en Modern Mode.
- **Compatible con Original Mode:** Parcial (fija época) · **Pertenece a Modern Mode:** Sí
- **Notas:** prerrequisito de M-15/M-16/M-17.

## M-05 — Borderless Windowed

- **Categoría:** Nivel 1 — Bajo riesgo · Display
- **Descripción:** Modo ventana sin bordes moderno.
- **Prioridad:** P1 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** FA-14, FA-03
- **Investigar antes:** fullscreen original, creación de ventana, cambio de
  resolución, foco, Alt+Tab.
- **Riesgos / incógnitas:** exclusive fullscreen asumido; pérdida de
  dispositivo D3D5/Glide al cambiar de contexto.
- **Relación con comportamiento original:** solo presentación; sin cambios
  de gameplay.
- **Compatible con Original Mode:** Parcial (presentación) · **Pertenece a Modern Mode:** Sí
- **Notas:** relacionado con M-18 (Alt+Tab robusto).

---

# NIVEL 2 — INPUT Y CONTROL MODERNO

## M-06 — Modern Input System

- **Categoría:** Nivel 2 — Input · Sistema
- **Descripción:** Sistema de entrada que separe claramente movimiento,
  cámara, acciones, menú y navegación.
- **Prioridad:** P2 · **Complejidad estimada:** L · **Estado:** PROPOSED
- **Dependencias:** FA-06, FA-01
- **Investigar antes:** input original PC completo (APIs, polling,
  mapeos cableados, menus).
- **Riesgos / incógnitas:** input cableado a lógica; menús sin ratón (I-18).
- **Relación con comportamiento original:** debe poder replicar el esquema
  original como perfil por defecto.
- **Compatible con Original Mode:** Sí (perfil original) · **Pertenece a Modern Mode:** Sí
- **Notas:** base de M-07…M-12.

## M-07 — XInput

- **Categoría:** Nivel 2 — Input · API
- **Descripción:** Soporte para mandos XInput modernos.
- **Prioridad:** P2 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** M-06, FA-06
- **Investigar antes:** qué API usa el juego (Fase 1: sin imports a
  DirectInput en el binario EU — joystick vía WinMM `joyGetPosEx`;
  carga dinámica pendiente de descartar en Fase 2); enumeración de
  dispositivos.
- **Riesgos / incógnitas:** triggers/sticks sin equivalente original.
- **Relación con comportamiento original:** nueva vía de entrada; no cambia
  acciones del juego.
- **Compatible con Original Mode:** Sí (opcional) · **Pertenece a Modern Mode:** Sí
- **Notas:** —

## M-08 — DualShock / DualSense

- **Categoría:** Nivel 2 — Input · API
- **Descripción:** Investigar compatibilidad con DualShock/DualSense.
- **Prioridad:** P2 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** M-06, FA-06
- **Investigar antes:** mismas APIs que M-07 + particularidades Sony en PC.
- **Riesgos / incógnitas:** no asumir disponibilidad de todas las funciones
  (táctil, gatillos adaptativos, etc.).
- **Relación con comportamiento original:** nueva vía de entrada.
- **Compatible con Original Mode:** Sí (opcional) · **Pertenece a Modern Mode:** Sí
- **Notas:** evaluar frente a XInput según esfuerzo real.

## M-09 — Button Remapping

- **Categoría:** Nivel 2 — Input · Configuración
- **Descripción:** Remapeo completo de botones.
- **Prioridad:** P2 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** M-06
- **Investigar antes:** tabla de acciones del juego; entradas cableadas.
- **Riesgos / incógnitas:** acciones contextuales; menús con entrada fija.
- **Relación con comportamiento original:** el mapeo por defecto replica el
  original.
- **Compatible con Original Mode:** Sí · **Pertenece a Modern Mode:** Sí
- **Notas:** exponer en M-20 (Controls).

## M-10 — Analog Deadzones

- **Categoría:** Nivel 2 — Input · Configuración
- **Descripción:** Configuración de deadzones para sticks analógicos.
- **Prioridad:** P2 · **Complejidad estimada:** S · **Estado:** PROPOSED
- **Dependencias:** M-06, M-07/M-08
- **Investigar antes:** si el juego aplica alguna zona muerta propia.
- **Riesgos / incógnitas:** doble deadzone (juego + sistema).
- **Relación con comportamiento original:** ajuste de sensibilidad, no de
  mecánicas.
- **Compatible con Original Mode:** Sí · **Pertenece a Modern Mode:** Sí
- **Notas:** —

## M-11 — Vibration / Rumble

- **Categoría:** Nivel 2 — Input · Feedback
- **Descripción:** Vibración cuando el dispositivo/API lo permita.
- **Prioridad:** P3 · **Complejidad estimada:** S · **Estado:** PROPOSED
- **Dependencias:** M-06, M-07/M-08
- **Investigar antes:** si el juego original emite eventos aptos para
  mapearse a rumble (daño, etc.).
- **Riesgos / incógnitas:** feature nueva sin equivalente original.
- **Relación con comportamiento original:** aditivo y desactivable.
- **Compatible con Original Mode:** Sí (desactivado) · **Pertenece a Modern Mode:** Sí
- **Notas:** opcional, desactivado por defecto en Original Mode.

## M-12 — Modern Camera Control ⭐

- **Categoría:** Nivel 2 — Input/Cámara · Sistema
- **Descripción:** Permitir (propuesta) stick izquierdo → movimiento y
  stick derecho → cámara.
- **Prioridad:** P2 · **Complejidad estimada:** L · **Estado:** PROPOSED
- **Dependencias:** M-06, FA-07
- **Investigar antes:** comportamiento de cámara de la versión **PC**;
  diferencias con PS1 (referencia secundaria: control asociado a L1/R1 en
  PS1, **no confirmado en PC**); arquitectura de cámara; límites.
- **Riesgos / incógnitas:** cámara PC desconocida; posible acoplamiento a
  movimiento/triggers de nivel; riesgo de romper diseño de niveles.
- **Relación con comportamiento original:** debe documentarse el
  comportamiento PC antes de proponer alternativas; cualquier cambio es
  Modern Mode y opcional.
- **Compatible con Original Mode:** Sí (cámara original intacta) · **Pertenece a Modern Mode:** Sí
- **Notas:** idea importante pero no confirmada; tratar como hipótesis.

---

# NIVEL 3 — TIMING Y RENDER MODERNO

> Delicado. Solo después de comprender perfectamente el game loop (FA-04,
> FA-05). Separar siempre **RENDER RATE** de **GAME SIMULATION / LOGIC**.

## M-13 — Modern Render Refresh Rates

- **Categoría:** Nivel 3 — Timing/Render
- **Descripción:** Renderizar a 60/120/144 Hz y otros refrescos modernos
  sin alterar la lógica original.
- **Prioridad:** P2 · **Complejidad estimada:** XL · **Estado:** PROPOSED
- **Dependencias:** FA-04, FA-05, FA-03
- **Investigar antes:** game loop completo; acoplamiento FPS↔lógica (I-01);
  por qué 30 FPS es "correcto" hoy.
- **Riesgos / incógnitas:** lógica ligada a frames (hipótesis); física y
  timers dependientes del refresco.
- **Relación con comportamiento original:** la simulación debe permanecer
  idéntica; solo el render se desacopla.
- **Compatible con Original Mode:** Sí (30 FPS fijos) · **Pertenece a Modern Mode:** Sí
- **Notas:** no asumir que subir FPS de gameplay sea correcto.
  E-3/M-13 (2026-10-10): flujo temporal estático en REVERSE_ENGINEERING
  Etapa C (throttle = `swap_interval` Glide; sim serie con render);
  acoplamiento cuantitativo pendiente de dinámica (E-2 listo E-2.3,
  ejecución pendiente).
  Línea B: 75 Hz pedidos confirmados (`0x46324D`); lectores `+0x10C`
  R1/R2 localizados (R2 consume el cociente: el diseño debe preservar
  esa ruta); boceto desacoplado mínimo en REVERSE_ENGINEERING
  (diseño solo, NO implementar; desbloqueo FPS prohibido regla 21).
  Propuesta completa: `docs/M13_DECOUPLE_PROPOSAL.md` (puerta §10).
  Pívot 2026-10-10: R2 WORD = dominio VOZ (tabla `0x5ACBE4` filas
  por voz, hipótesis fuerte; audio wall-clock obligatorio — valida
  propuesta §4); núcleo acumulador §4 implementado+verificado en
  `tools/m13-core` (harness 120 checks OK; NO integrado, M-13 sigue
  PROPOSED); shim instrumentación `tools/glide-shim` (fuente + .def
  36/36 vs IAT; sin compilar, requiere MinGW-32).

## M-14 — Unlimited Render Mode (experimental)

- **Categoría:** Nivel 3 — Timing/Render · Experimental
- **Descripción:** Investigar renderizado sin límite con lógica estable y
  desacoplada, si técnicamente es posible.
- **Prioridad:** P3 · **Complejidad estimada:** XL · **Estado:** PROPOSED
- **Dependencias:** M-13, FA-04, FA-05
- **Investigar antes:** lo mismo que M-13 + estabilidad a FPS variable.
- **Riesgos / incógnitas:** arquitectura real desconocida; tearing, pacing,
  consumo; bugs dependientes de timing.
- **Relación con comportamiento original:** experimental; nunca por defecto.
- **Compatible con Original Mode:** No (solo Modern, opcional) · **Pertenece a Modern Mode:** Sí
- **Notas:** tratar como objetivo experimental hasta conocer la arquitectura.

---

# NIVEL 4 — WIDESCREEN Y PRESENTACIÓN MODERNA

> Depende de renderer + cámara + resolución entendidos. No estirar 4:3.

## M-15 — Native 16:9

- **Categoría:** Nivel 4 — Display/Widescreen
- **Descripción:** 16:9 real si técnicamente es posible (no estirado).
- **Prioridad:** P2 · **Complejidad estimada:** L · **Estado:** PROPOSED
- **Dependencias:** M-04, FA-03, FA-07, FA-13
- **Investigar antes:** viewport, aspect ratio, cámara, clipping, HUD,
  menús, FMV, elementos 2D.
- **Riesgos / incógnitas:** HUD/menús 4:3 cableados; culling por frustum
  4:3; FMV con aspect fijo.
- **Relación con comportamiento original:** solo presentación; gameplay
  intacto.
- **Compatible con Original Mode:** No (4:3 original) · **Pertenece a Modern Mode:** Sí
- **Notas:** —

## M-16 — Ultrawide (21:9, 32:9)

- **Categoría:** Nivel 4 — Display/Widescreen
- **Descripción:** Soporte futuro para 21:9 y 32:9, después de 16:9.
- **Prioridad:** P3 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** M-15, M-17
- **Investigar antes:** lo mismo que M-15 en ratios extremos.
- **Riesgos / incógnitas:** HUD en bordes lejanos; culling; trampas visuales
  fuera del diseño original.
- **Relación con comportamiento original:** solo presentación.
- **Compatible con Original Mode:** No · **Pertenece a Modern Mode:** Sí
- **Notas:** —

## M-17 — Adaptive FOV / Camera for Widescreen

- **Categoría:** Nivel 4 — Cámara/Widescreen
- **Descripción:** Adaptar cámara/FOV al aspect ratio correctamente.
- **Prioridad:** P2 · **Complejidad estimada:** L · **Estado:** PROPOSED
- **Dependencias:** M-15, FA-07
- **Investigar antes:** FOV original; cámara; clipping; geometría visible;
  HUD; comportamiento 4:3 vs 16:9 vs ultrawide.
- **Riesgos / incógnitas:** no asumir que aumentar FOV sea correcto; puede
  alterar percepción de velocidad/dificultad.
- **Relación con comportamiento original:** en 4:3 debe ser idéntico.
- **Compatible con Original Mode:** Sí (inactivo en 4:3) · **Pertenece a Modern Mode:** Sí
- **Notas:** —

---

# NIVEL 5 — WINDOWS MODERNO / MULTI-DISPLAY

## M-18 — Robust Alt+Tab

- **Categoría:** Nivel 5 — Integración Windows
- **Descripción:** Recuperación correcta de fullscreen/borderless, renderer,
  audio e input tras Alt+Tab.
- **Prioridad:** P2 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** FA-14, FA-03, FA-08, M-05
- **Investigar antes:** manejo de foco/pérdida de dispositivo en el exe.
- **Riesgos / incógnitas:** exclusive mode legacy; audio que no se reanuda.
- **Relación con comportamiento original:** robustez, no cambio de juego.
- **Compatible con Original Mode:** Sí · **Pertenece a Modern Mode:** Sí
- **Notas:** —

## M-19 — Multi-monitor / DPI Awareness

- **Categoría:** Nivel 5 — Compatibilidad Windows · Meta, no feature
- **Descripción:** Compatibilidad con múltiples monitores, resoluciones y
  escalados distintos (100/125/150/200%), cambio de monitor y
  fullscreen/borderless en secundario. No es un "gestor de monitores".
- **Prioridad:** P2 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** FA-14, FA-13, M-05
- **Investigar antes:** cómo elige el juego adaptador/monitor; DPI
  awareness del exe (manifiesto ausente probable, sin confirmar).
- **Riesgos / incógnitas:** APIs de época con un solo display asumido.
- **Relación con comportamiento original:** solo compatibilidad moderna.
- **Compatible con Original Mode:** Sí · **Pertenece a Modern Mode:** Sí
- **Notas:** verificar vía matriz hardware (COMPATIBILITY.md).

---

# NIVEL 6 — IN-GAME OPTIONS MENU

## M-20 — In-Game Options Menu

- **Categoría:** Nivel 6 — Configuración en juego
- **Descripción:** Menú de opciones dentro del juego para reducir la
  dependencia del launcher externo de la época. Categorías: Video
  (resolución, aspect, fullscreen, borderless, VSync, render limit,
  unlimited, gráficos), Audio (música, efectos, voces, volumen, CD Audio),
  Controls (teclado, mando, cámara, sensibilidad, deadzones, vibración,
  remapeo), Advanced/Compatibility (renderer, Original/Modern Mode,
  experimentales).
- **Prioridad:** P2 · **Complejidad estimada:** XL · **Estado:** PROPOSED
- **Dependencias:** M-01, M-06, M-04, M-09, M-10, M-11, M-13, M-21, M-22
- **Investigar antes:** qué configura el launcher externo y cómo se
  almacena; UI/HUD del juego para integrarlo.
- **Riesgos / incógnitas:** no asumir que el launcher pueda eliminarse;
  gran superficie de integración.
- **Relación con comportamiento original:** añadido; el juego base intacto.
- **Compatible con Original Mode:** Sí (menú disponible, valores época) · **Pertenece a Modern Mode:** Sí
- **Notas:** es integración de muchos M-xx (incluye futuro selector de voces M-25 en Audio); va tarde a propósito.

---

# NIVEL 7 — ORIGINAL MODE / MODERN MODE

## M-21 — Original Mode

- **Categoría:** Nivel 7 — Modos · Preservación
- **Descripción:** Modo que preserva el comportamiento PC original:
  comportamiento, timing, presentación y compatibilidad histórica.
- **Prioridad:** P1 · **Complejidad estimada:** M · **Estado:** PROPOSED
- **Dependencias:** FA-01…FA-15 (definición de "original" verificada)
- **Investigar antes:** qué es exactamente "original" por sistema
  (el trabajo de toda la Fase A).
- **Riesgos / incógnitas:** definir "original" sin evidencia completa.
- **Relación con comportamiento original:** es la referencia viva del mismo.
- **Compatible con Original Mode:** Es Original Mode · **Pertenece a Modern Mode:** No
- **Notas:** sin esto, Modern Mode no tiene contra qué validarse.

## M-22 — Modern Mode

- **Categoría:** Nivel 7 — Modos · Modernización
- **Descripción:** Modo que permite las mejoras REZengineered:
  resoluciones modernas, 16:9, ultrawide, controles y cámara modernos,
  borderless, render moderno, unlimited, visuales opcionales.
- **Prioridad:** P2 · **Complejidad estimada:** L · **Estado:** PROPOSED
- **Dependencias:** M-21 + los M-xx que agrupe
- **Investigar antes:** cada mejora que incluya.
- **Riesgos / incógnitas:** convertirlo en excusa para modificar gameplay
  sin control — prohibido por definición.
- **Relación con comportamiento original:** respeta la identidad del juego;
  mejoras de presentación/compatibilidad, no de diseño.
- **Compatible con Original Mode:** No (es el otro modo) · **Pertenece a Modern Mode:** Es Modern Mode
- **Notas:** —

---

# NIVEL 8 — ENHANCED VISUALS / HD TEXTURE PACK

## M-23 — Optional Enhanced Visuals (HD Texture Pack)

- **Categoría:** Nivel 8 — Visuales opcionales
- **Descripción:** Sistema de mejoras visuales opcionales; primera idea:
  pack HD estilo remaster. Opcional, desactivable, independiente del modo
  original, no obligatorio, no mezclado con assets originales.
  Original Mode → assets originales; Modern Mode → puede activarlos.
- **Prioridad:** P3 · **Complejidad estimada:** L (+ trabajo de assets XL,
  externo) · **Estado:** PROPOSED
- **Dependencias:** FA-03, M-22
- **Investigar antes:** formato de texturas, extracción, sustitución,
  streaming, memoria, mipmaps, filtrado, compatibilidad con renderer.
- **Riesgos / incógnitas:** formatos propietarios desconocidos; memoria de
  época; licencias de assets nuevos.
- **Relación con comportamiento original:** intacta (solo assets).
- **Compatible con Original Mode:** Sí (desactivado) · **Pertenece a Modern Mode:** Sí
- **Notas:** no implementar nada ahora; el pack en sí es proyecto aparte.
- **Candidatos externos (2026-10-09, S-24, solo documentados):** upscale
  «Screen Titles» (Mega; contenido/licencia/autor sin verificar) +
  extracción PC vía H-01/H-02 (formato .VFX descrito; código sin
  licencia, ejecución sin verificar). Nada validado; nada incorporado.

---

# NIVEL 9 — LOCALIZACIÓN

## M-24 — Localization / Language Selection

- **Categoría:** Nivel 9 — Localización
- **Descripción:** Estudiar la posibilidad futura de localizar **textos**
  (no doblar voces): English (original) + Español (primero) + más idiomas
  después. Alcance posible: menús, opciones, mensajes, interfaz y demás
  textos que realmente existan en la versión PC. Arquitectura ideal:
  `Localization → English / Español / Other`, con selector en el futuro
  menú de opciones (M-20).
- **Prioridad:** P3 (interesante, posterior a fundamentos) · **Complejidad estimada:** Desconocida (TO INVESTIGATE) · **Estado:** PROPOSED
- **Dependencias:** FA-01, M-20, M-22
- **Investigar antes:** dónde están los textos; codificación; exe vs
  ficheros externos; identificación de strings; sistema de fuentes;
  soporte de caracteres para español (ñ, tildes); límites de longitud;
  caracteres especiales; viabilidad de un sistema externo de localización
  sin alterar el comportamiento original.
- **Riesgos / incógnitas:** strings cableados; fuentes bitmap sin acentos;
  límites de longitud; texto como textura; renders de texto propietarios.
- **Relación con comportamiento original:** English original = source of
  truth; las traducciones serían aditivas y opcionales. **No asumir que
  las traducciones de Gex Trilogy sean reutilizables; Trilogy no es
  fuente de verdad.** Otras versiones, solo referencias secundarias.
- **Compatible con Original Mode:** Sí (English original) · **Pertenece a Modern Mode:** Sí
- **Notas:** sin fase de roadmap asignada todavía (Ola 3, TBD).
  No confundir con M-25 (selección de voces UK/USA, audio).

# NIVEL 10 — AUDIO / VOCES

## M-25 — Voice Pack Selection (UK / USA)

- **Categoría:** Nivel 10 — Audio/Voces
- **Descripción:** Permitir al jugador seleccionar desde el futuro menú de
  opciones (M-20, categoría Audio) entre las voces originales UK (Leslie
  Phillips, edición europea) y USA (Dana Gould, edición estadounidense),
  utilizando audio original compatible aportado por el usuario cuando esté
  disponible. La preferencia debe poder guardarse y persistir entre
  sesiones si la arquitectura lo permite.
- **Prioridad:** P3 (provisional) · **Complejidad estimada:** Desconocida
  (TO INVESTIGATE; estimación provisional NO confirmada: S–M si basta con
  redirigir la carpeta de voces, L+ si hay que re-mapear frases o FMV) ·
  **Estado:** PROPOSED
- **Dependencias:** FA-08 (audio/voces), FA-01 (variante US), M-20
  (selector en menú), M-22 (Modern Mode)
- **Investigar antes (detalle en S-20):** set EU: 400 `.SAG` en
  `AUDIO/VOICEUK/` (CONFIRMED, inventario §8); set USA de PC (`voice/`):
  estructura y cobertura UNKNOWN (solo el nombre de carpeta, vía Tgames
  F-01/F-03); si ambos sets cubren las mismas frases o hay diferencias
  regionales de diálogos/chistes (UNKNOWN; indicios de interpretaciones
  distintas, no PC-específicos); si las voces FMV (`MOVIE/*.SAG`,
  hipótesis audio-por-tamaños) van separadas de las in-game (UNKNOWN);
  cómo selecciona el exe los ficheros de voz (UNKNOWN, Fase 2); formato
  `.SAG` (UNKNOWN-PENDING).
- **Riesgos / incógnitas:** sets no paralelos (nombres/cobertura distintos);
  chistes reescritos por región (Gould escribió sus líneas; Phillips =
  variación «muy distinta, más culta» según VGFacts — fuentes generales,
  no PC-específicas); FMV con voces integradas (no conmutables por
  fichero); comportamiento con ficheros ausentes (fallback UK por definir);
  procedencia: el audio USA es comercial — modelo BYO (copia propia del
  usuario), jamás redistribuir ni añadir voces al repo.
- **Relación con comportamiento original:** UK = base EU por defecto
  (Original Mode intacto); el pack USA sería opt-in en Modern Mode, solo
  assets, sin tocar la lógica del juego. No prometer cobertura total hasta
  verificar frase a frase.
- **Compatible con Original Mode:** Sí (UK por defecto) · **Pertenece a Modern Mode:** Sí
- **Notas:** no confundir con M-24 (localización de TEXTOS). Voz JP
  (consola) fuera de alcance. Gex Trilogy no es fuente de audio. Sin fase
  de roadmap asignada todavía (Ola 3, TBD).

---

# NIVEL 11 — CINEMÁTICAS / CALIDAD DE VIDA

## M-26 — Skippable Intro Cinematic

- **Categoría:** Nivel 11 — Cinemáticas/Calidad de vida
- **Descripción:** Permitir omitir (tecla/mando) la cinemática de
  introducción que muestra a Rez siguiendo a Gex y termina con el nombre
  del juego justo antes del menú principal. Actualmente no se puede
  omitir (CONFIRMADO por observación, Hito 1 / 2ª sesión).
- **Prioridad:** P3 (baja; no distrae del diagnóstico gráfico ni de la
  presentación correcta) · **Complejidad estimada:** Desconocida
  (TO INVESTIGATE) · **Estado:** PROPOSED
- **Dependencias:** FA-10 (reproductor FMV/códec), M-20 (opción en menú,
  si procede), M-22 (Modern Mode)
- **Investigar antes:** qué reproductor/códec usa la intro (FA-10);
  punto de entrada/salida de la cinemática; implementación compatible con
  el ejecutable original (NO asumir modificación del exe: valorar
  interceptar input, loader externo u otras vías en Fase 3+).
- **Riesgos / incógnitas:** mecanismo de reproducción UNKNOWN; no romper
  Original Mode (la cinemática debe seguir intacta por defecto).
- **Relación con comportamiento original:** añadido opt-in; Original Mode
  la reproduce siempre como el original.
- **Compatible con Original Mode:** Sí (siempre visible) · **Pertenece a Modern Mode:** Sí
- **Notas:** sin fase de roadmap asignada todavía (Ola 3, TBD).
  No confundir con bugs de intro (I-14): la intro FUNCIONA; esto es QoL.

---

# DEFERRED / LONG-TERM RESEARCH (fuera del roadmap normal)

## X-01 — Linux / Proton / Steam Deck — DEFERRED

- **Categoría:** Plataforma · Baja prioridad
- **Descripción:** Posible investigación futura de compatibilidad
  Linux/Proton/Steam Deck.
- **Prioridad:** P3 · **Complejidad estimada:** Desconocida · **Estado:** DEFERRED
- **Dependencias:** todo lo anterior en Windows primero.
- **Investigar antes:** (cuando se active) Wine/Proton con el exe + wrappers.
- **Riesgos / incógnitas:** dispersar el foco del proyecto.
- **Relación con comportamiento original:** n/a todavía.
- **Compatible con Original Mode:** n/a · **Pertenece a Modern Mode:** n/a
- **Notas:** explícitamente NO objetivo principal; no compite con Windows.
  La info Wine existente (VOGONS) queda como secundaria (S-05).

## X-02 — Possible Engine Rewrite in Rust + Bevy (long-term research)

- **Categoría:** Experimental / Long-term research (fuera del roadmap normal)
- **Descripción:** Dejar registrada la posibilidad de estudiar, en un futuro
  muy lejano, una reimplementación del engine en Rust + Bevy + arquitectura
  moderna a partir del conocimiento del RE. **No** convierte el proyecto en
  un remake; **no** abandona el engine original. Hipotéticamente podría ser
  una arquitectura independiente capaz de reproducir el comportamiento original.
- **Prioridad:** P3 (muy baja) · **Complejidad estimada:** Desconocida (posible XL) · **Estado:** DEFERRED
- **Dependencias:** conocimiento suficiente del engine original (FA-01…FA-15
  y Fases 1–4 esencialmente completas).
- **Investigar antes (preguntas fundamentales):** ¿tenemos suficiente
  conocimiento del engine?; ¿podemos reproducir su lógica?; ¿qué sistemas
  reconstruir?; ¿qué es específico de PC vs del engine?; ¿cómo reproducir
  física, cámara, timing, animaciones, colisiones, audio, niveles, enemigos,
  triggers?; ¿diferencias reimplementación vs ejecutable?; ¿compatibilidad
  con assets originales?; ¿reimplementación vs remake?
- **Riesgos / incógnitas:** explosión de alcance; desenfoque del proyecto;
  línea reimplementación/remake; compatibilidad de assets; todo desconocido.
- **Relación con comportamiento original:** el original seguiría siendo la
  fuente de verdad contra la que validarse.
- **Compatible con Original Mode:** n/a · **Pertenece a Modern Mode:** n/a
- **Notas:** solo se estudiaría si el conocimiento futuro lo justifica. El
  proyecto principal sigue siendo entender → documentar → analizar →
  corregir/modernizar el juego original.

---

# Mapa objetivo → fase del roadmap

| Objetivo | Fase |
|---|---|
| FA-01…FA-15 | 0–4 (Planning → Foundation/Compatibility) |
| M-01…M-05 | 5 (Low-risk Modernization) |
| M-06…M-12 | 6 (Modern Input) |
| M-13, M-14 | 7 (Timing / Render Decoupling) |
| M-15…M-17 | 8 (Widescreen / Resolution / Camera) |
| M-18, M-19 | 9 (Windows Modern Integration) |
| M-20 | 10 (In-Game Configuration) |
| M-21, M-22 | 11 (Original Mode / Modern Mode) |
| M-23 | 12 (Optional Enhanced Visuals) |
| M-24 | Sin fase asignada (post-11, TBD) |
| M-25 | Sin fase asignada (post-11, TBD) |
| M-26 | Sin fase asignada (post-11, TBD) |
| Matriz hardware | 13 (Extended Compatibility) |
| X-01 | 14 (Experimental / Deferred) |
| X-02 | 14 mención long-term (no es fase de implementación) |

Ver [ROADMAP.md](ROADMAP.md). Orden de principio a fin:

```text
ENTENDER → BASE → SENCILLO → DEPENDIENTE → COMPLEJO → EXPERIMENTAL
```
