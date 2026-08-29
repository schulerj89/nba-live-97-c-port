param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$assetRoot = Join-Path $repo '.local\assetpacks'
$exe = Join-Path $repo 'build-windows\Debug\nba97_boot_decomp.exe'
$coreTest = Join-Path $repo 'build-windows\Debug\nba97_create_player_tests.exe'
$storeTest = Join-Path $repo 'build-windows\Debug\nba97_create_player_store_tests.exe'

Push-Location $repo
try {
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
    }
    if (-not (Test-Path -LiteralPath $exe)) { throw "Missing native executable: $exe" }
    if (-not (Test-Path -LiteralPath $coreTest)) { throw "Missing Create Player core test: $coreTest" }
    if (-not (Test-Path -LiteralPath $storeTest)) { throw "Missing Create Player store test: $storeTest" }
    if (-not (Test-Path -LiteralPath (Join-Path $assetRoot 'menu\ZSET5-decoded'))) {
        throw 'Missing private ZSET5 assets. Run scripts/extract_assetpacks.ps1 locally first.'
    }

    & $coreTest
    if ($LASTEXITCODE -ne 0) { throw 'Create Player behavioral checks failed.' }
    Write-Host 'CREATE PLAYER CORE: PASS - transaction, validation, navigation, boundaries, adjustment, save, and cancel checks.'
    & $storeTest
    if ($LASTEXITCODE -ne 0) { throw 'Create Player persistence checks failed.' }
    Write-Host 'CREATE PLAYER STORE: PASS - create/reload/edit decode/delete, generation, no-op, CRC, backup recovery, and atomic replacement.'

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $root = Join-Path $repo ".local\verification\create_player\run-$stamp"
    $first = Join-Path $root 'first'
    $second = Join-Path $root 'second'
    New-Item -ItemType Directory -Force -Path $first,$second | Out-Null

    foreach ($run in @(@($first, 'first.log'), @($second, 'second.log'))) {
        $capture = $run[0]
        $trace = Join-Path $root $run[1]
        $output = & $exe --asset-root $assetRoot --capture-create-player $capture --trace $trace 2>&1
        $output | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) { throw "Create Player capture failed: $capture" }
        if (-not (($output -join "`n") -match 'CREATE-CAPTURE\s+PASS:')) {
            throw "Create Player capture did not report PASS: $capture"
        }
    }

    $expected = @(
        'empty-new-selected.ppm',
        'empty-title-phase.ppm',
        'editor-first-required.ppm',
        'editor-selector-gold.ppm',
        'editor-appearance-layer.ppm',
        'editor-model-motion-phase.ppm',
        'editor-layer-scroll-enter.ppm',
        'editor-layer-scroll-mid.ppm',
        'editor-layer-scroll-settled.ppm',
        'editor-ratings-final.ppm',
        'one-edit-selected.ppm',
        'one-delete-selected.ppm',
        'delete-free-agent.ppm',
        'delete-team-bench.ppm',
        'delete-team-starter.ppm',
        'full-new-disabled.ppm'
    )
    foreach ($name in $expected) {
        $a = Join-Path $first $name
        $b = Join-Path $second $name
        if (-not (Test-Path -LiteralPath $a) -or -not (Test-Path -LiteralPath $b)) {
            throw "Missing expected frame: $name"
        }
        $headerBytes = [IO.File]::ReadAllBytes($a)[0..31]
        $header = [Text.Encoding]::ASCII.GetString($headerBytes)
        if ($header -notmatch '^P6\s+512\s+240\s+255\s') {
            throw "Unexpected PPM geometry/header: $name"
        }
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $a).Hash -ne
            (Get-FileHash -Algorithm SHA256 -LiteralPath $b).Hash) {
            throw "Non-deterministic Create Player frame: $name"
        }
    }

    python (Join-Path $repo 'tools\verify_create_player_frames.py') $first
    if ($LASTEXITCODE -ne 0) { throw 'Create Player localized frame checks failed.' }
    Write-Host "CREATE PLAYER: PASS - 16/16 scenarios reproduced byte-identically across two runs; selector pulse, model motion, and three scroll phases are pixel-distinct."
    Write-Host "Evidence: $root"
    Write-Host 'Scope: manager/editor behavior, versioned persistence, three original Delete contexts, recovered name/college/scroll behavior, deterministic ZDOM/mocap preview frames. Textured PS1 polygon equivalence, roster insertion, and original visual scoring remain pending.'
} finally {
    Pop-Location
}
