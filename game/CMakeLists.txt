# Every game header and source, picked up automatically (CONFIGURE_DEPENDS re-globs on
# each build, so new files appear without editing this list).
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(game ${GAME_SOURCES} ${GAME_HEADERS})
# Hide the console window in every config except Debug (Windows only; ignored elsewhere). Debug
# keeps the console so stdout/stderr output stays visible. SDL_main.h in main.cpp supplies the
# WinMain entry point a GUI-subsystem executable needs.
set_target_properties(game PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# SDL3::SDL3 is linked directly only so main.cpp can include <SDL3/SDL_main.h>; the rest of the
# game code still goes through the engine and never touches SDL headers.
target_link_libraries(game PRIVATE engine::platform engine::warnings SDL3::SDL3)

# Mirror the folder layout as IDE filters: include/ and src/.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${GAME_SOURCES} ${GAME_HEADERS})

# Copy assets/ next to the executable, for every generator and config. The game finds them
# there (engine::core::ExecutableDirectory), so it doesn't depend on the working directory.
# A custom target with no outputs runs on every build, so an edited asset is copied even when
# the game itself is up to date; a POST_BUILD step would only run when the game relinks.
add_custom_target(game_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:game>/assets
    COMMENT "Copying game assets next to the executable"
    VERBATIM)
add_dependencies(game game_assets)

# For game_tests (tests/CMakeLists.txt): the game sources minus main.cpp, compiled
# straight into the test executable.
set(GAME_TESTABLE_SOURCES ${GAME_SOURCES})
list(REMOVE_ITEM GAME_TESTABLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
set(GAME_TESTABLE_SOURCES ${GAME_TESTABLE_SOURCES} PARENT_SCOPE)
set(GAME_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)

# The reference editor integration (ENGINE_BUILD_EDITOR): the game's sources again, with its own main.
if(ENGINE_BUILD_EDITOR)
    add_executable(tangle_run_editor ${GAME_TESTABLE_SOURCES} ${GAME_HEADERS} editor/main.cpp)
    set_target_properties(tangle_run_editor PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
    target_include_directories(tangle_run_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(tangle_run_editor PRIVATE engine::editor engine::warnings SDL3::SDL3)
    add_custom_target(tangle_run_editor_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:tangle_run_editor>/assets
        VERBATIM)
    add_dependencies(tangle_run_editor tangle_run_editor_assets)
    # The editor opens and saves scenes in the source folder, so edits persist and get committed (editor/main.cpp).
    target_compile_definitions(tangle_run_editor PRIVATE TANGLE_RUN_SOURCE_SCENES="${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes")
endif()
