# Game logic that doesn't need a window, in a library so game_tests can link it.
add_library(game_logic STATIC
    src/controls.cpp
)
target_include_directories(game_logic PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(game_logic PUBLIC engine::engine PRIVATE engine::warnings)

add_executable(game src/main.cpp)
target_link_libraries(game PRIVATE game_logic engine::warnings)

set_target_properties(game PROPERTIES
    # Run from the game folder in the VS debugger so relative asset paths work.
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
