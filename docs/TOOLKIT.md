# TOOLKIT — Entorno y herramientas

Inventario real y verificado (2026-10-08). Todo lo marcado ✅/❌ fue
probado en este entorno; no es una suposición.

## 1. Este sandbox (Debian 12, egress restringido)

| Herramienta | Estado | Notas |
|---|---|---|
| Python 3.11, git, gh, gcc 12, make | ✅ sistema | Base de trabajo |
| binutils (`objdump`, `readelf`, `strings`) | ✅ sistema | Desensamblado e inspección de PEs |
| `curl`, `unzip`, `tar`, Node 22, npm 10 | ✅ sistema | Descargas y tooling |
| venv `/opt/rea-toolkit/pyenv` | ✅ instalado | `pefile 2024.8.26`, `capstone 5.0.7`, `yara` |
| REA CLI 5.0.0 (`/usr/local/bin/rea`) | ⚠️ parcial | Instalado vía npm; análisis nativo NO disponible (ver §3) |
| Java / JDK | ❌ imposible aquí | Sin apt; los assets de release de GitHub redirigen a hosts bloqueados |
| Ghidra 12.1.4 | ❌ imposible aquí | Misma causa (zip de 543 MB en release-assets bloqueado) |
| Rizin / radare2 desde fuente | ❌ imposible aquí | Requieren descargas meson/wrap y headers `-dev` inexistentes |
| Windows de testing | ❌ no disponible | Reproducción y dinámico requieren PC Windows del mantenedor |

Activar: `source /opt/rea-toolkit/env.sh` (añade el venv al PATH).
Todo vive **fuera** del repo (`/opt/rea-toolkit`, ~251 MB): re-ejecutar el
script lo restaura si el sandbox se reinicia. Nunca commitear estos binarios.

## 2. Scripts reproducibles (en `tools/`)

| Script | Para qué | Dónde funciona |
|---|---|---|
| `tools/setup-sandbox-re.sh` | venv PyPI + REA CLI (idempotente) | Este sandbox / máquinas restringidas |
| `tools/setup-re-env.sh` | JDK Temurin 21 + Ghidra 12.1.4 + REA CLI | Máquinas con internet completo (pendiente de probar) |

Ver [tools/README.md](../tools/README.md).

## 3. REA: evaluación honesta (2026-10-08)

- `rea doctor`: `healthy: false` — host Debian 12 no soportado (pide Ubuntu
  24.04+/Fedora 41+/macOS/Arch), sin Hopper, sin Ghidra (`GHIDRA_INSTALL_DIR`
  no definido; espera Ghidra 12.1.x).
- Conclusión: REA queda instalado como capa CLI/MCP lista para usar en cuanto
  haya un backend (Ghidra aportado en una máquina adecuada). En este sandbox
  **no** puede analizar binarios nativos. No fingir lo contrario.

## 4. Kit de análisis estático viable hoy (Etapa A del plan)

Con `pefile` + `capstone` + `objdump` + `strings` se puede hacer, sin Ghidra:

- headers PE, secciones, timestamp, entry point;
- imports/exports y DLLs implícitas;
- strings (claves de registro, rutas, mensajes);
- desensamblado dirigido de funciones localizadas por patrones/YARA;
- diff binario entre variantes (original vs exes parcheados).

Este kit ejecutó la Etapa A en Fase 1 (2026-10-08): resultados en
[ORIGINAL_ARTIFACT_INVENTORY.md](ORIGINAL_ARTIFACT_INVENTORY.md) §2–§4.
Ghidra (decompilador, xrefs, call-graphs) se usará en la máquina del
mantenedor u otro entorno sin restricciones cuando se indique el cambio
de fase (Etapas B+ bloqueadas).

## 5. Plan pendiente

1. [ ] Probar `tools/setup-re-env.sh` en máquina con internet completo.
2. [ ] Definir máquina Windows de testing + protocolo de captura.
3. [x] Carpetas en Drive: `REZengineered/{originals,backups,analysis,builds,research}`.

## 6. Convenciones de almacenamiento

- Originales: solo en Drive (`originals/`), con SHA-256 registrado.
- Copias de trabajo: locales y efímeras; nunca commitearlas.
- Artefactos grandes (proyectos Ghidra, dumps): Drive (`analysis/`).
- En el repo: solo docs, código propio, scripts y configs pequeñas.
- Workspace Arena (~125 MB): temporal; comprobar espacio antes de
  operaciones grandes.
