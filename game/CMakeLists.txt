add_executable(game src/main.cpp)
target_link_libraries(game PRIVATE engine::engine engine::warnings)

set_target_properties(game PROPERTIES
    # Run from the game folder in the VS debugger so relative asset paths work.
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
