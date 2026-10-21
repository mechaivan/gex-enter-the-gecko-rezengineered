# Análisis del motor de timing / game loop — Gex: Enter the Gecko (PC)

> **Nivel de evidencia:** mixto. Lo marcado **CONFIRMADO** proviene de evidencia
> directa (disassembly, REA/Ghidra, strings). Lo marcado **HIPÓTESIS** es inferencia
> explícita. Lo marcado **UNKNOWN** está pendiente de verificación.
> Fuentes: `REVERSE_ENGINEERING.md` §3, `TESTING.md` E-3, `KNOWN_ISSUES.md` I-01,
> análisis Ghidra/REA 2026-10-21.

## 1. Visión general del flujo temporal (Gameplay, estado 2)

```
[Estado 3, 1×/nivel] 0x463A29 → 0x43B11D: +0x10C=0, t0=GetTickCount()→ds:0x57B5FC, +0x108=0, +0x104=0
  → estado 2 --+
               |
[Estado 2, por frame] 0x463AAA → 0x43EA36(ds:0x551640):
               |
               +-- pre-update    | 0x43F80D (DEMO MODE; 1er frame si +0x104==0)
               +-- update+sello  | 0x43E1BA (~20 subsistemas + doble-buffer +0x14 + sello final)
               |                 |   └─ sello: elapsed=now-t0, /16.666 → [obj+0x10C]; +0x104++
               +-- cola swap     | 0x43EA51/0x43EA5E: espera grBufferNumPending→0 + grBufferSwap(3)
               |                 |   └─ luego no-op 0x46300C
               +--               |
               +-----------------+
```

**CONFIRMADO:** simulación y render van en **serie** con `grBufferSwap(3)` (3 retraces = 25 FPS máx a 75 Hz pedido).

## 2. Funciones clave (direcciones del exe EU original)

| Dirección | Función observada | Evidencia | Dependencias |
|---|---|---|---|
| `0x43B36D`/`0x43B373` | Init temporal (1×/nivel): `[obj+0x10C]=0`, `t0=GetTickCount()`→`ds:0x57B5FC` | Disasm (único escritor t0) | obj=`ds:0x551640` |
| `0x43E5DD`/`0x43E5E3` | Sello por frame: `elapsed=now-t0`, `/16.666` (doble `0x54C060`) → `_ftol` → `[obj+0x10C]`; `[obj+0x104]++` | Disasm (único lector t0) | t0 `0x57B5FC` |
| `0x43E1BA` | **Update por frame**: ~20 subsistemas + doble-buffer `+0x14` + sello final | **Ghidra/REA 2026-10-21** (25 callees, 1 caller) | — |
| `0x43F80D` | Pre-update: OSD "DEMO MODE"; si `+0x104==0` → 1er frame | Disasm + strings | Contador `+0x104` |
| `0x43DC3A` | En update si `+0x4E38==0`: instala callbacks render + envía 2 listas + drena `grBufferNumPending` | Disasm | Contador `+0x104` |
| `0x43EA51`/`0x43EA5E` | Cola: espera `grBufferNumPending→0` + `grBufferSwap(3)`; luego no-op `0x46300C` | Disasm + IAT Glide | Presentación |
| `0x46324D` | `grSstWinOpen`: juego=(512×384, 75 Hz, 2 col, 0 aux); cine=(640×480, 60 Hz) | Disasm pushes + enums Glide | Presentación |

## 3. Análisis de `0x43E1BA` (Update por frame) — Ghidra/REA 2026-10-21

**Caller:** `0x43ea36` (FUN_0043ea36) — **único caller** (confirmado).

**Callees (25):**

