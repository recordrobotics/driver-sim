cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

FetchContent_Declare(
    discordsdk
    GIT_REPOSITORY https://github.com/Compdog-inc/discord-sdk-cmake.git
    GIT_TAG v1.0.0
    GIT_SHALLOW 1
)

FetchContent_MakeAvailable(discordsdk)