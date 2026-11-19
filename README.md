# ecs-engine

[![CI](https://github.com/brian-md-rose/ecs-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/brian-md-rose/ecs-engine/actions/workflows/ci.yml)

A learning-focused 2D game engine built on a hand-written Entity Component System, and
**Tangle Run**, an original maze-chase game made with it: eat every pellet in the maze, dodge
four enemies with their own personalities, and grab power pellets to turn the tables. The
name, characters, mazes, art and sound are all original.

The work is **engine-first**: each phase added a reusable engine system, and the game stays a
thin layer on top. See [docs/ROADMAP.md](docs/ROADMAP.md) for the phases.

## Status

Every roadmap phase is done, up to **v0.5.0**. The game is playable from title screen to game over,
with enemies, scoring, a HUD, sound effects and music, sprites, mouse, keyboard and gamepad menus, and
levels that get harder as you go. Phases 0 to 9 built the engine and the game; Phases 10 to 12 added what a
second game (Project Babel, see [docs/babel-support.md](docs/babel-support.md)) needs: JSON and a
seeded random number generator, mouse and text, camera zoom and clipping, UI building blocks, display
settings, gamepads and audio files with a mixer and music.

## Versions and releases

The engine uses [Semantic Versioning](https://semver.org/) and is released as `vX.Y.Z` git tags.
See [CHANGELOG.md](CHANGELOG.md) for what changed, and [docs/releasing.md](docs/releasing.md) for how
to pin a release from another project (`FetchContent` by tag) and how to cut one.
The version is available in code as `<core/version.hpp>`. To use the engine from your own game:

```cmake
include(FetchContent)
FetchContent_Declare(ecs_engine
    GIT_REPOSITORY https://github.com/brian-md-rose/ecs-engine.git
    GIT_TAG        v0.5.0
    GIT_SUBMODULES thirdparty/SDL   # the engine builds its vendored SDL3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(ecs_engine)

target_link_libraries(my_game PRIVATE engine::platform)   # or engine::core for a tool with no window or audio
```

## Features

**Engine** (`engine::core`, the SDL-free part, and `engine::platform`, which adds the SDL window, renderer and audio; both static libraries)

- **ECS**: `Entity` handles with generations, sparse-set `ComponentPool`s, a `Registry` whose
  `Each` loops over the smallest pool and can leave some out (`Each<Enemy>(Without<Frightened>, ...)`),
  hooks that run when a component is added or removed (`OnAdd<T>`, `OnRemove<T>`), and a
  `CommandBuffer` for creating and destroying entities safely during iteration. Debug builds
  assert on dead entities and missing components.
- **Parents and children**: an `ecs::Parent` link plus a `graphics::LocalTransform` places an
  entity relative to its parent, so a panel and its text move together; `DestroyWithChildren`
  removes a whole tree.
- **World and resources**: one-of-a-kind data (maze, score, game state) lives in typed
  `Resources` beside the registry ([ADR 0001](docs/decisions/0001-shared-game-data.md)).
- **Scheduler**: systems run in ordered phases (input, update, late update, render), each
  with an optional run condition.
- **States**: `State<T>` / `NextState<T>` with enter and exit systems, `InState` run conditions,
  `DespawnOnExit` / `DespawnOnEnter` for screen-scoped entities and sub-states, applied once per update before input
  ([ADR 0003](docs/decisions/0003-game-states.md)).
- **Events**: a typed event bus so scoring, audio and UI don't depend on each other
  ([ADR 0002](docs/decisions/0002-events.md)).
- **Scenes**: components described at run time by a type registry, and entities saved to and loaded from JSON with
  ids remapped on load, in `engine::core` with no editor needed ([scenes](docs/scenes.md),
  [ADR 0004](docs/decisions/0004-reflection-and-scenes.md)).
- **Platform**: an SDL3 window and an application loop with a fixed 60 Hz update and variable-rate
  rendering. SDL types stay out of the public headers.
- **Input**: one `Input` aggregate per update holding the keyboard (a full PC key set, plus the UTF-8 text typed
  for name fields), the mouse (position in logical pixels, wheel and three buttons, with edges) and up to four gamepads.
  `Input::lastUsed` says which device the player touched last.
- **Data and randomness**: a JSON reader and writer (`core/json.hpp`: ordered objects, line and column in parse
  errors), atomic file writes, save data, a levelled log file, and `Random`, a PCG32 generator that gives the same
  numbers on MSVC, GCC and Clang, with named streams and a saveable state.
- **Graphics**: shapes, animated sprites and bitmap-font text drawn in batches from one atlas
  texture, with a camera that scales a fixed logical resolution to the window and can zoom, shake and follow a point.
  A `ScreenSpace` tag makes an entity (and its children) ignore the camera, and a `ClipRect` confines one to a
  rectangle. Text wraps and aligns, and a character with no glyph draws as a placeholder. Sprites and the font are
  PNG files named by a small manifest, with the original text format as a fallback ([pixel sheets](docs/pixel-sheets.md)).
- **UI**: a mouse pointer, hit-testing with layers, buttons that report hover, press and click, nine-slice panels,
  clipped scrolling lists, Tab focus and wrapped text, all as plain components and systems with no look of their own
  ([UI guide](docs/ui.md)). Tangle Run's menus are built from these, and it also has a demo screen under OPTIONS.
- **Display**: windowed, borderless or exclusive fullscreen, VSync and frame cap, and integer or fit scaling, all changed
  while running through a resource ([display](docs/display.md)).
- **Gamepads**: up to four pads with hotplug, deadzones, button names by position and focus navigation in the UI
  ([gamepads](docs/gamepad.md)).
- **Audio**: an SDL3 audio device that plays synthesised sound effects and sound files (`.wav`, and `.ogg` through the vendored
  stb_vorbis), each at its own sample rate and channel count, mixed on three buses with live volumes, plus streamed looping music that
  crossfades ([audio](docs/audio.md)).
- **Debug panel** (F3): frames per second, draw time, entity counts per component and the
  time each system takes, drawn on screen with the bitmap font. The scheduler times every system.

**Game**

- Plain-text mazes ([format](docs/maze-format.md)) and grid movement with buffered turns,
  corner tolerance, wall stops and wrap tunnels, written as pure functions.
- Four enemies that steer toward a target tile, each choosing it differently, and switch
  between patrol, chase, frightened and returning home on a per-level schedule
  ([enemies](docs/enemies.md)).
- Pellets, power pellets, score, high score and lives, driven by engine states
  (screens: Title, Help, Options, NameEntry, Playing, LevelTransition, GameOver; the round and pause
  are sub-states of Playing; see [screens](docs/screens.md)).
- A difficulty curve read from `game/assets/levels.txt`, editable without rebuilding
  ([difficulty](docs/difficulty.md)).
- Title screen, pause menu and HUD, with mouse-clickable buttons, and a scrolling how-to-play screen, all built from
  the engine's UI module.
- Options screen (sound, music, volume, starting lives, display), saved with the high score. Looping music
  (`game/assets/music/theme.ogg`) on the mixer's music bus, effects on the effects bus.
- Arcade-style name entry for a new high score, shown on the title screen.
- A level-transition screen between levels.

Gameplay logic is SDL-free, so it is unit-tested on CI alongside the engine.

## Controls

| Keyboard          | Gamepad               | Action                                       |
|-------------------|-----------------------|----------------------------------------------|
| Arrow keys / WASD | D-pad or left stick   | Move; pick a menu item; change an option or a name letter (up/down), move between name slots or change an option (left/right) |
| Enter / Space     | South button (A, Cross) | Choose a menu item or change an option; confirm a name; leave GAME OVER |
| P / Esc           | Start or Back         | Pause; go back from the options and how-to-play screens |
| F1                |                       | Toggle the rendering stress test             |
| F3                |                       | Toggle the debug panel                       |

The mouse works in every menu: hover a button to select it and click to act on it (on an option, a click steps it
forward). The how-to-play list scrolls with the wheel, by dragging, or with the right stick. The UI demo's hints name
the keys or pad buttons of whichever device you used last.

## Layout

```
engine/   static libraries (engine::core, engine::platform)
  include/  application/ audio/ core/ debug/ ecs/ graphics/ grid/ input/ ui/
  src/
game/     Tangle Run, the game executable
  assets/   mazes, levels, sprites and font, copied next to the executable on every build
  include/ src/
tests/    Catch2 unit tests for the engine and the game's SDL-free logic
benchmarks/  Catch2 benchmarks for Registry iteration and component add/remove churn
docs/     roadmap, style guide, file formats, guides (UI, display, gamepads, audio), enemy design and decision records
thirdparty/SDL   SDL3, as a git submodule
thirdparty/stb_vorbis   the stb_vorbis Ogg Vorbis decoder (public domain / MIT), vendored
```

## Building

Requires CMake 3.25+ and a C++20 compiler. SDL3 is a submodule, so clone with it:

```bash
git clone --recurse-submodules https://github.com/brian-md-rose/ecs-engine.git
```

In an existing clone:

```bash
git submodule update --init
```

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

Options:

- `-DENGINE_BUILD_TESTS=OFF` skips the tests.
- `-DENGINE_BUILD_BENCHMARKS=OFF` skips the benchmarks (with the tests also off, Catch2 isn't
  downloaded).
- `-DENGINE_BUILD_GAME=OFF` builds the engine only.

**Benchmarks:** `engine_benchmarks` times `Registry::Each` and component add/remove churn at
1k, 10k and 100k entities. Timings only mean something in an optimized build, so they have
their own presets and aren't run by `ctest`:

```bash
# Windows: Release build with the msvc configure preset
cmake --build --preset bench
build/msvc/benchmarks/Release/engine_benchmarks.exe

# Linux / macOS: Release, engine and benchmarks only
cmake --preset ninja-bench
cmake --build --preset ninja-bench
./build/ninja-bench/benchmarks/engine_benchmarks
```

Pass `[iteration]` or `[churn]` to run one group, and `--benchmark-samples 20` for a quicker run.

All targets build with every warning enabled and warnings as errors (`/Wall /WX /analyze` on
MSVC), so new warnings fail the build. Code style is in [docs/STYLE.md](docs/STYLE.md).

## License

MIT, see [LICENSE](LICENSE).
