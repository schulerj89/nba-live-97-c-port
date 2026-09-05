param(
    [switch]$SkipBuild,
    [ValidateSet('Debug','RelWithDebInfo')][string]$Configuration='Debug'
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cmake='C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
Push-Location $repo
try {
    if(-not $SkipBuild) {
        & "$PSScriptRoot/build.ps1" -Configuration $Configuration
        if($LASTEXITCODE) {throw 'Native application build failed'}
        & $cmake --build "$repo/build-windows" --config $Configuration --target nba97_game_static_initializers_tests nba97_game_global_pointer_save_tests nba97_game_heap_initialize_tests nba97_game_cd_directory_initialize_tests nba97_game_path_prefix_set_tests nba97_game_directory_cache_configure_tests nba97_game_interrupt_mask_set_tests nba97_game_reset_callback_tests nba97_game_controller_resume_tests nba97_game_reset_graph_tests nba97_game_graph_debug_set_tests nba97_game_vblank_initialize_tests nba97_game_clock_initialize_tests nba97_game_gte_initialize_tests nba97_game_clock_delta_tests nba97_game_presentation_wait_tests nba97_game_video_environment_initialize_tests nba97_game_move_image_tests nba97_game_gpu_sync_tests nba97_game_display_mask_set_tests nba97_game_resource_validator_install_tests nba97_game_frame_rate_reset_tests nba97_game_main_tests --parallel
        if($LASTEXITCODE) {throw 'GAMEONLY game-entry owner tests failed'}
    }
    $staticUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_static_initializers_tests.exe"
    & $staticUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800948D0 unit tests failed'}
    $globalPointerUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_global_pointer_save_tests.exe"
    & $globalPointerUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A4830 unit tests failed'}
    $heapUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_heap_initialize_tests.exe"
    & $heapUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8008FA6C heap tests failed'}
    $cdDirectoryUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_cd_directory_initialize_tests.exe"
    & $cdDirectoryUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80091C08 CD-directory tests failed'}
    $pathPrefixUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_path_prefix_set_tests.exe"
    & $pathPrefixUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A35D8 path-prefix tests failed'}
    $directoryCacheUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_directory_cache_configure_tests.exe"
    & $directoryCacheUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80092C7C directory-cache tests failed'}
    $interruptMaskUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_interrupt_mask_set_tests.exe"
    & $interruptMaskUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800985B4 interrupt-mask tests failed'}
    $resetCallbackUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_reset_callback_tests.exe"
    & $resetCallbackUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800985DC ResetCallback wrapper tests failed'}
    $controllerResumeUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_controller_resume_tests.exe"
    & $controllerResumeUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8008F1D4 controller-resume tests failed'}
    $resetGraphUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_reset_graph_tests.exe"
    & $resetGraphUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80099058 ResetGraph tests failed'}
    $graphDebugUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_graph_debug_set_tests.exe"
    & $graphDebugUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800992C4 SetGraphDebug tests failed'}
    $vblankUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_vblank_initialize_tests.exe"
    & $vblankUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A43E8 VBlank-initialize tests failed'}
    $clockUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_clock_initialize_tests.exe"
    & $clockUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800914D8 game-clock-initialize tests failed'}
    $gteUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_gte_initialize_tests.exe"
    & $gteUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80056678 GTE-initialize tests failed'}
    $clockDeltaUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_clock_delta_tests.exe"
    & $clockDeltaUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A584C clock-delta tests failed'}
    $presentationWaitUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_presentation_wait_tests.exe"
    & $presentationWaitUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80029BDC presentation-wait tests failed'}
    $videoEnvironmentUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_video_environment_initialize_tests.exe"
    & $videoEnvironmentUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80029F20 video-environment tests failed'}
    $moveImageUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_move_image_tests.exe"
    & $moveImageUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800997E4 MoveImage tests failed'}
    $gpuSyncUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_gpu_sync_tests.exe"
    & $gpuSyncUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800994F4 DrawSync tests failed'}
    $displayMaskUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_display_mask_set_tests.exe"
    & $displayMaskUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80099458 SetDispMask tests failed'}
    $resourceValidatorUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_resource_validator_install_tests.exe"
    & $resourceValidatorUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A3E20 resource-validator install tests failed'}
    $frameRateResetUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_frame_rate_reset_tests.exe"
    & $frameRateResetUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A7738 frame-rate reset tests failed'}
    $mainUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_main_tests.exe"
    & $mainUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80029994 unit/composition tests failed'}

    $stamp=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    # The existing capture owner enforces this private root and requires every
    # mutable store to live beside the fresh frame directory.
    $run=Join-Path $repo ".local/verification/team_select/game-entry-$stamp"
    New-Item -ItemType Directory -Path $run | Out-Null
    $frames=Join-Path $run 'frames'
    $trace=Join-Path $run 'trace.log'
    $exe=Join-Path $repo "build-windows/$Configuration/nba97_boot_decomp.exe"
    $captureArgs=@('--asset-root',"$repo/.local/assetpacks",'--capture-team-select',$frames,
        '--settings',"$run/settings.ini",'--profiles',"$run/profiles.n97sav",
        '--created-players',"$run/created.n97cpl",'--roster-save',"$run/rosters.n97rst",'--trace',$trace)
    & $exe @captureArgs *> "$run/stdout.log"
    if($LASTEXITCODE) {Get-Content "$run/stdout.log" -Tail 20;throw 'Frontend game-entry capture failed'}
    python tools/verify_game_entry_visual.py --frames $frames --trace $trace
    if($LASTEXITCODE) {throw 'Frontend game-entry visual receipt failed'}
    Write-Host "Evidence: $run"
} finally {
    Pop-Location
}
