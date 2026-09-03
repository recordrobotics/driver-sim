cmake_minimum_required(VERSION 3.25)

include(FetchContent)

FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG v1.153.9385-563
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
set (MINIZ_LIBRARIES "miniz" CACHE INTERNAL "" FORCE)
set (MINIZ_INCLUDE_DIR "${FETCHCONTENT_BASE_DIR}/" CACHE INTERNAL "" FORCE)

add_compile_definitions(BGFX_PLATFORM_SUPPORTS_WGSL=0)

FetchContent_GetProperties(bgfx)
if(NOT bgfx_POPULATED)
    FetchContent_Populate(bgfx)
endif()

# Patch bgfx OpenGL clip control block
set(_bgfx_gl_file "${bgfx_SOURCE_DIR}/bgfx/src/renderer_gl.cpp")

if(EXISTS "${_bgfx_gl_file}")
    file(READ "${_bgfx_gl_file}" _bgfx_gl_contents)

    # Replace the commented block with the uncommented version
    string(REPLACE
"//				if (s_extension[Extension::ARB_clip_control].m_supported)
//				{
//					GL_CHECK(glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE) );
//					g_caps.originBottomLeft = true;
//				}
//				else"
"				if (s_extension[Extension::ARB_clip_control].m_supported)
				{
					GL_CHECK(glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE) );
					g_caps.originBottomLeft = true;
				}
				else"
        _bgfx_gl_contents
        "${_bgfx_gl_contents}"
    )

    file(WRITE "${_bgfx_gl_file}" "${_bgfx_gl_contents}")
endif()

# Patch bimg_decode miniz.c redefinition
set(_bimg_decode_file "${bgfx_SOURCE_DIR}/bimg/src/image_decode.cpp")

if(EXISTS "${_bimg_decode_file}")
    file(READ "${_bimg_decode_file}" _bimg_decode_contents)

    # Comment out the miniz.c include line
    string(REPLACE
"#include <miniz/miniz.c>"
"// #include <miniz/miniz.c>"
        _bimg_decode_contents
        "${_bimg_decode_contents}"
    )

    file(WRITE "${_bimg_decode_file}" "${_bimg_decode_contents}")
endif()

add_subdirectory("${bgfx_SOURCE_DIR}" "${bgfx_BINARY_DIR}")