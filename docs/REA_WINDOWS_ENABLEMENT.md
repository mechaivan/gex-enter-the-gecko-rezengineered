# REA en Windows — diagnóstico verificado y plan de habilitación

> Estado: **DIAGNÓSTICO CERRADO (2026-10-16) · HABILITACIÓN PENDIENTE DE
> AUTORIZACIÓN**. Este documento no ejecuta, descarga ni instala nada.
> Ninguna afirmación sobre resultados de REA está inventada: REA **no** se
> ha ejecutado y **no** ha analizado ningún binario de GEX.

## 1. Qué es REA (CONFIRMADO, documentation-based)

- REA (`morluto/rea`, MIT, TypeScript/Node; web `rea.tools`, paquete npm
  `rea-agents`) es una **capa de orquestación y evidencia** (CLI + servidor
  MCP), **no** un decompilador. Para análisis **nativo** necesita un
  **proveedor externo**: Ghidra, IDA, Hopper, Cutter, Rizin, o `objdump`
  GNU aportado por el usuario. Cita literal de `README.md`: «Native analysis
  can use an existing Hopper, Ghidra, or IDA installation».
- El carril nativo soportado en Windows para PE nativos es **Ghidra**
  («Windows Ghidra P0 admits native PE applications/DLLs», `docs/raw-binary.md`).
- REA aporta, cuando hay proveedor: `function`/`instructions`/`decompile`,
  **`xrefs`**, `procedure_references`, **`trace`** (trazas estáticas
  acotadas), tipado de estructuras, **snapshots Ghidra inmutables**
  (`snapshot_path`) y **`export_evidence_bundle`** — evidencia + limitaciones
  por conclusión (encaja con la política CONFIRMED/HYPOTHESIS de
  `REVERSE_ENGINEERING.md` §1).

## 2. Está documentada y adoptada, pero no demostrada sobre GEX

- Adoptada como herramienta oficial: `RESEARCH.md` §S-18; `docs/TOOLKIT.md`
  §3; `PROJECT_STATE.md`; `REVERSE_ENGINEERING.md` §4.
- **No existe evidencia de que REA haya analizado ningún binario de GEX.**
  Lo registrado es: instalación del CLI en el sandbox Linux y un
  `rea doctor` **negativo** (`healthy:false`; host no soportado, sin
  Ghidra/Hopper) — `docs/TOOLKIT.md`:35-40, `CHANGELOG.md`:1321-1328.

## 3. Estado verificado del host Windows (2026-10-16)

| Componente | Estado | Evidencia |
|---|---|---|
| Node.js | ✅ v26.7.0 | REA requiere Node 22+ |
| npm | ✅ 12.1.0 | |
| `rea` CLI | ❌ **ausente** | ni global en npm (solo `codex`, `npm`, `pnpm`) |
| Registro MCP de REA | ❌ ausente | sin recurso MCP registrado |
| Ghidra / IDA / Hopper / Rizin | ❌ **ausentes** | no hallados en ubicaciones habituales |
| Java | ⚠️ **1.8.0_503** | insuficiente: Ghidra 12.1.x exige JDK 21 |
| GNU `objdump.exe` | ❌ ausente | solo LLVM 22.1.8; `…\cmake-clang-v1\latest\bin\objdump` es un *wrapper* `.sh` de 2001 B |
| `gh` / token en entorno | ❌ ausentes | **no** bloquean un `push` con Git Credential Manager |

### 3.1 `tools/setup-re-env.sh`: pendiente y **Linux-only**

El script existe pero (a) está marcado «pendiente de probar»
(`tools/README.md`:6-10) y (b) usa un procedimiento **Linux**: descarga un
JDK **linux-x64** `.tar.gz` de Adoptium y descomprime Ghidra en
`/opt/rea-toolkit`. **No debe asumirse que sirva en Windows.** Se requiere un
equivalente `.ps1` (ver §5).

## 4. Binarios de GEX identificados (para cuando haya proveedor)

Candidatos que REA+Ghidra consumirían (siempre **sobre copias**, originales
intactos por hash — `REVERSE_ENGINEERING.md` §1):

