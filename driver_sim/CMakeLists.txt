cmake_minimum_required(VERSION 3.21)

project(driver_sim C CXX)

file(GLOB_RECURSE SOURCES ./**.cpp ./**.c)
file(GLOB_RECURSE HEADERS ./**.hpp ./**.h)
file(GLOB_RECURSE ASSETS ./assets/*)

file(GLOB_RECURSE FONTS ./assets/fonts/*.otf)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/generated/fonts")

file(GLOB_RECURSE IMAGES ./assets/images/*)
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

if(WINDOWS)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FONTS} ${IMAGES})
elseif(MACOS)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${FONTS} ${IMAGES})
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
    bimg_decode
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${blackboard_app_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/include/generated/fonts
    ${CMAKE_BINARY_DIR}/include/generated/images
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
