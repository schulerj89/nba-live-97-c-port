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
    add_executable(nba97_player_photo_tests tests/player_photo_test.cpp src/recovered/player_photo.c)
    target_compile_features(nba97_player_photo_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_player_photo_tests PRIVATE src)
    target_link_libraries(nba97_player_photo_tests PRIVATE Threads::Threads)
    add_test(NAME player_photo_lifecycle COMMAND nba97_player_photo_tests)
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
    add_executable(nba97_reorder_child_tests tests/reorder_child_test.cpp
        src/main_menu.cpp src/create_player_preview.cpp src/ps1_vram_texture.cpp src/zdomf_model.cpp src/zdomf_transform.cpp src/zdomf_projection.cpp src/zdomf_hierarchy.cpp src/zdomf_mocap.cpp src/zdomf_runtime.cpp src/zdomf_gte_compose.cpp src/frontend_settings.cpp src/user_profiles.cpp
        src/roster_database.cpp src/psh_font.cpp src/recovered/reorder_children.c
        src/recovered/reorder_screen.c src/recovered/roster_reorder.c
        src/recovered/roster_lists.c src/recovered/create_player.c
        src/recovered/semantic_trace.c)
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
    if(WIN32)
    add_executable(nba97_process_audio_capture_tests tests/process_audio_capture_test.cpp src/process_audio_capture.cpp src/frontend_music.cpp)
        target_compile_features(nba97_process_audio_capture_tests PRIVATE cxx_std_17)
        target_include_directories(nba97_process_audio_capture_tests PRIVATE src)
        target_link_libraries(nba97_process_audio_capture_tests PRIVATE ole32 uuid mmdevapi winmm)
        add_test(NAME process_audio_capture_guards COMMAND nba97_process_audio_capture_tests)
        add_executable(nba97_recovered_audio_tests tests/recovered_audio_test.cpp
            src/recovered_audio.cpp src/psx_adpcm.cpp src/recovered/frontend_audio.c)
        target_compile_features(nba97_recovered_audio_tests PRIVATE c_std_99 cxx_std_17)
        target_include_directories(nba97_recovered_audio_tests PRIVATE src)
        target_compile_definitions(nba97_recovered_audio_tests PRIVATE NOMINMAX)
        target_link_libraries(nba97_recovered_audio_tests PRIVATE winmm)
        add_test(NAME recovered_audio_gain COMMAND nba97_recovered_audio_tests)
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
        src/psx_adpcm.cpp
        src/recovered_audio.cpp
        src/recovered/frontend_audio.c
        src/recovered/semantic_trace.c
        src/boot_flow.cpp
        src/main_menu.cpp
        src/frontend_settings.cpp
        src/roster_database.cpp
        src/user_profiles.cpp
        src/png_image.cpp
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
