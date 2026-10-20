# tools/setup-re-env.ps1
# Entorno completo de RE para Windows: JDK 21 (Temurin) + Ghidra 12.1.4 + REA CLI.
# Equivalente Windows de tools/setup-re-env.sh (que es Linux-only y pendiente de probar).
#
# ESTADO: BORRADOR, PENDIENTE DE PROBAR. No ejecuta nada por defecto fuera de
#         verificaciones de solo lectura. Requiere autorizacion expresa del
#         mantenedor para descargar/instalar.
#
# Restricciones honradas:
#   - Todo vive fuera del repo (%LOCALAPPDATA%\rezengineered-rea\).
#   - Variables SOLO de sesion (env.ps1); nunca setx/sistema.
#   - No toca el Java 8 del sistema.
#   - Originales del juego intactos (trabajo siempre sobre copias).
#   - No ejecuta GEX.
#
# Uso (cuando se autorice):
#   powershell -ExecutionPolicy Bypass -File .\tools\setup-re-env.ps1 [-Prefix <dir>] [-SkipDownloads]
#   . <prefix>\env.ps1
#   rea doctor --provider ghidra --json
#
# Reversion: borrar <prefix>, npm uninstall -g rea-agents, quitar variables de sesion.

[CmdletBinding()]
param(
    [string]$Prefix = (Join-Path $env:LOCALAPPDATA "rezengineered-rea"),
    [string]$JdkVersion = "21",
    [string]$GhidraTag = "Ghidra_12.1.4_build",
    [string]$GhidraZipUrl = "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.4_build/ghidra_12.1.4_PUBLIC_20250620.zip",
    # Hash SHA-256 publicado por NSA para Ghidra 12.1.4 PUBLIC. COMPLETAR antes de usar.
    [string]$GhidraSha256 = "REEMPLAZAR_CON_HASH_PUBLICADO",
    [string]$ReaVersion = "6.7.0",
    [switch]$SkipDownloads,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Write-Step([string]$msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok([string]$msg)   { Write-Host "    OK: $msg" -ForegroundColor Green }
function Write-Warn([string]$msg)  { Write-Host "    AVISO: $msg" -ForegroundColor Yellow }
function Write-Fail([string]$msg)  { Write-Host "    ERROR: $msg" -ForegroundColor Red }

# --- 0. Verificaciones de solo lectura ---
Write-Step "Verificando prerrequisitos de solo lectura"
$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { Write-Fail "Node.js no encontrado (REA requiere 22.x/24.x/26+)"; exit 1 }
$nodeVer = (node --version).Trim()
Write-Ok "Node.js $nodeVer"
$npm = Get-Command npm -ErrorAction SilentlyContinue
if (-not $npm) { Write-Fail "npm no encontrado"; exit 1 }
$npmVer = (npm --version).Trim()
Write-Ok "npm $npmVer"

# AVISO CRITICO: releases publicadas de rea admiten en Windows solo PE x86-64.
Write-Warn "AVISO: las releases publicadas de rea (incl. $ReaVersion) admiten en Windows"
Write-Warn "       solo PE x86-64. GEX3D.EXE es PE32 x86 -> se rechazaria."
Write-Warn "       Alternativas: (a) ejecutar REA desde 'main' (build desde fuente),"
Write-Warn "       (b) analizar en host Linux/macOS con Ghidra, (c) usar IDA en Windows."

if ($DryRun) {
    Write-Host "`n[DryRun] Prefijo: $Prefix" -ForegroundColor Yellow
    Write-Host "[DryRun] No se descarga ni instala nada." -ForegroundColor Yellow
    exit 0
}

# --- 1. Crear prefijo ---
Write-Step "Creando prefijo $Prefix"
$dirs = @("jdk$JdkVersion", "ghidra", "evidence")
foreach ($d in $dirs) {
    $p = Join-Path $Prefix $d
    if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p -Force | Out-Null }
}
Write-Ok "Prefijo creado"

# --- 2. JDK 21 (Temurin) ---
$jdkDir = Join-Path $Prefix "jdk$JdkVersion"
$jdkMarker = Join-Path $jdkDir "bin\java.exe"
if (-not (Test-Path $jdkMarker)) {
    if ($SkipDownloads) { Write-Warn "SkipDownloads: JDK no instalado, saltando"; }
    else {
        Write-Step "Descargando Temurin JDK $JdkVersion (Adoptium)"
        # URL de la API de Adoptium para el ultimo JDK 21 Windows x64.
        $jdkApi = "https://api.adoptium.net/v3/binary/latest/$JdkVersion/ga/windows/x64/jdk/hotspot/normal/eclipse"
        $jdkZip = Join-Path $Prefix "temurin-jdk$JdkVersion.zip"
        Write-Host "    URL: $jdkApi"
        # NOTA: verificar SHA-256 con el 'checksum' del package de la API de Adoptium.
        curl.exe -L -o $jdkZip $jdkApi
        if ($LASTEXITCODE -ne 0) { Write-Fail "Descarga JDK fallo"; exit 1 }
        Write-Step "Extrayendo JDK"
        Expand-Archive -Path $jdkZip -DestinationPath $jdkDir -Force
        # Adoptium extrae en una subcarpeta; aplanar si es necesario.
        $sub = Get-ChildItem $jdkDir -Directory | Where-Object { $_.Name -like "jdk-*" } | Select-Object -First 1
        if ($sub) {
            Get-ChildItem $sub.FullName | Move-Item -Destination $jdkDir -Force
            Remove-Item $sub.FullName -Recurse -Force -ErrorAction SilentlyContinue
        }
        Remove-Item $jdkZip -Force -ErrorAction SilentlyContinue
        Write-Ok "JDK instalado en $jdkDir"
    }
} else {
    Write-Ok "JDK ya presente en $jdkDir"
}

# --- 3. Ghidra 12.1.4 ---
$ghidraDir = Join-Path $Prefix "ghidra"
$ghidraMarker = Join-Path $ghidraDir "support\analyzeHeadless.bat"
if (-not (Test-Path $ghidraMarker)) {
    if ($SkipDownloads) { Write-Warn "SkipDownloads: Ghidra no instalado, saltando"; }
    else {
        if ($GhidraSha256 -eq "REEMPLAZAR_CON_HASH_PUBLICADO") {
            Write-Fail "GhidraSha256 no configurado. Obtener el hash publicado por NSA y pasarlo con -GhidraSha256."
            exit 1
        }
        Write-Step "Descargando Ghidra $GhidraTag"
        $ghidraZip = Join-Path $Prefix "ghidra.zip"
        curl.exe -L -o $ghidraZip $GhidraZipUrl
        if ($LASTEXITCODE -ne 0) { Write-Fail "Descarga Ghidra fallo"; exit 1 }
        Write-Step "Verificando SHA-256 de Ghidra"
        $actual = (Get-FileHash $ghidraZip -Algorithm SHA256).Hash
        if ($actual -ne $GhidraSha256) {
            Write-Fail "SHA-256 no coincide: esperado $GhidraSha256, obtenido $actual"
            Remove-Item $ghidraZip -Force -ErrorAction SilentlyContinue
            exit 1
        }
        Write-Ok "SHA-256 verificado"
        Write-Step "Extrayendo Ghidra"
        Expand-Archive -Path $ghidraZip -DestinationPath $ghidraDir -Force
        $sub = Get-ChildItem $ghidraDir -Directory | Where-Object { $_.Name -like "ghidra_*" } | Select-Object -First 1
        if ($sub) {
            Get-ChildItem $sub.FullName | Move-Item -Destination $ghidraDir -Force
            Remove-Item $sub.FullName -Recurse -Force -ErrorAction SilentlyContinue
        }
        Remove-Item $ghidraZip -Force -ErrorAction SilentlyContinue
        Write-Ok "Ghidra instalado en $ghidraDir"
    }
} else {
    Write-Ok "Ghidra ya presente en $ghidraDir"
}

# --- 4. REA CLI ---
$rea = Get-Command rea -ErrorAction SilentlyContinue
if (-not $rea) {
    Write-Step "Instalando rea-agents@$ReaVersion (npm, global)"
    npm install -g "rea-agents@$ReaVersion"
    if ($LASTEXITCODE -ne 0) { Write-Fail "npm install fallo"; exit 1 }
    Write-Ok "rea instalado"
} else {
    Write-Ok "rea ya presente: $(rea --version 2>&1 | Select-Object -First 1)"
}

# --- 5. Generar env.ps1 (solo sesion) ---
$envFile = Join-Path $Prefix "env.ps1"
$envContent = @"
# Generado por tools/setup-re-env.ps1 - NO commitear.
# Variables SOLO de sesion: no setx, no sistema, no tocar Java 8.
`$env:JAVA_HOME = "$jdkDir"
`$env:GHIDRA_INSTALL_DIR = "$ghidraDir"
if (-not (`$env:PATH -split ';' | Where-Object { `$_ -eq (Join-Path '$jdkDir' 'bin') })) {
    `$env:PATH = (Join-Path '$jdkDir' 'bin') + ';' + `$env:PATH
}
Write-Host "Entorno RE activado: JAVA_HOME=`$env:JAVA_HOME ; GHIDRA_INSTALL_DIR=`$env:GHIDRA_INSTALL_DIR" -ForegroundColor Green
"@
Set-Content -Path $envFile -Value $envContent -Encoding UTF8
Write-Ok "env.ps1 generado en $envFile"

# --- 6. Verificacion final ---
Write-Step "Verificacion final"
& (Join-Path $jdkDir "bin\java.exe") -version 2>&1 | ForEach-Object { Write-Host "    $_" }
if (Test-Path $ghidraMarker) { Write-Ok "analyzeHeadless.bat presente" } else { Write-Warn "analyzeHeadless.bat NO presente" }
Write-Host ""
Write-Host "Activar entorno:  . $envFile" -ForegroundColor Yellow
Write-Host "Validar REA:      rea doctor --provider ghidra --json" -ForegroundColor Yellow
Write-Host "Inspeccionar:     rea inspect <copia>\GEX3D.EXE --provider ghidra --json" -ForegroundColor Yellow
Write-Host ""
Write-Warn "Recuerda: con una release publicada, PE x86 (32-bit) se rechaza en Windows."
Write-Warn "Ver docs/REA_WINDOWS_ENABLEMENT.md (Aviso de version)."

# --- 7. Reversion (documentada) ---
# Borrar $Prefix ; npm uninstall -g rea-agents ; quitar variables de sesion.
Write-Ok "Listo. Prefijo: $Prefix"
