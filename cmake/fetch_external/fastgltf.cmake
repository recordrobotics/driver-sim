cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(FASTGLTF_BUILD_SHARED_LIBS OFF CACHE BOOL "Build fastgltf as static" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES  OFF CACHE BOOL "Don't build examples" FORCE)
set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG main
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/fastgltf
)

set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(fastgltf)