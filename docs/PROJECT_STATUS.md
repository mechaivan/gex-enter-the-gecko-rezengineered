# PROJECT_STATUS — Metodología y registro del panel de progreso

> Fuente de verdad del panel visual [`PROJECT_STATUS.svg`](PROJECT_STATUS.svg).
> El SVG es una representación de estos datos; nunca una fuente independiente.
> Versión del panel: **v1.0** · Fecha: **2026-10-09** ·
> Ref: último commit con evidencia incorporada **`e9e846c`**.

## 1. Propósito

Medir el avance real de REZengineered con cifras trazables a objetivos,
hitos y evidencias documentadas. Los porcentajes son **estimaciones de
avance documentadas**: no miden la calidad del juego ni predicen fechas.

## 2. Alcance del panel v1 (5 áreas)

| Código | Área | Cubre (IDs reales) |
|---|---|---|
| A1 | Compatibilidad y diagnóstico | FA-11, FA-12, FA-13, FA-14, FA-15, FA-09†, TESTING, COMPATIBILITY, I-15, I-16, I-21, I-22, F-05 |
| A2 | Investigación técnica | FA-01, FA-02, FA-03, RESEARCH (S-21…), inventario estático, Etapa A |
| A3 | Mejoras gráficas | M-04, M-12…M-17, FA-04, FA-05†, FA-07, cierre FA-13/FA-14 (Fase 4) |
| A4 | Calidad de vida y UX | M-01…M-03, M-05, M-06…M-11, M-18…M-20, M-26, FA-06, FA-10† |
| A5 | Preparación para publicación | BUILD.md, LICENSE.md §3–§6, hitos de publicación (ROADMAP) |

† Cobertura parcial explícita: FA-09 solo hito «audibilidad base» en A1.2
(mapeo profundo vía matriz A1.3); FA-05 y FA-10 profundos vía matriz A1.3;
FA-10 «identificar reproductor» en A4.1. Sin solapes: cada hito vive en un
solo objetivo (§4.4).

### Exclusiones N/A del panel v1 (documentadas, no ocultas)

Fuera del denominador hasta que el panel crezca (área 6 u ampliación):

- M-21, M-22 (modos Original/Modern: contenedores que se evalúan en Fase 11).
- M-23 (pack visual HD, Fase 12), M-24 (localización), M-25 (voces UK/USA).
- X-01, X-02 (DEFERRED: fuera del alcance actual por definición).
- FA-08 (ligado a M-25).

## 3. Escala de pesos (puntos por hito)

| Talla | XS | S | M | L | XL |
|---|---|---|---|---|---|
| Puntos | 1 | 2 | 3 | 5 | 8 |

Los pesos estiman carga/importancia del trabajo, no facilidad de cierre.
Peso de un objetivo = suma de sus hitos. No se cambian pesos para inflar
cifras (§7).

## 4. Metodología

### 4.1 Estados verificables

| Estado | Regla |
|---|---|
| COMPLETADO | Todos sus hitos cumplen criterios de aceptación con evidencia. |
| EN CURSO | Trabajo activo + ≥1 hito completado con evidencia. |
| EN INVESTIGACIÓN | 0 hitos completados pero existen indicios/información verificable. |
| PENDIENTE | Sin iniciar. |
| BLOQUEADO | No puede avanzar hasta resolver una dependencia (cuenta 0, se señaliza). |
| N/A | Fuera del alcance del panel (§2); excluido del denominador. |

Reglas duras: **investigar ≠ solucionar** (identificar una DLL no resuelve
el renderer); **documentar ≠ implementar** (un M-xx PROPOSED puntúa 0);
crédito parcial solo vía hitos definidos con evidencia. Estado de área =
estado «más activo» de sus objetivos (EN CURSO > EN INVESTIGACIÓN >
PENDIENTE); los bloqueos se señalizan aparte.

### 4.2 Fórmulas

```text
% área   = 100 × puntos_completados_área / puntos_totales_área
% global = 100 × puntos_completados_total / puntos_totales_total
```

Redondeo: al entero más próximo (mitades hacia arriba). Se publica siempre
la fracción de puntos junto al porcentaje (p. ej. `16% · 47/285 PTS`).
Prohibido derivar cifras de commits, ficheros, líneas o sesiones.

### 4.3 Área A1 — Compatibilidad y diagnóstico (20/62 → 32%)

