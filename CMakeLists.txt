cmake_minimum_required(VERSION 3.20)

project(win32_helper)

add_subdirectory(cpp_helper)

add_library(win32_helper
    win32_helper.cpp
    win32_helper.hpp)

target_link_libraries(win32_helper PUBLIC cpp_helper)

target_compile_features(win32_helper PUBLIC cxx_std_20)
target_include_directories(win32_helper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(win32_helper_test
    main.cpp)

target_link_libraries(win32_helper_test PUBLIC win32_helper)
