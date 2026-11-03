# Investigación y diseño del renderer nativo (I-5)

> Estado: **investigación y diseño completados** el 2026-11-02. Este hito **no
> implementa** renderer real: no hay dispositivo gráfico, no se dibuja nada, no
> se toca el sistema de construcción ni se añaden dependencias. El resultado es
> una decisión técnica fundamentada: el renderer del port se construirá sobre
> **Direct3D 11** (API del sistema, sin descargas), detrás de un **contrato
> C11 agnóstico de la API gráfica** que conserva `gex_null_renderer` como
> implementación de prueba. La verificación final de la recomendación (crear
> un dispositivo) es el primer paso del hito de implementación (§7, R-1).
>
> **Actualización 2026-11-03:** R-1 (dispositivo D3D11, hardware y WARP) y R-2
> (swapchain DXGI sobre el `HWND` de I-4) están ejecutados; el resto de §7 sigue
> pendiente. El diseño de este documento no cambió: R-1 confirmó la elección y
> R-2 implementó la presentación **sin** `Present` ni clear. Ver
> `docs/D3D11_DEVICE.md` y `docs/D3D11_SWAPCHAIN.md`.
>
> Base de evidencia: `docs/FRAME_STEP_TRACE.md` (traza dinámica),
> `docs/SUBSYSTEM_SEGMENTS.md` §S2/S4, `docs/TIMING_GAME_LOOP_ANALYSIS.md` §2,
> `docs/F03_VS_ORIGINAL.md` §4, `docs/ORIGINAL_ARTIFACT_INVENTORY.md` §4.1,
> `docs/BYTEMATCH.md` §5, `docs/M13_DECOUPLE_PROPOSAL.md` §2/§9,
> `docs/FRAME_LOOP_PORT.md` §11 y `tools/glide-shim/gshim.def`.
> REA **no** está operativo para GEX y no se usó para ninguna afirmación de
> este documento (ver §1.2).

## 1. Alcance y fuentes

### 1.1 Qué se ha hecho

1. Lectura de toda la evidencia de renderizado ya documentada en el repo
   (estática y dinámica), sin repetir el análisis del binario: este hito
   consolida y cruza lo medido, no vuelve a desensamblar.
2. Censo exacto de la superficie Glide que usa el ejecutable (las 38
   importaciones, verificadas estáticamente por el proyecto en 2026-10-10 y
   ejercidas en ejecución real por `tools/glide-shim` en 2026-10-11).
3. Evaluación de alternativas de API gráfica contra criterios explícitos del
   proyecto (§5), con verificación de factibilidad de toolchain hecha **hoy**
   (cabeceras y librerías presentes, §5.4) — solo comprobación estática de
   disponibilidad, **no** se compiló ni ejecutó ningún programa D3D11/OpenGL.
4. Diseño del contrato del renderer y del plan de validación incremental,
   sin añadir código de producción.

### 1.2 Qué NO se ha hecho

- No se ejecutó REA (no está sano en este entorno y ningún binario de GEX ha
  sido analizado con él: `docs/REA_WINDOWS_ENABLEMENT.md`, `RESEARCH.md`
  S-18). Ninguna afirmación de aquí depende de REA.
- No se desensambló nada nuevo; no se crearon funciones nuevas de S2.
- No se probó ninguna API gráfica en ejecución: D3D11/OpenGL se evaluaron por
  documentación, disponibilidad de toolchain y arquitectura; lo medido por el
  proyecto hasta hoy sobre render en ejecución real es **el original bajo
  nGlide**, no una API moderna.
- No se tocaron `loop_core.c`, `frame.c`, el planificador de audio, I-4, el
  `Makefile` ni `build.ps1`.

## 2. Evidencia del renderizado original

Etiquetas: **CONFIRMADO** = demostrado por código/traza/imports verificados
por el proyecto. **INFERIDO** = conclusión reforzada por varios hechos, sin
prueba directa. **HIPÓTESIS** = pendiente de comprobar. Las direcciones son
del ejecutable EU v1.00.000 (`GEX3D.EXE`, MD5 `692b1282…`).

### 2.1 El original renderiza exclusivamente con Glide (CONFIRMADO)

Análisis PE estático (`docs/ORIGINAL_ARTIFACT_INVENTORY.md` §4.1,
`docs/BYTEMATCH.md` §5): `GEX3D.EXE` importa **38 funciones de
`glide2x.dll`** y **cero** funciones de DirectDraw/Direct3D. Cadenas del
binario: `This game requires Glide version 2.1 or higher.`,
`Error: Unable to load glide2x.dll`, `Voodoo`, `Voodoo2`. El juego no carga
`glide3x.dll` ni `glide.dll` (solo 2.x). `FONT.3DF` es una textura Glide
`.3df` (`3df v1.1`, `rgb332`, LOD 128×128, aspect 4:1) — corrobora la
plataforma gráfica.

Las 38 importaciones (fuente: `tools/glide-shim/gshim.def`, aridades
cotejadas contra los headers del SDK Glide 2.x; `FX_CALL=__stdcall`):

