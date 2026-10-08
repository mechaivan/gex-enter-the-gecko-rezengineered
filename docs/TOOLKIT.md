# TOOLKIT — Entorno y herramientas

Inventario real del entorno de trabajo (verificado 2026-10-08) + plan.

## 1. Entorno Arena (Linux, este sandbox)

| Herramienta | Estado | Notas |
|---|---|---|
| Debian 12, Python 3.11, git, gh | ✅ disponible | Base de trabajo |
| binutils (`objdump`, `readelf`, `strings`) | ✅ disponible | Análisis estático inicial de PEs |
| `unzip`, `file` | ✅ disponible | Inspección de archivos |
| Java | ❌ no instalado | Requerido por Ghidra |
| Ghidra | ❌ no instalado | Instalar cuando haya binarios; proyecto fuera del repo |
| REA (`rea-agents`, CLI/MCP) | ❌ no instalado | Orquestación/evidencia; backend Hopper o Ghidra aportado; repo: `morluto/rea` (MIT) |
| radare2 / Cutter / x64dbg | ❌ no instalados | Alternativas a evaluar |
| Wine | ❌ no instalado | Solo útil para pruebas parciales; no sustituye Windows real |
| Windows de testing | ❌ no disponible | **Limitación actual**: reproducción y testing dinámico requieren PC Windows |

Espacio: Workspace ~125 MB (temporal). Antes de operaciones grandes:
comprobar espacio, minimizar copias locales, persistir en Drive.

## 2. Plan de preparación

1. [ ] Instalar Java + Ghidra (o fijar versión portable) cuando lleguen los
       primeros binarios; documentar versiones exactas.
2. [ ] Evaluar REA (`npx rea-agents`) como orquestador MCP/CLI sobre Ghidra.
3. [ ] Definir máquina Windows de testing (propia del mantenedor) + protocolo
       de captura (logs, FPS, vídeos, hashes).
4. [ ] Crear carpetas en Drive: `REZengineered/{originals,backups,analysis,builds,research}`.

## 3. Convenciones

- Originales: solo en Drive (`originals/`), con SHA-256 registrado.
- Copias de trabajo: locales y efímeras; nunca commitearlas.
- Artefactos grandes (proyectos Ghidra, dumps): Drive (`analysis/`).
- En el repo: solo docs, código propio, scripts y configs pequeñas.
