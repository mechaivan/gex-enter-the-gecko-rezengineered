# M-13 — Propuesta de prototipo reversible: separación sim/render

> **DISEÑO, no implementación.** Propuesta técnica 2026-10-10, insumo
> para Fase 3. M-13 sigue PROPOSED; FA-04/FA-05 siguen PARCIAL; Fase 1
> en curso; E-2 BLOQUEADO (I-23); método (loader/DLL/parche) SIN
> decidir — lo decide Fase 3 con evidencia. Desbloqueo de FPS en el
> original: prohibido (regla 21). No se declara resuelta la causa del
> límite de FPS.
> Rev.1 (2026-10-10): revisión crítica — fases A/B, límites de A,
> fronteras por-campo, criterios corregidos. Ver §11.

## 1. Objetivo y alcance

Desacoplar la **lógica de juego** (dominio SIM) del **renderizado**
(dominio PRESENT) para que la velocidad del juego no dependa de los
FPS, permitiendo presentar a 60/120/144 Hz o sin límite si resulta
técnicamente viable, con la simulación preservada y el cambio
100 % reversible.

- **Ámbito:** gameplay (estado 2) + presentación. Fuera de ámbito:
  cine/intro (ruta `Swap(1)`, 640x480@60 Hz), init/loading
  (`Swap(2)`), máquina de estados de arranque.
- **Ancla de reversibilidad:** «Original Mode» = comportamiento actual
  (1 paso-sim por present, throttle intacto) como fallback conmutable.
- **Fases (revisión §11):** Fase A = equivalencia con throttle intacto
  (presents ≤~25/s; el bucle degenera a 1:1 y se verifica
  instrumentación + Original Mode). Fase B = present libre
  (60/120/144/sin-límite); requiere autorización expresa SEPARADA
  porque toca pacing de presentación (regla 21) y exige relajar el
  drenaje del propio exe (no basta una cuña externa).

## 2. Hechos de partida (congelados, no re-argumentados)

Punteros; el detalle vive en su documento (fuente única de verdad):

- Flujo temporal gameplay: init t0 `0x43B36D`→`ds:0x57B5FC`; sello
  por frame `elapsed/16.666`→`+0x10C` + `+0x104`++ (`0x43E5DD`);
  update→drenaje-a-cero (`0x43EA51`)→`grBufferSwap(3)`→casa, todo
  serie (REVERSE_ENGINEERING.md Etapa C).
- Cociente `+0x10C` CONSUMIDO: R1 `0x53FED4` (snapshot→`ds:0x5ACBD4`,
  1W/1R) y R2 `0x451A0C` (`delta=((Q−snap)>>1)+1` → tabla `0x5ACBE5`
  → WORD `[[ebp+0x10]]+0x10`), condicionales a modo `ds:0x5ACBC4`∈{0,3}
  + flag `ds:0x5ACBC3`. **Semántica de la tabla: UNKNOWN.**
- Alias `ds:0x551640`=`ds:0x551644`→`0x648900`; snapshot/restore
  contador vía `ds:0x65CD30`; args muertos ×2 a `0x473238`.
- `grSstWinOpen`: juego pide 512x384@75 Hz (`0x46324D`); enums
  `sst1vid.h` (75 Hz=`0x3`). Tasa REAL en runtime: sin confirmar.
- Semántica Glide (impl `gglide.c`): `Swap` encola y retorna; el freno
  es el spin-a-cero del frame siguiente ⇒ sim avanza 1×/swap.
- 25 FPS ≈ Hito 1 = INFERENCIA reforzada (4 hechos), NO prueba.
  F-03 (42–44 FPS + speedup) sin explicar por esta vía.
- Superficie timing: solo `Sleep`+`GetTickCount` (E-3); sin mm-timers.

## 3. Arquitectura mínima

Dos dominios con un solo puente (tiempo acumulado):

