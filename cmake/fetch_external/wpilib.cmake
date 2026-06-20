cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
    wpilib
    GIT_REPOSITORY https://github.com/wpilibsuite/allwpilib.git
    GIT_TAG        v2026.2.1
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/wpilib-src
)
set(WITH_CSCORE  OFF CACHE BOOL "" FORCE)
set(WITH_GUI  OFF CACHE BOOL "" FORCE)
set(WITH_SIMULATION_MODULES  OFF CACHE BOOL "" FORCE)
set(WITH_TESTS  OFF CACHE BOOL "" FORCE)
set(WITH_PROTOBUF  OFF CACHE BOOL "" FORCE)
set(WITH_WPILIB  OFF CACHE BOOL "" FORCE)
set(WITH_WPIMATH  ON CACHE BOOL "" FORCE)
set(NO_WERROR  ON CACHE BOOL "" FORCE)
set(WITH_BENCHMARK  OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_LIBUV OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_FMTLIB ON CACHE BOOL "" FORCE)

set(fmt_DIR "${fmt_BINARY_DIR}")

set(wpilib_SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/wpilib-src")

# Patch wpilib CMake files for MinGW compatibility
set(WPIUTIL_CMAKE_FILE "${wpilib_SOURCE_DIR}/wpiutil/CMakeLists.txt")
set(WPINET_CMAKE_FILE "${wpilib_SOURCE_DIR}/wpinet/CMakeLists.txt")

# Apply patches after fetching but before making available
FetchContent_GetProperties(wpilib)
if(NOT wpilib_POPULATED)
    FetchContent_Populate(wpilib)
    
    # Patch wpiutil/CMakeLists.txt
    file(READ ${WPIUTIL_CMAKE_FILE} WPIUTIL_CONTENT)
    string(REPLACE "if(MSVC)\n    target_sources(wpiutil PRIVATE \${wpiutil_windows_src})" 
                   "if(MSVC OR WIN32)\n    target_sources(wpiutil PRIVATE \${wpiutil_windows_src})" 
                   WPIUTIL_CONTENT "${WPIUTIL_CONTENT}")
    file(WRITE ${WPIUTIL_CMAKE_FILE} "${WPIUTIL_CONTENT}")
    
    # Patch wpinet/CMakeLists.txt
    file(READ ${WPINET_CMAKE_FILE} WPINET_CONTENT)
    string(REPLACE "if(NOT MSVC)\n        target_sources(wpinet PRIVATE \${uv_unix_src})" 
                   "if(NOT MSVC AND NOT WIN32)\n        target_sources(wpinet PRIVATE \${uv_unix_src})" 
                   WPINET_CONTENT "${WPINET_CONTENT}")
    string(REPLACE "if(MSVC)\n    target_sources(wpinet PRIVATE \${wpinet_windows_src})" 
                   "if(MSVC OR WIN32)\n    target_sources(wpinet PRIVATE \${wpinet_windows_src})" 
                   WPINET_CONTENT "${WPINET_CONTENT}")
    file(WRITE ${WPINET_CMAKE_FILE} "${WPINET_CONTENT}")
    
    add_subdirectory(${wpilib_SOURCE_DIR} ${wpilib_BINARY_DIR})
endif()

FetchContent_MakeAvailable(wpilib)