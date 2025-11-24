# In CLion reloading cmake profiles in parallel can cause issues with the vcpkg lock.
# Make sure to enable the Reload CMake profiles sequentially option in Settings | Advanced Settings | CMake.

cmake_minimum_required(VERSION 4.1)

include(FetchContent)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Verbose Makefile" FORCE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)

# Dependencies are managed with vcpkg when possible
include(cmake/VcpkgToolchain.cmake)
include(cmake/CompilerConfig.cmake)

# Project Config

project(CGProject)

add_subdirectory(src)