cmake_minimum_required(VERSION 3.16)
project(PvPArena LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SFML ------------------------------------------------------------
# On Windows with vcpkg: configure with
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake
# On Linux: install libsfml-dev (apt) or your distro's equivalent first.
# Targets SFML 3.x (CMake package name/targets changed from SFML 2.x).
find_package(SFML 3 COMPONENTS Graphics Window System Audio CONFIG REQUIRED)

# --- Shared code used by both server and client -----------------------
add_library(common STATIC
    common/UdpSocket.cpp
    common/PathUtils.cpp
)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_libraries(common PUBLIC ws2_32)
endif()

# --- Server simulation (no SFML dependency - it's headless logic) -----
add_library(server_lib STATIC
    server/GameServer.cpp
)
target_link_libraries(server_lib PUBLIC common)

# --- Standalone dedicated server executable ---------------------------
add_executable(server server/main_server.cpp)
target_link_libraries(server PRIVATE server_lib)

find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)

# --- Client executable (SFML required) ---------------------------------
add_executable(client
    client/main_client.cpp
    client/GameClient.cpp
    client/Renderer.cpp
    client/AudioManager.cpp
)
target_link_libraries(client PRIVATE common server_lib Threads::Threads
    SFML::Graphics SFML::Window SFML::System SFML::Audio)

# Sprites/audio load relative to the .exe at runtime, so make sure a
# fresh copy of assets/ always sits right next to whatever build the
# client just produced.
add_custom_command(TARGET client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:client>/assets"
    COMMENT "Copying assets/ next to the client executable"
)

# --- Optional headless combat-logic test (no SFML needed) --------------
option(BUILD_TESTS "Build the headless combat-logic test" ON)
if(BUILD_TESTS)
    add_executable(headless_test test/headless_test.cpp)
    target_link_libraries(headless_test PRIVATE common server_lib Threads::Threads)
endif()
