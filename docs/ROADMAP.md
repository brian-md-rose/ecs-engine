# Roadmap: an original maze-chase game on ecs-engine

The goal is an **original** maze-chase game: eat pellets in a maze, avoid chasing
enemies, and grab power-ups to turn the tables. The name, characters, mazes, art and
sound are all our own designs.

The approach is **engine-first**: nearly every phase adds a reusable engine system, and
the game is a thin test bed on top.

Decisions so far:

| Area      | Choice                                                       |
|-----------|--------------------------------------------------------------|
| Platform  | SDL3, vendored as a git submodule in `thirdparty/SDL`        |
| Art       | Coloured shapes first; original sprites since Phase 7        |
| Levels    | Plain-text maze files loaded from `assets/`                  |
| Testing   | Gameplay logic stays SDL-free so it can be unit-tested on CI |

---

## Phase 0 – Harden the ECS

Everything else builds on the registry, so fix its sharp edges first.

- [x] Assert on dead entities and missing components in `Registry` and `ComponentPool`.
- [x] Add an `Entity::Null` constant. A default `Entity{}` is currently `{0, 0}`, which is a real entity.
- [x] Make `Each` loop over the smallest pool. Stop `HasComponent` and `GetComponent` from creating empty pools.
- [x] Add a deferred command buffer so systems can create and destroy entities and components safely while `Each` is iterating.
- [x] Small cleanups (see the GitHub issues).

**Done when:** tests cover destroying entities during iteration and the asserts fire in Debug builds.

## Phase 1 – Platform layer (`engine/platform/`)

- [x] Add SDL3 (a git submodule, built statically). Keep SDL types out of public engine headers.
- [x] `Window`: create, resize, and scale integer-sized pixels to fit.
- [x] `Input`: map raw keys to actions (`MoveUp`, `Pause`, …) and remember the last direction pressed.
- [x] `Clock` plus an `Application` main loop with a fixed 60 Hz update and variable-rate rendering.
- [x] CI: build SDL on Linux with the dummy video driver, or add an option to skip the platform layer on CI runners.

**Done when:** a window opens, a square moves with the arrow keys, and the update rate is stable.

## Phase 2 – Rendering and system scheduling

- [x] Components: `Transform` and `Shape` (rectangle or circle, colour, layer).
- [x] `Renderer2D`: draws all shapes in batches each frame, with a camera that scales a fixed logical resolution such as 224×288.
- [x] `Scheduler`: an ordered list of systems in phases (input → update → late update → render).
- [x] Decide where single-instance game data (maze, score, game state) lives: in the registry or in a separate context object.

**Done when:** `main.cpp` only registers systems and runs the application.

## Phase 3 – Resources and mazes

- [x] Text maze format, e.g. `#` wall, `.` pellet, `o` power pellet, `P` player spawn,
      `E` enemy spawn, `-` enemy-house door, `T` wrap tunnel.
- [x] `TileMap` loader that turns tiles into entities and reports errors with line numbers.
- [x] CMake post-build step that copies `assets/` next to the executable.
- [x] Design the first original maze.

**Done when:** a maze loads from file and renders, and the loader has unit tests.

## Phase 4 – Grid movement

- [x] `GridMover` component: current tile, direction, buffered direction, speed.
- [x] Turns are allowed only near a tile's centre, with a small tolerance so corners feel smooth. Stop at walls. Wrap through tunnels.
- [x] Movement written as pure functions, unit-tested without SDL.

**Done when:** the player moves smoothly around the maze and corners feel smooth.

## Phase 5 – Game rules and state

- [x] Collision based on shared tiles; pellets, power pellets and score.
- [x] Engine event bus (`PelletEaten`, `PlayerCaught`, `LevelCleared`), so scoring, audio and UI don't depend on each other.
- [x] Game state machine: Ready → Playing → Dying → LevelComplete → GameOver (later moved onto engine states, Phase 8).

**Done when:** you can clear a level with no enemies and the level restarts.

