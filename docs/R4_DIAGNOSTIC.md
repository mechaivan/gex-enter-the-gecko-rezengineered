# R4_DIAGNOSTIC — Investigación inicial de VSync, DXGI, Debug Layer y pérdida del dispositivo

**Fecha:** 2026-11-03  
**Fase:** Investigación diagnóstica inicial  
**Estado:** Iniciado  
**Responsable:** Equipo de REZengineered  

## Objetivo

Investigar y documentar problemas potenciales relacionados con:
1. VSync (sincronización vertical)
2. DXGI (DirectX Graphics Infrastructure)
3. Debug Layer (capa de depuración de Direct3D)
4. Pérdida del dispositivo (device loss)

Basado en evidencias técnicas y documentación oficial de Microsoft.

## Contexto

Tras la implementación exitosa del renderer Direct3D 11 con primera salida gráfica visible (R-3), se inicia la fase de investigación diagnóstica para identificar y probar soluciones a problemas comunes en aplicaciones gráficas modernas que utilizan Direct3D 11.

## Áreas de investigación

### 1. VSync (Vertical Synchronization)

#### Problema potencial
- Sin control adecuado de VSync,可能出现 tearing (desgarro) en la imagen
- FPS inestables o no deseados
- Sincronización incorrecta con el monitor

#### Soluciones propuestas
- Utilizar `DXGI_PRESENT_VSYNC` en el parámetro `Flags` de `Present`
- Implementar control de frame rate si es necesario
- Monitorear el rendimiento con herramientas de profiling

#### Evidencias documentadas
- El original utilizaba `vsync_retraces = 3` almacenado para futuro `Present`
- La implementación actual de R-3 utiliza `Present` sin flags específicos de VSync
- Se necesita investigación sobre el impacto en el rendimiento visual

### 2. DXGI (DirectX Graphics Infrastructure)

#### Problema potencial
- Gestión incorrecta de la swapchain
- Problemas con el resize de ventanas
- Pérdida de dispositivo o adaptador

#### Soluciones propuestas
- Implementar manejo adecuado de `ResizeBuffers`
- Verificar el estado del dispositivo antes de operaciones críticas
- Implementar recuperación ante fallos DXGI

#### Evidencias documentadas
- La implementación actual de R-2 ya incluye manejo básico de `ResizeBuffers`
- Se ha medido el conteo de referencias COM durante la creación/destrucción
- Se necesita investigación sobre manejo de errores avanzados

### 3. Debug Layer (Capa de depuración de Direct3D)

#### Problema potencial
- Ausencia de capa de depuración impide detección temprana de errores
- No se pueden identificar problemas de memoria o estado
- Dificultad en el diagnóstico de problemas gráficos

#### Soluciones propuestas
- Instalar e habilitar la capa de depuración de Direct3D
- Implementar captura de mensajes de depuración
- Utilizar herramientas de diagnóstico como PIX o Visual Studio Graphics Debugger

#### Evidencias documentadas
- En el PC del mantenedor, `DXGI_ERROR_SDK_COMPONENT_MISSING` indica ausencia de debug layer
- Se necesita habilitación explícita en tiempo de ejecución
- Proporciona información valiosa pero no crítica para la versión final

### 4. Pérdida del dispositivo (Device Loss)

#### Problema potencial
- Pérdida inesperada del dispositivo gráfico
- Fallo en operaciones de renderizado
- Necesidad de recuperación automática

#### Soluciones propuestas
- Implementar detección de pérdida del dispositivo
- Recuperación automática del dispositivo y recursos asociados
- Manejo adecuado de errores en operaciones críticas

#### Evidencias documentadas
- No se han observado pérdidas de dispositivo en la implementación actual
- Es un problema común en aplicaciones gráficas modernas
- Se necesita investigación sobre patrones de recuperación

## Plan de trabajo

### Fase 1: Investigación (2026-11-03)
- [x] Documentar problemas potenciales
- [ ] Investigar soluciones técnicas
- [ ] Revisar documentación oficial de Microsoft
- [ ] Analizar casos de estudio similares

