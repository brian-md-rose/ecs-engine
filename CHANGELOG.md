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
  headless tools. It has no SDL include path, so using SDL there fails the build. The platform (SDL window, renderer, audio, image files,
  application loop, debug panel) is `engine::platform`, which links `engine::core` publicly; a game links just that. Tests split the same way:
  `engine_core_tests` (no SDL) and `engine_tests` (platform).
- `core/sdllog.hpp`: `RouteSdlLog()` sends SDL's log through `core::Log`.
- `input/input.hpp`: `input::Input`, everything the player does in one update (the keyboard, and `typed`, the UTF-8 text
  typed since the last update, for name fields). `Application` owns one and fills it from SDL; tests build one by hand.
- `input::Key` now has the keys a PC game needs: A to Z, `Num0` to `Num9`, F1 to F12, Tab, Backspace, Delete, Home, End,
  PageUp, PageDown, Shift, Ctrl and Alt (left and right count as one). They are one table, `ENGINE_KEY_LIST` in
  `input/keys.hpp`, which also builds the SDL scancode map, so a new key is a one-line change.
- `input/mouse.hpp`: `input::Mouse`, at `Input::mouse`: the pointer position in logical pixels (letterboxing is already
  undone), `inside` the logical area, wheel notches, and Left, Right and Middle buttons with `IsDown`/`WasPressed`/
  `WasReleased` edges that work like the keyboard's (a press and release inside one update still counts as a click).
- `graphics/font.hpp`: `WrapText` (breaks at spaces, long words by characters, `\n` respected) and `MeasureText` (wrapped
  width and height). `Text` gains `wrapWidth` (0 for none) and `align`, so the draw batch wraps and aligns each line
  itself. A character with no glyph draws as `BitmapFont::placeholder`, or the font's `?`, instead of vanishing (spaces
  and control characters stay gaps).

### Changed

- **Breaking:** `Game::Update` and `ScheduledGame::Update` take `const input::Input&` instead of `const input::Keyboard&`, and
  `ecs::FrameContext::keyboard` is replaced by `ecs::FrameContext::input`. Migrating a game (Sky Hop): in a `Game`
  subclass, change the parameter type; in systems, `context.keyboard->WasPressed(k)` becomes
  `context.input->keyboard.WasPressed(k)` (search for `context.keyboard` and `context.keyboard->`); in tests, keep an
  `input::Input input;` instead of an `input::Keyboard keyboard;`, call `input.keyboard.OnKeyDown(...)`, and
  `input.EndUpdate()` after each update. `Keyboard` itself is unchanged.
- **Breaking:** the library target `engine::engine` is now `engine::platform` (the SDL platform), with the SDL-free part split
  out as `engine::core`. Change `target_link_libraries(... engine::engine)` to `engine::platform`.
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
