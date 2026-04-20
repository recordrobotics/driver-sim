cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
    bgfx
    GIT_REPOSITORY "git@github.com:bkaradzic/bgfx.cmake.git"
    GIT_TAG v1.143.9226-530
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/bgfx
)

set( BGFX_BUILD_TOOLS ON CACHE INTERNAL "")
set( BGFX_BUILD_TOOLS_SHADER ON CACHE INTERNAL "")
set( BGFX_BUILD_EXAMPLES  OFF CACHE INTERNAL "" )
set( BGFX_CUSTOM_TARGETS  OFF CACHE INTERNAL "" )

FetchContent_GetProperties(bgfx)
if(NOT bgfx_POPULATED)
    FetchContent_Populate(bgfx)
endif()

set(_bgfx_tool_utils_file "${bgfx_SOURCE_DIR}/cmake/bgfxToolUtils.cmake")
if(EXISTS "${_bgfx_tool_utils_file}")
    file(READ "${_bgfx_tool_utils_file}" _bgfx_tool_utils_contents)
    string(REPLACE "$<IF:$<CONFIG:Debug>:0,3>" "$<IF:$<CONFIG:Debug>,0,3>" _bgfx_tool_utils_contents "${_bgfx_tool_utils_contents}")
    file(WRITE "${_bgfx_tool_utils_file}" "${_bgfx_tool_utils_contents}")
endif()

add_subdirectory("${bgfx_SOURCE_DIR}" "${bgfx_BINARY_DIR}")