## Phase 6 – Enemy AI

- [x] Each enemy picks a target tile. At every intersection it takes the exit closest to that target, never reversing.
- [x] Modes on timers: patrol, chase, frightened (after a power pellet), and returning home after being eaten.
- [x] Four original personalities, for example a direct chaser, one that cuts you off ahead, one that pincers with an ally, and one that keeps its distance.

**Done when:** all four enemies behave differently, the modes switch correctly, and the pathing has unit tests.
See [enemies.md](enemies.md) for the personalities, the schedule and the tuning.

## Phase 7 – Polish

- [x] Bitmap font for score, lives and level.
- [x] SDL3 audio with original sound effects.
- [x] Levels get faster as you progress.
- [x] Replace the coloured shapes with original sprite sheets.
- [x] Pause screen and title screen.

See [pixel-sheets.md](pixel-sheets.md) for the font and sprite format, and
[difficulty.md](difficulty.md) for the per-level table.

---

## Phase 8 – Game states and screens

- [x] Engine states: `State`, `NextState`, enter and exit systems, `InState`, `DespawnOnExit`, sub-states ([0003](decisions/0003-game-states.md)).
- [x] The game's flow moved onto states; menus are spawned on enter and removed on exit.
- [x] Options screen (sound, lives), name entry for a new high score, and a level-transition screen.

See [screens.md](screens.md).

---

## Phase 9 – ECS queries, hooks and hierarchy

- [x] Exclusion filters: `Each<Enemy>(Without<Frightened>, ...)` skips entities with the listed components.
- [x] Component hooks: `OnAdd<T>` and `OnRemove<T>` run when a component is added or removed, including by `Destroy` and a `CommandBuffer`.
- [x] Parent and child entities: `ecs::Parent` and `graphics::LocalTransform`, with world positions worked out after every update; the debug panel's text is a child of its background.

---

## Phases 10 to 12 – Features for the Project Babel port

JSON and a seeded RNG; mouse, more keys, full-ASCII text, camera zoom, clipping and UI
mechanisms; then window settings, gamepads and audio files. See
[babel-support.md](babel-support.md). All three phases shipped as v0.2.0 to v0.5.0.

---

## Editor – type registry, scenes and an ImGui editor (v0.6.0 onward)

Steps E1 to E7 of the editor plan. See [scenes.md](scenes.md), [editor.md](editor.md) and
[ADR 0004](decisions/0004-reflection-and-scenes.md).

- [x] **E1** (v0.6.0): `ecs::TypeRegistry` and the engine's own components registered.
- [x] **E2** (v0.6.0): JSON scene save and load, with entity ids remapped.
- [x] **E3** (v0.7.0): Dear ImGui as a submodule, the `EditorHost` hook, `engine::editor` and `tangle_run_editor` (`ENGINE_BUILD_EDITOR`, off by default).
- [x] **E4** (v0.7.0): Hierarchy and Inspector panels.
- [x] **E5** (v0.8.0): the standalone editor: edit and play worlds, scene files, Play and Stop.
- [x] **E6** (v0.8.0): Viewport with zoom, pan, picking and drag to move.
- [x] **E7** (v0.8.0): undo and redo.
- [x] Tangle Run's menu screens laid out by scene files (the game's own change, after v0.8.0). The playing field is still built from code.
- [x] Editor features after v0.8.0 (see the [Unreleased] notes in the [changelog](../CHANGELOG.md) for what has not been tagged): asset browser with thumbnails, import settings and drag and drop, console, profiler, build settings, docking, rotation and scale on `Transform`, rotate and scale gizmos, basic prefabs, tile tools, game extension points and a Game panel beside the Viewport.
- [ ] Not started: prefab overrides and nested prefabs, resource editing, animation clips and an animator, and a macOS or Linux editor build (CI builds the editor on Windows only).

---

## Open questions

- Game name and character designs.
- Tile size and logical resolution (8 px tiles at 224×288 is a good starting point).
