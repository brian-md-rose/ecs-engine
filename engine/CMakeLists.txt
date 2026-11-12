add_library(engine STATIC
    src/ecs/componentpool.cpp
    src/ecs/registry.cpp
)
add_library(engine::engine ALIAS engine)

target_include_directories(engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(engine PUBLIC cxx_std_20)
target_link_libraries(engine PRIVATE engine::warnings)

# Show the headers in IDE project trees alongside the sources.
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS include/*.hpp)
target_sources(engine PRIVATE ${ENGINE_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_HEADERS})
