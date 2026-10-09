# PROJECT_STATUS — Metodología y registro del panel de progreso

> Fuente de verdad del panel visual [`PROJECT_STATUS.svg`](PROJECT_STATUS.svg).
> El SVG es una representación de estos datos; nunca una fuente independiente.
> Versión del panel: **v3.2** · Fecha: **2026-10-09** ·
> Ref: último commit con evidencia incorporada **`e9e846c`** (sin cambios;
> v3.2 actualiza datos (A2.2 wrapper=SÍ; A2 37%, global 18%): mismo diseño y metodología).

## 1. Propósito

Medir el avance real de REZengineered con cifras trazables a objetivos,
hitos y evidencias documentadas. Los porcentajes son **estimaciones de
avance documentadas**: no miden la calidad del juego ni predicen fechas.

Dos métricas independientes: **MAIN** = avance hacia la primera versión
jugable preparada para pruebas (§4.9); **progreso general** = avance del
conjunto de la hoja de ruta (§4.8). MAIN al 100% no implica proyecto
terminado, pulido al máximo ni distribución pública autorizada.
El panel prioriza la comprensión inmediata: MAIN protagonista y
progreso general en segundo plano.

## 2. Alcance del panel (5 áreas)

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

### Exclusiones N/A del panel (documentadas, no ocultas)

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
| A2.2 Ruta gráfica (FA-03) | glide2x cargada (M) ✓ · API efectiva (L) ✗ · Wrapper sí/no (L) ✓ SÍ | 8/13 | EN INVESTIGACIÓN | TESTING 2ª+5ª+6ª sesión; instalador nGlide→SysWOW64 |
| A2.3 Procedencia (RESEARCH) | Atribución origen (L) ✗ · Cadena/mecanismo (M) ✗ | 0/8 | EN INVESTIGACIÓN | Ficha A2.1 como indicio; corpus S-22; hipótesis |
| A2.4 Base decisiones (FA-01, Etapa A) | Inventario EU (L) ✓ · Variantes US/demo (M) ✗ · Confirm. dinámicas mín. (L) ✗ · Etapa B Ghidra (XL) ✗ BLOQUEADO | 5/21 | EN CURSO +BLOQ | Inventario §4; RE plan (B+ bloqueadas) |

Sin doble cuenta: A2.1 registra evidencias; A2.2/A2.3 registran
conclusiones sobre esas evidencias. Artefactos distintos. «API efectiva»
(A2.2) exige prueba a nivel de llamadas o efecto demostrable de un
ajuste del wrapper (TESTING 5ª sesión).

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

### 4.8 Cifras vigentes (2026-10-09, ref `e9e846c`)

| Área | Puntos | % |
|---|---|---|
| A1 Compatibilidad y diagnóstico | 20/62 | 32% |
| A2 Investigación técnica | 20/54 | 37% |
| A3 Mejoras gráficas | 0/69 | 0% |
| A4 Calidad de vida y UX | 0/64 | 0% |
| A5 Preparación para publicación | 12/36 | 33% |
| **GLOBAL** | **52/285** | **18%** |
| **MAIN (1ª jugable)** | **14/35 · 6/12 hitos** | **40%** |

Comprobación: 20+20+0+0+12=52 ✓ · 62+54+69+64+36=285 ✓ ·
52/285=18.25%→18% · 20/62=32.26%→32% · 20/54=37.04%→37% ·
12/36=33.33%→33%.
MAIN: 2+3+3+2+2+2=14 ✓ · total 35 ✓ · 14/35=40.0%→40% · hitos 6/12 ✓.

## 4.9 MAIN — primera versión jugable preparada para pruebas (14/35 → 40%)

**Definición.** MAIN mide el avance hacia una versión mínima que permite
probar el juego y las mejoras implementadas: arranque reproducible desde
una copia legítima, sesión básica de pruebas posible (controles, gráficos
y audio suficientes), incidencias críticas resueltas o aceptadas por
escrito, y documentación de instalación/validación/limitaciones. Es una
métrica independiente del progreso general, con su propio denominador.

**Fórmula.** La misma escala de §3 (XS1/S2/M3/L5/XL8) y reglas de §4.1:
`% MAIN = 100 × puntos completados / 35`, redondeo al entero. Se publica
además el conteo de hitos (6/12). MAIN reutiliza *evidencias* de las áreas
(referencias en la tabla) pero nunca sus puntos: sin doble cuenta dentro
de cada métrica. Investigar, planificar o documentar una solución no
completa un hito MAIN.

