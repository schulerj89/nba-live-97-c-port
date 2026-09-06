cmake_minimum_required(VERSION 3.20)
project(nba_live_97_decomp LANGUAGES C CXX)

include(CTest)
if(BUILD_TESTING)
    add_executable(nba97_gameload_main_tests tests/gameload_main_tests.cpp src/recovered/gameload_main.c src/gameload_main_adapter.cpp src/recovered/gameload_entry.c)
    target_compile_features(nba97_gameload_main_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_gameload_main_tests PRIVATE src src/recovered)
    add_test(NAME gameload_main COMMAND nba97_gameload_main_tests)
    add_executable(nba97_gameload_main_integration_tests tests/gameload_main_integration_tests.cpp src/recovered/gameload_main.c src/gameload_main_adapter.cpp src/recovered/gameload_entry.c src/gameload_main_capture.cpp)
    target_compile_features(nba97_gameload_main_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_gameload_main_integration_tests PRIVATE src src/recovered)
    add_test(NAME gameload_main_integration COMMAND nba97_gameload_main_integration_tests)
    add_executable(nba97_frontend_resource_lookup_tests tests/frontend_resource_lookup_tests.cpp src/recovered/frontend_resource_lookup.c src/frontend_resource_lookup_adapter.cpp src/recovered/frontend_resource_load.c src/recovered/frontend_memory_copy.c)
    target_compile_features(nba97_frontend_resource_lookup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_resource_lookup_tests PRIVATE src src/recovered)
    add_test(NAME frontend_resource_lookup COMMAND nba97_frontend_resource_lookup_tests)
    add_executable(nba97_frontend_resource_lookup_integration_tests tests/frontend_resource_lookup_integration_tests.cpp src/recovered/frontend_resource_lookup.c src/frontend_resource_lookup_adapter.cpp src/recovered/frontend_resource_load.c src/recovered/frontend_memory_copy.c src/frontend_resource_lookup_capture.cpp)
    target_compile_features(nba97_frontend_resource_lookup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_resource_lookup_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_resource_lookup_integration COMMAND nba97_frontend_resource_lookup_integration_tests)
    add_executable(nba97_gameload_entry_tests tests/gameload_entry_tests.cpp src/recovered/gameload_entry.c)
    target_compile_features(nba97_gameload_entry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_gameload_entry_tests PRIVATE src src/recovered)
    add_test(NAME gameload_entry COMMAND nba97_gameload_entry_tests)
    add_executable(nba97_gameload_entry_integration_tests tests/gameload_entry_integration_tests.cpp src/gameload_entry_capture.cpp src/recovered/gameload_entry.c src/gameload_entry_adapter.cpp src/recovered/frontend_main.c src/recovered/frontend_memory_copy.c src/frontend_memory_copy_adapter.cpp)
    target_compile_features(nba97_gameload_entry_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_gameload_entry_integration_tests PRIVATE src src/recovered)
    add_test(NAME gameload_entry_integration COMMAND nba97_gameload_entry_integration_tests)
    add_executable(nba97_frontend_resource_load_tests tests/frontend_resource_load_tests.cpp src/recovered/frontend_resource_load.c)
    target_compile_features(nba97_frontend_resource_load_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_resource_load_tests PRIVATE src src/recovered)
    add_test(NAME frontend_resource_load COMMAND nba97_frontend_resource_load_tests)
    add_executable(nba97_frontend_resource_load_integration_tests tests/frontend_resource_load_integration_tests.cpp src/frontend_resource_load_capture.cpp src/recovered/frontend_resource_load.c src/frontend_resource_load_adapter.cpp src/recovered/frontend_load_payload.c src/frontend_load_payload_adapter.cpp src/recovered/frontend_overlay_load.c)
    target_compile_features(nba97_frontend_resource_load_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_resource_load_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_resource_load_integration COMMAND nba97_frontend_resource_load_integration_tests)
    add_executable(nba97_frontend_memory_copy_tests tests/frontend_memory_copy_tests.cpp src/recovered/frontend_memory_copy.c)
    target_compile_features(nba97_frontend_memory_copy_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_memory_copy_tests PRIVATE src src/recovered)
    add_test(NAME frontend_memory_copy COMMAND nba97_frontend_memory_copy_tests)
    add_executable(nba97_frontend_memory_copy_integration_tests tests/frontend_memory_copy_integration_tests.cpp src/frontend_memory_copy_capture.cpp src/recovered/frontend_memory_copy.c src/frontend_memory_copy_adapter.cpp src/recovered/frontend_main.c)
    target_compile_features(nba97_frontend_memory_copy_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_memory_copy_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_memory_copy_integration COMMAND nba97_frontend_memory_copy_integration_tests)
    add_executable(nba97_frontend_load_payload_tests tests/frontend_load_payload_tests.cpp src/recovered/frontend_load_payload.c)
    target_compile_features(nba97_frontend_load_payload_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_load_payload_tests PRIVATE src src/recovered)
    add_test(NAME frontend_load_payload COMMAND nba97_frontend_load_payload_tests)
    add_executable(nba97_frontend_load_payload_integration_tests tests/frontend_load_payload_integration_tests.cpp src/frontend_load_payload_capture.cpp src/recovered/frontend_load_payload.c src/frontend_load_payload_adapter.cpp src/recovered/frontend_overlay_load.c)
    target_compile_features(nba97_frontend_load_payload_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_load_payload_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_load_payload_integration COMMAND nba97_frontend_load_payload_integration_tests)
    add_executable(nba97_frontend_overlay_load_tests tests/frontend_overlay_load_tests.cpp src/recovered/frontend_overlay_load.c)
    target_compile_features(nba97_frontend_overlay_load_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_overlay_load_tests PRIVATE src src/recovered)
    add_test(NAME frontend_overlay_load COMMAND nba97_frontend_overlay_load_tests)
    add_executable(nba97_frontend_overlay_load_integration_tests tests/frontend_overlay_load_integration_tests.cpp src/frontend_overlay_load_capture.cpp src/recovered/frontend_overlay_load.c src/frontend_overlay_load_adapter.cpp src/recovered/frontend_main.c)
    target_compile_features(nba97_frontend_overlay_load_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_overlay_load_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_overlay_load_integration COMMAND nba97_frontend_overlay_load_integration_tests)
    add_executable(nba97_frontend_io_complete_tests tests/frontend_io_complete_tests.cpp src/recovered/frontend_io_complete.c)
    target_compile_features(nba97_frontend_io_complete_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_io_complete_tests PRIVATE src src/recovered)
    add_test(NAME frontend_io_complete COMMAND nba97_frontend_io_complete_tests)
    add_executable(nba97_frontend_io_complete_integration_tests tests/frontend_io_complete_integration_tests.cpp src/frontend_io_complete_capture.cpp src/recovered/frontend_io_complete.c src/frontend_io_complete_adapter.cpp src/recovered/frontend_exit_drain.c src/frontend_io_drain_adapter.cpp src/recovered/frontend_io_drain.c)
    target_compile_features(nba97_frontend_io_complete_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_io_complete_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_io_complete_integration COMMAND nba97_frontend_io_complete_integration_tests)
    add_executable(nba97_frontend_io_drain_tests tests/frontend_io_drain_tests.cpp src/recovered/frontend_io_drain.c)
    target_compile_features(nba97_frontend_io_drain_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_io_drain_tests PRIVATE src src/recovered)
    add_test(NAME frontend_io_drain COMMAND nba97_frontend_io_drain_tests)
    add_executable(nba97_frontend_io_drain_integration_tests tests/frontend_io_drain_integration_tests.cpp src/frontend_io_drain_capture.cpp src/recovered/frontend_io_drain.c src/frontend_io_drain_adapter.cpp src/recovered/frontend_exit_drain.c src/frontend_exit_drain_adapter.cpp)
    target_compile_features(nba97_frontend_io_drain_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_io_drain_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_io_drain_integration COMMAND nba97_frontend_io_drain_integration_tests)
    add_executable(nba97_frontend_clock_read_tests tests/frontend_clock_read_tests.cpp src/recovered/frontend_clock_read.c)
    target_compile_features(nba97_frontend_clock_read_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_clock_read_tests PRIVATE src src/recovered)
    add_test(NAME frontend_clock_read COMMAND nba97_frontend_clock_read_tests)
    add_executable(nba97_frontend_clock_read_integration_tests tests/frontend_clock_read_integration_tests.cpp src/frontend_clock_read_capture.cpp src/recovered/frontend_clock_read.c src/frontend_clock_read_adapter.cpp src/recovered/frontend_exit_wait.c)
    target_compile_features(nba97_frontend_clock_read_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_clock_read_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_clock_read_integration COMMAND nba97_frontend_clock_read_integration_tests)
    add_executable(nba97_frontend_exit_drain_tests tests/frontend_exit_drain_tests.cpp src/recovered/frontend_exit_drain.c)
    target_compile_features(nba97_frontend_exit_drain_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_exit_drain_tests PRIVATE src src/recovered)
    add_test(NAME frontend_exit_drain COMMAND nba97_frontend_exit_drain_tests)
    add_executable(nba97_frontend_exit_drain_integration_tests tests/frontend_exit_drain_integration_tests.cpp src/frontend_exit_drain_capture.cpp src/recovered/frontend_exit_drain.c src/frontend_exit_drain_adapter.cpp src/recovered/frontend_exit_cleanup.c src/frontend_exit_cleanup_adapter.cpp src/recovered/frontend_exit_wait.c src/frontend_exit_wait_adapter.cpp)
    target_compile_features(nba97_frontend_exit_drain_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_exit_drain_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_exit_drain_integration COMMAND nba97_frontend_exit_drain_integration_tests)
    add_executable(nba97_frontend_exit_wait_tests tests/frontend_exit_wait_tests.cpp src/recovered/frontend_exit_wait.c)
    target_compile_features(nba97_frontend_exit_wait_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_exit_wait_tests PRIVATE src src/recovered)
    add_test(NAME frontend_exit_wait COMMAND nba97_frontend_exit_wait_tests)
    add_executable(nba97_frontend_exit_wait_integration_tests tests/frontend_exit_wait_integration_tests.cpp src/frontend_exit_wait_capture.cpp src/recovered/frontend_exit_wait.c src/frontend_exit_wait_adapter.cpp src/recovered/frontend_exit_cleanup.c src/frontend_exit_cleanup_adapter.cpp)
    target_compile_features(nba97_frontend_exit_wait_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_exit_wait_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_exit_wait_integration COMMAND nba97_frontend_exit_wait_integration_tests)
    add_executable(nba97_frontend_exit_cleanup_tests tests/frontend_exit_cleanup_tests.cpp src/recovered/frontend_exit_cleanup.c)
    target_compile_features(nba97_frontend_exit_cleanup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_exit_cleanup_tests PRIVATE src src/recovered)
    add_test(NAME frontend_exit_cleanup COMMAND nba97_frontend_exit_cleanup_tests)
    add_executable(nba97_frontend_exit_cleanup_integration_tests tests/frontend_exit_cleanup_integration_tests.cpp src/frontend_exit_cleanup_capture.cpp src/recovered/frontend_exit_cleanup.c src/frontend_exit_cleanup_adapter.cpp src/recovered/frontend_main.c src/frontend_main_adapter.cpp src/recovered/frontend_dispatch_entry.c src/frontend_dispatch_entry_adapter.cpp src/recovered/frontend_dispatch.c src/frontend_dispatch_adapter.cpp src/user_setup_session.cpp src/user_profiles.cpp src/recovered/user_setup.c src/recovered/frontend_help.c src/recovered/roster_reset.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_frontend_exit_cleanup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_exit_cleanup_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_exit_cleanup_integration COMMAND nba97_frontend_exit_cleanup_integration_tests)
    add_executable(nba97_frontend_main_tests tests/frontend_main_tests.cpp src/recovered/frontend_main.c)
    target_compile_features(nba97_frontend_main_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_main_tests PRIVATE src src/recovered)
    add_test(NAME frontend_main COMMAND nba97_frontend_main_tests)
    add_executable(nba97_frontend_main_integration_tests tests/frontend_main_integration_tests.cpp src/frontend_main_capture.cpp src/recovered/frontend_main.c src/frontend_main_adapter.cpp src/recovered/frontend_dispatch_entry.c src/frontend_dispatch_entry_adapter.cpp src/recovered/frontend_dispatch.c src/frontend_dispatch_adapter.cpp src/user_setup_session.cpp src/user_profiles.cpp src/recovered/user_setup.c src/recovered/frontend_help.c src/recovered/roster_reset.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_frontend_main_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_main_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_main_integration COMMAND nba97_frontend_main_integration_tests)
    add_executable(nba97_frontend_dispatch_entry_tests tests/frontend_dispatch_entry_tests.cpp src/recovered/frontend_dispatch_entry.c)
    target_compile_features(nba97_frontend_dispatch_entry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_dispatch_entry_tests PRIVATE src src/recovered)
    add_test(NAME frontend_dispatch_entry COMMAND nba97_frontend_dispatch_entry_tests)
    add_executable(nba97_frontend_dispatch_entry_integration_tests tests/frontend_dispatch_entry_integration_tests.cpp src/frontend_dispatch_entry_capture.cpp src/recovered/frontend_dispatch_entry.c src/frontend_dispatch_entry_adapter.cpp src/recovered/frontend_dispatch.c src/frontend_dispatch_adapter.cpp src/user_setup_session.cpp src/user_profiles.cpp src/recovered/user_setup.c src/recovered/frontend_help.c src/recovered/roster_reset.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_frontend_dispatch_entry_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_dispatch_entry_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_dispatch_entry_integration COMMAND nba97_frontend_dispatch_entry_integration_tests)
    add_executable(nba97_frontend_dispatch_tests tests/frontend_dispatch_tests.cpp src/recovered/frontend_dispatch.c src/frontend_dispatch_adapter.cpp)
    target_compile_features(nba97_frontend_dispatch_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_dispatch_tests PRIVATE src src/recovered)
    add_test(NAME frontend_dispatch COMMAND nba97_frontend_dispatch_tests)
    add_executable(nba97_frontend_dispatch_integration_tests tests/frontend_dispatch_integration_tests.cpp src/recovered/frontend_dispatch.c src/frontend_dispatch_adapter.cpp src/user_setup_session.cpp src/user_profiles.cpp src/recovered/user_setup.c src/recovered/frontend_help.c src/recovered/roster_reset.c src/recovered/roster_reorder.c src/recovered/roster_lists.c src/recovered/semantic_trace.c)
    target_compile_features(nba97_frontend_dispatch_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_frontend_dispatch_integration_tests PRIVATE src src/recovered)
    add_test(NAME frontend_dispatch_integration COMMAND nba97_frontend_dispatch_integration_tests)
    add_executable(nba97_game_team_tactics_update_tests tests/game_team_tactics_update_tests.cpp src/recovered/game_team_tactics_update.c src/game_team_tactics_update_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_team_tactics_update_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_team_tactics_update_tests PRIVATE src src/recovered)
    add_test(NAME game_team_tactics_update COMMAND nba97_game_team_tactics_update_tests)
    add_executable(nba97_game_team_tactics_update_integration_tests tests/game_team_tactics_update_integration_tests.cpp src/recovered/game_team_tactics_update.c src/game_team_tactics_update_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_team_tactics_update_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_team_tactics_update_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_team_tactics_update_integration COMMAND nba97_game_team_tactics_update_integration_tests)
    add_executable(nba97_game_rectangle_normalize_tests tests/game_rectangle_normalize_tests.cpp src/recovered/game_rectangle_normalize.c src/game_rectangle_normalize_adapter.cpp src/recovered/game_rectangle_upload_submit.c)
    target_compile_features(nba97_game_rectangle_normalize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rectangle_normalize_tests PRIVATE src src/recovered)
    add_test(NAME game_rectangle_normalize COMMAND nba97_game_rectangle_normalize_tests)
    add_executable(nba97_game_rectangle_normalize_integration_tests tests/game_rectangle_normalize_integration_tests.cpp src/recovered/game_rectangle_normalize.c src/game_rectangle_normalize_adapter.cpp src/recovered/game_rectangle_upload_submit.c)
    target_compile_features(nba97_game_rectangle_normalize_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rectangle_normalize_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_rectangle_normalize_integration COMMAND nba97_game_rectangle_normalize_integration_tests)
    add_executable(nba97_game_text_submission_tests tests/game_text_submission_tests.cpp src/recovered/game_text_submission.c src/game_text_submission_adapter.cpp src/recovered/game_countdown_ui_update.c src/recovered/game_clear_ordering_table.c)
    target_compile_features(nba97_game_text_submission_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_text_submission_tests PRIVATE src src/recovered)
    add_test(NAME game_text_submission COMMAND nba97_game_text_submission_tests)
    add_executable(nba97_game_text_submission_integration_tests tests/game_text_submission_integration_tests.cpp src/recovered/game_text_submission.c src/game_text_submission_adapter.cpp src/recovered/game_countdown_ui_update.c src/recovered/game_clear_ordering_table.c)
    target_compile_features(nba97_game_text_submission_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_text_submission_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_text_submission_integration COMMAND nba97_game_text_submission_integration_tests)
    add_executable(nba97_game_rectangle_upload_submit_tests tests/game_rectangle_upload_submit_tests.cpp src/recovered/game_rectangle_upload_submit.c src/game_rectangle_upload_submit_adapter.cpp src/recovered/game_image_record_upload.c)
    target_compile_features(nba97_game_rectangle_upload_submit_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rectangle_upload_submit_tests PRIVATE src src/recovered)
    add_test(NAME game_rectangle_upload_submit COMMAND nba97_game_rectangle_upload_submit_tests)
    add_executable(nba97_game_rectangle_upload_submit_integration_tests tests/game_rectangle_upload_submit_integration_tests.cpp src/recovered/game_rectangle_upload_submit.c src/game_rectangle_upload_submit_adapter.cpp src/recovered/game_image_record_upload.c)
    target_compile_features(nba97_game_rectangle_upload_submit_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rectangle_upload_submit_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_rectangle_upload_submit_integration COMMAND nba97_game_rectangle_upload_submit_integration_tests)
    add_executable(nba97_game_camera_remainder_gate_tests tests/game_camera_remainder_gate_tests.cpp src/recovered/game_camera_remainder_gate.c src/game_camera_remainder_gate_adapter.cpp src/recovered/game_camera_elapsed_dispatch.c src/recovered/game_camera_state_lookup.c src/game_camera_state_lookup_adapter.cpp)
    target_compile_features(nba97_game_camera_remainder_gate_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_remainder_gate_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_remainder_gate COMMAND nba97_game_camera_remainder_gate_tests)
    add_executable(nba97_game_camera_remainder_gate_integration_tests tests/game_camera_remainder_gate_integration_tests.cpp src/recovered/game_camera_remainder_gate.c src/game_camera_remainder_gate_adapter.cpp src/recovered/game_camera_elapsed_dispatch.c src/recovered/game_camera_state_lookup.c src/game_camera_state_lookup_adapter.cpp)
    target_compile_features(nba97_game_camera_remainder_gate_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_remainder_gate_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_remainder_gate_integration COMMAND nba97_game_camera_remainder_gate_integration_tests)
    add_executable(nba97_game_stamina_handicap_tests tests/game_stamina_handicap_tests.cpp src/recovered/game_stamina_handicap.c src/game_stamina_handicap_adapter.cpp src/recovered/game_match_tick.c src/recovered/game_actor_timers.c src/game_actor_timers_adapter.cpp)
    target_compile_features(nba97_game_stamina_handicap_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_stamina_handicap_tests PRIVATE src src/recovered)
    add_test(NAME game_stamina_handicap COMMAND nba97_game_stamina_handicap_tests)
    add_executable(nba97_game_stamina_handicap_integration_tests tests/game_stamina_handicap_integration_tests.cpp src/recovered/game_stamina_handicap.c src/game_stamina_handicap_adapter.cpp src/recovered/game_match_tick.c src/recovered/game_actor_timers.c src/game_actor_timers_adapter.cpp)
    target_compile_features(nba97_game_stamina_handicap_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_stamina_handicap_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_stamina_handicap_integration COMMAND nba97_game_stamina_handicap_integration_tests)
    add_executable(nba97_game_image_record_upload_tests tests/game_image_record_upload_tests.cpp src/recovered/game_image_record_upload.c src/game_image_record_upload_adapter.cpp src/recovered/game_countdown_ui_update.c)
    target_compile_features(nba97_game_image_record_upload_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_image_record_upload_tests PRIVATE src src/recovered)
    add_test(NAME game_image_record_upload COMMAND nba97_game_image_record_upload_tests)
    add_executable(nba97_game_image_record_upload_integration_tests tests/game_image_record_upload_integration_tests.cpp src/recovered/game_image_record_upload.c src/game_image_record_upload_adapter.cpp src/recovered/game_countdown_ui_update.c)
    target_compile_features(nba97_game_image_record_upload_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_image_record_upload_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_image_record_upload_integration COMMAND nba97_game_image_record_upload_integration_tests)
    add_executable(nba97_game_text_chain_clear_tests tests/game_text_chain_clear_tests.cpp src/recovered/game_text_chain_clear.c src/game_text_chain_clear_adapter.cpp src/recovered/game_countdown_ui_update.c)
    target_compile_features(nba97_game_text_chain_clear_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_text_chain_clear_tests PRIVATE src src/recovered)
    add_test(NAME game_text_chain_clear COMMAND nba97_game_text_chain_clear_tests)
    add_executable(nba97_game_text_chain_clear_integration_tests tests/game_text_chain_clear_integration_tests.cpp src/recovered/game_text_chain_clear.c src/game_text_chain_clear_adapter.cpp src/recovered/game_countdown_ui_update.c)
    target_compile_features(nba97_game_text_chain_clear_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_text_chain_clear_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_text_chain_clear_integration COMMAND nba97_game_text_chain_clear_integration_tests)
    add_executable(nba97_game_camera_state_lookup_tests tests/game_camera_state_lookup_tests.cpp src/recovered/game_camera_state_lookup.c src/game_camera_state_lookup_adapter.cpp src/recovered/game_camera_elapsed_dispatch.c)
    target_compile_features(nba97_game_camera_state_lookup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_state_lookup_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_state_lookup COMMAND nba97_game_camera_state_lookup_tests)
    add_executable(nba97_game_camera_state_lookup_integration_tests tests/game_camera_state_lookup_integration_tests.cpp src/recovered/game_camera_state_lookup.c src/game_camera_state_lookup_adapter.cpp src/recovered/game_camera_elapsed_dispatch.c)
    target_compile_features(nba97_game_camera_state_lookup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_state_lookup_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_state_lookup_integration COMMAND nba97_game_camera_state_lookup_integration_tests)
    add_executable(nba97_game_actor_timers_tests tests/game_actor_timers_tests.cpp src/recovered/game_actor_timers.c src/game_actor_timers_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_actor_timers_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_timers_tests PRIVATE src src/recovered)
    add_test(NAME game_actor_timers COMMAND nba97_game_actor_timers_tests)
    add_executable(nba97_game_actor_timers_integration_tests tests/game_actor_timers_integration_tests.cpp src/recovered/game_actor_timers.c src/game_actor_timers_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_actor_timers_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_timers_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_actor_timers_integration COMMAND nba97_game_actor_timers_integration_tests)
    add_executable(nba97_game_countdown_ui_update_tests tests/game_countdown_ui_update_tests.cpp src/recovered/game_countdown_ui_update.c src/game_countdown_ui_update_adapter.cpp src/recovered/game_frame_ui_service.c)
    target_compile_features(nba97_game_countdown_ui_update_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_countdown_ui_update_tests PRIVATE src src/recovered)
    add_test(NAME game_countdown_ui_update COMMAND nba97_game_countdown_ui_update_tests)
    add_executable(nba97_game_countdown_ui_update_integration_tests tests/game_countdown_ui_update_integration_tests.cpp src/recovered/game_countdown_ui_update.c src/game_countdown_ui_update_adapter.cpp src/recovered/game_frame_ui_service.c)
    target_compile_features(nba97_game_countdown_ui_update_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_countdown_ui_update_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_countdown_ui_update_integration COMMAND nba97_game_countdown_ui_update_integration_tests)
    add_executable(nba97_game_camera_elapsed_dispatch_tests tests/game_camera_elapsed_dispatch_tests.cpp src/recovered/game_camera_elapsed_dispatch.c src/game_camera_elapsed_dispatch_adapter.cpp src/recovered/game_camera_select.c)
    target_compile_features(nba97_game_camera_elapsed_dispatch_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_elapsed_dispatch_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_elapsed_dispatch COMMAND nba97_game_camera_elapsed_dispatch_tests)
    add_executable(nba97_game_camera_elapsed_dispatch_integration_tests tests/game_camera_elapsed_dispatch_integration_tests.cpp src/recovered/game_camera_elapsed_dispatch.c src/game_camera_elapsed_dispatch_adapter.cpp src/recovered/game_camera_select.c)
    target_compile_features(nba97_game_camera_elapsed_dispatch_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_elapsed_dispatch_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_elapsed_dispatch_integration COMMAND nba97_game_camera_elapsed_dispatch_integration_tests)
    add_executable(nba97_game_cross_half_rule_tests tests/game_cross_half_rule_tests.cpp src/recovered/game_cross_half_rule.c src/game_cross_half_rule_adapter.cpp src/recovered/game_match_tick.c src/recovered/game_rule_delay.c)
    target_compile_features(nba97_game_cross_half_rule_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_cross_half_rule_tests PRIVATE src src/recovered)
    add_test(NAME game_cross_half_rule COMMAND nba97_game_cross_half_rule_tests)
    add_executable(nba97_game_cross_half_rule_integration_tests tests/game_cross_half_rule_integration_tests.cpp src/recovered/game_cross_half_rule.c src/game_cross_half_rule_adapter.cpp src/recovered/game_match_tick.c src/recovered/game_rule_delay.c)
    target_compile_features(nba97_game_cross_half_rule_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_cross_half_rule_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_cross_half_rule_integration COMMAND nba97_game_cross_half_rule_integration_tests)
    add_executable(nba97_game_frame_ui_service_tests tests/game_frame_ui_service_tests.cpp src/recovered/game_frame_ui_service.c src/game_frame_ui_service_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_frame_ui_service_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_frame_ui_service_tests PRIVATE src src/recovered)
    add_test(NAME game_frame_ui_service COMMAND nba97_game_frame_ui_service_tests)
    add_executable(nba97_game_frame_ui_service_integration_tests tests/game_frame_ui_service_integration_tests.cpp src/recovered/game_frame_ui_service.c src/game_frame_ui_service_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_frame_ui_service_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_frame_ui_service_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_frame_ui_service_integration COMMAND nba97_game_frame_ui_service_integration_tests)
    add_executable(nba97_game_camera_phase_select_tests tests/game_camera_phase_select_tests.cpp src/recovered/game_camera_phase_select.c src/game_camera_phase_select_adapter.cpp src/recovered/game_camera_select.c)
    target_compile_features(nba97_game_camera_phase_select_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_phase_select_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_phase_select COMMAND nba97_game_camera_phase_select_tests)
    add_executable(nba97_game_camera_phase_select_integration_tests tests/game_camera_phase_select_integration_tests.cpp src/recovered/game_camera_phase_select.c src/game_camera_phase_select_adapter.cpp src/recovered/game_camera_select.c)
    target_compile_features(nba97_game_camera_phase_select_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_phase_select_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_phase_select_integration COMMAND nba97_game_camera_phase_select_integration_tests)
    add_executable(nba97_game_pregame_selection_screen_tests tests/game_pregame_selection_screen_tests.cpp src/recovered/game_pregame_selection_screen.c src/game_pregame_selection_screen_adapter.cpp src/recovered/game_period_presentation_finish.c)
    target_compile_features(nba97_game_pregame_selection_screen_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_pregame_selection_screen_tests PRIVATE src src/recovered)
    add_test(NAME game_pregame_selection_screen COMMAND nba97_game_pregame_selection_screen_tests)
    add_executable(nba97_game_pregame_selection_screen_integration_tests tests/game_pregame_selection_screen_integration_tests.cpp src/recovered/game_pregame_selection_screen.c src/game_pregame_selection_screen_adapter.cpp src/recovered/game_period_presentation_finish.c)
    target_compile_features(nba97_game_pregame_selection_screen_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_pregame_selection_screen_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_pregame_selection_screen_integration COMMAND nba97_game_pregame_selection_screen_integration_tests)
    add_executable(nba97_game_pregame_match_card_tests tests/game_pregame_match_card_tests.cpp src/recovered/game_pregame_match_card.c src/game_pregame_match_card_adapter.cpp src/recovered/game_period_presentation_finish.c)
    target_compile_features(nba97_game_pregame_match_card_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_pregame_match_card_tests PRIVATE src src/recovered)
    add_test(NAME game_pregame_match_card COMMAND nba97_game_pregame_match_card_tests)
    add_executable(nba97_game_pregame_match_card_integration_tests tests/game_pregame_match_card_integration_tests.cpp src/recovered/game_pregame_match_card.c src/game_pregame_match_card_adapter.cpp src/recovered/game_period_presentation_finish.c)
    target_compile_features(nba97_game_pregame_match_card_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_pregame_match_card_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_pregame_match_card_integration COMMAND nba97_game_pregame_match_card_integration_tests)
    add_executable(nba97_game_period_audio_noop_tests tests/game_period_audio_noop_tests.cpp src/recovered/game_period_audio_noop.c src/game_period_audio_noop_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_audio_noop_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_audio_noop_tests PRIVATE src src/recovered)
    add_test(NAME game_period_audio_noop COMMAND nba97_game_period_audio_noop_tests)
    add_executable(nba97_game_period_audio_noop_integration_tests tests/game_period_audio_noop_integration_tests.cpp src/recovered/game_period_audio_noop.c src/game_period_audio_noop_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_audio_noop_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_audio_noop_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_period_audio_noop_integration COMMAND nba97_game_period_audio_noop_integration_tests)
    add_executable(nba97_game_period_audio_flag_clear_tests tests/game_period_audio_flag_clear_tests.cpp src/recovered/game_period_audio_flag_clear.c src/game_period_audio_flag_clear_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_audio_flag_clear_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_audio_flag_clear_tests PRIVATE src src/recovered)
    add_test(NAME game_period_audio_flag_clear COMMAND nba97_game_period_audio_flag_clear_tests)
    add_executable(nba97_game_period_audio_flag_clear_integration_tests tests/game_period_audio_flag_clear_integration_tests.cpp src/recovered/game_period_audio_flag_clear.c src/game_period_audio_flag_clear_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_audio_flag_clear_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_audio_flag_clear_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_period_audio_flag_clear_integration COMMAND nba97_game_period_audio_flag_clear_integration_tests)
    add_executable(nba97_game_period_presentation_finish_tests tests/game_period_presentation_finish_tests.cpp src/recovered/game_period_presentation_finish.c src/game_period_presentation_finish_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_presentation_finish_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_presentation_finish_tests PRIVATE src src/recovered)
    add_test(NAME game_period_presentation_finish COMMAND nba97_game_period_presentation_finish_tests)
    add_executable(nba97_game_period_presentation_finish_integration_tests tests/game_period_presentation_finish_integration_tests.cpp src/recovered/game_period_presentation_finish.c src/game_period_presentation_finish_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_presentation_finish_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_presentation_finish_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_period_presentation_finish_integration COMMAND nba97_game_period_presentation_finish_integration_tests)
    add_executable(nba97_game_period_music_start_tests tests/game_period_music_start_tests.cpp src/recovered/game_period_music_start.c src/game_period_music_start_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_music_start_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_music_start_tests PRIVATE src src/recovered)
    add_test(NAME game_period_music_start COMMAND nba97_game_period_music_start_tests)
    add_executable(nba97_game_period_music_start_integration_tests tests/game_period_music_start_integration_tests.cpp src/recovered/game_period_music_start.c src/game_period_music_start_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_period_music_start_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_music_start_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_period_music_start_integration COMMAND nba97_game_period_music_start_integration_tests)
    add_executable(nba97_game_match_buffer_compress_tests tests/game_match_buffer_compress_tests.cpp src/recovered/game_match_buffer_compress.c src/game_match_buffer_compress_adapter.cpp src/recovered/game_match_buffer_record.c)
    target_compile_features(nba97_game_match_buffer_compress_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_compress_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_compress COMMAND nba97_game_match_buffer_compress_tests)
    add_executable(nba97_game_match_buffer_compress_integration_tests tests/game_match_buffer_compress_integration_tests.cpp src/recovered/game_match_buffer_compress.c src/game_match_buffer_compress_adapter.cpp src/recovered/game_match_buffer_record.c)
    target_compile_features(nba97_game_match_buffer_compress_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_compress_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_compress_integration COMMAND nba97_game_match_buffer_compress_integration_tests)
    add_executable(nba97_game_match_buffer_record_tests tests/game_match_buffer_record_tests.cpp src/recovered/game_match_buffer_record.c src/game_match_buffer_record_adapter.cpp src/recovered/game_period_startup.c src/recovered/game_match_buffer_rewind.c src/game_match_buffer_rewind_adapter.cpp src/recovered/game_memory_zero.c src/recovered/game_match_state_reset.c src/recovered/game_match_buffer_initialize.c)
    target_compile_features(nba97_game_match_buffer_record_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_record_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_record COMMAND nba97_game_match_buffer_record_tests)
    add_executable(nba97_game_match_buffer_record_integration_tests tests/game_match_buffer_record_integration_tests.cpp src/recovered/game_match_buffer_record.c src/game_match_buffer_record_adapter.cpp src/recovered/game_period_startup.c src/recovered/game_match_buffer_rewind.c src/game_match_buffer_rewind_adapter.cpp src/recovered/game_memory_zero.c src/recovered/game_match_state_reset.c src/recovered/game_match_buffer_initialize.c)
    target_compile_features(nba97_game_match_buffer_record_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_record_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_record_integration COMMAND nba97_game_match_buffer_record_integration_tests)
    add_executable(nba97_game_match_buffer_pending_tests tests/game_match_buffer_pending_tests.cpp src/recovered/game_match_buffer_pending.c src/game_match_buffer_pending_adapter.cpp src/recovered/game_period_startup.c)
    target_compile_features(nba97_game_match_buffer_pending_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_pending_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_pending COMMAND nba97_game_match_buffer_pending_tests)
    add_executable(nba97_game_match_buffer_pending_integration_tests tests/game_match_buffer_pending_integration_tests.cpp src/recovered/game_match_buffer_pending.c src/game_match_buffer_pending_adapter.cpp src/recovered/game_period_startup.c)
    target_compile_features(nba97_game_match_buffer_pending_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_pending_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_pending_integration COMMAND nba97_game_match_buffer_pending_integration_tests)
    add_executable(nba97_game_substitution_candidate_select_tests tests/game_substitution_candidate_select_tests.cpp src/recovered/game_substitution_candidate_select.c src/game_substitution_candidate_select_adapter.cpp src/recovered/game_team_strategy_apply.c)
    target_compile_features(nba97_game_substitution_candidate_select_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_substitution_candidate_select_tests PRIVATE src src/recovered)
    add_test(NAME game_substitution_candidate_select COMMAND nba97_game_substitution_candidate_select_tests)
    add_executable(nba97_game_substitution_candidate_select_integration_tests tests/game_substitution_candidate_select_integration_tests.cpp src/recovered/game_substitution_candidate_select.c src/game_substitution_candidate_select_adapter.cpp src/recovered/game_team_strategy_apply.c)
    target_compile_features(nba97_game_substitution_candidate_select_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_substitution_candidate_select_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_substitution_candidate_select_integration COMMAND nba97_game_substitution_candidate_select_integration_tests)
    add_executable(nba97_game_match_buffer_rewind_tests tests/game_match_buffer_rewind_tests.cpp src/recovered/game_match_buffer_rewind.c src/game_match_buffer_rewind_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c src/recovered/game_match_buffer_initialize.c src/game_match_buffer_initialize_adapter.cpp)
    target_compile_features(nba97_game_match_buffer_rewind_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_rewind_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_rewind COMMAND nba97_game_match_buffer_rewind_tests)
    add_executable(nba97_game_match_buffer_rewind_integration_tests tests/game_match_buffer_rewind_integration_tests.cpp src/recovered/game_match_buffer_rewind.c src/game_match_buffer_rewind_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c src/recovered/game_match_buffer_initialize.c src/game_match_buffer_initialize_adapter.cpp)
    target_compile_features(nba97_game_match_buffer_rewind_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_rewind_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_rewind_integration COMMAND nba97_game_match_buffer_rewind_integration_tests)
    add_executable(nba97_game_match_buffer_initialize_tests tests/game_match_buffer_initialize_tests.cpp src/recovered/game_match_buffer_initialize.c src/game_match_buffer_initialize_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_match_buffer_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_initialize_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_initialize COMMAND nba97_game_match_buffer_initialize_tests)
    add_executable(nba97_game_match_buffer_initialize_integration_tests tests/game_match_buffer_initialize_integration_tests.cpp src/recovered/game_match_buffer_initialize.c src/game_match_buffer_initialize_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_match_buffer_initialize_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_buffer_initialize_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_match_buffer_initialize_integration COMMAND nba97_game_match_buffer_initialize_integration_tests)
    add_executable(nba97_game_team_strategy_apply_tests tests/game_team_strategy_apply_tests.cpp src/recovered/game_team_strategy_apply.c src/game_team_strategy_apply_adapter.cpp src/recovered/game_match_state_reset.c)
    target_compile_features(nba97_game_team_strategy_apply_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_team_strategy_apply_tests PRIVATE src src/recovered)
    add_test(NAME game_team_strategy_apply COMMAND nba97_game_team_strategy_apply_tests)
    add_executable(nba97_game_team_strategy_apply_integration_tests tests/game_team_strategy_apply_integration_tests.cpp src/recovered/game_team_strategy_apply.c src/game_team_strategy_apply_adapter.cpp src/recovered/game_match_state_reset.c)
    target_compile_features(nba97_game_team_strategy_apply_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_team_strategy_apply_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_team_strategy_apply_integration COMMAND nba97_game_team_strategy_apply_integration_tests)
    add_executable(nba97_game_team_header_initialize_tests tests/game_team_header_initialize_tests.cpp src/recovered/game_team_header_initialize.c src/game_team_header_initialize_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_team_header_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_team_header_initialize_tests PRIVATE src src/recovered)
    add_test(NAME game_team_header_initialize COMMAND nba97_game_team_header_initialize_tests)
    add_executable(nba97_game_team_header_initialize_integration_tests tests/game_team_header_initialize_integration_tests.cpp src/recovered/game_team_header_initialize.c src/game_team_header_initialize_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_team_header_initialize_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_team_header_initialize_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_team_header_initialize_integration COMMAND nba97_game_team_header_initialize_integration_tests)
    add_executable(nba97_game_controller_profile_reset_tests tests/game_controller_profile_reset_tests.cpp src/recovered/game_controller_profile_reset.c src/game_controller_profile_reset_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_controller_profile_reset_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controller_profile_reset_tests PRIVATE src src/recovered)
    add_test(NAME game_controller_profile_reset COMMAND nba97_game_controller_profile_reset_tests)
    add_executable(nba97_game_controller_profile_reset_integration_tests tests/game_controller_profile_reset_integration_tests.cpp src/recovered/game_controller_profile_reset.c src/game_controller_profile_reset_adapter.cpp src/recovered/game_match_state_reset.c src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_controller_profile_reset_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controller_profile_reset_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_controller_profile_reset_integration COMMAND nba97_game_controller_profile_reset_integration_tests)
    add_executable(nba97_game_texture_window_command_tests tests/game_texture_window_command_tests.cpp src/recovered/game_texture_window_command.c)
    target_compile_features(nba97_game_texture_window_command_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_texture_window_command_tests PRIVATE src src/recovered)
    add_test(NAME game_texture_window_command COMMAND nba97_game_texture_window_command_tests)
    add_executable(nba97_game_texture_window_command_integration_tests tests/game_texture_window_command_integration_tests.cpp
        src/recovered/game_texture_window_command.c src/game_texture_window_command_adapter.cpp src/recovered/game_draw_packet.c src/recovered/game_draw_area_start.c src/game_draw_area_start_adapter.cpp src/recovered/game_draw_area_end.c src/game_draw_area_end_adapter.cpp src/recovered/game_draw_offset_command.c src/game_draw_offset_command_adapter.cpp src/recovered/game_draw_mode_command.c src/game_draw_mode_command_adapter.cpp)
    target_compile_features(nba97_game_texture_window_command_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_texture_window_command_integration_tests PRIVATE src)
    add_test(NAME game_texture_window_command_integration COMMAND nba97_game_texture_window_command_integration_tests)
    add_executable(nba97_game_match_state_reset_tests tests/game_match_state_reset_tests.cpp src/recovered/game_match_state_reset.c)
    target_compile_features(nba97_game_match_state_reset_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_state_reset_tests PRIVATE src src/recovered)
    add_test(NAME game_match_state_reset COMMAND nba97_game_match_state_reset_tests)
    add_executable(nba97_game_match_state_reset_integration_tests tests/game_match_state_reset_integration_tests.cpp
        src/recovered/game_match_state_reset.c src/game_match_state_reset_adapter.cpp src/recovered/game_match_initialize.c src/recovered/game_memory_zero.c src/recovered/game_roster_bindings.c)
    target_compile_features(nba97_game_match_state_reset_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_state_reset_integration_tests PRIVATE src)
    add_test(NAME game_match_state_reset_integration COMMAND nba97_game_match_state_reset_integration_tests)
    add_executable(nba97_game_draw_mode_command_tests tests/game_draw_mode_command_tests.cpp src/recovered/game_draw_mode_command.c)
    target_compile_features(nba97_game_draw_mode_command_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_mode_command_tests PRIVATE src src/recovered)
    add_test(NAME game_draw_mode_command COMMAND nba97_game_draw_mode_command_tests)
    add_executable(nba97_game_draw_mode_command_integration_tests tests/game_draw_mode_command_integration_tests.cpp
        src/recovered/game_draw_mode_command.c src/game_draw_mode_command_adapter.cpp src/recovered/game_draw_packet.c src/recovered/game_draw_area_start.c src/game_draw_area_start_adapter.cpp src/recovered/game_draw_area_end.c src/game_draw_area_end_adapter.cpp src/recovered/game_draw_offset_command.c src/game_draw_offset_command_adapter.cpp)
    target_compile_features(nba97_game_draw_mode_command_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_mode_command_integration_tests PRIVATE src)
    add_test(NAME game_draw_mode_command_integration COMMAND nba97_game_draw_mode_command_integration_tests)
    add_executable(nba97_game_draw_offset_command_tests tests/game_draw_offset_command_tests.cpp src/recovered/game_draw_offset_command.c)
    target_compile_features(nba97_game_draw_offset_command_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_offset_command_tests PRIVATE src src/recovered)
    add_test(NAME game_draw_offset_command COMMAND nba97_game_draw_offset_command_tests)
    add_executable(nba97_game_draw_offset_command_integration_tests tests/game_draw_offset_command_integration_tests.cpp
        src/recovered/game_draw_offset_command.c src/game_draw_offset_command_adapter.cpp src/recovered/game_draw_packet.c src/recovered/game_draw_area_start.c src/game_draw_area_start_adapter.cpp src/recovered/game_draw_area_end.c src/game_draw_area_end_adapter.cpp)
    target_compile_features(nba97_game_draw_offset_command_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_offset_command_integration_tests PRIVATE src)
    add_test(NAME game_draw_offset_command_integration COMMAND nba97_game_draw_offset_command_integration_tests)
    add_executable(nba97_game_gpu_packet_dma_tests tests/game_gpu_packet_dma_tests.cpp src/recovered/game_gpu_packet_dma.c)
    target_compile_features(nba97_game_gpu_packet_dma_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gpu_packet_dma_tests PRIVATE src src/recovered)
    add_test(NAME game_gpu_packet_dma COMMAND nba97_game_gpu_packet_dma_tests)
    add_executable(nba97_game_gpu_packet_dma_integration_tests tests/game_gpu_packet_dma_integration_tests.cpp
        src/recovered/game_gpu_packet_dma.c src/game_gpu_packet_dma_adapter.cpp src/recovered/game_graphics_submit.c)
    target_compile_features(nba97_game_gpu_packet_dma_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gpu_packet_dma_integration_tests PRIVATE src)
    add_test(NAME game_gpu_packet_dma_integration COMMAND nba97_game_gpu_packet_dma_integration_tests)
    add_executable(nba97_game_graphics_submit_tests tests/game_graphics_submit_tests.cpp src/recovered/game_graphics_submit.c)
    target_compile_features(nba97_game_graphics_submit_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_graphics_submit_tests PRIVATE src src/recovered)
    add_test(NAME game_graphics_submit COMMAND nba97_game_graphics_submit_tests)
    add_executable(nba97_game_graphics_submit_integration_tests tests/game_graphics_submit_integration_tests.cpp
        src/recovered/game_graphics_submit.c src/game_graphics_submit_adapter.cpp src/recovered/game_draw_environment.c src/recovered/game_bios_memory_copy.c)
    target_compile_features(nba97_game_graphics_submit_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_graphics_submit_integration_tests PRIVATE src)
    add_test(NAME game_graphics_submit_integration COMMAND nba97_game_graphics_submit_integration_tests)
    add_executable(nba97_game_draw_area_end_tests tests/game_draw_area_end_tests.cpp src/recovered/game_draw_area_end.c)
    target_compile_features(nba97_game_draw_area_end_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_area_end_tests PRIVATE src src/recovered)
    add_test(NAME game_draw_area_end COMMAND nba97_game_draw_area_end_tests)
    add_executable(nba97_game_draw_area_end_integration_tests tests/game_draw_area_end_integration_tests.cpp
        src/recovered/game_draw_area_end.c src/game_draw_area_end_adapter.cpp src/recovered/game_draw_packet.c src/recovered/game_draw_area_start.c src/game_draw_area_start_adapter.cpp)
    target_compile_features(nba97_game_draw_area_end_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_area_end_integration_tests PRIVATE src)
    add_test(NAME game_draw_area_end_integration COMMAND nba97_game_draw_area_end_integration_tests)
    add_executable(nba97_game_draw_area_start_tests tests/game_draw_area_start_tests.cpp src/recovered/game_draw_area_start.c)
    target_compile_features(nba97_game_draw_area_start_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_area_start_tests PRIVATE src src/recovered)
    add_test(NAME game_draw_area_start COMMAND nba97_game_draw_area_start_tests)
    add_executable(nba97_game_draw_area_start_integration_tests tests/game_draw_area_start_integration_tests.cpp
        src/recovered/game_draw_area_start.c src/game_draw_area_start_adapter.cpp src/recovered/game_draw_packet.c)
    target_compile_features(nba97_game_draw_area_start_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_area_start_integration_tests PRIVATE src)
    add_test(NAME game_draw_area_start_integration COMMAND nba97_game_draw_area_start_integration_tests)
    add_executable(nba97_game_draw_packet_tests tests/game_draw_packet_tests.cpp src/recovered/game_draw_packet.c)
    target_compile_features(nba97_game_draw_packet_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_packet_tests PRIVATE src src/recovered)
    add_test(NAME game_draw_packet COMMAND nba97_game_draw_packet_tests)
    add_executable(nba97_game_draw_packet_integration_tests tests/game_draw_packet_integration_tests.cpp
        src/recovered/game_draw_packet.c src/game_draw_packet_adapter.cpp src/recovered/game_draw_environment.c src/recovered/game_bios_memory_copy.c)
    target_compile_features(nba97_game_draw_packet_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_packet_integration_tests PRIVATE src)
    add_test(NAME game_draw_packet_integration COMMAND nba97_game_draw_packet_integration_tests)
    add_executable(nba97_game_draw_environment_tests tests/game_draw_environment_tests.cpp src/recovered/game_draw_environment.c src/game_draw_environment_adapter.cpp)
    target_compile_features(nba97_game_draw_environment_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_environment_tests PRIVATE src src/recovered)
    add_test(NAME game_draw_environment COMMAND nba97_game_draw_environment_tests)
    add_executable(nba97_game_draw_environment_integration_tests tests/game_draw_environment_integration_tests.cpp
        src/recovered/game_draw_environment.c src/game_draw_environment_adapter.cpp src/recovered/game_scene_startup.c src/recovered/game_bios_memory_copy.c)
    target_compile_features(nba97_game_draw_environment_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_draw_environment_integration_tests PRIVATE src)
    add_test(NAME game_draw_environment_integration COMMAND nba97_game_draw_environment_integration_tests)
    add_executable(nba97_game_gpu_control_command_tests tests/game_gpu_control_command_tests.cpp src/recovered/game_gpu_control_command.c)
    target_compile_features(nba97_game_gpu_control_command_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gpu_control_command_tests PRIVATE src src/recovered)
    add_test(NAME game_gpu_control_command COMMAND nba97_game_gpu_control_command_tests)
    add_executable(nba97_game_gpu_control_command_integration_tests tests/game_gpu_control_command_integration_tests.cpp
        src/recovered/game_gpu_control_command.c src/game_gpu_control_command_adapter.cpp src/recovered/game_display_environment.c src/recovered/game_display_mask_set.c)
    target_compile_features(nba97_game_gpu_control_command_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gpu_control_command_integration_tests PRIVATE src)
    add_test(NAME game_gpu_control_command_integration COMMAND nba97_game_gpu_control_command_integration_tests)
    add_executable(nba97_game_video_mode_tests tests/game_video_mode_tests.cpp src/recovered/game_video_mode.c)
    target_compile_features(nba97_game_video_mode_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_video_mode_tests PRIVATE src src/recovered)
    add_test(NAME game_video_mode COMMAND nba97_game_video_mode_tests)
    add_executable(nba97_game_video_mode_integration_tests tests/game_video_mode_integration_tests.cpp
        src/recovered/game_video_mode.c src/game_video_mode_adapter.cpp src/recovered/game_display_environment.c)
    target_compile_features(nba97_game_video_mode_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_video_mode_integration_tests PRIVATE src)
    add_test(NAME game_video_mode_integration COMMAND nba97_game_video_mode_integration_tests)
    add_executable(nba97_game_display_environment_tests tests/game_display_environment_tests.cpp src/recovered/game_display_environment.c)
    target_compile_features(nba97_game_display_environment_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_display_environment_tests PRIVATE src src/recovered)
    add_test(NAME game_display_environment COMMAND nba97_game_display_environment_tests)
    add_executable(nba97_game_display_environment_integration_tests tests/game_display_environment_integration_tests.cpp
        src/recovered/game_display_environment.c src/game_display_environment_adapter.cpp src/recovered/game_scene_startup.c src/recovered/game_bios_memory_copy.c)
    target_compile_features(nba97_game_display_environment_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_display_environment_integration_tests PRIVATE src)
    add_test(NAME game_display_environment_integration COMMAND nba97_game_display_environment_integration_tests)
    add_executable(nba97_game_bios_memory_copy_tests tests/game_bios_memory_copy_tests.cpp src/recovered/game_bios_memory_copy.c)
    target_compile_features(nba97_game_bios_memory_copy_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_bios_memory_copy_tests PRIVATE src src/recovered)
    add_test(NAME game_bios_memory_copy COMMAND nba97_game_bios_memory_copy_tests)
    add_executable(nba97_game_bios_memory_copy_integration_tests tests/game_bios_memory_copy_integration_tests.cpp
        src/recovered/game_bios_memory_copy.c src/game_bios_memory_copy_adapter.cpp src/recovered/game_speech_initialize.c)
    target_compile_features(nba97_game_bios_memory_copy_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_bios_memory_copy_integration_tests PRIVATE src)
    add_test(NAME game_bios_memory_copy_integration COMMAND nba97_game_bios_memory_copy_integration_tests)
    add_executable(nba97_game_gte_reference_transform_tests tests/game_gte_reference_transform_tests.cpp src/recovered/game_gte_reference_transform.c src/game_gte_reference_transform_adapter.cpp src/game_player_geometry.cpp)
    target_compile_features(nba97_game_gte_reference_transform_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gte_reference_transform_tests PRIVATE src src/recovered)
    add_test(NAME game_gte_reference_transform COMMAND nba97_game_gte_reference_transform_tests)
    add_executable(nba97_game_gte_reference_transform_integration_tests tests/game_gte_reference_transform_integration_tests.cpp src/recovered/game_gte_reference_transform.c src/game_gte_reference_transform_adapter.cpp src/game_player_geometry.cpp
        src/recovered/game_camera_frame_transform.c src/recovered/game_rotation_matrix.c src/game_rotation_matrix_adapter.cpp
        src/recovered/game_gte_rotation_install.c src/game_gte_rotation_install_adapter.cpp
        src/recovered/game_gte_translation_install.c src/game_gte_translation_install_adapter.cpp)
    target_compile_features(nba97_game_gte_reference_transform_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gte_reference_transform_integration_tests PRIVATE src)
    add_test(NAME game_gte_reference_transform_integration COMMAND nba97_game_gte_reference_transform_integration_tests)
    add_executable(nba97_game_gte_translation_install_tests tests/game_gte_translation_install_tests.cpp src/recovered/game_gte_translation_install.c)
    target_compile_features(nba97_game_gte_translation_install_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gte_translation_install_tests PRIVATE src src/recovered)
    add_test(NAME game_gte_translation_install COMMAND nba97_game_gte_translation_install_tests)
    add_executable(nba97_game_gte_translation_install_integration_tests tests/game_gte_translation_install_integration_tests.cpp
        src/recovered/game_gte_translation_install.c src/game_gte_translation_install_adapter.cpp src/recovered/game_camera_frame_transform.c
        src/recovered/game_rotation_matrix.c src/game_rotation_matrix_adapter.cpp
        src/recovered/game_gte_rotation_install.c src/game_gte_rotation_install_adapter.cpp)
    target_compile_features(nba97_game_gte_translation_install_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gte_translation_install_integration_tests PRIVATE src)
    add_test(NAME game_gte_translation_install_integration COMMAND nba97_game_gte_translation_install_integration_tests)
    add_executable(nba97_game_gte_rotation_install_tests tests/game_gte_rotation_install_tests.cpp src/recovered/game_gte_rotation_install.c)
    target_compile_features(nba97_game_gte_rotation_install_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gte_rotation_install_tests PRIVATE src src/recovered)
    add_test(NAME game_gte_rotation_install COMMAND nba97_game_gte_rotation_install_tests)
    add_executable(nba97_game_gte_rotation_install_integration_tests tests/game_gte_rotation_install_integration_tests.cpp
        src/recovered/game_gte_rotation_install.c src/game_gte_rotation_install_adapter.cpp src/recovered/game_camera_frame_transform.c
        src/recovered/game_rotation_matrix.c src/game_rotation_matrix_adapter.cpp)
    target_compile_features(nba97_game_gte_rotation_install_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gte_rotation_install_integration_tests PRIVATE src)
    add_test(NAME game_gte_rotation_install_integration COMMAND nba97_game_gte_rotation_install_integration_tests)
    add_executable(nba97_game_ordering_table_dma_tests tests/game_ordering_table_dma_tests.cpp src/recovered/game_ordering_table_dma.c)
    target_compile_features(nba97_game_ordering_table_dma_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ordering_table_dma_tests PRIVATE src src/recovered)
    add_test(NAME game_ordering_table_dma COMMAND nba97_game_ordering_table_dma_tests)
    add_executable(nba97_game_ordering_table_dma_integration_tests tests/game_ordering_table_dma_integration_tests.cpp
        src/recovered/game_ordering_table_dma.c src/game_ordering_table_dma_adapter.cpp src/recovered/game_clear_ordering_table.c)
    target_compile_features(nba97_game_ordering_table_dma_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ordering_table_dma_integration_tests PRIVATE src)
    add_test(NAME game_ordering_table_dma_integration COMMAND nba97_game_ordering_table_dma_integration_tests)
    add_executable(nba97_game_rotation_matrix_tests tests/game_rotation_matrix_tests.cpp src/recovered/game_rotation_matrix.c)
    target_compile_features(nba97_game_rotation_matrix_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rotation_matrix_tests PRIVATE src src/recovered)
    add_test(NAME game_rotation_matrix COMMAND nba97_game_rotation_matrix_tests)
    add_executable(nba97_game_rotation_matrix_integration_tests tests/game_rotation_matrix_integration_tests.cpp
        src/recovered/game_rotation_matrix.c src/game_rotation_matrix_adapter.cpp src/recovered/game_camera_frame_transform.c)
    target_compile_features(nba97_game_rotation_matrix_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rotation_matrix_integration_tests PRIVATE src)
    add_test(NAME game_rotation_matrix_integration COMMAND nba97_game_rotation_matrix_integration_tests)
    add_executable(nba97_game_camera_overlay_packets_tests tests/game_camera_overlay_packets_tests.cpp src/recovered/game_camera_overlay_packets.c)
    target_compile_features(nba97_game_camera_overlay_packets_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_overlay_packets_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_overlay_packets COMMAND nba97_game_camera_overlay_packets_tests)
    add_executable(nba97_game_camera_overlay_packets_integration_tests tests/game_camera_overlay_packets_integration_tests.cpp
        src/recovered/game_camera_overlay_packets.c src/game_camera_overlay_packets_adapter.cpp src/recovered/game_match_frame.c src/recovered/game_court_packets.c)
    target_compile_features(nba97_game_camera_overlay_packets_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_overlay_packets_integration_tests PRIVATE src)
    add_test(NAME game_camera_overlay_packets_integration COMMAND nba97_game_camera_overlay_packets_integration_tests)
    add_executable(nba97_game_clear_ordering_table_tests tests/game_clear_ordering_table_tests.cpp src/recovered/game_clear_ordering_table.c)
    target_compile_features(nba97_game_clear_ordering_table_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clear_ordering_table_tests PRIVATE src src/recovered)
    add_test(NAME game_clear_ordering_table COMMAND nba97_game_clear_ordering_table_tests)
    add_executable(nba97_game_clear_ordering_table_integration_tests tests/game_clear_ordering_table_integration_tests.cpp
        src/recovered/game_clear_ordering_table.c src/game_clear_ordering_table_adapter.cpp src/recovered/game_match_frame.c)
    target_compile_features(nba97_game_clear_ordering_table_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clear_ordering_table_integration_tests PRIVATE src src/recovered)
    add_test(NAME game_clear_ordering_table_integration COMMAND nba97_game_clear_ordering_table_integration_tests)
    add_executable(nba97_game_actor_collision_response_tests tests/game_actor_collision_response_tests.cpp src/recovered/game_actor_collision_response.c src/game_actor_collision_response_adapter.cpp src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_actor_collision_response_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_collision_response_tests PRIVATE src src/recovered)
    add_test(NAME game_actor_collision_response COMMAND nba97_game_actor_collision_response_tests)
    add_executable(nba97_game_actor_collision_response_integration_tests tests/game_actor_collision_response_integration_tests.cpp
        src/recovered/game_actor_collision_response.c src/game_actor_collision_response_adapter.cpp src/recovered/game_opponent_contact.c src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_actor_collision_response_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_collision_response_integration_tests PRIVATE src)
    add_test(NAME game_actor_collision_response_integration COMMAND nba97_game_actor_collision_response_integration_tests)
    add_executable(nba97_game_frame_interrupt_restore_tests tests/game_frame_interrupt_restore_tests.cpp src/recovered/game_frame_interrupt_restore.c)
    target_compile_features(nba97_game_frame_interrupt_restore_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_frame_interrupt_restore_tests PRIVATE src src/recovered)
    add_test(NAME game_frame_interrupt_restore COMMAND nba97_game_frame_interrupt_restore_tests)
    add_executable(nba97_game_frame_interrupt_restore_integration_tests tests/game_frame_interrupt_restore_integration_tests.cpp
        src/recovered/game_frame_interrupt_restore.c src/game_frame_interrupt_restore_adapter.cpp src/recovered/game_match_frame.c src/recovered/game_frame_interrupt_disable.c src/game_frame_interrupt_disable_adapter.cpp)
    target_compile_features(nba97_game_frame_interrupt_restore_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_frame_interrupt_restore_integration_tests PRIVATE src)
    add_test(NAME game_frame_interrupt_restore_integration COMMAND nba97_game_frame_interrupt_restore_integration_tests)
    add_executable(nba97_game_frame_interrupt_disable_tests tests/game_frame_interrupt_disable_tests.cpp src/recovered/game_frame_interrupt_disable.c)
    target_compile_features(nba97_game_frame_interrupt_disable_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_frame_interrupt_disable_tests PRIVATE src src/recovered)
    add_test(NAME game_frame_interrupt_disable COMMAND nba97_game_frame_interrupt_disable_tests)
    add_executable(nba97_game_frame_interrupt_disable_integration_tests tests/game_frame_interrupt_disable_integration_tests.cpp
        src/recovered/game_frame_interrupt_disable.c src/game_frame_interrupt_disable_adapter.cpp src/recovered/game_match_frame.c)
    target_compile_features(nba97_game_frame_interrupt_disable_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_frame_interrupt_disable_integration_tests PRIVATE src)
    add_test(NAME game_frame_interrupt_disable_integration COMMAND nba97_game_frame_interrupt_disable_integration_tests)
    add_executable(nba97_game_camera_frame_transform_tests tests/game_camera_frame_transform_tests.cpp src/recovered/game_camera_frame_transform.c)
    target_compile_features(nba97_game_camera_frame_transform_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_frame_transform_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_frame_transform COMMAND nba97_game_camera_frame_transform_tests)
    add_executable(nba97_game_camera_frame_transform_integration_tests tests/game_camera_frame_transform_integration_tests.cpp
        src/recovered/game_camera_frame_transform.c src/game_camera_frame_transform_adapter.cpp src/recovered/game_match_frame.c)
    target_compile_features(nba97_game_camera_frame_transform_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_frame_transform_integration_tests PRIVATE src)
    add_test(NAME game_camera_frame_transform_integration COMMAND nba97_game_camera_frame_transform_integration_tests)
    add_executable(nba97_game_opponent_contact_tests tests/game_opponent_contact_tests.cpp src/recovered/game_opponent_contact.c)
    target_compile_features(nba97_game_opponent_contact_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_opponent_contact_tests PRIVATE src src/recovered)
    add_test(NAME game_opponent_contact COMMAND nba97_game_opponent_contact_tests)
    add_executable(nba97_game_opponent_contact_integration_tests tests/game_opponent_contact_integration_tests.cpp
        src/recovered/game_opponent_contact.c src/game_opponent_contact_adapter.cpp src/recovered/game_actor_contact_eligibility.c
        src/game_actor_contact_eligibility_adapter.cpp src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_opponent_contact_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_opponent_contact_integration_tests PRIVATE src)
    add_test(NAME game_opponent_contact_integration COMMAND nba97_game_opponent_contact_integration_tests)
    add_executable(nba97_game_camera_override_end_tests tests/game_camera_override_end_tests.cpp src/recovered/game_camera_override_end.c)
    target_compile_features(nba97_game_camera_override_end_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_override_end_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_override_end COMMAND nba97_game_camera_override_end_tests)
    add_executable(nba97_game_camera_override_end_integration_tests tests/game_camera_override_end_integration_tests.cpp
        src/recovered/game_camera_override_end.c src/game_camera_override_end_adapter.cpp src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_camera_override_end_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_override_end_integration_tests PRIVATE src)
    add_test(NAME game_camera_override_end_integration COMMAND nba97_game_camera_override_end_integration_tests)
    add_executable(nba97_game_actor_contact_eligibility_tests tests/game_actor_contact_eligibility_tests.cpp src/recovered/game_actor_contact_eligibility.c)
    target_compile_features(nba97_game_actor_contact_eligibility_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_contact_eligibility_tests PRIVATE src src/recovered)
    add_test(NAME game_actor_contact_eligibility COMMAND nba97_game_actor_contact_eligibility_tests)
    add_executable(nba97_game_actor_contact_eligibility_integration_tests tests/game_actor_contact_eligibility_integration_tests.cpp
        src/recovered/game_actor_contact_eligibility.c src/game_actor_contact_eligibility_adapter.cpp src/recovered/game_actor_contact_gate.c src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_actor_contact_eligibility_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_contact_eligibility_integration_tests PRIVATE src)
    add_test(NAME game_actor_contact_eligibility_integration COMMAND nba97_game_actor_contact_eligibility_integration_tests)
    add_executable(nba97_game_actor_input_tests tests/game_actor_input_tests.cpp src/recovered/game_actor_input.c)
    target_compile_features(nba97_game_actor_input_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_input_tests PRIVATE src src/recovered)
    add_test(NAME game_actor_input COMMAND nba97_game_actor_input_tests)
    add_executable(nba97_game_actor_input_integration_tests tests/game_actor_input_integration_tests.cpp
        src/recovered/game_actor_input.c src/game_actor_input_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_actor_input_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_input_integration_tests PRIVATE src)
    add_test(NAME game_actor_input_integration COMMAND nba97_game_actor_input_integration_tests)
    add_executable(nba97_game_ball_acquire_tests tests/game_ball_acquire_tests.cpp src/recovered/game_ball_acquire.c)
    target_compile_features(nba97_game_ball_acquire_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_acquire_tests PRIVATE src src/recovered)
    add_test(NAME game_ball_acquire COMMAND nba97_game_ball_acquire_tests)
    add_executable(nba97_game_ball_acquire_integration_tests tests/game_ball_acquire_integration_tests.cpp
        src/recovered/game_ball_acquire.c src/game_ball_acquire_adapter.cpp src/recovered/game_ball_actor_contact.c src/game_ball_actor_contact_adapter.cpp src/recovered/game_actor_resume.c src/recovered/game_rule_delay.c)
    target_compile_features(nba97_game_ball_acquire_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_acquire_integration_tests PRIVATE src)
    add_test(NAME game_ball_acquire_integration COMMAND nba97_game_ball_acquire_integration_tests)
    add_executable(nba97_game_actor_contact_gate_tests tests/game_actor_contact_gate_tests.cpp src/recovered/game_actor_contact_gate.c)
    target_compile_features(nba97_game_actor_contact_gate_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_contact_gate_tests PRIVATE src src/recovered)
    add_test(NAME game_actor_contact_gate COMMAND nba97_game_actor_contact_gate_tests)
    add_executable(nba97_game_actor_contact_gate_integration_tests tests/game_actor_contact_gate_integration_tests.cpp
        src/recovered/game_actor_contact_gate.c src/game_actor_contact_gate_adapter.cpp src/recovered/game_contact_dispatch.c)
    target_compile_features(nba97_game_actor_contact_gate_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_contact_gate_integration_tests PRIVATE src)
    add_test(NAME game_actor_contact_gate_integration COMMAND nba97_game_actor_contact_gate_integration_tests)
    add_executable(nba97_game_contact_dispatch_tests tests/game_contact_dispatch_tests.cpp src/recovered/game_contact_dispatch.c)
    target_compile_features(nba97_game_contact_dispatch_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_contact_dispatch_tests PRIVATE src src/recovered)
    add_test(NAME game_contact_dispatch COMMAND nba97_game_contact_dispatch_tests)
    add_executable(nba97_game_contact_dispatch_integration_tests tests/game_contact_dispatch_integration_tests.cpp
        src/recovered/game_contact_dispatch.c src/game_contact_dispatch_adapter.cpp src/recovered/game_match_tick.c src/recovered/game_ball_contact_gate.c src/game_ball_contact_gate_adapter.cpp src/recovered/game_ball_actor_contact.c src/game_ball_actor_contact_adapter.cpp src/recovered/game_actor_resume.c src/recovered/game_rule_delay.c)
    target_compile_features(nba97_game_contact_dispatch_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_contact_dispatch_integration_tests PRIVATE src)
    add_test(NAME game_contact_dispatch_integration COMMAND nba97_game_contact_dispatch_integration_tests)
    add_executable(nba97_game_ball_contact_gate_tests tests/game_ball_contact_gate_tests.cpp src/recovered/game_ball_contact_gate.c)
    target_compile_features(nba97_game_ball_contact_gate_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_contact_gate_tests PRIVATE src src/recovered)
    add_test(NAME game_ball_contact_gate COMMAND nba97_game_ball_contact_gate_tests)
    add_executable(nba97_game_ball_contact_gate_integration_tests tests/game_ball_contact_gate_integration_tests.cpp
        src/recovered/game_ball_contact_gate.c src/game_ball_contact_gate_adapter.cpp src/recovered/game_ball_actor_contact.c src/game_ball_actor_contact_adapter.cpp src/recovered/game_actor_resume.c src/recovered/game_rule_delay.c)
    target_compile_features(nba97_game_ball_contact_gate_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_contact_gate_integration_tests PRIVATE src)
    add_test(NAME game_ball_contact_gate_integration COMMAND nba97_game_ball_contact_gate_integration_tests)
    add_executable(nba97_game_ball_actor_contact_tests tests/game_ball_actor_contact_tests.cpp src/recovered/game_ball_actor_contact.c)
    target_compile_features(nba97_game_ball_actor_contact_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_actor_contact_tests PRIVATE src src/recovered)
    add_test(NAME game_ball_actor_contact COMMAND nba97_game_ball_actor_contact_tests)
    add_executable(nba97_game_ball_actor_contact_integration_tests tests/game_ball_actor_contact_integration_tests.cpp
        src/recovered/game_ball_actor_contact.c src/game_ball_actor_contact_adapter.cpp src/recovered/game_actor_resume.c src/recovered/game_rule_delay.c)
    target_compile_features(nba97_game_ball_actor_contact_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_actor_contact_integration_tests PRIVATE src)
    add_test(NAME game_ball_actor_contact_integration COMMAND nba97_game_ball_actor_contact_integration_tests)
    add_executable(nba97_game_stream_queue_count_tests tests/game_stream_queue_count_tests.cpp src/recovered/game_stream_queue_count.c)
    target_compile_features(nba97_game_stream_queue_count_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_stream_queue_count_tests PRIVATE src src/recovered)
    add_test(NAME game_stream_queue_count COMMAND nba97_game_stream_queue_count_tests)
    add_executable(nba97_game_stream_queue_count_integration_tests tests/game_stream_queue_count_integration_tests.cpp
        src/recovered/game_stream_queue_count.c src/game_stream_queue_count_adapter.cpp src/recovered/game_stream_readiness.c)
    target_compile_features(nba97_game_stream_queue_count_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_stream_queue_count_integration_tests PRIVATE src)
    add_test(NAME game_stream_queue_count_integration COMMAND nba97_game_stream_queue_count_integration_tests)
    add_executable(nba97_game_actor_resume_tests tests/game_actor_resume_tests.cpp src/recovered/game_actor_resume.c)
    target_compile_features(nba97_game_actor_resume_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_resume_tests PRIVATE src src/recovered)
    add_test(NAME game_actor_resume COMMAND nba97_game_actor_resume_tests)
    add_executable(nba97_game_actor_resume_integration_tests tests/game_actor_resume_integration_tests.cpp
        src/recovered/game_actor_resume.c src/game_actor_resume_adapter.cpp src/recovered/game_period_expiry.c)
    target_compile_features(nba97_game_actor_resume_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_actor_resume_integration_tests PRIVATE src)
    add_test(NAME game_actor_resume_integration COMMAND nba97_game_actor_resume_integration_tests)
    add_executable(nba97_game_rule_delay_tests tests/game_rule_delay_tests.cpp src/recovered/game_rule_delay.c)
    target_compile_features(nba97_game_rule_delay_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rule_delay_tests PRIVATE src src/recovered)
    add_test(NAME game_rule_delay COMMAND nba97_game_rule_delay_tests)
    add_executable(nba97_game_rule_delay_integration_tests tests/game_rule_delay_integration_tests.cpp
        src/recovered/game_rule_delay.c src/game_rule_delay_adapter.cpp src/recovered/game_clock_violations.c)
    target_compile_features(nba97_game_rule_delay_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_rule_delay_integration_tests PRIVATE src)
    add_test(NAME game_rule_delay_integration COMMAND nba97_game_rule_delay_integration_tests)
    add_executable(nba97_game_stream_readiness_tests tests/game_stream_readiness_tests.cpp src/recovered/game_stream_readiness.c)
    target_compile_features(nba97_game_stream_readiness_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_stream_readiness_tests PRIVATE src src/recovered)
    add_test(NAME game_stream_readiness COMMAND nba97_game_stream_readiness_tests)
    add_executable(nba97_game_stream_readiness_integration_tests tests/game_stream_readiness_integration_tests.cpp
        src/recovered/game_stream_readiness.c src/game_stream_readiness_adapter.cpp src/recovered/game_match_audio_service.c
        src/game_match_audio_service_adapter.cpp src/recovered/game_clock_read.c src/recovered/game_audio_stream_status.c)
    target_compile_features(nba97_game_stream_readiness_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_stream_readiness_integration_tests PRIVATE src)
    add_test(NAME game_stream_readiness_integration COMMAND nba97_game_stream_readiness_integration_tests)
    add_executable(nba97_game_match_audio_service_tests tests/game_match_audio_service_tests.cpp src/recovered/game_match_audio_service.c)
    target_compile_features(nba97_game_match_audio_service_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_audio_service_tests PRIVATE src src/recovered)
    add_test(NAME game_match_audio_service COMMAND nba97_game_match_audio_service_tests)
    add_executable(nba97_game_match_audio_service_integration_tests tests/game_match_audio_service_integration_tests.cpp
        src/recovered/game_match_audio_service.c src/game_match_audio_service_adapter.cpp src/recovered/game_match_service_publish.c
        src/recovered/game_clock_read.c src/recovered/game_audio_stream_status.c)
    target_compile_features(nba97_game_match_audio_service_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_audio_service_integration_tests PRIVATE src)
    add_test(NAME game_match_audio_service_integration COMMAND nba97_game_match_audio_service_integration_tests)
    add_executable(nba97_game_clock_read_tests tests/game_clock_read_tests.cpp src/recovered/game_clock_read.c)
    target_compile_features(nba97_game_clock_read_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clock_read_tests PRIVATE src src/recovered)
    add_test(NAME game_clock_read COMMAND nba97_game_clock_read_tests)
    add_executable(nba97_game_clock_read_integration_tests tests/game_clock_read_integration_tests.cpp
        src/recovered/game_clock_read.c src/game_clock_read_adapter.cpp src/recovered/game_speech_startup.c)
    target_compile_features(nba97_game_clock_read_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clock_read_integration_tests PRIVATE src)
    add_test(NAME game_clock_read_integration COMMAND nba97_game_clock_read_integration_tests)
    add_executable(nba97_game_match_service_publish_tests tests/game_match_service_publish_tests.cpp src/recovered/game_match_service_publish.c)
    target_compile_features(nba97_game_match_service_publish_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_service_publish_tests PRIVATE src src/recovered)
    add_test(NAME game_match_service_publish COMMAND nba97_game_match_service_publish_tests)
    add_executable(nba97_game_match_service_publish_integration_tests tests/game_match_service_publish_integration_tests.cpp
        src/recovered/game_match_service_publish.c src/game_match_service_publish_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_match_service_publish_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_service_publish_integration_tests PRIVATE src)
    add_test(NAME game_match_service_publish_integration COMMAND nba97_game_match_service_publish_integration_tests)
    add_executable(nba97_game_period_expiry_tests tests/game_period_expiry_tests.cpp src/recovered/game_period_expiry.c)
    target_compile_features(nba97_game_period_expiry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_expiry_tests PRIVATE src src/recovered)
    add_test(NAME game_period_expiry COMMAND nba97_game_period_expiry_tests)
    add_executable(nba97_game_period_expiry_integration_tests tests/game_period_expiry_integration_tests.cpp
        src/recovered/game_period_expiry.c src/game_period_expiry_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_period_expiry_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_expiry_integration_tests PRIVATE src)
    add_test(NAME game_period_expiry_integration COMMAND nba97_game_period_expiry_integration_tests)
    add_executable(nba97_game_audio_stream_service_tests tests/game_audio_stream_service_tests.cpp src/recovered/game_audio_stream_service.c)
    target_compile_features(nba97_game_audio_stream_service_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_stream_service_tests PRIVATE src src/recovered)
    add_test(NAME game_audio_stream_service COMMAND nba97_game_audio_stream_service_tests)
    add_executable(nba97_game_audio_stream_service_integration_tests tests/game_audio_stream_service_integration_tests.cpp
        src/recovered/game_audio_stream_service.c src/game_audio_stream_service_adapter.cpp src/recovered/game_audio_stream_pump.c src/recovered/game_audio_stream_status.c src/game_audio_stream_status_adapter.cpp)
    target_compile_features(nba97_game_audio_stream_service_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_stream_service_integration_tests PRIVATE src)
    add_test(NAME game_audio_stream_service_integration COMMAND nba97_game_audio_stream_service_integration_tests)
    add_executable(nba97_game_clock_violations_tests tests/game_clock_violations_tests.cpp src/recovered/game_clock_violations.c)
    target_compile_features(nba97_game_clock_violations_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clock_violations_tests PRIVATE src src/recovered)
    add_test(NAME game_clock_violations COMMAND nba97_game_clock_violations_tests)
    add_executable(nba97_game_clock_violations_integration_tests tests/game_clock_violations_integration_tests.cpp
        src/recovered/game_clock_violations.c src/game_clock_violations_adapter.cpp src/recovered/game_match_tick.c src/recovered/game_match_clocks.c src/game_match_clocks_adapter.cpp)
    target_compile_features(nba97_game_clock_violations_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clock_violations_integration_tests PRIVATE src)
    add_test(NAME game_clock_violations_integration COMMAND nba97_game_clock_violations_integration_tests)
    add_executable(nba97_game_audio_stream_status_tests tests/game_audio_stream_status_tests.cpp src/recovered/game_audio_stream_status.c)
    target_compile_features(nba97_game_audio_stream_status_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_stream_status_tests PRIVATE src src/recovered)
    add_test(NAME game_audio_stream_status COMMAND nba97_game_audio_stream_status_tests)
    add_executable(nba97_game_audio_stream_status_integration_tests tests/game_audio_stream_status_integration_tests.cpp
        src/recovered/game_audio_stream_status.c src/game_audio_stream_status_adapter.cpp src/recovered/game_audio_stream_pump.c)
    target_compile_features(nba97_game_audio_stream_status_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_stream_status_integration_tests PRIVATE src)
    add_test(NAME game_audio_stream_status_integration COMMAND nba97_game_audio_stream_status_integration_tests)
    add_executable(nba97_game_match_clocks_tests tests/game_match_clocks_tests.cpp src/recovered/game_match_clocks.c)
    target_compile_features(nba97_game_match_clocks_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_clocks_tests PRIVATE src src/recovered)
    add_test(NAME game_match_clocks COMMAND nba97_game_match_clocks_tests)
    add_executable(nba97_game_match_clocks_integration_tests tests/game_match_clocks_integration_tests.cpp
        src/recovered/game_match_clocks.c src/game_match_clocks_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_match_clocks_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_clocks_integration_tests PRIVATE src)
    add_test(NAME game_match_clocks_integration COMMAND nba97_game_match_clocks_integration_tests)
    add_executable(nba97_game_audio_stream_pump_tests tests/game_audio_stream_pump_tests.cpp src/recovered/game_audio_stream_pump.c)
    target_compile_features(nba97_game_audio_stream_pump_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_stream_pump_tests PRIVATE src src/recovered)
    add_test(NAME game_audio_stream_pump COMMAND nba97_game_audio_stream_pump_tests)
    add_executable(nba97_game_audio_stream_pump_integration_tests tests/game_audio_stream_pump_integration_tests.cpp
        src/recovered/game_audio_stream_pump.c src/game_audio_stream_pump_adapter.cpp src/recovered/game_speech_startup.c src/recovered/game_controller_frame_reset.c)
    target_compile_features(nba97_game_audio_stream_pump_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_stream_pump_integration_tests PRIVATE src)
    add_test(NAME game_audio_stream_pump_integration COMMAND nba97_game_audio_stream_pump_integration_tests)
    add_executable(nba97_game_controller_frame_reset_tests tests/game_controller_frame_reset_tests.cpp src/recovered/game_controller_frame_reset.c)
    target_compile_features(nba97_game_controller_frame_reset_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controller_frame_reset_tests PRIVATE src src/recovered)
    add_test(NAME game_controller_frame_reset COMMAND nba97_game_controller_frame_reset_tests)
    add_executable(nba97_game_controller_frame_reset_integration_tests tests/game_controller_frame_reset_integration_tests.cpp
        src/recovered/game_controller_frame_reset.c src/game_controller_frame_reset_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_controller_frame_reset_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controller_frame_reset_integration_tests PRIVATE src)
    add_test(NAME game_controller_frame_reset_integration COMMAND nba97_game_controller_frame_reset_integration_tests)
    add_executable(nba97_game_tipoff_announcement_tests tests/game_tipoff_announcement_tests.cpp src/recovered/game_tipoff_announcement.c)
    target_compile_features(nba97_game_tipoff_announcement_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_tipoff_announcement_tests PRIVATE src src/recovered)
    add_test(NAME game_tipoff_announcement COMMAND nba97_game_tipoff_announcement_tests)
    add_executable(nba97_game_tipoff_announcement_integration_tests tests/game_tipoff_announcement_integration_tests.cpp
        src/recovered/game_tipoff_announcement.c src/game_tipoff_announcement_adapter.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_tipoff_announcement_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_tipoff_announcement_integration_tests PRIVATE src)
    add_test(NAME game_tipoff_announcement_integration COMMAND nba97_game_tipoff_announcement_integration_tests)
    add_executable(nba97_game_late_period_limits_tests tests/game_late_period_limits_tests.cpp src/recovered/game_late_period_limits.c)
    target_compile_features(nba97_game_late_period_limits_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_late_period_limits_tests PRIVATE src src/recovered)
    add_test(NAME game_late_period_limits COMMAND nba97_game_late_period_limits_tests)
    add_executable(nba97_game_late_period_limits_integration_tests tests/game_late_period_limits_integration_tests.cpp
        src/recovered/game_late_period_limits.c src/game_late_period_limits_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_late_period_limits_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_late_period_limits_integration_tests PRIVATE src)
    add_test(NAME game_late_period_limits_integration COMMAND nba97_game_late_period_limits_integration_tests)
    add_executable(nba97_game_camera_select_tests tests/game_camera_select_tests.cpp src/recovered/game_camera_select.c)
    target_compile_features(nba97_game_camera_select_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_select_tests PRIVATE src src/recovered)
    add_test(NAME game_camera_select COMMAND nba97_game_camera_select_tests)
    add_executable(nba97_game_camera_select_integration_tests tests/game_camera_select_integration_tests.cpp
        src/recovered/game_camera_select.c src/game_camera_select_adapter.cpp src/recovered/game_camera_startup.c)
    target_compile_features(nba97_game_camera_select_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_select_integration_tests PRIVATE src)
    add_test(NAME game_camera_select_integration COMMAND nba97_game_camera_select_integration_tests)
    add_executable(nba97_game_speech_startup_tests tests/game_speech_startup_tests.cpp src/recovered/game_speech_startup.c)
    target_compile_features(nba97_game_speech_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_speech_startup_tests PRIVATE src/recovered)
    add_test(NAME game_speech_startup COMMAND nba97_game_speech_startup_tests)
    add_executable(nba97_game_speech_startup_integration_tests tests/game_speech_startup_integration_tests.cpp
        src/recovered/game_speech_startup.c src/game_speech_startup_adapter.cpp src/recovered/game_scene_random_warmup.c
        src/recovered/game_random_seed.c src/game_random_seed_adapter.cpp)
    target_compile_features(nba97_game_speech_startup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_speech_startup_integration_tests PRIVATE src)
    add_test(NAME game_speech_startup_integration COMMAND nba97_game_speech_startup_integration_tests)
    add_executable(nba97_game_first_period_startup_tests tests/game_first_period_startup_tests.cpp src/recovered/game_first_period_startup.c)
    target_compile_features(nba97_game_first_period_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_first_period_startup_tests PRIVATE src)
    add_test(NAME game_first_period_startup COMMAND nba97_game_first_period_startup_tests)
    add_executable(nba97_game_first_period_startup_integration_tests tests/game_first_period_startup_integration_tests.cpp
        src/recovered/game_first_period_startup.c src/game_first_period_startup_adapter.cpp src/recovered/game_period_startup.c)
    target_compile_features(nba97_game_first_period_startup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_first_period_startup_integration_tests PRIVATE src)
    add_test(NAME game_first_period_startup_integration COMMAND nba97_game_first_period_startup_integration_tests)
    add_executable(nba97_game_speech_initialize_tests tests/game_speech_initialize_tests.cpp src/recovered/game_speech_initialize.c)
    target_compile_features(nba97_game_speech_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_speech_initialize_tests PRIVATE src)
    add_test(NAME game_speech_initialize COMMAND nba97_game_speech_initialize_tests)
    add_executable(nba97_game_speech_initialize_integration_tests tests/game_speech_initialize_integration_tests.cpp
        src/recovered/game_speech_initialize.c src/game_speech_initialize_adapter.cpp src/recovered/game_resource_loader.c
        src/recovered/game_match_initialize.c src/game_match_initialize_adapter.cpp src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_speech_initialize_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_speech_initialize_integration_tests PRIVATE src)
    add_test(NAME game_speech_initialize_integration COMMAND nba97_game_speech_initialize_integration_tests)
    add_executable(nba97_game_camera_startup_tests tests/game_camera_startup_tests.cpp src/recovered/game_camera_startup.c)
    target_compile_features(nba97_game_camera_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_startup_tests PRIVATE src)
    add_test(NAME game_camera_startup COMMAND nba97_game_camera_startup_tests)
    add_executable(nba97_game_camera_startup_integration_tests tests/game_camera_startup_integration_tests.cpp
        src/recovered/game_camera_startup.c src/game_camera_startup_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_camera_startup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_startup_integration_tests PRIVATE src)
    add_test(NAME game_camera_startup_integration COMMAND nba97_game_camera_startup_integration_tests)
    add_executable(nba97_game_random_seed_tests tests/game_random_seed_tests.cpp src/recovered/game_random_seed.c)
    target_compile_features(nba97_game_random_seed_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_random_seed_tests PRIVATE src)
    add_test(NAME game_random_seed COMMAND nba97_game_random_seed_tests)
    add_executable(nba97_game_random_seed_integration_tests tests/game_random_seed_integration_tests.cpp
        src/recovered/game_random_seed.c src/game_random_seed_adapter.cpp src/recovered/game_scene_random_warmup.c)
    target_compile_features(nba97_game_random_seed_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_random_seed_integration_tests PRIVATE src)
    add_test(NAME game_random_seed_integration COMMAND nba97_game_random_seed_integration_tests)
    add_executable(nba97_game_period_startup_tests tests/game_period_startup_tests.cpp src/recovered/game_period_startup.c)
    target_compile_features(nba97_game_period_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_startup_tests PRIVATE src)
    add_test(NAME game_period_startup COMMAND nba97_game_period_startup_tests)
    add_executable(nba97_game_period_startup_integration_tests tests/game_period_startup_integration_tests.cpp
        src/recovered/game_period_startup.c src/game_period_startup_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_period_startup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_period_startup_integration_tests PRIVATE src)
    add_test(NAME game_period_startup_integration COMMAND nba97_game_period_startup_integration_tests)
    add_executable(nba97_game_match_hot_start_tests tests/game_match_hot_start_tests.cpp src/recovered/game_match_hot_start.c)
    target_compile_features(nba97_game_match_hot_start_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_hot_start_tests PRIVATE src)
    add_test(NAME game_match_hot_start COMMAND nba97_game_match_hot_start_tests)
    add_executable(nba97_game_match_hot_start_integration_tests tests/game_match_hot_start_integration_tests.cpp
        src/recovered/game_match_hot_start.c src/game_match_hot_start_adapter.cpp src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_match_hot_start_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_hot_start_integration_tests PRIVATE src)
    add_test(NAME game_match_hot_start_integration COMMAND nba97_game_match_hot_start_integration_tests)
