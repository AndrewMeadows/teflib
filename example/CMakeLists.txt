# teflib/src/CMakeLists.txt
#
set(TARGET_NAME example)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(fmt)

add_executable (${TARGET_NAME}
    main.cpp
    util/thread_pool.h
    util/timing_util.cpp
    util/timing_util.h
    util/log_util.cpp
    util/log_util.h
)

add_definitions("-DUSE_TEF")

target_include_directories(${TARGET_NAME} PUBLIC ../src/)

target_link_libraries (${TARGET_NAME}
    PUBLIC
    fmt
    teflib
)
