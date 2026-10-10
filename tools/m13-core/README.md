# m13-core — Núcleo del bucle M-13 (SIM/PRESENT)

## Qué hace

Implementación C pura (sin API de plataforma) del acumulador de paso
fijo de `docs/M13_DECOUPLE_PROPOSAL.md` §4. Reloj inyectado (µs,
monótono): el mismo núcleo corre en este harness Linux y en el futuro
shim Windows. Sin `malloc`, sin hilos.

## Uso

```bash
make            # compila el harness
./test_m13_core # 120 checks; exit 0 = todo pasa
make clean
```

## Requisitos

`gcc` C11. Sin dependencias.

## Evidencia

2026-10-10 (sandbox Linux, gcc 12.2): **120 checks, 0 fallos** —
independencia sim-vs-presents (25 Hz y 120 Hz ⇒ mismos ~250 pasos/10 s),
catch-up acotado, descarte instrumentado, Original Mode 1:1 exacto,
carry fraccional exacto, reloj invertido seguro, 60 s cadencia variable
sin descartes.
