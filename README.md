![Gex: Enter the Gecko — REZengineered](assets/logo02.png)

# Gex: Enter the Gecko — REZengineered

**Un port nativo para PC, reconstruido con ingeniería inversa y Direct3D 11.**

Queremos recuperar y modernizar la versión original de *Gex: Enter the Gecko* para PC, entendiendo primero cómo funciona y construyendo una implementación nativa que conserve su comportamiento y personalidad.

No es un remake con un motor nuevo ni una simple modificación del ejecutable original: estamos desarrollando un **port propio**, apoyado en el análisis del juego de PC y en evidencias técnicas contrastables.

> **Estado actual:** ya existe una base nativa con audio, ventana, bucle de ejecución y renderer Direct3D 11. El escenario del menú (MAP7) se carga y se dibuja con geometría y texturas reales, y la cámara original y los elementos principales del menú están integrados. **Todavía no es una versión jugable:** faltan la interacción completa del menú y los sistemas de gameplay.

[![Progreso en ByteMatch](https://bytematch.dev/mechaivan/gex-enter-the-gecko-rezengineered.svg?label=Code&measure=matched_code_percent)](https://bytematch.dev/mechaivan/gex-enter-the-gecko-rezengineered)

## Progreso del proyecto

<img src="docs/PROJECT_STATUS.svg" alt="Panel de progreso de Gex REZengineered" width="100%" />

La barra principal muestra el camino a la primera build jugable DX11: **10 hitos definidos, 1 completado (10%)**. Debajo aparecen los porcentajes independientes de cada subsistema. Los hitos parciales no suman hasta estar verificados. Consulta los [criterios y el desglose de cada cifra](docs/PROJECT_STATUS.md).

## ¿Qué queremos conseguir?

### 1. Recuperar el juego original

- Comprender el ejecutable de PC, sus formatos, subsistemas y comportamiento.
- Reconstruir la carga de niveles, geometría, cámara, materiales, audio y lógica.
- Contrastar las conclusiones con el ejecutable original y con ports históricos de PC cuando aporten evidencia.
- Documentar hallazgos, decisiones, limitaciones y pruebas.

### 2. Construir un port nativo

- Sustituir la ruta gráfica heredada por un renderer moderno basado en Direct3D 11.
- Recuperar los escenarios, modelos, texturas, efectos y presentación original.
- Integrar audio, entrada, controles, cámara y sistemas de juego.
- Mantener una arquitectura reutilizable para los distintos niveles.

### 3. Modernizar la experiencia

Una vez recuperada la base del juego, queremos ofrecer mejoras opcionales sin perder la experiencia original:

| Área | Mejoras previstas |
|---|---|
| Pantalla | Resoluciones modernas, 16:9, ultrawide y ajustes de cámara |
| Controles | Mandos actuales, reasignación de botones, zonas muertas y vibración |
| Opciones | Configuración accesible y modos Original / Modern |
| Experiencia | Alt-tab, multimonitor, DPI y ajustes de presentación |
| Contenido | Localización, voces opcionales e introducción omisible |
| Gráficos | Recursos HD opcionales y mejoras visuales configurables |

Estas mejoras son objetivos de futuro, no funciones que afirmemos tener ya implementadas. El catálogo completo está en [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md).

## Estado de las funcionalidades

| Subsistema | Estado actual |
|---|---|
| Fundamentos del port nativo | ✓ Implementados y probados |
| Renderer Direct3D 11 | ✓ Base y pipeline funcionando en GPU real |
| Escenario MAP7 | ◐ Geometría, cámara y texturas integradas; quedan detalles de fidelidad |
| Menú principal | ◐ Escena y botones visibles; falta completar fondo exacto, encuadre e interacción |
| Audio | ◐ Ruta base implementada; falta integrarla con el contenido y la lógica del juego |
| Entrada | ◐ Ventana y teclado capturados; falta el control completo del personaje |
| Jugabilidad | ○ Pendiente de integración |
| Modernización | ○ Planificada |

**Leyenda:** ✓ implementado y verificado · ◐ parcial/en integración · ○ planificado o pendiente · Ⅱ aplazado.

## Hoja de ruta

1. **Fundamentos nativos** — proceso, audio, entrada, bucle de ejecución y renderer.
2. **Primer escenario y menú** — geometría, texturas, cámara, composición 2D y presentación de MAP7.
3. **Integración del juego** — menú funcional, personaje, controles, animaciones, físicas, colisiones, objetos y lógica.
4. **Contenido y niveles** — reutilizar los sistemas comunes y validar las particularidades de cada nivel.
5. **Primera versión jugable** — completar una experiencia jugable de principio a fin y someterla a pruebas.
6. **Modernización y pulido** — opciones, mandos, resoluciones, mejoras visuales y compatibilidad.

El orden puede cambiar si la ingeniería inversa descubre dependencias nuevas. No prometemos fechas hasta tener hitos verificables.

Consulta la [hoja de ruta detallada](ROADMAP.md) y el [estado técnico actual](PROJECT_STATE.md).

## Cómo trabajamos

- **Primero evidencia, después implementación.** Las hipótesis se identifican como tales.
- **Fidelidad al PC original.** Otras versiones sirven como referencia secundaria, no como sustituto del comportamiento de PC.
- **Pruebas antes de cerrar.** Los cambios se validan con tests y, cuando corresponde, ejecución real y GPU.
- **Documentación continua.** Los hallazgos técnicos se conservan para que el trabajo sea reproducible.
- **Preservación legal.** No distribuimos ejecutables, assets ni otros archivos propietarios del juego.

## Documentación

| Documento | Para qué sirve |
|---|---|
| [PROJECT_STATE.md](PROJECT_STATE.md) | Registro técnico detallado y estado actual |
| [Índice de documentación técnica](docs/TECHNICAL_DOCUMENTATION_INDEX.md) | Informes de investigación, resultados de fases y auditorías organizados por tema |
| [ROADMAP.md](ROADMAP.md) | Etapas del proyecto y próximos hitos |
| [PROJECT_STATUS.svg](docs/PROJECT_STATUS.svg) | Panel visual de progreso |
| [PROJECT_STATUS.md](docs/PROJECT_STATUS.md) | Criterios, inventarios y cálculo de porcentajes |
| [MODERNIZATION_GOALS.md](MODERNIZATION_GOALS.md) | Catálogo completo de mejoras futuras |
| [CHANGELOG.md](CHANGELOG.md) | Historial de cambios |
| [D3D11_FIRST_SCENE_PLAN.md](docs/D3D11_FIRST_SCENE_PLAN.md) | Investigación e implementación del primer escenario |
| [port/README.md](port/README.md) | Arquitectura y componentes del port nativo |
| [BUILD.md](BUILD.md) · [TESTING.md](TESTING.md) | Compilación y pruebas |
| [LICENSE.md](LICENSE.md) | Licencia y límites de distribución |

## Repositorio y contenido

Los informes de investigación y resultados de fases se organizan bajo `docs/reports/` y las auditorías bajo `docs/audits/`; consulta el [índice de documentación técnica](docs/TECHNICAL_DOCUMENTATION_INDEX.md).\n\nEl repositorio contiene código nuevo, herramientas propias y documentación. Los archivos originales del juego y otros materiales propietarios no se publican aquí. Consulta [LICENSE.md](LICENSE.md).

---

*Proyecto independiente de preservación y desarrollo, sin afiliación con Crystal Dynamics, Eidos, Square Enix, Midway, Ubisoft o Limited Run Games. Gex es marca de sus respectivos propietarios.*

**English:** REZengineered is a native PC port and preservation project for the original 1998 *Gex: Enter the Gecko* PC release. We reverse-engineer its behavior, build a Direct3D 11 implementation, and plan optional quality-of-life improvements while preserving the original experience. The project is not yet playable.
