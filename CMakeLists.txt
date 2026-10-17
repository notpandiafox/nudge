cmake_minimum_required(VERSION 3.20)
project(nudge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The WebAssembly build compiles only the library (and later the bindings):
# GoogleTest, the tests and the native tools are not built with Emscripten.
if(EMSCRIPTEN)
    set(nudge_build_tests_default OFF)
else()
    set(nudge_build_tests_default ON)
endif()
option(NUDGE_BUILD_TESTS "Build the tests" ${nudge_build_tests_default})
option(NUDGE_COVERAGE "Instrument the solver library for coverage (use a Debug build)" OFF)
option(NUDGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (on in CI)" OFF)

if(NUDGE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        URL_HASH SHA256=40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

add_subdirectory(solver)
