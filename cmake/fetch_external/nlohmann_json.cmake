cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW 1
    SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/nlohmann_json
)
FetchContent_MakeAvailable(nlohmann_json)