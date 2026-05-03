cmake_minimum_required(VERSION 3.16)
project(torrent-client-prototype CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

# Системный libcpr (например libcurl4-openssl-dev + libcpr-dev в Debian/Ubuntu) или
# автоматическая загрузка исходников, если пакет не установлен.
find_package(cpr CONFIG QUIET)
if (NOT cpr_FOUND)
    message(STATUS "cpr не найден в системе — подключаю libcpr через FetchContent (нужны git и сеть при первой конфигурации)")
    set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    include(FetchContent)
    FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.10.5
    )
    FetchContent_MakeAvailable(cpr)
endif ()

add_executable(torrent-client-prototype
    main.cpp
    bencode.cpp
    byte_tools.cpp
    message.cpp
    peer_connect.cpp
    piece.cpp
    piece_storage.cpp
    tcp_connect.cpp
    torrent_file.cpp
    torrent_tracker.cpp
)

target_link_libraries(torrent-client-prototype PRIVATE OpenSSL::Crypto cpr::cpr)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(torrent-client-prototype PRIVATE -Wall -Wextra -pedantic)
endif ()
