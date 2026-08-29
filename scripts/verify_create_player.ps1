param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$assetRoot = Join-Path $repo '.local\assetpacks'
$exe = Join-Path $repo 'build-windows\Debug\nba97_boot_decomp.exe'
$coreTest = Join-Path $repo 'build-windows\Debug\nba97_create_player_tests.exe'
$storeTest = Join-Path $repo 'build-windows\Debug\nba97_create_player_store_tests.exe'
$modelTest = Join-Path $repo 'build-windows\Debug\nba97_zdomf_model_tests.exe'
$transformTest = Join-Path $repo 'build-windows\Debug\nba97_zdomf_transform_tests.exe'
$gteComposeTest = Join-Path $repo 'build-windows\Debug\nba97_zdomf_gte_compose_tests.exe'
$projectionTest = Join-Path $repo 'build-windows\Debug\nba97_zdomf_projection_tests.exe'
$hierarchyTest = Join-Path $repo 'build-windows\Debug\nba97_zdomf_hierarchy_tests.exe'
$mocapTest = Join-Path $repo 'build-windows\Debug\nba97_zdomf_mocap_tests.exe'
$vramTextureTest = Join-Path $repo 'build-windows\Debug\nba97_ps1_vram_texture_tests.exe'
$hierarchySmoke = Join-Path $repo 'build-windows\Debug\nba97_zdomf_hierarchy_smoke.exe'

