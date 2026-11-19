# engine::editor: a Dear ImGui overlay for a running game (docs/editor.md). Only built when
# ENGINE_BUILD_EDITOR is ON (it is OFF by default), so a game that doesn't want it never compiles it.

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/thirdparty/imgui/imgui.h)
    message(FATAL_ERROR "thirdparty/imgui is empty. Run: git submodule update --init thirdparty/imgui")
endif()

# Dear ImGui and its SDL3 and SDL_Renderer backends, someone else's code, so no engine warning flags.
set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/thirdparty/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
add_library(imgui::imgui ALIAS imgui)
target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_features(imgui PUBLIC cxx_std_20)
# Dear ImGui writes no imgui.ini and needs no old-style key-index API.
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
endif()

file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(engine_editor STATIC ${EDITOR_SOURCES} ${EDITOR_HEADERS})
add_library(engine::editor ALIAS engine_editor)
target_include_directories(engine_editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Games write panels against ImGui, so it is public. SDL is only used inside the editor's own sources.
target_link_libraries(engine_editor PUBLIC engine::platform imgui::imgui PRIVATE engine::warnings)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SOURCES} ${EDITOR_HEADERS})
