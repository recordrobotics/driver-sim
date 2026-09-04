cmake_minimum_required(VERSION 3.25)

project(driver_sim C CXX)

include(../cmake/GitVersion.cmake)
get_version_from_git()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ./**.cpp ./**.c)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ./**.hpp ./**.h)
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS ./assets/*)

file(GLOB_RECURSE FONTS CONFIGURE_DEPENDS ./assets/fonts/*.otf ./assets/fonts/*.ttf)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/generated/fonts")

file(GLOB IMAGES_ALL CONFIGURE_DEPENDS ./assets/images/*)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/generated/images")

set(IMAGES)
foreach(entry IN LISTS IMAGES_ALL)
    if(NOT IS_DIRECTORY "${entry}")
        list(APPEND IMAGES "${entry}")
    endif()
endforeach()

file(GLOB_RECURSE GAME_IMAGES CONFIGURE_DEPENDS ./assets/images/rebuilt2026/*)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/generated/images/rebuilt2026")

file(GLOB_RECURSE PACKAGED_ASSETS CONFIGURE_DEPENDS ./packaged/*)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/generated/packaged")

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

foreach(GAME_IMAGES_FILE IN LISTS GAME_IMAGES)
    get_filename_component(GAME_IMAGES_FILENAME ${GAME_IMAGES_FILE} NAME)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" GAME_IMAGES_FILENAME_REP ${GAME_IMAGES_FILENAME})

    bgfx_compile_binary_to_header(
        INPUT_FILE ${GAME_IMAGES_FILE}
        OUTPUT_FILE ${CMAKE_BINARY_DIR}/include/generated/images/rebuilt2026/${GAME_IMAGES_FILENAME}.h
        ARRAY_NAME ${GAME_IMAGES_FILENAME_REP}_bytes
    )
endforeach()

foreach(PACKAGED_ASSET IN LISTS PACKAGED_ASSETS)
    get_filename_component(ASSET_FILENAME ${PACKAGED_ASSET} NAME)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" ASSET_FILENAME_REP ${ASSET_FILENAME})

    bgfx_compile_binary_to_header(
        INPUT_FILE ${PACKAGED_ASSET}
        OUTPUT_FILE ${CMAKE_BINARY_DIR}/include/generated/packaged/${ASSET_FILENAME}.h
        ARRAY_NAME ${ASSET_FILENAME_REP}_bytes
    )
endforeach()

file(GLOB_RECURSE VS_SHADERS CONFIGURE_DEPENDS vs_*.sc)
file(GLOB_RECURSE FS_SHADERS CONFIGURE_DEPENDS fs_*.sc)
file(GLOB_RECURSE CS_SHADERS CONFIGURE_DEPENDS cs_*.sc)

set(BIG_SHADER_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/blackboard_app/platform/include
)

set(SHADER_PROFILES spirv 430)

if(IOS)
    list(APPEND SHADER_PROFILES metal)
elseif(APPLE)
    list(APPEND SHADER_PROFILES metal)
elseif(
    WIN32
    OR MINGW
    OR MSYS
    OR CYGWIN
)
    list(APPEND SHADER_PROFILES s_5_0 s_6_0)
elseif(ORBIS) # ORBIS should be defined by a PS4 CMake toolchain
    list(APPEND SHADER_PROFILES pssl)
else()
    # pssl for Agc and Gnm renderers
    # nvn for Nvn renderer
    message(error "shaderc: Unsupported platform")
endif()

bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${VS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/assets/shaders/common/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  PROFILES ${SHADER_PROFILES}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)
bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${FS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/assets/shaders/common/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  PROFILES ${SHADER_PROFILES}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)
bgfx_compile_shaders(
  TYPE COMPUTE
  SHADERS ${CS_SHADERS}
    VARYING_DEF ${CMAKE_CURRENT_LIST_DIR}/assets/shaders/common/varying.def.sc
    INCLUDE_DIRS ${BIG_SHADER_INCLUDE_DIRS}
  PROFILES ${SHADER_PROFILES}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  AS_HEADERS
)

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/version.h.in
    ${CMAKE_BINARY_DIR}/include/generated/templates/version.h
)

if(WINDOWS)
    add_executable(${PROJECT_NAME} WIN32 
            ${SOURCES} ${HEADERS}
            ${FONTS} ${IMAGES} ${GAME_IMAGES}
            ${VS_SHADERS} ${FS_SHADERS} ${CS_SHADERS}
            ${PACKAGED_ASSETS}
            ${CMAKE_CURRENT_LIST_DIR}/assets/app.rc
            ${CMAKE_CURRENT_LIST_DIR}/assets/segmentheap.manifest)
elseif(MACOS)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${FONTS} ${IMAGES} ${GAME_IMAGES} ${VS_SHADERS} ${FS_SHADERS} ${CS_SHADERS} ${PACKAGED_ASSETS})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE True
        RESOURCE True
        MACOSX_BUNDLE_GUI_IDENTIFIER org.recordrobotics.driversim
        MACOSX_BUNDLE_BUNDLE_NAME driversim
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)
elseif(LINUX)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FONTS} ${IMAGES} ${GAME_IMAGES} ${VS_SHADERS} ${FS_SHADERS} ${CS_SHADERS} ${PACKAGED_ASSETS})
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    blackboard::app
    nlohmann_json::nlohmann_json
    fastgltf::fastgltf
    picosha2
    cpr::cpr
    miniz
    tiny-process-library
    ntcore
    wpiutil
    wpimath
    argparse
    fmt::fmt
    discordsdk::discordsdk
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${blackboard_app_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/include/generated/fonts
    ${CMAKE_BINARY_DIR}/include/generated/images
    ${CMAKE_BINARY_DIR}/include/generated/shaders
    ${CMAKE_BINARY_DIR}/include/generated/packaged
    ${CMAKE_BINARY_DIR}/include/generated/templates
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

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    NOMINMAX
)

# Create tree group to tidy the files inside IDEs

source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${HEADERS} ${SOURCES} ${ASSETS})