Push-Location $repo
try {
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
    }
    if (-not (Test-Path -LiteralPath $exe)) { throw "Missing native executable: $exe" }
    if (-not (Test-Path -LiteralPath $coreTest)) { throw "Missing Create Player core test: $coreTest" }
    if (-not (Test-Path -LiteralPath $storeTest)) { throw "Missing Create Player store test: $storeTest" }
    if (-not (Test-Path -LiteralPath $modelTest)) { throw "Missing ZDOMF model decoder test: $modelTest" }
    if (-not (Test-Path -LiteralPath $transformTest)) { throw "Missing ZDOMF transform test: $transformTest" }
    if (-not (Test-Path -LiteralPath $gteComposeTest)) { throw "Missing ZDOMF GTE composition test: $gteComposeTest" }
    if (-not (Test-Path -LiteralPath $projectionTest)) { throw "Missing ZDOMF projection test: $projectionTest" }
    if (-not (Test-Path -LiteralPath $hierarchyTest)) { throw "Missing ZDOMF hierarchy test: $hierarchyTest" }
    if (-not (Test-Path -LiteralPath $mocapTest)) { throw "Missing ZDOMF mocap test: $mocapTest" }
    if (-not (Test-Path -LiteralPath $vramTextureTest)) { throw "Missing PS1 VRAM texture test: $vramTextureTest" }
    if (-not (Test-Path -LiteralPath $hierarchySmoke)) { throw "Missing ZDOMF hierarchy smoke test: $hierarchySmoke" }
    if (-not (Test-Path -LiteralPath (Join-Path $assetRoot 'menu\ZSET5-decoded'))) {
        throw 'Missing private ZSET5 assets. Run scripts/extract_assetpacks.ps1 locally first.'
    }

    & $coreTest
    if ($LASTEXITCODE -ne 0) { throw 'Create Player behavioral checks failed.' }
    Write-Host 'CREATE PLAYER CORE: PASS - transaction, validation, navigation, boundaries, adjustment, save, and cancel checks.'
    & $storeTest
    if ($LASTEXITCODE -ne 0) { throw 'Create Player persistence checks failed.' }
    Write-Host 'CREATE PLAYER STORE: PASS - create/reload/edit decode/delete, generation, no-op, CRC, backup recovery, and atomic replacement.'
    & $modelTest
    if ($LASTEXITCODE -ne 0) { throw 'ZDOMF model decoder checks failed.' }
    Write-Host 'CREATE PLAYER MODEL: PASS - FUN_800687BC-derived layout, signed vertices, FT3 metadata, and per-corner part ownership.'
    & $transformTest
    if ($LASTEXITCODE -ne 0) { throw 'ZDOMF fixed-point transform checks failed.' }
    Write-Host 'CREATE PLAYER TRANSFORM: PASS - FUN_80067100 matrix construction and FUN_80067378 fixed-point application.'
    & $gteComposeTest
    if ($LASTEXITCODE -ne 0) { throw 'ZDOMF GTE composition checks failed.' }
    Write-Host 'CREATE PLAYER GTE COMPOSE: PASS - live FUN_80066FF4 column path plus FUN_80066090 row and attachment-translation paths.'
    & $projectionTest
    if ($LASTEXITCODE -ne 0) { throw 'ZDOMF GTE projection checks failed.' }
    Write-Host 'CREATE PLAYER PROJECTION: PASS - recovered camera state and FUN_8006734C RTPS fixed-point boundary.'
    & $hierarchyTest
    if ($LASTEXITCODE -ne 0) { throw 'ZDOMF hierarchy checks failed.' }
    Write-Host 'CREATE PLAYER HIERARCHY: PASS - FUN_80069098 parent graph and fixed-point world composition.'
    & $mocapTest
    if ($LASTEXITCODE -ne 0) { throw 'ZDOMF mocap/runtime checks failed.' }
    Write-Host 'CREATE PLAYER MOCAP: PASS - FUN_80035260 paired directories, FUN_80065D40 blending, and FUN_80062C00 scale.'
    & $vramTextureTest
    if ($LASTEXITCODE -ne 0) { throw 'PS1 VRAM texture addressing checks failed.' }
    Write-Host 'CREATE PLAYER VRAM: PASS - 4/8-bpp TPAGE word addressing, FUN_80067F74 dthr/dthl, five FUN_80067A14 team uploads, and shared SHOE upload.'

    $hierarchyEvidence = Join-Path $repo '.local\verification\create_player\hierarchy'
    New-Item -ItemType Directory -Force -Path $hierarchyEvidence | Out-Null
    $hierarchyFrame = Join-Path $hierarchyEvidence 'smoke.ppm'
    & $hierarchySmoke (Join-Path $assetRoot 'create_player\model') `
        (Join-Path $assetRoot 'menu\ZFEMOCAP.BIN') $hierarchyFrame
    if ($LASTEXITCODE -ne 0) { throw 'ZDOMF hierarchy rendered smoke check failed.' }
    Write-Host "CREATE PLAYER RUNTIME SMOKE: PASS - exact RTPS points plus filled auto-fit 3D model: $hierarchyFrame"

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
        $textureAudit = $output -join "`n"
        if ($textureAudit -notmatch '\[CREATE-TEXTURE-TOTAL\] opaque=3209 transparent=3 missing=0' -or
            $textureAudit -notmatch '\[CREATE-TEXTURE-CLUT\] clut=0x7d60 opaque=1674 transparent=0 missing=0' -or
            $textureAudit -notmatch '\[CREATE-TEXTURE-CLUT\] clut=0x7da2 opaque=243 transparent=0 missing=0' -or
            $textureAudit -notmatch '\[CREATE-TEXTURE-UPLOAD\] upload=shared-dthr-r1 index=0 opaque=888 transparent=0 missing=0' -or
            $textureAudit -notmatch '\[CREATE-TEXTURE-UPLOAD\] upload=shared-dthl-r4 index=1 opaque=566 transparent=0 missing=0' -or
            $textureAudit -notmatch '\[CREATE-TEXTURE-UPLOAD\] upload=shoe-r6 index=7 opaque=243 transparent=0 missing=0' -or
            $textureAudit -notmatch '\[CREATE-TEXTURE-UPLOAD\] upload=jersey-number .*opaque=2 transparent=3 missing=0' -or
            $textureAudit -notmatch '\[CREATE-TEXTURE-SAMPLE-TRACE\] face=1 .*word=847/418 .*palette-index=234 palette-value=0xd71c .*final-rgb=227/196/173') {
            throw "Create Player ordered-stream/VRAM texture audit failed: $capture"
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
    Write-Host 'CREATE PLAYER MODEL: PASS - retail NCLIP selection and merged 251-primary/81-part FT3 streams reproduce the live ordering-table path.'
    Write-Host 'CREATE PLAYER TEXTURE: PASS - packet-selected shared/team/head/number/name/shoe VRAM uploads have zero missing samples; exact face-1 palette trace matches live RAM.'
    Write-Host "Evidence: $root"
    Write-Host 'Scope: manager/editor behavior, persistence, Delete contexts, deterministic ZDOM frames, exact field-selected frontend cameras, mocap, exact 20-record parent/pivot/matrix/endpoint runtime, synchronized 753/753 packet SXY, descriptor-0 AVSZ3 ordering, NCLIP, the merged primary/per-part FT3 streams, paired team geometry/UVs, shared dthr/dthl body surfaces, five indexed team uploads, dynamic hair/facial head composition, number/name pages, packet-selected CLUT rows, and SHOE 4-bpp. Roster insertion, pixel-exact PS1 triangle coverage/interpolation, and whole-screen original visual scoring remain pending.'
} finally {
    Pop-Location
}
