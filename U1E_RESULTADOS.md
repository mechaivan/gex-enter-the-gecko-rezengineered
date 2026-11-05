# RESULTADOS DE U-1e (RELACIÓN GEOMETRÍA-ÍNDICES) (2026-11-04)

> **NOTA DE CORRECCIÓN (U-1f, 2026-11-05).** Este documento se conserva como
> historial, pero varias de sus afirmaciones quedaron desmentidas por el
> desensamblado completo de U-1f. En concreto:
>
> * No existe un «stride de 12 bytes»: el `imul ...,0xC` de `0x00432E1B` es la
>   altura en píxeles de un rectángulo.
> * No existe un «cálculo de w»: se escribe la constante `1.0f` en `vértice+0x20`
>   (campo no verificado) y el `+8` es la altura/ancho del quad.
> * `0x00432BBD` y `0x00432DD1` **no son rutas 2D y 3D**: son un glifo de texto y
>   un rectángulo sólido, ambas 2D/HUD. La geometría 3D está en `0x0046558A`.
> * Las hipótesis H1 y H6 quedan **retiradas**.
>
> Evidencia y correcciones completas: [`U1F_RESULTADOS.md`](U1F_RESULTADOS.md).

## Resumen ejecutivo

Se ha completado el análisis de U-1e (relación geometría-índices). Se ha analizado cómo se relacionan los vértices con los índices, se ha determinado si los argumentos son coordenadas directas o índices a tablas, se ha investigado la relación entre los datos de entrada y los vértices finales, y se han documentado todas las evidencias y conclusiones.

---

## 1. Análisis de relación vértices-índices

### Hechos demostrados

**Evidencias de relación:**
- Los 3 argumentos se usan para generar 4 vértices
- No hay secuencia de índices independiente
- Los triángulos se construyen directamente a partir de los argumentos

**Hipótesis sobre la relación:**
- Los argumentos podrían ser coordenadas directas
- Los argumentos podrían ser índices a tablas de coordenadas
- Los argumentos podrían definir una geometría paramétrica

**Análisis de las conversiones:**
- Los argumentos se procesan con operaciones específicas
- El procesamiento varía entre 2D y 3D
- Los resultados se pasan a grDrawTriangle

### Conclusión

- **Los vértices se generan directamente a partir de los argumentos**
- No hay una secuencia de índices intermediaria
- La relación depende de la interpretación de los argumentos

---

## 2. Análisis de tipo de datos de los argumentos

### Hechos demostrados

**Evidencias a favor de coordenadas directas:**
- Tres campos de 4 bytes cada uno
- Separación de 4 bytes entre campos
- Uso típico en gráficos 3D para coordenadas
- Conversión a flotante en el código

**Evidencias a favor de índices a tablas:**
- Multiplicación por stride (0xc) en 0x00432DD1
- Común en estructuras de datos indexadas
- Podría indicar acceso a tablas de datos

**Análisis del procesamiento:**
- En 0x00432BBD: procesamiento con operaciones bit a bit
- En 0x00432DD1: procesamiento con operaciones de desplazamiento
- Ambos realizan conversiones a flotante

### Conclusión

- **Hay evidencia mixta que apunta a diferentes interpretaciones**
- La multiplicación por stride sugiere índices u offsets
- El procesamiento a flotante sugiere coordenadas
- Se necesita más análisis para determinar la interpretación exacta

---

## 3. Análisis de relación datos de entrada-vértices finales

### Hechos demostrados

**Proceso en 0x00432BBD (2D/HUD):**
- Se reciben 3 argumentos
- Se procesan con operaciones bit a bit
- Se calcula w = 1.0f
- Se preparan 4 vértices en la pila
- Se llama a grDrawTriangle 2 veces

**Proceso en 0x00432DD1 (3D):**
- Se reciben 3 argumentos
- Se procesan con operaciones de desplazamiento
- Se calcula w = valor + 8.0f
- Se preparan 4 vértices en la pila
- Se llama a grDrawTriangle 2 veces

