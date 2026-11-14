include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

# Every test file, picked up automatically (CONFIGURE_DEPENDS re-globs on each build).
# Files under game/ test the game and go in game_tests; everything else tests the engine.
file(GLOB_RECURSE TEST_HEADERS CONFIGURE_DEPENDS *.hpp)
file(GLOB_RECURSE ENGINE_TEST_SOURCES CONFIGURE_DEPENDS *_tests.cpp)
file(GLOB_RECURSE GAME_TEST_SOURCES CONFIGURE_DEPENDS game/*_tests.cpp)
list(REMOVE_ITEM ENGINE_TEST_SOURCES ${GAME_TEST_SOURCES})

add_executable(engine_tests ${ENGINE_TEST_SOURCES} ${TEST_HEADERS})
target_link_libraries(engine_tests PRIVATE engine::engine engine::warnings Catch2::Catch2WithMain)
target_include_directories(engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    # C4868 (brace-init evaluation order) fires inside Catch2's assertion
    # templates, which are instantiated in our test files; not our code.
    target_compile_options(engine_tests PRIVATE /wd4868)
endif()
# Mirror the folder layout as IDE filters: core/, ecs/, graphics/, input/.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_TEST_SOURCES} ${TEST_HEADERS})

include(Catch)
catch_discover_tests(engine_tests)

# Tests for the game's own logic, kept apart from the engine tests. The game is an
# executable, so its sources (all but main.cpp) are compiled in here directly.
if(TARGET game)
    add_executable(game_tests ${GAME_TEST_SOURCES} ${GAME_TESTABLE_SOURCES})
    target_include_directories(game_tests PRIVATE ${GAME_INCLUDE_DIR})
    target_link_libraries(game_tests PRIVATE engine::engine engine::warnings Catch2::Catch2WithMain)
    if(MSVC)
        target_compile_options(game_tests PRIVATE /wd4868)
    endif()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/game PREFIX "tests" FILES ${GAME_TEST_SOURCES})
    source_group(TREE ${PROJECT_SOURCE_DIR}/game/src PREFIX "game" FILES ${GAME_TESTABLE_SOURCES})
    catch_discover_tests(game_tests)
endif()
