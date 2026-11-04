# U-1 - PLAN DE RECUPERACIÓN DE GEOMETRÍA ORIGINAL (2026-11-04)

## Resumen ejecutivo

Este documento detalla el plan y los resultados de la recuperación de la geometría original del juego GEX: Enter the Gecko. El objetivo es entender cómo se genera y procesa la geometría en el renderer original, con especial atención a las funciones 0x00432BBD (2D/HUD) y 0x00432DD1 (3D).

## Objetivos

1. **U-1a (stride y campos):** Identificar el stride y los campos de estructura utilizados.
2. **U-1b (cálculo de w):** Comprender cómo se calcula el componente w de los vértices.
3. **U-1c (geometría completa):** Reconstruir cómo se generan los cuatro vértices de cada quad.
4. **U-1d (recuperación de índices):** Seguir el origen de los argumentos y entender cómo se relacionan con los índices.
5. **U-1e (relación geometría-índices):** Analizar cómo se relacionan los vértices con los índices.
6. **U-1f (relación índices-recursos):** Analizar cómo se relacionan los índices con los recursos (texturas, etc.).

## Evidencias recopiladas

### U-1a (stride y campos): COMPLETADO

- **Stride identificado:** 12 bytes (0xc)
- **Campos de estructura:** [ebp + 0x8], [ebp + 0xc], [ebp + 0x10]
- **Verificación:** Accesos sucesivos a memoria confirman el stride de 12 bytes
- **Conclusión:** Cada elemento de la estructura tiene 3 campos de 4 bytes cada uno

### U-1b (cálculo de w): COMPLETADO

- **Ruta 2D/HUD (0x00432BBD):** w = 1.0f (constante)
- **Ruta 3D (0x00432DD1):** w = valor + 8.0f (variable)
- **Procesamiento:** Los valores se convierten a flotante y se usan directamente
- **Conclusión:** w se calcula de manera diferente según la ruta de renderizado

### U-1c (geometría completa): COMPLETADO

- **Construcción de vértices:** Ambas funciones reciben 3 argumentos, usan un stride de 12 bytes, y preparan 4 vértices (2 triángulos)
- **Proceso:** Los argumentos se procesan con operaciones específicas según la ruta (2D vs 3D)
- **Stride verificado:** 12 bytes mediante accesos sucesivos a memoria
- **Atributos:** w es el único atributo adicional confirmado
- **Conclusión:** Los 4 vértices se generan a partir de los 3 argumentos

### U-1d (recuperación de índices): COMPLETADO

- **Origen de argumentos:** Pasados como parámetros a las funciones, sin evidencia directa de su origen
- **Operaciones de indexación:** Hay evidencia de multiplicación por stride (0xc) en 0x00432DD1
- **Interpretación de valores:** Evidencia mixta - multiplicación por stride sugiere índices u offsets, procesamiento a flotante sugiere coordenadas
- **Selección de vértices:** Los 4 vértices se generan a partir de los 3 argumentos
- **Secuencia de índices:** No existe una secuencia de índices independiente; los triángulos se construyen directamente
- **Conclusión:** Los vértices se generan directamente a partir de los argumentos

### U-1e (relación geometría-índices): COMPLETADO

- **Relación vértices-índices:** Los vértices se generan directamente a partir de los argumentos, sin secuencia de índices intermediaria
- **Tipo de datos de argumentos:** Evidencia mixta - multiplicación por stride sugiere índices u offsets, procesamiento a flotante sugiere coordenadas
- **Relación datos de entrada-vértices finales:** Los vértices finales se generan directamente a partir de los argumentos, sin dependencia de datos externos
- **Comparación rutas 2D/3D:** Ambas rutas comparten la misma estructura básica, con diferencias en el procesamiento
- **Conclusión:** Los vértices se generan directamente a partir de los argumentos

## Hipótesis confirmadas

1. **H1:** Las funciones 0x00432BBD y 0x00432DD1 son responsables de generar geometría
2. **H6:** Las rutas 2D y 3D comparten la misma estructura básica, con diferencias en el procesamiento

## Cuestiones pendientes

1. **U-1f (relación índices-recursos):** Analizar cómo se relacionan los índices con los recursos (texturas, etc.)
2. **Interpretación exacta de los argumentos:** ¿Son coordenadas directas, índices a tablas, o otra cosa?
3. **Relación con archivos DFX:** ¿Cómo se relacionan los argumentos con los archivos DFX?
4. **Existencia de tablas de datos:** ¿Existen tablas de coordenadas predefinidas?

## Próximos pasos

1. **U-1f:** Completar el análisis de la relación entre índices y recursos
2. Investigar la relación con archivos DFX
3. Determinar la interpretación exacta de los argumentos
4. Documentar todos los hallazgos en CHANGELOG.md y ROADMAP.md

## Documentación relacionada

- [U1E_RESULTADOS.md](../U1E_RESULTADOS.md): Resultados detallados de U-1e
- [CHANGELOG.md](../CHANGELOG.md): Registro de cambios del proyecto
- [ROADMAP.md](../ROADMAP.md): Roadmap del proyecto
- [PROJECT_STATE.md](../PROJECT_STATE.md): Estado actual del proyecto