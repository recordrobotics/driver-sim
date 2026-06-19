cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set( BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set( BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set ( INSTALL_PROJECT OFF CACHE INTERNAL "" FORCE)

FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        2.2.0
    GIT_SHALLOW 1
)
FetchContent_MakeAvailable(miniz)