```text
Reloj monótono (lado prototipo, NO GetTickCount del juego)
        │ dt real
        ▼
┌───────────────┐   N×STEP consumidos    ┌────────────────┐
│  ACUMULADOR   │ ─────────────────────▶ │  DOMINIO SIM   │
│  acc += dt    │ ◀───────────────────── │  paso fijo     │
└───────────────┘   (clamp anti-espiral) │  1 step ≡      │
        │                                │  1 update de   │
        │ cada iteración                 │  hoy + sello Q │
        ▼                                └────────────────┘
┌────────────────┐
│ DOMINIO PRESENT│  cadencia libre (Fase B; Fase A = intacta)
│ drenaje+swap+  │  Fase A: NO avanza la sim de más; 1:1
│ housekeeping   │  preserva rol R1 (ver §6)
└────────────────┘
```

- **Paso-sim fijo:** `STEP` parametrizado, valor PENDIENTE de E-2:
  `40.0 ms` si la cadencia a preservar es 25 Hz (Hito 1) o `33.33 ms`
  si es 30 Hz (PCGW/Zeus «correcto»). Q se produce 1×/paso-sim con
  la fórmula original (`elapsed_ms/16.666`; unidades = frames@60Hz).
  `STEP` fija la tasa MÁXIMA de sim (el original es present-bound
  sin suelo: a frame lento, sim lenta — ver R-catchup §6).
- **Present:** Fase A = cadencia original intacta (present NO avanza
  la sim de más). Fase B = cadencia libre (requiere §1/autorización).
  Repetir estado entre presents es aceptable en el prototipo (sin
  interpolación; ver §6).
- **Original Mode:** `STEP` bloqueado + exactamente 1 paso/present +
  cadencia de swap original = indistinguible del comportamiento
  actual; es el testigo de reversibilidad (criterio ORG-1 §7).
- **Fronteras NO limpias (revisión §11, Hito K):** el update actual
  INCLUYE envío render (`0x43DC3A`: callbacks + 2 listas + drenaje) y
  el housekeeping (`0x53FCB7`) mezcla cuenta-de-frames (375) con flags
  de modo R1/R2. La frontera SIM/PRESENT debe trazarse POR CAMPO, no
  por función; repetir `0x43DC3A` N× repite submits (no solo sim) y
  mover `0x53FCB7` a cadencia-present cambia la cadencia de los flags
  de modo. Sin este mapa, B es inseguro.

## 4. Bucle propuesto (pseudocódigo de referencia)

```text
# Comportamiento objetivo; no es código final ni fija el método.
t_prev = reloj_monotono()
acc = 0.0
loop:
  t = reloj_monotono()
  acc += min(t - t_prev, MAX_DT)   # MAX_DT anti-espiral (proponer: 250 ms)
  t_prev = t
  n = 0
  while acc >= STEP and n < MAX_STEPS:  # MAX_STEPS (proponer: 5)
    SimStep()      # ≡ 1 update actual: subsistemas + sello Q + contador
    acc -= STEP; n += 1
  if n == MAX_STEPS and acc >= STEP:
    acc = 0.0      # descarta atraso; instrumentar contador (observable)
  Present()        # ≡ drenaje + swap + housekeeping; cadencia según fase
  Registrar(n, acc, dt)  # pasos/s, FPS, descartes (solo prototipo)

# Q sigue RELOJ DE PARED ABSOLUTO (now−t0, fórmula original) por paso-sim:
# preserva R2 ante cualquier N. PENDIENTE (requiere dinámica): cadencia del
# snapshot rol-R1 (por paso-sim o por present) y colocación del pre-update
# 0x43F80D (por paso o por present). R2 NO debe contar presents.
```

**Por qué la velocidad no depende de los FPS:** los pasos-sim los
autoriza el tiempo real acumulado en cuantos fijos (`acc >= STEP`),
no el conteo de presents. Subir FPS solo ejecuta `Present()` más
veces con el mismo estado-sim (o estados sucesivos ya calculados);
la sim avanza `1/STEP` pasos por segundo con independencia del
refresco. Inversamente, si el render se ralentiza, el `while`
inyecta pasos de recuperación acotados (`MAX_STEPS` + descarte
instrumentado) en vez de cámara lenta indefinida — ESTO CAMBIA el
comportamiento bajo carga respecto al original (R-catchup §6):
a frame lento el original ralentiza el juego; el prototipo lo
mantiene. Es el objetivo del desacoplado, pero debe caracterizarse,
no asumirse gratis.

