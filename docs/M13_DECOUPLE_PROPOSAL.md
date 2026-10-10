# M-13 — Propuesta de prototipo reversible: separación sim/render

> **DISEÑO, no implementación.** Propuesta técnica 2026-10-10, insumo
> para Fase 3. M-13 sigue PROPOSED; FA-04/FA-05 siguen PARCIAL; Fase 1
> en curso; E-2 BLOQUEADO (I-23); método (loader/DLL/parche) SIN
> decidir — lo decide Fase 3 con evidencia. Desbloqueo de FPS en el
> original: prohibido (regla 21). No se declara resuelta la causa del
> límite de FPS.

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
│ DOMINIO PRESENT│  cadencia libre (VSync/límite aparte)
│ drenaje+swap+  │  NO avanza la sim
│ housekeeping   │  preserva rol R1 (ver §6)
└────────────────┘
```

- **Paso-sim fijo:** `STEP` parametrizado, valor PENDIENTE de E-2:
  `40.0 ms` si la cadencia a preservar es 25 Hz (Hito 1) o `33.33 ms`
  si es 30 Hz (PCGW/Zeus «correcto»). Q se produce 1×/paso-sim con
  la fórmula original (`elapsed_ms/16.666`; unidades = frames@60Hz).
- **Present libre:** `Present()` no consume ni produce pasos-sim.
  Repetir estado entre presents es aceptable en el prototipo (sin
  interpolación; ver §6).
- **Original Mode:** `STEP` bloqueado + exactamente 1 paso/present +
  cadencia de swap original = indistinguible del comportamiento
  actual; es el testigo de reversibilidad.

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
  Present()        # ≡ drenaje + swap + housekeeping; cadencia libre
  Registrar(n, acc, dt)  # pasos/s, FPS, descartes (solo prototipo)

# PENDIENTE (requiere dinámica): snapshot rol-R1 por paso-sim o por
# present; R2 debe derivarse del tiempo ACUMULADO, no del nº de presents.
```

**Por qué la velocidad no depende de los FPS:** los pasos-sim los
autoriza el tiempo real acumulado en cuantos fijos (`acc >= STEP`),
no el conteo de presents. Subir FPS solo ejecuta `Present()` más
veces con el mismo estado-sim (o estados sucesivos ya calculados);
la sim avanza `1/STEP` pasos por segundo con independencia del
refresco. Inversamente, si el render se ralentiza, el `while`
inyecta pasos de recuperación acotados (`MAX_STEPS` + descarte
instrumentado) en vez de cámara lenta indefinida.

## 5. Puntos de intervención candidatos (método lo decide Fase 3)

| ID | Punto | Qué desacopla | Estado evidencia / bloqueo |
|---|---|---|---|
| A | Cuña en presentación (lado wrapper/cuña Glide): pacing libre de presents, SIN inyectar pasos-sim | Nada aún (control): separa throttle-present de pacing-sim; complementa E-2 | Diseñable hoy; no toca sim |
| B | Inyección de pasos-sim in-exe (re-entrar update N×/present) | Desacoplado real | BLOQUEADO: exige fronteras exactas del paso-sim + mapa de efectos (R1/R2, contadores, estados) + re-entrancia probada ⇒ Hito K + E-2 + método Fase 3 |
| C | Diezmado simétrico (sim cada K presents) | Solo diagnóstico (cámara lenta controlada); valida instrumentación | Diseñable hoy; no es producto |

Prohibido como «optimización»: tocar `Swap(3)`/intervalo o el drenaje
(regla 21); el throttle queda intacto hasta Origin Mode verificado.

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

**Supuestos NO demostrados (S-01…S-06):** S-01 refresh real = 75 Hz;
S-02 cadencia-sim «correcta» (25 vs 30 Hz); S-03 rol de Q en velocidad
de juego; S-04 semántica `+0x104` (¿contador puro o driver lógico?);
S-05 condiciones de disparo R1/R2; S-06 update re-entrante sin efectos
de orden. Ninguno puede asumirse en la implementación.

**Interpolación (TRX):** DIFERIDA. Exige estado render-mapeado
(posiciones/poses interpolables) que hoy no existe; el prototipo
repite estado entre presents (posible judder, velocidad correcta).

## 7. Criterios medibles (umbrales PROPUESTOS, pendientes de aprobación)

- **REV-1 (reversible):** tras retirar el prototipo, md5 de la copia =
  pin `692b1282…`; registro/config restaurados (checklist); 0 ficheros
  residuales; arranque Hito 1 repetible.
- **SIM-1 (ritmo sim):** travesía fija cronometrada ×3 (protocolo E-2):
  media dentro de ±2 % del baseline A; anims periódicas/10 s idénticas
  (±1 ciclo).
- **FPS-1 (independencia):** misma travesía a 60/120/sin-límite: tiempos
  iguales entre sí (±2 %); FPS varía con complejidad de escena
  (render-bound, no timer).
- **PASO-1 (instrumentación):** pasos-sim/s ≈ 1/STEP (±1 %),
  independiente de FPS; descartes de acumulador = 0 en carga normal.
- **NREG-1 (no regresión):** sesión Hito 1 repetible
  (arranque→nivel→movimiento→audio); MAIN-1…4b intactos; cine/init
  sin cambios observables.

## 8. Límites: estático vs dinámico

| Diseñable con evidencia estática (hecho aquí) | Requiere dinámica (NO diseñado) |
|---|---|
| Estructura del bucle + invariantes | Valor `STEP` (E-2: H1/H2/H3 + ancla 25/30) |
| Restricciones R1/R2 + alias + contadores | Disparo R1/R2 + semántica tabla `0x5ACBE5` |
| Puntos de intervención A/B/C | Re-entrancia de update (Fase 2) |
| Riesgos R-* y supuestos S-* | Refresh real; H2 vs H3 (E-2) |
| Criterios REV/SIM/FPS/PASO/NREG | Validación de umbrales (mantenedor) |
| Puerta de autorización (§10) | Método loader/DLL/parche (Fase 3) |

Causa del límite de FPS: NO resuelta. Candidatas vivas: swap(3)+drain
@75 Hz pedidos vs comportamiento del driver vs pacing del motor;
F-03 incompatible-conocido. Nada de esto se implementa ni se parchea.

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
2. **Hito K completo:** DÓNDE intervenir — fronteras exactas sim/present,
   mapa de efectos (R1/R2 + contadores + estados), hipótesis de tabla
   `0x5ACBE5` como mínimo.
3. **Fase 3 decidida:** método (loader/DLL/parche) con evidencia +
   arquitectura mínima Hito P + promociones a PLANNED de los FA/M
   implicados (esta propuesta es insumo, no sustituto).
4. **Umbrales §7 aprobados** por el mantenedor.

**Pendiente de Hito K:** proyecto Ghidra + naming (`WinMain`,
game-loop); C mínima (timing FA-04/05 resto, ruta/registro FA-11,
init FA-03); D mínima (diffs F-02/F-03 vs original).
**Pendiente de Fase 3:** todo (soluciones candidatas Hito K, método,
arquitectura Hito P, promociones PLANNED). **Pendiente de Fase 1:**
specs Hito 1, decisión A/B, `setup-re-env.sh`, cierre→autorización
Fase 2 (ROADMAP). Sin estos, ninguna implementación.
