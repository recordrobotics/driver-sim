cmake_minimum_required(VERSION 3.25)

set(PROJECT_NAME blackboard_app)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_SKIP_INSTALL_RULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(UNIX AND NOT APPLE)
	set(LINUX TRUE)
elseif(UNIX AND APPLE)
	set(MACOS TRUE)
elseif(WIN32)
	set(WINDOWS TRUE)
endif()

if(WINDOWS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	project (${PROJECT_NAME} C CXX)
elseif(MACOS)
	project (${PROJECT_NAME} C CXX OBJC)
elseif(LINUX)
	project (${PROJECT_NAME} C CXX)
endif()

include(cmake/fetch_external_dependencies.cmake)
	
add_subdirectory(blackboard_app)
add_subdirectory(driver_sim)

file(GLOB_RECURSE ALL_SOURCE_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/blackboard_app/*.cpp
    ${CMAKE_SOURCE_DIR}/blackboard_app/**/*.h
    ${CMAKE_SOURCE_DIR}/blackboard_app/**/*.hpp

    ${CMAKE_SOURCE_DIR}/driver_sim/*.cpp
    ${CMAKE_SOURCE_DIR}/driver_sim/**/*.h
    ${CMAKE_SOURCE_DIR}/driver_sim/**/*.hpp
)

# clang-format
find_program(CLANG_FORMAT clang-format)

if(CLANG_FORMAT)
    message(STATUS "Found clang-format: ${CLANG_FORMAT}")

    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format"
    )
else()
    message(WARNING "clang-format not found. 'format' target disabled.")
endif()

# clang-tidy
find_program(CLANG_TIDY clang-tidy)

if(CLANG_TIDY)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY}")
	
	set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")

    add_custom_target(tidy
        COMMAND ${CLANG_TIDY}
            -p ${CMAKE_BINARY_DIR}
            ${ALL_SOURCE_FILES}
        COMMENT "Running clang-tidy"
    )
else()
    message(WARNING "clang-tidy not found. 'tidy' target disabled.")
endif()
