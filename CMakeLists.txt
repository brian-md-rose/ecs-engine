cmake_minimum_required(VERSION 3.25)

project(EcsEngine
    VERSION 0.8.0
    DESCRIPTION "A learning-focused 2D game engine built on a hand-written ECS"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset, e.g. `cmake --preset msvc`.")
endif()

# When another project pulls this one in (FetchContent or add_subdirectory), build only the
# engine library: the game, tests and benchmarks default to OFF.
if(PROJECT_IS_TOP_LEVEL)
    set(ENGINE_IS_TOP_LEVEL ON)
else()
    set(ENGINE_IS_TOP_LEVEL OFF)
endif()
option(ENGINE_BUILD_GAME  "Build the game executable" ${ENGINE_IS_TOP_LEVEL})
option(ENGINE_BUILD_TESTS "Build the engine tests"    ${ENGINE_IS_TOP_LEVEL})
option(ENGINE_BUILD_BENCHMARKS "Build the ECS benchmarks" ${ENGINE_IS_TOP_LEVEL})
option(ENGINE_BUILD_EDITOR "Build the Dear ImGui editor overlay and tangle_run_editor (needs thirdparty/imgui)" OFF)

if(ENGINE_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

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
        # Layout of a lambda that captures a member pointer "may have changed from a previous compiler
        # version" (4371): purely informational, and the type registry captures one per field.
        /wd4371
        # Copy/move constructor or assignment implicitly deleted (4625, 4626,
        # 5026, 5027): fires for every move-only type (e.g. anything holding a
        # std::unique_ptr), which is intentional, not a defect.
        /wd4625 /wd4626 /wd5026 /wd5027)
else()
    target_compile_options(engine_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror)
endif()

# AddressSanitizer + UndefinedBehaviorSanitizer for GCC/Clang. Applied only to our own
# targets (they all link engine::warnings), not to SDL. Any finding aborts the run, so the
# tests fail instead of just printing a report.
option(ENGINE_SANITIZE "Build our targets with ASan and UBSan (GCC/Clang only)" OFF)
if(ENGINE_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "ENGINE_SANITIZE is for GCC/Clang; use the ninja-asan preset.")
    endif()
    target_compile_options(engine_warnings INTERFACE
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(engine_warnings INTERFACE -fsanitize=address,undefined)
endif()

# clang-tidy reads this to know how each file is compiled.
if(ENGINE_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Visual Studio compiles the files of one project one at a time unless /MP is set.
# Applied before SDL is added so its ~400 C files build in parallel too. Ninja already
# runs one compiler per core, so this is for the Visual Studio generator only.
if(ENGINE_IS_TOP_LEVEL AND CMAKE_GENERATOR MATCHES "Visual Studio")
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

# Ogg Vorbis decoding (one vendored C file, built without the engine's warning flags).
add_subdirectory(thirdparty/stb_vorbis)

add_subdirectory(engine)

if(ENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(ENGINE_BUILD_GAME)
    add_subdirectory(game)
    # Make F5 in the generated Visual Studio solution launch the game.
    set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT game)
endif()

# Catch2 runs both the tests and the benchmarks.
if(ENGINE_BUILD_TESTS OR ENGINE_BUILD_BENCHMARKS)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
        SYSTEM)
    FetchContent_MakeAvailable(Catch2)
endif()

if(ENGINE_BUILD_TESTS)
    if(NOT EXISTS ${PROJECT_SOURCE_DIR}/tests/CMakeLists.txt)
        message(FATAL_ERROR "ENGINE_BUILD_TESTS is ON but tests/CMakeLists.txt is missing.")
    endif()
    enable_testing()
    add_subdirectory(tests)
endif()


if(ENGINE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Group the projects of the generated Visual Studio solution into solution folders
# (other generators ignore this). Done last so every target already exists.
if(ENGINE_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakePredefinedTargets")

    # Every target created under `dir`, including its subdirectories.
    function(engine_set_folder_recursive dir folder)
        get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
        foreach(subdir IN LISTS subdirs)
            engine_set_folder_recursive(${subdir} ${folder})
        endforeach()
        get_property(dir_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(dir_target IN LISTS dir_targets)
            get_target_property(type ${dir_target} TYPE)
            if(NOT type STREQUAL "INTERFACE_LIBRARY")
                set_target_properties(${dir_target} PROPERTIES FOLDER "${folder}")
            endif()
        endforeach()
    endfunction()

    engine_set_folder_recursive(${PROJECT_SOURCE_DIR}/thirdparty/SDL "ThirdParty/SDL")
    engine_set_folder_recursive(${PROJECT_SOURCE_DIR}/thirdparty/stb_vorbis "ThirdParty")
    if(ENGINE_BUILD_EDITOR)
        engine_set_folder_recursive(${PROJECT_SOURCE_DIR}/editor "Engine")
        if(TARGET imgui)
            set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")
        endif()
    endif()
    engine_set_folder_recursive(${PROJECT_SOURCE_DIR}/engine "Engine")
    if(ENGINE_BUILD_GAME)
        engine_set_folder_recursive(${PROJECT_SOURCE_DIR}/game "Game")
    endif()
    if(ENGINE_BUILD_TESTS)
        engine_set_folder_recursive(${PROJECT_SOURCE_DIR}/tests "Tests")
    endif()
    if(ENGINE_BUILD_BENCHMARKS)
        engine_set_folder_recursive(${PROJECT_SOURCE_DIR}/benchmarks "Benchmarks")
    endif()
    if(catch2_SOURCE_DIR)
        engine_set_folder_recursive(${catch2_SOURCE_DIR} "ThirdParty/Catch2")
    endif()
endif()
