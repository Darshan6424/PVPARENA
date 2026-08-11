cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(PvPArena LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Single-config generators (Make, Ninja) leave this empty, which silently gives
# you an unoptimised build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Static CRT, to match the x64-windows-static vcpkg triplet in
# .vscode/settings.json. Both have to agree or you get link errors, or worse,
# two heaps in one process.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
else()
    target_compile_options(project_warnings INTERFACE -Wall -Wextra)
endif()

# Everything includes from the src root, so a header is always spelled the same
# way no matter which package includes it:  #include "server/sim/Fighter.h"
add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# The dedicated server needs no graphics, audio or display at all, so a
# headless box can build just that:  cmake -B build -S . -DBUILD_CLIENT=OFF
option(BUILD_CLIENT "Build the SFML client (needs SFML 3)" ON)

if(BUILD_CLIENT)
    # SFML 3.x. The 2.x API differs enough (events, sf::Text and sf::FloatRect
    # constructors, scoped enums, openFromFile) that it will not compile.
    #
    # SFML can be installed as shared libraries, static ones, or both. When
    # only the static ones are present its config file reports SFML_FOUND as
    # false until SFML_STATIC_LIBRARIES is set, so try again that way before
    # giving up.
    find_package(SFML 3 COMPONENTS Graphics Window System Audio CONFIG QUIET)
    if(NOT SFML_FOUND)
        set(SFML_STATIC_LIBRARIES ON)
        find_package(SFML 3 COMPONENTS Graphics Window System Audio CONFIG REQUIRED)
        message(STATUS "SFML: using static libraries")
    endif()
endif()

find_package(Threads REQUIRED)

# --- src/common: shared by both sides ---------------------------------------
add_library(common STATIC
    src/common/UdpSocket.cpp
    src/common/PathUtils.cpp
)
target_link_libraries(common PUBLIC project_includes)
target_link_libraries(common PRIVATE project_warnings)
if(WIN32)
    target_link_libraries(common PUBLIC ws2_32)
endif()

# --- src/server: rules and networking, no SFML ------------------------------
add_library(server_lib STATIC
    src/server/sim/Fighter.cpp
    src/server/sim/ArenaSim.cpp
    src/server/net/GameServer.cpp
)
target_link_libraries(server_lib PUBLIC common Threads::Threads)
target_link_libraries(server_lib PRIVATE project_warnings)

add_executable(server src/server/main.cpp)
target_link_libraries(server PRIVATE server_lib project_warnings)

# --- src/client: the parts with no SFML in them -----------------------------
# Kept separate so the tests can use them on a headless build.
add_library(client_lib STATIC
    src/client/net/GameClient.cpp
    src/client/app/MatchWatcher.cpp
)
target_link_libraries(client_lib PUBLIC common)
target_link_libraries(client_lib PRIVATE project_warnings)

if(BUILD_CLIENT)
    add_executable(client
        src/client/app/main.cpp
        src/client/app/Game.cpp
        src/client/screens/TitleScreen.cpp
        src/client/screens/AddressScreen.cpp
        src/client/screens/ConnectingScreen.cpp
        src/client/screens/MatchScreen.cpp
        src/client/view/Assets.cpp
        src/client/view/Renderer.cpp
        src/client/view/AudioManager.cpp
        src/client/input/InputCapture.cpp
    )
    target_link_libraries(client PRIVATE
        client_lib server_lib project_warnings
        SFML::Graphics SFML::Window SFML::System SFML::Audio)

    # Assets load relative to the executable, so keep a copy beside it.
    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:client>/assets"
        COMMENT "Copying assets next to the client"
    )
endif()

# Fully static server binary: no libstdc++, no libgcc, nothing to install on
# the target box. Deploying then means copying one file.
option(STATIC_SERVER "Link the server executable statically" OFF)
if(STATIC_SERVER)
    if(MSVC)
        message(FATAL_ERROR "STATIC_SERVER is for GCC/Clang; MSVC already uses the static CRT here")
    endif()
    target_link_options(server PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# --- tests ------------------------------------------------------------------
option(BUILD_TESTS "Build the test executables" ON)
if(BUILD_TESTS)
    enable_testing()

    add_executable(fighter_test tests/fighter_test.cpp)
    target_link_libraries(fighter_test PRIVATE server_lib project_warnings)

    add_executable(sim_test tests/sim_test.cpp)
    target_link_libraries(sim_test PRIVATE server_lib project_warnings)

    add_executable(network_test tests/network_test.cpp)
    target_link_libraries(network_test PRIVATE server_lib client_lib project_warnings)

    add_executable(watcher_test tests/watcher_test.cpp)
    target_link_libraries(watcher_test PRIVATE client_lib project_warnings)

    add_test(NAME fighter COMMAND fighter_test)
    add_test(NAME sim COMMAND sim_test)
    add_test(NAME network COMMAND network_test)
    add_test(NAME watcher COMMAND watcher_test)
endif()

# --- install ----------------------------------------------------------------
# "cmake --install build --prefix dist" gives you a folder you can zip and send.
install(TARGETS server RUNTIME DESTINATION .)
if(BUILD_CLIENT)
    install(TARGETS client RUNTIME DESTINATION .)
    install(DIRECTORY assets DESTINATION .)   # client-only; the server reads no files
endif()
