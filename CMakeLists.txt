cmake_minimum_required(VERSION 3.16)
project(OPN C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

include_directories(.)

set(ym2612Srcs
		ym2612/2612intf.c
		ym2612/fm2612.c
		)

add_library(OPN SHARED
        OPN_DLL.cpp
        audio/Stream.c
		${ym2612Srcs})

add_executable(libOPNTest Tests/OPNTest.cpp)

target_link_libraries(OPN winmm)
target_compile_definitions(OPN PUBLIC WIN_EXPORT)

if(MSVC)
	set_target_properties(OPN PROPERTIES OUTPUT_NAME "libOPN") # makes sure MSVC gives the right name
endif()