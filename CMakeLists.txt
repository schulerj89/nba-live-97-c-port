cmake_minimum_required(VERSION 3.20)
project(nba_live_97_decomp LANGUAGES CXX)

if(WIN32)
    add_executable(nba97_boot_decomp
        src/win32_main.cpp
        src/intro_player.cpp
        src/frontend_music.cpp
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
        src/psh_image.cpp
        src/psh_font.cpp
    )
    target_link_libraries(nba97_boot_decomp PRIVATE SDL2::SDL2)
endif()

target_compile_features(nba97_boot_decomp PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(nba97_boot_decomp PRIVATE /W4 /permissive-)
else()
    target_compile_options(nba97_boot_decomp PRIVATE -Wall -Wextra -Wpedantic)
endif()
