cmake_minimum_required(VERSION 3.16)
project(OPN C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

if (MSVC)
	# Force to always compile with W4
	if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else ()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif ()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
else ()
	message(WARNING "CMake flags for compiler aren't set for compiler ${CMAKE_CXX_COMPILER_ID}")
endif ()

option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
if (${FORCE_COLORED_OUTPUT})
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		add_compile_options (-fdiagnostics-color=always)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		add_compile_options (-fcolor-diagnostics)
	endif ()
endif ()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

include_directories(.)

set(ym2612Srcs
		src/ym2612/2612intf.c
		src/ym2612/2612intf.h
		src/ym2612/fm2612.c
		src/ym2612/fm.h
		src/ym2612/mamedef.h
		)

add_library(OPN SHARED
		src/OPN_DLL.cpp
		src/OPN_DLL.hpp
		src/audio/Stream.c
		src/audio/Stream.h
		${ym2612Srcs})

target_link_libraries(OPN winmm)
target_compile_definitions(OPN PUBLIC WIN_EXPORT)

if(MSVC)
	set_target_properties(OPN PROPERTIES OUTPUT_NAME "libOPN") # makes sure MSVC gives the right name
endif()

add_executable(OPNTest Tests/OPNTest.cpp)
target_link_libraries(OPNTest OPN)

enable_testing()

add_test(NAME SoundTest
		COMMAND OPNTest
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/Tests)