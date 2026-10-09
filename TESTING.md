# TESTING — Metodología de pruebas

> Estado: **Hito 1 ejecutado (2026-10-09, PC del mantenedor) + resto de
> la matriz pendiente** (Fase 2 sin iniciar). Detalle en «Hito 1» más abajo.

## Principio

Ninguna solución se considera terminada porque "funcione una vez".
Siempre comparar:

**Original vs Fix existente vs REZengineered**

## Qué comprobar (por cambio)

- Estabilidad (arranques repetidos, sesiones largas, cambios de nivel).
- Rendimiento (FPS medidos, frame pacing).
- Comportamiento (velocidad de juego, física, timing).
- Compatibilidad (versiones US/EU/demo, GPUs, wrappers).
- Controles, gráficos, audio, resolución.
- Regresiones (re-ejecutar tests de cambios anteriores).

## Protocolo de registro (futuro)

Cada test registra: fecha, versión del juego (hash — referencia EU en
[docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md)),
OS/GPU/driver, wrapper y versión, fix aplicado, pasos, FPS medidos,
resultado observado y artefactos (logs/vídeo). Matriz de resultados en
[COMPATIBILITY.md](COMPATIBILITY.md).

## Protocolo de prueba reproducible (2026-10-09; base del Hito 1)

Base obligatoria: copia de trabajo de la **EU v1.00.000 con Glide**
(inventario [docs/ORIGINAL_ARTIFACT_INVENTORY.md](docs/ORIGINAL_ARTIFACT_INVENTORY.md));
US/D3D/parches solo como comparadores, nunca como referencia de «correcto».

- **Objetivo y alcance:** una pregunta por prueba (área FA + issue I-xx si
  aplica); qué queda fuera, explícito.
- **Entorno a registrar:** SO + arquitectura + build, CPU/RAM, GPU + driver
  exacto, wrapper Glide (nombre/versión/config) o HW 3Dfx, refresco usado.
- **Procedencia y hashes:** origen de cada fichero (CD EU, imagen, fix) +
  MD5/SHA-256 verificados contra inventario antes de probar.
- **Estado de instalación:** método (F-05 u otro), claves de registro
  creadas/valores, unidad CD (física/imagen/letra; TOC preservada o no),
  `glide2x.dll` en uso (origen/versión).
- **Pasos y veredicto:** pasos numerados reproducibles; resultado esperado
  (declarado antes); resultado observado (hechos); éxito/fallo según
  criterio escrito antes de ejecutar.
- **Artefactos:** capturas (arranque, menús, HUD, fallo), logs (juego,
  wrapper, monitor de registro/sistema), vídeo si hay timing implicado;
  FPS medidos + método de medida.
- **Fallos:** cada fallo registra síntoma, contexto completo, frecuencia
  (siempre/a veces/una vez) y repetición paso a paso; **observación**
  (visto) separada de **hipótesis** (causa propuesta) e **inferencia**.
- **Repetición:** rehacer desde estado limpio documentado (misma copia o
  copia fresca con igual hash); anotar qué cambió si no reproduce.
- **Custodia:** originales de Drive intactos (nunca probar sobre ellos);
  copias de trabajo aisladas por prueba; ningún byte del juego en el repo;
  re-verificar hashes al cerrar la prueba.
- **Limitaciones moderno-vs-época:** cada resultado anota qué parte del
  entorno es moderna (emulación MCI/DSound, wrappers, GPU, SO) y qué
  exigiría HW/SW de época; sin época, «correcto» queda provisional.

## Matriz de pruebas (priorizada, FA-04…FA-15)

> Fila FA-11/FA-12 ejecutada (Hito 1, 2026-10-09); resto sin ejecutar.
> Sin nuevos IDs (trazabilidad por área FA + nombre).
> FA-01…FA-03 fuera de la matriz runtime (inventario estático + diffs
> Etapa D). Prioridad: P0 = puerta de entrada · P1 = sistemas · P2 =
> complementarias. Resultados futuros → [COMPATIBILITY.md](COMPATIBILITY.md).

