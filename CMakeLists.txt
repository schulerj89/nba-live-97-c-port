cmake_minimum_required(VERSION 3.20)
project(nba_live_97_decomp LANGUAGES C CXX)

include(CTest)
option(NBA97_RECOVERED_TESTS_ONLY "Build asset-free recovered logic tests without the UI" OFF)
if(MSVC)
    # Visual Studio can compile a target's translation units concurrently.
    # Serialize writes to the shared compiler PDB so clean parallel builds do
    # not intermittently fail with C1041.
    add_compile_options(/FS)
endif()
if(BUILD_TESTING)
    add_executable(nba97_voice_channels_tests tests/voice_channels_tests.cpp src/recovered/voice_channels.c
        src/recovered/voice_handles.c src/recovered/music_voice.c src/recovered/frontend_resource_cleanup.c)
    target_compile_features(nba97_voice_channels_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_voice_channels_tests PRIVATE src)
    add_test(NAME voice_channels COMMAND nba97_voice_channels_tests)
    add_executable(nba97_game_render_backend_tests tests/game_render_backend_tests.cpp src/game_render_backend.cpp
        src/recovered/game_image_upload.c src/recovered/game_head_cache.c)
    target_compile_features(nba97_game_render_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_render_backend_tests PRIVATE src)
    add_test(NAME game_render_backend COMMAND nba97_game_render_backend_tests)
    add_executable(nba97_game_image_upload_tests tests/game_image_upload_tests.cpp src/recovered/game_image_upload.c)
    target_compile_features(nba97_game_image_upload_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_image_upload_tests PRIVATE src)
    add_test(NAME game_image_upload COMMAND nba97_game_image_upload_tests)
    add_executable(nba97_game_player_jump_tests tests/game_player_jump_tests.cpp src/recovered/game_player_jump.c)
    target_compile_features(nba97_game_player_jump_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_jump_tests PRIVATE src)
    add_test(NAME game_player_jump COMMAND nba97_game_player_jump_tests)
    add_executable(nba97_voice_handles_tests tests/voice_handles_tests.cpp src/recovered/voice_handles.c src/recovered/music_voice.c)
    target_compile_features(nba97_voice_handles_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_voice_handles_tests PRIVATE src)
    add_test(NAME voice_handles COMMAND nba97_voice_handles_tests)
    add_executable(nba97_game_render_entry_tests tests/game_render_entry_tests.cpp
        src/recovered/game_render_textures.c src/recovered/game_head_cache.c src/recovered/game_player_labels.c)
    target_compile_features(nba97_game_render_entry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_render_entry_tests PRIVATE src)
    add_test(NAME game_render_entry COMMAND nba97_game_render_entry_tests)
    add_executable(nba97_frontend_resource_cleanup_tests tests/frontend_resource_cleanup_tests.cpp src/recovered/frontend_resource_cleanup.c)
    target_compile_features(nba97_frontend_resource_cleanup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_resource_cleanup_tests PRIVATE src)
    add_test(NAME frontend_resource_cleanup COMMAND nba97_frontend_resource_cleanup_tests)
    add_executable(nba97_game_player_update_tests tests/game_player_update_tests.cpp src/recovered/game_player_update.c)
    target_compile_features(nba97_game_player_update_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_update_tests PRIVATE src)
    add_test(NAME game_player_update COMMAND nba97_game_player_update_tests)
    add_executable(nba97_game_player_physics_tests tests/game_player_physics_tests.cpp src/recovered/game_player_physics.c)
    target_compile_features(nba97_game_player_physics_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_physics_tests PRIVATE src)
    add_test(NAME game_player_physics COMMAND nba97_game_player_physics_tests)
    add_executable(nba97_frontend_resource_tests tests/frontend_resource_tests.cpp src/recovered/frontend_resource.c)
    target_compile_features(nba97_frontend_resource_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_resource_tests PRIVATE src)
    add_test(NAME frontend_resource COMMAND nba97_frontend_resource_tests)
    add_executable(nba97_match_pose_tests tests/match_pose_tests.cpp src/match_pose.cpp src/gameplay_pose.cpp
        src/gameplay_setup.cpp src/gameplay_mocap.cpp src/recovered/gameplay_mocap.c
        src/recovered/game_pose_request.c src/recovered/game_pose_sample.c)
    target_compile_features(nba97_match_pose_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_match_pose_tests PRIVATE src)
    add_test(NAME match_pose COMMAND nba97_match_pose_tests)
    add_executable(nba97_game_pose_sample_tests tests/game_pose_sample_tests.cpp src/recovered/game_pose_sample.c
        src/gameplay_pose.cpp src/gameplay_mocap.cpp src/recovered/gameplay_mocap.c)
    target_compile_features(nba97_game_pose_sample_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_pose_sample_tests PRIVATE src)
    add_test(NAME game_pose_sample COMMAND nba97_game_pose_sample_tests)
    add_executable(nba97_game_pose_request_tests tests/game_pose_request_tests.cpp src/recovered/game_pose_request.c)
    target_compile_features(nba97_game_pose_request_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_pose_request_tests PRIVATE src)
    add_test(NAME game_pose_request COMMAND nba97_game_pose_request_tests)
    add_executable(nba97_gameplay_animation_tests tests/gameplay_animation_tests.cpp src/gameplay_animation.cpp
        src/gameplay_setup.cpp src/gameplay_mocap.cpp src/recovered/gameplay_mocap.c)
    target_compile_features(nba97_gameplay_animation_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_gameplay_animation_tests PRIVATE src)
    add_test(NAME gameplay_animation COMMAND nba97_gameplay_animation_tests)
    add_executable(nba97_game_animation_advance_tests tests/game_animation_advance_tests.cpp src/recovered/game_animation_advance.c src/recovered/game_animation_queue.c)
    target_compile_features(nba97_game_animation_advance_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_animation_advance_tests PRIVATE src)
    add_test(NAME game_animation_advance COMMAND nba97_game_animation_advance_tests)
    add_executable(nba97_game_animation_queue_tests tests/game_animation_queue_tests.cpp src/recovered/game_animation_advance.c src/recovered/game_animation_queue.c)
    target_compile_features(nba97_game_animation_queue_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_animation_queue_tests PRIVATE src)
    add_test(NAME game_animation_queue COMMAND nba97_game_animation_queue_tests)
    add_executable(nba97_music_transition_tests tests/music_transition_tests.cpp src/recovered/music_transition.c)
    target_compile_features(nba97_music_transition_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_music_transition_tests PRIVATE src)
    add_test(NAME music_transition COMMAND nba97_music_transition_tests)
    add_executable(nba97_music_playback_tests tests/music_playback_tests.cpp src/music_playback.cpp src/ea_schl.cpp
        src/recovered/music_routing.c src/recovered/music_voice.c src/recovered/music_stream.c src/recovered/frontend_title.c)
    target_compile_features(nba97_music_playback_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_music_playback_tests PRIVATE src)
    add_test(NAME music_playback COMMAND nba97_music_playback_tests)
    add_executable(nba97_game_player_attributes_tests tests/game_player_attributes_tests.cpp src/recovered/game_player_attributes.c)
    target_compile_features(nba97_game_player_attributes_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_attributes_tests PRIVATE src)
    add_test(NAME game_player_attributes COMMAND nba97_game_player_attributes_tests)
    add_executable(nba97_game_substitution_support_tests tests/game_substitution_support_tests.cpp src/recovered/game_substitution_support.c)
    target_compile_features(nba97_game_substitution_support_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_substitution_support_tests PRIVATE src)
    add_test(NAME game_substitution_support COMMAND nba97_game_substitution_support_tests)
    add_executable(nba97_match_runtime_tests tests/match_runtime_tests.cpp src/match_runtime.cpp
        src/match_player_update.cpp src/recovered/game_player_update.c src/recovered/game_player_physics.c
        src/match_pose.cpp src/gameplay_pose.cpp src/recovered/game_pose_request.c src/recovered/game_pose_sample.c
        src/gameplay_animation.cpp src/recovered/game_animation_advance.c src/recovered/game_animation_queue.c
        src/gameplay_setup.cpp src/gameplay_mocap.cpp src/recovered/gameplay_mocap.c
        src/recovered/game_player_attributes.c
        src/recovered/game_period.c src/recovered/game_period_dependencies.c
        src/recovered/game_player_bindings.c src/recovered/game_team_roles.c
        src/recovered/game_lineup_recovery.c src/recovered/game_player_initialization.c
        src/recovered/game_controller_selection.c src/recovered/team_header.c src/recovered/game_controllers.c)
    target_compile_features(nba97_match_runtime_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_match_runtime_tests PRIVATE src)
    add_test(NAME match_runtime COMMAND nba97_match_runtime_tests)
    add_executable(nba97_game_period_dependencies_tests tests/game_period_dependencies_tests.cpp src/recovered/game_period_dependencies.c)
    target_compile_features(nba97_game_period_dependencies_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_dependencies_tests PRIVATE src)
    add_test(NAME game_period_dependencies COMMAND nba97_game_period_dependencies_tests)
    add_executable(nba97_music_stream_tests tests/music_stream_tests.cpp src/recovered/music_stream.c)
    target_compile_features(nba97_music_stream_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_music_stream_tests PRIVATE src)
    add_test(NAME music_stream COMMAND nba97_music_stream_tests)
    add_executable(nba97_game_period_tests tests/game_period_tests.cpp src/recovered/game_period.c)
    target_compile_features(nba97_game_period_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_tests PRIVATE src)
    add_test(NAME game_period COMMAND nba97_game_period_tests)
    add_executable(nba97_game_substitution_tests tests/game_substitution_tests.cpp src/recovered/game_substitution.c)
    target_compile_features(nba97_game_substitution_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_substitution_tests PRIVATE src)
    add_test(NAME game_substitution COMMAND nba97_game_substitution_tests)
    add_executable(nba97_game_team_roles_tests tests/game_team_roles_tests.cpp src/recovered/game_team_roles.c)
    target_compile_features(nba97_game_team_roles_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_team_roles_tests PRIVATE src)
    add_test(NAME game_team_roles COMMAND nba97_game_team_roles_tests)
    add_executable(nba97_gameplay_setup_tests tests/gameplay_setup_tests.cpp src/gameplay_setup.cpp
        src/gameplay_mocap.cpp src/recovered/gameplay_mocap.c src/recovered/game_player_initialization.c)
    target_compile_features(nba97_gameplay_setup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_gameplay_setup_tests PRIVATE src)
    add_test(NAME gameplay_setup COMMAND nba97_gameplay_setup_tests)
    add_executable(nba97_game_player_initialization_tests tests/game_player_initialization_tests.cpp
        src/recovered/game_player_initialization.c)
    target_compile_features(nba97_game_player_initialization_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_initialization_tests PRIVATE src)
    add_test(NAME game_player_initialization COMMAND nba97_game_player_initialization_tests)
    add_executable(nba97_music_voice_tests tests/music_voice_tests.cpp src/recovered/music_voice.c)
    target_compile_features(nba97_music_voice_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_music_voice_tests PRIVATE src)
    add_test(NAME music_voice COMMAND nba97_music_voice_tests)
    add_executable(nba97_game_lineup_recovery_tests tests/game_lineup_recovery_tests.cpp
        src/recovered/game_lineup_recovery.c)
    target_compile_features(nba97_game_lineup_recovery_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_lineup_recovery_tests PRIVATE src)
    add_test(NAME game_lineup_recovery COMMAND nba97_game_lineup_recovery_tests)
    add_executable(nba97_game_player_bindings_tests tests/game_player_bindings_tests.cpp
        src/recovered/game_player_bindings.c)
    target_compile_features(nba97_game_player_bindings_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_bindings_tests PRIVATE src)
    add_test(NAME game_player_bindings COMMAND nba97_game_player_bindings_tests)
    add_executable(nba97_game_controller_selection_tests tests/game_controller_selection_tests.cpp
        src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_controller_selection_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controller_selection_tests PRIVATE src)
    add_test(NAME game_controller_selection COMMAND nba97_game_controller_selection_tests)
    add_executable(nba97_ea_schl_tests tests/ea_schl_tests.cpp src/ea_schl.cpp)
    target_compile_features(nba97_ea_schl_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_ea_schl_tests PRIVATE src)
    add_test(NAME ea_schl COMMAND nba97_ea_schl_tests)
    add_executable(nba97_game_controllers_tests tests/game_controllers_tests.cpp src/recovered/game_controllers.c)
    target_compile_features(nba97_game_controllers_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controllers_tests PRIVATE src)
    add_test(NAME game_controllers COMMAND nba97_game_controllers_tests)
    add_executable(nba97_gameplay_mocap_tests tests/gameplay_mocap_tests.cpp
        src/recovered/gameplay_mocap.c src/gameplay_mocap.cpp)
    target_compile_features(nba97_gameplay_mocap_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_gameplay_mocap_tests PRIVATE src)
    add_test(NAME gameplay_mocap COMMAND nba97_gameplay_mocap_tests)
    add_executable(nba97_music_routing_tests tests/music_routing_tests.cpp
        src/recovered/music_routing.c src/recovered/frontend_title.c)
    target_compile_features(nba97_music_routing_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_music_routing_tests PRIVATE src)
    add_test(NAME music_routing COMMAND nba97_music_routing_tests)
    add_executable(nba97_team_header_tests tests/team_header_test.cpp src/recovered/team_header.c)
    target_compile_features(nba97_team_header_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_team_header_tests PRIVATE src)
    add_test(NAME team_header COMMAND nba97_team_header_tests)
    add_executable(nba97_match_strategy_tests tests/match_strategy_test.cpp src/recovered/match_strategy.c)
    target_compile_features(nba97_match_strategy_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_match_strategy_tests PRIVATE src)
    add_test(NAME match_strategy COMMAND nba97_match_strategy_tests)
    add_executable(nba97_frontend_plate_tests tests/frontend_plate_test.cpp src/frontend_plate.cpp)
    target_compile_features(nba97_frontend_plate_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_frontend_plate_tests PRIVATE src)
    add_test(NAME frontend_plate COMMAND nba97_frontend_plate_tests)
    add_executable(nba97_team_select_text_tests tests/team_select_text_test.cpp
        src/recovered/team_select_text.c src/recovered/roster_reorder.c
        src/recovered/roster_lists.c)
    target_compile_features(nba97_team_select_text_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_team_select_text_tests PRIVATE src)
    add_test(NAME team_select_text COMMAND nba97_team_select_text_tests)
    add_executable(nba97_team_select_placement_tests tests/team_select_placement_test.cpp
        src/recovered/team_select_placement.c src/recovered/team_select_poll.c)
    target_compile_features(nba97_team_select_placement_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_team_select_placement_tests PRIVATE src)
    add_test(NAME team_select_placement COMMAND nba97_team_select_placement_tests)
    add_executable(nba97_team_select_poll_tests tests/team_select_poll_test.cpp
        src/recovered/team_select_poll.c src/recovered/team_select.c)
    target_compile_features(nba97_team_select_poll_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_team_select_poll_tests PRIVATE src)
    add_test(NAME team_select_poll COMMAND nba97_team_select_poll_tests)
    add_executable(nba97_match_snapshot_tests tests/match_snapshot_test.cpp src/match_snapshot.cpp
        src/match_controls.cpp src/match_assets.cpp src/roster_ratings.cpp src/frontend_settings.cpp src/roster_database.cpp
        src/recovered/match_setup.c src/recovered/match_strategy.c src/recovered/team_header.c src/recovered/game_controllers.c src/recovered/match_controls.c src/recovered/team_ratings.c src/recovered/user_setup.c
        src/recovered/create_player.c src/recovered/roster_reorder.c src/recovered/roster_lists.c
        src/recovered/reorder_screen.c src/recovered/semantic_trace.c src/recovered/team_select.c)
    target_compile_features(nba97_match_snapshot_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_match_snapshot_tests PRIVATE src)
    target_compile_definitions(nba97_match_snapshot_tests PRIVATE NBA97_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME match_snapshot COMMAND nba97_match_snapshot_tests)
    get_target_property(nba97_match_snapshot_sources nba97_match_snapshot_tests SOURCES)
    list(REMOVE_ITEM nba97_match_snapshot_sources tests/match_snapshot_test.cpp)
    add_executable(nba97_team_header_snapshot_tests tests/team_header_snapshot_test.cpp ${nba97_match_snapshot_sources})
    target_compile_features(nba97_team_header_snapshot_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_team_header_snapshot_tests PRIVATE src)
    target_compile_definitions(nba97_team_header_snapshot_tests PRIVATE NBA97_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME team_header_snapshot COMMAND nba97_team_header_snapshot_tests)
    add_executable(nba97_match_presentation_tests tests/match_presentation_test.cpp
        src/recovered/match_setup.c src/recovered/team_select.c)
    target_compile_features(nba97_match_presentation_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_match_presentation_tests PRIVATE src)
    add_test(NAME match_presentation COMMAND nba97_match_presentation_tests)
    add_executable(nba97_match_controls_tests tests/match_controls_test.cpp
        src/match_controls.cpp src/recovered/match_controls.c)
    target_compile_features(nba97_match_controls_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_match_controls_tests PRIVATE src)
    add_test(NAME match_controls_core COMMAND nba97_match_controls_tests)
    add_executable(nba97_user_profiles_tests tests/user_profiles_test.cpp src/user_profiles.cpp)
    target_compile_features(nba97_user_profiles_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_user_profiles_tests PRIVATE src)
    target_compile_definitions(nba97_user_profiles_tests PRIVATE NBA97_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME user_profiles_store COMMAND nba97_user_profiles_tests)
    add_executable(nba97_user_setup_session_tests tests/user_setup_session_test.cpp
        src/user_setup_session.cpp src/user_profiles.cpp src/recovered/user_setup.c src/recovered/frontend_help.c
        src/recovered/roster_reset.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_user_setup_session_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_user_setup_session_tests PRIVATE src)
    add_test(NAME user_setup_session COMMAND nba97_user_setup_session_tests)
    add_executable(nba97_user_setup_visibility_tests tests/user_setup_visibility_test.cpp
        src/user_setup_session.cpp src/user_profiles.cpp src/recovered/user_setup.c src/recovered/frontend_help.c
        src/recovered/roster_reset.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_user_setup_visibility_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_user_setup_visibility_tests PRIVATE src)
    add_test(NAME user_setup_visibility COMMAND nba97_user_setup_visibility_tests)
    add_executable(nba97_user_setup_tests tests/user_setup_test.cpp src/recovered/user_setup.c)
    target_compile_features(nba97_user_setup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_user_setup_tests PRIVATE src)
    add_test(NAME user_setup_core COMMAND nba97_user_setup_tests)
    add_executable(nba97_team_select_tests tests/team_select_test.cpp src/recovered/team_select.c src/recovered/game_setup.c)
    target_compile_features(nba97_team_select_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_team_select_tests PRIVATE src)
    add_test(NAME team_select_core COMMAND nba97_team_select_tests)
    add_executable(nba97_team_ratings_tests tests/team_ratings_test.cpp src/recovered/team_ratings.c)
    target_compile_features(nba97_team_ratings_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_team_ratings_tests PRIVATE src)
    add_test(NAME team_ratings_core COMMAND nba97_team_ratings_tests)
    add_executable(nba97_win32_keyboard_tests tests/win32_keyboard_test.cpp)
    target_compile_features(nba97_win32_keyboard_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_win32_keyboard_tests PRIVATE src)
    add_test(NAME win32_keyboard_delivery COMMAND nba97_win32_keyboard_tests)
    add_executable(nba97_create_player_tests tests/create_player_test.cpp
        src/recovered/create_player.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_create_player_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_create_player_tests PRIVATE src)
    add_test(NAME create_player_core COMMAND nba97_create_player_tests)
    add_executable(nba97_create_player_store_tests tests/create_player_store_test.cpp
        src/create_player_store.cpp src/recovered/create_player.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_create_player_store_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_create_player_store_tests PRIVATE src)
    add_test(NAME create_player_store COMMAND nba97_create_player_store_tests)
    add_executable(nba97_zdomf_model_tests tests/zdomf_model_test.cpp src/zdomf_model.cpp)
    target_compile_features(nba97_zdomf_model_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_model_tests PRIVATE src)
    add_test(NAME zdomf_model_decode COMMAND nba97_zdomf_model_tests)
    add_executable(nba97_zdomf_transform_tests tests/zdomf_transform_test.cpp
        src/zdomf_transform.cpp)
    target_compile_features(nba97_zdomf_transform_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_transform_tests PRIVATE src)
    add_test(NAME zdomf_transform_core COMMAND nba97_zdomf_transform_tests)
    add_executable(nba97_zdomf_gte_compose_tests tests/zdomf_gte_compose_test.cpp
        src/zdomf_gte_compose.cpp)
    target_compile_features(nba97_zdomf_gte_compose_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_gte_compose_tests PRIVATE src)
    add_test(NAME zdomf_gte_compose_core COMMAND nba97_zdomf_gte_compose_tests)
    add_executable(nba97_zdomf_projection_tests tests/zdomf_projection_test.cpp
        src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_zdomf_projection_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_projection_tests PRIVATE src)
    add_test(NAME zdomf_projection_core COMMAND nba97_zdomf_projection_tests)
    add_executable(nba97_zdomf_hierarchy_tests tests/zdomf_hierarchy_test.cpp
        src/zdomf_hierarchy.cpp)
    target_compile_features(nba97_zdomf_hierarchy_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_hierarchy_tests PRIVATE src)
    add_test(NAME zdomf_hierarchy_core COMMAND nba97_zdomf_hierarchy_tests)
    add_executable(nba97_zdomf_mocap_tests tests/zdomf_mocap_test.cpp
        src/zdomf_mocap.cpp src/zdomf_runtime.cpp src/zdomf_hierarchy.cpp
        src/zdomf_transform.cpp src/zdomf_gte_compose.cpp)
    target_compile_features(nba97_zdomf_mocap_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_mocap_tests PRIVATE src)
    add_test(NAME zdomf_mocap_core COMMAND nba97_zdomf_mocap_tests)
    add_executable(nba97_zdomf_hierarchy_smoke tests/zdomf_hierarchy_smoke.cpp
        src/zdomf_hierarchy.cpp src/zdomf_model.cpp src/zdomf_projection.cpp
        src/zdomf_transform.cpp src/zdomf_mocap.cpp src/zdomf_runtime.cpp
        src/zdomf_gte_compose.cpp)
    target_compile_features(nba97_zdomf_hierarchy_smoke PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_hierarchy_smoke PRIVATE src)
    add_executable(nba97_zdomf_runtime_diff tests/zdomf_runtime_diff.cpp
        src/zdomf_runtime_records.cpp src/zdomf_hierarchy.cpp src/zdomf_mocap.cpp
        src/zdomf_model.cpp src/zdomf_transform.cpp src/zdomf_gte_compose.cpp
        src/zdomf_projection.cpp src/zdomf_runtime.cpp)
    target_compile_features(nba97_zdomf_runtime_diff PRIVATE cxx_std_17)
    target_include_directories(nba97_zdomf_runtime_diff PRIVATE src)
    add_executable(nba97_release_tests tests/roster_release_test.cpp src/recovered/roster_release.c
        src/recovered/roster_trade.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_release_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_release_tests PRIVATE src)
    add_test(NAME release_players_core COMMAND nba97_release_tests)
    add_executable(nba97_sign_tests tests/roster_sign_test.cpp src/recovered/roster_sign.c
        src/recovered/roster_trade.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_sign_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_sign_tests PRIVATE src)
    add_test(NAME sign_free_agent_core COMMAND nba97_sign_tests)
    find_package(Threads REQUIRED)
    add_executable(nba97_trade_tests tests/roster_trade_test.cpp
        src/recovered/roster_trade.c src/recovered/roster_reorder.c
        src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_trade_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_trade_tests PRIVATE src)
    add_test(NAME trade_entry_core COMMAND nba97_trade_tests)
    add_executable(nba97_trade_screen_tests tests/trade_screen_test.cpp
        src/recovered/roster_trade.c src/recovered/roster_reorder.c
        src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_trade_screen_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_trade_screen_tests PRIVATE src)
    add_test(NAME trade_screen_core COMMAND nba97_trade_screen_tests)
    add_executable(nba97_trade_assets_tests tests/trade_assets_test.cpp src/trade_assets.cpp
        src/frontend_help.cpp src/psh_font.cpp src/roster_database.cpp
        src/recovered/frontend_help.c src/recovered/reorder_screen.c src/recovered/roster_reorder.c
        src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_trade_assets_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_trade_assets_tests PRIVATE src)
    add_test(NAME trade_assets_core COMMAND nba97_trade_assets_tests)
    add_executable(nba97_frontend_title_tests tests/frontend_title_test.cpp src/recovered/frontend_title.c)
    target_compile_features(nba97_frontend_title_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_title_tests PRIVATE src)
    add_test(NAME frontend_title_core COMMAND nba97_frontend_title_tests)
    add_executable(nba97_player_photo_tests tests/player_photo_test.cpp src/recovered/player_photo.c
        src/player_photo_loader.cpp src/player_portrait_archive.cpp src/recovered/frontend_resource.c)
    target_compile_features(nba97_player_photo_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_player_photo_tests PRIVATE src)
    target_link_libraries(nba97_player_photo_tests PRIVATE Threads::Threads)
    add_test(NAME player_photo_lifecycle COMMAND nba97_player_photo_tests)
    add_executable(nba97_player_portrait_archive_tests tests/player_portrait_archive_tests.cpp
        src/player_portrait_archive.cpp src/recovered/frontend_resource.c)
    target_compile_features(nba97_player_portrait_archive_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_player_portrait_archive_tests PRIVATE src)
    add_test(NAME player_portrait_archive COMMAND nba97_player_portrait_archive_tests)
    add_executable(nba97_player_photo_checksum_tests tests/player_photo_checksum_tests.cpp
        src/player_photo_loader.cpp src/player_portrait_archive.cpp src/recovered/frontend_resource.c src/recovered/player_photo.c)
    target_compile_features(nba97_player_photo_checksum_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_player_photo_checksum_tests PRIVATE src)
    target_link_libraries(nba97_player_photo_checksum_tests PRIVATE Threads::Threads)
    add_test(NAME player_photo_checksum COMMAND nba97_player_photo_checksum_tests)
    add_executable(nba97_frontend_palette_tests tests/frontend_palette_test.cpp src/recovered/frontend_palette.c)
    target_compile_features(nba97_frontend_palette_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_palette_tests PRIVATE src)
    add_test(NAME frontend_palette_core COMMAND nba97_frontend_palette_tests)
    add_executable(nba97_frontend_palette_assets_tests tests/frontend_palette_assets_test.cpp
        src/frontend_palette_assets.cpp src/recovered/frontend_palette.c)
    target_compile_features(nba97_frontend_palette_assets_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_palette_assets_tests PRIVATE src)
    add_test(NAME frontend_palette_assets COMMAND nba97_frontend_palette_assets_tests)
    add_executable(nba97_music_pcm_tests tests/music_pcm_test.cpp)
    target_compile_features(nba97_music_pcm_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_music_pcm_tests PRIVATE src)
    add_test(NAME music_pcm COMMAND nba97_music_pcm_tests)
    add_executable(nba97_native_frame_capture_tests tests/native_frame_capture_test.cpp src/native_frame_capture.cpp)
    target_compile_features(nba97_native_frame_capture_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_native_frame_capture_tests PRIVATE src)
    target_link_libraries(nba97_native_frame_capture_tests PRIVATE Threads::Threads)
    add_test(NAME native_frame_capture COMMAND nba97_native_frame_capture_tests)
    add_executable(nba97_reorder_tests
        tests/reorder_rosters_test.cpp
        tests/roster_lists_test.cpp
        src/reorder_preview.cpp
        src/psh_font.cpp
        src/recovered/roster_reorder.c
        src/recovered/roster_lists.c
        src/recovered/reorder_screen.c
        src/roster_database.cpp
        src/recovered/semantic_trace.c)
    target_compile_features(nba97_reorder_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_reorder_tests PRIVATE src)
    add_test(NAME reorder_rosters_core COMMAND nba97_reorder_tests)
    add_executable(nba97_frontend_help_tests tests/frontend_help_test.cpp
        src/frontend_help.cpp src/psh_font.cpp src/recovered/frontend_help.c
        src/recovered/roster_reorder.c src/recovered/roster_lists.c
        src/recovered/reorder_screen.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_frontend_help_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_help_tests PRIVATE src)
    add_test(NAME frontend_help_core COMMAND nba97_frontend_help_tests)
    add_executable(nba97_frontend_help_presentation_tests tests/frontend_help_presentation_test.cpp src/recovered/frontend_help.c)
    target_compile_features(nba97_frontend_help_presentation_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_help_presentation_tests PRIVATE src)
    add_test(NAME frontend_help_presentation COMMAND nba97_frontend_help_presentation_tests)
    add_executable(nba97_reorder_child_tests tests/reorder_child_test.cpp
        src/main_menu.cpp src/create_player_preview.cpp src/ps1_vram_texture.cpp src/zdomf_model.cpp src/zdomf_transform.cpp src/zdomf_projection.cpp src/zdomf_hierarchy.cpp src/zdomf_mocap.cpp src/zdomf_runtime.cpp src/zdomf_gte_compose.cpp src/frontend_settings.cpp src/user_profiles.cpp
        src/roster_database.cpp src/psh_font.cpp src/recovered/reorder_children.c
        src/recovered/reorder_screen.c src/recovered/roster_reorder.c
        src/recovered/roster_lists.c src/recovered/create_player.c
        src/recovered/semantic_trace.c)
    target_sources(nba97_reorder_child_tests PRIVATE src/recovered/match_setup.c src/recovered/team_select.c src/roster_ratings.cpp src/frontend_plate.cpp)
    target_compile_features(nba97_reorder_child_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_reorder_child_tests PRIVATE src)
    target_sources(nba97_reorder_child_tests PRIVATE src/recovered/roster_compare.c)
    target_sources(nba97_reorder_child_tests PRIVATE src/compare_assets.cpp)
    target_sources(nba97_reorder_child_tests PRIVATE src/frontend_palette_assets.cpp src/recovered/frontend_palette.c)
    add_test(NAME reorder_children_core COMMAND nba97_reorder_child_tests)
    target_sources(nba97_reorder_child_tests PRIVATE src/team_select_assets.cpp
        src/recovered/team_ratings.c src/recovered/game_setup.c src/frontend_help.cpp src/recovered/frontend_help.c
        src/user_setup_assets.cpp src/recovered/user_setup.c)
    add_executable(nba97_roster_compare_tests tests/roster_compare_test.cpp src/recovered/roster_compare.c)
    target_compile_features(nba97_roster_compare_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_roster_compare_tests PRIVATE src)
    add_test(NAME roster_compare_core COMMAND nba97_roster_compare_tests)
    add_executable(nba97_compare_assets_tests tests/compare_assets_test.cpp src/compare_assets.cpp
        src/roster_database.cpp src/recovered/reorder_screen.c src/recovered/roster_reorder.c
        src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_compare_assets_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_compare_assets_tests PRIVATE src)
    add_test(NAME compare_assets_core COMMAND nba97_compare_assets_tests)
    add_executable(nba97_roster_save_tests tests/roster_save_test.cpp src/roster_save_codec.cpp)
    target_compile_features(nba97_roster_save_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_roster_save_tests PRIVATE src)
    add_test(NAME roster_save_codec COMMAND nba97_roster_save_tests)
    add_executable(nba97_roster_database_save_tests tests/roster_database_save_test.cpp
        src/roster_save_codec.cpp src/roster_database.cpp src/recovered/roster_reorder.c
        src/recovered/roster_lists.c src/recovered/reorder_screen.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_roster_database_save_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_roster_database_save_tests PRIVATE src)
    if(MSVC)
        # MSVC debug _Hash_vec allocates an iterator proxy inside a noexcept
        # constructor. Global OOM injection there terminates the CRT instead
        # of exercising our transaction. Use release iterator semantics only
        # for this isolated fault-injection executable (all its TUs agree).
        # App and the other six native suites keep normal Debug checks.
        target_compile_definitions(nba97_roster_database_save_tests PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    endif()
    add_test(NAME roster_save_database COMMAND nba97_roster_database_save_tests)
    add_executable(nba97_roster_save_store_tests tests/roster_save_store_test.cpp
        src/roster_save_store.cpp src/roster_save_codec.cpp src/roster_database.cpp
        src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/reorder_screen.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_roster_save_store_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_roster_save_store_tests PRIVATE src)
    add_test(NAME roster_save_store COMMAND nba97_roster_save_store_tests)
    add_executable(nba97_roster_reset_tests tests/roster_reset_test.cpp
        src/roster_reset_assets.cpp src/psh_font.cpp src/recovered/roster_reset.c src/recovered/frontend_help.c
        src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_roster_reset_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_roster_reset_tests PRIVATE src)
    add_test(NAME roster_reset_core COMMAND nba97_roster_reset_tests)
    add_executable(nba97_player_notice_tests tests/player_notice_test.cpp src/player_notice.cpp
        src/frontend_help.cpp src/psh_font.cpp src/recovered/frontend_help.c)
    target_compile_features(nba97_player_notice_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_player_notice_tests PRIVATE src)
    add_test(NAME player_notice_core COMMAND nba97_player_notice_tests)
    add_executable(nba97_cool_fact_selection_tests tests/cool_fact_selection_test.cpp src/recovered/cool_fact_selection.c)
    target_compile_features(nba97_cool_fact_selection_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_cool_fact_selection_tests PRIVATE src)
    add_test(NAME cool_fact_selection_core COMMAND nba97_cool_fact_selection_tests)
    add_executable(nba97_ps1_vram_texture_tests tests/ps1_vram_texture_test.cpp src/ps1_vram_texture.cpp)
    target_compile_features(nba97_ps1_vram_texture_tests PRIVATE cxx_std_17)
    target_include_directories(nba97_ps1_vram_texture_tests PRIVATE src)
    add_test(NAME ps1_vram_texture_core COMMAND nba97_ps1_vram_texture_tests)
    add_executable(nba97_frontend_audio_scalars_tests tests/frontend_audio_scalars_test.cpp src/recovered/frontend_audio.c)
    target_compile_features(nba97_frontend_audio_scalars_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_audio_scalars_tests PRIVATE src)
    add_test(NAME frontend_audio_scalars COMMAND nba97_frontend_audio_scalars_tests)
    if(WIN32)
    add_executable(nba97_process_audio_capture_tests tests/process_audio_capture_test.cpp src/process_audio_capture.cpp src/frontend_music.cpp src/ea_schl.cpp)
    target_sources(nba97_process_audio_capture_tests PRIVATE src/music_playback.cpp
        src/recovered/music_routing.c src/recovered/music_voice.c src/recovered/music_stream.c src/recovered/frontend_title.c)
        target_compile_features(nba97_process_audio_capture_tests PRIVATE cxx_std_17)
        target_include_directories(nba97_process_audio_capture_tests PRIVATE src)
        target_link_libraries(nba97_process_audio_capture_tests PRIVATE ole32 uuid mmdevapi winmm)
        add_test(NAME process_audio_capture_guards COMMAND nba97_process_audio_capture_tests)
        add_executable(nba97_recovered_audio_tests tests/recovered_audio_test.cpp
            src/recovered_audio.cpp src/recovered_wave_output.cpp src/psx_adpcm.cpp src/recovered/frontend_audio.c)
        target_compile_features(nba97_recovered_audio_tests PRIVATE c_std_99 cxx_std_17)
        target_include_directories(nba97_recovered_audio_tests PRIVATE src)
        target_compile_definitions(nba97_recovered_audio_tests PRIVATE NOMINMAX)
        target_link_libraries(nba97_recovered_audio_tests PRIVATE winmm)
        add_test(NAME recovered_audio_gain COMMAND nba97_recovered_audio_tests)
        add_executable(nba97_recovered_wave_output_tests tests/recovered_wave_output_tests.cpp src/recovered_wave_output.cpp)
        target_compile_features(nba97_recovered_wave_output_tests PRIVATE cxx_std_17)
        target_include_directories(nba97_recovered_wave_output_tests PRIVATE src)
        target_compile_definitions(nba97_recovered_wave_output_tests PRIVATE NOMINMAX)
        target_link_libraries(nba97_recovered_wave_output_tests PRIVATE winmm)
        add_test(NAME recovered_wave_output_lifetime COMMAND nba97_recovered_wave_output_tests)
        add_executable(nba97_recovered_audio_lifetime_tests tests/recovered_audio_lifetime_tests.cpp
            src/recovered_audio.cpp src/recovered_wave_output.cpp src/psx_adpcm.cpp src/recovered/frontend_audio.c)
        target_compile_features(nba97_recovered_audio_lifetime_tests PRIVATE c_std_99 cxx_std_17)
        target_include_directories(nba97_recovered_audio_lifetime_tests PRIVATE src)
        target_compile_definitions(nba97_recovered_audio_lifetime_tests PRIVATE NOMINMAX)
        target_link_libraries(nba97_recovered_audio_lifetime_tests PRIVATE winmm)
        add_test(NAME recovered_audio_lifetime COMMAND nba97_recovered_audio_lifetime_tests)
    endif()
endif()
if(NBA97_RECOVERED_TESTS_ONLY)
    return()
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(progress
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/report_progress.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating decompilation and native-port progress reports"
    )
    add_custom_target(progress-check
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/report_progress.py --check
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking committed progress reports"
    )
endif()

if(WIN32)
    add_executable(nba97_capture_reference_audio tools/capture_reference_audio.cpp src/process_audio_capture.cpp)
    target_compile_features(nba97_capture_reference_audio PRIVATE cxx_std_17)
    target_include_directories(nba97_capture_reference_audio PRIVATE src)
    target_link_libraries(nba97_capture_reference_audio PRIVATE ole32 uuid mmdevapi)
    add_executable(nba97_boot_decomp
        src/win32_main.cpp
        src/native_frame_capture.cpp
        src/process_audio_capture.cpp
        src/intro_player.cpp
        src/frontend_music.cpp
        src/ea_schl.cpp
        src/psx_adpcm.cpp
        src/recovered_audio.cpp
        src/recovered_wave_output.cpp
        src/recovered/frontend_audio.c
        src/recovered/semantic_trace.c
        src/boot_flow.cpp
        src/main_menu.cpp
        src/frontend_settings.cpp
        src/roster_database.cpp
        src/user_profiles.cpp
        src/png_image.cpp
        src/player_photo_loader.cpp src/player_portrait_archive.cpp
        src/recovered/player_photo.c
        src/psh_image.cpp
        src/psh_font.cpp
    )
    target_compile_definitions(nba97_boot_decomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(nba97_boot_decomp PRIVATE
        user32 gdi32 ole32 oleaut32 strmiids uuid d3d9 windowscodecs winmm mmdevapi
    )
else()
    find_package(SDL2 REQUIRED)
    add_executable(nba97_boot_decomp
        src/main.cpp
        src/boot_flow.cpp
        src/main_menu.cpp
        src/frontend_settings.cpp
        src/roster_database.cpp
        src/user_profiles.cpp
        src/recovered/semantic_trace.c
        src/psh_image.cpp
        src/psh_font.cpp
    )
    target_link_libraries(nba97_boot_decomp PRIVATE SDL2::SDL2)
endif()

target_compile_features(nba97_boot_decomp PRIVATE c_std_99 cxx_std_17)
target_sources(nba97_boot_decomp PRIVATE src/recovered/team_select_poll.c src/recovered/team_select_placement.c
    src/recovered/team_select_text.c)
target_sources(nba97_boot_decomp PRIVATE src/frontend_plate.cpp src/recovered/match_setup.c src/recovered/match_strategy.c src/recovered/team_header.c src/recovered/match_controls.c
    src/match_controls.cpp src/match_assets.cpp src/match_snapshot.cpp src/roster_ratings.cpp
    src/recovered/game_controllers.c src/recovered/game_controller_selection.c
    src/recovered/game_player_bindings.c src/recovered/game_lineup_recovery.c
    src/recovered/game_period.c src/recovered/game_substitution.c src/recovered/music_stream.c
    src/recovered/game_period_dependencies.c src/match_runtime.cpp
    src/match_pose.cpp src/gameplay_pose.cpp src/recovered/game_pose_request.c src/recovered/game_pose_sample.c
    src/gameplay_animation.cpp src/recovered/game_animation_advance.c src/recovered/game_animation_queue.c
    src/recovered/game_player_physics.c src/recovered/game_player_update.c src/match_player_update.cpp
    src/recovered/game_player_jump.c src/recovered/voice_handles.c src/recovered/voice_channels.c
    src/recovered/game_substitution_support.c
    src/recovered/game_player_attributes.c src/music_playback.cpp src/recovered/music_transition.c src/recovered/frontend_resource.c
    src/recovered/frontend_resource_cleanup.c
    src/recovered/game_render_textures.c src/recovered/game_head_cache.c src/recovered/game_player_labels.c
    src/game_render_backend.cpp src/recovered/game_image_upload.c
    src/recovered/game_player_initialization.c src/gameplay_setup.cpp src/recovered/music_voice.c
    src/recovered/game_team_roles.c
    src/recovered/gameplay_mocap.c src/gameplay_mocap.cpp src/recovered/music_routing.c)
target_sources(nba97_boot_decomp PRIVATE src/roster_reset_assets.cpp src/recovered/roster_reset.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/create_player.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/team_select.c src/recovered/team_ratings.c src/recovered/game_setup.c
    src/team_select_assets.cpp src/recovered/user_setup.c src/user_setup_assets.cpp src/user_setup_session.cpp)
target_sources(nba97_boot_decomp PRIVATE src/create_player_preview.cpp)
target_sources(nba97_boot_decomp PRIVATE src/ps1_vram_texture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/zdomf_model.cpp)
target_sources(nba97_boot_decomp PRIVATE src/zdomf_transform.cpp)
target_sources(nba97_boot_decomp PRIVATE src/zdomf_projection.cpp)
target_sources(nba97_boot_decomp PRIVATE src/zdomf_hierarchy.cpp)
target_sources(nba97_boot_decomp PRIVATE src/zdomf_mocap.cpp src/zdomf_runtime.cpp src/zdomf_gte_compose.cpp)
target_sources(nba97_boot_decomp PRIVATE src/create_player_store.cpp)
target_sources(nba97_boot_decomp PRIVATE src/create_player_delete_assets.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/roster_reorder.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/roster_lists.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/roster_trade.c src/recovered/roster_sign.c src/trade_assets.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/roster_release.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/reorder_screen.c src/reorder_preview.cpp)
target_sources(nba97_boot_decomp PRIVATE src/frontend_help.cpp src/recovered/frontend_help.c)
target_sources(nba97_boot_decomp PRIVATE src/player_notice.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/cool_fact_selection.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_title.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/reorder_children.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/roster_compare.c)
target_sources(nba97_boot_decomp PRIVATE src/compare_assets.cpp)
target_sources(nba97_boot_decomp PRIVATE src/frontend_palette_assets.cpp src/recovered/frontend_palette.c)
target_sources(nba97_boot_decomp PRIVATE src/roster_save_codec.cpp)
target_sources(nba97_boot_decomp PRIVATE src/roster_save_store.cpp)
if(MSVC)
    target_compile_options(nba97_boot_decomp PRIVATE /W4 /permissive-)
else()
    target_compile_options(nba97_boot_decomp PRIVATE -Wall -Wextra -Wpedantic)
endif()
