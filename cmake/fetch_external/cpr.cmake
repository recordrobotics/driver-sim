cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL "Disable libpsl in curl" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "Disable zlib in curl" FORCE)

FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.14.2
)
FetchContent_MakeAvailable(cpr)