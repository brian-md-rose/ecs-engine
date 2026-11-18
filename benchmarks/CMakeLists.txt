# ECS benchmarks (Catch2's BENCHMARK). Not registered with CTest: timings are only meaningful
# in an optimized build, run on their own. See the ninja-bench and bench presets.
add_executable(engine_benchmarks ecs_benchmarks.cpp)
target_link_libraries(engine_benchmarks PRIVATE engine::platform engine::warnings Catch2::Catch2WithMain)
if(MSVC)
    # C4868 (brace-init evaluation order) fires inside Catch2's templates; not our code.
    target_compile_options(engine_benchmarks PRIVATE /wd4868)
endif()