endif()
option(NBA97_RECOVERED_TESTS_ONLY "Build asset-free recovered logic tests without the UI" OFF)
if(MSVC)
    # Visual Studio can compile a target's translation units concurrently.
    # Serialize writes to the shared compiler PDB so clean parallel builds do
    # not intermittently fail with C1041.
    add_compile_options(/FS)
endif()
if(BUILD_TESTING)
    add_executable(nba97_game_scene_resources_tests
        tests/game_scene_resources_tests.cpp src/recovered/game_scene_resources.c)
    target_compile_features(nba97_game_scene_resources_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_resources_tests PRIVATE src)
    add_test(NAME game_scene_resources COMMAND nba97_game_scene_resources_tests)
    add_executable(nba97_game_scene_resources_integration_tests
        tests/game_scene_resources_integration_tests.cpp src/recovered/game_scene_resources.c
        src/game_scene_resources_adapter.cpp src/recovered/game_scene_startup.c
        src/recovered/game_resource_loader.c)
    target_compile_features(nba97_game_scene_resources_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_resources_integration_tests PRIVATE src)
    add_test(NAME game_scene_resources_integration COMMAND nba97_game_scene_resources_integration_tests)
    add_executable(nba97_game_scene_startup_tests
        tests/game_scene_startup_tests.cpp src/recovered/game_scene_startup.c)
    target_compile_features(nba97_game_scene_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_startup_tests PRIVATE src/recovered)
    add_test(NAME game_scene_startup COMMAND nba97_game_scene_startup_tests)
    add_executable(nba97_game_scene_startup_integration_tests
        tests/game_scene_startup_integration_tests.cpp src/recovered/game_scene_startup.c
        src/game_scene_startup_adapter.cpp src/recovered/game_scene_load.c)
    target_compile_features(nba97_game_scene_startup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_startup_integration_tests PRIVATE src)
    add_test(NAME game_scene_startup_integration COMMAND nba97_game_scene_startup_integration_tests)
    add_executable(nba97_game_scene_random_warmup_tests
        tests/game_scene_random_warmup_tests.cpp src/recovered/game_scene_random_warmup.c)
    target_compile_features(nba97_game_scene_random_warmup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_random_warmup_tests PRIVATE src)
    add_test(NAME game_scene_random_warmup COMMAND nba97_game_scene_random_warmup_tests)
    add_executable(nba97_game_scene_random_warmup_integration_tests
        tests/game_scene_random_warmup_integration_tests.cpp src/recovered/game_scene_random_warmup.c
        src/game_scene_random_warmup_adapter.cpp src/recovered/game_scene_load.c)
    target_compile_features(nba97_game_scene_random_warmup_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_random_warmup_integration_tests PRIVATE src)
    add_test(NAME game_scene_random_warmup_integration COMMAND nba97_game_scene_random_warmup_integration_tests)
    add_executable(nba97_game_loop_entry_tests
        tests/game_loop_entry_tests.cpp src/recovered/game_loop_entry.c)
    target_compile_features(nba97_game_loop_entry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_loop_entry_tests PRIVATE src)
    add_test(NAME game_loop_entry COMMAND nba97_game_loop_entry_tests)
    add_executable(nba97_game_loop_entry_integration_tests
        tests/game_loop_entry_integration_tests.cpp src/recovered/game_loop_entry.c
        src/game_loop_entry_adapter.cpp src/recovered/game_match_tick.c
        src/recovered/game_match_session.c)
    target_compile_features(nba97_game_loop_entry_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_loop_entry_integration_tests PRIVATE src)
    add_test(NAME game_loop_entry_integration COMMAND nba97_game_loop_entry_integration_tests)
    add_executable(nba97_game_audio_initialize_tests
        tests/game_audio_initialize_tests.cpp src/recovered/game_audio_initialize.c)
    target_compile_features(nba97_game_audio_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_initialize_tests PRIVATE src)
    add_test(NAME game_audio_initialize COMMAND nba97_game_audio_initialize_tests)
    add_executable(nba97_game_audio_initialize_integration_tests
        tests/game_audio_initialize_integration_tests.cpp src/recovered/game_audio_initialize.c
        src/game_audio_initialize_adapter.cpp src/recovered/game_resource_loader.c
        src/recovered/game_match_initialize.c src/game_match_initialize_adapter.cpp
        src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_audio_initialize_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_audio_initialize_integration_tests PRIVATE src)
    add_test(NAME game_audio_initialize_integration COMMAND nba97_game_audio_initialize_integration_tests)
    add_executable(nba97_game_scene_load_tests
        tests/game_scene_load_tests.cpp src/recovered/game_scene_load.c)
    target_compile_features(nba97_game_scene_load_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_load_tests PRIVATE src)
    add_test(NAME game_scene_load COMMAND nba97_game_scene_load_tests)
    add_executable(nba97_game_scene_load_integration_tests
        tests/game_scene_load_integration_tests.cpp src/recovered/game_scene_load.c
        src/game_scene_load_adapter.cpp src/recovered/game_match_session.c)
    target_compile_features(nba97_game_scene_load_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scene_load_integration_tests PRIVATE src)
    add_test(NAME game_scene_load_integration COMMAND nba97_game_scene_load_integration_tests)
    add_executable(nba97_game_roster_bindings_tests
        tests/game_roster_bindings_tests.cpp src/recovered/game_roster_bindings.c)
    target_compile_features(nba97_game_roster_bindings_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_roster_bindings_tests PRIVATE src)
    add_test(NAME game_roster_bindings COMMAND nba97_game_roster_bindings_tests)
    add_executable(nba97_game_roster_bindings_integration_tests
        tests/game_roster_bindings_integration_tests.cpp src/recovered/game_roster_bindings.c
        src/recovered/game_match_initialize.c src/game_match_initialize_adapter.cpp
        src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_roster_bindings_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_roster_bindings_integration_tests PRIVATE src)
    add_test(NAME game_roster_bindings_integration COMMAND nba97_game_roster_bindings_integration_tests)
    add_executable(nba97_game_match_initialize_tests
        tests/game_match_initialize_tests.cpp src/recovered/game_match_initialize.c
        src/game_match_initialize_adapter.cpp src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_match_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_initialize_tests PRIVATE src)
    add_test(NAME game_match_initialize COMMAND nba97_game_match_initialize_tests)
    add_executable(nba97_game_match_initialize_integration_tests
        tests/game_match_initialize_integration_tests.cpp src/recovered/game_match_initialize.c
        src/game_match_initialize_adapter.cpp src/recovered/game_memory_zero.c
        src/recovered/game_match_session.c)
    target_compile_features(nba97_game_match_initialize_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_initialize_integration_tests PRIVATE src)
    add_test(NAME game_match_initialize_integration COMMAND nba97_game_match_initialize_integration_tests)
    add_executable(nba97_feload_entry_tests tests/feload_entry_tests.cpp
        src/recovered/feload_entry.c)
    target_compile_features(nba97_feload_entry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_feload_entry_tests PRIVATE src)
    add_test(NAME feload_entry COMMAND nba97_feload_entry_tests)
    add_executable(nba97_feload_entry_integration_tests
        tests/feload_entry_integration_tests.cpp src/recovered/feload_entry.c
        src/recovered/game_main.c src/recovered/game_memory_copy.c)
    target_compile_features(nba97_feload_entry_integration_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_feload_entry_integration_tests PRIVATE src)
    add_test(NAME feload_entry_integration COMMAND nba97_feload_entry_integration_tests)
    add_executable(nba97_game_pose_frame_tests tests/game_pose_frame_tests.cpp
        src/recovered/game_pose_frame.c src/recovered/game_pose_sample.c)
    target_compile_features(nba97_game_pose_frame_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_pose_frame_tests PRIVATE src)
    add_test(NAME game_pose_frame COMMAND nba97_game_pose_frame_tests)
    add_executable(nba97_game_player_label_frame_tests tests/game_player_label_frame_tests.cpp
        src/recovered/game_player_label_frame.c src/recovered/game_court_packets.c)
    target_compile_features(nba97_game_player_label_frame_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_label_frame_tests PRIVATE src)
    add_test(NAME game_player_label_frame COMMAND nba97_game_player_label_frame_tests)
    add_executable(nba97_game_player_marker_update_tests tests/game_player_marker_update_tests.cpp
        src/recovered/game_player_marker_update.c src/game_player_marker_update.cpp
        src/recovered/game_image_upload.c src/game_render_backend.cpp)
    target_compile_features(nba97_game_player_marker_update_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_marker_update_tests PRIVATE src)
    add_test(NAME game_player_marker_update COMMAND nba97_game_player_marker_update_tests)
    add_executable(nba97_game_ball_simulation_tests tests/game_ball_simulation_tests.cpp
        src/recovered/game_ball_simulation.c src/recovered/game_ball_attachment.c
        src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_ball_simulation_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_simulation_tests PRIVATE src)
    add_test(NAME game_ball_simulation COMMAND nba97_game_ball_simulation_tests)
    add_executable(nba97_game_court_roster_startup_tests tests/game_court_roster_startup_tests.cpp
        src/recovered/game_court_roster_startup.c)
    target_compile_features(nba97_game_court_roster_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_roster_startup_tests PRIVATE src)
    add_test(NAME game_court_roster_startup COMMAND nba97_game_court_roster_startup_tests)
    add_executable(nba97_game_court_interactive_tests tests/game_court_interactive_tests.cpp
        src/recovered/game_court_interactive.c src/recovered/game_font_loader.c)
    target_compile_features(nba97_game_court_interactive_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_interactive_tests PRIVATE src)
    add_test(NAME game_court_interactive COMMAND nba97_game_court_interactive_tests)
    add_executable(nba97_game_court_packet_startup_tests tests/game_court_packet_startup_tests.cpp
        src/recovered/game_court_packet_startup.c)
    target_compile_features(nba97_game_court_packet_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_packet_startup_tests PRIVATE src)
    add_test(NAME game_court_packet_startup COMMAND nba97_game_court_packet_startup_tests)
    add_executable(nba97_game_court_frame_compose_tests tests/game_court_frame_compose_tests.cpp
        src/recovered/game_court_packets.c src/recovered/game_net.c src/game_court_frame_compose.cpp src/game_net.cpp
        src/game_player_projection.cpp src/game_player_root.cpp src/game_player_geometry.cpp
        src/game_court_geometry.cpp src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_court_frame_compose_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_frame_compose_tests PRIVATE src)
    add_test(NAME game_court_frame_compose COMMAND nba97_game_court_frame_compose_tests)
    add_executable(nba97_game_net_transform_tests tests/game_net_transform_tests.cpp
        src/recovered/game_net_transform.c)
    target_compile_features(nba97_game_net_transform_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_net_transform_tests PRIVATE src)
    add_test(NAME game_net_transform COMMAND nba97_game_net_transform_tests)
    add_executable(nba97_game_page_offset_tests tests/game_page_offset_tests.cpp
        src/recovered/game_page_offset.c)
    target_compile_features(nba97_game_page_offset_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_page_offset_tests PRIVATE src)
    add_test(NAME game_page_offset COMMAND nba97_game_page_offset_tests)
    add_executable(nba97_game_gpu_sync_tests tests/game_gpu_sync_tests.cpp
        src/recovered/game_gpu_sync.c)
    target_compile_features(nba97_game_gpu_sync_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gpu_sync_tests PRIVATE src)
    add_test(NAME game_gpu_sync COMMAND nba97_game_gpu_sync_tests)
    add_executable(nba97_game_display_mask_set_tests
        tests/game_display_mask_set_tests.cpp
        src/recovered/game_display_mask_set.c)
    target_compile_features(nba97_game_display_mask_set_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_display_mask_set_tests PRIVATE src)
    add_test(NAME game_display_mask_set COMMAND nba97_game_display_mask_set_tests)
    add_executable(nba97_game_resource_validator_install_tests
        tests/game_resource_validator_install_tests.cpp
        src/recovered/game_resource_validator_install.c)
    target_compile_features(nba97_game_resource_validator_install_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_resource_validator_install_tests
        PRIVATE src)
    add_test(NAME game_resource_validator_install
        COMMAND nba97_game_resource_validator_install_tests)
    add_executable(nba97_game_frame_rate_reset_tests
        tests/game_frame_rate_reset_tests.cpp
        src/recovered/game_frame_rate_reset.c)
    target_compile_features(nba97_game_frame_rate_reset_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_frame_rate_reset_tests PRIVATE src)
    add_test(NAME game_frame_rate_reset
        COMMAND nba97_game_frame_rate_reset_tests)
    add_executable(nba97_game_match_session_tests
        tests/game_match_session_tests.cpp
        src/recovered/game_match_session.c)
    target_compile_features(nba97_game_match_session_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_session_tests PRIVATE src)
    add_test(NAME game_match_session COMMAND nba97_game_match_session_tests)
    add_executable(nba97_game_loading_screen_tests
        tests/game_loading_screen_tests.cpp
        src/recovered/game_loading_screen.c)
    target_compile_features(nba97_game_loading_screen_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_loading_screen_tests PRIVATE src)
    add_test(NAME game_loading_screen COMMAND nba97_game_loading_screen_tests)
    add_executable(nba97_game_resource_loader_tests
        tests/game_resource_loader_tests.cpp
        src/recovered/game_resource_loader.c)
    target_compile_features(nba97_game_resource_loader_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_resource_loader_tests PRIVATE src)
    add_test(NAME game_resource_loader COMMAND nba97_game_resource_loader_tests)
    add_executable(nba97_game_heap_payload_size_tests
        tests/game_heap_payload_size_tests.cpp
        src/recovered/game_heap_payload_size.c
        src/recovered/game_heap_release.c)
    target_compile_features(nba97_game_heap_payload_size_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_heap_payload_size_tests PRIVATE src)
    add_test(NAME game_heap_payload_size COMMAND nba97_game_heap_payload_size_tests)
    add_executable(nba97_game_cd_sync_tests
        tests/game_cd_sync_tests.cpp
        src/recovered/game_cd_sync.c)
    target_compile_features(nba97_game_cd_sync_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_cd_sync_tests PRIVATE src)
    add_test(NAME game_cd_sync COMMAND nba97_game_cd_sync_tests)
    add_executable(nba97_game_cd_ready_callback_tests
        tests/game_cd_ready_callback_tests.cpp
        src/recovered/game_cd_ready_callback.c)
    target_compile_features(nba97_game_cd_ready_callback_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_cd_ready_callback_tests PRIVATE src)
    add_test(NAME game_cd_ready_callback
        COMMAND nba97_game_cd_ready_callback_tests)
    add_executable(nba97_game_cd_sync_callback_tests
        tests/game_cd_sync_callback_tests.cpp
        src/recovered/game_cd_sync_callback.c)
    target_compile_features(nba97_game_cd_sync_callback_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_cd_sync_callback_tests PRIVATE src)
    add_test(NAME game_cd_sync_callback
        COMMAND nba97_game_cd_sync_callback_tests)
    add_executable(nba97_game_vblank_shutdown_tests
        tests/game_vblank_shutdown_tests.cpp
        src/recovered/game_vblank_shutdown.c)
    target_compile_features(nba97_game_vblank_shutdown_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_vblank_shutdown_tests PRIVATE src)
    add_test(NAME game_vblank_shutdown
        COMMAND nba97_game_vblank_shutdown_tests)
    add_executable(nba97_game_clock_shutdown_tests
        tests/game_clock_shutdown_tests.cpp
        src/recovered/game_clock_shutdown.c)
    target_compile_features(nba97_game_clock_shutdown_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clock_shutdown_tests PRIVATE src)
    add_test(NAME game_clock_shutdown
        COMMAND nba97_game_clock_shutdown_tests)
    add_executable(nba97_game_controller_suspend_tests
        tests/game_controller_suspend_tests.cpp
        src/recovered/game_controller_suspend.c)
    target_compile_features(nba97_game_controller_suspend_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controller_suspend_tests PRIVATE src)
    add_test(NAME game_controller_suspend
        COMMAND nba97_game_controller_suspend_tests)
    add_executable(nba97_game_memory_zero_tests
        tests/game_memory_zero_tests.cpp
        src/recovered/game_memory_zero.c)
    target_compile_features(nba97_game_memory_zero_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_memory_zero_tests PRIVATE src)
    add_test(NAME game_memory_zero COMMAND nba97_game_memory_zero_tests)
    add_executable(nba97_game_memory_copy_tests
        tests/game_memory_copy_tests.cpp
        src/recovered/game_memory_copy.c)
    target_compile_features(nba97_game_memory_copy_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_memory_copy_tests PRIVATE src)
    add_test(NAME game_memory_copy COMMAND nba97_game_memory_copy_tests)
    add_executable(nba97_game_ball_scoring_tests tests/game_ball_scoring_tests.cpp
        src/recovered/game_ball_scoring.c src/recovered/game_controller_selection.c)
    target_compile_features(nba97_game_ball_scoring_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_scoring_tests PRIVATE src)
    add_test(NAME game_ball_scoring COMMAND nba97_game_ball_scoring_tests)
    add_executable(nba97_game_scoring_actor_ai_tests tests/game_scoring_actor_ai_tests.cpp
        src/recovered/game_scoring_actor_ai.c)
    target_compile_features(nba97_game_scoring_actor_ai_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_scoring_actor_ai_tests PRIVATE src)
    add_test(NAME game_scoring_actor_ai COMMAND nba97_game_scoring_actor_ai_tests)
    add_executable(nba97_game_court_startup_sequence_tests tests/game_court_startup_sequence_tests.cpp
        src/recovered/game_court_startup_sequence.c src/recovered/game_court_roster_startup.c
        src/recovered/game_court_interactive.c src/recovered/game_court_packet_startup.c
        src/recovered/game_court_startup.c src/recovered/game_court_textures.c
        src/recovered/game_court_resources.c src/recovered/game_font_loader.c
        src/recovered/game_image_upload.c src/recovered/game_text_pools.c)
    target_compile_features(nba97_game_court_startup_sequence_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_startup_sequence_tests PRIVATE src)
    add_test(NAME game_court_startup_sequence COMMAND nba97_game_court_startup_sequence_tests)
    add_executable(nba97_game_match_frame_tests tests/game_match_frame_tests.cpp
        src/recovered/game_match_frame.c)
    target_compile_features(nba97_game_match_frame_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_frame_tests PRIVATE src)
    add_test(NAME game_match_frame COMMAND nba97_game_match_frame_tests)
    add_executable(nba97_game_match_tick_tests tests/game_match_tick_tests.cpp
        src/recovered/game_match_tick.c)
    target_compile_features(nba97_game_match_tick_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_tick_tests PRIVATE src)
    add_test(NAME game_match_tick COMMAND nba97_game_match_tick_tests)
    add_executable(nba97_game_match_frame_backend_tests tests/game_match_frame_backend_tests.cpp
        src/recovered/game_match_frame.c src/game_match_frame.cpp
        src/recovered/game_frame_interrupt_disable.c src/game_frame_interrupt_disable_adapter.cpp
        src/recovered/game_frame_interrupt_restore.c src/game_frame_interrupt_restore_adapter.cpp
        src/recovered/game_net.c src/game_net.cpp
        src/recovered/game_ball_attachment.c src/game_ball_attachment.cpp
        src/recovered/game_ball_frame.c src/game_ball_frame.cpp
        src/recovered/game_player_frame.c src/game_player_frame.cpp
        src/recovered/game_pose_frame.c src/recovered/game_pose_sample.c
        src/recovered/game_player_label_frame.c
        src/recovered/game_player_marker_update.c src/game_player_marker_update.cpp
        src/recovered/game_player_root.c src/game_player_root.cpp
        src/recovered/game_player_geometry.c src/game_player_geometry.cpp
        src/recovered/game_player_projection.c src/game_player_projection.cpp
        src/recovered/game_court_packets.c src/game_court_geometry.cpp src/game_court_frame_compose.cpp
        src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_match_frame_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_match_frame_backend_tests PRIVATE src)
    add_test(NAME game_match_frame_backend COMMAND nba97_game_match_frame_backend_tests)
    add_executable(nba97_game_net_tests tests/game_net_tests.cpp
        src/recovered/game_net.c src/game_net.cpp src/recovered/game_court_packets.c
        src/game_player_projection.cpp src/game_player_root.cpp src/game_player_geometry.cpp
        src/game_court_geometry.cpp src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_net_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_net_tests PRIVATE src)
    add_test(NAME game_net COMMAND nba97_game_net_tests)
    add_executable(nba97_game_ball_attachment_tests tests/game_ball_attachment_tests.cpp
        src/recovered/game_ball_attachment.c src/game_ball_attachment.cpp
        src/recovered/game_player_frame.c src/game_player_frame.cpp
        src/recovered/game_player_root.c src/game_player_root.cpp
        src/recovered/game_player_geometry.c src/game_player_geometry.cpp
        src/recovered/game_player_projection.c src/game_player_projection.cpp
        src/recovered/game_court_packets.c src/game_court_geometry.cpp
        src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_ball_attachment_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_attachment_tests PRIVATE src)
    add_test(NAME game_ball_attachment COMMAND nba97_game_ball_attachment_tests)
    add_executable(nba97_game_court_startup_tests tests/game_court_startup_tests.cpp
        src/recovered/game_court_startup.c)
    target_compile_features(nba97_game_court_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_startup_tests PRIVATE src)
    add_test(NAME game_court_startup COMMAND nba97_game_court_startup_tests)
    add_executable(nba97_game_court_startup_services_tests tests/game_court_startup_services_tests.cpp
        src/recovered/game_court_startup.c src/recovered/game_court_startup_services.c
        src/recovered/game_heap_release.c)
    target_compile_features(nba97_game_court_startup_services_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_startup_services_tests PRIVATE src)
    add_test(NAME game_court_startup_services COMMAND nba97_game_court_startup_services_tests)
    add_executable(nba97_game_player_marker_resources_tests tests/game_player_marker_resources_tests.cpp
        src/recovered/game_player_marker_resources.c)
    target_compile_features(nba97_game_player_marker_resources_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_marker_resources_tests PRIVATE src)
    add_test(NAME game_player_marker_resources COMMAND nba97_game_player_marker_resources_tests)
    add_executable(nba97_game_ball_frame_tests tests/game_ball_frame_tests.cpp
        src/recovered/game_ball_frame.c src/game_ball_frame.cpp
        src/recovered/game_player_frame.c src/game_player_frame.cpp
        src/recovered/game_player_root.c src/game_player_root.cpp
        src/recovered/game_player_geometry.c src/game_player_geometry.cpp
        src/recovered/game_player_projection.c src/game_player_projection.cpp
        src/recovered/game_court_packets.c src/game_court_geometry.cpp
        src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_ball_frame_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_frame_tests PRIVATE src)
    add_test(NAME game_ball_frame COMMAND nba97_game_ball_frame_tests)
    add_executable(nba97_game_camera_tests tests/game_camera_tests.cpp
        src/recovered/game_camera.c src/game_player_root.cpp src/game_player_geometry.cpp
        src/game_court_geometry.cpp src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_camera_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_camera_tests PRIVATE src)
    add_test(NAME game_camera COMMAND nba97_game_camera_tests)
    add_executable(nba97_game_player_frame_tests tests/game_player_frame_tests.cpp
        src/recovered/game_player_frame.c src/game_player_frame.cpp
        src/recovered/game_player_root.c src/game_player_root.cpp
        src/recovered/game_player_geometry.c src/game_player_geometry.cpp
        src/recovered/game_player_projection.c src/game_player_projection.cpp
        src/recovered/game_court_packets.c src/game_court_geometry.cpp
        src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_player_frame_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_frame_tests PRIVATE src)
    add_test(NAME game_player_frame COMMAND nba97_game_player_frame_tests)
    add_executable(nba97_game_court_resources_tests tests/game_court_resources_tests.cpp
        src/recovered/game_court_resources.c src/recovered/game_text_pools.c src/recovered/game_heap_allocate.c)
    target_compile_features(nba97_game_court_resources_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_resources_tests PRIVATE src)
    add_test(NAME game_court_resources COMMAND nba97_game_court_resources_tests)
    add_executable(nba97_game_player_projection_tests tests/game_player_projection_tests.cpp
        src/recovered/game_player_projection.c src/game_player_projection.cpp src/game_player_root.cpp
        src/game_player_geometry.cpp src/game_court_geometry.cpp src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_player_projection_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_projection_tests PRIVATE src)
    add_test(NAME game_player_projection COMMAND nba97_game_player_projection_tests)
    add_executable(nba97_game_player_root_tests tests/game_player_root_tests.cpp
        src/recovered/game_player_root.c src/game_player_root.cpp src/game_player_geometry.cpp
        src/game_court_geometry.cpp src/zdomf_projection.cpp src/zdomf_transform.cpp)
    target_compile_features(nba97_game_player_root_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_root_tests PRIVATE src)
    add_test(NAME game_player_root COMMAND nba97_game_player_root_tests)
    add_executable(nba97_game_court_textures_tests tests/game_court_textures_tests.cpp
        src/recovered/game_court_textures.c src/recovered/game_image_upload.c src/game_render_backend.cpp)
    target_compile_features(nba97_game_court_textures_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_textures_tests PRIVATE src)
    add_test(NAME game_court_textures COMMAND nba97_game_court_textures_tests)
    add_executable(nba97_game_player_geometry_tests tests/game_player_geometry_tests.cpp
        src/recovered/game_player_geometry.c src/game_player_geometry.cpp)
    target_compile_features(nba97_game_player_geometry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_geometry_tests PRIVATE src)
    add_test(NAME game_player_geometry COMMAND nba97_game_player_geometry_tests)
    add_executable(nba97_game_packet_renderer_tests tests/game_packet_renderer_tests.cpp
        src/game_packet_renderer.cpp src/game_render_backend.cpp src/recovered/game_image_upload.c)
    target_compile_features(nba97_game_packet_renderer_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_packet_renderer_tests PRIVATE src)
    add_test(NAME game_packet_renderer COMMAND nba97_game_packet_renderer_tests)
    add_executable(nba97_game_court_packets_tests tests/game_court_packets_tests.cpp
        src/recovered/game_court_packets.c src/game_court_geometry.cpp src/zdomf_projection.cpp src/zdomf_transform.cpp
        src/game_packet_renderer.cpp src/game_render_backend.cpp src/recovered/game_image_upload.c)
    target_compile_features(nba97_game_court_packets_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_court_packets_tests PRIVATE src)
    add_test(NAME game_court_packets COMMAND nba97_game_court_packets_tests)
    add_executable(nba97_game_tipoff_phase_tests tests/game_tipoff_phase_tests.cpp src/recovered/game_tipoff_phase.c)
    target_compile_features(nba97_game_tipoff_phase_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_tipoff_phase_tests PRIVATE src)
    add_test(NAME game_tipoff_phase COMMAND nba97_game_tipoff_phase_tests)
    add_executable(nba97_game_ball_release_tests tests/game_ball_release_tests.cpp src/recovered/game_ball_release.c)
    target_compile_features(nba97_game_ball_release_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_ball_release_tests PRIVATE src)
    add_test(NAME game_ball_release COMMAND nba97_game_ball_release_tests)
    add_executable(nba97_game_body_names_tests tests/game_body_names_tests.cpp src/recovered/game_body_names.c)
    target_compile_features(nba97_game_body_names_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_body_names_tests PRIVATE src)
    add_test(NAME game_body_names COMMAND nba97_game_body_names_tests)
    add_executable(nba97_game_body_names_backend_tests tests/game_body_names_backend_tests.cpp src/game_body_names.cpp
        src/game_body_resources.cpp src/game_render_backend.cpp src/recovered/game_body_geometry.c
        src/recovered/game_body_names.c src/recovered/game_render_textures.c src/recovered/game_image_upload.c)
    target_compile_features(nba97_game_body_names_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_body_names_backend_tests PRIVATE src)
    add_test(NAME game_body_names_backend COMMAND nba97_game_body_names_backend_tests)
    add_executable(nba97_game_font_loader_tests tests/game_font_loader_tests.cpp src/recovered/game_font_loader.c)
    target_compile_features(nba97_game_font_loader_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_font_loader_tests PRIVATE src)
    add_test(NAME game_font_loader COMMAND nba97_game_font_loader_tests)
    add_executable(nba97_game_text_pools_tests tests/game_text_pools_tests.cpp src/recovered/game_text_pools.c)
    target_compile_features(nba97_game_text_pools_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_text_pools_tests PRIVATE src)
    add_test(NAME game_text_pools COMMAND nba97_game_text_pools_tests)
    add_executable(nba97_game_heap_release_tests tests/game_heap_release_tests.cpp src/recovered/game_heap_release.c)
    target_compile_features(nba97_game_heap_release_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_heap_release_tests PRIVATE src)
    add_test(NAME game_heap_release COMMAND nba97_game_heap_release_tests)
    add_executable(nba97_game_heap_initialize_tests tests/game_heap_initialize_tests.cpp src/recovered/game_heap_initialize.c)
    target_compile_features(nba97_game_heap_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_heap_initialize_tests PRIVATE src)
    add_test(NAME game_heap_initialize COMMAND nba97_game_heap_initialize_tests)
    add_executable(nba97_game_overlay_entry_tests tests/game_overlay_entry_tests.cpp src/recovered/game_overlay_entry.c)
    target_compile_features(nba97_game_overlay_entry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_overlay_entry_tests PRIVATE src)
    add_test(NAME game_overlay_entry COMMAND nba97_game_overlay_entry_tests)
    add_executable(nba97_game_main_tests tests/game_main_tests.cpp src/recovered/game_main.c
        src/recovered/game_overlay_entry.c src/recovered/game_static_initializers.c
        src/recovered/game_global_pointer_save.c src/recovered/game_heap_initialize.c
        src/recovered/game_cd_directory_initialize.c src/recovered/game_path_prefix_set.c
        src/recovered/game_directory_cache_configure.c
        src/recovered/game_interrupt_mask_set.c
        src/recovered/game_reset_callback.c
        src/recovered/game_controller_resume.c
        src/recovered/game_reset_graph.c
        src/recovered/game_graph_debug_set.c
        src/recovered/game_vblank_initialize.c
        src/recovered/game_clock_initialize.c
        src/recovered/game_gte_initialize.c
        src/recovered/game_clock_delta.c
        src/recovered/game_presentation_wait.c
        src/recovered/game_video_environment_initialize.c
        src/recovered/game_move_image.c
        src/recovered/game_gpu_sync.c
        src/recovered/game_display_mask_set.c
        src/recovered/game_resource_validator_install.c
        src/recovered/game_frame_rate_reset.c
        src/recovered/game_match_session.c
        src/recovered/game_loading_screen.c
        src/recovered/game_resource_loader.c
        src/recovered/game_heap_payload_size.c
        src/recovered/game_cd_sync.c
        src/recovered/game_cd_ready_callback.c
        src/recovered/game_cd_sync_callback.c
        src/recovered/game_vblank_shutdown.c
        src/recovered/game_clock_shutdown.c
        src/recovered/game_controller_suspend.c
        src/recovered/game_memory_zero.c
        src/recovered/game_memory_copy.c
        src/recovered/game_heap_release.c)
    target_compile_features(nba97_game_main_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_main_tests PRIVATE src)
    add_test(NAME game_main COMMAND nba97_game_main_tests)
    add_executable(nba97_game_static_initializers_tests tests/game_static_initializers_tests.cpp
        src/recovered/game_static_initializers.c)
    target_compile_features(nba97_game_static_initializers_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_static_initializers_tests PRIVATE src)
    add_test(NAME game_static_initializers COMMAND nba97_game_static_initializers_tests)
    add_executable(nba97_game_global_pointer_save_tests tests/game_global_pointer_save_tests.cpp
        src/recovered/game_global_pointer_save.c)
    target_compile_features(nba97_game_global_pointer_save_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_global_pointer_save_tests PRIVATE src)
    add_test(NAME game_global_pointer_save COMMAND nba97_game_global_pointer_save_tests)
    add_executable(nba97_game_cd_directory_initialize_tests
        tests/game_cd_directory_initialize_tests.cpp
        src/recovered/game_cd_directory_initialize.c)
    target_compile_features(nba97_game_cd_directory_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_cd_directory_initialize_tests PRIVATE src)
    add_test(NAME game_cd_directory_initialize COMMAND nba97_game_cd_directory_initialize_tests)
    add_executable(nba97_game_path_prefix_set_tests
        tests/game_path_prefix_set_tests.cpp
        src/recovered/game_path_prefix_set.c)
    target_compile_features(nba97_game_path_prefix_set_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_path_prefix_set_tests PRIVATE src)
    add_test(NAME game_path_prefix_set COMMAND nba97_game_path_prefix_set_tests)
    add_executable(nba97_game_directory_cache_configure_tests
        tests/game_directory_cache_configure_tests.cpp
        src/recovered/game_directory_cache_configure.c)
    target_compile_features(nba97_game_directory_cache_configure_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_directory_cache_configure_tests PRIVATE src)
    add_test(NAME game_directory_cache_configure
        COMMAND nba97_game_directory_cache_configure_tests)
    add_executable(nba97_game_interrupt_mask_set_tests
        tests/game_interrupt_mask_set_tests.cpp
        src/recovered/game_interrupt_mask_set.c)
    target_compile_features(nba97_game_interrupt_mask_set_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_interrupt_mask_set_tests PRIVATE src)
    add_test(NAME game_interrupt_mask_set
        COMMAND nba97_game_interrupt_mask_set_tests)
    add_executable(nba97_game_reset_callback_tests
        tests/game_reset_callback_tests.cpp
        src/recovered/game_reset_callback.c)
    target_compile_features(nba97_game_reset_callback_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_reset_callback_tests PRIVATE src)
    add_test(NAME game_reset_callback
        COMMAND nba97_game_reset_callback_tests)
    add_executable(nba97_game_controller_resume_tests
        tests/game_controller_resume_tests.cpp
        src/recovered/game_controller_resume.c)
    target_compile_features(nba97_game_controller_resume_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_controller_resume_tests PRIVATE src)
    add_test(NAME game_controller_resume
        COMMAND nba97_game_controller_resume_tests)
    add_executable(nba97_game_reset_graph_tests
        tests/game_reset_graph_tests.cpp
        src/recovered/game_reset_graph.c)
    target_compile_features(nba97_game_reset_graph_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_reset_graph_tests PRIVATE src)
    add_test(NAME game_reset_graph
        COMMAND nba97_game_reset_graph_tests)
    add_executable(nba97_game_graph_debug_set_tests
        tests/game_graph_debug_set_tests.cpp
        src/recovered/game_graph_debug_set.c)
    target_compile_features(nba97_game_graph_debug_set_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_graph_debug_set_tests PRIVATE src)
    add_test(NAME game_graph_debug_set
        COMMAND nba97_game_graph_debug_set_tests)
    add_executable(nba97_game_vblank_initialize_tests
        tests/game_vblank_initialize_tests.cpp
        src/recovered/game_vblank_initialize.c)
    target_compile_features(nba97_game_vblank_initialize_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_vblank_initialize_tests PRIVATE src)
    add_test(NAME game_vblank_initialize
        COMMAND nba97_game_vblank_initialize_tests)
    add_executable(nba97_game_clock_initialize_tests
        tests/game_clock_initialize_tests.cpp
        src/recovered/game_clock_initialize.c)
    target_compile_features(nba97_game_clock_initialize_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clock_initialize_tests PRIVATE src)
    add_test(NAME game_clock_initialize
        COMMAND nba97_game_clock_initialize_tests)
    add_executable(nba97_game_gte_initialize_tests
        tests/game_gte_initialize_tests.cpp
        src/recovered/game_gte_initialize.c)
    target_compile_features(nba97_game_gte_initialize_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gte_initialize_tests PRIVATE src)
    add_test(NAME game_gte_initialize
        COMMAND nba97_game_gte_initialize_tests)
    add_executable(nba97_game_clock_delta_tests
        tests/game_clock_delta_tests.cpp
        src/recovered/game_clock_delta.c)
    target_compile_features(nba97_game_clock_delta_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_clock_delta_tests PRIVATE src)
    add_test(NAME game_clock_delta
        COMMAND nba97_game_clock_delta_tests)
    add_executable(nba97_game_presentation_wait_tests
        tests/game_presentation_wait_tests.cpp
        src/recovered/game_presentation_wait.c)
    target_compile_features(nba97_game_presentation_wait_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_presentation_wait_tests PRIVATE src)
    add_test(NAME game_presentation_wait
        COMMAND nba97_game_presentation_wait_tests)
    add_executable(nba97_game_video_environment_initialize_tests
        tests/game_video_environment_initialize_tests.cpp
        src/recovered/game_video_environment_initialize.c)
    target_compile_features(nba97_game_video_environment_initialize_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_video_environment_initialize_tests
        PRIVATE src)
    add_test(NAME game_video_environment_initialize
        COMMAND nba97_game_video_environment_initialize_tests)
    add_executable(nba97_game_move_image_tests
        tests/game_move_image_tests.cpp
        src/recovered/game_move_image.c)
    target_compile_features(nba97_game_move_image_tests
        PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_move_image_tests PRIVATE src)
    add_test(NAME game_move_image COMMAND nba97_game_move_image_tests)
    add_executable(nba97_game_heap_allocate_tests tests/game_heap_allocate_tests.cpp src/recovered/game_heap_allocate.c)
    target_compile_features(nba97_game_heap_allocate_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_heap_allocate_tests PRIVATE src)
    add_test(NAME game_heap_allocate COMMAND nba97_game_heap_allocate_tests)
    add_executable(nba97_game_body_geometry_tests tests/game_body_geometry_tests.cpp src/recovered/game_body_geometry.c)
    target_compile_features(nba97_game_body_geometry_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_body_geometry_tests PRIVATE src)
    add_test(NAME game_body_geometry COMMAND nba97_game_body_geometry_tests)
    add_executable(nba97_game_body_resources_tests tests/game_body_resources_tests.cpp src/game_body_resources.cpp
        src/game_render_backend.cpp src/recovered/game_body_geometry.c src/recovered/game_image_upload.c)
    target_compile_features(nba97_game_body_resources_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_body_resources_tests PRIVATE src)
    add_test(NAME game_body_resources COMMAND nba97_game_body_resources_tests)
    add_executable(nba97_voice_patl_upload_tests tests/voice_patl_upload_tests.cpp
        src/recovered/voice_patl_upload.c src/recovered/voice_programs.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_voice_patl_upload_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_voice_patl_upload_tests PRIVATE src)
    add_test(NAME voice_patl_upload COMMAND nba97_voice_patl_upload_tests)
    add_executable(nba97_audio_startup_tests tests/audio_startup_tests.cpp
        src/recovered/audio_startup.c src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_audio_startup_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_audio_startup_tests PRIVATE src)
    add_test(NAME audio_startup COMMAND nba97_audio_startup_tests)
    add_executable(nba97_audio_startup_backend_tests tests/audio_startup_backend_tests.cpp
        src/audio_startup_backend.cpp src/recovered/audio_startup.c
        src/spu_initialize_backend.cpp src/interrupt_controller_backend.cpp src/spu_event_backend.cpp src/spu_sample_backend.cpp
        src/recovered/spu_initialize.c src/recovered/interrupt_controller.c src/recovered/spu_events.c
        src/recovered/spu_transfer_mapping.c src/recovered/spu_transfer.c src/recovered/spu_heap.c
        src/recovered/spu_heap_mapping.c src/recovered/voice_mapping.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_audio_startup_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_audio_startup_backend_tests PRIVATE src)
    add_test(NAME audio_startup_backend COMMAND nba97_audio_startup_backend_tests)
    add_executable(nba97_spu_initialize_tests tests/spu_initialize_tests.cpp
        src/recovered/spu_initialize.c src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_initialize_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_initialize_tests PRIVATE src)
    add_test(NAME spu_initialize COMMAND nba97_spu_initialize_tests)
    add_executable(nba97_spu_initialize_backend_tests tests/spu_initialize_backend_tests.cpp
        src/spu_initialize_backend.cpp src/interrupt_controller_backend.cpp src/spu_event_backend.cpp src/spu_sample_backend.cpp
        src/recovered/spu_initialize.c src/recovered/interrupt_controller.c src/recovered/spu_events.c
        src/recovered/spu_transfer_mapping.c src/recovered/spu_transfer.c src/recovered/spu_heap.c
        src/recovered/spu_heap_mapping.c src/recovered/voice_mapping.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_initialize_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_initialize_backend_tests PRIVATE src)
    add_test(NAME spu_initialize_backend COMMAND nba97_spu_initialize_backend_tests)
    add_executable(nba97_interrupt_controller_tests tests/interrupt_controller_tests.cpp
        src/recovered/interrupt_controller.c src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_interrupt_controller_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_interrupt_controller_tests PRIVATE src)
    add_test(NAME interrupt_controller COMMAND nba97_interrupt_controller_tests)
    add_executable(nba97_interrupt_controller_backend_tests tests/interrupt_controller_backend_tests.cpp
        src/interrupt_controller_backend.cpp src/spu_event_backend.cpp src/spu_sample_backend.cpp
        src/recovered/interrupt_controller.c src/recovered/spu_events.c src/recovered/spu_transfer.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_interrupt_controller_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_interrupt_controller_backend_tests PRIVATE src)
    add_test(NAME interrupt_controller_backend COMMAND nba97_interrupt_controller_backend_tests)
    add_executable(nba97_spu_event_backend_tests tests/spu_event_backend_tests.cpp
        src/spu_event_backend.cpp src/spu_sample_backend.cpp src/recovered/spu_events.c
        src/recovered/spu_transfer.c src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_event_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_event_backend_tests PRIVATE src)
    add_test(NAME spu_event_backend COMMAND nba97_spu_event_backend_tests)
    add_executable(nba97_spu_events_tests tests/spu_events_tests.cpp src/recovered/spu_events.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_events_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_events_tests PRIVATE src)
    add_test(NAME spu_events COMMAND nba97_spu_events_tests)
    add_executable(nba97_spu_sample_backend_tests tests/spu_sample_backend_tests.cpp src/spu_sample_backend.cpp
        src/recovered/spu_transfer.c src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_sample_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_sample_backend_tests PRIVATE src)
    add_test(NAME spu_sample_backend COMMAND nba97_spu_sample_backend_tests)
    add_executable(nba97_spu_transfer_mapping_tests tests/spu_transfer_mapping_tests.cpp src/spu_sample_backend.cpp src/spu_event_backend.cpp
        src/recovered/spu_initialize.c src/spu_initialize_backend.cpp
        src/recovered/spu_events.c src/recovered/interrupt_controller.c src/interrupt_controller_backend.cpp
        src/recovered/spu_transfer_mapping.c src/recovered/spu_transfer.c
        src/recovered/spu_heap_mapping.c src/recovered/spu_heap.c src/recovered/voice_mapping.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_transfer_mapping_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_transfer_mapping_tests PRIVATE src)
    add_test(NAME spu_transfer_mapping COMMAND nba97_spu_transfer_mapping_tests)
    add_executable(nba97_spu_transfer_tests tests/spu_transfer_tests.cpp src/recovered/spu_transfer.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_transfer_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_transfer_tests PRIVATE src)
    add_test(NAME spu_transfer COMMAND nba97_spu_transfer_tests)
    add_executable(nba97_spu_heap_mapping_tests tests/spu_heap_mapping_tests.cpp
        src/recovered/spu_heap_mapping.c src/recovered/spu_heap.c src/recovered/voice_mapping.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_heap_mapping_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_heap_mapping_tests PRIVATE src)
    add_test(NAME spu_heap_mapping COMMAND nba97_spu_heap_mapping_tests)
    add_executable(nba97_spu_heap_tests tests/spu_heap_tests.cpp src/recovered/spu_heap.c
        src/recovered/voice_patl_upload.c src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_spu_heap_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_spu_heap_tests PRIVATE src)
    add_test(NAME spu_heap COMMAND nba97_spu_heap_tests)
    add_executable(nba97_voice_mapping_tests tests/voice_mapping_tests.cpp
        src/recovered/voice_mapping.c src/recovered/voice_mapping_table.c
        src/recovered/voice_patl_upload.c src/recovered/voice_programs.c)
    target_compile_features(nba97_voice_mapping_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_voice_mapping_tests PRIVATE src)
    add_test(NAME voice_mapping COMMAND nba97_voice_mapping_tests)
    add_executable(nba97_game_player_label_backend_tests tests/game_player_label_backend_tests.cpp
        src/game_player_label_backend.cpp src/game_render_backend.cpp src/recovered/game_player_labels.c
        src/recovered/game_text_objects.c src/recovered/game_image_upload.c)
    target_compile_features(nba97_game_player_label_backend_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_label_backend_tests PRIVATE src)
    add_test(NAME game_player_label_backend COMMAND nba97_game_player_label_backend_tests)
    add_executable(nba97_game_text_objects_tests tests/game_text_objects_tests.cpp src/recovered/game_text_objects.c)
    target_compile_features(nba97_game_text_objects_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_text_objects_tests PRIVATE src)
    add_test(NAME game_text_objects COMMAND nba97_game_text_objects_tests)
    add_executable(nba97_voice_allocation_tests tests/voice_allocation_tests.cpp src/recovered/voice_allocation.c
        src/recovered/voice_handles.c src/recovered/music_voice.c)
    target_compile_features(nba97_voice_allocation_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_voice_allocation_tests PRIVATE src)
    add_test(NAME voice_allocation COMMAND nba97_voice_allocation_tests)
    add_executable(nba97_voice_programs_tests tests/voice_programs_tests.cpp src/recovered/voice_programs.c
        src/recovered/voice_mapping_table.c)
    target_compile_features(nba97_voice_programs_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_voice_programs_tests PRIVATE src)
    add_test(NAME voice_programs COMMAND nba97_voice_programs_tests)
    add_executable(nba97_game_gameplay_audio_tests tests/game_gameplay_audio_tests.cpp
        src/recovered/game_gameplay_audio.c)
    target_compile_features(nba97_game_gameplay_audio_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_gameplay_audio_tests PRIVATE src)
    add_test(NAME game_gameplay_audio COMMAND nba97_game_gameplay_audio_tests)
    add_executable(nba97_match_input_edges_tests tests/match_input_edges_tests.cpp src/match_input_edges.cpp
        src/recovered/game_player_input.c)
    target_compile_features(nba97_match_input_edges_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_match_input_edges_tests PRIVATE src)
    add_test(NAME match_input_edges COMMAND nba97_match_input_edges_tests)
    add_executable(nba97_game_player_input_tests tests/game_player_input_tests.cpp src/recovered/game_player_input.c)
    target_compile_features(nba97_game_player_input_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_game_player_input_tests PRIVATE src)
    add_test(NAME game_player_input COMMAND nba97_game_player_input_tests)
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
    src/recovered/game_pose_frame.c
    src/gameplay_animation.cpp src/recovered/game_animation_advance.c src/recovered/game_animation_queue.c
    src/recovered/game_player_physics.c src/recovered/game_player_update.c src/match_player_update.cpp
    src/recovered/game_player_jump.c src/recovered/game_player_input.c src/match_input_edges.cpp
    src/recovered/game_tipoff_phase.c src/recovered/game_ball_release.c
    src/recovered/game_court_packets.c src/game_court_geometry.cpp
    src/recovered/game_player_geometry.c src/game_player_geometry.cpp src/game_packet_renderer.cpp
    src/recovered/game_player_root.c src/game_player_root.cpp src/recovered/game_court_textures.c
    src/recovered/game_player_projection.c src/game_player_projection.cpp src/recovered/game_court_resources.c
    src/recovered/game_camera.c src/recovered/game_player_frame.c src/game_player_frame.cpp
    src/recovered/game_ball_frame.c src/game_ball_frame.cpp src/recovered/game_player_marker_resources.c
    src/recovered/game_ball_attachment.c src/game_ball_attachment.cpp src/recovered/game_ball_simulation.c
    src/recovered/game_ball_scoring.c src/recovered/game_scoring_actor_ai.c
    src/recovered/game_net.c src/game_net.cpp src/recovered/game_net_transform.c src/recovered/game_page_offset.c
    src/recovered/game_gpu_sync.c
    src/recovered/game_court_startup.c src/recovered/game_court_startup_services.c src/recovered/game_court_roster_startup.c
    src/recovered/game_court_interactive.c src/recovered/game_court_packet_startup.c src/recovered/game_court_startup_sequence.c
    src/game_court_frame_compose.cpp
    src/recovered/game_match_frame.c src/game_match_frame.cpp src/recovered/game_match_tick.c
    src/recovered/game_player_label_frame.c src/recovered/game_player_marker_update.c src/game_player_marker_update.cpp
    src/recovered/voice_handles.c src/recovered/voice_channels.c
    src/recovered/voice_allocation.c src/recovered/voice_programs.c src/recovered/voice_patl_upload.c
    src/recovered/game_gameplay_audio.c
    src/recovered/voice_mapping.c src/recovered/voice_mapping_table.c src/recovered/spu_heap.c src/recovered/spu_heap_mapping.c
    src/recovered/spu_transfer.c src/recovered/spu_transfer_mapping.c src/spu_sample_backend.cpp src/recovered/spu_events.c src/spu_event_backend.cpp
    src/recovered/interrupt_controller.c src/interrupt_controller_backend.cpp
    src/recovered/spu_initialize.c src/spu_initialize_backend.cpp
    src/recovered/audio_startup.c src/audio_startup_backend.cpp
    src/recovered/game_text_objects.c src/recovered/game_text_pools.c src/recovered/game_font_loader.c src/game_player_label_backend.cpp
    src/recovered/game_overlay_entry.c src/recovered/game_main.c src/recovered/game_static_initializers.c src/recovered/game_global_pointer_save.c src/recovered/game_heap_initialize.c src/recovered/game_cd_directory_initialize.c src/recovered/game_path_prefix_set.c src/recovered/game_directory_cache_configure.c src/recovered/game_interrupt_mask_set.c src/recovered/game_reset_callback.c src/recovered/game_controller_resume.c src/recovered/game_reset_graph.c src/recovered/game_graph_debug_set.c src/recovered/game_vblank_initialize.c src/recovered/game_clock_initialize.c src/recovered/game_gte_initialize.c src/recovered/game_clock_delta.c src/recovered/game_presentation_wait.c src/recovered/game_video_environment_initialize.c src/recovered/game_move_image.c src/recovered/game_display_mask_set.c src/recovered/game_resource_validator_install.c src/recovered/game_frame_rate_reset.c src/recovered/game_match_session.c src/recovered/game_loading_screen.c src/recovered/game_resource_loader.c src/recovered/game_heap_payload_size.c src/recovered/game_cd_sync.c src/recovered/game_cd_ready_callback.c src/recovered/game_cd_sync_callback.c src/recovered/game_vblank_shutdown.c src/recovered/game_clock_shutdown.c src/recovered/game_controller_suspend.c src/recovered/game_memory_zero.c src/recovered/game_memory_copy.c src/recovered/game_heap_allocate.c src/recovered/game_heap_release.c
    src/recovered/game_substitution_support.c
    src/recovered/game_player_attributes.c src/music_playback.cpp src/recovered/music_transition.c src/recovered/frontend_resource.c
    src/recovered/frontend_resource_cleanup.c
    src/recovered/game_render_textures.c src/recovered/game_head_cache.c src/recovered/game_player_labels.c
    src/game_render_backend.cpp src/recovered/game_image_upload.c
    src/recovered/game_body_geometry.c src/game_body_resources.cpp src/recovered/game_body_names.c src/game_body_names.cpp
    src/recovered/game_player_initialization.c src/gameplay_setup.cpp src/recovered/music_voice.c
    src/recovered/game_team_roles.c
    src/recovered/gameplay_mocap.c src/gameplay_mocap.cpp src/recovered/music_routing.c)
