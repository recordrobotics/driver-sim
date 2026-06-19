cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set(REPROC++ ON CACHE BOOL "" FORCE)
set(REPROC_OBJECT_LIBRARIES ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    reproc++
    GIT_REPOSITORY https://github.com/daandemeyer/reproc.git
    GIT_TAG        v14.2.7
)
FetchContent_MakeAvailable(reproc++)