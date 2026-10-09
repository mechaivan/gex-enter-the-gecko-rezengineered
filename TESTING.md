# TESTING — Metodología de pruebas

> Estado: **metodología + protocolo + matriz FA definidos, sin ejecutar**
> (2026-10-09; máquina Windows de pruebas pendiente; Fase 2 sin iniciar).

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

## Protocolo de prueba reproducible (2026-10-09, sin ejecutar)

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

## Matriz de pruebas futuras (priorizada, FA-04…FA-15)

> Sin ejecutar; sin nuevos IDs (trazabilidad por área FA + nombre).
> FA-01…FA-03 fuera de la matriz runtime (inventario estático + diffs
> Etapa D). Prioridad: P0 = puerta de entrada · P1 = sistemas · P2 =
> complementarias. Resultados futuros → [COMPATIBILITY.md](COMPATIBILITY.md).

| Área FA | Prueba | Requisitos | Dependencias | Datos a capturar | Moderno / época | Riesgos | Prioridad |
|---|---|---|---|---|---|---|---|
| FA-11/FA-12 | Instalación + primer arranque EU (F-05) | Windows + contenido CD EU + unidad/imagen | Ninguna (puerta de entrada) | Claves creadas, mensajes CD, ¿arranca? | Moderno: instalador roto → F-05 manual; época: instalador real | I-15/I-16/I-17; valores `.reg` sin probar | P0 |
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

## Evaluación de alternativas de entorno (2026-10-09, sin probar)

> Ninguna configuración declarada compatible: sin pruebas, sin veredicto.
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
- **Riesgos:** atribuir al juego artefactos del wrapper/SO/GPU; F-05 sin
  probar; «correcto» de época inalcanzable aquí.
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
  (T-01…T-06) sin probar.
- **Recomendaciones:** empezar por A; usar B para repetición P0; reservar C
  para referencia; contrastar 2 wrappers antes de concluir render/timing.
- **Incógnitas:** máquina real (datos del checklist pendientes); TOC de la
  imagen disponible; refrescos soportados; causa admin; valores `.reg`.
- **Decisiones del mantenedor:** PC Windows concreto; VM sí/no; HW época
  sí/no (+modelos); herramientas de captura; momento de Fase 2.

## Categorías futuras de prueba

Cuando haya implementación, cubrir por cambio:

- Original behavior · Compatibility · Timing · Rendering
- Resolution · Aspect ratio · Camera
- Input · Controllers · Audio · FMV
- Alt+Tab · Multi-monitor · DPI · Configuration
- Original Mode · Modern Mode · Enhanced Visuals

Principio aplicable: **Original → Fix existente → REZengineered**.