| Familia | Imports | Funciones |
| --- | ---: | --- |
| Inicialización y estado global | 6 | `grGlideInit@0`, `grGlideShutdown@0`, `grSstQueryHardware@4`, `grSstSelect@4`, `grSstWinOpen@28`, `grSstWinClose@0` |
| Estado de render | 4 | `grGlideGetState@4`, `grGlideSetState@4`, `grSstControl@4`, `grFogMode@4` |
| Presentación y cola | 2 | `grBufferSwap@4`, `grBufferNumPending@0` |
| Geometría | 3 | `grDrawTriangle@12`, `grDrawLine@8`, `grDrawPlanarPolygonVertexList@8` |
| Color/alpha/chromakey/culling/depth | 9 | `guColorCombineFunction@4`, `grConstantColorValue4@16`, `grAlphaCombine@20`, `grAlphaBlendFunction@16`, `grChromakeyMode@4`, `grChromakeyValue@4`, `grCullMode@4`, `grDepthBufferMode@4`, `grDepthMask@4` |
| Texturas | 11 | `grTexSource@16`, `grTexDownloadMipMap@16`, `grTexDownloadMipMapLevelPartial@40`, `grTexDownloadTable@12`, `grTexMipMapMode@12`, `grTexFilterMode@12`, `grTexCombineFunction@8`, `grTexNCCTable@8`, `grTexMaxAddress@4`, `grTexMinAddress@4`, `grTexTextureMemRequired@8` |
| Ficheros `.3df` | 2 | `gu3dfGetInfo@8`, `gu3dfLoad@8` |
| Framebuffer lineal (2D) | 1 | `grLfbWriteRegion@32` |

Esta superficie es **la especificación de facto** del renderer del original:
todo lo que el port tenga que reproducir visualmente pasa por estas llamadas
y por su semántica Glide 2.x.

### 2.2 Quién llama a Glide y qué se sabe de cada sitio (S2, §S4)

`docs/SUBSYSTEM_SEGMENTS.md` §S2 agrupa las 10 funciones de juego con
importaciones Glide directas (todas con confianza ALTA por evidencia de
imports+grafo; el *papel de juego* de cada una sigue siendo hipótesis):

| Dirección | Tamaño | Llamadores | Imports Glide observados / papel |
| --- | ---: | ---: | --- |
| `0x0043DC3A` | 1 408 | 2 | **present del frame loop**: instala callbacks de render, envía 2 listas y drena la cola (`grBufferNumPending`) (`TIMING_GAME_LOOP_ANALYSIS.md` §2) |
| `0x0046558A` | 17 068 | 1 | mayor consumidor Glide (10 APIs): `grDrawTriangle`, `grTexSource`, `grAlphaCombine`, `grConstantColorValue4`, `grChromakeyMode`, `grDrawLine`, `grDrawPlanarPolygonVertexList`, `grTexDownloadTable`, entre otras |
| `0x00461C32` | 473 | 1 | descarga de mipmaps de textura |
| `0x004329EF` | 462 | 1 | carga de textura `.3df` (`gu3dfGetInfo`/`gu3dfLoad`) |
| `0x00432BBD` | 532 | 1 | triángulos + estado de color/alpha |
| `0x00432DD1` | 264 | 1 | triángulos + estado de color/alpha (variante corta) |
| `0x00424C0F` | 121 | 2 | `grBufferSwap(1)` de la ruta de arranque |
| `0x00424D82` | 648 | 1 | `grSstQueryHardware`: sonda de hardware Glide |
| `0x00473238` | 504 | 1 | descarga de mipmap y de nivel parcial |
| `0x00469836` | 122 | 3 | `grTexNCCTable`: tabla de paleta |

Ruta de cine (S4, también ALTA): `0x00537EA6` hace `grSstWinOpen`
**640×480 @ 60 Hz** para película, `0x00537D70` vuelca vídeo con
`grLfbWriteRegion`, `0x00537E69` hace `grBufferSwap(1)` y `0x00537ED1`
guarda/restaura estado con `grGlideGetState`/`grGlideSetState`.

### 2.3 Inicialización, resolución y buffers (CONFIRMADO por desensamblado del repo)

`0x0046324D` llama a `grSstWinOpen` con (desensamblado de pushes + enums
`sst1vid.h`; `docs/TIMING_GAME_LOOP_ANALYSIS.md` §2,
`docs/F03_VS_ORIGINAL.md` §4):

- **Juego: 512×384, 75 Hz, 2 buffers de color, 0 auxiliares.**
- **Cine: 640×480, 60 Hz** (vía `0x00537EA6`).
- Enums de referencia (`sezero/glide` G-01, `sst1vid.h`): 75 Hz = `0x3`,
  60 Hz = `0x0`, 512×384 = `0x3`, 640×480 = `0x7`.

La sonda `grSstQueryHardware` (`0x00424D82`) y el swap(1) de arranque
(`0x00424C0F`) muestran que la ventana de juego se abre **después** de
verificar hardware y de una presentación de arranque más rápida. La **tasa
real** de refresco en runtime no está confirmada: el wrapper moderno escala
los modos (§4).

