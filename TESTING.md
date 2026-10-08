# TESTING — Metodología de pruebas

> Estado: **metodología definida, sin ejecutar** (no hay implementación ni
> entorno Windows de pruebas todavía).

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

Cada test registra: fecha, versión del juego (hash), OS/GPU/driver, wrapper
y versión, fix aplicado, pasos, FPS medidos, resultado observado y artefactos
(logs/vídeo). Matriz de resultados en [COMPATIBILITY.md](COMPATIBILITY.md).
