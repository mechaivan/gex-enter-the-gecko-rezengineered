# SOUL-RE — Referencia comparativa del motor (Soul Reaver PS1) para la RE de Gex PC

- **Estado:** investigación documental (2026-10-11). Solo análisis estático de
  fuentes públicas. NO se ha ejecutado Soul Reaver ni Gex, ni se ha tocado el
  ejecutable, la DLL, el shim, la instalación ni el Registro.
- **Jerarquía de evidencia:** el ejecutable original EU de Gex PC sigue siendo la
  única fuente de verdad del proyecto. Este repositorio externo es una
  **referencia comparativa**, nunca una prueba automática del comportamiento de
  la versión PC.
- **Regla aplicada:** ninguna afirmación de este documento sobre Gex PC se da por
  demostrada; todas las hipótesis llevan su método de verificación pendiente
  (§6).

---

## 1. Resumen ejecutivo

`fmil95/soul-re` («Soul Reaverse») es un proyecto de reconstrucción del código
fuente de *Legacy of Kain: Soul Reaver* (PS1, Crystal Dynamics/1999) a partir de
un **prototipo con símbolos de depuración completos** (construido dos semanas
antes del lanzamiento), lo que permite tener nombres reales de funciones y
ficheros. Su README identifica el ejecutable principal `SLUS_007.08` como
**«Gex 2 engine + game code»** y reparte el resto de la lógica en **overlays**
(`.drm`): el mismo linaje de motor que nos interesa para Gex: Enter the Gecko PC.

Hallazgos más útiles para nuestro proyecto, por orden de prioridad:

1. **Modelo de tiempo del motor** con `timeMult` en punto fijo 12 bits
   (4096 = un «frame» de 33 ms), `frameRateLock` 30/20 fps, modo 24 fps y
   acumulador de frames lógicos (`gameFramePassed`): un marco excelente para
   formular hipótesis comprobables sobre los **25 FPS estables** de Gex PC.
2. **Integración física** (`PhysicsMove`) que escala velocidad/aceleración por
   el mismo `timeMult` (pos += v·t/4096 + a·t²/8192; clamp a `max*Vel`).
3. **Orden de actualización por frame** documentado paso a paso en
   `GAMELOOP_Process` (eventos → sonido → instancias → transformadas G2 → FX →
   VM → planificación → colisiones → cámara → render).
4. **Modelo de objetos** (`Instance`) con vtablas por convención
   (`processFunc`/`queryFunc`/`messageFunc`…), mensajería/consultas
   (`INSTANCE_Post/Query/Broadcast`), enlaces padre-hijo y ciclo de vida con
   introducción/expulsión y desactivación por distancia en tiempo repartido.
5. **Streaming y overlays**: cola de carga de 40 entradas con estados y
   callbacks, `MEMPACK` con recolección de basura de bloques relocalizables,
   binarios de overlay con **tabla de reubicación** y punto de entrada por
   **tabla de funciones versionada** (ejemplo verificado: `cinemax`).
6. **Animación G2**: secciones/claves, transformadas por cuaterniones,
   **controladores** procedurales que se superponen a la animación clave a clave,
   y compresión de canales con **ADPCM** (tablas de paso/índice): un algoritmo
   general con posible eco en los formatos de animación de Gex PC.

Ninguna de estas piezas se ha verificado contra `GEX3D.EXE` todavía: todas
quedan como material de contraste para las fases estática (Ghidra) y dinámica
(shim Glide) ya previstas en el proyecto.

---

## 2. Fuentes y commit exacto analizado