### 2.4 Presentación y sincronización (CONFIRMADO por traza dinámica)

Secuencia medida en 2 326 frames (`docs/FRAME_STEP_TRACE.md` §3.1):

```text
0x0043EA36 frame step
├── 0x0043F80D  entrada
├── 0x0043E1BA  update
│     └── 0x0043DC3A  render (solo si WORD [obj+0x4E38] == 0)
│           └── 0x0054053E  grBufferNumPending (drenaje dentro del render)
├── 0x0054053E  grBufferNumPending (bucle 0x0043EA51: mientras != 0)
├── 0x005404F6  grBufferSwap(3)
└── 0x0046300C  hoja de 5 B (rol sin identificar)
```

Hechos medidos:

- El render es **condicional** (`0x0043E5B3`); en toda la muestra
  `[obj+0x4E38] == 0` y el render se ejecutó siempre, pero la rama existe.
- El **drenaje ocurre dos veces por frame**: dentro del render y en el frame
  step. El bucle gira hasta que la cola de la tarjeta queda vacía.
- `grBufferSwap` se llama con **arg 3** (3 retraces): a 75 Hz pedidos, techo
  de ~25 FPS. El **swap bloquea ~23 ms** de los 40 del frame (p50 = 40 ms
  sobre 1 723 intervalos; mínimo 37 ms).
- Semántica del swap (`sezero/glide` G-01, `gglide.c`, confianza alta):
  `grBufferSwap` **encola y retorna**; el freno real es el spin-a-cero del
  frame siguiente (`grBufferNumPending` lee un campo de 3 bits del hardware).
  Es decir: el original es *present-bound* porque su bucle espera la cola,
  no porque el swap devuelva sincrónico.
- El original NO tiene pausa del bucle, NO drena con una API de sistema y NO
  usa temporizadores multimedia para el pacing (superficie timing E-3: solo
  `Sleep` + `GetTickCount`).

### 2.5 Geometría, estado y transformación

- Glide 2.x **no tiene matriz de proyección ni pipeline programable**: la API
  recibe triángulos en coordenadas de pantalla con `w` por vértice
  (perspectiva correcta) y rasaca en hardware. El juego, por tanto, hace la
  transformación **en software** y envía triángulos ya proyectados vía
  `grDrawTriangle`/`grDrawLine`/`grDrawPlanarPolygonVertexList` (hecho de la
  API Glide 2.x, CONFIRMADO como semántica del SDK; que el juego transforma
  en CPU es INFERIDO de no existir imports de transformación: Glide no ofrece
  ninguna).
- El **formato del vértice** (`GrVertex` del SDK) está fijado por la API, con
  `x,y,z,w`, `r,g,b,a`, `s,o,t` y `sow,tow`. Qué campos rellena el juego en
  cada ruta y cómo calcula `w` es **UNKNOWN** (lectura guiada pendiente).
- Estado combinable observado en imports: color combine
  (`guColorCombineFunction`), alpha combine (`grAlphaCombine`), blending
  (`grAlphaBlendFunction`), chromakey, culling, depth mode/mask, niebla
  (`grFogMode`), color constante (`grConstantColorValue4`). El mapeo concreto
  de modos que usa el juego es **UNKNOWN**; el imports-lista solo prueba que
  el juego *puede* configurarlos.
- `0x0043DC3A` "instala callbacks de render y envía 2 listas"
  (`TIMING_GAME_LOOP_ANALYSIS.md` §2): hay una cola de render interna del
  juego (probablemente listas de vértices/estado por frame) **no mapeada
  todavía**. Su estructura es UNKNOWN.

### 2.6 Texturas y memoria gráfica

- Carga desde fichero: `gu3dfGetInfo`/`gu3dfLoad` (`0x004329EF`) — el juego
  usa el formato contenedor `.3df` de 3dfx (cabecera `3df v1.1` verificada en
  `FONT.3DF`: `rgb332`, LOD 128×128, aspect 4:1).
- Descarga a la tarjeta: `grTexDownloadMipMap`,
  `grTexDownloadMipMapLevelPartial` (`0x00473238`: descarga parcial de nivel,
  sugiere streaming/actualización de texturas en memoria TMU),
  `grTexDownloadTable` + `grTexNCCTable` (tablas de paleta NCC/YAB).
- Consultas de memoria: `grTexMaxAddress`/`grTexMinAddress`/
  `grTexTextureMemRequired` — el juego gestiona la memoria de textura
  explícitamente, como era normal con 2–4 MB de TMU.
- Formatos de píxel usados por el juego: UNKNOWN salvo `rgb332` para la
  fuente. La presencia de `grTexNCCTable` demuestra uso de **texturas 8-bit
  con paleta NCC** en alguna ruta.
- `GEX.VFX`/`GEX.DFX` (rampas tipo gamma, mapas) siguen UNKNOWN-PENDING
  (`ORIGINAL_ARTIFACT_INVENTORY.md` §4.2); su relación con el render (gamma,
  paletas, geometría de niveles) no está establecida.

### 2.7 Relación con el frame loop del port (I-3/I-4)