| Obj. | Hitos (peso) ✓/✗ | Pts | Estado | Evidencia |
|---|---|---|---|---|
| A1.1 Instalación manual (FA-11, I-15, F-05) | Procedimiento documentado (S) ✓ · Valores `.reg` en ejecución (M) ✓ · Causa admin (S) ✗ · Ubicación guardado (S) ✗ | 5/9 | EN CURSO | TESTING Hito 1; PATCH F-05 |
| A1.2 Arranque + básico (FA-12, FA-09†, I-16) | Arranca con imagen (M) ✓ · Nivel + movimiento (M) ✓ · Música/SFX base (S) ✓ · Comportamiento sin CD (S) ✗ | 8/10 | EN CURSO | TESTING Hito 1 |
| A1.3 Matriz compatibilidad | Protocolo + matriz (M) ✓ · Specs PC (S) ✗ · Filas P0 restantes (L) ✗ · Filas P1 (XL) ✗ | 3/18 | EN CURSO | TESTING matriz; COMPATIBILITY |
| A1.4 Pantalla diagnosticada (I-21/22, FA-13/14) | I-21 observado ×2 (S) ✓ · I-22 observado ×2 (S) ✓ · Causa I-21 (M) ✗ · Causa I-22 (M) ✗ | 4/10 | EN INVESTIGACIÓN | TESTING Hito 1 + 2ª sesión; KNOWN I-21/22 |
| A1.5 Referencia de época (FA-15) | Decisión HW (S) ✗ · Montaje (L) ✗ · Capturas referencia (XL) ✗ | 0/15 | PENDIENTE | TESTING vía C (sin HW) |

### 4.4 Área A2 — Investigación técnica (15/54 → 28%)

| Obj. | Hitos (peso) ✓/✗ | Pts | Estado | Evidencia |
|---|---|---|---|---|
| A2.1 DLLs cargadas (FA-02) | Enumeración 32-bit (S) ✓ · Ficha glide2x (S) ✓ · Ficha 3dfxSpl2 (XS) ✓ · Fecha/firma (S) ✓ · Rol acompañantes (S) ✗ · Resto deps dinámicas (M) ✗ | 7/12 | EN CURSO | TESTING 2ª–4ª sesión |
| A2.2 Ruta gráfica (FA-03) | glide2x cargada (M) ✓ · API efectiva (L) ✗ · Wrapper sí/no (L) ✗ | 3/13 | EN INVESTIGACIÓN | TESTING 2ª sesión |
| A2.3 Procedencia (RESEARCH) | Atribución origen (L) ✗ · Cadena/mecanismo (M) ✗ | 0/8 | EN INVESTIGACIÓN | Ficha A2.1 como indicio; RESEARCH hipótesis |
| A2.4 Base decisiones (FA-01, Etapa A) | Inventario EU (L) ✓ · Variantes US/demo (M) ✗ · Confirm. dinámicas mín. (L) ✗ · Etapa B Ghidra (XL) ✗ BLOQUEADO | 5/21 | EN CURSO +BLOQ | Inventario §4; RE plan (B+ bloqueadas) |

Sin doble cuenta: A2.1 registra evidencias; A2.2/A2.3 registran
conclusiones sobre esas evidencias. Artefactos distintos.

### 4.5 Área A3 — Mejoras gráficas (0/69 → 0%)

Nada implementado (Fase 1). Todo PENDIENTE.

| Obj. | Hitos (peso) | Pts |
|---|---|---|
| A3.1 Resolución + panorámico (M-04, M-15, M-16) | Diseño (L) ✗ · Impl. (XL) ✗ · Verificación vs original (M) ✗ | 0/16 |
| A3.2 Cámara/FOV (M-12, M-17, FA-07) | Comportamiento original (M) ✗ · Diseño (L) ✗ · Impl.+verif. (XL) ✗ | 0/16 |
| A3.3 Render desacoplado (M-13, M-14, FA-04/05) | FA-04/05 entendidos (XL) ✗ · Diseño (L) ✗ · Impl. (XL) ✗ | 0/21 |
| A3.4 Corrección I-21/22 (Fase 4, FA-13/14) | Diseño (L) ✗ · Impl. (XL) ✗ · Verificado sin regresión (M) ✗ | 0/16 |

Correspondencia: A3.4 no tiene M-xx; mapea al ítem real «Fase 4: cerrar
FA-13/FA-14 con verificación» (ROADMAP). Depende de las causas de A1.4.

### 4.6 Área A4 — Calidad de vida y UX (0/64 → 0%)

Nada implementado (Fase 1). Todo PENDIENTE.

