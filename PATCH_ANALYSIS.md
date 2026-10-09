# PATCH_ANALYSIS — Catálogo y análisis de fixes existentes

> **Nivel de evidencia global: DESCRIPCIONES VERIFICADAS, BINARIOS SIN ANALIZAR.**
> Lote 1 (2026-10-09): F-01…F-09 investigados documentalmente en sus fuentes
> + F-10…F-12 catalogados. Ningún interno (funciones/offsets/instrucciones)
> está confirmado: ese análisis es trabajo de las Fases 1–2.

## 0. Taxonomía (categorías estrictamente separadas)

| Código | Categoría | Qué es | Ejemplos |
|---|---|---|---|
| — | **ORIGINAL** | El juego PC de 1998 y sus variantes retail/demo | US/EU/demo (ver COMPATIBILITY.md) |
| F-xx | **FIX comunitario** | Parches, exes modificados, procedimientos y paquetes creados para Gex PC | F-01…F-12 |
| T-xx | **WRAPPER / HERRAMIENTA** | Wrappers y utilidades genéricas (no específicas de Gex) | T-01…T-06 |
| R-xx | **REFERENCIA otra versión** | Proyectos/material de otras versiones de Gex | R-01, R-02 |

Reglas:

- Nada de lo catalogado aquí se asume como solución final de REZengineered.
- El objetivo posterior es estudiar qué soluciona cada recurso, qué cambia y
  por qué — y solo después decidir la implementación propia.
- No descargar ni aplicar nada ciegamente: primero determinar qué hace cada
  fix y por qué funciona. Comparar siempre
  **Original → Fix comunitario → REZengineered**.
- Estado por fix: `DESCRIPCIÓN VERIFICADA` = texto/fuente leídos de primera
  mano; ningún fix tiene binario inspeccionado por el proyecto.

## F-01 — Unofficial Direct3D Patch (PAL) — tgames.fr

- **Fuente verificada (leída íntegra 2026-10-09):**
  <https://www.tgames.fr/pc/progs-pc/patch-patch-direct-3d-gex-3d-enter-the-gecko-windows-7-8-10-t12116.html>
  (OP 2024-11-10 + 6 respuestas). Página 2018 original:
  <https://tgames.fr/progs-pc/patch-direct-3d-gex-3d-enter-the-gecko-windows-7-8-10-t12116.html>.
- **Autor/versiones:** Tgames (admin); conversión a EU atribuida a una persona
  anónima («thanks to the person who converted this to European»). Historial
  publicado: V1.0 (2018-03) → V1.1.1 Beta (registro interno, voces sin
  modificar carpetas, «still missing some voices») → V1.2.0 (2018-04-08,
  «Support de toutes les voix originales UK»; fichero
  `GEX3DFX_PatchD3D_V1.2.zip` citado en el foro AF) → «V1.1 controller sound
  fix» (2025-08-24, numeración ambigua respecto a la serie 2018).
- **Versión objetivo:** PAL/UK (EU 3DFX → D3D): título «(PAL) for
  Nvidia/ATI cards (Windows XP)»; el parche valida la versión («wrong
  version» en copias no-EU) y busca la carpeta `voice` (la copia EU válida
  usa `voiceuk` — ver F-03).
- **Problema que dice solucionar:** añadir render Direct3D a versiones no-US
  (I-03). Efectos declarados: No-CD, soporte de tarjetas DX3D, sin wrapper,
  Win 95–7 (texto 2018); en Win 10/11 usar dentro de la carpeta `gex23d`
  (versión D3D) + ficheros de `gex3d_windows10.zip` (PCGW).
- **Archivos/hashes:** no publicados. Descargas solo para miembros (contenido
  no inspeccionado). Indicio sin confirmar: el hilo menciona «Lunar IPS
  patcher» (posible patcher estilo IPS sobre 1 de 2 ficheros).
- **Modificación exacta:** UNKNOWN. El hilo incluye dgVoodoo2 v2.82.4
  standalone como alternativa de prueba (no parte del parche).
- **Resultados publicados:** un reporte de éxito («Patch works, checked at
  the first level» + controller sound fix instalado) y un reporte de error
  de validación en copia no-EU. Sin detalle de Windows/GPU.
- **Compat/límites:** sin probar por nosotros; efectos secundarios UNKNOWN.
- **Relación EU v1.00.000:** aplicable a la familia EU (misma edición que el
  inventario Fase 1); coherente con el estático (el exe EU carece de imports
  D3D: F-01 añade una ruta inexistente de fábrica).