El hook `present` de `gex_frame` reproduce el **papel** de `0x0043DC3A`
(render) + drenaje + `grBufferSwap`: un callback que, si falla, aborta el
frame y vacía el acumulador (política de errores de I-3,
`GEX_FRAME_ERR_PRESENT`). El renderer real debe caber ahí **sin cambiar
`frame.c`**: recibe `gex_frame_tick` (muestra de reloj, paso, índice de
frame, pasos consumidos) y devuelve 0/1 igual que hoy `gex_null_renderer`.
El drenaje-a-cero del original **no se replica**: en D3D11 la cola la gestiona
el runtime (§5.1, D-3), y modelarla sería simular un artefacto de hardware
que el port no tiene.

## 3. El original en hardware moderno (contexto del wrapper)

- En el PC mantenedor el original corre bajo **nGlide 2.10** (wrapper
  freeware **cerrado**, Glide→D3D; CONFIRMADO por payload-hash en la 9ª
  sesión de Hito 1). La `glide2x.dll` pre-nGlide de circulación en `SysWOW64`
  era la de 3dfx «Glide for Voodoo Banshee» 2.61 (S-21/S-22); nGlide instala
  su propia DLL en el directorio del juego.
- El A/B con `tools/glide-shim` (proxy glide2x de instrumentación, DLL real
  renombrada, sin alterar llamadas) midió 8 318 swaps en ~2 min del original
  y 7 363 en F-03, con QPC crudo: es la **primera evidencia dinámica del
  pacing de presentación** del juego y demuestra, además, que una
  `glide2x.dll` propia de 32 bits es factible de construir con MinGW (el shim
  se construyó y ejecutó en el PC mantenedor, F-3 H4a 38/38 exports).
- El juego familia también existió como **build NTSC-D3D** (F-02, renderer
  Direct3D): prueba que el motor no estaba casado con Glide, pero **no** es
  nuestra base ni comparte ejecutable (binarios de orígenes distintos,
  CONFIRMADO por diff estático).
- Consecuencia práctica: la referencia visual «correcta» que todos conocemos
  hoy **es el original bajo un wrapper D3D**, no Glide sobre Voodoo real. El
  port no puede (ni debe) afirmar equivalencia con el Voodoo de época; sí
  puede apuntar a la semántica de las llamadas Glide del ejecutable EU.

## 4. Requisitos del renderer del port (derivados de la evidencia)

1. **Pipeline:** triángulos en espacio de pantalla con `w` por vértice,
   texturas 8-bit con paleta (NCC) y formatos de 16/8 bits, modos de
   combine color/alpha, chromakey, niebla, depth (w-buffer en el original;
   el modo exacto del juego UNKNOWN).
2. **Superficie:** 512×384 nativo (juego) y 640×480 (cine), escalados a la
   ventana del port con filtro a decidir (R-3).
3. **Presentación:** modelo de cola/cadencia controlable; en I-3/I-4 la
   cadencia la pone el bucle del port, no el swap. `SyncInterval` es el
   análogo directo de los retraces de `grBufferSwap`.
4. **Pruebas sin assets del juego:** fixtures sintéticos de `.3df` (formato
   cabecera ya documentado), colores uniformes, triángulos de prueba; nunca
   `FONT.3DF` ni `.DFX/.VFX` reales en el repo.
5. **Aislamiento:** el núcleo (I-3) y la app (I-4) no pueden ver la API
   gráfica; solo el backend la toca. El renderer nulo debe seguir usando las
   pruebas existentes (117 checks de `test_app` no pueden romperse).

## 5. Evaluación de alternativas

Criterios: (C1) compatibilidad con Windows moderno; (C2) complejidad de
implementación/mantenimiento; (C3) dependencias externas y distribución;
(C4) capacidad de reproducir la semántica del original; (C5) facilidad de
probar sin assets ni GPU dedicada; (C6) mantener el núcleo independiente de
la API; (C7) riesgo de diferencias visuales/temporales.

### 5.1 Opción A — Win32 + Direct3D 11 (RECOMENDADA)

- **C1:** `d3d11.dll`/`dxgi.dll` vienen con Windows 7+; es la API gráfica de
  sistema vigente.
- **C2:** COM desde C11 ya es práctica del repo (`backend_dsound.c` usa
  `IDirectSoundBuffer`): el patrón está probado. Los estados fijos de Glide
  (combine/alpha/chromakey/fog) se expresan con shaders pequeños y constantes
  de constant-buffer; no hay un mapeo 1:1 por API, pero sí uno 1:1 por
  *semántica* documentada en los imports (§2.1).
- **C3:** **cero descargas.** Verificado hoy en este PC: `d3d11.h`, `dxgi.h`,
  `d3dcommon.h` y `libd3d11.a`/`libdxgi.a` están presentes en **ambos**
  toolchains del proyecto (llvm-mingw de retcomm y MSYS2 mingw64), igual que
  `dsound.h` lo estaba para el audio.
