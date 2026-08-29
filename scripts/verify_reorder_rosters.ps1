param([switch]$SkipBuild, [switch]$RequireDatabase, [switch]$RequireReferences)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Debug -AllTargets
    }
    $exe = Join-Path $repo 'build-windows\Debug\nba97_reorder_tests.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw 'Build nba97_reorder_tests first.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_frontend_title_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Recovered title arithmetic tests failed (live rendering integration is separate).' }
    & (Join-Path $repo 'build-windows\Debug\nba97_player_photo_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'View Player photo lifecycle tests failed.' }
    python -m unittest discover -s tools -p test_reorder_progress.py
    if ($LASTEXITCODE -ne 0) { throw 'Re-order ledger tests failed.' }
    python -m unittest discover -s tools -p test_reorder_reference.py
    if ($LASTEXITCODE -ne 0) { throw 'Synthetic reference-comparator tests failed.' }
    python -m unittest discover -s tools -p test_inspect_native_frames.py
    if ($LASTEXITCODE -ne 0) { throw 'Native recording-inspector tests failed.' }
    python -m unittest discover -s tools -p test_inspect_title_frame_steps.py
    if ($LASTEXITCODE -ne 0) { throw 'Original frame-step observation inspector tests failed.' }
    python -m unittest discover -s tools -p test_inspect_help_events.py
    if ($LASTEXITCODE -ne 0) { throw 'Native Help event-inspector tests failed.' }
    python -m unittest discover -s tools -p test_inspect_process_audio.py
    if ($LASTEXITCODE -ne 0) { throw 'Process-audio inspector tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_process_audio_capture_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Process-audio capture guard tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_music_pcm_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Independent music PCM tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_native_frame_capture_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native recording writer regression tests failed.' }
    python tools/verify_reorder_reference.py --check-config
    if ($LASTEXITCODE -ne 0) { throw 'Reference scenario contract invalid.' }
    python -m unittest discover -s tools -p test_frontend_help.py
    if ($LASTEXITCODE -ne 0) { throw 'Help extraction tests failed.' }
    python -m unittest discover -s tools -p test_compare_assets.py
    if ($LASTEXITCODE -ne 0) { throw 'Compare extraction tests failed.' }
    python -m unittest discover -s tools -p test_team_backgrounds.py
    if ($LASTEXITCODE -ne 0) { throw 'Indexed background extraction tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_frontend_palette_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Original palette arithmetic tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_frontend_palette_assets_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Indexed palette renderer tests failed.' }
    python -m unittest discover -s tools -p test_reorder_portraits.py
    if ($LASTEXITCODE -ne 0) { throw 'Small portrait CLUT transparency tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_compare_assets_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Compare asset tests failed.' }
    $helpExe = Join-Path $repo 'build-windows\Debug\nba97_frontend_help_tests.exe'
    & $helpExe
    if ($LASTEXITCODE -ne 0) { throw 'Help modal regression tests failed.' }
    $childExe = Join-Path $repo 'build-windows\Debug\nba97_reorder_child_tests.exe'
    & $childExe
    if ($LASTEXITCODE -ne 0) { throw 'Re-order child regression tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_roster_compare_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Compare controller regression tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_roster_save_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Roster save codec regression tests failed.' }
    $saveDbExe = Join-Path $repo 'build-windows\Debug\nba97_roster_database_save_tests.exe'
    & $saveDbExe
    if ($LASTEXITCODE -ne 0) { throw 'Roster save database regression tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_roster_save_store_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Roster disk transaction regression tests failed.' }
    python -m unittest discover -s tools -p test_roster_reset.py
    if ($LASTEXITCODE -ne 0) { throw 'Reset extraction tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_roster_reset_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Reset core tests failed.' }
    python -m unittest discover -s tools -p test_player_notice.py
    if ($LASTEXITCODE -ne 0) { throw 'Player notice extraction tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_player_notice_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Player notice core tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_cool_fact_selection_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Cool Fact selection/flash tests failed.' }
    & (Join-Path $repo 'build-windows\Debug\nba97_recovered_audio_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Recovered SFX gain tests failed.' }
    $arguments = @('tools/verify_reorder_rosters.py', '--check', '--native-test', $exe)
    $database = Join-Path $repo '.local\assetpacks\database\roster.n97db'
    if (Test-Path -LiteralPath $database) {
        if (-not (Test-Path -LiteralPath '.local/assetpacks/reorder/dialogs.n97ui') -or
            -not (Test-Path -LiteralPath '.local/assetpacks/reorder/discard.n97ui')) {
            python tools/extract_reorder_dialogs.py .local/extracted/FEONLY.BIN
            if ($LASTEXITCODE -ne 0) { throw 'Private Re-order dialog extraction failed.' }
        }
        $arguments += @('--database', $database)
        python tools/extract_frontend_help.py .local/extracted/FEONLY.BIN
        if ($LASTEXITCODE -ne 0) { throw 'Private Help extraction failed.' }
        python tools/extract_compare_assets.py .local/extracted/FEONLY.BIN
        if ($LASTEXITCODE -ne 0) { throw 'Private Compare extraction failed.' }
        & (Join-Path $repo 'build-windows\Debug\nba97_compare_assets_tests.exe') '.local/assetpacks/reorder/compare.n97ui'
        if ($LASTEXITCODE -ne 0) { throw 'Private Compare pack tests failed.' }
        & $helpExe '.local/assetpacks'
        if ($LASTEXITCODE -ne 0) { throw 'Private Help font/descriptor tests failed.' }
        & $childExe $database
        if ($LASTEXITCODE -ne 0) { throw 'Private draft/View child tests failed.' }
        & (Join-Path $repo 'build-windows\Debug\nba97_recovered_audio_tests.exe') '.local/assetpacks/menu'
        if ($LASTEXITCODE -ne 0) { throw 'Private SFX gain/pitch tests failed.' }
        & $saveDbExe $database
        if ($LASTEXITCODE -ne 0) { throw 'Private roster identity/preparation tests failed.' }
        & (Join-Path $repo 'build-windows\Debug\nba97_roster_save_store_tests.exe') $database
        if ($LASTEXITCODE -ne 0) { throw 'Private roster disk save/restart/reset tests failed.' }
        python tools/extract_roster_reset.py .local/extracted/FEONLY.BIN
        if ($LASTEXITCODE -ne 0) { throw 'Private Reset extraction failed.' }
        python tools/extract_player_notice.py .local/extracted/FEONLY.BIN
        if ($LASTEXITCODE -ne 0) { throw 'Private Player notice extraction failed.' }
        & (Join-Path $repo 'build-windows\Debug\nba97_player_notice_tests.exe') '.local/assetpacks'
        if ($LASTEXITCODE -ne 0) { throw 'Private Player notice font/descriptor tests failed.' }
        & (Join-Path $repo 'build-windows\Debug\nba97_roster_reset_tests.exe') '.local/assetpacks'
        if ($LASTEXITCODE -ne 0) { throw 'Private Reset font/descriptor tests failed.' }
        python tools/verify_reorder_save_host.py
        if ($LASTEXITCODE -ne 0) { throw 'Isolated host save/notice/restart regressions failed.' }
    }
    elseif ($RequireDatabase) { throw "Missing local roster database: $database" }
    else { Write-Host 'Local database absent: only asset-free scenarios will run.' }
    New-Item -ItemType Directory -Force -Path '.local/logs' | Out-Null
    & python @arguments | Tee-Object -FilePath '.local/logs/reorder_rosters_verification.log'
    if ($LASTEXITCODE -ne 0) { throw 'Re-order verification failed.' }
    if ($RequireReferences) {
        python tools/verify_reorder_reference.py
        if ($LASTEXITCODE -ne 0) {
            throw 'Original-reference gate incomplete or different; inspect .local/reports/reorder_reference_run.json.'
        }
    } else {
        Write-Host 'Original-reference media not checked. Use -RequireReferences for that separate gate.'
    }
} finally { Pop-Location }
