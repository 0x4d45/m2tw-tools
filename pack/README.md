# pack

This directory contains the source code of the `pack` tool. The tool can currently be used to peek into and extract files from `.pack` files.

## Getting started

1. Make sure you have the following tools:
    - Clang
    - CMake
    - Ninja
2. Ensure all submodules have been cloned: `git submodule update --init --recursive`
3. Configure the project: `cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release`
4. Build the project: `cmake --build build --parallel`
5. Run the executable: `build/src/pack --help`
