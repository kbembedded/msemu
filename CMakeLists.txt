cmake_minimum_required(VERSION 3.7)

# Pull current version from git tags
find_package(Git)
execute_process(COMMAND
    "${GIT_EXECUTABLE}" describe --tags
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_TAGS
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX REPLACE "^v([0-9]+\\.[0-9]+).*" "\\1" VERSION "${GIT_TAGS}")

project(msemu LANGUAGES C VERSION "${VERSION}")

configure_file(config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    message(WARNING "No CMAKE_BUILD_TYPE value specified, defaulting to ${CMAKE_BUILD_TYPE}.")
else ()
    message(STATUS "The CMake build type is: ${CMAKE_BUILD_TYPE}")
endif ()

add_subdirectory(src)
