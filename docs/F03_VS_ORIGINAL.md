# Comparativa F-03 (EU) vs Original EU — Gex: Enter the Gecko (PC)

> **Nivel de evidencia:** mixto. Lo marcado **CONFIRMADO** proviene de evidencia
> directa (hashes, disassembly, strings). Lo marcado **HIPÓTESIS** es inferencia
> explícita. Lo marcado **UNKNOWN** está pendiente de verificación.
> Fuentes: `KNOWN_ISSUES.md`, `PATCH_ANALYSIS.md` (F-03), `REVERSE_ENGINEERING.md`
> (Etapa C/E-3), `TESTING.md` (P-F03), `MODERNIZATION_GOALS.md` (FA-01/FA-04).

## 1. Identidad de los binarios

| Campo | Original EU | F-03 EU |
|---|---|---|
| Fichero | `GEX3D.EXE` | `GEX3D.EXE` |
| md5 | `692B1282…` | `3198350e…` (E-1.1, paste mantenedor) / `7A9B6851…` (sha256) |
| sha256 | `692b1282…` (pendiente de verificación completa) | `7a9b6851…a3e8254` |
| TimeDateStamp (PE) | `0x3560A32E` (19-may-1998) | `0x35980BE1` (29-jun-1998) |
| Tamaño | ~1.1 MB (pendiente de verificar) | ~1.1 MB |
| Versión declarada | 1.00.000 | Sin cambio visible (mismo nombre) |
| Imports timing | `Sleep` + `GetTickCount` | `Sleep` + `GetTickCount` (sin nueva API) |
| Imports adicionales | — | `+mciGetErrorStringA` |

**CONFIRMADO:** son **builds distintas** (fechas de compilación diferentes, 82.9%
de bytes distintos). F-03 NO es un parche quirúrgico sobre el original; es una
recompilación de junio de 1998.

## 2. Diff estático (E-1, ejecutado 2026-10-09)

| Métrica | Valor |
|---|---|
| Bytes distintos | 82.9% (55016 runs) |
| Bloques 4 KB idénticos | 1 de 381 |
| Nueva API timing | No (solo `+mciGetErrorStringA`) |
| Path por defecto | ORIG: `…\Gex2` / F-03: `…\Gex23dfx_demo` |
| EU vs US (F-03) | 67.3% distintos (builds separadas) |

**CONFIRMADO:** la diferencia es masiva y sistemática, no un parche puntual.
El path por defecto `_demo` sugiere que F-03 es una **build de línea demo**
de junio-1998, no una corrección de bugs.

## 3. Comportamiento observado (P-F03/B, 2026-10-09)

| Escenario | Original EU | F-03 EU |
|---|---|---|
| FPS en menú+juego | ~25 FPS | 42–44 FPS |
| Cinemáticas | ~25 FPS | ~15 FPS |
| Velocidad de simulación | Normal (acoplada a frame) | Acelerada (acoplamiento sim↔frame OBSERVADO) |
| Audio | Normal | Audio-rate independiente (observado; triggers sin probar) |

**CONFIRMADO (observación):** F-03 corre a 42–44 FPS vs ~25 del original,
con simulación acelerada. El audio mantiene su tasa independiente.

**HIPÓTESIS:** el acoplamiento sim↔frame es la causa del speedup; desacoplar
sim↔render requeriría ingeniería nueva (M-13).

## 4. Análisis estático del timing (E-3/M-13, 2026-10-10)

### Original EU (direcciones documentadas)

| Dirección | Función observada | Evidencia |
|---|---|---|
| `0x43B36D` | Init temporal (1×/nivel): `[obj+0x10C]=0`, `t0=GetTickCount()`→`ds:0x57B5FC` | Disasm (único escritor t0) |
| `0x43E5DD` | Sello por frame: `elapsed=now-t0`, `/16.666` → `[obj+0x10C]`; `[obj+0x104]++` | Disasm (único lector t0) |
| `0x43E1BA` | Update por frame: ~20 subsistemas + doble-buffer + sello final | Disasm completo |
| `0x43EA51` | Espera `grBufferNumPending→0` + `grBufferSwap(3)` | Disasm + IAT Glide |
| `0x43F80D` | OSD "DEMO MODE"; si `+0x104==0` → 1er frame | Disasm + strings |
| `0x46324D` | `grSstWinOpen`: 512×384 @ 75 Hz (juego) | Disasm pushes + enums Glide |

