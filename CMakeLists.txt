# teflib/CMakeLists.txt
#
cmake_minimum_required(VERSION 3.9)
project(FormatOutput CXX)

message("adebug -- CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}")
message("adebug -- CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}")

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
message("adebug -- CMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}")

add_definitions("-std=c++11")

include(externals)

add_subdirectory(src)
add_subdirectory(example)

