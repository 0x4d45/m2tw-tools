cmake_minimum_required(VERSION 3.14)
project(pack VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

add_subdirectory(extern)
add_subdirectory(src)