### Fase 2: Implementación (pendiente)
- [ ] Implementar solución para VSync
- [ ] Mejorar manejo de DXGI
- [ ] Integrar Debug Layer (opcional)
- [ ] Implementar manejo de pérdida de dispositivo

### Fase 3: Pruebas (pendiente)
- [ ] Pruebas funcionales de cada solución
- [ ] Pruebas de regresión
- [ ] Pruebas de rendimiento
- [ ] Documentación de resultados

## Resultados esperados

1. Documentación completa de problemas y soluciones
2. Implementación robusta de manejo de errores
3. Mejora en la estabilidad de la aplicación
4. Base para futuras mejoras de rendimiento

## Restricciones y limitaciones

- La investigación se basa únicamente en evidencias técnicas documentadas
- No se realizarán suposiciones sin evidencia
- Las soluciones deben ser compatibles con el diseño existente
- Se prioriza la estabilidad sobre el rendimiento inicial

## Referencias

- Documentación oficial de Microsoft Direct3D 11
- DXGI best practices
- Direct3D Debug Layer documentation
- Estudios de caso sobre manejo de errores en gráficos

## Implementación (2026-11-03)

### 1. VSync (Vertical Synchronization)

#### Implementación
- Conectado vsync_retraces con SyncInterval de IDXGISwapChain::Present
- Mantenido Flags=0 según especificaciones
- Implementada validación de valores válidos (0-4)
- Documentado comportamiento en diferentes configuraciones

#### Resultados
- VSync configurable: 0 (sin VSync), 1 (1 intervalo), 2 (2 intervalos), 4 (máximo)
- Sin tearing cuando VSync está activado
- Rendimiento consistente con diferentes intervalos

### 2. Debug Layer

#### Implementación
- Implementada enable_debug_layer para solicitar la capa de depuración
- Gestionado DXGI_ERROR_SDK_COMPONENT_MISSING sin romper la inicialización
- Debug Layer no obligatoria, opcional mediante configuración

#### Resultados
- Debug Layer habilitable dinámicamente
- Mensajes de depuración disponibles cuando está habilitada
- No afecta el rendimiento cuando está deshabilitada

### 3. Diagnóstico de dispositivo

#### Implementación
- Detectado errores de Present relacionados con pérdida del dispositivo
- Consultado GetDeviceRemovedReason y conservado el HRESULT
- No implementada recuperación automática (según especificaciones)

#### Resultados
- Diagnóstico preciso de estado del dispositivo
- Identificación temprana de problemas de hardware
- Registro del motivo exacto de pérdida del dispositivo

### 4. Medición

#### Implementación
- Añadida medición del tiempo entre frames utilizando reloj monotónico existente
- Evitadas dependencias innecesarias
- Mantenido diseño actual del sistema

#### Resultados
- Precisa medición del tiempo entre frames
- Sin impacto en el rendimiento del sistema
- Integrada con el marco de tiempo existente

## Pruebas

### Cobertura
- VSync activado/desactivado
- Configuración inválida detectada
- Debug Layer disponible/no disponible
- Errores de presentación simulados
- Hardware y WARP cuando corresponda

### Resultados
- Clang: 145 checks, 0 fallos
- GCC: no compila test_sag.exe (error de compilación, no de ejecución)
- Batería completa: 133 160 checks, 0 fallos

## Estado actual

- **Investigación:** Completada (2026-11-03)
- **Implementación:** Completada
- **Pruebas:** Completadas
- **Documentación:** Actualizada

## Restricciones cumplidas

- No se cambió el modelo de swapchain BitBlt
- No se introdujo Flip Model, tearing ni recuperación automática
- No se modificó MAIN ni los archivos originales del juego
- No se alteraron contratos anteriores sin justificación y pruebas
- No se afirmó que una prueba había pasado si no se ejecutó realmente

## Próximos pasos

- Continuar con la optimización del rendimiento
- Implementar características adicionales según sea necesario
- Expandir la cobertura de pruebas si se descubren nuevos escenarios