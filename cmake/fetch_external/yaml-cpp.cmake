cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS  OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        master
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/yaml-cpp
)
FetchContent_MakeAvailable(yaml-cpp)