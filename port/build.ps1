<#
.SYNOPSIS
    Build and check the native port components on Windows.

.DESCRIPTION
    There is no `make` in this environment, so this script mirrors the POSIX
    Makefile targets:

        .\build.ps1 test    build and run the native checks (container + audio
                            + the real-device backend, which self-skips when no
                            sound device exists)
        .\build.ps1 demo    build sagdump/sagplay and run their synthetic demos
        .\build.ps1 audio   build sagplay and decode the synthetic tone
        .\build.ps1 clean   remove build\ (only what this build produced)

    The compiler is looked up in PATH: clang first, then gcc. Override with
    -Compiler. Nothing here touches the retail executable or its assets.

    The device backend needs dsound.h and dsound.lib, which ship with the
    Windows toolchain (MinGW-w64/llvm-mingw). No third-party dependency is
    downloaded or vendored.
#>

[CmdletBinding()]
param(
    [ValidateSet('test', 'demo', 'audio', 'clean', 'all')]
    [string]$Target = 'all',
    [string]$Compiler
)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

if (-not $Compiler) {
    foreach ($candidate in @('clang', 'gcc')) {
        $found = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($found) { $Compiler = $candidate; break }
    }
}
if (-not $Compiler) {
    throw 'No C compiler found in PATH (tried clang, gcc). Pass -Compiler <path>.'
}

$buildDir = Join-Path $PSScriptRoot 'build'
$cflags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Iinclude')
$sagSrc = Join-Path $PSScriptRoot 'src/asset/sag.c'
$audioSrc = @(
    (Join-Path $PSScriptRoot 'src/audio/adpcm.c'),
    (Join-Path $PSScriptRoot 'src/audio/stream.c'),
    (Join-Path $PSScriptRoot 'src/audio/sched.c')
)
$deviceSrc = @((Join-Path $PSScriptRoot 'src/audio/backend_dsound.c'))
$coreSrc = @(
    (Join-Path $PSScriptRoot 'src/core/loop_core.c'),
    (Join-Path $PSScriptRoot 'src/core/frame.c')
)

function Build-Exe([string]$source, [string]$name, [string[]]$sources = @(), [string[]]$flags = @()) {
    $out = Join-Path $buildDir $name
    Write-Host "compile $name ($Compiler)"
    & $Compiler @cflags @flags -o $out $source $sagSrc @sources
    if ($LASTEXITCODE -ne 0) { throw "compilation failed: $name" }
    return $out
}

function Run-Check([string]$exe, [string]$label) {
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$label failed (exit $LASTEXITCODE)" }
}

if ($Target -eq 'clean') {
    if (Test-Path -LiteralPath $buildDir) {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
        Write-Host 'removed build\'
    }
    return
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

if ($Target -in @('test', 'all')) {
    Run-Check (Build-Exe (Join-Path $PSScriptRoot 'tests/test_sag.c') 'test_sag.exe') 'SLAG container checks'
    Run-Check (Build-Exe (Join-Path $PSScriptRoot 'tests/test_adpcm.c') 'test_adpcm.exe' $audioSrc) 'ADPCM checks'
    Run-Check (Build-Exe (Join-Path $PSScriptRoot 'tests/test_audio.c') 'test_audio.exe' $audioSrc) 'audio path checks'
    Run-Check (Build-Exe (Join-Path $PSScriptRoot 'tests/test_frame.c') 'test_frame.exe' ($coreSrc + $audioSrc)) 'frame loop checks'
    Run-Check (Build-Exe (Join-Path $PSScriptRoot 'tests/test_device.c') 'test_device.exe' ($audioSrc + $deviceSrc) @('-ldsound')) 'device backend checks'
}

if ($Target -in @('demo', 'all')) {
    $exe = Build-Exe (Join-Path $PSScriptRoot 'tools/sagdump.c') 'sagdump.exe'
    $fixture = Join-Path $buildDir 'demo.sag'
    node (Join-Path $PSScriptRoot 'tests/make_demo_sag.mjs') $fixture
    if ($LASTEXITCODE -ne 0) { throw 'fixture generation failed' }
    & $exe $fixture
    if ($LASTEXITCODE -ne 0) { throw "sagdump failed (exit $LASTEXITCODE)" }
}

if ($Target -in @('demo', 'all')) {
    $exe = Build-Exe (Join-Path $PSScriptRoot 'tools/framedemo.c') 'framedemo.exe' ($coreSrc + $audioSrc)
    & $exe --frames 4
    if ($LASTEXITCODE -ne 0) { throw "framedemo failed (exit $LASTEXITCODE)" }
    & $exe --decoupled --frames 3 --dt 60000 --audio
    if ($LASTEXITCODE -ne 0) { throw "framedemo --decoupled failed (exit $LASTEXITCODE)" }
}

if ($Target -in @('audio', 'demo', 'all')) {
    $exe = Build-Exe (Join-Path $PSScriptRoot 'tools/sagplay.c') 'sagplay.exe' ($audioSrc + $deviceSrc) @('-lm', '-ldsound')
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "sagplay failed (exit $LASTEXITCODE)" }
}

Write-Host 'ok'