target_sources(nba97_boot_decomp PRIVATE src/roster_reset_assets.cpp src/recovered/roster_reset.c)
target_sources(nba97_boot_decomp PRIVATE src/recovered/feload_entry.c src/feload_entry_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_scene_resources.c
    src/game_scene_resources_adapter.cpp src/game_scene_resources_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_speech_initialize.c src/game_speech_initialize_adapter.cpp src/game_speech_initialize_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_startup.c src/game_camera_startup_adapter.cpp src/game_camera_startup_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_random_seed.c src/game_random_seed_adapter.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_period_startup.c
    src/game_period_startup_adapter.cpp src/game_period_startup_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_hot_start.c
    src/game_match_hot_start_adapter.cpp src/game_match_hot_start_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_scene_startup.c
    src/game_scene_startup_adapter.cpp src/game_scene_startup_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_scene_random_warmup.c
    src/game_scene_random_warmup_adapter.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_loop_entry.c
    src/game_loop_entry_adapter.cpp src/game_loop_entry_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_audio_initialize.c
    src/game_audio_initialize_adapter.cpp src/game_audio_initialize_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_scene_load.c
    src/game_scene_load_adapter.cpp src/game_scene_load_capture.cpp)
target_sources(nba97_boot_decomp PRIVATE src/recovered/game_roster_bindings.c src/recovered/game_match_initialize.c
    src/game_match_initialize_adapter.cpp src/game_match_initialize_capture.cpp)
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

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_first_period_startup.c src/game_first_period_startup_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_speech_startup.c src/game_speech_startup_adapter.cpp src/game_speech_startup_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_select.c src/game_camera_select_adapter.cpp src/game_camera_select_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_late_period_limits.c src/game_late_period_limits_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_tipoff_announcement.c src/game_tipoff_announcement_adapter.cpp src/game_tipoff_announcement_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_controller_frame_reset.c src/game_controller_frame_reset_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/game_controller_frame_reset_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_audio_stream_pump.c src/game_audio_stream_pump_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/game_audio_stream_pump_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_clocks.c src/game_match_clocks_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/game_match_clocks_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_audio_stream_status.c src/game_audio_stream_status_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_clock_violations.c src/game_clock_violations_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/game_clock_violations_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_audio_stream_service.c src/game_audio_stream_service_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_period_expiry.c src/game_period_expiry_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/game_period_expiry_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_service_publish.c src/game_match_service_publish_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/game_match_service_publish_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_clock_read.c src/game_clock_read_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_audio_service.c src/game_match_audio_service_adapter.cpp src/game_match_audio_service_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_stream_readiness.c src/game_stream_readiness_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_rule_delay.c src/game_rule_delay_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_actor_resume.c src/game_actor_resume_adapter.cpp src/game_actor_resume_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_stream_queue_count.c src/game_stream_queue_count_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_ball_actor_contact.c src/game_ball_actor_contact_adapter.cpp src/game_ball_actor_contact_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_ball_contact_gate.c src/game_ball_contact_gate_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_contact_dispatch.c src/game_contact_dispatch_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_actor_contact_gate.c src/game_actor_contact_gate_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_ball_acquire.c src/game_ball_acquire_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_actor_input.c src/game_actor_input_adapter.cpp src/game_actor_input_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_actor_contact_eligibility.c src/game_actor_contact_eligibility_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_override_end.c src/game_camera_override_end_adapter.cpp src/game_camera_override_end_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_opponent_contact.c src/game_opponent_contact_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_frame_transform.c src/game_camera_frame_transform_adapter.cpp src/game_camera_frame_transform_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_frame_interrupt_disable.c src/game_frame_interrupt_disable_adapter.cpp src/game_frame_interrupt_disable_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_frame_interrupt_restore.c src/game_frame_interrupt_restore_adapter.cpp src/game_frame_interrupt_restore_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_actor_collision_response.c src/game_actor_collision_response_adapter.cpp src/game_actor_collision_response_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_clear_ordering_table.c src/game_clear_ordering_table_adapter.cpp src/game_clear_ordering_table_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_overlay_packets.c src/game_camera_overlay_packets_adapter.cpp src/game_camera_overlay_packets_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_rotation_matrix.c src/game_rotation_matrix_adapter.cpp src/game_rotation_matrix_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_ordering_table_dma.c src/game_ordering_table_dma_adapter.cpp src/game_ordering_table_dma_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_gte_rotation_install.c src/game_gte_rotation_install_adapter.cpp src/game_gte_rotation_install_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_gte_translation_install.c src/game_gte_translation_install_adapter.cpp src/game_gte_translation_install_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_gte_reference_transform.c src/game_gte_reference_transform_adapter.cpp src/game_gte_reference_transform_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_bios_memory_copy.c src/game_bios_memory_copy_adapter.cpp src/game_bios_memory_copy_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_display_environment.c src/game_display_environment_adapter.cpp src/game_display_environment_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_video_mode.c src/game_video_mode_adapter.cpp src/game_video_mode_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_gpu_control_command.c src/game_gpu_control_command_adapter.cpp src/game_gpu_control_command_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_draw_environment.c src/game_draw_environment_adapter.cpp src/game_draw_environment_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_draw_packet.c src/game_draw_packet_adapter.cpp src/game_draw_packet_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_draw_area_start.c src/game_draw_area_start_adapter.cpp src/game_draw_area_start_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_draw_area_end.c src/game_draw_area_end_adapter.cpp src/game_draw_area_end_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_graphics_submit.c src/game_graphics_submit_adapter.cpp src/game_graphics_submit_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_gpu_packet_dma.c src/game_gpu_packet_dma_adapter.cpp src/game_gpu_packet_dma_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_draw_offset_command.c src/game_draw_offset_command_adapter.cpp src/game_draw_offset_command_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_draw_mode_command.c src/game_draw_mode_command_adapter.cpp src/game_draw_mode_command_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_state_reset.c src/game_match_state_reset_adapter.cpp src/game_match_state_reset_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_texture_window_command.c src/game_texture_window_command_adapter.cpp src/game_texture_window_command_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_controller_profile_reset.c src/game_controller_profile_reset_adapter.cpp src/game_controller_profile_reset_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_team_header_initialize.c src/game_team_header_initialize_adapter.cpp src/game_team_header_initialize_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_team_strategy_apply.c src/game_team_strategy_apply_adapter.cpp src/game_team_strategy_apply_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_buffer_initialize.c src/game_match_buffer_initialize_adapter.cpp src/game_match_buffer_initialize_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_buffer_rewind.c src/game_match_buffer_rewind_adapter.cpp src/game_match_buffer_rewind_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_substitution_candidate_select.c src/game_substitution_candidate_select_adapter.cpp src/game_substitution_candidate_select_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_buffer_pending.c src/game_match_buffer_pending_adapter.cpp src/game_match_buffer_pending_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_buffer_record.c src/game_match_buffer_record_adapter.cpp src/game_match_buffer_record_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_match_buffer_compress.c src/game_match_buffer_compress_adapter.cpp src/game_match_buffer_compress_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_period_music_start.c src/game_period_music_start_adapter.cpp src/game_period_music_start_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_period_presentation_finish.c src/game_period_presentation_finish_adapter.cpp src/game_period_presentation_finish_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_period_audio_flag_clear.c src/game_period_audio_flag_clear_adapter.cpp src/game_period_audio_flag_clear_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_period_audio_noop.c src/game_period_audio_noop_adapter.cpp src/game_period_audio_noop_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_pregame_match_card.c src/game_pregame_match_card_adapter.cpp src/game_pregame_match_card_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_pregame_selection_screen.c src/game_pregame_selection_screen_adapter.cpp src/game_pregame_selection_screen_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_phase_select.c src/game_camera_phase_select_adapter.cpp src/game_camera_phase_select_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_frame_ui_service.c src/game_frame_ui_service_adapter.cpp src/game_frame_ui_service_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_cross_half_rule.c src/game_cross_half_rule_adapter.cpp src/game_cross_half_rule_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_elapsed_dispatch.c src/game_camera_elapsed_dispatch_adapter.cpp src/game_camera_elapsed_dispatch_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_countdown_ui_update.c src/game_countdown_ui_update_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_actor_timers.c src/game_actor_timers_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_state_lookup.c src/game_camera_state_lookup_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_text_chain_clear.c src/game_text_chain_clear_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_image_record_upload.c src/game_image_record_upload_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_stamina_handicap.c src/game_stamina_handicap_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_camera_remainder_gate.c src/game_camera_remainder_gate_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_rectangle_upload_submit.c src/game_rectangle_upload_submit_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_text_submission.c src/game_text_submission_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_rectangle_normalize.c src/game_rectangle_normalize_adapter.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/game_team_tactics_update.c src/game_team_tactics_update_adapter.cpp src/game_team_tactics_update_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_dispatch.c src/frontend_dispatch_adapter.cpp src/frontend_dispatch_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_dispatch_entry.c src/frontend_dispatch_entry_adapter.cpp src/frontend_dispatch_entry_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_main.c src/frontend_main_adapter.cpp src/frontend_main_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_exit_cleanup.c src/frontend_exit_cleanup_adapter.cpp src/frontend_exit_cleanup_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_exit_wait.c src/frontend_exit_wait_adapter.cpp src/frontend_exit_wait_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_exit_drain.c src/frontend_exit_drain_adapter.cpp src/frontend_exit_drain_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_clock_read.c src/frontend_clock_read_adapter.cpp src/frontend_clock_read_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_io_drain.c src/frontend_io_drain_adapter.cpp src/frontend_io_drain_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_io_complete.c src/frontend_io_complete_adapter.cpp src/frontend_io_complete_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_overlay_load.c src/frontend_overlay_load_adapter.cpp src/frontend_overlay_load_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_load_payload.c src/frontend_load_payload_adapter.cpp src/frontend_load_payload_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_memory_copy.c src/frontend_memory_copy_adapter.cpp src/frontend_memory_copy_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_resource_load.c src/frontend_resource_load_adapter.cpp src/frontend_resource_load_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/gameload_entry.c src/gameload_entry_adapter.cpp src/gameload_entry_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/frontend_resource_lookup.c src/frontend_resource_lookup_adapter.cpp src/frontend_resource_lookup_capture.cpp)

target_sources(nba97_boot_decomp PRIVATE src/recovered/gameload_main.c src/gameload_main_adapter.cpp src/gameload_main_capture.cpp)