## 5. Puntos de intervención candidatos (método lo decide Fase 3)

| ID | Punto | Qué desacopla | Estado evidencia / bloqueo |
|---|---|---|---|
| A | Cuña en presentación (lado wrapper/cuña Glide): pacing ADITIVO (solo ralentizar/forzar VSync) + instrumentación; SIN inyectar pasos-sim | Nada aún (control): separa throttle-present de pacing-sim; complementa la variable VSync de E-2 | Diseñable hoy; NO puede acelerar presents: el drenaje-a-cero vive en el exe, y relajar swap/drenaje = Fase B (autorización separada) |
| B | Inyección de pasos-sim in-exe (re-entrar update N×/present) | Desacoplado real | BLOQUEADO: exige fronteras por-campo (§3: `0x43DC3A` submit-vs-drain, `0x53FCB7` cuenta-vs-modos, colocación `0x43F80D`) + mapa de efectos + re-entrancia probada ⇒ Hito K + E-2 + método Fase 3 |
| C | Diezmado simétrico (sim cada K presents) | Solo diagnóstico (cámara lenta controlada); valida instrumentación | Diseñable hoy; no prueba la ruta multi-paso (n>1); no es producto |
| C′ | STEP reducido temporalmente (p. ej. 20 ms), throttle intacto ⇒ n=2/present habitual | Diagnóstico de la RUTA MULTI-PASO sin tocar presentación ni FPS | Diseñable hoy; prerrequisito barato de B (seguridad de repetir update) |

Prohibido como «optimización»: tocar `Swap(3)`/intervalo o el drenaje
(regla 21); el throttle queda intacto en Fase A y solo se revisa en
Fase B con autorización separada. Present-libre NO es alcanzable solo
con cuña externa: exige relajar el drenaje del exe.

## 6. Preservación, riesgos y supuestos no demostrados

**A preservar:** cadencia sim observada; productor Q 1×/paso con
fórmula original; ruta R1/R2 (snapshot + delta) con unidades intactas;
orden update→present; contadores (`+0x104`, cuenta 375, `0x412F8B`);
ruta cine/init sin cambios.

| Riesgo | Área | Por qué |
|---|---|---|
| R-audio | R2→tabla→WORD de semántica UNKNOWN (¿pitch/tempo/volumen?) | Deltas Q distintos de los originales ⇒ artefactos de audio |
| R-anim | Lógica contada en frames (`0x412F8B`, cuenta 375, `+0x104`) | N pasos/present cambia conteos si algún consumidor asume 1:1 |
| R-timer | `GetTickCount`/`Sleep`/busy-waits; debounce 10 ms | Doble dominio temporal si el exe lee reloj de pared mid-paso |
| R-estado | Máquina 9 estados; transiciones possibly por frame | Re-entrar update puede saltar transiciones |
| R-input | Muestreo por update | N pasos/present ⇒ input rancio o multiplicado |
| R-trigger | Disparo R1/R2 en runtime UNKNOWN | Snapshot en momento distinto ⇒ delta distinto |
| R-reentr | Re-entrancia de update sin probar | Estado global a medio paso ⇒ corrupción |
| R-submit | `0x43DC3A` re-ejecutado N×: re-envía 2 listas render por paso | Intermediate states al mismo backbuffer (el último gana + coste); drenaje repetido SÍ es inocuo (espera vacua) pero el submit no está probado |
| R-hkmode | Flags modo `ds:0x5ACBC4`/`0x5ACBC3` escritos en familia `0x53FCxx` (housekeeping+`0x53FE80`) | Si modo corre a cadencia-present y R2 a cadencia-sim, combinaciones jamás probadas por el original |
| R-catchup | Catch-up bajo carga (diseño §4) | El original ralentiza el juego a frame lento; el prototipo lo mantiene ⇒ DIVERGENCIA INTENCIONADA que debe caracterizarse (no es regresión si se declara y se acota) |
| R-count | Consumidores de cadencia del contador (snapshot/restore `ds:0x65CD30`, `0x4721B6`, puerta 1er frame `0x43F85D`) | N incrementos/present cambian lo que ven respecto al 1:1 original |

