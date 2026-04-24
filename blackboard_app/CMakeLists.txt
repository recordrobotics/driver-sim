cmake_minimum_required(VERSION 3.21)

project(blackboard_app_lib)

option(BIG_RENDERER_GL "Use GL" OFF)
cmake_dependent_option(BIG_RENDERER_VULKAN "Use Vulkan" ON
                       "BIG_PLATFORM_LINUX; NOT BIG_RENDERER_GL" OFF)
option(BIG_RENDERER_WEBGPU "Compile WGSL shaders" ON)

option(BIG_WM_WAYLAND "Use Wayland" OFF)
cmake_dependent_option(BIG_WM_X11 "Use X11" ON
                        "BIG_PLATFORM_LINUX; NOT BIG_WM_WAYLAND" OFF)
                       
if(${BIG_WM_X11})
    set(BIG_COMPILE_DEFS
        SDL_VIDEO_DRIVER_X11=1
    )
elseif(${BIG_WM_WAYLAND})
    set(BIG_COMPILE_DEFS
        SDL_VIDEO_DRIVER_WAYLAND=1
    )
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ./**.cpp ./**.c)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ./**.hpp ./**.h)
file(GLOB_RECURSE VS_SHADERS CONFIGURE_DEPENDS vs_*.sc)
file(GLOB_RECURSE FS_SHADERS CONFIGURE_DEPENDS fs_*.sc)

# bgfx_compile_shaders appends the wgsl profile only when this variable is enabled.
set(BGFX_CONFIG_RENDERER_WEBGPU ${BIG_RENDERER_WEBGPU})

set(BIG_SHADER_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/platform/include
)

bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${VS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/platform/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)
bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${FS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/platform/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS} ${VS_SHADERS} ${FS_SHADERS})
add_library(blackboard::app ALIAS ${PROJECT_NAME})
find_package(X11)
target_link_libraries(${PROJECT_NAME}
    SDL3::SDL3
    bgfx
    bx
    glm
    ImGui
    spdlog
    bimg_decode
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${FETCHCONTENT_BASE_DIR}
    ${bgfx_cmake_SOURCE_DIR}/bx/inlcude
    ${SDL3_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/include/generated/shaders
)

if(WINDOWS)
target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${bgfx_cmake_SOURCE_DIR}/bx/include/compat/msvc
)
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    "-D_CRT_SECURE_NO_WARNINGS"
    "-D__STDC_FORMAT_MACROS"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    SDL_VIDEO_DRIVER_X11=1
    NOMINMAX
)

# target_compile_definitions(${PROJECT_NAME} PUBLIC "BX_CONFIG_DEBUG=$<CONFIG:Debug>")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${HEADERS} ${SOURCES})
