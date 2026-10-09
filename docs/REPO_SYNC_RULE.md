# REGLA PERMANENTE — Revisión y sincronización global del repositorio

> Adoptada 2026-10-08 por decisión del mantenedor. Se aplica
> automáticamente a toda tarea futura. Flujo obligatorio:
> **UNA MODIFICACIÓN → REVISIÓN GLOBAL → SINCRONIZACIÓN → COMMIT**.

## Principio

Tras cada cambio relevante (investigación, hallazgo, cambio de estado o
fase, hipótesis, herramienta, arquitectura, implementación, fix, prueba,
resultado experimental, objetivo o decisión técnica), no basta actualizar
el documento directamente relacionado: hay que revisar que **todo** lo
demás siga siendo coherente con el nuevo estado. La documentación debe
representar siempre un único estado coherente del proyecto.

## Documentos a revisar (mínimo)

`README.md`, `ROADMAP.md`, `PROJECT_STATE.md`, `CHANGELOG.md`,
`RESEARCH.md`, `REVERSE_ENGINEERING.md`, `PATCH_ANALYSIS.md`,
`COMPATIBILITY.md`, `KNOWN_ISSUES.md`, `TESTING.md`, `BUILD.md`,
`CREDITS.md`, `MODERNIZATION_GOALS.md`, `docs/*` y `tools/` si procede.
Comprobarlos no implica modificarlos: **si un documento sigue siendo
correcto, no tocarlo** (coherencia global, no commits innecesarios).

## Qué comprobar

- **Estado global:** fase/subfase actual; completado, en progreso,
  pendiente, bloqueado, deferred. `PROJECT_STATE.md` responde
  «¿dónde estamos?»; `ROADMAP.md` representa pasado/presente/futuro;
  `README.md` resume y enlaza (sin convertirse en documento técnico).
- **Evidencia:** CONFIRMED / HYPOTHESIS / EXPERIMENTAL /
  UNKNOWN-PENDING / REFERENCE. Nunca convertir una hipótesis en hecho
  por repetición. Si dos documentos discrepan sobre un estado o fase,
  corregirlos antes del commit.
- **Objetivos FA-/M-/X-:** si aparece uno nuevo, añadirlo donde
  corresponda + roadmap + estado + changelog + referencias cruzadas.
- **Issues I-XX:** si una investigación confirma, refuta o modifica uno,
  actualizar `KNOWN_ISSUES.md`, docs de investigación, estado y changelog.
- **RE / Compatibilidad / Testing:** nueva evidencia sobre ejecutables,
  renderer, audio, input, dependencias, formatos, Windows, DirectX,
  Glide, wrappers, GPUs… obliga a revisar los documentos
  correspondientes. Cada prueba registra qué/versión/config/resultado/
  evidencia/limitaciones/reproducibilidad.
- **Referencias cruzadas:** buscar restos del estado anterior (fases,
  números de objetivos, «desconocido» ya identificado, enlaces internos
  rotos, documentos que apuntan a ficheros inexistentes). Pregunta guía:
  *«¿existe alguna parte del repo que ahora sea falsa, incompleta,
  ambigua o contradictoria por este cambio?»*
- **CHANGELOG.md:** todo cambio relevante queda registrado de forma
  humana y útil (qué cambió y por qué), no como copia del commit.
- **Dashboard:** `docs/PROJECT_STATUS.svg` solo se actualiza ante
  cambio real y verificable (regla permanente en
  `docs/PROJECT_STATUS.md` §6); sin avance real, intacto.
- **Historia:** no reescribirla; conservar changelog y commits;
  distinguir hechos nuevos de históricos.

## Prohibido

- Avanzar de fase unilateralmente al documentar (registrar
  «inventario: COMPLETE» no implica «RE: STARTED»): las transiciones
  siguen el ROADMAP y decisiones explícitas del proyecto.

## Comprobación final obligatoria (antes de cada commit relevante)

- [ ] README actualizado · [ ] PROJECT_STATE actualizado ·
  [ ] ROADMAP actualizado · [ ] CHANGELOG actualizado ·
  [ ] objetivos sincronizados · [ ] issues sincronizados ·
  [ ] investigación sincronizada · [ ] testing sincronizado ·
  [ ] compatibility sincronizada · [ ] RE plan sincronizado ·
  [ ] referencias cruzadas comprobadas · [ ] enlaces internos comprobados ·
  [ ] estados CONFIRMED/HYPOTHESIS/UNKNOWN correctos ·
  [ ] no existen contradicciones conocidas

(Lo no aplicable se marca N/A. **Regla de oro:** si alguien clonara el
repo ahora y solo leyera la documentación, ¿obtendría una imagen
coherente y actual? Si no, no commitear todavía.)
