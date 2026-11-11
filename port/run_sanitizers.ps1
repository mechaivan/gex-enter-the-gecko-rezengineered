$root = 'C:\Users\mechaivan\Documents\GitHub\GEX-REZengineered\port'
Set-Location -LiteralPath $root
$OUTDIR = 'C:\Users\mechaivan\AppData\Local\Temp\opencode'
$Compiler = 'clang'
$cameraSrc = @((Join-Path $root 'src/camera/camera.c'))
$bspSrc = @((Join-Path $root 'src/bsp/bsp.c'))
$dfxBspSrc = @((Join-Path $root 'src/asset/dfx.c'))
$vfxSrc = @((Join-Path $root 'src/asset/vfx.c'))
$vfxDecodeSrc = @((Join-Path $root 'src/asset/vfx_decode.c'))
$vfxTexSrc = @((Join-Path $root 'src/asset/vfx_texture.c'))
$emitSrc = @((Join-Path $root 'src/emit/emit.c'))
$sagSrc = @((Join-Path $root 'src/asset/sag.c'))
$geometrySrc = @((Join-Path $root 'src/geometry/geometry.c'))
$sceneSrc = @((Join-Path $root 'src/scene/scene_level.c'))

$variants = @(
  @{ n = 'O0';    c = @('-std=c11','-O0','-Wall','-Wextra','-Iinclude') },
  @{ n = 'O2';    c = @('-std=c11','-O2','-Wall','-Wextra','-Iinclude') },
  @{ n = 'asan';  c = @('-std=c11','-O1','-g','-fsanitize=address','-fno-omit-frame-pointer','-Wall','-Wextra','-Iinclude') },
  @{ n = 'ubsan'; c = @('-std=c11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-Wall','-Wextra','-Iinclude') }
)
$fail = @()
foreach ($v in $variants) {
  foreach ($t in @(
      @{ s = 'tests/test_camera.c';      n = 'test_camera';      x = @($cameraSrc + $bspSrc); f = @() },
      @{ s = 'tests/test_bsp.c';         n = 'test_bsp';         x = @($bspSrc + $cameraSrc + $dfxBspSrc); f = @('-DGEX_BSP_TEST_FAULT_INJECT', '-I' + (Join-Path $root 'tests')) },
      @{ s = 'tests/test_level_emit.c';  n = 'test_level_emit';  x = @($dfxBspSrc + $bspSrc + $cameraSrc + $emitSrc); f = @('-I' + (Join-Path $root 'tests')) },
      @{ s = 'tests/test_scene_level.c'; n = 'test_scene_level'; x = @($sceneSrc + $geometrySrc + $emitSrc + $bspSrc + $cameraSrc + $dfxBspSrc); f = @('-I' + (Join-Path $root 'tests')) },
      @{ s = 'tests/test_vfx.c';        n = 'test_vfx';        x = @($vfxSrc + $vfxDecodeSrc + $vfxTexSrc + $dfxBspSrc + $emitSrc + $bspSrc + $cameraSrc); f = @('-I' + (Join-Path $root 'tests')) })) {
    $exe = Join-Path $OUTDIR ("{0}_{1}.exe" -f $t.n, $v.n)
    & $Compiler @($v.c) -o $exe (Join-Path $root $t.s) $sagSrc @($t.x) @($t.f) @('-lm') 2>&1 |
      Where-Object { $_ -match 'error' } | ForEach-Object { Write-Host "ERROR $_" }
    & $exe 2>&1 | Where-Object { $_ -match '(^FAIL |runtime error|ERROR:|error:)' } |
      ForEach-Object { Write-Host "FAIL $_" }
    if ($LASTEXITCODE -ne 0) {
      $fail += "$($t.n)/$($v.n)"
      Write-Host "BUILD FAIL $($t.n)/$($v.n)"
    } else {
      Write-Host "OK $($t.n)/$($v.n)"
    }
  }
}
if ($fail.Count -gt 0) {
  Write-Host ("SANITIZERS + O0/O2: FALLOS: " + ($fail -join ', '))
  exit 1
}
Write-Host "SANITIZERS + O0/O2: TODO OK"