Colocación del pre-update `0x43F80D` (OSD + puerta 1er frame) y de la
cuenta 375: TBD en Hito K (por paso-sim o por present).

**Supuestos NO demostrados (S-01…S-06):** S-01 refresh real = 75 Hz;
S-02 cadencia-sim «correcta» (25 vs 30 Hz); S-03 rol de Q en velocidad
de juego; S-04 semántica `+0x104` (¿contador puro o driver lógico?);
S-05 condiciones de disparo R1/R2; S-06 update re-entrante sin efectos
de orden. Ninguno puede asumirse en la implementación. Clasificación
con evidencia: §11.

**Interpolación (TRX):** DIFERIDA. Exige estado render-mapeado
(posiciones/poses interpolables) que hoy no existe; el prototipo
repite estado entre presents (posible judder, velocidad correcta).

## 7. Criterios medibles (umbrales PROPUESTOS, pendientes de aprobación)

Procedimiento de tolerancias (revisión §11): ningún % es válido hasta
medir la varianza del baseline (herramienta + operador + runs). Los
valores abajo son PLACEHOLDERS; la tolerancia final = k×σ_baseline con
k aprobado, tras Hito R (método reproducible + checklist MAIN-8, hoy
inexistente). Sin método de medición no hay criterio ejecutable.

- **REV-1 (reversible):** tras retirar el prototipo, md5 de la copia =
  pin `692b1282…` (método cuña: el exe nunca cambia; método parche:
  re-copiar desde backup + verificar); registro/config restaurados
  según checklist (a redactar con el prototipo, alimenta MAIN-8);
  0 ficheros residuales contra manifiesto previo; arranque Hito 1
  repetible.
- **SIM-1 (ritmo sim):** travesía fija cronometrada ×3 (protocolo E-2):
  media dentro de ±2 % [PLACEHOLDER] del baseline A; anims
  periódicas/10 s idénticas (±1 ciclo [PLACEHOLDER]). Requiere definir
  ANTES: qué travesía, qué anim, con qué cronómetro, qué operador.
- **FPS-1A (Fase A, throttle intacto):** FPS ≈ baseline Hito 1 (±σ) y
  pasos-sim/s ≈ 25 (±1 % [PLACEHOLDER], ventana 10 s rodante):
  el bucle nuevo reproduce el 1:1 original.
- **FPS-1B (Fase B, SOLO con autorización separada):** misma travesía a
  60/120/sin-límite: tiempos iguales entre sí (±2 % [PLACEHOLDER]);
  FPS varía con complejidad de escena (simple↔compleja E-2) ⇒
  render-bound, no timer.
- **PASO-1 (instrumentación):** pasos-sim/s ≈ 1/STEP (±1 %
  [PLACEHOLDER], ventana 10 s rodante), independiente de FPS;
  descartes de acumulador = 0 en carga normal (escenas E-2
  simple/compleja).
- **ORG-1 (testigo Original Mode):** con el toggle en original, SIM-1 +
  FPS baseline reproducidos (mismas tolerancias): demuestra que el
  andamiaje no altera el comportamiento.
- **NREG-1 (no regresión):** sesión Hito 1 repetible
  (arranque→nivel→movimiento→audio); MAIN-1…4b intactos; init llega a
  jugable; duraciones de cine ≈ baseline A/B (±σ, con independencia
  del mecanismo hold 4000 ms, inferencia E-3).

## 8. Límites: estático vs dinámico

