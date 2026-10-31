<#
.SYNOPSIS
    Build and check the native port components on Windows.

.DESCRIPTION
    There is no `make` in this environment, so this script mirrors the POSIX
    Makefile targets:

        .\build.ps1 test    build and run the native checks
        .\build.ps1 demo    build sagdump and dump a synthetic fixture
        .\build.ps1 clean   remove build\ (only what this build produced)

    The compiler is looked up in PATH: clang first, then gcc. Override with
    -Compiler. Nothing here touches the retail executable or its assets.
#>

[CmdletBinding()]
param(
    [ValidateSet('test', 'demo', 'clean', 'all')]
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

function Build-Exe([string]$source, [string]$name) {
    $out = Join-Path $buildDir $name
    Write-Host "compile $name ($Compiler)"
    & $Compiler @cflags -o $out $source $sagSrc
    if ($LASTEXITCODE -ne 0) { throw "compilation failed: $name" }
    return $out
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
    $exe = Build-Exe (Join-Path $PSScriptRoot 'tests/test_sag.c') 'test_sag.exe'
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "native checks failed (exit $LASTEXITCODE)" }
}

if ($Target -in @('demo', 'all')) {
    $exe = Build-Exe (Join-Path $PSScriptRoot 'tools/sagdump.c') 'sagdump.exe'
    $fixture = Join-Path $buildDir 'demo.sag'
    node (Join-Path $PSScriptRoot 'tests/make_demo_sag.mjs') $fixture
    if ($LASTEXITCODE -ne 0) { throw 'fixture generation failed' }
    & $exe $fixture
    if ($LASTEXITCODE -ne 0) { throw "sagdump failed (exit $LASTEXITCODE)" }
}

Write-Host 'ok'
