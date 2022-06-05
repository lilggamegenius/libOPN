cmake_minimum_required(VERSION 3.16)
project(libOPN C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_library(libOPN SHARED
        OPN_DLL.cpp
        ym2612/2612intf.c
        ym2612/fm2612.c
        audio/Stream.c)

target_link_libraries(libOPN winmm)