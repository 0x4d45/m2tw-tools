set(SUBMODULES ../../submodules)

# ----------------------------------------------------------
# argparse
# ----------------------------------------------------------

add_library(extern-argparse INTERFACE)
target_include_directories(extern-argparse INTERFACE ${SUBMODULES}/argparse/include)
add_library(extern::argparse ALIAS extern-argparse)

# ----------------------------------------------------------
# glob
# ----------------------------------------------------------

add_library(extern-glob INTERFACE)
target_include_directories(extern-glob INTERFACE ${SUBMODULES}/glob/single_include)
add_library(extern::glob ALIAS extern-glob)

# ----------------------------------------------------------
# lzokay
# ----------------------------------------------------------

add_subdirectory(${SUBMODULES}/lzokay lzokay)
add_library(extern-lzokay INTERFACE)
target_link_libraries(extern-lzokay INTERFACE lzokay)
add_library(extern::lzokay ALIAS extern-lzokay)

# ----------------------------------------------------------
# mio
# ----------------------------------------------------------

add_library(extern-mio INTERFACE)
target_include_directories(extern-mio INTERFACE ${SUBMODULES}/mio/single_include)
add_library(extern::mio ALIAS extern-mio)
