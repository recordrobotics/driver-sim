cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.2
    GIT_SHALLOW    1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/zlib
)

FetchContent_MakeAvailable(zlib)

set(ZLIB_INCLUDE_DIR
    "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}"
    CACHE STRING "Zlib include directories" FORCE
)

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(zlib_name "zsd.lib")
    else()
        set(zlib_name "zs.lib")
    endif()
else()
    set(zlib_name "libz.a")
endif(WIN32)

set(ZLIB_LIBRARY
    "${zlib_BINARY_DIR}/${zlib_name}"
    CACHE STRING "Zlib static library" FORCE
)