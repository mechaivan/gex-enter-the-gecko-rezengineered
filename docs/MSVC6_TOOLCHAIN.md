# MSVC 6.0 Toolchain — cómo crear el release asset

Este documento explica cómo generar `msvc6-toolchain.zip` y publicarlo en GitHub Releases
para que el CI (`msvc6-build.yml`) lo descargue y compile con el toolchain exacto que
usa el binario original (`linker 5.00` → MSVC 5.0/6.0).

## 1. Requisitos previos (en tu máquina Windows)

| Componente | Versión | Dónde conseguirlo |
|---|---|---|
| Visual C++ 6.0 Professional/Enterprise | 6.0 (SP6) | CDs originales / MSDN / Archive.org (`MSDN_Visual_C++_6.0_Professional_Edition.img`) |
| Platform SDK | February 1998 | Archive.org (`microsoft-platform-sdk-february-1998`) |
| 7-Zip | ≥ 19.00 | https://www.7-zip.org/ |
| Git + GitHub CLI (`gh`) | — | https://cli.github.com/ |

> **Nota:** VC++ 6.0 se instala en `C:\Program Files (x86)\Microsoft Visual Studio\VC98`.
> El Platform SDK Feb 1998 se instala en `C:\Program Files\Microsoft Platform SDK`.

## 2. Ejecutar el script de empaquetado

```powershell
# En PowerShell (ejecutar como administrador si VC6 está en Program Files)
cd C:\ruta\a\GEX-REZengineered
.\tools\package-msvc6.ps1
```

El script:
1. Crea `C:\msvc6-stage` con la estructura `VC98/` + `PSDK/`.
2. Copia binarios (`cl.exe`, `link.exe`, `lib.exe`, `rc.exe`, `c1.exe`, `c2.dll`, `mspdb60.dll`), headers y libs.
3. Si el Platform SDK Feb 1998 no está, avisa pero continúa (el toolchain funciona para código que no use APIs del SDK).
4. Genera `msvc6-toolchain.zip` en el directorio actual.
5. Escribe `README.txt` con instrucciones de uso en CI.

Verifica el zip:
```powershell
7z l msvc6-toolchain.zip
# Debe contener: VC98/Bin/cl.exe, VC98/Include/, VC98/Lib/, PSDK/Include/, PSDK/Lib/, etc.
```

## 3. Publicar en GitHub Releases

```powershell
# Crear release (tag: msvc6-toolchain) y subir asset
gh release create msvc6-toolchain msvc6-toolchain.zip \
  --title "MSVC 6.0 Toolchain (portable)" \
  --notes "VC++ 6.0 SP6 + Platform SDK Feb 1998 — portable toolchain for GEX matching decompilation CI"
```

El asset quedará en:
```
https://github.com/mechaivan/gex-enter-the-gecko-rezengineered/releases/download/msvc6-toolchain/msvc6-toolchain.zip
```

## 4. Qué instala el workflow `msvc6-build.yml`

En `windows-latest`:
1. Descarga `msvc6-toolchain.zip` desde el release.
2. Extrae a `msvc6/`.
3. Añade `msvc6\VC98\Bin` al `PATH`.
4. Setea `INCLUDE` y `LIB` con las rutas del toolchain.
5. `cl.exe`, `link.exe`, `lib.exe` quedan disponibles en el resto de steps.

## 5. Qué falta para el matching real (objdiff)

Con el toolchain listo, el pipeline de matching es:

1. **Splitter**: partir `GEX3D.EXE` en `.obj` por unidad (`.obj` objetivo).
   - Herramientas: `llvm-objcopy --extract-symbol`, `split.py` casero, o `IDA Pro` + script.
   - Requiere `symbols.txt` (función → dirección → fichero origen).

2. **C esqueleto**: escribir el `.c` de la primera unidad (ej. `game/timing.c` para `0x43E1BA`).

3. **Compilar**: `cl.exe /Fo timing.obj /c timing.c` (flags `/Od /Ob0 /Oi- /Oy- /Gd /MT /Gy /Z7`).

4. **objdiff**: `objdiff diff --target target.obj --base timing.obj --format json`.

5. **CI**: subir `EU_report.json` (objdiff report) → bytematch.dev.

## 6. Referencias de toolchain MSVC 6.0 reproducible

| Proyecto | Cómo lo resuelven |
|---|---|
| **dx-ball** (N0zoM1z0) | Dockerfile con VC6 + PSDK preinstalados en imagen custom |
| **Halo PC (halo-re)** | `vc6-toolchain.zip` como release asset (igual que aquí) |
| **reVC / re3** | Imagen Docker `ghcr.io/.../vc6` publicada en GHCR |
| **OpenLara** | Toolchain GCC/PSY-Q (PSX), no MSVC |

La estrategia del **release asset zip** es la más portable: no requiere Docker en el runner, funciona en `windows-latest` estándar, y el asset es inmutable (SHA-256 fijo).

## 7. Checklist de verificación

- [ ] `msvc6-toolchain.zip` subido a release `msvc6-toolchain`
- [ ] Workflow `msvc6-build.yml` pasa (compile + objdiff smoke test)
- [ ] Primera unidad real definida en `symbols.txt`
- [ ] Primer `.obj` objetivo extraído del `GEX3D.EXE`
- [ ] Primer `.c` escrito y compilado con toolchain VC6
- [ ] `objdiff diff` produce diff válido
- [ ] `objdiff report generate` produce `EU_report.json` con matched > 0
- [ ] bytematch.dev muestra % > 0

---

> **Nota:** El toolchain VC6 es **inmutable** una vez publicado. Si necesitas actualizarlo (ej. añadir un header del SDK), crea un nuevo tag `msvc6-toolchain-v2` y actualiza la URL en el workflow.