- **C4:** feature level 9_1+ cubre el conjunto de características de la era
  fixed-function; los modos NCC/paleta se implementan con tablas en constant
  buffers o texturas de paleta; w-buffer se aproxima con depth normal (el
  modo exacto del original es UNKNOWN de todos modos).
- **C5:** **WARP** (rasterizador software incluido en Windows, sin GPU) da
  pruebas deterministas de dispositivo/limpiado/presentación en cualquier
  máquina y CI; el *debug layer* valida errores de uso de API.
- **C6:** el contrato propuesto (§6) es agnóstico: `frame.c` e `i4_app.c` no
  ven `d3d11.h`.
- **C7:** riesgo temporal bajo: la cadencia la sigue poniendo el bucle
  (`MsgWaitForMultipleObjectsEx`), no el Present; el modelo flip/bitblt y
  `SyncInterval` se fijan en R-2/R-4 con medición, no por defecto.

Riesgo principal: el manejo de *device-lost/removed* y la política de
redimensionado de swapchain son código nuevo con más casos que DirectSound
(se planifican en §6.6 y R-6).

### 5.2 Opción B — Win32 + OpenGL

- **C1:** `opengl32.dll` del sistema; `gl/gl.h`+`gl/glext.h` presentes en los
  dos toolchains (verificado hoy) y `libopengl32.a` en MSYS2 y retcomm.
- **C2/C4:** el perfil *compatibility* 2.1 mapea casi 1:1 al modelo Glide
  (inmediato, texturas con paleta vía fragmentos, blending), pero está en
  desuso y los drivers modernos lo degradan; el *core profile* (3.3+)
  exige el mismo trabajo de shaders que D3D11 **más** la carga de funciones
  por `wglGetProcAddress` (los headers de MinGW solo llegan a GL 1.1) y el
  manejo de contextos de WGL.
- **C5:** sin rasterizador software del sistema equivalente a WARP; probar
  sin GPU dedicada requeriría vendorizar Mesa/llvmpipe (dependencia externa,
  contra C3).
- **C6/C7:** mismos aislamiento y riesgo temporal que A.
- Conclusión: **viable pero inferior a A en C5 y con más boilerplate en C2**;
  se conserva como alternativa real porque el contrato de §6 no la excluye.

### 5.3 Opción C — capa de compatibilidad Glide

Dos formas muy distintas, y la evidencia las separa:

1. **Implementar nuestra propia `glide2x.dll`** (como hace el shim, pero
   renderizando en vez de reenviar). Solo tiene sentido para **reutilizar el
   binario original**, cosa que este port **no hace** por diseño (C11 limpio,
   sin redistribuir código del juego). Como objetivo de *fidelidad de
   comparación* podría resucitarse algún día, pero no es el camino del port
   nativo y duplicaría el renderer en un segundo formato.
2. **Dependencias existentes:** nGlide (freeware, cerrado, requiere
   instalación por el usuario), dgVoodoo2 (freeware, cerrado,
   Glide/DD/D3D→D3D11/12) y **OpenGLide/OpenGlide** (LGPL-2.0, estado Beta,
   última actividad 2020-04-23, SourceForge; CVS movido a GitHub). Todas
   violan la regla del proyecto de **cero dependencias que descargar**
   (o la desaconsejan: un envoltorio beta sin mantenimiento) y añaden un
   DLL externo a la distribución. El original ya corre con nGlide para el
   A/B del mantenedor; no hace falta que el port dependa de él.

Veredicto: **descartada como base**; documentada como ruta condicionada si
alguna vez se prioriza comparación binaria con el exe original.

### 5.4 Opción D — rasterizador software propio (C11 puro)

- Sin dependencias, determinista al 100 % y con fidelidad semántica total
  (podría implementar NCC/chromakey/fog tal cual Glide). A 512×384 (0,2 MP)
  el coste por frame es asumible en CPU moderna.
- Pero duplica el trabajo que la GPU da gratis, sacrifica aceleración para
  resoluciones mayores, y su salida igualmente debe presentarse a la ventana
  (GDI/`StretchBlt` o textura D3D11), sumando la complejidad de ambos
  caminos.
- Veredicto: **plan B documentado**. Si R-1 (crear dispositivo D3D11) o el
  soporte de feature level 9_1 fallara en máquinas objetivo, el rasterizador
  C11 sobre la misma ventana es la salida de emergencia con el contrato de
  §6 intacto.

### 5.5 Comparativa

| Criterio | A: D3D 11 | B: OpenGL | C: capa Glide | D: software propio |
| --- | --- | --- | --- | --- |
| C1 Windows moderno | sí (sistema) | sí (sistema) | depende de DLL externa | sí |
| C2 complejidad | media (COM ya probado) | media-alta (ext. loader) | alta (o nula con externa) | alta |
| C3 dependencias | **ninguna** (verificado) | ninguna (sin fallback SW) | **descarga externa** | ninguna |
| C4 semántica original | completa con shaders | completa (core) / nativa (compat, deprecado) | idéntica (es Glide) | completa |
| C5 pruebas sin GPU/assets | **WARP determinista** | sin fallback de sistema | la del wrapper externo | determinista |
| C6 núcleo agnóstico | sí (contrato §6) | sí | no aplica | sí |
| C7 riesgo temporal | bajo (cadencia del bucle) | bajo | irrelevante (otro binario) | medio (CPU en escenas complejas) |

