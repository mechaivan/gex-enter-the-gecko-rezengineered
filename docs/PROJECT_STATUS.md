# Panel de progreso — metodología y datos

**Última revisión:** 2026-11-23  
**Rama de referencia:** `dev/gex`  
**Último hito incorporado:** S-13 (reconstrucción de la capa de composición 2D de MAP7), commit [`00ec5cf`](https://github.com/mechaivan/gex-enter-the-gecko-rezengineered/commit/00ec5cf6f18fe16e5a91643073430a875ac561ca).

Este documento es la fuente de verdad del panel [`PROJECT_STATUS.svg`](PROJECT_STATUS.svg). Los porcentajes por subsistema describen **listas de entregables delimitadas**; la barra principal mide hitos completos hacia una primera build jugable DX11. Ninguna cifra representa tiempo invertido ni una predicción de fechas.

## Cómo leer el panel

- **Implementado y verificado:** el entregable está integrado y cuenta con pruebas o ejecución real documentada.
- **Parcial / en integración:** existe una parte funcional, pero queda validación, cobertura o integración necesaria.
- **Pendiente / planificado:** todavía no está implementado.
- **Aplazado:** fuera del alcance activo por decisión expresa.

Los colores del SVG acompañan al texto; el estado nunca depende solo del color.

## Porcentajes y fórmula

Cada área tiene un inventario cerrado de entregables de alcance comparable. Cada entregable cuenta una vez:

`porcentaje = 100 × entregables implementados y verificados / entregables definidos`

Se redondea al entero más próximo. Los elementos parciales se muestran por separado y **no suman** al numerador. Los elementos planificados permanecen en el denominador. No se suman las áreas para crear un porcentaje global: sus alcances son distintos y algunas capacidades sirven de base a más de un subsistema.

## Camino a la primera versión jugable DX11

<badge color="info">10% · 1/10 hitos completados</badge>

Esta es la barra principal del dashboard: mide el camino hasta una primera build que permita arrancar el port, entrar desde el menú a un nivel, controlar al personaje y jugar una sección funcional con colisiones y audio. **No mide cuánto renderer se ha programado ni el porcentaje de juego original reconstruido.**

Se usa un criterio conservador de hitos completos: cada hito cuenta 1 punto cuando está integrado y verificado; los hitos parciales no suman. El porcentaje es `hitos completados / 10 × 100`.

| # | Hito necesario para la primera build jugable | Estado |
|---:|---|---|
| 1 | Runtime nativo, ventana y presentación D3D11 en GPU real | ✓ Completado |
| 2 | Escena inicial y presentación del menú reconstruidas y verificadas | ◐ Parcial: MAP7, cámara, geometría, texturas y elementos de menú funcionan; faltan fidelidad visual y encuadre |
| 3 | Menú funcional: navegación, selección y transición a partida | ○ Pendiente |
| 4 | Carga y presentación del primer nivel de gameplay | ○ Pendiente |
| 5 | Control del personaje y movimiento jugable | ○ Pendiente |
| 6 | Animaciones y estados del personaje integrados | ○ Pendiente |
| 7 | Física, suelo y colisiones funcionales | ○ Pendiente |
| 8 | Enemigos, objetos e interacciones esenciales | ○ Pendiente |
| 9 | Lógica y eventos necesarios para completar una sección del nivel | ○ Pendiente |
| 10 | Audio integrado y prueba de principio a fin de la primera sección jugable | ○ Pendiente |

El 10% indica que está cerrado el primer hito de infraestructura; **quedan nueve hitos por cerrar**. El hito 2 está muy avanzado técnicamente, pero no se cuenta hasta completar sus criterios. Esta barra se actualizará solo al cerrar hitos, por lo que puede avanzar en saltos.

## Estado por áreas

| Área | Resultado | Desglose |
|---|---:|---|
| Fundamentos del port | **100% · 7/7** | Base nativa, audio base, bucle, ventana, dispositivo, swapchain y pipeline de presentación |
| Escenario y gráficos MAP7 | **60% · 9/15** | 9 completos, 3 parciales, 3 pendientes |
| Audio del juego | **75% · 3/4** | Lectura, decodificación/streaming y salida DirectSound; integración del contenido real en gameplay pendiente |
| Entrada y controles | **33% · 1/3** | Captura de teclado en Win32; mapeo de juego y mandos modernos pendientes |
| Sistemas de jugabilidad | **0% · 0/6** | Personaje, animaciones, físicas/colisiones, enemigos/objetos, lógica de niveles y progresión/guardado |
| Modernización de usuario | **0% · 0/26** | Objetivos M-01…M-26 (26 mejoras propuestas); ninguno declarado completado como función de usuario |

Estas cifras son una instantánea del alcance que se define debajo. Si cambia el inventario, se documentará el cambio y su efecto en los porcentajes. Los objetivos aplazados X-01/X-02 no forman parte del denominador de modernización.

## Inventario que respalda los porcentajes

### Fundamentos del port — 7/7

1. Lector del contenedor de audio SLAG.
2. Decodificador IMA ADPCM, streaming y reproductor con backend inyectable.
3. Salida de audio DirectSound.
4. Bucle nativo de frames y planificador.
5. Proceso Win32, ventana, reloj y captura de eventos de teclado.
6. Dispositivo Direct3D 11 y swapchain DXGI.
7. Pipeline D3D11 con presentación real en GPU.

El 100% se limita a estos fundamentos. No significa que el juego original esté integrado o sea jugable.

### Escenario y gráficos MAP7 — 9/15

**Implementados y verificados (9):**
1. Trayectoria y orientación de cámara original de MAP7.
2. Emisión de geometría del nivel y proyección reproducidas.
3. Lectura VFX y decodificación de los formatos de textura utilizados.
4. Conversión de geometría y recursos de malla para D3D11.
5. Atlas con cobertura de las texturas del vuelo y del menú.
6. Escala UV, conversión de color y mezcla por comando corregidas.
7. Escena MAP7 dibujada en GPU real con la cámara del juego.
8. Actor del menú, modelos de botones y rótulos integrados.
9. Primitivas 2D reutilizables y capa de fondo conectada (S-13).

**Parciales (3; no suman al porcentaje):**
10. Parser DFX: ruta necesaria implementada, alcance global y validación completa aún abiertos.
11. Recorrido BSP: estructura, visibilidad y emisión integradas; queda cerrar la validación dinámica integral.
12. Fondo del menú: degradado aproximado conectado; falta recuperar el emisor, la tabla de colocación y las texturas originales.

**Pendientes (3):**
13. Encuadre exacto de la cámara al presentar el menú.
14. Detalles visuales restantes: pantalla inferior del televisor, guirnalda de luces, patrón de hierba y polígono pendiente.
15. Navegación e interacción funcional del menú y selección de opciones.

### Audio — 3/4

1. Lector SLAG: implementado y probado.
2. Decodificación IMA ADPCM, streaming y reproductor: implementados y probados.
3. Backend DirectSound: implementado y probado con dispositivo real.
4. Integración de los bancos y eventos de audio del juego dentro de la experiencia jugable: pendiente.

### Entrada y controles — 1/3

1. Recepción de eventos de teclado y gestión de ventana/foco: implementada.
2. Mapeo completo de acciones originales de Gex a la lógica del juego: pendiente.
3. Compatibilidad moderna con mandos, reasignación y ajustes: planificada.

### Jugabilidad — 0/6

1. Movimiento y control del personaje.
2. Animaciones y estados del personaje.
3. Física, colisiones y detección de suelo.
4. Enemigos, objetos e interacciones.
5. Lógica y eventos de los niveles.
6. Progresión, estados de partida y guardado.

Los seis elementos representan los grandes sistemas necesarios para una primera versión jugable; no afirman que la investigación de esos sistemas esté completa.

### Modernización — 0/26

Los 26 objetivos M-01…M-26 siguen siendo propuestas en el documento de referencia. Incluyen configuración, resoluciones y pantalla, controles, cámara, frecuencia de refresco, experiencia en ventanas, modos Original/Modern, recursos visuales opcionales, localización, voces e introducción omisible. La existencia de infraestructura compartida no equivale a declarar una mejora de usuario terminada.

## Evidencias y documentos de referencia

- [PROJECT_STATE.md](../PROJECT_STATE.md): registro técnico cronológico y estado detallado.
- [ROADMAP.md](../ROADMAP.md): hoja de ruta pública y etapas.
- [MODERNIZATION_GOALS.md](../MODERNIZATION_GOALS.md): catálogo de objetivos de modernización.
- [D3D11_FIRST_SCENE_PLAN.md](D3D11_FIRST_SCENE_PLAN.md): plan técnico y evidencias del primer escenario.
- [CHANGELOG.md](../CHANGELOG.md): historial de cambios.
- [Archivo de metodología anterior v3.6](archive/PROJECT_STATUS_METHODOLOGY_v3.6_2026-10-11.md): conservación íntegra del sistema anterior.

## Procedimiento de actualización

1. Revisar el estado técnico y las evidencias del último trabajo.
2. Cambiar un entregable a «implementado y verificado» solo cuando cumpla su criterio y exista evidencia.
3. Recalcular la fracción de cada área con el inventario anterior.
4. Actualizar el SVG con exactamente los mismos valores, incluida la barra principal de hitos hacia la primera build jugable.
5. Actualizar README, ROADMAP y CHANGELOG cuando el cambio afecte a la presentación pública.
6. Comprobar enlaces, cifras y coherencia entre documentos.
7. No eliminar registros históricos ni cambiar el alcance para inflar porcentajes.

**Regla:** investigar no equivale a implementar; documentar no equivale a integrar; compilar no equivale a validar en ejecución; mostrar MAP7 no equivale a tener un juego jugable.
