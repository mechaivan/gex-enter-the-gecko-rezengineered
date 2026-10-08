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

## Reglas

- Solo código propio, bajo la licencia del repo (ver `/LICENSE.md`).
- Cada herramienta con README mínimo: qué hace, uso, requisitos.
- Nada propietario, nada binario grande. Trabajar siempre sobre copias.
