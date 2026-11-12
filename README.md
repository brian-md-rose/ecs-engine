# ecs-engine

[![CI](https://github.com/brian-md-rose/ecs-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/brian-md-rose/ecs-engine/actions/workflows/ci.yml)

A learning-focused 2D game engine built on a hand-written Entity Component System.

## Layout

```
engine/   static library (engine::engine)
  include/ecs/   Entity, Component concept, ComponentPool (sparse set), Registry
  src/ecs/
game/     sample executable that links the engine
tests/    Catch2 unit tests (fetched automatically at configure time)
```

## Building

Requires CMake 3.25+ and a C++20 compiler.

**Windows (Visual Studio 2026):**

```bash
cmake --preset msvc
cmake --build --preset debug
ctest --preset debug
```

Open `build/msvc/EcsEngine.slnx` in Visual Studio to debug; `game` is the startup project.

**Linux / macOS (Ninja):**

```bash
cmake --preset ninja-clang
cmake --build --preset ninja-clang
ctest --preset ninja-clang
```

Pass `-DENGINE_BUILD_TESTS=OFF` to skip the tests (and the Catch2 download).

All targets build with every warning enabled and warnings as errors (`/Wall /WX /analyze` on MSVC), so new warnings fail the build.

## License

MIT, see [LICENSE](LICENSE).
