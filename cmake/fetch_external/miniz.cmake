cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

set( BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set( BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set ( INSTALL_PROJECT OFF CACHE INTERNAL "" FORCE)

FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        2.2.0
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/miniz
)
FetchContent_GetProperties(miniz)
if(NOT miniz_POPULATED)
    FetchContent_Populate(miniz)
endif()

# Patch cmakelists
set(_cmakelists_file "${miniz_SOURCE_DIR}/CMakeLists.txt")

if(EXISTS "${_cmakelists_file}")
    file(READ "${_cmakelists_file}" _cmakelists_contents)

    # Replace min version
    string(REPLACE
"cmake_minimum_required(VERSION 3.0)"
"cmake_minimum_required(VERSION 3.5)"
        _cmakelists_contents
        "${_cmakelists_contents}"
    )

    file(WRITE "${_cmakelists_file}" "${_cmakelists_contents}")
endif()

add_subdirectory("${miniz_SOURCE_DIR}" "${miniz_BINARY_DIR}")