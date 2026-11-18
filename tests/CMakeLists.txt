# Catch2 is fetched by the top-level CMakeLists.txt (the benchmarks use it too).

# Every test file, picked up automatically (CONFIGURE_DEPENDS re-globs on each build).
# Files under game/ test the game and go in game_tests. Of the rest, the ones that need the SDL
# platform library (listed below) go in engine_tests; everything else tests engine::core and goes
# in engine_core_tests, which links no SDL at all. That keeps engine::core honest: a test that
# needs SDL fails to link there.
file(GLOB_RECURSE TEST_HEADERS CONFIGURE_DEPENDS *.hpp)
file(GLOB_RECURSE ENGINE_TEST_SOURCES CONFIGURE_DEPENDS *_tests.cpp)
file(GLOB_RECURSE GAME_TEST_SOURCES CONFIGURE_DEPENDS game/*_tests.cpp)
list(REMOVE_ITEM ENGINE_TEST_SOURCES ${GAME_TEST_SOURCES})

# Tests of the platform library (paths: SDL_GetBasePath; debug panel: the renderer; sheet
# manifests: image files).
set(PLATFORM_TEST_NAMES
    core/paths_tests.cpp
    debug/debugpanel_tests.cpp
    graphics/sheetmanifest_tests.cpp)
set(PLATFORM_TEST_SOURCES ${PLATFORM_TEST_NAMES})
list(TRANSFORM PLATFORM_TEST_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
set(CORE_TEST_SOURCES ${ENGINE_TEST_SOURCES})
list(REMOVE_ITEM CORE_TEST_SOURCES ${PLATFORM_TEST_SOURCES})

add_executable(engine_core_tests ${CORE_TEST_SOURCES} ${TEST_HEADERS})
target_link_libraries(engine_core_tests PRIVATE engine::core engine::warnings Catch2::Catch2WithMain)
target_include_directories(engine_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(engine_tests ${PLATFORM_TEST_SOURCES} ${TEST_HEADERS})
target_link_libraries(engine_tests PRIVATE engine::platform engine::warnings Catch2::Catch2WithMain)
target_include_directories(engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    # C4868 (brace-init evaluation order) fires inside Catch2's assertion
    # templates, which are instantiated in our test files; not our code.
    target_compile_options(engine_core_tests PRIVATE /wd4868)
    target_compile_options(engine_tests PRIVATE /wd4868)
endif()
# Mirror the folder layout as IDE filters: core/, ecs/, graphics/, input/.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_TEST_SOURCES} ${TEST_HEADERS})

include(Catch)
catch_discover_tests(engine_core_tests)
catch_discover_tests(engine_tests)

# Tests for the game's own logic, kept apart from the engine tests. The game is an
# executable, so its sources (all but main.cpp) are compiled in here directly.
if(TARGET game)
    add_executable(game_tests ${GAME_TEST_SOURCES} ${GAME_TESTABLE_SOURCES})
    target_include_directories(game_tests PRIVATE ${GAME_INCLUDE_DIR})
    # The asset tests read the mazes from the source tree, as written.
    target_compile_definitions(game_tests PRIVATE GAME_ASSETS_DIR="${PROJECT_SOURCE_DIR}/game/assets")
    target_link_libraries(game_tests PRIVATE engine::platform engine::warnings Catch2::Catch2WithMain)
    if(MSVC)
        target_compile_options(game_tests PRIVATE /wd4868)
    endif()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/game PREFIX "tests" FILES ${GAME_TEST_SOURCES})
    source_group(TREE ${PROJECT_SOURCE_DIR}/game/src PREFIX "game" FILES ${GAME_TESTABLE_SOURCES})
    catch_discover_tests(game_tests)
endif()
