# Releasing the engine

Versions follow [Semantic Versioning](https://semver.org/). The single source of truth is
`project(EcsEngine VERSION x.y.z ...)` in the top-level `CMakeLists.txt`; the generated
`<core/version.hpp>` and the git tag are checked against it.

1. Move the `[Unreleased]` entries in `CHANGELOG.md` under a new `## [x.y.z] - YYYY-MM-DD`
   heading, and update the compare links at the bottom.
2. Set `VERSION x.y.z` in `CMakeLists.txt`.
3. Open a PR, wait for CI, and merge to `main`.
4. Tag the merge commit and push the tag:
   `git tag -a vx.y.z -m "vx.y.z" && git push origin vx.y.z`.
5. The `Release` workflow checks that the tag matches `CMakeLists.txt`, then creates a GitHub
   release whose notes are the changelog section for that version.

## Using a release from another project

```cmake
include(FetchContent)
FetchContent_Declare(ecs_engine
    GIT_REPOSITORY https://github.com/brian-md-rose/ecs-engine.git
    GIT_TAG        v0.1.0
    GIT_SUBMODULES thirdparty/SDL   # the engine builds its vendored SDL3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(ecs_engine)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE engine::platform)
```

A headless tool (a simulator, a data checker) can link `engine::core` instead. It has the ECS, data, input
types and draw batching, but no SDL, window or audio, and its build fails if it uses them.

As a subproject the engine builds only the library (`ENGINE_BUILD_GAME`, `ENGINE_BUILD_TESTS`
and `ENGINE_BUILD_BENCHMARKS` default to OFF). The engine's warning flags are applied to the
engine target only, not to your code. Log `engine::kVersionString` at runtime to record which version you built against.

### Hiding the console window (Windows)

An executable is a console app by default, so a console window opens beside the game. To build a
GUI app instead, mark the target `WIN32_EXECUTABLE` and include `<SDL3/SDL_main.h>` in the file
that defines `main`, so SDL supplies the `WinMain` entry point:

```cmake
# Keep the console in Debug so printf/std::cout output stays visible.
set_target_properties(my_game PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
```

```cpp
#include <SDL3/SDL_main.h>
```

Output to stdout/stderr goes nowhere once the console is gone, so make a `engine::core::LogSession`
(`core/log.hpp`) at the top of `main()` to log to a file instead; see `game/src/main.cpp`. It also
routes SDL's own log (`SDL_Log` and friends) into the file. The property has no effect on other
platforms. The game hides its console in `game/CMakeLists.txt`.