| Área FA | Prueba | Requisitos | Dependencias | Datos a capturar | Moderno / época | Riesgos | Prioridad |
|---|---|---|---|---|---|---|---|
| FA-11/FA-12 | Instalación + primer arranque EU (F-05) ✅ Hito 1 | Windows + contenido CD EU + unidad/imagen | Ninguna (puerta de entrada) | Claves creadas, mensajes CD, ¿arranca? | Moderno: instalador roto → F-05 manual; época: instalador real | I-15/I-16/I-17; Hito 1: arranca (ver abajo) | P0 |
| FA-12 | Detección CD: físico vs imagen vs ausente | Unidad óptica y/o imagen con TOC preservada | Instalación (fila anterior) | Mensaje exacto, letra/unidad aceptada, primer lector | Moderno: MCI degradado; época: lector real | Imágenes sin TOC/subcanales falsean | P0 |
| FA-11 | Registro: lecturas, guardado, admin | Monitor de registro/sistema (a elegir, p. ej. Sysinternals) | Instalación | Claves tocadas, imprescindible vs config, ubicación `GEX2*.GEX`, causa admin | Moderno sí | HKLM vs portable; I-17 | P1 |
| FA-09 | CD-audio por nivel + cambios de nivel | Audio funcional + CD/imagen con 16 pistas | Instalación + detección CD | Mapeo pista↔nivel, cortes (I-12), loops, volumen | Moderno: MCI/DSound emulados; época: referencia | I-11/I-12; 15-vs-16 abierto | P1 |
| FA-05 | FPS + velocidad baseline | Contador FPS + referencia visual | Instalación | FPS, frame pacing, velocidad observada | Moderno bajo wrapper (no referencia); época: Voodoo = referencia | Sin FA-15 no se valida «correcto» | P1 |
| FA-04 | Timing: velocidad vs refresco/V-Sync | Control de refresco (60/75 Hz si posible) | FPS baseline | Velocidad a distintos Hz/con V-Sync | Moderno parcial (modos limitados); época: 75 Hz pedido | I-01/I-02/I-07 | P1 |
| FA-13 | Modos de vídeo + viewport/HUD | Enumeración de modos (wrapper/log) | Instalación | Modos ofrecidos, resolución real, HUD por modo | Moderno: wrapper escala; época: 512x384@60 ref | I-06 (contexto wrapper) | P1 |
| FA-10 | Intro + cinemáticas | Arranque funcional | Instalación | ¿Reproducen? AV-sync, códec observable (sin asumir Indeo) | Moderno sí; época: referencia | I-14; F-06 débil | P1 |
| FA-06 | Teclado + joystick + ratón | Teclado, mando, observación en menús/juego | Instalación | Mapeo real, detección mando, ausencia ratón | Moderno: mandos ≠ época; época: joystick WinMM | I-18/I-19 | P1 |
| FA-08 | SFX + voces UK + volúmenes | Audio funcional | Instalación | SFX por nivel, disparadores de voz, efecto volumen (I-13) | Moderno: DSound emulado | I-13 (un solo reporte) | P1 |
| FA-14 | Fullscreen/ventana/foco/Alt+Tab | — | Instalación | Modo real, Alt+Tab, pérdida de foco | Moderno sí (+wrappers); época: exclusividad ref | I-06 | P2 |
| FA-07 | Cámara: controles y comportamiento | Juego jugable | Instalación | Controles reales PC, seguimiento, colisiones | Ambas (época ideal); sin evidencia previa | Sin base documental | P2 |
| FA-05 | F-03 antes/después (diff funcional) | Binario F-03 (acceso tgames, Fase 2+) | FPS baseline + acceso | FPS/velocidad exe F-03 vs original | Moderno (declarado Win98–XP: limitación) | Alcance declarado estrecho | P2 |
| FA-15 | Referencias «correcto» per sistema + matriz HW | Resultados de las filas anteriores | Todas (continua) | Tabla de referencia por sistema; matriz HW (Fase 13) | Gap época: sin HW propio salvo mantenedor | No inventar época | P1 |

## Hito 1 — Primera prueba funcional (2026-10-09, PC del mantenedor) ✅ EJECUTADA

