set(SUBMODULES ../../submodules)
set(BUILD_TESTING OFF)

# ----------------------------------------------------------
# extern::cli11
# ----------------------------------------------------------

add_subdirectory(${SUBMODULES}/CLI11 CLI11 EXCLUDE_FROM_ALL)

add_library(extern-cli11 INTERFACE)
target_link_libraries(extern-cli11 INTERFACE CLI11::CLI11)
add_library(extern::cli11 ALIAS extern-cli11)

# ----------------------------------------------------------
# extern::lzokay
# ----------------------------------------------------------

add_subdirectory(${SUBMODULES}/lzokay lzokay EXCLUDE_FROM_ALL)

add_library(extern-lzokay INTERFACE)
target_link_libraries(extern-lzokay INTERFACE lzokay)
set_target_properties(lzokaytest PROPERTIES EXCLUDE_FROM_ALL ON)
add_library(extern::lzokay ALIAS extern-lzokay)

# ----------------------------------------------------------
# extern::mio
# ----------------------------------------------------------

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(${SUBMODULES}/mio mio EXCLUDE_FROM_ALL)

add_library(extern-mio INTERFACE)
target_link_libraries(extern-mio INTERFACE mio::mio)
add_library(extern::mio ALIAS extern-mio)

# ----------------------------------------------------------
# extern::spdlog
# ----------------------------------------------------------

set(SPDLOG_USE_STD_FORMAT ON)
add_subdirectory(${SUBMODULES}/spdlog spdlog EXCLUDE_FROM_ALL)

add_library(extern-spdlog INTERFACE)
target_link_libraries(extern-spdlog INTERFACE spdlog::spdlog)
add_library(extern::spdlog ALIAS extern-spdlog)
