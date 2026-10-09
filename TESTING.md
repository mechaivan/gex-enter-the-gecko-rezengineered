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

## Categorías futuras de prueba

Cuando haya implementación, cubrir por cambio:

- Original behavior · Compatibility · Timing · Rendering
- Resolution · Aspect ratio · Camera
- Input · Controllers · Audio · FMV
- Alt+Tab · Multi-monitor · DPI · Configuration
- Original Mode · Modern Mode · Enhanced Visuals

Principio aplicable: **Original → Fix existente → REZengineered**.
