# REA en Windows — diagnóstico verificado, habilitación y uso real

> Estado: **DIAGNÓSTICO CERRADO (2026-10-16) · HABILITADO Y USADO EN WSL
> (2026-11-24, S-14)**. La vía elegida fue **WSL2 Ubuntu + Ghidra 12.1.4 +
> REA 6.7.0**, instalada local al usuario y sin `sudo`. El resultado verificado
> está en **§8**; el análisis que produjo se documenta en
> [`reports/scene/S14_MAP7_MENU_BACKGROUND.md`](reports/scene/S14_MAP7_MENU_BACKGROUND.md).
> Las secciones §1–§7 conservan el diagnóstico original (histórico) y no se
> reescriben.

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
  REA, variables solo de sesión, verificación SHA-256 (hash real completado) y
  reversión. No ejecutar sin autorización.
- **Recomendado:** usar **WSL/Linux/macOS** (§7) para analizar GEX3D.EXE (PE32
  x86), ya que las releases publicadas de REA en Windows solo admiten x86-64.

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

## 7. Alternativa recomendada: WSL / Linux / macOS con Ghidra

Dado que las *releases* publicadas de REA admiten en Windows solo PE x86-64
y `GEX3D.EXE` es PE32 x86, la vía más directa y sin coste para analizar Gex
es usar **Ghidra en Linux o macOS** (que admite PE x86 sin la restricción P0).

### 7.1 WSL (Windows Subsystem for Linux) — recomendado para el mantenedor

WSL viene incluido en Windows 10/11 y proporciona un entorno Linux completo
sin salir del PC.

1. **Activar WSL** (si no está activo):
   ```
   wsl --install
   ```
   (Requiere reinicio. Instala Ubuntu por defecto.)

2. **Instalar dependencias en WSL:**
   ```
   sudo apt update
   sudo apt install -y openjdk-21-jdk unzip curl
   ```

3. **Descargar e instalar Ghidra 12.1.4:**
   ```
   mkdir -p ~/.rezengineered-rea
   cd ~/.rezengineered-rea
   curl -L -o ghidra.zip https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.4_build/ghidra_12.1.4_PUBLIC_20250620.zip
   # Verificar SHA-256:
   echo "ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db  ghidra.zip" | sha256sum -c
   unzip ghidra.zip -d ghidra
   ```

4. **Instalar REA:**
   ```
   npm install -g rea-agents@6.7.0
   ```

5. **Configurar variables de sesión** (en `~/.bashrc` o `env.sh`):
   ```
   export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
   export GHIDRA_INSTALL_DIR=$HOME/.rezengineered-rea/ghidra/ghidra_12.1.4_PUBLIC
   export PATH=$JAVA_HOME/bin:$PATH
   ```

6. **Verificar:**
   ```
   java -version          # debe mostrar 21.x
   $GHIDRA_INSTALL_DIR/support/analyzeHeadless  # debe imprimir ayuda
   rea doctor --provider ghidra --json  # debe dar healthy:true
   ```

7. **Trabajo acotado** (sobre copia de `GEX3D.EXE`):
   ```
   rea inspect /path/to/copy/GEX3D.EXE --provider ghidra --json
   rea xrefs /path/to/copy/GEX3D.EXE 0x43E1BA --provider ghidra --json
   rea function /path/to/copy/GEX3D.EXE 0x43E1BA --provider ghidra --json
   ```

### 7.2 Notas
- En WSL/Linux, Ghidra admite PE x86 (32-bit) sin restricción P0.
- Los originales del juego deben estar en el filesystem de Windows accesible
  desde WSL (`/mnt/c/...`) o copiados al filesystem de WSL.
- Trabajar siempre sobre **copias**; originales intactos por hash.
- REA no instala ni actualiza Ghidra/Java; el usuario los instala manualmente.
- Reversión: borrar `~/.rezengineered-rea/`, `npm uninstall -g rea-agents`,
  quitar variables de `~/.bashrc`.

### 7.3 Comparación de vías

| Vía | Coste | Complejidad | Admite PE x86 (32-bit) |
|---|---|---|---|
| WSL + Ghidra (recomendado) | Gratuito | Media | Sí |
| Linux/macOS nativo + Ghidra | Gratuito | Media | Sí |
| IDA en Windows | Licencia de pago | Baja | Sí |
| Build REA desde `main` en Windows | Gratuito | Alta | Sí (sin P0) |
| REA published en Windows (6.7.0) | Gratuito | Baja | **No** (solo x86-64) |