| Diseñable con evidencia estática (hecho aquí) | Requiere dinámica (NO diseñado) |
|---|---|
| Estructura del bucle + invariantes | Valor `STEP` (E-2: H1/H2/H3 + ancla 25/30) |
| Restricciones R1/R2 + alias + contadores | Disparo R1/R2 + semántica tabla `0x5ACBE5` |
| Puntos de intervención A/B/C/C′ | Re-entrancia de update (Fase 2) |
| Riesgos R-* y supuestos S-* | Refresh real; H2 vs H3 (E-2) |
| Criterios REV/SIM/FPS/PASO/ORG/NREG | Validación de umbrales (mantenedor) |
| Puerta de autorización (§10) | Método loader/DLL/parche (Fase 3) |
| Fronteras por-campo (diseño del mapa) | Comportamiento multi-paso real (C′/B) |

Estático pendiente (Fase 2/Hito K, con autorización expresa como las
anteriores): interior de `0x43DC3A` (submit-vs-drain), `0x53FCB7`
(cuenta-vs-modos), escritores de tabla `0x5ACBE5`, identidad del 3er
arg R2, colocación `0x43F80D`. Causa del límite de FPS: NO resuelta.
Candidatas vivas: swap(3)+drain @75 Hz pedidos vs comportamiento del
driver vs pacing del motor; F-03 incompatible-conocido. Nada de esto
se implementa ni se parchea.

## 9. Referencias de diseño (soul-re, TRX — sin extrapolar)

Verificado en exe EU (2026-10-10): SIN símbolos `decouple`/`frameRate`/
`timeMult`/`gameFrame` — ninguna estructura ajena se presupone en Gex PC.

**soul-re** (`GAMELOOP_DoTimeProcess`, RESEARCH S-26/G-02). Útil: patrón
acumulador punto-fijo (`timeMult`→`while(>=unidad){paso}`); compuerta
lógica por flag (`gameFramePassed`); clamp anti-espiral (66 ms ≈ nuestro
`MAX_DT`/`MAX_STEPS`); concepto de toggle (`decoupleGame` ≈ nuestro
Original Mode). NO trasladable: llamadas VBL/`VSync` (PSX, Gex PC es
Glide/present-driven); valores 33/50 ms; campos/flags concretos del
motor Soul Reaver; suponer que Gex PC comparte código.

**TRX** (`phase/executor.c`, `clock/`, RESEARCH S-26/G-03). Útil: patrón
`nframes=WaitTick()`→N×`Control()`→`Draw()`; separación Control/Draw
(≈ SIM/PRESENT); concepto de draw interpolado opcional (diferido aquí,
§6). NO trasladable: TRX posee el fuente completo (reimplementación);
Gex exige intervenir un binario cerrado con método sin decidir;
renderer OpenGL moderno ≠ pipeline Glide fijo; interpolación sin
estado render-mapeado.

**sezero/glide** (G-01): ya minado (semántica swap/drenaje usada en §2).
**KAIN2-PC** (G-04): excluido (sin Glide, obsoleto).

## 10. Siguiente decisión: puerta de autorización

Evidencia MÍNIMA antes de autorizar implementación:

1. **E-2 desbloqueado + ejecutado** (tras decisión método A/B pese a
   I-23, sin investigar origen I-23): veredicto H1/H2/H3, ancla STEP
   (25/30 Hz), render-bound probado.
2. **Hito K completo:** DÓNDE intervenir — fronteras exactas sim/present
   POR CAMPO (`0x43DC3A`, `0x53FCB7`, `0x43F80D`), mapa de efectos
   (R1/R2 + contadores + estados), hipótesis de tabla `0x5ACBE5` como
   mínimo.
3. **Hito R + MAIN-8:** método de medición reproducible y checklist de
   validación redactado (sin esto, §7 no es ejecutable).
4. **Fase 3 decidida:** método (loader/DLL/parche) con evidencia +
   arquitectura mínima Hito P + promociones a PLANNED de los FA/M
   implicados (esta propuesta es insumo, no sustituto). La cuña
   externa no basta para Fase B (drenaje en el exe): el método debe
   contemplar componente in-exe.
5. **Umbrales §7 aprobados** por el mantenedor (tras medir σ_baseline).
6. **Fase B:** autorización expresa SEPARADA (pacing de presentación).

