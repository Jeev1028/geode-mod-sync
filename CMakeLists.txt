cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal macOS only; forcing it on iOS breaks the compiler check (see Level Link).
if (NOT GEODE_TARGET_PLATFORM STREQUAL "iOS")
	set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "Minimum OS X deployment version")

project(ModSync VERSION 1.0.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (NOT DEFINED ENV{GEODE_SDK})
	message(FATAL_ERROR "Set the GEODE_SDK environment variable.")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Workaround for geode-sdk/bindings#1413: on iOS CI the bindings' forced unity
# build (batch of 128) puts GeneratedSource.cpp in a batch without the Geode
# headers -> "unknown type name 'FMOD_RESULT'". Turn unity off for that target.
if (GEODE_TARGET_PLATFORM STREQUAL "iOS" AND TARGET GeodeBindings)
	set_target_properties(GeodeBindings PROPERTIES UNITY_BUILD OFF)
	# GeneratedSource.cpp also lacks its own includes (FMOD_RESULT, FunctionType),
	# so force the Geode bindings header into every TU of that target.
	target_compile_options(GeodeBindings PRIVATE -include Geode/Bindings.hpp)
endif()

setup_geode_mod(${PROJECT_NAME})
