# Plantilla de Engineering Log

Copiar esta plantilla a `docs/engineering-log/ISSUE-<id>.md` por cada
problema relevante. Cumplimentar solo con evidencia; lo no sabido se marca.

```markdown
# Engineering Log — <I-XX corto>

## Problema
¿Qué ocurre?

## Reproducción
¿Cómo se reproduce? (versión, OS, GPU, wrapper, pasos exactos)

## Comportamiento original
¿Qué hace el juego sin modificar? (referencia: hardware de época si aplica)

## Investigación
¿Qué hemos encontrado? (fuentes, experimentos)

## Localización
¿Dónde está el comportamiento dentro del ejecutable?
(exe, offset/RVA, función, módulo)

## Evidencia
¿Qué demuestra que esa es la causa? (etiquetar: confirmado/inferido)

## Fix existente
¿Qué hacen las soluciones de terceros? (bytes, funciones, mecanismos)

## Hipótesis
¿Qué creemos que está ocurriendo? (marcar como NO confirmada)

## Solución propuesta
¿Qué podemos hacer? (alternativas + riesgos)

## Implementación
¿Qué se ha modificado? (commit, archivos)

## Verificación
¿Cómo hemos comprobado que funciona?
(comparar Original vs Fix existente vs REZengineered)

## Resultado
¿Qué ha ocurrido?

## Estado
- [ ] Pendiente / Experimental / Confirmado / Rechazado
```
