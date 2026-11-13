# Every game header and source, picked up automatically (CONFIGURE_DEPENDS re-globs on
# each build, so new files appear without editing this list).
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(game ${GAME_SOURCES} ${GAME_HEADERS})
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(game PRIVATE engine::engine engine::warnings)

# Mirror the folder layout as IDE filters: include/ and src/.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${GAME_SOURCES} ${GAME_HEADERS})

set_target_properties(game PROPERTIES
    # Run from the game folder in the VS debugger so relative asset paths work.
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# For game_tests (tests/CMakeLists.txt): the game sources minus main.cpp, compiled
# straight into the test executable.
set(GAME_TESTABLE_SOURCES ${GAME_SOURCES})
list(REMOVE_ITEM GAME_TESTABLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
set(GAME_TESTABLE_SOURCES ${GAME_TESTABLE_SOURCES} PARENT_SCOPE)
set(GAME_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