> Primer hito funcional confirmado del proyecto: instalación manual F-05
> (sin paso F-01) + arranque + nivel jugable en Windows 11 64-bit, con
> ejecutable EU original inalterado (MD5 verificado). Renderer en uso:
> DESCONOCIDO (ver punto 7). Sin FPS medidos, sin logs ni capturas
> archivadas todavía.

1. **Entorno (CONFIRMADO, parcial):** Windows 11 de 64 bits. Build de
   Windows, CPU/RAM, GPU + driver, refresco y layout de monitores:
   PENDIENTES de registrar (parte del siguiente paso recomendado).
2. **Instalación (CONFIRMADO):** copia manual de `GEX2/` EU a
   `C:\GEX_REZ\GEX2` (532 ficheros); `GEX3D.EXE` 1557504 B, MD5
   `692b12825003417cc4a5a9d4db13eebe` (coincide con inventario §4).
   Exe inalterado (sin parches); nGlide no instalado *durante* esta
   prueba (instalaciones anteriores: UNKNOWN).
3. **Registro (CONFIRMADO en esta config):** `...\Gex2\1.00` con
   `Version`=DWORD 2, `InstallDir`=`C:\GEX_REZ\GEX2`,
   `CDDriveName`=`D`. Imprescindible-vs-config y causa admin:
   pendientes (FA-11).
4. **CD (CONFIRMADO en esta config):** imagen CloneCD original montada
   como `D:`; el juego arranca sin error de CD (FA-12). TOC de la
   imagen montada: no re-verificada en esta prueba.
5. **Resultado (CONFIRMADO):** `GEX3D.EXE` arranca, entra en un nivel y
   permite mover al personaje; música y efectos funcionan en esta
   config (I-11 NO reproducido aquí; I-12 pendiente de más niveles).
6. **Problemas observados (CONFIRMADO como observación, causa UNKNOWN):**
   resolución baja 4:3 (esperable de la época, modo exacto sin medir);
   bandas negras arriba/izquierda con centrado sin confirmar; HUD
   ligeramente descolocado; iconos del escritorio desplazados al
   segundo monitor durante el juego (reversible al cerrar) → I-21, I-22.
