# ecs-engine

[![CI](https://github.com/brian-md-rose/ecs-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/brian-md-rose/ecs-engine/actions/workflows/ci.yml)

A learning-focused 2D game engine built on a hand-written Entity Component System, with an
original maze-chase game as its test bed: eat every pellet in the maze, avoid the enemies, and
grab power pellets to turn the tables.

The work is **engine-first**: each phase adds a reusable engine system, and the game stays a thin
layer on top. See [docs/ROADMAP.md](docs/ROADMAP.md) for the plan and progress.

## Status

The roadmap is complete through Phase 5. You can steer the player around the first maze, eat pellets and power
pellets for points, and clear the level, which then restarts. Enemies and their AI come next
(Phase 6), then polish: fonts, audio, sprites and menus (Phase 7).

## Features

**Engine** (`engine::engine`, a static library)

- **ECS**: `Entity` handles with generations, sparse-set `ComponentPool`s, a `Registry` whose
  `Each` loops over the smallest pool, and a `CommandBuffer` for creating and destroying
  entities safely during iteration. Debug builds assert on dead entities and missing components.
- **World and resources**: one-of-a-kind data (maze, score, game state) lives in typed
  `Resources` beside the registry ([ADR 0001](docs/decisions/0001-shared-game-data.md)).
- **Scheduler**: systems run in ordered phases (input → update → late update → render), each
  with an optional run condition.
- **Events**: a typed event bus so scoring, audio and UI don't depend on each other
  ([ADR 0002](docs/decisions/0002-events.md)).
- **Platform**: SDL3 window, keyboard input and an application loop with a fixed 60 Hz update
  and variable-rate rendering. SDL types stay out of the public headers.
- **Graphics**: `Transform` and `Shape` components drawn in batches by `Renderer2D`, with a
  camera that scales a fixed logical resolution to the window.

**Game**

- Plain-text mazes loaded from `game/assets/mazes/` with line-numbered errors
  ([format](docs/maze-format.md)).
- Grid movement with buffered turns, corner tolerance, wall stops and wrap tunnels, written as
  pure functions.
- Pellets, power pellets and score, and a state machine:
  Ready → Playing → Dying → LevelComplete → GameOver.

## Controls

| Key                  | Action                          |
|----------------------|---------------------------------|
| Arrow keys / WASD    | Move                            |
| P / Esc              | Pause                           |
| Enter / Space        | Start a new game after game over |
| F1                   | Toggle the rendering stress test |

## Layout

```
engine/   static library (engine::engine)
  include/  application/ core/ ecs/ graphics/ input/
  src/
game/     the maze game executable
  assets/   mazes and other run-time files, copied next to the executable on every build
  include/ src/
tests/    Catch2 unit tests for the engine and the game's SDL-free logic
docs/     roadmap, style guide, maze format and decision records
thirdparty/SDL   SDL3, as a git submodule
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

- `-DENGINE_BUILD_TESTS=OFF` skips the tests (and the Catch2 download).
- `-DENGINE_BUILD_GAME=OFF` builds the engine only.

All targets build with every warning enabled and warnings as errors (`/Wall /WX /analyze` on
MSVC), so new warnings fail the build. Code style is in [docs/STYLE.md](docs/STYLE.md).

## License

MIT, see [LICENSE](LICENSE).
