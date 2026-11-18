# Two libraries (see docs/babel-support.md, PR 11.0):
#
#   engine::core    The ECS, data, text, input types, draw batching and everything else that needs no
#                   platform. It does NOT link SDL and has no SDL include path, so an SDL include there
#                   fails the build. Headless tools (a simulator) link just this.
#   engine::engine  The platform: SDL window, renderer, audio device, image files, the application loop
#                   and the debug panel. It links engine::core publicly, so existing code that links
#                   engine::engine keeps working.
#
# Headers all live in include/ and are shared; only the sources are split.

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS include/*.hpp)

# The sources that need SDL (or the code built on them). Everything else under src/ is core.
# Paths are relative to src/.
set(ENGINE_PLATFORM_SOURCE_NAMES
    application/application.cpp
    audio/audiodevice.cpp
    core/paths.cpp
    core/sdllog.cpp
    debug/debugpanel.cpp
    graphics/imagefile.cpp
    graphics/renderer.cpp
    graphics/renderer2d.cpp
    graphics/sheetmanifest.cpp
    graphics/window.cpp)

# Every source, picked up automatically (CONFIGURE_DEPENDS re-globs on each build, so new files appear
# without editing this list; a new file is core unless it is added to the platform list above).
file(GLOB_RECURSE ENGINE_ALL_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src src/*.cpp)
set(ENGINE_CORE_SOURCES ${ENGINE_ALL_SOURCES})
list(REMOVE_ITEM ENGINE_CORE_SOURCES ${ENGINE_PLATFORM_SOURCE_NAMES})
list(TRANSFORM ENGINE_CORE_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(ENGINE_PLATFORM_SOURCES ${ENGINE_PLATFORM_SOURCE_NAMES})
list(TRANSFORM ENGINE_PLATFORM_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)
foreach(platform_source IN LISTS ENGINE_PLATFORM_SOURCES)
    if(NOT EXISTS ${platform_source})
        message(FATAL_ERROR "engine/CMakeLists.txt lists ${platform_source} as a platform source, but it doesn't exist.")
    endif()
endforeach()

# ---- engine::core ----
add_library(engine_core STATIC ${ENGINE_CORE_SOURCES} ${ENGINE_HEADERS})
add_library(engine::core ALIAS engine_core)

# ENGINE_ASSERT is active in Debug builds only. PUBLIC so every target that
# includes engine headers sees the same setting.
target_compile_definitions(engine_core PUBLIC $<$<CONFIG:Debug>:ENGINE_ENABLE_ASSERTS>)
target_include_directories(engine_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(engine_core PUBLIC cxx_std_20)
target_link_libraries(engine_core PRIVATE engine::warnings)

# Generated version header (<core/version.hpp>), filled in from project(... VERSION ...).
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/core/version.hpp
    @ONLY)
target_include_directories(engine_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)
set_target_properties(engine_core PROPERTIES VERSION ${PROJECT_VERSION})

# ---- engine::engine (the platform) ----
add_library(engine STATIC ${ENGINE_PLATFORM_SOURCES})
add_library(engine::engine ALIAS engine)
target_link_libraries(engine PUBLIC engine::core)
# SDL3 is PRIVATE: only the platform .cpp files may include SDL headers (the game links SDL3 itself
# just for SDL_main.h in main.cpp). The game still links SDL through the engine, since engine is a
# static library.
target_link_libraries(engine PRIVATE engine::warnings SDL3::SDL3)
set_target_properties(engine PROPERTIES VERSION ${PROJECT_VERSION})

# Mirror the folder layout as IDE filters: include/ and src/.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_CORE_SOURCES} ${ENGINE_PLATFORM_SOURCES} ${ENGINE_HEADERS})
