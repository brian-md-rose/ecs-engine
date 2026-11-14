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
        /wd4514 /wd4710 /wd4711 /wd4820 /wd5045
        # Copy/move constructor or assignment implicitly deleted (4625, 4626,
        # 5026, 5027): fires for every move-only type (e.g. anything holding a
        # std::unique_ptr), which is intentional, not a defect.
        /wd4625 /wd4626 /wd5026 /wd5027)
else()
    target_compile_options(engine_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror)
endif()

# Visual Studio compiles the files of one project one at a time unless /MP is set.
# Applied before SDL is added so its ~400 C files build in parallel too. Ninja already
# runs one compiler per core, so this is for the Visual Studio generator only.
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options($<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:/MP>)
endif()

# SDL3, vendored as a git submodule. Static, so there's no SDL3.dll to copy
# next to the game. SYSTEM keeps our /Wall /WX off SDL's headers;
# EXCLUDE_FROM_ALL builds only the SDL targets we link.
if(NOT EXISTS ${PROJECT_SOURCE_DIR}/thirdparty/SDL/CMakeLists.txt)
    message(FATAL_ERROR "thirdparty/SDL is empty. Run: git submodule update --init")
endif()
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST_LIBRARY OFF)
add_subdirectory(thirdparty/SDL EXCLUDE_FROM_ALL SYSTEM)

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
