# tools/ — Herramientas propias

## Scripts de entorno

- **`setup-sandbox-re.sh`** — Prepara el entorno RE viable en máquinas con
  egress restringido (este sandbox): venv Python con `pefile` + `capstone` +
  `yara-python` (PyPI) y REA CLI (npm) en `/opt/rea-toolkit`. Idempotente.
  Uso: `sudo ./tools/setup-sandbox-re.sh`. Verificado 2026-10-08.
- **`setup-re-env.sh`** — Entorno completo (JDK Temurin 21 + Ghidra 12.1.4 +
  REA CLI) para máquinas con internet sin restricciones. **Pendiente de
  probar.** Uso: `sudo ./tools/setup-re-env.sh [PREFIX]`.

## Uso del kit sandbox

```bash
source /opt/rea-toolkit/env.sh
python -c "import pefile; pe = pefile.PE('copia_de_trabajo.exe'); print(pe.dump_info())"
```

## Prototipos e instrumentación

- **`m13-core/`** — Núcleo C11 del acumulador de paso fijo
  (`docs/M13_DECOUPLE_PROPOSAL.md` §4): reloj inyectado, sin
  plataforma, harness propio. Verificado 2026-10-10 (120 checks,
  0 fallos, gcc Linux). NO integrado; M-13 sigue PROPOSED.
- **`glide-shim/`** — Proxy `glide2x.dll` SOLO instrumentación
  (Fase A): 35 reenvíos + intercepta `grBufferSwap` /
  `grBufferNumPending` / `grGlideShutdown` → CSV (ticks QPC
  crudos, anillo RAM, auto-coste, modo `GSHIM_NOLOG` aislado).
  v3 riesgo-revisado 2026-10-10: `DllMain` solo-ATTACH + finalize
  en shutdown, fail-fast (exit 111, sin retornos ficticios),
  `.def` 38/38 vs IAT + aridades vs SDK Glide 2.x, suite
  `./tests/run_tests.sh` verde (14+3+55); **sin compilar ni
  ejecutar** (requiere MinGW-32 + V-0/V-1/V-1b/V-2 en PC Windows;
  compatibilidad plena NO confirmada). Ver README propio
  (construir + validar + reversión).

## Reglas

- Solo código propio, bajo la licencia del repo (ver `/LICENSE.md`).
- Cada herramienta con README mínimo: qué hace, uso, requisitos.
- Nada propietario, nada binario grande. Trabajar siempre sobre copias.