### 5.6 Decisión

**Recomendación: Direct3D 11** (Opción A), con las salidas abiertas que el
contrato de §6 preserva: OpenGL como backend alternativo y el rasterizador
software como plan B. La decisión quedó **condicionada a R-1**: crear y
destruir un dispositivo D3D11 (HW y WARP) con los headers ya presentes.
**R-1 se ejecutó el 2026-11-02 y salió limpio en ambos modos (FL 11_1,
205 checks, 0 fallos, Clang y GCC): la recomendación queda confirmada.**
La elección no fue por popularidad: es la única opción que cumplía los siete
criterios del proyecto en el momento de decidir, con verificación de
toolchain en esta máquina y sin descargas. Detalle en `docs/D3D11_DEVICE.md`.

## 6. Arquitectura propuesta (NO implementada)

### 6.1 Principios

1. **El contrato es el módulo; el backend es un detalle.** Igual que el
   audio: `backend_dsound.c` implementó un contrato inyectable; el renderer
   sigue el mismo patrón.
2. **El renderer nulo no muere:** sigue siendo el renderer de `test_app`,
   `framedemo` y de todo lo portable. El contrato admite ambas
   implementaciones y las pruebas existentes no cambian.
3. **Solo el backend ve la API gráfica.** `windows.h` ya está acotado a tres
   ficheros en I-4; el backend D3D11 añade los suyos (Windows-only, excluido
   del `Makefile` POSIX como `backend_dsound.c`).
4. **Nada de `frame.c` cambia.** El hook `present` es y sigue siendo
   `int (*)(void *ctx, const gex_frame_tick *)`.

### 6.2 Contrato público (pseudointerfaz, no producción)

```c
/* port/include/gex/render/renderer.h (propuesta) */

typedef enum gex_renderer_kind {
    GEX_RENDERER_NULL = 0,   /* I-4: cuenta presents, no dibuja (sigue vivo) */
    GEX_RENDERER_D3D11       /* futuro: backend Windows */
} gex_renderer_kind;

typedef struct gex_renderer_config {
    void *hwnd;              /* HWND prestado (Windows); NULL en pruebas */
    uint32_t width, height;  /* 512x384 nativo del juego (§2.3) */
    int vsync_retraces;      /* análogo de grBufferSwap(3): SyncInterval */
    int want_debug_layer;    /* D3D11 debug layer en pruebas */
} gex_renderer_config;

typedef struct gex_renderer {
    gex_renderer_kind kind;
    int initialized;
    /* estadísticas que ya expone el nulo, más las del backend */
    uint64_t presents, last_now_us, last_steps, last_frame_index;
    /* la vtable del backend; el llamante nunca la toca */
    const struct gex_renderer_backend *backend;
    void *backend_state;     /* gex_null_renderer / estado D3D11 */
} gex_renderer;

/* Ciclo de vida: espejo exacto de gex_null_renderer_*. */
int  gex_renderer_init(gex_renderer *r, gex_renderer_kind kind,
                       const gex_renderer_config *cfg);
/* gex_frame_callback: envuelve backend->present; 0 ok, 1 error. */
int  gex_renderer_present(void *ctx, const gex_frame_tick *tick);
void gex_renderer_shutdown(gex_renderer *r);
```

La **vtable** del backend (interfaz interna, `renderer_backend.h`) separa lo
que el nulo ignora de lo que el backend real necesita:

```c
typedef struct gex_renderer_backend {
    int  (*init)(void *state, const gex_renderer_config *cfg);
    int  (*begin_frame)(void *state, const gex_frame_tick *tick); /* clear */
    int  (*present)(void *state, const gex_frame_tick *tick);
    void (*on_focus_lost)(void *state);   /* estado limpio, como gex_input */
    void (*shutdown)(void *state);
} gex_renderer_backend;
```

`begin_frame` separa el *clear* del *present* porque el original los separa
también: el render (`0x0043DC3A`) llega **antes** de que el frame step drene y
haga swap, y un clear por frame es el primer efecto visible verificable
(R-3) sin dibujar geometría.

### 6.3 Módulos y ubicación

```text
port/include/gex/render/renderer.h          contrato (nuevo)
port/include/gex/render/renderer_backend.h  vtable interna (nuevo)
port/include/gex/render/null_renderer.h     EXISTENTE: sin cambios
port/src/render/renderer.c                  contrato + adaptador nulo (nuevo)
port/src/render/backend_d3d11.c             Windows-only (futuro; hoy NO existe)
port/src/app/i4_main_win32.c                pasaría el HWND en el config
```

`i4_app.c` pasa a crear `gex_renderer` en vez de `gex_null_renderer` con
`GEX_RENDERER_NULL` por defecto: **mismo comportamiento de I-4, byte a byte**
en estadísticas y salida. El Makefile POSIX compila `renderer.c` (portable)
y no compila `backend_d3d11.c` (como ya hace con `backend_dsound.c`).

