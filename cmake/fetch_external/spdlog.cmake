cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as static" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Don't build examples" FORCE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY "git@github.com:gabime/spdlog.git"
    GIT_TAG v1.17.0
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/spdlog
)
FetchContent_MakeAvailable(spdlog)