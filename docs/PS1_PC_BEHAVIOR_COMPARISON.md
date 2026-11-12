# Comparativa de comportamiento: PC europeo vs PlayStation PAL

**Estado:** observaciones manuales del mantenedor; investigación técnica pendiente.  
**Referencia primaria del proyecto:** versión PC europea v1.00.000.  
**Referencia secundaria:** versión PlayStation PAL europea, probada por el mantenedor el 2026-11-12.  
**Alcance:** diferencias observadas en flujo de arranque, menú y música. No se han inspeccionado todavía los binarios ni las rutinas PS1 para explicar sus causas.

## 1. Modo DEMO desde el menú principal

### PlayStation PAL — observado

Si el jugador permanece inactivo en el **menú principal**, el juego entra automáticamente en un modo de demostración: carga escenarios y muestra el texto **DEMO MODE**, mientras Gex juega solo.

En la sesión de prueba se identificaron al menos estas demostraciones:
- Un nivel de temática Kung Fu.
- Un nivel de temática espacial.
- Una secuencia que parecía corresponder al combate del escenario de Gexzilla (nombre recordado por el mantenedor; identificación exacta pendiente).

El modo se activa por inactividad en el menú principal. **No** se observó que se active por dejar al personaje quieto durante el primer nivel jugable.

### PC europeo — observado

Al dejar el menú principal sin tocar controles, no se observó transición a una demostración: el menú permanece sin iniciar una partida ni cargar escenarios aleatorios.

### Investigación pendiente

- Localizar el temporizador/estado de inactividad del menú PS1 y la selección/ciclo de demos.
- Identificar exactamente los niveles y secuencias usados, su orden, duración y si la lista es cíclica.
- Comparar con el flujo de menú PC para confirmar si carece de esta función o si existe una ruta inaccesible.
- No confundir esta función de attract/demo mode con la cadena de vídeos de introducción ni con el primer nivel jugable.

**Nota de ingeniería inversa PC:** el ejecutable PC contiene la cadena «DEMO MODE» y una ruta de OSD asociada ya documentada en `REVERSE_ENGINEERING.md`; el estado asociado fue clasificado como vestigial/muerto por falta de lectores. Esto no demuestra que PC implemente el attract mode de PS1. La coincidencia de texto no debe tomarse como evidencia de equivalencia funcional.

## 2. Música: repetición al terminar una pista

### PlayStation PAL — observado

La música vuelve a empezar al llegar al final de la pista. En el primer nivel jugable (el que aparece **después** del menú principal, no el escenario/fondo del menú), el cambio entre el final y el comienzo resulta prácticamente imperceptible: el mantenedor describe el loop como perfecto.

### PC europeo — observado

En las pruebas del mantenedor, cuando termina una canción no vuelve a comenzar. Se observó tanto en la música del menú como en la de los niveles.

### Posible mejora del port

Registrar como candidato de mejora de comportamiento: permitir que la música ambiental/de nivel vuelva a empezar al terminar, con un punto de unión sin cortes audibles. Antes de implementarlo hay que investigar:
1. Si existe una corrección pública conocida y reutilizable.
2. Si el comportamiento se puede conseguir con la ruta de audio original o requiere un gestor de pistas.
3. Cómo determinar los límites exactos del loop y evitar clics, silencios o discontinuidades.
4. Si debe ser una opción de compatibilidad separada, ya que el comportamiento observado en PC original es de reproducción sin repetición y la referencia de fidelidad principal del proyecto sigue siendo PC.

### Pistas ya catalogadas (no verificadas como solución)

- `RESEARCH.md` § S-02 recoge un reporte comunitario de que `_inmm.dll` podría proporcionar música y loop mediante un wrapper WinMM. Es un reporte aislado; no se ha reproducido ni analizado técnicamente.
- `PATCH_ANALYSIS.md` § F-10 describe «Gex 3D: Enter The Gecko D3D/3DFX (Musics CD & Files Handler)», un launcher externo que declara soporte para WAV, CD, loop y cortes. El binario no ha sido inspeccionado y su aplicabilidad directa a la edición PC europea Glide no está demostrada.
- `docs/AUDIO_MIXER_RE.md` documenta el reproductor y el streaming del port. El loop de bajo nivel del buffer DirectSound no equivale por sí solo a repetir una pista musical completa: la repetición de la canción debe resolverse en la lógica de pistas/playlist o en el backend que corresponda.

**No implementar todavía:** primero investigar las soluciones existentes y definir explícitamente si el loop será una opción de comportamiento PS1 o una modificación del comportamiento PC por defecto.

## 3. Cinemática de introducción

### PlayStation PAL — observado

No aparece la cinemática/logotipo de Ubisoft observada en la edición PC europea. Tras el arranque se pasa directamente a la cinemática propia del juego en la que Gex realiza movimientos de Kung Fu.

### PC europeo — observado

La secuencia de arranque incluye la cinemática de Ubisoft antes de la cinemática propia del juego.

### Interpretación pendiente

Es posible que la diferencia esté relacionada con la edición, distribución o presentación regional de PC, pero no se ha verificado quién añadió o exigió la secuencia de Ubisoft ni por qué falta en PS1. No atribuirlo todavía a Ubisoft como distribuidora europea ni a una decisión concreta del fabricante.

## 4. Separación de evidencias

| Punto | PS1 PAL | PC europeo | Estado |
|---|---|---|---|
| Demo por inactividad en menú principal | Sí; varios escenarios con Gex jugando solo | No observado | Observación comparativa |
| Demo por inactividad durante el primer nivel | No observado | No observado | No confundir con el attract mode del menú |
| Loop de música al terminar | Sí; unión prácticamente imperceptible en el primer nivel | No; la pista termina y no reinicia en las pruebas | Observación comparativa; solución pendiente |
| Cinemática de Ubisoft | Ausente | Presente | Observación comparativa |
| Cinemática propia de Gex/Kung Fu | Presente | Presente después de la secuencia inicial | Observación comparativa |

Estas observaciones describen las dos versiones probadas, no todas las regiones, revisiones o builds. Las causas técnicas y la equivalencia exacta de contenidos quedan pendientes de verificación.

## 5. Próximas investigaciones

1. Audio: buscar y evaluar soluciones existentes para loop musical, empezando por las pistas S-02 y F-10 ya catalogadas; comprobar su compatibilidad con PC EU antes de decidir implementación.
2. PS1: investigar el controlador de attract mode, sus temporizadores, selección de niveles y secuencias de demostración.
3. Intro: comparar listas de vídeos y condiciones de reproducción entre las ediciones, sin asumir una causa editorial.
4. Mantener el comportamiento PC observado como baseline de fidelidad. Cualquier comportamiento tomado de PS1 debe quedar identificado como mejora/opción y no mezclarse silenciosamente con la reproducción exacta de PC.
