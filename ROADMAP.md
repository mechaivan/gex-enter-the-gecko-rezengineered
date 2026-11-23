# Hoja de ruta — Gex: Enter the Gecko REZengineered

Este documento presenta el plan del proyecto en lenguaje accesible. El detalle técnico, los análisis históricos y las tareas de ingeniería inversa se conservan íntegros en el [archivo de la hoja de ruta técnica anterior](docs/archive/ROADMAP_TECHNICAL_2026-11-22.md), además de sus documentos especializados.

**Objetivo principal:** conseguir una primera versión nativa y jugable de *Gex: Enter the Gecko* para PC, basada en el comportamiento de la versión original y con un renderer Direct3D 11.

**Estado a 2026-11-23:** fundamentos nativos implementados; escenario MAP7 y menú en integración; gameplay todavía pendiente.

## Estados

- ✓ **Implementado y verificado:** integrado y probado con evidencia.
- ◐ **Parcial / en desarrollo:** hay una parte funcional, pero faltan integración, cobertura o validación.
- ○ **Pendiente / planificado:** todavía no implementado.
- Ⅱ **Aplazado:** fuera del alcance activo por ahora.

## Etapas principales

### 1. Ingeniería inversa y documentación del original

**Estado: base de investigación establecida.**

Entender cómo funciona la versión original de PC: formatos, carga de niveles, cámara, renderizado, audio, entrada y lógica. Las conclusiones se documentan con su nivel de evidencia y se contrastan con el ejecutable original. Los ports antiguos de PC, incluidos los basados en 3DFX o desarrollados por el mismo estudio, se usan como referencias secundarias cuando ayudan a resolver dudas.

**Continuación:** cerrar las áreas que todavía requieren descompilación o validación dinámica con el entorno RE.

### 2. Fundamentos del port nativo

**Estado: implementados y probados.**

- Proceso y ventana nativos de Windows.
- Bucle de ejecución y reloj.
- Captura básica de teclado.
- Lectura, decodificación y streaming de audio, con salida DirectSound.
- Dispositivo, swapchain y pipeline de presentación Direct3D 11.

Esto es la infraestructura del port, no el juego completo.

### 3. Primer escenario: MAP7 y menú

**Estado: en integración.**

MAP7 es el escenario que aparece durante la presentación del menú original. Ya se han integrado y probado en GPU real:

- Carga de geometría y recorrido del escenario.
- Cámara original y trayectoria cinematográfica.
- Texturas, atlas, UV, color y mezcla.
- Modelos y rótulos principales del menú.
- Primitivas 2D reutilizables y una primera aproximación al fondo del menú.

**Trabajo pendiente:**

- Recuperar el fondo original texturizado, sus estrellas, el sol y la colocación exacta.
- Reproducir la guirnalda de luces y resolver detalles visuales restantes.
- Ajustar el encuadre con la cámara que usa el menú.
- Completar la presentación del televisor y otros elementos pendientes.
- Implementar navegación y selección funcional de las opciones.

El desglose técnico está en [D3D11_FIRST_SCENE_PLAN.md](docs/D3D11_FIRST_SCENE_PLAN.md), especialmente en sus últimas secciones.

### 4. Integración de la jugabilidad

**Estado: pendiente.**

Recuperar e integrar los sistemas que convierten la escena en un juego:

- Movimiento y control del personaje.
- Animaciones y estados.
- Física, colisiones y detección de suelo.
- Enemigos, objetos e interacciones.
- Eventos, scripts y lógica de nivel.
- Progresión y estado de partida.

La prioridad es comprender los sistemas comunes y reutilizarlos. No se pretende reconstruir cada nivel desde cero.

### 5. Primera versión jugable

<badge color="warning">Pendiente</badge>

Integrar los sistemas anteriores en una experiencia que pueda jugarse de principio a fin, con carga de nivel, controles, cámara, audio, colisiones y progresión suficientemente estables para pruebas.

**Este es el hito principal del proyecto.** No equivale todavía a una edición comercial terminada ni autoriza por sí mismo una distribución pública.

### 6. Modernización y pulido

**Estado: planificado.**

Una vez consolidada la experiencia original, abordar mejoras opcionales:

- Resoluciones modernas, 16:9 y ultrawide.
- Mandos actuales, reasignación, zonas muertas y vibración.
- Opciones gráficas, cámara y modos Original / Modern.
- Mejoras de ventana, DPI y multimonitor.
- Recursos HD opcionales, localización y voces.
- Introducción omisible y ajustes de calidad de vida.

La lista íntegra y sus dependencias están en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md). Ninguna mejora se considera implementada por estar descrita aquí.

## Próximos trabajos técnicos

El siguiente foco continúa siendo cerrar MAP7 y el menú antes de ampliar el alcance:

1. Habilitar Ghidra + JDK 21 para que REA pueda descompilar el ejecutable y recuperar el emisor del fondo 2D.
2. Resolver el fondo texturizado, estrellas/sol y guirnalda de luces.
3. Medir el encuadre exacto de la presentación del menú.
4. Resolver los detalles gráficos pendientes con evidencia, sin introducir cambios especulativos.
5. Completar la interacción del menú y preparar la transición al primer nivel jugable.

## Criterios de avance

- Un análisis o una hipótesis no cuentan como funcionalidad implementada.
- Una compilación correcta no sustituye una prueba de ejecución.
- Las tareas gráficas se validan con capturas reales y comparaciones cuando existe una referencia adecuada.
- Los porcentajes y su inventario están en [docs/PROJECT_STATUS.md](docs/PROJECT_STATUS.md).
- El estado técnico cronológico y los hallazgos completos permanecen en [PROJECT_STATE.md](PROJECT_STATE.md) y en los documentos enlazados desde él.

## Historial técnico conservado

La hoja de ruta anterior, con todas sus tareas, IDs, notas, dependencias, pruebas y registros detallados, se conserva sin recortes en [docs/archive/ROADMAP_TECHNICAL_2026-11-22.md](docs/archive/ROADMAP_TECHNICAL_2026-11-22.md). No se ha descartado trabajo histórico.