**Análisis de la relación:**
- Los 3 argumentos se usan para generar 4 vértices
- Cada vértice se construye a partir de los argumentos
- No hay evidencia de acceso a datos externos durante la construcción

**Hipótesis sobre la relación:**
- Los argumentos podrían definir un punto y dos direcciones
- Podrían definir un plano o una superficie
- Los 4 vértices podrían derivarse de estos 3 valores

### Conclusión

- **Los vértices finales se generan directamente a partir de los argumentos**
- No hay evidencia de dependencia de datos externos durante la construcción
- La relación depende de la interpretación de los argumentos

---

## 4. Documentación de evidencias y conclusiones

### Resumen de evidencias clave

1. **Evidencias de relación vértices-índices:**
   - Los 3 argumentos se usan para generar 4 vértices
   - No hay secuencia de índices independiente
   - Los triángulos se construyen directamente a partir de los argumentos

2. **Evidencias de tipo de datos:**
   - Tres campos de 4 bytes cada uno (float)
   - Separación de 4 bytes entre campos
   - Multiplicación por stride (0xc) en 0x00432DD1

3. **Evidencias de relación datos de entrada-vértices finales:**
   - Los vértices se generan directamente a partir de los argumentos
   - No hay dependencia de datos externos durante la construcción
   - El procesamiento varía entre 2D y 3D

### Conclusiones

- **Los vértices se generan directamente a partir de los argumentos**
- No hay una secuencia de índices intermediaria
- La relación depende de la interpretación de los argumentos
- Hay evidencia mixta que apunta a diferentes interpretaciones

### Indicios no confirmados

- Los tres campos de 4 bytes podrían ser valores flotantes
- Podrían ser coordenadas directas o índices a tablas
- No hay evidencia concluyente sobre la naturaleza exacta de los datos

### Próximos pasos

- **U-1f (relación índices-recursos):** Analizar cómo se relacionan los índices con los recursos (texturas, etc.)
- Investigar la relación con archivos DFX
- Determinar la interpretación exacta de los argumentos

---

## Conclusión

### Lo que sabemos realmente:

1. **Relación vértices-índices:**
   - Los vértices se generan directamente a partir de los argumentos
   - No hay una secuencia de índices intermediaria
   - Los triángulos se construyen directamente

2. **Tipo de datos de los argumentos:**
   - Hay evidencia mixta que apunta a diferentes interpretaciones
   - La multiplicación por stride sugiere índices u offsets
   - El procesamiento a flotante sugiere coordenadas

3. **Relación datos de entrada-vértices finales:**
   - Los vértices finales se generan directamente a partir de los argumentos
   - No hay dependencia de datos externos durante la construcción
   - La relación depende de la interpretación de los argumentos

### Lo que permanece desconocido:

1. **Interpretación exacta de los argumentos:**
   - ¿Son realmente coordenadas directas?
   - ¿O son índices a tablas de coordenadas?
   - ¿O definen una geometría paramétrica?

2. **Relación con archivos DFX:**
   - ¿Cómo se relacionan los argumentos con los archivos DFX?
   - ¿Contienen estos archivos los datos originales?

3. **Uso de tablas de datos:**
   - ¿Existen tablas de coordenadas predefinidas?
   - ¿Cómo se cargan desde archivos DFX?

### Próximo paso técnico

**U-1f (relación índices-recursos):** Analizar cómo se relacionan los índices con los recursos (texturas, etc.).

1. **Objetivo:** Comprender la relación entre los argumentos y los recursos del juego
2. **Enfoque:** Investigar cómo se asignan texturas y otros recursos
3. **Método:** Analizar las referencias a funciones de recursos
4. **Herramientas:** Mantener el enfoque en el desensamblado dirigido
5. **Evidencia:** Documentar todas las instrucciones y conversiones implicadas

---

**Fin del documento.**