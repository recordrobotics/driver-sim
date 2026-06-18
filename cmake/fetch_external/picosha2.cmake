cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
    picosha2
    GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
    GIT_TAG        v1.0.1
)
FetchContent_MakeAvailable(picosha2)