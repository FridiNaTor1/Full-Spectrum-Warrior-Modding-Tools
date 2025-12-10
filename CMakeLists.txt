cmake_minimum_required(VERSION 3.16)
project(fsw_mod_kit_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(fsw_mod_kit_cli main.cpp)

# Enable large file support on Windows
if(MSVC)
  target_compile_options(fsw_mod_kit_cli PRIVATE /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
else()
  target_compile_options(fsw_mod_kit_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
