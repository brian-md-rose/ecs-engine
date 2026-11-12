include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(engine_tests
    ecs/registry_tests.cpp
    ecs/componentpool_tests.cpp
    ecs/commandbuffer_tests.cpp
    ecs/entity_tests.cpp
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
