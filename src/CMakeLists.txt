# teflib/src/CMakeLists.txt
#
set(TARGET_NAME teflib)

find_package(fmt)

add_library( ${TARGET_NAME}
    trace.cpp
    trace.h
)
