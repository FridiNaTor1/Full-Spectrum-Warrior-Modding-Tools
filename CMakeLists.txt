cmake_minimum_required(VERSION 3.16)
project(fsw_mod_kit_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(fsw_tool STATIC fsw_tool.cpp)
target_include_directories(fsw_tool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(fsw_mod_kit_cli cli_main.cpp)
target_link_libraries(fsw_mod_kit_cli PRIVATE fsw_tool)

# Enable warnings
if(MSVC)
  target_compile_options(fsw_mod_kit_cli PRIVATE /W4 /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
else()
  target_compile_options(fsw_mod_kit_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GUI target (Qt5 or Qt6)
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

add_executable(fsw_mod_kit_gui gui_main.cpp)
target_link_libraries(fsw_mod_kit_gui PRIVATE fsw_tool Qt${QT_VERSION_MAJOR}::Widgets)

if(MSVC)
  target_compile_options(fsw_mod_kit_gui PRIVATE /W4 /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
else()
  target_compile_options(fsw_mod_kit_gui PRIVATE -Wall -Wextra -Wpedantic)
endif()

