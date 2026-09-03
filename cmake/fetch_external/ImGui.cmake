cmake_minimum_required(VERSION 3.25)

include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b-docking
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/imgui
)
FetchContent_MakeAvailable(imgui)

set(imgui_SOURCES
   ${imgui_SOURCE_DIR}/imgui.cpp
   ${imgui_SOURCE_DIR}/imgui_demo.cpp
   ${imgui_SOURCE_DIR}/imgui_draw.cpp
   ${imgui_SOURCE_DIR}/imgui_tables.cpp
   ${imgui_SOURCE_DIR}/imgui_widgets.cpp
   ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
   ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    # ${imgui_color_text_edit_SOURCE_DIR}/TextEditor.cpp
)

set(imgui_HEADERS
    # ${CMAKE_CURRENT_SOURCE_DIR}/blackboard_core/gui/imconfig.h
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imstb_rectpack.h
    ${imgui_SOURCE_DIR}/imstb_truetype.h
    ${imgui_SOURCE_DIR}/imgui_internal.h
    ${imgui_SOURCE_DIR}/imstb_textedit.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.h
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h
    # ${imgui_color_text_edit_SOURCE_DIR}/TextEditor.h
)

add_library(imgui ${imgui_HEADERS} ${imgui_SOURCES})

target_include_directories(imgui
    PUBLIC
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
    # $<BUILD_INTERFACE:${FETCHCONTENT_BASE_DIR}/SDL/include>
)

target_link_libraries(imgui
    PUBLIC
    SDL3::SDL3
)

target_compile_definitions(imgui
    PUBLIC
    IMGUI_DISABLE_DEFAULT_FONT
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)