| Dato | Valor |
|---|---|
| Repositorio | `https://github.com/fmil95/soul-re` (fork de trabajo: no; proyecto original) |
| Commit analizado | `59d1ee87749da95b6ced413a9f60b30a6fb91d45` («Merge pull request #97 from StableCrimson/walbossb») |
| Fecha de ese commit (push del repo) | 2026-09-21 (último `pushed_at` del repositorio) |
| Clon usado | copia superficial **fuera del repo del proyecto** (`/tmp/soul-re`), no versionada aquí |
| Licencia | MIT — © 2024-2026 Federico Milesi & Stable Crimson (ver §9) |
| README (identificación de bins) | [`README.md`](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/README.md) |
| Guía de decompilación | [`docs/DECOMPILATION.MD`](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/docs/DECOMPILATION.MD) |
| Build | [`Makefile`](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/Makefile) (CC `tools/gcc-2.8.1-psx/cc1` + `maspsx`, ASPSX 2.81; `make report` → `objdiff report generate`) |
| Publicación de métricas | [`.github/workflows/progress_report.yml`](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/.github/workflows/progress_report.yml) → artefacto `build/progress.json` → decomp.dev |
| Categorías de unidades | [`config/objdiff.yaml`](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/config/objdiff.yaml) |

### 2.1. Alcance real y métrica de progreso (sin dar el 85 % por sentado)

Página pública de progreso (consultada 2026-10-11): *«Soul Reaver is 85.05 %
decompiled»*, medida **en el commit `59d1ee8`** (el mismo que se ha clonado),
con *Code (751.84 kB)* y *Data (156.62 kB)* agregados de todas las unidades.
Desglose por unidad consultado: **Main Executable 90.04 %** (Code 653.01 kB,
Data 76.38 kB). Es decir, el agregado de ~85 % está **lastrado por los overlays**,
no por el ejecutable principal.

Qué mide cada concepto:

- **Código decompilado (`matched_code_percent`):** porcentaje de **bytes de
  código** que coinciden byte a byte con el binario original tras recompilar.
  Es la medida que muestra el titular (85.05 %).
- **Funciones emparejadas (`matched_functions`):** recuento de **funciones**
  íntegramente emparejadas (badge del README). Es una métrica distinta: no pesa
  por tamaño, así que puede diferir del % de bytes. *(No se ha podido recuperar
  el recuento exacto en esta sesión — dato pendiente; se declara como
  limitación, no se estima.)*
- **Código de datos (`Data`):** los segmentos de datos (tablas, gráficos
  empaquetados, etc.) se puntúan **aparte**: 156.62 kB en total, 76.38 kB en el
  ejecutable principal.
- **Cobertura de overlays:** cada `.drm` (`cinemax`, `mcardx`, `hunter`,
  `skinner`, `wallcr`, `aluka`, `skinbos`, `kain`, `walboss/b`,
  `alukabss`, `roninbss`) es una **unidad independiente** con su propia
  categoría; al ser la parte menos avanzada, su peso arrastra el agregado.
  Comprobación independiente en el árbol: de **232 funciones aún en
  ensamblador (`INCLUDE_ASM`)**, la mayoría están en overlays
  (`wallcr` 52, `mcardx` 50, `walboss` 38…), no en `Game/`.

Dos cautelas importantes: (a) «emparejado» significa *byte-idéntico tras
recompilar*, **no** «comprendido» — y al revés, hay código C allí que aún no está
emparejado (por eso existen los stubs `INCLUDE_ASM`); (b) la página raíz de
decomp.dev mostraba el aviso **«This project is disabled.»** en la consulta
(la vista por categoría sí renderizaba), por lo que la cifra debe tratarse como
**último dato publicado** para ese commit, no necesariamente «en vivo».

*(Nota: el «85 %» se ha verificado como correspondiente al commit analizado; no
se extrapola más.)*

---

## 3. Mapa de subsistemas → archivos relevantes

Todo bajo `src/Game/` del repositorio externo (enlaces permanentes al commit
`59d1ee8`). Los nombres en negrita son el objetivo de esta investigación.

| Subsistema | Ficheros principales | Notas de plataforma |
|---|---|---|
| Bucle principal | `GAMELOOP.c`, `PSX/MAIN.c` | PS1: `VSync`, callbacks VBlank/GPU |
| Timing | `TIMER.c`, `G2/TIMERG2.c` | PS1: contador RCNT 33.8688 MHz |
| Cámara | `CAMERA.c`, `SPLINE.c` | GTE para vectores |
| Colisión | `COLLIDE.c` | GTE para normales/transformadas |
| Física | `PHYSICS.c`, `PHYSOBS.c` | aritmética entera |
| Instancias | `INSTANCE.c`, `LIST.c`, `OBTABLE.c` | — |
| Streaming/carga | `STREAM.c`, `STRMLOAD.c`, `LOAD3D.c`, `MEMPACK.c`, `RESOLVE.c` | CD-ROM (CdlLOC), bigfile |
| Pipeline/render | `PIPE3D.c`, `DRAW.c`, `LIGHT3D.c`, `VRAM.c` | GTE, OT (ordering tables), PrimPool |
| Animación G2 | `G2/ANIMG2.c`, `G2/ANMCTRLR.c`, `G2/ANMDECMP.c`, `G2/ANMINTRP.c`, `G2/INSTNCG2.c`, `G2/QUATG2.c`, `G2/POOLMMG2.c`, `G2/MAING2.c` | GTE (`hasm_segmatrixop`) |
| Scripting/lógica | `EVENT.c`, `SCRIPT.c`, `SCRIPT2.c`, `SIGNAL.c`, `VM.c` | — |
| Sonido | `SOUND.c`, `VOICEXA.c`, `PSX/` (AAD) | SPU + XA (CD) |
| Overlays | `src/Overlays/*/`, `Object->relocModule` | reubicación en runtime |

Volumen del árbol (para dimensionar): 111 ficheros `.c`, ~112 000 líneas; de
ellas ~95 000 en `src/Game/` (sin overlays).

---

## 4. Hallazgos priorizados

Clasificación usada en cada hallazgo:

- **[MOTOR]** probable código compartido del motor (linaje Gex 2).
- **[SR]** específico de Soul Reaver.
- **[PS1]** específico de PlayStation (MIPS/GTE/CD-ROM/VBlank/SPU…).
- **[IND]** indeterminado con la evidencia actual.

### H-01. Modelo de tiempo: `timeMult` 4.12 + `frameRateLock` + acumulador de frames lógicos — **[MOTOR]**

`GAMELOOP_DoTimeProcess()`
([GAMELOOP.c L1583](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1583))
es el corazón del timing. Esquema:

- `frameRateLock` tiene dos valores válidos (1 = 33 ms, 2 = 50 ms) y se recorta
  a ese rango; `lastLoopTime` toma 33 o 50 ms según el lock.
- `timeMult = (lastLoopTime << 12) / 33` → **punto fijo de 12 bits con
  `4096` = un frame de 30 fps**. Con 33 ms vale 4096; con 50 ms ≈ 6206.
- Acumulador: `timeSinceLastGameFrame += timeMult`; cada vez que supera 4097 se
  marca `gameFramePassed = 1`, se resta 4096 y se incrementa `fps30Count`:
  **frames lógicos a 30 Hz desacoplados del frame de render**.
- Existe una rama «desacoplada» (`decoupleGame != 0`) que mide tiempo real
  (`holdTime - currentTime`, en ms), lo **recorta a [lockRate, 66]** y deriva
  `timeMult` de él.
- Modo 24 fps: `GAMELOOP_Set24FPS()/Reset24FPS()`
  ([L1573](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1573))
  activan `frameRate24fps`; en la rama medida resta 9 ms al presupuesto
  (33 − 9 = 24 ms).
- Campos en `GameTracker`
  ([TYPES.h L4209-4437](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/include/Game/TYPES.h#L4209);
  `timeMult` en [L4410](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/include/Game/TYPES.h#L4410)):
  `timeMult`, `globalTimeMult`, `spectralTimeMult`, `materialTimeMult`,
  `lastLoopTime`, `currentTicks`, `totalTime`, `frameRateLock`, `frameRate24fps`.

**Por qué importa:** es la mejor plantilla conceptual para explicar los **25 FPS
estables** de Gex PC. Hipótesis a comprobar (marcadas como hipótesis, no como
hecho): un lock fijo de 40 ms (25 fps) heredado de la cadencia PAL
(50 Hz / 2), o bien un `timeMult` fijo con render a la frecuencia del lock.
**Método de verificación:** buscar en `GEX3D.EXE` constantes de tiempo fijas
(33/40/50/66 ms; 4096/4097; QPC o `timeGetTime`) y medir la cadencia real de
`grBufferSwap` con nuestra propia instrumentación (ver §8, V-2).

### H-02. Convención de punto fijo 12 bits (4096 = 1.0) — **[MOTOR]**

Aparece de forma sistemática: `timeMult` (4096 = frame de referencia),
`PhysicsMove` (divisores 4096/8192), `Decouple_AngleMoveToward`
([CAMERA.c L1437](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/CAMERA.c#L1437):
`step = step * timeMult >> 12`), y `G2Timer_GetFrameTime`
([TIMERG2.c L6](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/TIMERG2.c#L6):
`(timeMult * 100) / 4096`, con suelo 1). Es una **convención de motor**, no de
plataforma.
**Verificación:** buscar usos de `4096`/`0x1000` y desplazamientos `<<12`/`>>12`
junto a rutinas de tiempo y física en el desensamblado de Gex PC.

### H-03. Pacing: sincronización con el flip gestionado por VBlank — **[PS1]** (concepto trasladable)

En PS1: `VblTick`
([MAIN.c L324](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PSX/MAIN.c#L324))
aplica `PutDispEnv` solo cuando `vblFrames > frameRateLock` (⇒ cambio de pantalla
cada 2 o 3 VBlanks: 30/20 fps en NTSC); `DrawCallback`
([L344](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PSX/MAIN.c#L344))
mide el tiempo de dibujo y pide el flip; y el final de `GAMELOOP_DisplayFrame`
([L1380-1391](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1380))
hace **espera activa** (`while (CheckVolatile(reqDisp) != 0);`) cuando la CPU
termina el frame antes del flip.
**Traslado:** en PC no hay VBlank de consola, pero el patrón «no empiezas el
siguiente frame hasta que el flip se ha consumido» es exactamente lo que hay que
buscar (o descartar) en Gex PC — y es directamente medible con los timestamps de
`grBufferSwap` que ya captura nuestro shim.

### H-04. Modo 24 fps de serie — **[IND]**

`GAMELOOP_Set24FPS/Reset24FPS` existen y se usan en el código de juego (p. ej.
cámara/cinemáticas). El 24 fps aquí es un **factor de tiempo**, no (según el
código leído) una cadencia de swap. **Verificación:** ¿existe en Gex PC un
«modo 24» o un multiplicador 2978/4096? (2978 ≈ 4096·24/33). Buscarlo en
constantes y en puntos de entrada de cinemáticas.

### H-05. Integración física explícita con `timeMult` — **[MOTOR]**

`PhysicsMove(instance, pos, time)`
([PHYSICS.c L1778](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PHYSICS.c#L1778)):
integración de Euler explícita con `time` en unidades 1/4096
(`pos += v·t/4096 + (a·t/4096)·t/8192; v += a·t/4096`), con recorte a
`maxXVel/maxYVel/maxZVel` por eje. Los llamadores pasan
`gameTrackerX.timeMult` (p. ej.
[MONLIB.c L1158](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/MONSTER/MONLIB.c#L1158)).
La física de personaje es una colección de «checks» por modo
(`PhysicsCheckGravity` L263, `PhysicsCheckSliding` L852,
`PhysicsCheckEdgeGrabbing` L553, …) más `PHYSICS_GenericLineCheck` contra el
mundo.
**Verificación:** localizar en Gex PC una rutina equivalente (posiblemente con
las mismas constantes 4096/8192) y comprobar si el parámetro temporal procede de
un multiplicador global.

### H-06. Orden de actualización por frame — **[MOTOR]**

`GAMELOOP_Process`
([L1682](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1682))
fija el orden: tiempo → tiempo de juego/morph → señales/modo → `ResetPrimPool`
→ movimiento entre unidades de stream → `EVENT_DoProcess` → `EVENT_ProcessEvents`
por unidad + `EVENT_BSPProcess` → `SOUND_ProcessInstanceSounds` por marcador →
`INSTANCE_SpatialRelationships` → `INSTANCE_ProcessFunctions` (update de
objetos) → limpieza → `INSTANCE_DeactivateFarInstances` → `G2Instance_BuildTransformsForList`
→ `FX_ProcessList` → `VM_Tick(256)` + `VM_ProcessVMObjectList_S` →
planificación → `DEBUG_Process` → 4 pasadas de colisión → sombras → `CAMERA_Process`
→ `PIPE3D_CalculateWCTransform` + inversa + clip → `GAMELOOP_DisplayFrame`.
**Verificación:** usar esta secuencia como plantilla para etiquetar el bucle
principal de Gex PC cuando llegue el análisis con Ghidra (Fase 2).

### H-07. Modelo de objetos: `Instance` + mensajería — **[MOTOR]**

Struct `_Instance`
([TYPES.h L2467-2677](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/include/Game/TYPES.h#L2467)):
posición/rotación/escala + matrices, `oldPos`/`oldRotation` (deltas por frame),
punteros de comportamiento **`processFunc`/`collideFunc`/`additionalCollideFunc`/
`additionalDrawFunc`/`queryFunc`/`messageFunc`**, `LinkParent/LinkChild/
LinkSibling` + `ParentLinkNode`, velocidades/aceleraciones y topes,
`currentMainState/SubState` + `work0..work9`, `G2Anim anim` **embebido** (180 B),
campos de spline (`targetFrame`, `clipBeg/End`), `checkMask` (filtro de queries
espaciales). API: `INSTANCE_Post`
([L1506](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/INSTANCE.c#L1506)),
`INSTANCE_Query` ([L1492](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/INSTANCE.c#L1492)),
`INSTANCE_Broadcast` ([L1520](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/INSTANCE.c#L1520)),
ciclo de vida `INSTANCE_IntroduceInstance` (L579), `INSTANCE_BirthObject` (L1189),
`INSTANCE_ReallyRemoveInstance` (L319).
**Verificación:** comparar con la estructura de objetos de Gex PC (p. ej.
patrones de 6 punteros de función consecutivos, o desplazamientos coherentes en
los ficheros de nivel) tras identificar el modelo de datos en el exe.

### H-08. Actualización por turnos (time-slicing) de instancias lejanas — **[MOTOR]**

`INSTANCE_DeactivateFarInstances`
([L97](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/INSTANCE.c#L97))
procesa **1/8 de la lista por frame** con un índice estático (`lastInst`), usando
la línea cámara→instancia como criterio. Es un patrón de coste amortiguado
habitual del estudio.
**Verificación:** en Gex PC, buscar un contador estático análogo junto a
comparaciones de distancia a cámara.

### H-09. Colisión: grupos + primitivas + BSP + señales — **[MOTOR]/[PS1]**

`COLLIDE_InstanceList`
([L1706](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/COLLIDE.c#L1706))
recorre **grupos de atributos** (grupos 16..31 para el jugador; tablas
`dyna_clddyna/stat_clddyna/dyna_cldstat` de 8 entradas para el resto) y hace
pares dentro/entre grupos con rechazo por flags. Las primitivas: punto-en-
triángulo ([L43](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/COLLIDE.c#L43)),
esfera-vs-HBox (L1082), esfera-judador↔esfera-instancia (L1204), esfera-vs-
terreno (`COLLIDE_SAndT` L2017) y **BSP** para terreno. Las «señales»
(`COLLIDE_InstanceListWithSignals` L2702, `COLLIDE_CameraWithStreamSignals`
L2565) actúan como volúmenes de disparo/cámara ligados al nivel.
**Verificación:** los formatos de nivel de Gex PC (¿BSP? ¿clasificación de
caras?) y la existencia de «señales» como datos.

### H-10. Streaming: cola de carga con estados + `MEMPACK` con GC — **[MOTOR]**

`STREAM_PollLoadQueue`
([STRMLOAD.c L179](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/STRMLOAD.c#L179))
implementa una **cola de 40 entradas** con máquina de estados (leer CD → cerrar
→ reubicar binario → callback) y callbacks de finalización; se llama **una vez
por iteración** del bucle principal. `STREAM_InitLoader` (L37) fija
`BIGFILE.DAT`; `LOAD_*` encapsula CD (`.../reg:PSX`): carga no bloqueante,
direcciones, `LOAD_ChangeDirectory` y cola de XA (audio).
`MEMPACK_DoGarbageCollection`
([MEMPACK.c L496](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/MEMPACK.c#L496))
compacta memoria y **reubica** bloques por tipo (área/objeto/CD) ajustando
punteros internos con listas de reubicación.
**Verificación:** en Gex PC (también basado en CD): ¿existe una cola con
callbacks y un gestor de memoria con compactación? Buscar la tabla de
reubicación o funciones de «relocate» en el desensamblado.

### H-11. Overlays y enlazado dinámico en runtime — **[PS1]/[MOTOR]**

Formato de overlay: `LOAD_RelocBinaryData`
([LOAD3D.c L831](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/LOAD3D.c#L831))
lee una **tabla de redirección** (primera palabra = número de punteros, alineada
a 512) y llama a `RESOLVE_Pointers`
([RESOLVE.c L3](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/RESOLVE.c#L3)),
que suma la dirección base a cada puntero; después **compacta** el payload.
El caso de uso está en `CINE_Load`
([CINEPSX.c L55](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/CINEMA/CINEPSX.c#L55)):
carga el objeto `cinemax` (poll de la cola hasta 400 intentos), toma el punto de
entrada como **tabla de funciones** (`Object->relocModule`) y la invoca con
**comprobación de versión** (`versionID == monVersion`) — API versionada entre
ejecutable y overlay.
**Verificación:** en Gex PC, ¿hay módulos cargables? (DLLs ya conocidas del
entorno; y en el propio juego, recursos con tabla de saltos). Menor prioridad:
la tech es de CD/PS1.

### H-12. Cámara: despacho por modos + seguimiento amortiguado crítico — **[MOTOR]/[SR]**

`CAMERA_Process`
([L3371](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/CAMERA.c#L3371))
despacha por `camera->mode`: 0/12/13 → `CAMERA_GenericCameraProcess` (L2701,
seguimiento), 5 → cinematográfica (L2889), 2/4 → spline (L3230), 6 → look
(L1921), 8/15 → sin proceso. Antes: `CAMERA_HandleTransitions` (L2020),
cálculo de la velocidad del foco (`instance_xyvel`) y `CAMERA_ShakeCamera`
(L3352). El seguimiento usa **amortiguación crítica** por estado
(velocidad+aceleración) en posición y ángulo: `CriticalDampAngle`
([L1451](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/CAMERA.c#L1451)),
`CriticalDampValue` (INCLUDE_ASM L1447), `CriticalDampPosition` (INCLUDE_ASM
L1449). La oclusión usa esfera-esfera con líneas
(`CAMERA_SphereToSphereWithLines` L1556) y volumen de vista/plano cercano
(`CAMERA_SetViewVolume` L170, `CAMERA_CalcVVClipInfo` L151).
**Verificación:** comprobar en Gex PC la existencia de pares (valor, velocidad,
aceleración) por eje/ángulo en el estado de cámara, y una tabla de modos con
valores por defecto; comparar con nuestros hallazgos de M-13/temporización.

### H-13. Animación G2: secciones + controladores + compresión ADPCM — **[MOTOR]**

- `G2Anim_BuildTransforms`
  ([ANIMG2.c L239](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANIMG2.c#L239)):
  actualiza el «stored frame» de la sección, aplica root motion y compone la
  jerarquía con o sin controladores (`_G2Anim_BuildTransformsWithControllers`
  [ANMCTRLR.c L308](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANMCTRLR.c#L308)).
- **Controladores** = animación procedural superpuesta (interpolar cuaternión o
  vector durante una duración: `G2Anim_SetInterpController_Quat`
  [L272](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANMCTRLR.c#L272)),
  con lista de controladores por segmento.
- **Descompresión de canales con ADPCM**: `_G2Anim_DecompressChannel_AdaptiveDelta`
  ([ANMDECMP.c L3](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANMDECMP.c#L3))
  usa tablas de tamaño de paso e índice (esquema IMA/ADPCM clásico, deltas de
  4 bits) para reconstruir las claves del canal. Es un **algoritmo general**
  (no de plataforma).
- Root motion por rango de tiempo (`G2Anim_GetRootMotionFromTimeForDuration`
  [L461](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANIMG2.c#L461)).
- GTE: `hasm_segmatrixop` (inline asm con `gte_rtir12`) en `ANIMG2.h`
  ([L44+](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/include/Game/G2/ANIMG2.h#L44))
  — **[PS1]**, no trasladable.

**Verificación:** ¿los formatos de animación de Gex PC usan compresión
diferencial (delta/ADPCM) y cuaterniones? Es un punto de contraste **muy
concreto** para los ficheros de personaje (`*.GEX`/`*.VFX`/etc. del inventario).

### H-14. Scripting: splines, VM con reloj propio y señales — **[MOTOR]/[SR]**

- Movimiento/funciones guiados por **splines** por instancia
  (`SCRIPT_InstanceSplineProcess`
  [SCRIPT.c L608](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/SCRIPT.c#L608),
  `MultiSpline` con posición/rotación/escala).
- **Reloj de VM**: `VM_Tick(time)` acumula `vmRealClock += time; vmClock =
  vmRealClock >> 8` ([VM.c L454](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/VM.c#L454)),
  invocado con `256` por frame ⇒ +1 unidad de `vmClock` por frame; los objetos
  de VM por nivel se procesan con ese reloj.
- **Señales** por nivel despachadas a instancias/cámara
  (`SIGNAL_HandleSignal`
  [SIGNAL.c L570](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/SIGNAL.c#L570);
  hay una familia entera de señales de cámara: modo/lock/unlock/smooth/timer/
  save/restore/value).
- **Eventos** con temporizadores propios (`EVENT_ProcessTimers`
  [EVENT.c L232](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/EVENT.c#L232),
  `EVENT_ProcessEvents` [L675](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/EVENT.c#L675)).
- **[SR]** `VM_UpdateMorph` (paletas morph material/espectral) — mecánica de
  Soul Reaver, no trasladable.

**Verificación:** en Gex PC, buscar un reloj global decreciente/creciente ligado
a objetos de nivel y datos de spline en los ficheros de nivel.

### H-15. Sonido: biblioteca propia AAD + SPU/XA — **[PS1]** (abstracción [MOTOR])

`SOUND.c` gestiona **instancias de sonido por objeto** con tipos (periódico,
one-shot, evento), distancia/volumen y 3D
(`SOUND_ProcessInstanceSounds`
[L80](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/SOUND.c#L80),
`SOUND_Play3dSound` [L903](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/SOUND.c#L903)),
sobre la biblioteca interna **AAD** (`include/Game/PSX/AADLIB.h`: colas de
carga, bancos dinámicos, volumen maestro con cuadrado, paneo no lineal) y
**VOICEXA** para voces XA por CD (`VOICEXA_Play`
[L288](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/VOICEXA.c#L288)).
La implementación es PS1 (SPU/CD-XA); la **abstracción** (sonido por objeto,
disparadores, mezcla por distancia) probablemente tiene eco en PC (en nuestro
caso, el entorno DirectSound ya identificado).
**Verificación:** mapear el modelo de sonido de Gex PC (objetos con tabla de
sonidos, disparadores por animación/estado) contra este patrón.

### H-16. Overlays de servicio: vídeo (`cinemax`) y memory card (`mcardx`) — **[PS1]**

`src/Overlays/cinemax/cinemax.c` reproduce vídeo con `CdControl/CdlLOC/ClearImage`
y una tabla de funciones de entrada; `mcardx` cubre la memory card (50 de las
232 funciones sin emparejar, en `Overlays/mcardx`). Sin equivalente directo en
PC (donde el vídeo es AVI/… y el guardado es otra cosa), pero ilustran el patrón
«módulo de servicio con API versionada» (H-11).

### H-17. Especificidades PS1 a NO trasladar — **[PS1]**

- **GTE** en línea (`hasm_segmatrixop` en `ANIMG2.h`; `SetRotMatrix`/
  `SetTransMatrix`/`ApplyMatrixSV` en PIPE3D/CAMERA; `gte_*` en
  `include/common.h`/`include/gte_macros.s`).
- **Contador RCNT** de 33.8688 MHz y su conversión a ms (`TIMER.c`
  [L7-72](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/TIMER.c#L7),
  constante 33869 ≈ ticks/ms).
- **Scratchpad** de 1 KB (`getScratchAddr(0/1)` usado como variables rápidas en
  el bucle).
- **Ordering tables / PrimPool / `ClearOTagR` / `DrawOTag`** (PIPE3D/DRAW/
  GAMELOOP): el «presupuesto de primitivas» de PS1 no existe igual en Glide/PC.
- **CD-ROM y XA** (CdlLOC, CdControl, streaming de audio), **SPU** (AAD).
- **VSync/PutDispEnv/DrawSync** y callbacks de GPU.

### H-18. Evidencia del linaje «Gex 2 engine» — **[IND] → probable**

El propio README etiqueta `SLUS_007.08` como **«Gex 2 engine + game code»**
(repositorio de terceros; afirmación **suya**, no verificada por nosotros). En el
árbol hay señales internas coherentes con un motor con nombre propio «G2»:
punto de entrada `MainG2` ([MAIN.c L652](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PSX/MAIN.c#L652)),
módulos `G2/MAING2.c`, `ANIMG2.c`, `INSTNCG2.c`, `TIMERG2.c`, `QUATG2.c`,
`POOLMMG2.c`, y preposiciones `G2Anim_*`/`G2Instance_*` por todo el código; el
directorio de módulos PS1 se llama `PSX_G2`. No se ha encontrado en el árbol
ninguna otra expansión documentada de «G2» ni ninguna otra referencia a «Gex»
(solo el README). **Conclusión:** la hipótesis «mismo linaje de motor que Gex 2 /
Gex PC» es plausible y consistente con la nomenclatura, pero **no está
demostrada** por este análisis; nuestra verificación real es el contraste con
`GEX3D.EXE` (§8).

---

## 5. Qué aplica y qué no (resumen por prioridad)

| Prioridad | Hallazgo | Aplicabilidad a Gex PC | Confianza | Método de verificación |
|---|---|---|---|---|
| 1 | H-01 modelo de tiempo (`timeMult`, lock 30/20, modo 24) | Alta (explicaría 25 FPS) | Media (hipótesis) | Constantes y rutinas de tiempo en el exe; medición de swaps con el shim |
| 2 | H-02 punto fijo 12 bits | Alta | Media | Búsqueda de 4096/`>>12` junto a física/tiempo |
| 3 | H-05 física Euler + clamps | Alta | Media | Rutina equivalente en el exe (ratios 4096/8192) |
| 4 | H-06 orden de frame | Media-alta | Media | Etiquetado del bucle principal en Ghidra (Fase 2) |
| 5 | H-07 modelo de objetos/mensajería | Media | Baja-media | Estructura de objetos y vtables en el exe/datos |
| 6 | H-12 cámara (modos + damping) | Media | Baja-media | Estado de cámara (pares vel/accl) y modos |
| 7 | H-10 streaming + GC | Media | Baja | Cola de carga/compactación en el exe |
| 8 | H-13 animación (controladores, ADPCM) | Media | Baja | Formato de animación de Gex PC |
| 9 | H-14 splines/VM/señales | Media-baja | Baja | Datos de nivel y reloj global |
| 10 | H-15 sonido (abstracción) | Media-baja | Baja | Modelo de sonido PC (DSound) |
| — | H-03/H-04/H-09/H-11/H-16/H-17 específicos de PS1 | Nula o testimonial | Alta | No aplica (documentado para no confundir) |

---

## 6. Riesgos y limitaciones

- **Divergencia de plataforma:** PS1 (MIPS R3000A, GTE, GPU de 1 MB, CD 2×) vs
  PC (x86, Glide, disco duro). Nada de `PIPE3D`/`DRAW`/`G2` puede portarse
  literalmente.
- **Divergencia temporal:** Gex PC (1998-05, build EU) precede a Soul Reaver PS1
  (prototipo ~1999). El motor pudo evolucionar entre ambos; proporciones y
  constantes pueden diferir.
- **Prototipo vs release:** el proyecto usa un prototipo con símbolos; puede
  contener código ausente o alterado respecto al release (y
  `docs/INSTALL.MD` lo asume explícitamente).
- **Símbolos ≠ nuestro binario:** Gex PC está sin símbolos (retail). Cualquier
  correspondencia de nombres es orientativa.
- **Métrica ≠ comprensión:** el 85 % es emparejamiento de bytes; y el árbol
  contiene 232 funciones en ensamblador (mayoría en overlays).
- **Estado de la métrica:** aviso «This project is disabled.» observado en la
  página agregada de decomp.dev (2026-10-11); las cifras deben citarse como
  «último dato publicado para `59d1ee8`». Recuento exacto de
  `matched_functions` no recuperado en esta sesión (dato pendiente).
- **Un solo commit:** todo lo anterior pertenece al commit `59d1ee8`; no se ha
  revisado historia ni PRs.
- **Sin ejecución:** nada de esto demuestra comportamiento en runtime, ni en
  Soul Reaver ni en Gex.
- **Licencia:** cubre el **código de la reconstrucción** (MIT); no concede
  derechos sobre el juego original ni sus recursos. Ver §9.

---

## 7. Próximas investigaciones recomendadas (fácil → compleja)

1. **(Fácil, estático, sin ejecutar nada)** Cribar `GEX3D.EXE` por constantes de
   tiempo plausibles (33/40/50/66 ms; 4096/4097; 0x1000; `timeGetTime`/
   `QueryPerformanceCounter` en la IAT) y por patrones `>>12`/`<<12`.
2. **(Fácil, ya previsto)** Usar la **tanda instrumentada de 60 s** (shim H4a,
   pendiente de ejecución del mantenedor) para medir los intervalos reales entre
   `grBufferSwap` y su varianza: ¿cadencia fija? ¿batching? ¿jitter? Es la
   prueba directa contra H-01/H-03.
3. **(Media)** Comparar la lista de imports/subsistemas de Gex PC con el mapa de
   §3 (¿hay evidencias de streaming, de splines, de controladores?) mediante
   análisis estático del binario y de los datos.
4. **(Media)** Contraste de formato de animación: buscar en los activos de
   personaje de Gex PC indicios de compresión diferencial tipo ADPCM (H-13).
5. **(Compleja, Fase 2)** Con Ghidra sobre el exe: localizar las rutinas de
   tiempo/física/cámara y compararlas función a función con los algoritmos de
   este informe (plantilla de H-06 para el bucle).
6. **(Compleja)** Verificar H-18 (linaje) de forma independiente: comparar
   firmas de código/estructuras entre `SLUS_007.08` y `GEX3D.EXE` más allá de la
   nomenclatura.

Ninguna de estas acciones modifica el shim ni el juego: son lecturas, mediciones
con instrumentación ya existente y comparaciones documentales.

---

## 8. Licencia y atribución

- **Proyecto referenciado:** `fmil95/soul-re` — *Soul Reaverse*.
  Licencia **MIT**, © 2024-2026 **Federico Milesi** (`fmil95`) & **Stable
  Crimson**. Texto: [`LICENSE`](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/LICENSE).
  Si en el futuro se reutilizara código (no es el caso: este informe solo
  **resume** algoritmos), habría que conservar el aviso de copyright y la
  licencia, y anotarlo en el commit correspondiente.
- Herramientas del ecosistema de decompilación usadas por ese proyecto
  (`splat`, `objdiff`, `maspsx`, `m2c`, `decomp-permuter`, `cd-dat-utils`)
  pertenecen a sus respectivos autores y no se han usado aquí.
- Nota de **no afiliación**: Crystal Dynamics, Eidos, Square Enix y las marcas
  de los juegos pertenecen a sus titulares (coherente con `LICENSE.md` del
  proyecto). Este documento no redistribuye ningún contenido del juego ni del
  repositorio externo.

---

## 9. Qué NO se debe copiar ni asumir

1. **No copiar código** del repositorio externo a nuestro proyecto (ni siquiera
   fragmentos «pequeños»): este informe resume; no traslada. Si alguna vez se
   citara algo >2 líneas, se hará con atribución MIT y revisión explícita.
2. **No asumir** que los **offsets/campos** del struct `Instance`/`GameTracker` o
   los layouts de datos de PS1 valgan en la versión PC.
3. **No asumir** las **constantes** (33/50/66 ms, 4096/4097, tablas de tamaño de
   paso, 33869 ticks/ms, `frameRateLock` 1/2): son del build PS1 analizado.
4. **No asumir** que Gex PC tenga los sistemas de Soul Reaver: morph
   material/espectral, glifos, warpgates, planificador de enemigos, AAD/SPU/XA.
5. **No asumir equivalencia 1:1 de nombres** de funciones/ficheros con el exe
   PC (los símbolos proceden de un prototipo distinto).
6. **No extrapolar comportamiento** (FPS, pacing, física, render) sin medirlo en
   el PC: la jerarquía de evidencia manda (exe EU original primero).
7. **No copiar formatos** de datos PS1 (`.drm`, `BIGFILE.DAT`, XA, OT/PrimPool)
   ni sus rutinas de reubicación como si aplicaran al PC.
8. **No usar este informe** como justificación para cambiar el shim Glide ni
   para tocar el ejecutable/DLL originales; el shim solo cambia con evidencia
   propia (política del proyecto).
9. **No redistribuir** binarios, ROMs, imágenes de disco ni capturas del juego;
   este documento no añade ninguno al repositorio.

---

### Anexo A. Enlaces permanentes usados (commit `59d1ee8`)

- Bucle principal y tiempo: `GAMELOOP.c`
  [L1012](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1012)
  (flip), [L1124](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1124)
  (DisplayFrame), [L1380](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1380)
  (sincronía), [L1573](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1573)
  (24 fps), [L1583](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1583)
  (DoTimeProcess), [L1682](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L1682)
  (Process), [L2147](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/GAMELOOP.c#L2147)
  (PSX_GameLoop).
- Entrada y VBlank: `PSX/MAIN.c`
  [L307](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PSX/MAIN.c#L307)
  (StartTimer), [L324](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PSX/MAIN.c#L324)
  (VblTick), [L344](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PSX/MAIN.c#L344)
  (DrawCallback), [L652](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PSX/MAIN.c#L652)
  (MainG2).
- Tiempo: `TIMER.c`
  [L7](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/TIMER.c#L7)
  (GetTimeMS), [L22](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/TIMER.c#L22)
  (TimeDiff); `G2/TIMERG2.c`
  [L6](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/TIMERG2.c#L6)
  (GetFrameTime).
- Física/colisión: `PHYSICS.c`
  [L1778](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PHYSICS.c#L1778);
  `COLLIDE.c`
  [L1706](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/COLLIDE.c#L1706).
- Objetos: `INSTANCE.c`
  [L97](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/INSTANCE.c#L97),
  [L894](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/INSTANCE.c#L894),
  [L2029](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/INSTANCE.c#L2029);
  `TYPES.h`
  [L2467](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/include/Game/TYPES.h#L2467).
- Streaming/overlays: `STRMLOAD.c`
  [L179](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/STRMLOAD.c#L179);
  `LOAD3D.c`
  [L831](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/LOAD3D.c#L831);
  `RESOLVE.c`
  [L3](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/RESOLVE.c#L3);
  `MEMPACK.c`
  [L496](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/MEMPACK.c#L496);
  `CINEMA/CINEPSX.c`
  [L55](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/CINEMA/CINEPSX.c#L55).
- Cámara/animación/scripting: `CAMERA.c`
  [L3371](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/CAMERA.c#L3371);
  `G2/ANIMG2.c`
  [L239](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANIMG2.c#L239);
  `G2/ANMCTRLR.c`
  [L272](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANMCTRLR.c#L272);
  `G2/ANMDECMP.c`
  [L3](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/ANMDECMP.c#L3);
  `G2/INSTNCG2.c`
  [L9](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/G2/INSTNCG2.c#L9);
  `SCRIPT.c`
  [L608](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/SCRIPT.c#L608);
  `VM.c`
  [L454](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/VM.c#L454);
  `SIGNAL.c`
  [L570](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/SIGNAL.c#L570);
  `EVENT.c`
  [L232](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/EVENT.c#L232).
- Render/sonido: `PIPE3D.c`
  [L34](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/PIPE3D.c#L34);
  `DRAW.c`
  [L47](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/DRAW.c#L47);
  `SOUND.c`
  [L80](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/SOUND.c#L80);
  `VOICEXA.c`
  [L288](https://github.com/fmil95/soul-re/blob/59d1ee87749da95b6ced413a9f60b30a6fb91d45/src/Game/VOICEXA.c#L288).