| Obj. | Hitos (peso) | Pts |
|---|---|---|
| A4.1 Intro omisible (M-26, FA-10) | Reproductor/códec (M) ✗ · Diseño compatible exe intacto (M) ✗ · Impl.+verif. (L) ✗ | 0/11 |
| A4.2 Configuración (M-01/02/03, M-20) | Diseño (L) ✗ · Impl. (XL) ✗ · Menú en juego (XL) ✗ | 0/21 |
| A4.3 Input (M-06…M-11, FA-06) | Comportamiento original (M) ✗ · Diseño (L) ✗ · Impl.+verif. (XL) ✗ | 0/16 |
| A4.4 Ventana/multi (M-05, M-18, M-19) | Alt+Tab robusto (L) ✗ · Borderless+multi/DPI (XL) ✗ · Verificado (M) ✗ | 0/16 |

### 4.7 Área A5 — Preparación para publicación (12/36 → 33%)

Solo puntúan entregables de planificación/documentación que EXISTEN;
verificaciones y revisiones siguen pendientes. Nada implementado.

| Obj. | Hitos (peso) ✓/✗ | Pts | Estado | Evidencia |
|---|---|---|---|---|
| A5.1 Docs instalación (BUILD) | Requisitos (M) ✓ · Guías escritas (L) ✗ · Guías verificadas (M) ✗ | 3/11 | EN CURSO | BUILD.md §distribución |
| A5.2 Copias/restauración (BUILD) | Requisitos (S) ✓ · Procedimiento verificado (M) ✗ | 2/5 | EN CURSO | BUILD.md §distribución |
| A5.3 Separación propio/original | Principio (S) ✓ · Diseño conforme (M) ✗ | 2/5 | EN CURSO | BUILD.md; LICENSE §3 |
| A5.4 Legal/avisos (LICENSE) | Licencias doc. (S) ✓ · Aviso no-afiliación (XS) ✓ · Evaluación método (M) ✗ · Confirmación idoneidad (S) ✗ | 3/8 | EN CURSO | LICENSE §1–§6; README |
| A5.5 Repo/historial | Checklist (S) ✓ · Auditoría ejecutada (M) ✗ · Limpieza si procede (S) ✗ | 2/7 | EN CURSO | LICENSE §6 |

### 4.8 Cifras v1.0 (2026-10-09, ref `e9e846c`)

| Área | Puntos | % |
|---|---|---|
| A1 Compatibilidad y diagnóstico | 20/62 | 32% |
| A2 Investigación técnica | 15/54 | 28% |
| A3 Mejoras gráficas | 0/69 | 0% |
| A4 Calidad de vida y UX | 0/64 | 0% |
| A5 Preparación para publicación | 12/36 | 33% |
| **GLOBAL** | **47/285** | **16%** |

Comprobación: 20+15+0+0+12=47 ✓ · 62+54+69+64+36=285 ✓ ·
47/285=16.49%→16% · 20/62=32.26%→32% · 15/54=27.78%→28% ·
12/36=33.33%→33%.

## 5. Registro de cambios del panel

| Versión | Fecha | Commit | Cambio |
|---|---|---|---|
| v1.0 | 2026-10-09 | (este commit) | Inicialización con datos Hito 1 (1ª–4ª sesión). Global 16%. |

## 6. Procedimiento de actualización (Arena)

1. Leer este documento + los objetivos afectados por el último trabajo.
2. Identificar qué criterios de aceptación cambiaron REALMENTE (evidencia).
3. Actualizar estados y evidencias en las tablas (§4.3–§4.7).
4. Recalcular con §4.2 (verificar sumas como en §4.8).
5. Actualizar el SVG con los mismos datos (geometría abajo) + fecha/ref.
6. Comprobar coherencia SVG ↔ este doc ↔ MODERNIZATION_GOALS ↔ ROADMAP.
7. Registrar fecha, commit y fila en §5.
8. Dejar constancia de bloqueos y faltas de evidencia.

Actualizar solo ante cambios reales de estado; lo editorial no altera
cifras. Prohibido tocar pesos/criterios/alcance para inflar el progreso;
todo cambio metodológico se documenta con su impacto en cifras anteriores.

### Geometría del SVG (viewBox 0 0 760 600)

- Barras de área: pista x=230 w=330 → relleno = % × 3.3
  (A1 105.6 · A2 92.4 · A3 0 · A4 0 · A5 108.9).
- Barra global: pista x=28 w=532 → relleno = % × 5.32 (16% → 85.1).
- Textos a actualizar: 6 porcentajes, 6 fracciones de puntos, fecha (UPD),
  referencia (REF), versión del panel si cambia la metodología.
- Con 0% no hay rect de relleno (añadirlo al superar 0, misma x/y/h).
- Colores: marco #7c3aed · texto #e9d5ff · dim #8b5cf6 ·
  verde #4ade80 · ámbar #fbbf24 · gris #6b7280 · fondo #07030d.