Estado real a 2026-11-03: existen ya `port/include/gex/render/d3d11_device.h`
y `port/src/render/d3d11_device.c` (R-1) y
`port/include/gex/render/d3d11_swapchain.h` /
`port/src/render/d3d11_swapchain.c` (R-2). El contrato `renderer.h`, la vtable
`renderer_backend.h` y el backend `backend_d3d11.c` **siguen sin existir**:
llegan con la integración de R-3, cuando haya algo que presentar. La swapchain
se construyó como módulo propio, no como backend, para no adelantar ese
contrato.

### 6.4 Flujo por frame (futuro renderer real)

```text
bucle I-4 (sin cambios):
  poll de mensajes -> gex_i4_app_step -> gex_frame_run
    pre_frame (stub) -> update x N (stub) -> audio tick -> present:
      gex_renderer_present
        backend->begin_frame   clear del color uniforme (R-3)
        backend->present       Draw + Present(SyncInterval) (R-4)
```

La **cadencia sigue siendo del bucle** (espera en
`MsgWaitForMultipleObjectsEx` + `timeBeginPeriod(1)`), no del Present: así el
renderer no introduce acoplamiento sim↔present, que es justamente lo que
M-13/I-3 desacoplaron del original.

### 6.5 Errores y pérdida/restauración de dispositivo

- El backend devuelve 1 (error) y `gex_frame_run` lo trata como
  `GEX_FRAME_ERR_PRESENT` (aborta frame, vacía acumulador, reancla reloj) —
  **política ya probada en I-3, sin cambios**.
- *Device removed/lost* (`DXGI_ERROR_DEVICE_REMOVED/RESET`): el backend lo
  distingue de un error de uso. Política propuesta: reconstruir el
  dispositivo de forma transparente en el siguiente `begin_frame` y
  contabilizar `device_resets`; si la reconstrucción falla N veces seguidas,
  error permanente (el proceso sale limpio con código != 0). Decisión
  detallada en R-6, cuando exista el backend que la ejercite.
- `gex_renderer_shutdown` idempotente, espejo del nulo y de
  `gex_i4_app_shutdown`.

### 6.6 Texturas y recursos (futuro, fuera de este hito)

El contrato anterior basta para R-1…R-6 (dispositivo, clear, present). Los
recursos gráficos se añadirán como operaciones explícitas del backend cuando
llegue la geometría de prueba (R-7), en un segundo bloque del contrato:

```c
/* borrador, no compromiso:
   int (*texture_upload)(state, w, h, format, pitch, const void *pixels);
   int (*set_palette)(state, const uint32_t rgba[256]);
   int (*draw_triangles)(state, const gex_rdr_vertex *v, size_t n);
*/
```

Los formatos de píxel del original (`.3df`/NCC) se decodificarán a formatos
del backend en la carga, con fixtures `.3df` sintéticos generados por script
(igual que el `SLAG` sintético): **ningún asset del juego entra en el repo**.

### 6.7 Qué NO cambia

- `loop_core.c`, `frame.c`, `sched.c`, `stream.c`, `adpcm.c`, `sag.c`:
  intocables (este hito no los toca; el futuro hito tampoco debe hacerlo).
- El comportamiento observable de I-4 (`gexloop` con renderer nulo) se
  conserva; el renderer real será una **opción** (`GEX_RENDERER_D3D11`), no
  un reemplazo, hasta que pase sus pruebas R-1…R-6.
- `docs/PROJECT_STATUS.svg`: mide MAIN; ni este hito ni el renderer lo
  tocan.

## 7. Plan de validación incremental (para el hito de implementación)

| Paso | Qué demuestra | Sin assets del juego | Estado |
| --- | --- | --- | --- |
| R-1 | Crear/destruir dispositivo D3D11 (HW y WARP); verifica la decisión de §5.6 | sí | **ejecutado 2026-11-02**: ambos modos a FL 11_1, 205 checks, 0 fallos, Clang y GCC; `docs/D3D11_DEVICE.md` |
| R-2 | Crear swapchain sobre el HWND de I-4 (512×384) y presentarla | sí | **ejecutado 2026-11-03**: swapchain DXGI bitblt (`DISCARD`, 2 buffers, `R8G8B8A8_UNORM`, ventana) sobre el `HWND` de I-4, con `ResizeBuffers` y COM verificado; **sin `Present`** (eso es R-3); 429 checks, 0 fallos, Clang y GCC; `docs/D3D11_SWAPCHAIN.md` |
| R-3 | `begin_frame` con clear de color uniforme; comprobación visual programática (`CopyResource` + hash del backbuffer bajo WARP) | sí | pendiente |
| R-4 | Presentación estable: cadencia del bucle intacta (~105–108 presents/s con nulo; medir el cambio con Present real), sin fugas (contador de objetos vivos del debug layer) | sí | pendiente |
| R-5 | Sincronización: `SyncInterval` 0/1/3 medidos con QPC; comparar el efecto de cola frente al drenaje+swap(3) del original (referencia: glide-shim CSVs, 8 318 swaps) | sí | pendiente |
| R-6 | Errores y recuperación: destrucción forzada del dispositivo, device-removed sintético, cierre limpio con Escape/`WM_CLOSE` durante el render | sí | pendiente |
| R-7 | Geometría de prueba (triángulo con `w`, textura sintética `.3df` rgb332 + paleta NCC), luego recursos reales | triángulo/textura: sí; assets reales: requiere material del juego | pendiente |
| R-8 | A/B visual contra el original (PC mantenedor, nGlide): solamente comparación humana de referencia, nunca un check automatizado | **requiere el juego** | pendiente |

