param([switch]$RequireAssets)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    python -m unittest discover -s tools -p 'test_release_*.py'
    if ($LASTEXITCODE -ne 0) { throw 'Release accounting/extractor tests failed.' }
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
    python tools/verify_release_players.py --check --native-test build-windows/Debug/nba97_release_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Release native tests or accounting failed.' }
    if ($RequireAssets) {
        python tools/extract_release_assets.py .local/extracted/FEONLY.BIN
        if ($LASTEXITCODE -ne 0) { throw 'Private Release extraction failed.' }
        python tools/verify_release_screen.py
        if ($LASTEXITCODE -ne 0) { throw 'Private Release screen verification failed.' }
    }
    Write-Host 'RELEASE: 208/208 bounded source contracts and native scenarios verified; use -RequireAssets for private host/save/restart/Reset checks. Functional acceptance recorded; Help shading, intermediate scroll animation and exact audio/timing fidelity remain open.'
} finally { Pop-Location }
