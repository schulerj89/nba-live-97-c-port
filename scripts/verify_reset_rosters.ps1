param([switch]$RequireAssets)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    python -m unittest discover -s tools -p 'test_reset_rosters.py'
    if ($LASTEXITCODE -ne 0) { throw 'Reset accounting tests failed' }
    python -m unittest discover -s tools -p 'test_roster_reset.py'
    if ($LASTEXITCODE -ne 0) { throw 'Reset extraction tests failed' }
    python -m unittest discover -s tools -p 'test_reset_release_host.py'
    if ($LASTEXITCODE -ne 0) { throw 'Release/Reset evidence tests failed' }
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
    python tools/verify_reset_rosters.py --check --native-test build-windows/Debug/nba97_roster_reset_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Reset core tests/accounting failed' }
    if ($RequireAssets) {
        python tools/verify_reset_release_host.py
        if ($LASTEXITCODE -ne 0) { throw 'Short Release/Reset host verification failed' }
        python tools/verify_reorder_save_host.py
        if ($LASTEXITCODE -ne 0) { throw 'Isolated save/Reset host verification failed' }
    }
    Write-Host 'RESET: bounded accounting and native scenarios checked; created-player paths and original presentation acceptance remain separate.'
} finally { Pop-Location }