### F-03 EU
**UNKNOWN:** las direcciones del flujo temporal de F-03 no han sido
desensambladas. Se sabe que usa las mismas APIs (`Sleep`+`GetTickCount`) y que
el comportamiento difiere, pero el mecanismo interno (si es conducta emergente
de la build o un cambio de código) no está confirmado.

**HIPÓTESIS líder (E-1):** línea demo de junio-1998 con conducta emergente,
no parche quirúrgico. El path `_demo` y la fecha de compilación (29-jun-1998)
sugieren que es una build de desarrollo/demo, no una corrección.

## 5. Mecanismo de timing (inferido)

El original EU acopla simulación a frame:
- `elapsed = now - t0` (en ms), dividido por 16.666 (frames @60 Hz).
- El resultado (`+0x10C`) es el "cociente" de frames transcurridos.
- Simulación y render van en serie con `grBufferSwap(3)` (3 retraces = 25 FPS
  máx a 75 Hz pedido).
- Los lectores de `+0x10C` (R1 snapshot `ds:0x5ACBD4`, R2 delta→tabla)
  consumen el cociente para animación/sonido.

**HIPÓTESIS:** F-03 podría tener un divisor distinto, un contador distinto,
o un swap con menos retraces, lo que explicaría los 42–44 FPS. Pero esto es
**UNKNOWN** sin desensamblar F-03.

## 6. Anomalía abierta: 34 ms vs 42–44 FPS

- El original tiene un bucle de espera de 34 ms (`0x43EBAD–0x43EBC9`) en
  la función de frame `0x43EB76` (F-03, no el original EU).
- Si F-03 corre a 42–44 FPS (~23 ms/frame), el límite de 34 ms no debería
  ser el cuello de botella.
- **UNKNOWN:** esta dirección (`0x43EB76`) proviene del análisis de la sesión
  sobre F-03 y **no está documentada en el repo** para el original EU. El
  original EU usa `0x43E1BA` (update) / `0x43E5DD` (sello) / `0x43EA51` (swap).
- La anomalía sigue **ABIERTA** (no resuelta).

## 7. Hipótesis abiertas

| # | Hipótesis | Estado |
|---|---|---|
| H-1 | F-03 es una build demo de junio-1998 con conducta emergente, no un parche | HIPÓTESIS (líder) |
| H-2 | El speedup se debe a un divisor o contador distinto en el sello 16.666 | UNKNOWN (sin desensamblar F-03) |
| H-3 | El path `_demo` afecta el flujo temporal (demo mode) | HIPÓTESIS (no verificado) |
| H-4 | F-03 no es aplicable como fix a la versión retail EU | HIPÓTESIS (probable, por ser build distinta) |
| H-5 | El audio-rate independiente implica que el sonido no se acelera con la sim | OBSERVADO (triggers sin probar) |

## 8. Gaps pendientes

1. **Desensamblar F-03** con las mismas herramientas que el original EU
   (objdump/pefile) para comparar el flujo temporal y el divisor 16.666.
2. **Comparar imports** de F-03 vs original (ya se sabe: solo `+mciGetErrorStringA`,
   pero falta verificar si hay otras diferencias).
3. **Verificar el path `_demo`** en runtime (requiere ejecución, bloqueado).
4. **Probar E-2** (A/B original vs F-03) para cuantificar el acoplamiento
   sim↔frame (protocolo listo, ejecución pendiente).
5. **Analizar el diff binario** a nivel de funciones (requiere Ghidra para
   call-graph).

## 9. Referencias

- `KNOWN_ISSUES.md` I-01, I-02
- `PATCH_ANALYSIS.md` F-03, E-1, E-1.1, P-F03, P-F03/B
- `REVERSE_ENGINEERING.md` §3 (tabla de direcciones, flujo temporal)
- `TESTING.md` (protocolo P-F03, E-2, E-3)
- `MODERNIZATION_GOALS.md` FA-01, FA-04
- `docs/ORIGINAL_ARTIFACT_INVENTORY.md` §2–§4 (metadatos de binarios)
- `docs/REA_WINDOWS_ENABLEMENT.md` (análisis profundo pendiente de Ghidra)