- **Preguntas RE:** ¿exe reemplazado o patcher? ¿qué imports D3D añade? ¿cómo
  resuelve el CD-audio (I-11)? ¿mapeo de voces UK? ¿por qué cap 24 FPS?
- **Estado:** DESCRIPCIÓN VERIFICADA, binarios no inspeccionados.

## F-02 — nGlide + `gex2_patch.zip` — Zeus Software

- **Fuente verificada (leída 2026-10-09):**
  <https://www.zeus-software.com/downloads/nglide/compatibility> — fila
  «Gex 2: Enter The Gecko»: «To play you must replace 'gex3d.exe' and
  install patch from here» → `gex2_patch.zip`. Estado: jugable.
- **Autor/versión:** Zeus Software (autor de nGlide); sin versión/fecha en la
  fila; descarga pública directa (no descargada por el proyecto).
- **Versión objetivo:** no indicada en la fila (presumiblemente 3DFX EU/US;
  pendiente de binario).
- **Problema que dice solucionar:** compatibilidad Glide en GPUs modernas
  (I-05).
- **Archivos/hashes:** no publicados en la página. Payload confirmado por F-07:
  el zip contiene un `GEX3D.exe` de reemplazo (extraer y sobrescribir en la
  raíz del juego).
- **Modificación exacta:** UNKNOWN. Notas legacy del foro Zeus (S-02, no
  releídas en Lote 1): el juego pide 75 Hz; reportes de «wrong drawing» y
  «pure virtual function call».
- **Resultados publicados:** ninguno en la compatibility list.
- **Relación EU v1.00.000:** pendiente (determinar contra qué edición valida).
- **Preguntas RE:** diff del exe reemplazado; ¿qué renderer paths toca?
  ¿incluye ya un cap de FPS (ver F-04)?
- **Estado:** DESCRIPCIÓN VERIFICADA, binario no inspeccionado.

## F-03 — 3DFX FPS Limiter (EU/US) — tgames.fr

- **Fuente verificada (leída íntegra 2026-10-09):**
  <https://www.tgames.fr/pc/progs-pc/patch-patch-3dfx-fps-limiter-gex-3d-enter-the-gecko-t12190.html>
  (OP Tgames 2025-08-20 18:54 + 1 respuesta «Merci!» de AMJ, 2026-10-05).
- **Versión objetivo:** EU y US (3DFX). Detector documentado verbatim: en
  `…\gex23dfx\audio`, carpeta `voiceuk` = EU, `voice` = US. «Exclu Tgames.fr
  pour la version EU (UK)!».
- **Problema que dice solucionar:** velocidad excesiva en sistemas rápidos
  (I-01): «This patch add a FPS lock on the game».
- **Alcance declarado:** «It's only for 3DFX Cards systems» y «only applies
  to older Windows (98/ME/2000/XP)»; para DirectX/Win 10-11 remite a F-01.
- **Archivos/hashes:** 2× `GEX3D.EXE` (EU, US) para reemplazar en la carpeta
  del juego; solo miembros; hashes no publicados. (El paso 2 del OP cita
  `…\gex23dfx\audio` como destino del exe — probable errata: el exe va en
  la raíz; la ruta audio es donde viven `voice`/`voiceuk`.)
- **Modificación exacta:** UNKNOWN (mecanismo y FPS objetivo sin declarar).
  Cita legacy «runs smooth and at normal speed» (procedencia pendiente de
  re-verificación). PCGamingWiki tiene sección FPS Limiter que enlaza este
  fix (verificado en Lote 1).
- **Resultados publicados:** ninguno con datos (solo «Merci!»).
- **Relación EU v1.00.000:** incluye exe EU → candidato aplicable; pendiente
  de binario.
- **Preguntas RE:** ¿mecanismo (sleep/hook/bucle)?, ¿a qué FPS limita?, ¿los
  exes EU/US difieren solo en voces?
- **Estado:** DESCRIPCIÓN MÍNIMA VERIFICADA (OP + respuesta vacía).

## F-04 — Supuesto «ejecutable capeado a 30 FPS» (nGlide compatibility list)

- **Investigación Lote 1:** NO localizado como pieza separada. La
  compatibility list ofrece un ÚNICO exe parcheado (F-02); la cita «Gex2
  works too fast even at 60fps, needs a 30fps cap» (foro Zeus, S-02)
  describe el problema que motiva el parche, no un segundo fichero. PCGW
  reporta cap base de 30 FPS (secundario, sin verificar).