## 6. Referencias
- `RESEARCH.md` §S-18 · `docs/TOOLKIT.md` §3–§6 · `PROJECT_STATE.md` ·
  `REVERSE_ENGINEERING.md` §1,§4 · `CHANGELOG.md` (2026-10-08, «Entorno RE
  sandbox operativo») · `tools/setup-re-env.sh` · `tools/setup-sandbox-re.sh`.
- REA: `github.com/morluto/rea` — `README.md`, `docs/native-investigation.md`,
  `docs/raw-binary.md`, `docs/windows-ghidra-p0.md`, `docs/cli.md`,
  `docs/installation.md`, `docs/reverse-engineering-tools.md`.

## 8. Habilitación verificada (2026-11-24, S-14)

Esta sección sustituye al «pendiente de autorización» de §5: la vía **WSL2 +
Ghidra + REA** se ejecutó y se usó para analizar `GEX3D.EXE` (PE32 x86).

### 8.1 Versiones y hashes

| Componente | Versión | Verificación |
|---|---|---|
| WSL | Ubuntu (WSL2) | `wsl -d Ubuntu` |
| JDK | **21.0.12.1** | `/usr/lib/jvm/java-21-openjdk-amd64` |
| Ghidra | **12.1.4 PUBLIC** | SHA-256 `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db` (zip `ghidra_12.1.4_PUBLIC_20260921.zip`, 569 649 598 B) |
| REA | **6.7.0** | `~/.npm-global/bin/rea` |
| `rea doctor --provider ghidra` | — | **`healthy:true`** (ghidra, version, headless, decompiler y java 21.0.12.1) |
| Copia analizada | `GEX3D.EXE` | SHA-256 `7a9b68516867db5460b6ec0d152841742192b1bb47198d2c7b0357d7aa3e8254` (1 559 040 B) |

Los avisos de `rea doctor` sobre `hopper`, `ida`, `jadx`, `kuna` y
`skill:identity` son irrelevantes para este carril y **no** afectan a Ghidra.

### 8.2 Capacidad PE32 x86 (CONFIRMADO)

`rea inspect` **acepta** `GEX3D.EXE`: `pe`, `x86`, `image_base 0x400000`,
2 725 procedimientos y 2 052 cadenas, con las secciones mapeadas. Esto confirma
el diagnóstico de §1/§5.1: la restricción x86-64 es de las *releases* en
**Windows**; en WSL/Linux el mismo REA 6.7.0 analiza PE32 x86 sin problema.

### 8.3 Instalación local (reversible, sin `sudo`)

```
~/.rezengineered-rea/
  ghidra/ghidra_12.1.4_PUBLIC/     # Ghidra
  work/GEX3D.EXE                   # copia verificada por hash
  work/*.json, *.pseudo.txt        # evidencias REA
  env.sh                           # JAVA_HOME, GHIDRA_INSTALL_DIR, PATH
~/.npm-global/                     # prefijo npm (rea)
```

- Variables **solo de sesión** (`source ~/.rezengineered-rea/env.sh`).
- **No** se tocó el Java del sistema ni se instaló nada con `sudo`.
- Reversión: borrar `~/.rezengineered-rea/`, `npm uninstall -g rea-agents` y
  quitar el `source` de `env.sh`.

### 8.4 Uso real

```
rea inspect   GEX3D.EXE --provider ghidra --json
rea decompile GEX3D.EXE 0x40172b --provider ghidra --format md
rea xrefs     GEX3D.EXE 0x650060 --provider ghidra --json
```

Cada `rea decompile` tarda ~20 s (incluye cabecera de evidencia con
`evidence_id`, digest y perfil de análisis). El resultado de S-14 está en
[`reports/scene/S14_MAP7_MENU_BACKGROUND.md`](reports/scene/S14_MAP7_MENU_BACKGROUND.md).

### 8.5 Límites observados

- `rea doctor` sigue marcando `healthy:false` en Windows nativo: no se usa.
- Los snapshots de Ghidra aceleran el análisis (`--snapshot`) pero no se
  versionan en el repo.
- Las evidencias (JSON, pseudocódigo, capturas) se guardan **fuera del repo**.
