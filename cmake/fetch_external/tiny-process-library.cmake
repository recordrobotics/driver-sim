cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
    tiny-process-library
    GIT_REPOSITORY https://gitlab.com/eidheim/tiny-process-library.git
    GIT_TAG        master
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/tiny-process-library
)
FetchContent_MakeAvailable(tiny-process-library)

target_compile_definitions(tiny-process-library PRIVATE WIN32_LEAN_AND_MEAN)