cmake_minimum_required(VERSION 3.22)
project(CMDTorrent)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)

add_executable(CMDTorrent
        BencodeParser.cpp
        BencodeParser.hpp
        TorrentFile.hpp)