| Dirección | Nombre | Clasificación | Rol inferido |
|---|---|---|---|
| EXTERNAL:0x10 | `KERNEL32.DLL::GetTickCount` | External | Reloj del sistema (lectura t0/now) |
| `0x540950` | `__ftol` | External (CRT) | Conversión float→int (sello `elapsed/16.666`) |
| `0x540650` | `_memset` | External (CRT) | Limpieza de buffers/structs |
| `0x43dc3a` | FUN_0043dc3a | Internal | Instala callbacks render + envía listas + drena `grBufferNumPending` |
| `0x412f8b` | FUN_00412f8b | Internal | Cuenta atrás por frame (`+0x4BC−−`) + print (lógica frame-acoplada) |
| `0x43e719` | FUN_0043e719 | Internal | (pendiente) |
| `0x45e5c5` / `0x45eac4` | FUN_0045e5c5/ac4 | Internal | (pendiente) |
| `0x460bf7` | FUN_00460bf7 | Internal | (pendiente) |
| `0x465530` | FUN_00465530 | Internal | (pendiente) |
| `0x46dd40` / `0x46e643` / `0x46face` | FUN_0046xxxx | Internal | (pendiente) |
| `0x47344b` / `0x473471` | FUN_0047344b/71 | Internal | (pendiente) |
| `0x537862` | FUN_00537862 | Internal | (pendiente) |
| `0x53f610` / `0x53f631` | FUN_0053f610/31 | Internal | Audio/voz (casa voice scheduler `0x53FCB7`) |
| `0x5401cd` | FUN_005401cd | Internal | (pendiente) |
| `0x540650` | `_memset` | External (CRT) | Limpieza |
| `0x540950` | `__ftol` | External (CRT) | Conversión float→int (sello 16.666) |
| ... | ... | ... | ... |

**Total:** 25 callees (1 external GetTickCount, 2 CRT, ~22 internos).

## 4. Sello temporal (`elapsed/16.666`) — Mecanismo confirmado

- **Divisor:** doble en `0x54C060` = `16.666...` (frames @60 Hz).
- **Cálculo:** `elapsed = now - t0` (ms) → `/16.666` → `_ftol` (float→int) → almacena en `[obj+0x10C]`.
- **Contador:** `[obj+0x104]++` por frame.
- **Lectores del cociente (`+0x10C`):**
  - R1: `0x53FED4` → snapshot `ds:0x5ACBD4` (1W/1R cerrado).
  - R2: `0x451A0C` → `delta=((Q−snap)>>1)+1`, clamp, tabla `0x5ACBE5` → WORD canal voz.

## 5. Anomalía abierta: 34 ms vs 42–44 FPS

- F-03 EU (build 1998-06-29) corre a **42–44 FPS** (~23 ms/frame) con sim acelerada.
- Original EU tiene un bucle de espera de **34 ms** en `0x43EBAD–0x43EBC9` (función `0x43EB76` en F-03).
- **UNKNOWN:** si F-03 usa un divisor distinto, un contador distinto, o un swap con menos retraces.
- El exe original EU usa `0x43E1BA`/`0x43E5DD`/`0x43EA51` (no `0x43EB76`).

## 6. Hipótesis abiertas

| # | Hipótesis | Estado |
|---|---|---|
| H-1 | F-03 es build demo 1998-06-29 con conducta emergente, no parche quirúrgico | HIPÓTESIS (líder) |
| H-2 | Speedup por divisor/contador distinto en sello 16.666 | UNKNOWN (sin desensamblar F-03) |
| H-3 | Path `_demo` afecta flujo temporal (demo mode) | HIPÓTESIS (no verificado) |
| H-4 | F-03 no aplicable como fix a retail EU | HIPÓTESIS (probable) |

## 6. Gaps pendientes

1. **Nombrar las 22 funciones internas** callees de `0x43E1BA` (mov/anim/cám/fís/input/cine).
2. **Analizar F-03** (desensamblar `0x43EB76` y comparar divisor/contador).
3. **Probar E-2** (A/B original vs F-03) para cuantificar acoplamiento sim↔frame.
4. **Verificar PDB** `gex3d.pdb` (GUID unknown, age a, sig `355b4fd7`) para nombres reales.
5. **Mapear subsistemas** en `0x43E1BA` (mov/anim/cám/fís/input/cine).

## 7. Referencias cruzadas

- `REVERSE_ENGINEERING.md` §3 (tabla de direcciones, flujo temporal).
- `KNOWN_ISSUES.md` I-01, I-02.
- `TESTING.md` E-3, P-F03, E-2.
- `F03_VS_ORIGINAL.md` (comparativa F-03 vs original).
- `F02_VS_ORIGINAL.md` (comparativa F-02 vs original).
- `docs/ORIGINAL_ARTIFACT_INVENTORY.md` §2–§4.

## 8. Evidencia técnica (Ghidra/REA 2026-10-21)

- **Toolchain:** REA 6.7.0 + Ghidra 12.1.4 (Linux WSL) + JDK 21.
- **Binario analizado:** `GEX3D_original.EU.exe` (sha256 `d0932549...`).
- **PDB:** `gex3d.pdb` (GUID unknown, age a, signature `355b4fd7`, version NB10).
- **Comando:** `rea xrefs/function/decompile --provider ghidra`.
- **Evidencia:** `ev_c515cdf7...` (xrefs), `ev_...` (function).