cmake_minimum_required(VERSION 3.25)

project(EcsEngine
    VERSION 0.1.0
    DESCRIPTION "A learning-focused 2D game engine built on a hand-written ECS"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset, e.g. `cmake --preset msvc`.")
endif()

option(ENGINE_BUILD_GAME  "Build the game executable" ON)
option(ENGINE_BUILD_TESTS "Build the engine tests"    ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Power of Ten rule 10: every warning on, warnings are errors, and the MSVC
# static analyzer runs on every build. Applied to all of our own targets.
add_library(engine_warnings INTERFACE)
add_library(engine::warnings ALIAS engine_warnings)
if(MSVC)
    target_compile_options(engine_warnings INTERFACE
        /Wall /WX /permissive- /Zc:__cplusplus /utf-8
        /analyze /analyze:external-
        # Standard-library and third-party headers (always included with <>)
        # are not ours to fix; our own headers are included with "".
        /external:anglebrackets /external:W0
        # /Wall also emits purely informational notes that don't indicate a
        # defect in the code: inlining decisions (4710, 4711), unreferenced
        # inline functions removed (4514), struct padding added (4820), and
        # Spectre mitigation insertion points (5045).
        /wd4514 /wd4710 /wd4711 /wd4820 /wd5045)
else()
    target_compile_options(engine_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror)
endif()

add_subdirectory(engine)

if(ENGINE_BUILD_GAME)
    add_subdirectory(game)
    # Make F5 in the generated Visual Studio solution launch the game.
    set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT game)
endif()

if(ENGINE_BUILD_TESTS)
    if(NOT EXISTS ${PROJECT_SOURCE_DIR}/tests/CMakeLists.txt)
        message(FATAL_ERROR "ENGINE_BUILD_TESTS is ON but tests/CMakeLists.txt is missing.")
    endif()
    enable_testing()
    add_subdirectory(tests)
endif()
