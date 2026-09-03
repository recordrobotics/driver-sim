cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL "Disable libpsl in curl" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "Disable zlib in curl" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "Disable building libcurl docs" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "Disable building libcurl docs" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "Disable building libcurl docs" FORCE)

FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.14.2
    GIT_SHALLOW 1
)
FetchContent_MakeAvailable(cpr)