cmake_minimum_required(VERSION 3.20)
project(nba_live_97_decomp LANGUAGES C CXX)

include(CTest)
option(NBA97_RECOVERED_TESTS_ONLY "Build asset-free recovered logic tests without the UI" OFF)
if(BUILD_TESTING)
    add_executable(nba97_reorder_tests
        tests/reorder_rosters_test.cpp
        src/reorder_preview.cpp
        src/psh_font.cpp
        src/recovered/roster_reorder.c
        src/roster_database.cpp
        src/recovered/semantic_trace.c)
    target_compile_features(nba97_reorder_tests PRIVATE c_std_99 cxx_std_17)
    target_include_directories(nba97_reorder_tests PRIVATE src)
    add_test(NAME reorder_rosters_core COMMAND nba97_reorder_tests)
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
    add_executable(nba97_boot_decomp
        src/win32_main.cpp
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
        src/psh_image.cpp
        src/psh_font.cpp
    )
    target_compile_definitions(nba97_boot_decomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(nba97_boot_decomp PRIVATE
        user32 gdi32 ole32 oleaut32 strmiids uuid d3d9 windowscodecs winmm
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
target_sources(nba97_boot_decomp PRIVATE src/recovered/roster_reorder.c)
if(MSVC)
    target_compile_options(nba97_boot_decomp PRIVATE /W4 /permissive-)
else()
    target_compile_options(nba97_boot_decomp PRIVATE -Wall -Wextra -Wpedantic)
endif()