7. **Hallazgo Glide (2ª sesión: `glide2x` CARGADA):** `glide2x.dll` cargada
   desde `C:\WINDOWS\SYSTEM32\` (= fichero físico `SysWOW64\glide2x.dll`
   por redirección WOW64; coherente con el observado de 1630208 B — el
   hash registrado en 3ª sesión). `3dfxSpl2.dll` también cargada (splash 3dfx, S-21).
   `glide.dll`/`glide3x.dll`: carga sin confirmar. Procedencia e identidad
   de la `glide2x` (wrapper moderno vs época): pendientes (FA-02/FA-03).
8. **Pantalla 3DFX (2ª sesión: mecanismo identificado):** la muestra
   `3dfxSpl2.dll` (biblioteca splash de Glide 2.x, S-21), cargada en el
   proceso. Por sí sola sigue sin demostrar el renderer activo (eso lo
   indica `glide2x`, ver nota 2ª sesión).
9. **Custodia (CONFIRMADO):** originales de Drive intactos; prueba sobre
   copia aislada; ningún byte del juego en el repo; ningún dato personal
   del PC registrado salvo lo listado aquí.

### Nota post-Hito 1 (2026-10-09): enumeración 64-bit vacía = artefacto WOW64

- **Evidencia (CONFIRMADO):** con el juego en marcha, `(Get-Process GEX3D)`
  desde PowerShell de 64-bit devuelve 7 módulos (`GEX3D.EXE`, `ntdll.dll`,
  `wow64.dll`, `wow64base.dll`, `wow64win.dll`, `wow64con.dll`,
  `wow64cpu.dll`); el filtro gráfico
  (`glide|3dfx|d3d|ddraw|dxgi|opengl|vulkan|…`) no devuelve nada.
- **Explicación (comportamiento documentado de WOW64, no del juego):** un
  observador de 64-bit solo ve el lado de 64-bit del proceso emulado (la
  capa WOW64); las DLL reales de 32-bit (`glide2x`, `winmm`, `dsound`…)
  son invisibles por esta vía. Es un artefacto de medida: **no dice nada
  sobre el renderer**, ni a favor ni en contra de Glide. La composición
  exacta del set (`wow64base`/`wow64con` incluidos) depende de la build de
  Windows y no es diagnóstica.
- **Siguiente paso (EJECUTADO 2026-10-09):** consulta repetida desde
  PowerShell de 32-bit → ver nota «2ª sesión» (`glide2x` cargada).

### Nota post-Hito 1 (2026-10-09, 2ª sesión): `glide2x` cargada + parpadeos

- **Renderer (CONFIRMADO carga; hipótesis sólida):** con el juego en marcha,
  `glide2x.dll` CARGADA desde `C:\WINDOWS\SYSTEM32\glide2x.dll` (= físico
  `SysWOW64\glide2x.dll` por redirección WOW64; coherente con el fichero
  observado de 1630208 B — hash registrado en 3ª sesión). `3dfxSpl2.dll` también
  cargada (splash Glide 2.x, S-21). Ruta Glide = hipótesis muy sólida;
  ficha registrada en 3ª sesión; originalidad/wrapper pendientes.
- **Acompañantes (OBSERVADO, rol UNKNOWN):** `ddraw.dll`, `d3d9.dll`,
  `dxgi.dll` y `atidx9loader32.dll` presentes en el proceso. Su presencia
  NO demuestra que sean el renderer activo (driver, appcompat, …).
  `atidx9loader32` sugiere GPU AMD (HYPOTHESIS por prefijo; modelo/driver
  pendientes). Exe intacto (MD5 re-confirmado).
- **Pantalla (OBSERVADO):** el 2º monitor parpadea tras el splash 3dfx, al
  empezar las cinemáticas (Ubisoft + juego) y al pasar al menú principal;
  4:3 + bandas arriba/izquierda + HUD desalineado de nuevo → I-21/I-22.
- **Siguiente paso (EJECUTADO 2026-10-09):** ficha del fichero registrada
  → ver nota «3ª sesión».

### Nota post-Hito 1 (2026-10-09, 3ª sesión): ficha Glide registrada

- **Ficha `glide2x.dll` (CONFIRMADO, solo lectura):**
  `SysWOW64\glide2x.dll` (1630208 B) — Descripción `3Dfx Interactive, Inc.
  Glide DLL`; Producto `Glide para Voodoo Banshee` (transcripción aproximada;
  nombre exacto en 4ª sesión); Versión de producto `2.60.0.658`; SHA-256
  `7cbd095872e821b54cd6fa03f76aa22073271567175069c53ebb2e73b0299aab`.
  Presente en los módulos de `GEX3D.EXE` en ejecución.
- **Ficha `3dfxSpl2.dll` (CONFIRMADO):** Descripción y Producto `3dfx
  Splash Screen`, versión `1.0.0.4`; también cargada en el proceso.
  Biblioteca de pantalla de inicio (S-21); NO es prueba del renderer.
- **Interpretación (rigurosa):** la ficha es COMPATIBLE con una
  implementación Glide de 3dfx, pero metadatos + hash por sí solos NO
  demuestran si el fichero es original, redistribuido o modificado, ni
  si interviene un wrapper. Ruta Glide = hipótesis muy sólida (no hecho
  cerrado). I-21/I-22 siguen pendientes, sin causa atribuida.
- **Siguiente paso (EJECUTADO 2026-10-09):** fechas/firma consultadas
  → ver nota «4ª sesión».

### Nota post-Hito 1 (2026-10-09, 4ª sesión): fecha mostrada + sin firma

- **Producto exacto (CONFIRMADO):** `Glide® for Voodoo Banshee®` (nombre
  mostrado en Propiedades; corrige la transcripción aproximada de la 3ª
  sesión). Resto de la ficha intacto (2.60.0.658, SHA-256 verificado).
- **Fecha mostrada en Propiedades (CONFIRMADO como dato mostrado):**
  domingo 2019-09-15 00:54:48. Campo específico (creación/modificación)
  NO identificado: no atribuir a ninguno.
- **Firma digital (CONFIRMADO ausencia observada):** ninguna pestaña de
  firmas digitales ni firma visible en las propiedades consultadas.
- **Interpretación (documentada):** metadatos compatibles con Glide de 3dfx
  / Voodoo Banshee; fecha + ausencia de firma NO determinan si la DLL es
  original, redistribuida, modificada o de un paquete posterior; el hash
  identifica el contenido, no su procedencia o autenticidad; la carga es
  pista relevante, no demuestra la API de renderizado ni descarta un
  wrapper. `3dfxSpl2.dll` = pantalla de inicio (S-21), no prueba de
  renderer. I-21/I-22 pendientes, sin causa definitiva.
- **Siguiente diagnóstico:** pendiente de definir (Fase 1). No se solicitan
  de nuevo metadatos ni hash ya facilitados.

## Evaluación de alternativas de entorno (2026-10-09; Hito 1 = vía A)

> Hito 1 (2026-10-09): primera config observada funcionando (vía A:
> Win11 64-bit + F-05 manual + imagen como D:, sin parches). Veredicto
> general de compatibilidad: pendiente (una sola prueba; ficha + fecha y
> firma registradas — 4ª sesión; originalidad/wrapper pendientes).
> Base: EU v1.00.000 Glide (inventario §4.1: 38 `glide2x`, MCI, DSound,
> WinMM; instalador 16-bit roto en moderno → F-05).

### A. Windows moderno + wrapper Glide (PC físico)

- **Ventajas:** HW real y disponible; drivers actuales; captura (FPS,
  vídeo, logs) con herramientas modernas; instalable vía F-05 + wrapper.
- **Limitaciones:** MCI/CD-audio degradado (I-11); DSound emulado; modos
  y refrescos limitados por GPU/monitor modernos (75 Hz incierto);
  instalador 16-bit inútil; UAC/admin (I-17); DWM/Alt+Tab ≠ época.
- **Requisitos:** Win 10/11 64-bit exacto (versión+build); GPU+driver
  exactos; wrapper (nombre+versión+config); unidad óptica o imagen con
  TOC; herramientas de captura.
- **Riesgos:** atribuir al juego artefactos del wrapper/SO/GPU; Hito 1
  ejecutado una sola vez (ficha + fecha/firma — 4ª sesión;
  originalidad/wrapper pendientes); «correcto» de época
  inalcanzable aquí.
- **Sirve para:** P0 (instalación, detección CD lógica, registro,
  guardado, admin) + P1 observación (FPS relativo, modos modernos,
  input moderno, intro/SFX audibilidad). **No sirve para:** referencia
  «correcto» (FA-15), timing/FPS absolutos, CD-audio MCI original.

### B. Máquina virtual Windows (viabilidad parcial)

- **Ventajas:** snapshots y estado limpio reproducible; aislamiento;
  varios SO huésped (XP/7/10) en un host; instalación/registro
  repetibles.
- **Limitaciones:** GPU virtual o passthrough (doble emulación Glide→
  wrapper→GPU virtual); reloj virtualizado (timing/FPS no fiables);
  CD-audio/MCI en huésped degradado; refrescos virtuales; input con
  latencia.
- **Requisitos:** hipervisor+versión, huésped exacto, guest additions,
  GPU virtual vs passthrough documentado, política de snapshots.
- **Riesgos:** artefactos de virtualización confundidos con juego.
- **Veredicto:** viable para P0 (instalación/registro/detección lógica)
  y observación gruesa; **no válida** para FA-04/FA-05 absolutos,
  FA-13 referencia ni FA-15. No equivalente a A ni a C.

### C. Hardware histórico 3dfx (referencia de época)

- **Ventajas:** única referencia válida de «correcto»: Voodoo real,
  MCI/CD-audio original, refrescos de época, DSound HW, joystick WinMM
  época, instalador 16-bit funcional (Win95/98).
- **Limitaciones:** disponibilidad (no consta HW en proyecto; decisión
  del mantenedor); fragilidad; captura externa obligatoria (VGA);
  medición FPS por vídeo/conteo (sin overlays modernos); drivers época.
- **Requisitos (abiertos):** PC época o compatible, Voodoo (modelo a
  decidir), Win95/98 + DX época, unidad CD, joystick, capturadora.
  Sin modelo concreto: no consta en documentación.
- **Riesgos:** HW único/frágil, coste, tiempo.
- **Sirve para:** FA-15 (definir «correcto»), timing/FPS referencia,
  CD-audio referencia, render referencia, instalador original.

### Separar SO / wrapper / hardware

Variar un factor cada vez (mismo juego+wrapper en 2 SO; mismo SO+juego
con 2 wrappers; mismo todo en 2 GPUs); matriz de combinaciones
registrada; cada resultado con tupla completa (protocolo); contraste
contra época cuando exista.

## Requisitos mínimos: checklist previo (2026-10-09)

> Clasificación: IMP = imprescindible (toda prueba) · REC = recomendable ·
> ESP = específico de pruebas. Sin especs mínimas inventadas del juego.

| # | Requisito | Clase | Pruebas |
|---|---|---|---|
| 1 | Windows versión + arquitectura + build | IMP | Todas |
| 2 | CPU + RAM | IMP | Todas |
| 3 | GPU + driver exacto | IMP | Todas |
| 4 | Wrapper Glide + versión + config, o 3dfx modelo+driver | IMP | Todas |
| 5 | Método instalación + hashes verificados | IMP | Todas |
| 6 | Unidad CD física (modelo) o imagen + herramienta montaje + TOC verificada | IMP | FA-09/FA-12; resto REC |
| 7 | Contador FPS + método | IMP | FA-04/FA-05; resto REC |
| 8 | Captura pantalla/vídeo + logs + monitor registro | IMP | Todas (monitor: FA-11 IMP) |
| 9 | Copias aisladas + verificación de hash | IMP | Todas |
| 10 | Refrescos disponibles (lista EDID) | REC | FA-04/FA-05/FA-13 |
| 11 | Mando (modelo/conexión) | REC (ESP época: FA-06) | FA-06 |
| 12 | Capturadora externa | ESP época (C) | FA-15 + referencia |
| 13 | Snapshots / estado limpio repetible | REC (IMP si VM) | Todas en B |
| 14 | HW época (PC+Voodoo+Win95/98+CD+joystick) | ESP (C) | FA-15 + referencias |

## Propuesta: MVP y referencia ideal (2026-10-09)

- **MVP para empezar (A):** PC físico Windows 10/11 64-bit + wrapper Glide
  documentado (nGlide 2.10, estándar de facto; dgVoodoo2 para contraste) +
  imagen CD con TOC verificada + captura FPS/vídeo/logs + copias aisladas.
  Permite P0 + P1 observación. Sin HW concreto (no consta).
- **Referencia ideal (C):** HW época 3dfx + Win95/98 + CD físico + joystick
  + capturadora externa. Permite FA-15 y validación «correcto». Modelos a
  decidir por el mantenedor.
- **Hechos confirmados:** base EU-Glide estática (inventario); instalador
  roto en moderno (F-05); rutas MCI/DSound estáticas; wrappers catalogados
  (T-01…T-06) sin probar; Hito 1: F-05 manual + arranque + nivel jugable
  en Win11 64-bit (4ª sesión: ficha + fecha/firma; wrapper pendiente).
- **Recomendaciones:** empezar por A; usar B para repetición P0; reservar C
  para referencia; contrastar 2 wrappers antes de concluir render/timing.
- **Incógnitas:** specs del PC del Hito 1 (build/GPU AMD?/driver); TOC de
  la imagen montada; refrescos soportados; causa admin; origen de la
  `glide2x` (fecha mostrada 2019-09-15, sin firma; diagnóstico pendiente);
  resto de valores `.reg` (1 vez OK).
- **Decisiones del mantenedor:** completar specs + captura; VM sí/no;
  HW época sí/no (+modelos); herramientas de captura; momento de Fase 2.

## Categorías futuras de prueba

Cuando haya implementación, cubrir por cambio:

- Original behavior · Compatibility · Timing · Rendering
- Resolution · Aspect ratio · Camera
- Input · Controllers · Audio · FMV
- Alt+Tab · Multi-monitor · DPI · Configuration
- Original Mode · Modern Mode · Enhanced Visuals

Principio aplicable: **Original → Fix existente → REZengineered**.
