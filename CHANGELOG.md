# Changelog

All notable changes to the engine are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). While the major version is 0, minor releases
may contain breaking changes.

To cut a release, see [docs/releasing.md](docs/releasing.md).

## [Unreleased]

### Added

- `core/log.hpp`: `LogSession` and `Log`/`LogInfo`/`LogWarning`/`LogError` write timestamped, levelled
  messages to a log file (the previous run's kept as `log.old.txt`) and, in Debug builds, the console.
  SDL's log is routed through it. The game logs to `log.txt` next to its save file.
- `core/json.hpp`: `JsonValue`, `ParseJson`, `LoadJson`, `FormatJson` and `WriteJson`. Objects keep their key
  order, integers are stored as `int64_t` apart from doubles, parse errors give a line and column, and nesting
  is capped at 64 levels. Phase 10 of [docs/babel-support.md](docs/babel-support.md).
- `core/files.hpp`: `WriteFileAtomic`, the temporary-file-then-rename write that `WriteSaveData` and
  `WriteJson` share.
- `core/random.hpp`: `Random`, a PCG32 generator that gives the same numbers on MSVC, GCC and Clang (no `<random>`
  distributions). `Range` has no modulo bias, and there are `Unit`, `Chance`, `Shuffle`, `PickWeighted`, `Fork(name)`
  for independent named streams, and `State()` for saving a run mid-way.
- `engine::core`: the SDL-free part of the engine (ECS, `core/`, input types, draw batching, fonts, sheets), for
  headless tools. It has no SDL include path, so using SDL there fails the build. `engine::engine` links it
  publicly and adds the platform, so code that links `engine::engine` is unchanged. Tests split the same way:
  `engine_core_tests` (no SDL) and `engine_tests` (platform).
- `core/sdllog.hpp`: `RouteSdlLog()` sends SDL's log through `core::Log`.

### Changed

- `LogSession` no longer installs the SDL log routing itself (it is SDL-free now). Call `core::RouteSdlLog()` after
  making one; `Application` also does. `Log` timestamps now come from `std::chrono`.
- The engine's and the game's error messages go through the log instead of straight to stderr.

## [0.1.0] - 2026-11-18

First release. The engine as of roadmap phases 0 to 9: sparse-set ECS, scheduler with states, event bus,
SDL3 window, input and audio, batched 2D renderer, and debug panel.

### Added

- Engine version in `project(EcsEngine VERSION ...)`, exposed to code as `<core/version.hpp>`
  (`engine::kVersionString` and friends).
- The engine can be consumed from another CMake project with `FetchContent` pinned to a tag;
  the game, tests and benchmarks are not built when the engine is a subproject.
- Release workflow that publishes a GitHub release when a `vX.Y.Z` tag is pushed.

[Unreleased]: https://github.com/brian-md-rose/ecs-engine/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/brian-md-rose/ecs-engine/releases/tag/v0.1.0
