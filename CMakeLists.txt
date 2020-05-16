cmake_minimum_required(VERSION 3.7)

project(msemu LANGUAGES C)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    message(WARNING "No CMAKE_BUILD_TYPE value specified, defaulting to ${CMAKE_BUILD_TYPE}.")
else ()
    message(STATUS "The CMake build type is: ${CMAKE_BUILD_TYPE}")
endif ()

add_subdirectory(src)
