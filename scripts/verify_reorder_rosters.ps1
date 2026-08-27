param([switch]$SkipBuild, [switch]$RequireDatabase)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') }
    $exe = Join-Path $repo 'build-windows\Debug\nba97_reorder_tests.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw 'Build nba97_reorder_tests first.' }
    python -m unittest discover -s tools -p test_reorder_progress.py
    if ($LASTEXITCODE -ne 0) { throw 'Re-order ledger tests failed.' }
    $arguments = @('tools/verify_reorder_rosters.py', '--check', '--native-test', $exe)
    $database = Join-Path $repo '.local\assetpacks\database\roster.n97db'
    if (Test-Path -LiteralPath $database) {
        if (-not (Test-Path -LiteralPath '.local/assetpacks/reorder/dialogs.n97ui') -or
            -not (Test-Path -LiteralPath '.local/assetpacks/reorder/discard.n97ui')) {
            python tools/extract_reorder_dialogs.py .local/extracted/FEONLY.BIN
            if ($LASTEXITCODE -ne 0) { throw 'Private Re-order dialog extraction failed.' }
        }
        $arguments += @('--database', $database)
    }
    elseif ($RequireDatabase) { throw "Missing local roster database: $database" }
    else { Write-Host 'Local database absent: only asset-free scenarios will run.' }
    New-Item -ItemType Directory -Force -Path '.local/logs' | Out-Null
    & python @arguments | Tee-Object -FilePath '.local/logs/reorder_rosters_verification.log'
    if ($LASTEXITCODE -ne 0) { throw 'Re-order verification failed.' }
} finally { Pop-Location }