| ID | Hito (condición de aceptación) | Peso | Estado | Evidencia |
|---|---|---|---|---|
| MAIN-1 | Original intacto (MD5 del exe verificado, sin sustitución silenciosa) | S=2 | ✓ COMPLETADO | TESTING Hito 1 (MD5 re-confirmado) |
| MAIN-2 | Método de ejecución reproducible documentado (F-05 + imagen, entorno Win definido) | M=3 | ✓ COMPLETADO | TESTING Hito 1 (ejecutado 1 vez) |
| MAIN-3 | Arranque hasta sección jugable | M=3 | ✓ COMPLETADO | TESTING Hito 1 (nivel + movimiento) |
| MAIN-4a | Audio suficiente (música + SFX en config base) | S=2 | ✓ COMPLETADO | TESTING Hito 1 |
| MAIN-4b | Controles básicos (movimiento del personaje demostrado) | S=2 | ✓ COMPLETADO | TESTING Hito 1 (mapeo → limitación) |
| MAIN-4c | Gráficos suficientes (I-21 resuelto o aceptabilidad evaluada) | M=3 | ✗ PENDIENTE | I-21 sin causa ni evaluación |
| MAIN-5 | Críticos resueltos o aceptados por escrito (I-21/I-22) | L=5 | ✗ PENDIENTE | KNOWN I-21/22 sin causa |
| MAIN-6 | Guías de usuario (instalación, requisitos, validación, copias, restauración, desinstalación) | L=5 | ✗ PENDIENTE | Solo requisitos (BUILD) |
| MAIN-7 | Lista BYO explicada (qué aporta cada usuario) | S=2 | ✓ COMPLETADO | TESTING Hito 1 (copia + imagen) |
| MAIN-8 | Procedimiento de validación repetible + lista de limitaciones | M=3 | ✗ PENDIENTE | Checklist MAIN inexistente |
| MAIN-9 | Sin materiales protegidos en el entregable | S=2 | ✗ PENDIENTE | Verificable al empaquetar |
| MAIN-10 | Checklist de aceptación MAIN superado | M=3 | ✗ PENDIENTE | Checklist sin definir |

**Fuera del alcance de MAIN** (siguen en el progreso general): M-01…M-03
(config), M-04/M-05 (gráficos), M-06…M-11 (input moderno), M-12…M-17
(gráficos), M-18/M-19/M-20, M-21/M-22, M-23, M-24, M-25, M-26,
FA-04/05/07/08/15 en profundidad y RE Etapa B+
(más allá de lo que exija MAIN-5). El método de corrección de I-21/I-22 se
decidirá en Fase 3+; MAIN no presupone ninguna implementación concreta.

**100% MAIN = PRIMERA VERSIÓN JUGABLE PREPARADA PARA PRUEBAS.** No significa
proyecto terminado ni pulido máximo. Tres niveles distintos: (1) *jugable
para pruebas* (MAIN 100%); (2) *autorizada para distribución pública*
(requiere revisión legal, licencias y empaquetado: LICENSE §5–§6, fase
independiente); (3) *modernización completa* (toda la hoja de ruta). En el SVG (v3.1+)
esta leyenda se abrevia como etiqueta «VERSIÓN JUGABLE» bajo la barra
MAIN; el significado completo vive en este documento, no en el panel.

## 4.10 Zona de resumen del panel (contenido literal del SVG)

Actualizar estas líneas solo con hechos verificados (§6). Hipótesis, no.

**CONSEGUIDO**: Instalación + arranque + nivel / Audio base ·
ficha glide2x.
**EN INVESTIGACIÓN**: Wrapper: SÍ (¿nGlide?) / I-21 · I-22 ·
API efectiva.
**SIGUIENTE HITO**: Causa de 25 FPS estables (FA-05 · I-01/I-02);
A/B VSync ejecutado (nulo, 6ª); siguiente: medida precisa+pacing (TESTING 6ª).

## 5. Registro de cambios del panel