**Pendiente de Hito K:** proyecto Ghidra + naming (`WinMain`,
game-loop); C mínima (timing FA-04/05 resto, ruta/registro FA-11,
init FA-03); D mínima (diffs F-02/F-03 vs original); fronteras
por-campo (§3); escritores de tabla `0x5ACBE5`.
**Pendiente de Fase 3:** todo (soluciones candidatas Hito K, método,
arquitectura Hito P, promociones PLANNED). **Pendiente de Fase 1:**
specs Hito 1, decisión A/B, `setup-re-env.sh`, cierre→autorización
Fase 2 (ROADMAP). Sin estos, ninguna implementación.

## 11. Revisión crítica Rev.1 (2026-10-10; sin implementación)

Veredicto: **MANTENER con REVISIÓN** (esta). Estructura coherente, sin
defecto fatal; los cambios de Rev.1 son precondición para usarla como
insumo Fase 3.

Hallazgos aplicados (ordenados por riesgo):

1. **Fronteras por función FALSAS (§3, R-submit, R-hkmode):** `0x43DC3A`
   (update) envía render y `0x53FCB7` (housekeeping) escribe flags de
   modo. Repetir update N× repite submits; mover modo a cadencia-present
   crea combinaciones inéditas. Frontera por-campo ⇒ Hito K.
2. **Throttle intacto vs present-libre (§1/§5/§7):** FPS-1 original era
   inejecutable (con throttle intacto no hay 60/120). Split Fase A
   (equivalencia 1:1) / Fase B (present libre, autorización separada).
3. **A no acelera (§5):** el drenaje vive en el exe; la cuña solo suma
   esperas/instrumenta. Present-libre exige componente in-exe (§10.4).
4. **Tolerancias sin base (§7):** ±2 %/±1 %/±1 ciclo eran placeholders
   sin análisis de error (cronómetro humano, MAIN-8 inexistente).
   Procedimiento: medir σ_baseline (Hito R) → tolerancia = k×σ.
5. **Q ambigua (§4):** aclarado — Q sigue reloj de pared absoluto por
   paso (preserva R2 ante cualquier N); lo TBD es la cadencia del
   snapshot y la colocación de `0x43F80D`.
6. **Catch-up = divergencia declarada (R-catchup):** bajo carga el
   original ralentiza el juego y el prototipo no. Intencionado, pero
   debe caracterizarse (no es regresión si se acota).
7. **Criterios incompletos (§7):** faltaba ORG-1 (testigo Original Mode);
   REV-1 por método + manifiesto + MAIN-8; NREG-1 medible en cine/init;
   PASO-1 con ventana y carga definidas. Añadido C′ (multi-paso sin
   tocar presentación).
8. **Typo:** «Origin Mode» → «Original Mode» (§5).

Clasificación de supuestos y riesgos (D=demostrado, P=parcialmente
respaldado, N=no demostrado, R=refutado):

| ID | Enunciado | Estado | Evidencia |
|---|---|---|---|
| S-01 | Refresh real = 75 Hz | P | Pedidos 75 Hz estático (`0x46324D`); real sin confirmar (driver) |
| S-02 | Cadencia-sim «correcta» 25 vs 30 | N | ≈25 FPS observado (Hito 1, hecho); «correcto» indefinido; 30 = PCGW/Zeus (ajeno); F-03 speedup sugiere acoplamiento, no corrección |
| S-03 | Q gobierna velocidad de juego | N | Dataflow Q→R2 probado; vínculo con velocidad del movimiento: cero evidencia (probablemente manda el conteo de pasos) |
| S-04 | `+0x104` driver lógico vs contador | P | Lectores/escritores mapeados; consumidores reales (puerta 1er frame, `0x4721B6`, snapshot/restore) ⇒ no puramente informativo; fuerza exacta TBD |
| S-05 | Disparo R1/R2 | P | Estructura estática probada (cadenas + flags); condiciones runtime TBD |
| S-06 | Update re-entrante | N | Cero evidencia en ningún sentido |
| R-audio…R-count | 12 riesgos §6 | P (fundados) | Premisas con base estática cada una; magnitudes/efectos TBD dinámica |

Nada refutado; nada implementado; E-2 sigue BLOQUEADO.