- **Veredicto:** probable duplicado descriptivo de F-02. Se conserva el ID
  para no romper referencias; fusión pendiente de comparación binaria
  (Fase 2): comprobar si `gex2_patch.zip` ya incluye el cap.
- **Estado:** INFERIDO (pendiente de binario).

## F-05 — Instalación manual + `.reg` + modo administrador — PCGamingWiki

- **Fuente verificada (releída íntegra 2026-10-09):**
  <https://www.pcgamingwiki.com/wiki/Gex:_Enter_the_Gecko>.
- **Problema que dice solucionar:** instalador roto en Windows moderno (I-15).
- **Procedimiento (verbatim):** copiar la carpeta `gex2` del CD; crear un
  `.reg` con `HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00` (`Version`=dword:2,
  `InstallDir`, `CDDriveName`); aplicar F-01 (parche D3D); ejecutar
  `gex3d.exe` como administrador.
- **Datos nuevos del Lote 1:** sección FPS Limiter (enlaza F-03); enlace
  alternativo a F-01 + `gex3d_windows10.zip`; API Direct3D 5; sin soporte
  de ratón; Red Book CD-audio puede fallar en Windows; caps 30 FPS
  (24 con D3D no oficial); requisitos P166/32 MB/DX5; título EU «Gex 3D».
  La casilla «save location» está VACÍA (desconocida incluso para PCGW:
  no inventar ruta).
- **Correlato Fase 1:** claves `...\Gex2\1.00` confirmadas en strings del
  exe EU; valores y uso real requieren dinámico.
- **Estado:** PROCEDIMIENTO VERIFICADO (no ejecutado por nosotros).

## F-06 — Fix del códec Indeo (intro) — VOGONS

- **Fuente verificada (leída íntegra 2026-10-09):**
  <https://www.vogons.org/viewtopic.php?t=40033> — 24 respuestas,
  2014-07-15→18. Contexto Wine/Linux + OSX, no Windows.
- **Contenido sugerido:** colocar `ir32_32.dll` junto al exe y registrar
  descripciones en `HKLM\...\drivers.desc` («Indeo® Video R3.2»…).
- **Resultado del hilo: NO RESUELTO.** El reportero nunca lo hizo funcionar
  (atascado en `wine: command not found`); Jorpho dudó del Indeo desde la
  respuesta 2 y advirtió sobre binarios de procedencia desconocida; Dominus
  redirigió a WineHQ.
- **Valoración:** evidencia DÉBIL, fuente única, sin validación. I-14 sigue
  HIPÓTESIS (Indeo no confirmado; la intro podría usar otro códec). No citar
  como solución confirmada.
- **Estado:** DESCRIPCIÓN VERIFICADA, EFICACIA NO VALIDADA.

## F-07 — nGlide 0.99 en Vista + procedimiento `gex2_patch` — Abandonware France

