include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(engine_tests
    core/fixedtimestep_tests.cpp
    ecs/registry_tests.cpp
    ecs/componentpool_tests.cpp
    ecs/commandbuffer_tests.cpp
    ecs/entity_tests.cpp
    graphics/components_tests.cpp
    graphics/shapebatch_tests.cpp
    input/keyboard_tests.cpp
)
target_link_libraries(engine_tests PRIVATE engine::engine engine::warnings Catch2::Catch2WithMain)
target_include_directories(engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    # C4868 (brace-init evaluation order) fires inside Catch2's assertion
    # templates, which are instantiated in our test files; not our code.
    target_compile_options(engine_tests PRIVATE /wd4868)
endif()

include(Catch)
catch_discover_tests(engine_tests)

# Tests for the game's own logic, kept apart from the engine tests. The game is an
# executable, so its sources (all but main.cpp) are compiled in here directly.
if(TARGET game)
    add_executable(game_tests
        game/controls_tests.cpp
        ${GAME_TESTABLE_SOURCES}
    )
    target_include_directories(game_tests PRIVATE ${GAME_INCLUDE_DIR})
    target_link_libraries(game_tests PRIVATE engine::engine engine::warnings Catch2::Catch2WithMain)
    if(MSVC)
        target_compile_options(game_tests PRIVATE /wd4868)
    endif()
    catch_discover_tests(game_tests)
endif()
