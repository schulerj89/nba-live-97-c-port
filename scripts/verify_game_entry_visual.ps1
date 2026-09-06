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
        & $cmake --build "$repo/build-windows" --config $Configuration --target nba97_game_static_initializers_tests nba97_game_global_pointer_save_tests nba97_game_heap_initialize_tests nba97_game_cd_directory_initialize_tests nba97_game_path_prefix_set_tests nba97_game_directory_cache_configure_tests nba97_game_interrupt_mask_set_tests nba97_game_reset_callback_tests nba97_game_controller_resume_tests nba97_game_reset_graph_tests nba97_game_graph_debug_set_tests nba97_game_vblank_initialize_tests nba97_game_clock_initialize_tests nba97_game_gte_initialize_tests nba97_game_clock_delta_tests nba97_game_presentation_wait_tests nba97_game_video_environment_initialize_tests nba97_game_move_image_tests nba97_game_gpu_sync_tests nba97_game_display_mask_set_tests nba97_game_resource_validator_install_tests nba97_game_frame_rate_reset_tests nba97_game_match_session_tests nba97_game_loading_screen_tests nba97_game_resource_loader_tests nba97_game_heap_payload_size_tests nba97_game_cd_sync_tests nba97_game_cd_ready_callback_tests nba97_game_cd_sync_callback_tests nba97_game_vblank_shutdown_tests nba97_game_clock_shutdown_tests nba97_game_controller_suspend_tests nba97_game_memory_zero_tests nba97_game_memory_copy_tests nba97_game_main_tests --parallel
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
    $matchSessionUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_match_session_tests.exe"
    & $matchSessionUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8002D8D4 match-session tests failed'}
    $loadingScreenUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_loading_screen_tests.exe"
    & $loadingScreenUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80029E58 loading-screen tests failed'}
    $resourceLoaderUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_resource_loader_tests.exe"
    & $resourceLoaderUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80029BFC resource-loader tests failed'}
    $heapPayloadSizeUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_heap_payload_size_tests.exe"
    & $heapPayloadSizeUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80090D60 heap payload-size tests failed'}
    $cdSyncUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_cd_sync_tests.exe"
    & $cdSyncUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8009DBA0 CdSync wrapper tests failed'}
    $cdReadyCallbackUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_cd_ready_callback_tests.exe"
    & $cdReadyCallbackUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8009DBE0 CdReadyCallback tests failed'}
    $cdSyncCallbackUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_cd_sync_callback_tests.exe"
    & $cdSyncCallbackUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8009DBF8 CdSyncCallback tests failed'}
    $vblankShutdownUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_vblank_shutdown_tests.exe"
    & $vblankShutdownUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A44D4 VBlank-shutdown tests failed'}
    $clockShutdownUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_clock_shutdown_tests.exe"
    & $clockShutdownUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8009167C game-clock-shutdown tests failed'}
    $controllerSuspendUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_controller_suspend_tests.exe"
    & $controllerSuspendUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x8008F19C controller-suspend tests failed'}
    $memoryZeroUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_memory_zero_tests.exe"
    & $memoryZeroUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800A3A74 zero-fill tests failed'}
    $memoryCopyUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_memory_copy_tests.exe"
    & $memoryCopyUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x800AA468 memory-copy tests failed'}
    $mainUnit=Join-Path $repo "build-windows/$Configuration/nba97_game_main_tests.exe"
    foreach($target in @('nba97_feload_entry_tests','nba97_feload_entry_integration_tests')) {
        if(-not $SkipBuild) {
            & $cmake --build "$repo/build-windows" --config $Configuration --target $target --parallel
            if($LASTEXITCODE) {throw "FELOAD target build failed: $target"}
        }
        & (Join-Path $repo "build-windows/$Configuration/$target.exe")
        if($LASTEXITCODE) {throw "FELOAD startup verification failed: $target"}
    }
    & $mainUnit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80029994 unit/composition tests failed'}
    foreach($target in @('nba97_game_match_initialize_tests','nba97_game_match_initialize_integration_tests',
        'nba97_game_roster_bindings_tests','nba97_game_roster_bindings_integration_tests',
        'nba97_game_scene_load_tests','nba97_game_scene_load_integration_tests',
        'nba97_game_audio_initialize_tests','nba97_game_audio_initialize_integration_tests',
        'nba97_game_scene_random_warmup_tests','nba97_game_scene_random_warmup_integration_tests',
        'nba97_game_scene_startup_tests','nba97_game_scene_startup_integration_tests',
        'nba97_game_scene_resources_tests','nba97_game_scene_resources_integration_tests',
        'nba97_game_match_hot_start_tests','nba97_game_match_hot_start_integration_tests',
        'nba97_game_first_period_startup_tests','nba97_game_first_period_startup_integration_tests',
        'nba97_game_speech_startup_tests','nba97_game_speech_startup_integration_tests',
        'nba97_game_camera_select_tests','nba97_game_camera_select_integration_tests',
        'nba97_game_late_period_limits_tests','nba97_game_late_period_limits_integration_tests',
        'nba97_game_tipoff_announcement_tests','nba97_game_tipoff_announcement_integration_tests',
        'nba97_game_controller_frame_reset_tests','nba97_game_controller_frame_reset_integration_tests',
        'nba97_game_audio_stream_pump_tests','nba97_game_audio_stream_pump_integration_tests',
        'nba97_game_match_clocks_tests','nba97_game_match_clocks_integration_tests',
        'nba97_game_audio_stream_status_tests','nba97_game_audio_stream_status_integration_tests',
        'nba97_game_clock_violations_tests','nba97_game_clock_violations_integration_tests',
        'nba97_game_audio_stream_service_tests','nba97_game_audio_stream_service_integration_tests',
        'nba97_game_period_expiry_tests','nba97_game_period_expiry_integration_tests',
        'nba97_game_match_service_publish_tests','nba97_game_match_service_publish_integration_tests',
        'nba97_game_clock_read_tests','nba97_game_clock_read_integration_tests',
        'nba97_game_match_audio_service_tests','nba97_game_match_audio_service_integration_tests',
        'nba97_game_stream_readiness_tests','nba97_game_stream_readiness_integration_tests',
        'nba97_game_rule_delay_tests','nba97_game_rule_delay_integration_tests',
        'nba97_game_actor_resume_tests','nba97_game_actor_resume_integration_tests',
        'nba97_game_stream_queue_count_tests','nba97_game_stream_queue_count_integration_tests',
        'nba97_game_ball_actor_contact_tests','nba97_game_ball_actor_contact_integration_tests',
        'nba97_game_ball_contact_gate_tests','nba97_game_ball_contact_gate_integration_tests',
        'nba97_game_contact_dispatch_tests','nba97_game_contact_dispatch_integration_tests',
        'nba97_game_actor_contact_gate_tests','nba97_game_actor_contact_gate_integration_tests',
        'nba97_game_ball_acquire_tests','nba97_game_ball_acquire_integration_tests',
        'nba97_game_actor_input_tests','nba97_game_actor_input_integration_tests',
        'nba97_game_actor_contact_eligibility_tests','nba97_game_actor_contact_eligibility_integration_tests',
        'nba97_game_camera_override_end_tests','nba97_game_camera_override_end_integration_tests',
        'nba97_game_opponent_contact_tests','nba97_game_opponent_contact_integration_tests',
        'nba97_game_camera_frame_transform_tests','nba97_game_camera_frame_transform_integration_tests',
        'nba97_game_frame_interrupt_disable_tests','nba97_game_frame_interrupt_disable_integration_tests',
        'nba97_game_frame_interrupt_restore_tests','nba97_game_frame_interrupt_restore_integration_tests',
        'nba97_game_actor_collision_response_tests','nba97_game_actor_collision_response_integration_tests',
        'nba97_game_clear_ordering_table_tests','nba97_game_clear_ordering_table_integration_tests',
        'nba97_game_camera_overlay_packets_tests','nba97_game_camera_overlay_packets_integration_tests',
        'nba97_game_rotation_matrix_tests','nba97_game_rotation_matrix_integration_tests',
        'nba97_game_ordering_table_dma_tests','nba97_game_ordering_table_dma_integration_tests',
        'nba97_game_gte_rotation_install_tests','nba97_game_gte_rotation_install_integration_tests',
        'nba97_game_gte_translation_install_tests','nba97_game_gte_translation_install_integration_tests',
        'nba97_game_gte_reference_transform_tests','nba97_game_gte_reference_transform_integration_tests',
        'nba97_game_bios_memory_copy_tests','nba97_game_bios_memory_copy_integration_tests',
        'nba97_game_display_environment_tests','nba97_game_display_environment_integration_tests',
        'nba97_game_video_mode_tests','nba97_game_video_mode_integration_tests',
        'nba97_game_gpu_control_command_tests','nba97_game_gpu_control_command_integration_tests',
        'nba97_game_draw_environment_tests','nba97_game_draw_environment_integration_tests',
        'nba97_game_draw_packet_tests','nba97_game_draw_packet_integration_tests',
        'nba97_game_draw_area_start_tests','nba97_game_draw_area_start_integration_tests',
        'nba97_game_draw_area_end_tests','nba97_game_draw_area_end_integration_tests',
        'nba97_game_graphics_submit_tests','nba97_game_graphics_submit_integration_tests',
        'nba97_game_gpu_packet_dma_tests','nba97_game_gpu_packet_dma_integration_tests',
        'nba97_game_draw_offset_command_tests','nba97_game_draw_offset_command_integration_tests',
        'nba97_game_draw_mode_command_tests','nba97_game_draw_mode_command_integration_tests',
        'nba97_game_period_startup_tests','nba97_game_period_startup_integration_tests',
        'nba97_game_random_seed_tests','nba97_game_random_seed_integration_tests',
        'nba97_game_camera_startup_tests','nba97_game_camera_startup_integration_tests',
        'nba97_game_speech_initialize_tests','nba97_game_speech_initialize_integration_tests',
        'nba97_game_loop_entry_tests','nba97_game_loop_entry_integration_tests')) {
        if(-not $SkipBuild) {
            & $cmake --build "$repo/build-windows" --config $Configuration --target $target --parallel
            if($LASTEXITCODE) {throw "Match initializer target build failed: $target"}
        }
        & (Join-Path $repo "build-windows/$Configuration/$target.exe")
        if($LASTEXITCODE) {throw "Match initializer verification failed: $target"}
    }

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
