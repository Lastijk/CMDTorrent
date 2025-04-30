cmake_minimum_required(VERSION 3.22)
project(CMDTorrentProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG master
)
FetchContent_MakeAvailable(cpr)

add_executable(CMDTorrent
        src/BencodeParser.cpp
        src/BencodeParser.hpp
        src/Peer.hpp
        src/TorrentFile.cpp
        src/TorrentFile.hpp
        src/TorrentTracker.cpp
        src/TorrentTracker.hpp
        src/TCP_Conncet.hpp
        src/TCP_Connect.cpp
)

target_link_libraries(CMDTorrent PRIVATE cpr::cpr)