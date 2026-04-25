cmake_minimum_required(VERSION 3.21)

project(driver_sim C CXX)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ./**.cpp ./**.c)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ./**.hpp ./**.h)
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS ./assets/*)

file(GLOB_RECURSE FONTS CONFIGURE_DEPENDS ./assets/fonts/*.otf)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/generated/fonts")

file(GLOB_RECURSE IMAGES CONFIGURE_DEPENDS ./assets/images/*)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/generated/images")

foreach(FONT_FILE IN LISTS FONTS)
    get_filename_component(FONT_FILENAME ${FONT_FILE} NAME)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" FONT_FILENAME_REP ${FONT_FILENAME})

    bgfx_compile_binary_to_header(
        INPUT_FILE ${FONT_FILE}
        OUTPUT_FILE ${CMAKE_BINARY_DIR}/include/generated/fonts/${FONT_FILENAME}.h
        ARRAY_NAME ${FONT_FILENAME_REP}_bytes
    )
endforeach()

foreach(IMAGES_FILE IN LISTS IMAGES)
    get_filename_component(IMAGES_FILENAME ${IMAGES_FILE} NAME)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" IMAGES_FILENAME_REP ${IMAGES_FILENAME})

    bgfx_compile_binary_to_header(
        INPUT_FILE ${IMAGES_FILE}
        OUTPUT_FILE ${CMAKE_BINARY_DIR}/include/generated/images/${IMAGES_FILENAME}.h
        ARRAY_NAME ${IMAGES_FILENAME_REP}_bytes
    )
endforeach()

file(GLOB_RECURSE VS_SHADERS CONFIGURE_DEPENDS vs_*.sc)
file(GLOB_RECURSE FS_SHADERS CONFIGURE_DEPENDS fs_*.sc)
file(GLOB_RECURSE CS_SHADERS CONFIGURE_DEPENDS cs_*.sc)

# bgfx_compile_shaders appends the wgsl profile only when this variable is enabled.
set(BGFX_CONFIG_RENDERER_WEBGPU ${BIG_RENDERER_WEBGPU})

set(BIG_SHADER_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/blackboard_app/platform/include
)

bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${VS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/assets/shaders/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)
bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${FS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/assets/shaders/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)
bgfx_compile_shaders(
  TYPE COMPUTE
  SHADERS ${CS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/assets/shaders/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)

if(WINDOWS)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FONTS} ${IMAGES} ${VS_SHADERS} ${FS_SHADERS} ${CS_SHADERS})
elseif(MACOS)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${FONTS} ${IMAGES} ${VS_SHADERS} ${FS_SHADERS} ${CS_SHADERS})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE True
        RESOURCE True
        MACOSX_BUNDLE_GUI_IDENTIFIER ll.dashandslash.${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
)
elseif(LINUX)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FONTS} ${IMAGES})
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    blackboard::app
    nlohmann_json::nlohmann_json
    fastgltf::fastgltf
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${blackboard_app_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/include/generated/fonts
    ${CMAKE_BINARY_DIR}/include/generated/images
    ${CMAKE_BINARY_DIR}/include/generated/shaders
)

if(WINDOWS)
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${bgfx_SOURCE_DIR}/bx/include/compat/msvc
)
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    "-D_CRT_SECURE_NO_WARNINGS"
    "-D__STDC_FORMAT_MACROS"
    "-DSDL_MAIN_HANDLED"
)

# Create tree group to tidy the files inside IDEs

source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${HEADERS} ${SOURCES} ${ASSETS})
