# Get vcpkg cmake toolchain file from VCPKG_ROOT
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
    message(STATUS "CMAKE_TOOLCHAIN_FILE not set or empty. Using VCPKG_ROOT to load vcpkg toolchain file.")
    if ((NOT DEFINED VCPKG_ROOT OR VCPKG_ROOT STREQUAL "") AND DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    endif ()
    if (NOT DEFINED VCPKG_ROOT OR VCPKG_ROOT STREQUAL "")
        message(FATAL_ERROR "VCPKG_ROOT not set or empty! Please install vcpkg and set the VCPKG_ROOT environment or cmake variable.")
    endif ()
    cmake_path(NORMAL_PATH VCPKG_ROOT OUTPUT_VARIABLE VCPKG_ROOT)

    message(STATUS "VCPKG_ROOT is: ${VCPKG_ROOT}")
    cmake_path(APPEND VCPKG_TOOLCHAIN_PATH "${VCPKG_ROOT}" "scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN_PATH}" CACHE STRING "VCPKG toolchain file")
    message(STATUS "Using vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif ()

# Vcpkg config needs to come before "project". Using a toolchain file would be better.
set(ENV{CC} ${CMAKE_C_COMPILER})
set(ENV{CXX} ${CMAKE_CXX_COMPILER})