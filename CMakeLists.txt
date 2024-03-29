cmake_minimum_required(VERSION 3.16)
project(OPN C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

set(MAX_CHIPS 0x10 CACHE STRING "Max amount of chips supported")
if(NOT MAX_CHIPS MATCHES "^0[xX][0-9a-fA-F]+$")
	message(FATAL_ERROR "MAX_CHIPS must be a hexadecimal number greater than 1")
endif()

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

option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
if (${FORCE_COLORED_OUTPUT})
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		add_compile_options(-fdiagnostics-color=always)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		add_compile_options(-fcolor-diagnostics)
	endif ()
endif ()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

include_directories(.)
include_directories(lib/miniaudio)

set(ym2612Srcs
		src/ym2612/fm2612.cpp
		src/ym2612/mamedef.hpp
		)

add_library(OPN SHARED
		src/OPN_DLL.cpp
		src/OPN_DLL.hpp
		lib/miniaudio/miniaudio.c
		#src/audio/Stream.c
		src/audio/miniaudioStream.cpp
		src/audio/stream.hpp
		${ym2612Srcs} src/audio/ym2612DataSource.cpp src/audio/ym2612DataSource.hpp)

target_link_libraries(OPN winmm)
target_compile_definitions(OPN PUBLIC WIN_EXPORT MA_USE_STDINT MAX_CHIPS=${MAX_CHIPS})

if (MSVC)
	set_target_properties(OPN PROPERTIES OUTPUT_NAME "libOPN") # makes sure MSVC gives the right name
endif ()

enable_testing()

add_executable(OPNTest Tests/OPNTest.cpp)
set_target_properties(OPNTest PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_link_libraries(OPNTest OPN)

add_test(NAME SoundTest
		COMMAND OPNTest
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/Tests)