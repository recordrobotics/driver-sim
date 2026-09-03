cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG        v3.2
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/argparse
)

set(ARGPARSE_INSTALL OFF CACHE INTERNAL "" FORCE)
set(ARGPARSE_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)

FetchContent_MakeAvailable(argparse)