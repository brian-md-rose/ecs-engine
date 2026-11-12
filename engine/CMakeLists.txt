add_library(engine STATIC
    src/core/assert.cpp
    src/ecs/registry.cpp
    src/graphics/renderer.cpp
)
add_library(engine::engine ALIAS engine)

# ENGINE_ASSERT is active in Debug builds only. PUBLIC so every target that
# includes engine headers sees the same setting.
target_compile_definitions(engine PUBLIC $<$<CONFIG:Debug>:ENGINE_ENABLE_ASSERTS>)

target_include_directories(engine PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(engine PUBLIC cxx_std_20)
# SDL3 is PRIVATE: only engine .cpp files may include SDL headers. The game
# still links SDL through the engine, since engine is a static library.
target_link_libraries(engine PRIVATE engine::warnings SDL3::SDL3)

# Show the headers in IDE project trees alongside the sources.
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS include/*.hpp)
target_sources(engine PRIVATE ${ENGINE_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_HEADERS})