Notas honestas: **R-1 y R-2 se han ejecutado** (dispositivo y swapchain
reales, sin dibujo). R-3…R-6 **no se han ejecutado**: no hay clear, ni
`Present`, ni medición de cadencia, ni recuperación ante device-removed. WARP
permite automatizar R-3/R-6 sin GPU; R-8 depende del mantenedor por diseño del
proyecto.

## 8. Riesgos, incógnitas y decisiones pendientes

| ID | Riesgo/incógnita | Impacto | Mitigación |
| --- | --- | --- | --- |
| R-1 | Que la recomendación D3D11 falle en máquina objetivo (nunca ejecutado) | alto | es el primer paso del hito siguiente; plan B = OpenGL o rasterizador propio con el mismo contrato |
| U-1 | Qué campos de `GrVertex` rellena el juego y cómo calcula `w` | medio | lectura guiada de `0x00432BBD`/`0x0046558A` antes de fijar la estructura de vértice del port |
| U-2 | Modos concretos de combine/alpha/chromakey/fog que usa el juego | medio | solo se sabe que *puede* configurarlos (imports); la lista de modos se derivará de los constantes que el juego pasa |
| U-3 | Formatos de textura (NCC obligado por `grTexNCCTable`; resto UNKNOWN) | medio | fixtures sintéticos + lectura de `0x004329EF`/`0x00461C32` |
| U-4 | Estructura de las «2 listas» que envía `0x0043DC3A` | bajo-medio | no se modela hasta tenerla; el port define su propia cola |
| U-5 | Papel de `grSstControl` y de `grGlideGetState/SetState` fuera del cine | bajo | no se inventa semántica |
| U-6 | `GEX.VFX` (rampa tipo gamma) y su relación con el render | bajo | UNKNOWN-PENDING en inventario |
| U-7 | Tasa real de refresco en runtime bajo wrapper (75 Hz pedidos) | bajo | ya medible con glide-shim; no bloquea el port |
| D-1 | Elección definitiva flip-model vs bitblt-model | medio | **parcialmente resuelta en R-2**: se usa bitblt (`DXGI_SWAP_EFFECT_DISCARD`) como base medida y portable; la comparación de latencia/cadencia con flip-model se hará en R-4 con `Present` real (`docs/D3D11_SWAPCHAIN.md` §3 y §10) |
| D-2 | Política exacta de device-lost (reintentos, salida) | medio | se fija en R-6 con el backend real |
| D-3 | Reproducir el drenaje doble del original | bajo | deliberadamente NO se replica: es un artefacto de cola de Voodoo, no semántica de juego (§2.4, §2.7) |

## 9. Referencias

**Repositorio (fuente de verdad del original):**
`docs/FRAME_STEP_TRACE.md` §3.1/§3.2/§10; `docs/SUBSYSTEM_SEGMENTS.md` §S2/§S4;
`docs/TIMING_GAME_LOOP_ANALYSIS.md` §2; `docs/F03_VS_ORIGINAL.md` §4;
`docs/ORIGINAL_ARTIFACT_INVENTORY.md` §4.1–4.2; `docs/BYTEMATCH.md` §5;
`docs/M13_DECOUPLE_PROPOSAL.md` §2/§9; `docs/FRAME_LOOP_PORT.md` §11;
`docs/WINDOW_INPUT_PORT.md`; `tools/glide-shim/` (gshim.def, README);
`TESTING.md` (Hito 1, sesiones 2ª–9ª; A/B glide-shim).

**Externas (referencia semántica, sin copiar código):**
- `sezero/glide` (G-01, RESEARCH S-26): fuente 3dfx Glide, licencia GPL de
  3dfx — minado solo para semántica de `grBufferSwap`/`grBufferNumPending` y
  enums `sst1vid.h`. **No se copia código** (GPL incompatible con el repo).
- `fmil95/soul-re` (G-02) y `LostArtefacts/TRX` (G-03): patrones de
  desacoplamiento Control/Draw de la familia Crystal Dynamics y de
  reimplementaciones modernas.
- `Hydroloxed/gexdecomp` (S-27): Gex 1996, **otro juego**; solo vocabulario.
- OpenGLide/OpenGlide (SourceForge, consultado 2026-11-02): LGPL-2.0, Beta,
  última actividad 2020-04-23 — dato usado en §5.3.
- nGlide y dgVoodoo2: freeware cerrado (RESEARCH S-02/S-20), usados como
  contexto de distribución, nunca como dependencia.
