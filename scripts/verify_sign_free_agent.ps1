param([switch]$SkipBuild,[switch]$RequireAssets)
$ErrorActionPreference = 'Stop'
$signRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $signRepo
try {
    if (-not $SkipBuild) { & ./scripts/build.ps1 -Configuration Debug -AllTargets }
    python -m unittest discover -s tools -p test_sign*.py
    if ($LASTEXITCODE -ne 0) { throw 'Sign evidence regression failed.' }
    python -m unittest discover -s tools -p test_trade_assets.py
    if ($LASTEXITCODE -ne 0) { throw 'Shared Sign/Trade extraction regression failed.' }
    python tools/verify_sign_free_agent.py --check --native-test build-windows/Debug/nba97_sign_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Sign source/controller verification failed.' }
    & ./build-windows/Debug/nba97_trade_assets_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Sign/Trade asset parser regression failed.' }
    & ./build-windows/Debug/nba97_frontend_help_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Help route regression failed.' }
    if ($RequireAssets) {
        python tools/verify_sign_screen.py
        if ($LASTEXITCODE -ne 0) { throw 'Sign private host regression failed.' }
    }
} finally {
    Pop-Location
}
