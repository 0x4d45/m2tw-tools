set(THIS pack)

configure_file(main.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/main.hpp)
add_executable(${THIS} main.cpp pack.cpp util.cpp)
target_compile_features(${THIS} PRIVATE cxx_std_23)
target_compile_definitions(${THIS} PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(${THIS} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(${THIS} PRIVATE extern::cli11 extern::lzokay extern::mio extern::spdlog)
install(TARGETS ${THIS} DESTINATION bin)
