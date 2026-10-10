# glide-shim — Proxy `glide2x.dll` de instrumentación (Fase A)

## Qué hace

Sustituto reversible de `glide2x.dll` que reenvía las 38 imports Glide
del exe EU a la DLL real (renombrada) y registra `grBufferSwap`
(intervalo) + `grBufferNumPending` (profundidad, muestreada) con
timestamps QPC en `gshim_log.csv`. **No altera ninguna llamada.**
El exe queda intacto (md5 verificable antes/después).

Evidencia que produce (1ª dinámica real del proyecto): tasa real de
presents/s (S-01), intervalos swap usados por escena, profundidad de
cola, dependencia escena (input H2/H3), ancla STEP. Sin tocar registro
ni exe.

## Requisitos

PC Windows + MinGW 32-bit (`i686-w64-mingw32-gcc`). Copia de trabajo
del juego (NO el original).

## Construir

```bat
i686-w64-mingw32-gcc -m32 -shared -O2 -o glide2x.dll gshim.c gshim.def
```

## Procedimiento (copia de trabajo)

1. Punto de restauración: `md5 GEX3D.EXE` (= pin repo `692b1282…`);
   copia de seguridad de `glide2x.dll` original fuera de la carpeta.
2. Renombrar en la copia: `glide2x.dll` → `glide2x_gex_real.dll`.
3. Copiar la `glide2x.dll` compilada a la carpeta. Borrar
   `gshim_log.csv` previo si existe.
4. Arrancar desde la carpeta del juego, jugar 60 s (escena simple +
   escena compleja), salir. Verificar: `gshim_log.csv` existe y crece;
   juego indistinguible (test de instrumentación, NO equivalencia).
5. Reversión: borrar `glide2x.dll` (shim) + `gshim_log.csv`,
   renombrar `glide2x_gex_real.dll` → `glide2x.dll`; re-verificar md5
   del exe; arranque Hito 1 repetible.

## Criterios aceptar/descartar

- ACEPTAR: log con swaps ≈25/s (o el valor real que sea), juego sin
  cambios observables, md5 exe intacto, reversión limpia.
- DESCARTAR (no usar el log): el juego no arranca / va distinto con
  el shim (fallo de reenvío) → reportar + revertir.

## Estado

Fuente solo; sintaxis verificada con `gcc -fsyntax-only` (stub Win32,
sandbox 2026-10-10); `.def` verificado 36/36 contra la IAT del exe.
**Sin compilar ni ejecutar** (sin MinGW/Wine en sandbox): requiere PC
mantenedor.