| Versión | Fecha | Commit | Cambio |
|---|---|---|---|
| v1.0 | 2026-10-09 | cbd7fab | Inicialización con datos Hito 1 (1ª–4ª sesión). Global 16%. |
| v2.0 | 2026-10-09 | 3e56f1c | Sección MAIN (40%, 14/35, 6/12 hitos) + resumen + nombres legibles. Global sin cambios (16%). |
| v3.0 | 2026-10-09 | 6ae7059 | Rediseño visual tarjeta: MAIN protagonista (40% gigante + barra ancha), global secundario, 5 mini-tarjetas, resumen compacto. Cifras idénticas. |
| v3.1 | 2026-10-09 | 87f33e3 | Ajustes: 40% MAIN reducido (124→68px), secundarias ampliadas (resumen 12px, áreas 26/9.5/9px), leyenda 100% sustituida por etiqueta VERSIÓN JUGABLE. Cifras idénticas. |
| v3.2 | 2026-10-09 | 6114f0b | Datos: A2.2 wrapper=SÍ (nGlide 2.10 + DLL 2.61 + render en HW moderno) → A2 37%, global 18%. Siguiente hito: causa 25 FPS. Mismo diseño. |

## 6. Procedimiento de actualización (Arena)

1. Leer este documento + los objetivos afectados por el último trabajo.
2. Identificar qué criterios de aceptación cambiaron REALMENTE (evidencia).
3. Actualizar estados y evidencias en las tablas (§4.3–§4.7, §4.9–§4.10).
4. Recalcular con §4.2 (verificar sumas como en §4.8).
5. Actualizar el SVG con los mismos datos (geometría abajo) + fecha/ref.
6. Comprobar coherencia SVG ↔ este doc ↔ MODERNIZATION_GOALS ↔ ROADMAP.
7. Registrar fecha, commit y fila en §5.
8. Dejar constancia de bloqueos y faltas de evidencia.

Actualizar solo ante cambios reales de estado; lo editorial no altera
cifras. Prohibido tocar pesos/criterios/alcance para inflar el progreso;
todo cambio metodológico se documenta con su impacto en cifras anteriores.

### Regla permanente del dashboard (adoptada 2026-10-09)

El SVG es el dashboard oficial y persistente. Actualizarlo SOLO ante
cambio real y verificable: hito completado con evidencia; cambio
justificado de % MAIN/global/área; cambio de estado documentado;
nuevo siguiente hito o dato resumido; corrección de un dato erróneo.
NO actualizar por revisar docs, analizar posibilidades, repetir
diagnósticos sin evidencia nueva, reorganizar tareas o iniciar sesión.
Investigar ≠ resolver; documentar ≠ solucionar. Al actualizar:
evidencia → SVG (solo elementos afectados) → MD → CHANGELOG →
revisión de coherencia. Sin cambios reales: SVG y MD intactos, sin
commits vacíos. Conservar el diseño aprobado (solo datos, barras,
estados y textos afectados); no rediseñar sin petición explícita.
Cada sesión informa si el dashboard cambió o no, y por qué.

### Geometría del SVG v3.2 (viewBox 0 0 760 700)

- Tarjeta: x=16 y=16 w=728 h=668. MAIN: «40%» 68px + stats
  (6/12 hitos, 14/35 puntos) + barra protagonista x=44 w=672 h=34 →
  relleno = % × 6.72 (40% → 268.8); ticks en x=212/380/548 (25/50/75%);
  etiqueta «VERSIÓN JUGABLE» bajo la barra (significado completo
  en §4.9, no en el SVG).
- Global secundario: «18%» 40px + barra x=140 w=400 h=16 →
  relleno = % × 4.0 (18% → 72) + «52/285 PTS».
- Mini-tarjetas de área: x=44/180/316/452/588, w=128 h=104;
  minibarra x+10 w=108 h=10 → relleno = % × 1.0
  (A1 32 · A2 37 · A3 0 · A4 0 · A5 33).
- Jerarquía tipográfica: cabecera 15px · MAIN 68px · global 40px ·
  % áreas 26px · stats 15px · secundarias 12/11/10.5px · micro 9.5/9px.
- Textos a actualizar: % + puntos MAIN/global/áreas, estado MAIN, fecha
  (UPD), referencia (REF), versión del panel, líneas del resumen.
- Con 0% no hay rect de relleno (añadirlo al superar 0, misma x/y/h).
- Colores: marco #7c3aed · texto #e9d5ff · dim #8b5cf6 ·
  verde #4ade80 · ámbar #fbbf24 · gris #6b7280 · fondos #050309/#0c0616.
