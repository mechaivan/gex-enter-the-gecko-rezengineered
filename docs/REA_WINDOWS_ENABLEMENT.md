# REA en Windows — diagnóstico verificado y plan de habilitación

> Estado: **DIAGNÓSTICO CERRADO (2026-10-16) · HABILITACIÓN PENDIENTE DE
> AUTORIZACIÓN**. Este documento no ejecuta, descarga ni instala nada.
> Ninguna afirmación sobre resultados de REA está inventada: REA **no** se
> ha ejecutado y **no** ha analizado ningún binario de GEX.

## 1. Qué es REA (CONFIRMADO, documentation-based)

- REA (`morluto/rea`, MIT, TypeScript/Node; web `rea.tools`, paquete npm
  `rea-agents`) es una **capa de orquestación y evidencia** (CLI + servidor
  MCP), **no** un decompilador. Para análisis **nativo** necesita un
  **proveedor externo**: Hopper, Ghidra o IDA (cita literal de `README.md`:
  «Native analysis can use an existing Hopper, Ghidra, or IDA installation»).
  Para inspección offline acepta además GNU `objdump` y Rizin **aportados por
  el usuario** (no los instala); existen otros puentes (p. ej. Cutter, Kuna).
- El único carril nativo para PE en Windows es **Ghidra experimental (P0)**:
  solo PE x86/x86-64/ARMNT/ARM64, **solo lectura** y sobre volumen **local fijo
  NTFS**, en Windows 10+ x64 («Windows Ghidra P0 admits native PE
  applications/DLLs», `docs/raw-binary.md`; `docs/windows-ghidra-p0.md`).
- **Aviso de versión (crítico para GEX):** las *releases* publicadas (incluida la
  más reciente, **6.7.0**) admiten en Windows solo PE **x86-64**; el soporte
  **x86 (32-bit)** está solo en `main` (build desde fuente). `GEX3D.EXE` es
  PE32 **x86**, así que con una release publicada en Windows se **rechazaría**.
  Alternativas: (a) ejecutar REA desde `main` en Windows, (b) analizar en un host
  Linux/macOS con Ghidra (que admite PE x86), (c) usar IDA en Windows. Decisión
  a autorizar (ver §5.1).
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
  Ghidra/Hopper) — `docs/TOOLKIT.md` §3 y `CHANGELOG.md` (2026-10-08,
  «Entorno RE sandbox operativo»).

## 3. Estado verificado del host Windows (2026-10-16)

| Componente | Estado | Evidencia |
|---|---|---|
| Node.js | ✅ v26.7.0 | REA requiere 22.x (≥22.19), 24.x (≥24.11) o 26+ |
| npm | ✅ 12.1.0 | |
| `rea` CLI | ❌ **ausente** | ni global en npm (solo `codex`, `npm`, `pnpm`) |
| Registro MCP de REA | ❌ ausente | sin recurso MCP registrado |
| Ghidra / IDA / Hopper / Rizin | ❌ **ausentes** | no hallados en ubicaciones habituales |
| Java | ⚠️ **1.8.0_503** | insuficiente: Ghidra 12.1.x exige JDK 21 o superior |
| GNU `objdump.exe` | ❌ ausente | solo LLVM 22.1.8; `…\cmake-clang-v1\latest\bin\objdump` es un *wrapper* `.sh` de 2001 B |
| `gh` / token en entorno | ❌ ausentes | **no** bloquean un `push` con Git Credential Manager |

### 3.1 `tools/setup-re-env.sh` / `setup-re-env.ps1`: pendiente de probar

- `setup-re-env.sh` sigue marcado «pendiente de probar» (`tools/README.md`:9-11)
  y es **Linux-x64** (JDK linux `.tar.gz` + `/opt/rea-toolkit`): **no asumir
  validez en Windows.**
- Existe ahora un borrador Windows **`tools/setup-re-env.ps1`** (pendiente de
  probar): prefijo `%LOCALAPPDATA%\rezengineered-rea\`, JDK 21 + Ghidra 12.1.4 +
  REA, variables solo de sesión, verificación SHA-256 y reversión. No ejecutar
  sin autorización.

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

**Oportunidad:** con Ghidra como proveedor, REA puede completar el **grafo de
llamadas/xrefs** y el tipado del objeto de frame en torno a las direcciones ya
documentadas del path temporal de gameplay (exe EU original): update por frame
**`0x43E1BA`**, sello `elapsed/16.666`→`+0x10C` en **`0x43E5DD`** y cola de
swap **`0x43EA51`** (`REVERSE_ENGINEERING.md` §3, `TESTING.md`). Es justo el
hueco abierto para preparar una intervención mínima y reversible.

### 5.1 Dependencias
1. Node ≥22 + npm (presente).
2. **JDK 21** (Temurin) — instalación **local al prefijo**, sin tocar el Java 8 del sistema.
3. **Ghidra 12.1.x** (el proyecto fija `Ghidra_12.1.4_build`).
4. `rea-agents` (npm): **ninguna release publicada (incl. 6.7.0) admite PE x86
   (32-bit) en Windows**; solo x86-64. Para `GEX3D.EXE` (PE32 x86) hay que
   decidir: (a) ejecutar REA desde `main` (build desde fuente, más complejo),
   (b) analizar en host Linux/macOS con Ghidra, o (c) usar IDA en Windows.
   Versión **fijada** si se usa npm, **no** `@latest`.
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
  `env.ps1`, `evidence\` (volumen local fijo NTFS, como exige P0).
- Variables **solo de sesión** (en `env.ps1`): `JAVA_HOME`, `GHIDRA_INSTALL_DIR`
  (lo lee REA), `PATH`. **No** `setx`/sistema; **no** tocar el Java 8.
- Evidencia/snapshots (proyectos Ghidra) → Drive `REZengineered/analysis/`
  (`docs/TOOLKIT.md` §6).

### 5.5 Validación (solo con autorización; nunca ejecutar GEX)
1. `java -version` → 21.x.
2. `& "$env:GHIDRA_INSTALL_DIR\support\analyzeHeadless.bat"` imprime ayuda.
3. `npx -y rea-agents@<pin> doctor --provider ghidra --json` → `healthy:true`.
4. `rea inspect <copia>\GEX3D.EXE --provider ghidra --json` → PE32 x86
   (con una release publicada en Windows se rechazará: ver §1/§5.1).
5. Trabajo acotado: `rea xrefs <copia>\GEX3D.EXE 0x43E1BA --provider ghidra --json`
   y `rea function <copia>\GEX3D.EXE 0x43E1BA --provider ghidra --json`
   (opcional: `rea decompile` o `rea trace <copia>\GEX3D.EXE "<consulta>"`).

### 5.6 Reversión
- Borrar el prefijo `%LOCALAPPDATA%\rezengineered-rea\`.
- `npm uninstall -g rea-agents`.
- Quitar las variables (están solo en `env.ps1` de sesión).
- Sin cambios de sistema/registro/repo; originales verificados por hash intactos.

## 6. Referencias
- `RESEARCH.md` §S-18 · `docs/TOOLKIT.md` §3–§6 · `PROJECT_STATE.md` ·
  `REVERSE_ENGINEERING.md` §1,§4 · `CHANGELOG.md` (2026-10-08, «Entorno RE
  sandbox operativo») · `tools/setup-re-env.sh` · `tools/setup-sandbox-re.sh`.
- REA: `github.com/morluto/rea` — `README.md`, `docs/native-investigation.md`,
  `docs/raw-binary.md`, `docs/windows-ghidra-p0.md`, `docs/cli.md`,
  `docs/installation.md`, `docs/reverse-engineering-tools.md`.
