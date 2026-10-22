#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Prepara el toolchain MSVC 6.0 portable (cl.exe, link.exe, lib.exe, headers, libs)
    y lo empaqueta como release asset para que el CI lo descargue en 1 paso.
    Ejecutar UNA VEZ en local (Windows), luego subir el .zip a GitHub Releases.
#>

$ErrorActionPreference = "Stop"
$OutZip = "msvc6-toolchain.zip"
$Stage = "C:\msvc6-stage"

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage\VC98\Bin, $Stage\VC98\Include, $Stage\VC98\Lib,
    $Stage\VC98\Crt\Include, $Stage\VC98\Crt\Lib, $Stage\VC98\Mfc\Include, $Stage\VC98\Mfc\Lib,
    $Stage\PSDK\Include, $Stage\PSDK\Lib, $Stage\PSDK\Bin -Force | Out-Null

Write-Host "Copiando VC++ 6.0 (requiere tenerlo instalado en C:\Program Files (x86)\Microsoft Visual Studio\VC98)..."
$srcVC = "C:\Program Files (x86)\Microsoft Visual Studio\VC98"
if (-not (Test-Path $srcVC)) {
    Write-Error "VC++ 6.0 no encontrado en $srcVC. Instálalo primero o cambia la ruta."
    exit 1
}
Copy-Item "$srcVC\Bin\cl.exe", "$srcVC\Bin\link.exe", "$srcVC\Bin\lib.exe",
    "$srcVC\Bin\rc.exe", "$srcVC\Bin\c1.exe", "$srcVC\Bin\c2.dll",
    "$srcVC\Bin\mspdb60.dll" -Destination "$Stage\VC98\Bin" -Force
Copy-Item "$srcVC\Include\*" -Destination "$Stage\VC98\Include" -Recurse -Force
Copy-Item "$srcVC\Lib\*" -Destination "$Stage\VC98\Lib" -Recurse -Force
Copy-Item "$srcVC\Crt\Include\*" -Destination "$Stage\VC98\Crt\Include" -Recurse -Force
Copy-Item "$srcVC\Crt\Lib\*" -Destination "$Stage\VC98\Crt\Lib" -Recurse -Force
Copy-Item "$srcVC\Mfc\Include\*" -Destination "$Stage\VC98\Mfc\Include" -Recurse -Force
Copy-Item "$srcVC\Mfc\Lib\*" -Destination "$Stage\VC98\Mfc\Lib" -Recurse -Force

Write-Host "Copiando Platform SDK Feb 1998 (C:\Program Files\Microsoft Platform SDK)..."
$srcPSDK = "C:\Program Files\Microsoft Platform SDK"
if (-not (Test-Path $srcPSDK)) {
    Write-Warning "Platform SDK no encontrado en $srcPSDK. El toolchain funcionará pero sin headers/libs del SDK."
} else {
    Copy-Item "$srcPSDK\Include\*" -Destination "$Stage\PSDK\Include" -Recurse -Force
    Copy-Item "$srcPSDK\Lib\*" -Destination "$Stage\PSDK\Lib" -Recurse -Force
    Copy-Item "$srcPSDK\Bin\*" -Destination "$Stage\PSDK\Bin" -Recurse -Force
}

# README con instrucciones de uso
@"
# MSVC 6.0 Toolchain (portable)

Binarios: cl.exe, link.exe, lib.exe, rc.exe, c1.exe, c2.dll, mspdb60.dll
Headers:  VC98/Include, VC98/Crt/Include, VC98/Mfc/Include, PSDK/Include
Libs:     VC98/Lib, VC98/Crt/Lib, VC98/Mfc/Lib, PSDK/Lib

## Uso en CI (GitHub Actions)

- name: Download MSVC 6.0 toolchain
  run: |
    curl -L -o msvc6.zip https://github.com/mechaivan/gex-enter-the-gecko-rezengineered/releases/download/msvc6-toolchain/msvc6-toolchain.zip
    7z x msvc6.zip -omsvc6
  shell: pwsh

- name: Setup environment
  run: |
    echo "C:\msvc6\VC98\Bin" | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append
    echo "INCLUDE=C:\msvc6\VC98\Include;C:\msvc6\VC98\Crt\Include;C:\msvc6\VC98\Mfc\Include;C:\msvc6\PSDK\Include" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    echo "LIB=C:\msvc6\VC98\Lib;C:\msvc6\VC98\Crt\Lib;C:\msvc6\VC98\Mfc\Lib;C:\msvc6\PSDK\Lib" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
  shell: pwsh
"@ | Set-Content "$Stage\README.txt" -Encoding utf8

Write-Host "Creando $OutZip..."
Compress-Archive -Path "$Stage\*" -DestinationPath $OutZip -Force
Write-Host "Hecho: $OutZip ($(($OutZip.length/1MB).ToString('N1')) MB)"
Write-Host "Súbelo a GitHub Releases (tag: msvc6-toolchain) y el workflow lo descargará."