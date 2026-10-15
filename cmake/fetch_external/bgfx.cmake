cmake_minimum_required(VERSION 3.25)

include(FetchContent)

FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG v1.166.9546-589
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/bgfx
    PATCH_COMMAND
        ${CMAKE_COMMAND}
            -DSOURCE_DIR=<SOURCE_DIR>/bgfx
            -DPATCH_URL=https://github.com/bkaradzic/bgfx/compare/master...Compdog-inc:bgfx:shaderc-rt-format.patch
            -P ${CMAKE_CURRENT_LIST_DIR}/../ApplyBgfxPatch.cmake
)

set( BGFX_BUILD_TOOLS ON CACHE INTERNAL "")
set( BGFX_BUILD_TOOLS_SHADER ON CACHE INTERNAL "")
set( BGFX_BUILD_EXAMPLES  OFF CACHE INTERNAL "" )
set( BGFX_CUSTOM_TARGETS  OFF CACHE INTERNAL "" )
set (BGFX_CONFIG_RENDERER_WEBGPU OFF CACHE INTERNAL "" FORCE)
set( BGFX_INSTALL  OFF CACHE INTERNAL "" )

add_compile_definitions(
    BGFX_PLATFORM_SUPPORTS_WGSL=0
    BGFX_PLATFORM_SUPPORTS_ESSL=0
    BGFX_CONFIG_GL_NORMALIZE_NDC_CONVENTIONS=1
    )

if(NOT WIN32)
    add_compile_definitions(BGFX_PLATFORM_SUPPORTS_DXBC=0)
endif()

FetchContent_GetProperties(bgfx)
if(NOT bgfx_POPULATED)
    FetchContent_Populate(bgfx)
endif()

add_subdirectory("${bgfx_SOURCE_DIR}" "${bgfx_BINARY_DIR}")