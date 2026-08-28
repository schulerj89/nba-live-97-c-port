param([switch]$SkipBuild,[switch]$RequireAssets)
$ErrorActionPreference = 'Stop'
$tradeRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $tradeRepo
try {
    if (-not $SkipBuild) { & ./scripts/build.ps1 }
    python -m unittest discover -s tools -p test_trade_progress.py
    if ($LASTEXITCODE -ne 0) { throw 'Trade ledger regression failed.' }
    python tools/verify_trade_rosters.py --check --native-test build-windows/Debug/nba97_trade_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trade entry verification failed.' }
    & ./build-windows/Debug/nba97_trade_screen_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trade controller verification failed.' }
    & ./build-windows/Debug/nba97_trade_assets_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trade pack parser verification failed.' }
    python -m unittest discover -s tools -p test_trade_assets.py
    if ($LASTEXITCODE -ne 0) { throw 'Trade extraction verification failed.' }
    python -m unittest discover -s tools -p test_trade_screen.py
    if ($LASTEXITCODE -ne 0) { throw 'Trade host evidence regression failed.' }
    python -m unittest discover -s tools -p test_trade_recording.py
    if ($LASTEXITCODE -ne 0) { throw 'Trade live-recording evidence regression failed.' }
    if ($RequireAssets) {
        python tools/verify_trade_screen.py
        if ($LASTEXITCODE -ne 0) { throw 'Trade private host verification failed.' }
    }
} finally {
    Pop-Location
}
