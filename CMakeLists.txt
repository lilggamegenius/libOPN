cmake_minimum_required(VERSION 3.16)
project(libOPN C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_library(libOPN
        2612intf.c
        2612intf.h
        fm.h
        fm2612.c
        mamedef.h
        OPN_DLL.c
        OPN_DLL.h
        stdbool.h
        Stream.c
        Stream.h)
