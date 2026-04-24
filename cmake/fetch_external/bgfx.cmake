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
    if(NOT _bgfx_tool_utils_contents MATCHES "PROFILE STREQUAL \"spirv\" OR PROFILE STREQUAL \"wgsl\"")
        string(REPLACE
            "set(PLATFORM_I \${PLATFORM})"
            "set(PLATFORM_I \${PLATFORM})\n\t\t\t\tif(APPLE AND (PROFILE STREQUAL \"spirv\" OR PROFILE STREQUAL \"wgsl\"))\n\t\t\t\t\t# On macOS, shaderc's osx path can select legacy sampler translation for SPIR-V/WGSL.\n\t\t\t\t\t# Using linux platform for these profiles keeps generated binaries valid and fixes parser issues.\n\t\t\t\t\tset(PLATFORM_I LINUX)\n\t\t\t\tendif()"
            _bgfx_tool_utils_contents
            "${_bgfx_tool_utils_contents}"
        )
    endif()
    file(WRITE "${_bgfx_tool_utils_file}" "${_bgfx_tool_utils_contents}")
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

add_subdirectory("${bgfx_SOURCE_DIR}" "${bgfx_BINARY_DIR}")