- `GEX3D.EXE` original EU — md5 `692B1282…`; PE32 x86 (Glide).
- Exe **F-03** EU (build distinto) y exe **2013 NTSC-D3D** (F-02/S-14), para diff.
- DLLs Glide del juego.

Limitaciones actuales: sin decompilador local el análisis se hace hoy con
`llvm-objdump`; el hueco abierto es el **grafo de llamadas/xrefs** del motor
de timing/cámara.

## 5. Oportunidad y plan de habilitación en Windows (PENDIENTE DE AUTORIZACIÓN)

**Oportunidad:** con Ghidra como proveedor, REA puede producir el grafo de
llamadas y tipar el objeto de frame en torno a la función de frame
**`0x43EB76`** (dirección virtual ya identificada en GEX), justo lo que falta
para preparar una intervención mínima y reversible.

### 5.1 Dependencias
1. Node ≥22 + npm (presente).
2. **JDK 21** (Temurin) — instalación **local al prefijo**, sin tocar el Java 8 del sistema.
3. **Ghidra 12.1.x** (el proyecto fija `Ghidra_12.1.4_build`).
4. `rea-agents` (npm), versión **fijada** (no `@latest`).
5. (Opcional) GNU `objdump` si se quiere el carril `inspect_with_objdump`.

### 5.2 Descargas necesarias (a autorizar)
- Temurin JDK 21 Windows x64 (Adoptium API/releases).
- Ghidra 12.1.4 PUBLIC `.zip` (NSA/GitHub releases).
- Paquete npm `rea-agents`.

### 5.3 Verificación de integridad
- Temurin: cotejar SHA-256 con el `checksum` del `package` en la API de
  Adoptium (`…/v3/assets/latest/21/hotspot?os=windows&architecture=x64`).
- Ghidra: cotejar SHA-256 con el digest publicado en las notas/release.
- npm: `npm audit signatures` + versión fijada (provenance si está disponible).
- Registrar los hashes obtenidos en este documento antes de usar.

### 5.4 Ubicaciones de instalación (fuera del repo)
- Prefijo: `%LOCALAPPDATA%\rezengineered-rea\` con `jdk21\`, `ghidra\`,
  `env.ps1`, `evidence\`.
- Variables **solo de sesión** (en `env.ps1`): `JAVA_HOME`, `GHIDRA_INSTALL_DIR`
  (lo lee REA), `PATH`. **No** `setx`/sistema; **no** tocar el Java 8.
- Evidencia/snapshots (proyectos Ghidra) → Drive `REZengineered/analysis/`
  (`docs/TOOLKIT.md` §6).

### 5.5 Validación (solo con autorización; nunca ejecutar GEX)
1. `java -version` → 21.x.
2. `& "$env:GHIDRA_INSTALL_DIR\support\analyzeHeadless.bat"` imprime ayuda.
3. `npx -y rea-agents@<pin> doctor --provider ghidra --json` → `healthy:true`.
4. `rea inspect <copia>\GEX3D.EXE --provider ghidra --json` → PE32 x86.
5. Trabajo acotado: `rea xrefs <copia>\GEX3D.EXE 0x43EB76 --provider ghidra --json`
   y `rea function`/`rea trace` sobre `0x43EB76`.

### 5.6 Reversión
- Borrar el prefijo `%LOCALAPPDATA%\rezengineered-rea\`.
- `npm uninstall -g rea-agents`.
- Quitar las variables (están solo en `env.ps1` de sesión).
- Sin cambios de sistema/registro/repo; originales verificados por hash intactos.

## 6. Referencias
- `RESEARCH.md` §S-18 · `docs/TOOLKIT.md` §3–§6 · `PROJECT_STATE.md` ·
  `REVERSE_ENGINEERING.md` §1,§4 · `CHANGELOG.md`:1321-1328 ·
  `tools/setup-re-env.sh` · `tools/setup-sandbox-re.sh`.
- REA: `github.com/morluto/rea` — `README.md`, `docs/native-investigation.md`,
  `docs/raw-binary.md`, `docs/reverse-engineering-tools.md`, `docs/cli.md`.