- **Fuente verificada (leída verbatim 2026-10-09):**
  <https://www.abandonware-france.org/ltf_abandon/ltf_jeu.php?id=1876&fic=aides>
  — «Trucs & Astuces · Fonctionnement sous Vista»: nGlide 1.03 NO
  soportada en Vista («on n'arrive même pas à démarrer le jeu»); nGlide
  0.99 «fonctionne parfaitement». Procedimiento: instalar el juego +
  `nGlide099_setup.exe` + `gex2_patch.zip` → extraer `GEX3D.exe` →
  sobrescribir en la raíz. (Ambas versiones de nGlide siguen hospedadas
  en zeus-software.com — ver T-03.)
- **Ficha principal (id=1876, leída 2026-10-09):** edición EU FR (Ubi Soft +
  reedición Pointsoft + Proein ES); Technique: 3DFX obligatoria, música CD
  que exige el disco en el PRIMER lector óptico (relación I-11/`CDDriveName`);
  bug «música una sola vez por nivel salvo abrir el menú de pausa»
  (adyacente a I-12); mando Xbox 360 recomendado (contexto moderno, no
  evidencia del original); voces Dana Gould (US) / Leslie Phillips (EU)
  (corrobora `voiceuk`). Créditos: «Tgames (Jeu version D3D, Patch)» — AF
  redistribuye su trabajo (no usar; solo referencia documental).
- **Estado:** PROCEDIMIENTO VERIFICADO (en su contexto Vista/época).

## F-08 — Parches de patches-scrolls.de («patch for 3dfx PC», «fix PC»)

- **Fuente verificada (releída, 2 chunks, 2026-10-09):**
  <https://www.patches-scrolls.de/patch/1826/7> — «Gex II»: entradas «patch
  for 3dfx PC» y «fix PC», 16.08.13, «Editiert von: nobody» (sin autoría),
  sin descripciones ni descargas visibles en el texto.
- **Estado:** CATALOGADO (contenido/autoría UNKNOWN).

## F-09 — PC Version Setup Package (Mysticore, speedrun.com)

- **Fuente verificada (releída 2026-10-09):**
  <https://www.speedrun.com/gex2/resources/e3dsk> — «PC Version Setup
  Package» por Mysticore (mod), actualizado hace ~4 años: «everything you
  need to get the NTSC PC version to run on modern systems, without
  needing the physical disc… follow the README». Drive ID `1XvJa18j…`
  coincide con metadatos previos (S-14).
- **Regla de tarea:** NO descargar/extraer/ejecutar el paquete (~411 MB
  catalogados; la tarea cita ~393 MB — discrepancia registrada, ambas sin
  verificar contra bytes). La página NO publica contenidos, README ni
  hashes → payload DESCONOCIDO.
- **Método futuro seguro (fuera del sandbox):** descargar → hash → `unzip -l`
  → extraer solo README → inventario; jamás ejecutar. Copia de trabajo ya
  custodiada en `Drive → REZengineered/research/` (S-14).
- **Estado:** METADATOS VERIFICADOS, CONTENIDO NO INSPECCIONADO.

## F-10 — Music Handler D3D/3DFX (WAV + CD) — tgames.fr [NUEVO Lote 1]

- **Fuente verificada (leída íntegra 2026-10-09):**
  <https://www.tgames.fr/pc/progs-pc/patch-gex-3d-pc-support-des-musiques-sous-windows-7-8-10-t12122.html>
  (OP Tgames 2018-04-15 19:39 + update 2018-04-20 + «Merci!» de AMJ).
- **Qué es:** «Gex 3D: Enter The Gecko D3D/3DFX (Musics CD & Files Handler)
  V1.1»: launcher `Gex3DMusicsHandlerV1.1.exe` + carpeta `musics` (pistas
  WAV) + BONUS pack WAV de PS1 (31 pistas con un futuro Launcher V1.2; 15
  en V1.1; PS1 trae músicas distintas — clones Indy/SW «censurés sur PC»).
  Solo miembros; Win 7/8/10 (32/64).
- **Versión objetivo:** «Only D3D version of the game supported or 3DFX
  patched to D3D» (verbatim). Rutas `…\Crystal Dynamics\gex23dfx` y
  `…\gex23d`; ISOs 3DFX y D3D soportadas para montar. WAVs resueltos vía
  registro (`HKLM\SOFTWARE\Crystal Dynamics\Gex2\1.00\InstallDir` —
  correlato F-05/Fase 1).
- **Problema que dice solucionar:** música en Windows moderno con cualquier
  letra de CD (I-11), o WAVs sin CD; volumen/loop/cortes soportados.
  V1.0: soporte WAV + prioridad CD-si-presente + optimización código CD.
  V1.1: botón de lanzamiento + soporte EXPERIMENTAL inestable de stick
  izquierdo Xbox 360 (desenchufar para probar solo música).
- **Nota:** el mismo hilo ofrece «Gex 3D Controllers Xbox 360 and Xbox One
  Handlers V1.1 (Deprecated)» — no catalogado como fix separado (deprecated
  + sin detalle público).
- **Resultados publicados:** ninguno con datos.
- **Relación EU v1.00.000:** F-01 V1.2.0 (voces UK) remite a esta herramienta
  para la música → cadena F-01+F-10 para EU en Win 10.
- **Preguntas RE:** ¿intercepta MCI/winmm? ¿cómo detecta pistas y loops?
- **Estado:** DESCRIPCIÓN VERIFICADA, binario no inspeccionado.

## F-11 — NO-CD standalone versión D3D — tgames.fr [NUEVO Lote 1]

- **Fuente verificada (leída íntegra 2026-10-09):**
  <https://www.tgames.fr/pc/progs-pc/no-cd-no-cd-gex-3d-enter-the-gecko-version-direct-3d-t12117.html>
  (OP Tgames 2018-03-22 01:33; 0 respuestas).
- **Qué es:** «Gex 3D: Enter The Gecko No-CD (Direct3D Version)»: reemplazar
  `gex3d.exe` en `C:\Program Files\Crystal Dynamics\gex23dfx` (!verbatim:
  carpeta `gex23dfx` aunque el título diga D3D — posible errata o
  nomenclatura Tgames; flag para Fase 2). «Without the disc you will not
  get music during the game» (esto motiva F-10).
- **Versión objetivo:** versión D3D; una descripción externa (rutube, no
  visionado) la rotula «NO CD D3D (USA) Version» — corroboración débil.
- **Modificación exacta:** UNKNOWN. Indicio sin verificar (snippet de buscador
  del antiguo foro AF, URL muerta): el No-CD «maison» serían «quelques NOPs
  au même endroit» sobre versión auto 3DFX+parche euro — pista del propio
  autor, NO confirmada de primera mano.
- **Relación EU v1.00.000:** F-01 ya incluye No-CD para PAL; F-11 standalone
  probablemente US — verificar en Fase 2.
- **Preguntas RE:** ¿NOPs en el check de CD? ¿mismo punto que F-01?
- **Estado:** DESCRIPCIÓN MÍNIMA VERIFICADA (OP sin respuestas).

## F-12 — GEX 3D D3D Debug Tool (trainer + menú debug) — tgames.fr [NUEVO Lote 1]

- **Fuente verificada (leída íntegra 2026-10-09):**
  <https://www.tgames.fr/pc/progs-pc/trainer-gex-3d-enter-the-gecko-direct-3d-cheats-debug-t12121.html>
  (OP Tgames 2018-04-09 10:28; 0 respuestas).
- **Qué es:** «GEX 3D D3D Debug Tool v0.1»: `gex3d_d3d_debugtool.exe` aplica
  memory patch (antes o después del juego); F1 → debug menu in-game;
  posibles falsos positivos de antivirus. Solo miembros.
- **HALLAZGO:** confirma que la build D3D de PC contiene un DEBUG MENU de
  desarrolladores accesible — superficie RE valiosa (los menús debug suelen
  exponer funciones/estados internos).
- **Contexto adicional (snippet, URL AF muerta):** update 2018-04-11 con
  moonjump + passe-muraille (grilles) «pour tester tout le jeu en 5min»;
  el autor distingue la tool externa del DEBUG MENU reactivado («topic
  séparé» — no localizado; posible 2ª pieza pendiente). Contexto familia
  (TCRF PS1, secundario): el debug existe en Gex 2 vía código — NO
  evidencia PC, solo orientativo.
- **Uso proyecto:** herramienta potencial de Fase 2 (nunca redistribuir).
- **Estado:** DESCRIPCIÓN VERIFICADA, binario no inspeccionado.

## Fuera de alcance (registrado para no redescubrir)

- **t12123 «Full Game Gex 3D Windows 10 + NO CD + Musics without CD!»**
  (tgames.fr): repack de juego completo. Visto en related-topics y en una
  descripción externa; FUERA DE ALCANCE por decisión de tarea; no consultado.

## Herramientas de compatibilidad (no son fixes del juego)

Se catalogan como ayudas de testing/estudio. El proyecto decidirá en Fase 3
qué papel juega cada una; la prioridad son soluciones nativas y fundamentadas.

## T-01 — DxWrapper (elishacloud, open source)

- **Qué es:** DDraw/D3D1–7 → D3D9, D3D8 → D9, DInput1–7 → 8, hooks DirectSound,
  loader `.asi`, resolution hack legacy, modo ventana.
- **Interés:** cubre D3D5 (API de la versión US / parche F-01; el binario EU
  inventariado no tiene ruta D3D); código abierto para estudiar
  intercepción de APIs legacy.
- **Fuente:** S-16. **Estado:** catalogado, sin probar.

## T-02 — dgVoodoo2 (dege-diosg, freeware, código cerrado)

- **Qué es:** Glide/DirectDraw/D3D3–9 → D3D11/12.
- **Interés:** comparar rutas Glide y D3D bajo wrappers distintos.
- **Fuente:** S-17. **Estado:** catalogado, sin probar.

## T-03 — nGlide (Zeus Software, freeware, código cerrado)

- **Qué es:** wrapper Glide → Direct3D/Vulkan; estándar de facto para Gex2.
  Versión vigente 2.10 (Win XP–11; Glide 2.11/2.60/3.10); versiones antiguas
  hospedadas incl. 0.99 y 1.03 (relevante F-07). FAQ oficial: «too fast» →
  activar V-Sync en el configurador. (Página verificada 2026-10-09.)
- **Interés:** ruta Glide en GPUs modernas; exe parcheado `gex2_patch.zip`
  (F-02; supuesto capeado F-04 pendiente de confirmar contra ese binario).
- **Fuente:** S-02/S-04. **Estado:** catalogado, sin probar.

## T-04 — DDrawCompat (narzoul, open source, 0BSD)

- **Qué es:** wrapper DirectDraw/Direct3D 1–7 (cubre D3D5) para Vista–11;
  compatibilidad + rendimiento, sin opciones de configuración por diseño.
  Activo (último push verificado: ene 2026).
- **Interés:** alternativa ligera y abierta para la ruta D3D (US/F-01; no
  aplica al binario EU sin modificar);
  DxWrapper lo integra (v0.2.0b/0.2.1/0.3.2).
- **Fuente:** S-19. **Estado:** catalogado, sin probar.

## T-05 — DxWnd (ghotik) — upstream pendiente de confirmar

- **Qué es:** hooker genérico para juegos legacy (modo ventana, hooks de API,
  shims de compatibilidad).
- **Nota:** el mirror GitHub `ghotik/DxWnd` está estancado (2017); el upstream
  actual está **pendiente de localizar y verificar**.
- **Fuente:** S-19. **Estado:** catalogado, sin probar.

## T-06 — WineD3D for Windows (fdossena)

- **Qué es:** builds de WineD3D para Windows: DX1–7 sobre OpenGL
  (arrastrar DLLs junto al exe).
- **Interés:** otra ruta alternativa para Direct3D legacy si Dd7to9/dgVoodoo2
  no cubren algún caso.
- **Fuente:** S-19 (mención en guía Steam; pendiente de verificación directa).
- **Estado:** catalogado, sin probar.

## Proyectos relacionados (no son fixes de la versión PC)

## R-01 — Gex64Decomp (MatBourgon / Tokatta007) — REFERENCIA SECUNDARIA

- **Qué es:** decompilación WIP de *Gex 64* (N64, MIPS) con splat/decomp.me.
- **Utilidad potencial:** nombres, sistemas, lógica, estructuras como
  **referencia** arquitectónica para distinguir comportamiento de la familia
  Gex de problemas del port PC. **NO reproducir su comportamiento;
  NO es código PC.**
- **Advertencia:** NO asumir identidad con la versión PC (distinto port:
  LTI Gray Matter; distinta plataforma y CPU).
- **Estado:** catalogado como referencia secundaria (fuente S-11).

## R-02 — Gex Trilogy (2025) — NO ES REFERENCIA (contexto)

- Por decisión del proyecto (2026-10-08), Gex Trilogy (Limited Run / Carbon
  Engine, emulación de versiones PlayStation) **no** forma parte de las
  referencias. Se conserva esta nota solo como contexto histórico.

## Relación fixes → objetivos (qué estudiar de cada fix)

| Fix | Informa a |
|---|---|
| F-01 (D3D PAL) | FA-03, M-04 (cómo se habilita D3D), M-25 (gestión de voces UK) |
| F-02 (nGlide, exe reemplazo) | FA-03, FA-05, M-13 (renderer; cap 30 por confirmar) |
| F-03 (FPS Limiter) | FA-05, M-13 (mecanismo de límite), M-25 (detector `voiceuk`/`voice`) |
| F-04 (supuesto exe capeado) | Pendiente fusión con F-02 (Fase 2) |
| F-05 (instalación manual) | FA-11, M-01…M-03 |
| F-06 (Indeo, no validado) | FA-10 (hipótesis débil) |
| F-07 (nGlide 0.99 Vista) | Contexto wrapper/época; payload F-02 |
| F-08 | Pendiente de identificar |
| F-09 (setup package) | FA-01, procedimiento de referencia |
| F-10 (music handler) | FA-08, FA-09, M-20 (CD-audio/WAV sin CD) |
| F-11 (NO-CD D3D) | FA-12 (check de CD; posible US) |
| F-12 (debug tool) | Herramienta potencial Fase 2 (menú debug PC) |
| T-01…T-06 (wrappers) | Comparativas de testing (Fase 5+) |

## Plantilla de análisis (usar en Fases 1–2)

Para cada fix documentar: nombre, autor, fecha, problema, comportamiento
antes/después, archivos y ejecutables afectados, DLLs, offsets, funciones,
instrucciones modificadas, hooks, wrappers, APIs, dependencias,
limitaciones, compatibilidad y efectos secundarios. Comparar siempre
**Original → Fix comunitario → REZengineered**.
