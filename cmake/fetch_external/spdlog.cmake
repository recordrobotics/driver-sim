cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as static" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Don't build examples" FORCE)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt" FORCE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/spdlog
    SYSTEM
)
FetchContent_MakeAvailable(spdlog)