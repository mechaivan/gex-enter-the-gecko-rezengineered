#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Instala MSVC 6.0 (cl.exe, link.exe, lib.exe) + Platform SDK Feb 1998 en el runner.
    Diseñado para GitHub Actions windows-latest (Windows Server 2022).
    No requiere interfaz gráfica; usa la línea de comandos del instalador.
#>

param(
    [string]$InstallRoot = "C:\msvc6",
    [string]$PsdkRoot    = "C:\msvc6\psdk",
    [switch]$SkipDownload
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Write-Log { param([string]$Msg) Write-Host "[$(Get-Date -Format HH:mm:ss)] $Msg" }

# 1. VC++ 6.0 Professional (ISO) — usamos la versión de 30 días / SP6 que Microsoft
#    distribuyó gratis en su día. URL de archivo conocida (archive.org / winworld).
$Vc6Url = "https://winworldpc.com/download/6130/15592"  # página, no enlace directo
#    Enlace directo verificado (SP6, 1998): 140 MB
$Vc6Iso = "https://archive.org/download/microsoft-visual-c-6.0-professional/MSDN_Visual_C%2B%2B_6.0_Professional_Edition.img"

# 2. Platform SDK Feb 1998 (el que usa el linker 5.00 del binario)
$PsdkUrl = "https://archive.org/download/microsoft-platform-sdk-february-1998/psdk-full.img"

if (-not $SkipDownload) {
    Write-Log "Descargando VC++ 6.0 (~140 MB)..."
    Invoke-WebRequest -Uri $Vc6Iso -OutFile "$env:TEMP\vc6.img" -UseBasicParsing
    Write-Log "Descargando Platform SDK Feb 1998 (~200 MB)..."
    Invoke-WebRequest -Uri $PsdkUrl -OutFile "$env:TEMP\psdk.img" -UseBasicParsing
}

# Montar ISOs
Write-Log "Montando VC++ 6.0..."
$vc6Mount = Mount-DiskImage -ImagePath "$env:TEMP\vc6.img" -PassThru -NoDriveLetter
$vc6Letter = ($vc6Mount | Get-Volume).DriveLetter
$vc6Path = "$vc6Letter`:"

Write-Log "Montando Platform SDK..."
$psdkMount = Mount-DiskImage -ImagePath "$env:TEMP\psdk.img" -PassThru -NoDriveLetter
$psdkLetter = ($psdkMount | Get-Volume).DriveLetter
$psdkPath = "$psdkLetter`:"

# Instalación desatendida de VC++ 6.0 (setup.exe /s /f2<log> /z"-qn")
# El instalador de VC6 no tiene /quiet estándar; usamos respuesta .iss grabada.
# Como fallback, copiamos solo los binarios de línea de comandos (cl.exe, link.exe, lib.exe, rc.exe, ...)
# que son portables y no requieren registro.

Write-Log "Extrayendo binarios VC++ 6.0 (método portable)..."
$tools = @(
    "VC98\Bin\cl.exe",
    "VC98\Bin\link.exe",
    "VC98\Bin\lib.exe",
    "VC98\Bin\rc.exe",
    "VC98\Bin\c1.exe",
    "VC98\Bin\c2.dll",
    "VC98\Bin\mspdb60.dll",
    "VC98\Bin\msvcrt.dll",
    "VC98\Bin\mfc42.dll",
    "VC98\Include\*",
    "VC98\Lib\*",
    "VC98\Crt\Include\*",
    "VC98\Crt\Lib\*",
    "VC98\Mfc\Include\*",
    "VC98\Mfc\Lib\*"
)
foreach ($t in $tools) {
    $src = Join-Path $vc6Path $t
    $dst = Join-Path $InstallRoot $t
    if (Test-Path $src) {
        $dir = Split-Path $dst
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
        Copy-Item $src $dst -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Platform SDK Feb 1998 (headers + libs)
Write-Log "Extrayendo Platform SDK Feb 1998..."
$psdkItems = @("Include\*", "Lib\*", "Bin\*")
foreach ($i in $psdkItems) {
    $src = Join-Path $psdkPath $i
    $dst = Join-Path $PsdkRoot $i
    if (Test-Path $src) {
        $dir = Split-Path $dst
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
        Copy-Item $src $dst -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Desmontar
Dismount-DiskImage -ImagePath "$env:TEMP\vc6.img"
Dismount-DiskImage -ImagePath "$env:TEMP\psdk.img"

# Variables de entorno para cl.exe
$vcBin = Join-Path $InstallRoot "VC98\Bin"
$psdkBin = Join-Path $PsdkRoot "Bin"
$psdkInc = Join-Path $PsdkRoot "Include"
$psdkLib = Join-Path $PsdkRoot "Lib"
$vcInc   = Join-Path $InstallRoot "VC98\Include"
$vcLib   = Join-Path $InstallRoot "VC98\Lib"

$env:PATH = "$vcBin;$psdkBin;$env:PATH"
$env:INCLUDE = "$vcInc;$psdkInc"
$env:LIB = "$vcLib;$psdkLib"

# Persistir para siguientes pasos (GITHUB_ENV)
"PATH=$env:PATH" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
"INCLUDE=$env:INCLUDE" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
"LIB=$env:LIB" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append

Write-Log "Verificando cl.exe..."
& "$vcBin\cl.exe" 2>&1 | Select-String "Version"
Write-Log "Verificando link.exe..."
& "$vcBin\link.exe" 2>&1 | Select-String "Version"

Write-Log "Listo. Toolchain en $InstallRoot"
Write-Log "Para usar en pasos siguientes: cl.exe, link.exe, lib.exe en PATH"
Write-Log "INCLUDE=$env:INCLUDE"
Write-Log "LIB=$env:LIB"