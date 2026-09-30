cmake_minimum_required(VERSION 3.20)
project(CodeQuizzio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CQ_BUILD_TESTS "Build unit tests" ON)
option(CQ_BUILD_UI "Build the Qt desktop app" ON)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(core)
add_subdirectory(console)

if(CQ_BUILD_TESTS)
    enable_testing()
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
endif()

# Needs GoogleTest (above) for its controller tests, so it comes last.
if(CQ_BUILD_UI)
    find_package(Qt6 6.5 QUIET COMPONENTS Quick QuickControls2 Test)
    if(Qt6_FOUND)
        add_subdirectory(ui)
    else()
        message(WARNING "Qt 6 not found - skipping the desktop app. "
                        "Pass -DCMAKE_PREFIX_PATH=<Qt>/msvc2022_64 to build it.")
    endif()
endif()
