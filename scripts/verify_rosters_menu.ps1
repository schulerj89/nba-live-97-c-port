param([switch]$SkipBuild, [switch]$RequireReference)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Debug -AllTargets
    }
    $exe = Join-Path $repo 'build-windows\Debug\nba97_boot_decomp.exe'
    & $exe --self-test --trace '.local/logs/rosters_menu_self_test.log'
    if ($LASTEXITCODE -ne 0) { throw 'Rosters menu native self-test failed.' }
    & $exe --capture-rosters-menu '.local/verification/rosters_menu/native' `
        --trace '.local/logs/rosters_menu_capture.log'
    if ($LASTEXITCODE -ne 0) { throw 'Rosters menu deterministic capture failed.' }
    $arguments = @('tools/verify_rosters_menu.py', '--behavior-pass')
    if ($RequireReference) { $arguments += '--require-reference' }
    & python $arguments
    if ($LASTEXITCODE -ne 0) { throw 'Rosters menu fidelity verification failed.' }
} finally {
    Pop-Location
}
