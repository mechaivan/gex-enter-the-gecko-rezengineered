# m13-core — Núcleo del bucle M-13 (SIM/PRESENT)

> **I-3 (2026-11-01): la implementación ya no vive aquí.** Se movió al port,
> a `port/include/gex/core/loop_core.h` + `port/src/core/loop_core.c`, para
> que el frame loop nativo y este harness compartan **una sola**
> implementación. Este directorio conserva las 120 pruebas originales y un
> `m13_core.h` de compatibilidad que traduce los nombres `m13_*` a los
> `gex_loop_core_*`. No hay copia divergente.

## Qué hace

Implementación C pura (sin API de plataforma) del acumulador de paso
fijo de `docs/M13_DECOUPLE_PROPOSAL.md` §4. Reloj inyectado (µs,
monótono): el mismo núcleo corre en este harness y en el frame loop del
port. Sin `malloc`, sin hilos.

## Uso

```bash
make            # compila el harness contra el núcleo del port
./test_m13_core # 120 checks; exit 0 = todo pasa
make clean
```

En Windows, sin `make`, el equivalente es:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -I../../port/include `
    -o test_m13_core.exe test_m13_core.c ../../port/src/core/loop_core.c
.\test_m13_core.exe
```

## Requisitos

`gcc` C11. Sin dependencias.

## Evidencia

2026-10-10 (sandbox Linux, gcc 12.2): **120 checks, 0 fallos** —
independencia sim-vs-presents (25 Hz y 120 Hz ⇒ mismos ~250 pasos/10 s),
catch-up acotado, descarte instrumentado, Original Mode 1:1 exacto,
carry fraccional exacto, reloj invertido seguro, 60 s cadencia variable
sin descartes.

2026-11-01 (I-3, esta máquina, gcc/clang MinGW-w64): **120 checks, 0
fallos** con el núcleo compilado desde `port/src/core/loop_core.c`.
