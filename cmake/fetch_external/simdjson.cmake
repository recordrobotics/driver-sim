cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG        v4.6.4
    GIT_SHALLOW 1
)
FetchContent_MakeAvailable(simdjson)