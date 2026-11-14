# Roadmap: an original maze-chase game on ecs-engine

The goal is an **original** maze-chase game: eat pellets in a maze, avoid chasing
enemies, and grab power-ups to turn the tables. The name, characters, mazes, art and
sound are all our own designs.

The approach is **engine-first**: nearly every phase adds a reusable engine system, and
the game is a thin test bed on top.

Decisions so far:

| Area      | Choice                                                       |
|-----------|--------------------------------------------------------------|
| Platform  | SDL3, fetched with CMake `FetchContent`                      |
| Art       | Coloured shapes first; original sprites in Phase 7           |
| Levels    | Plain-text maze files loaded from `assets/`                  |
| Testing   | Gameplay logic stays SDL-free so it can be unit-tested on CI |

---

## Phase 0 – Harden the ECS

Everything else builds on the registry, so fix its sharp edges first.

- [ ] Assert on dead entities and missing components in `Registry` and `ComponentPool`.
- [ ] Add an `Entity::Null` constant. A default `Entity{}` is currently `{0, 0}`, which is a real entity.
- [ ] Make `Each` loop over the smallest pool. Stop `HasComponent` and `GetComponent` from creating empty pools.
- [ ] Add a deferred command buffer so systems can create and destroy entities and components safely while `Each` is iterating.
- [ ] Small cleanups (see the GitHub issues).

**Done when:** tests cover destroying entities during iteration and the asserts fire in Debug builds.

## Phase 1 – Platform layer (`engine/platform/`)

- [ ] Fetch SDL3 via `FetchContent`. Keep SDL types out of public engine headers.
- [ ] `Window`: create, resize, and scale integer-sized pixels to fit.
- [ ] `Input`: map raw keys to actions (`MoveUp`, `Pause`, …) and remember the last direction pressed.
- [ ] `Clock` plus an `Application` main loop with a fixed 60 Hz update and variable-rate rendering.
- [ ] CI: build SDL on Linux with the dummy video driver, or add an option to skip the platform layer on CI runners.

**Done when:** a window opens, a square moves with the arrow keys, and the update rate is stable.

## Phase 2 – Rendering and system scheduling

- [x] Components: `Transform` and `Shape` (rectangle or circle, colour, layer).
- [x] `Renderer2D`: draws all shapes in batches each frame, with a camera that scales a fixed logical resolution such as 224×288.
- [ ] `Scheduler`: an ordered list of systems in phases (input → update → late update → render).
- [ ] Decide where single-instance game data (maze, score, game state) lives: in the registry or in a separate context object.

**Done when:** `main.cpp` only registers systems and runs the application.

## Phase 3 – Resources and mazes

- [ ] Text maze format, e.g. `#` wall, `.` pellet, `o` power pellet, `P` player spawn,
      `E` enemy spawn, `-` enemy-house door, `T` wrap tunnel.
- [ ] `TileMap` loader that turns tiles into entities and reports errors with line numbers.
- [ ] CMake post-build step that copies `assets/` next to the executable.
- [ ] Design the first original maze.

**Done when:** a maze loads from file and renders, and the loader has unit tests.

## Phase 4 – Grid movement

- [ ] `GridMover` component: current tile, direction, buffered direction, speed.
- [ ] Turns are allowed only near a tile's centre, with a small tolerance so corners feel smooth. Stop at walls. Wrap through tunnels.
- [ ] Movement written as pure functions, unit-tested without SDL.

**Done when:** the player moves smoothly around the maze and corners feel smooth.

## Phase 5 – Game rules and state

- [ ] Collision based on shared tiles; pellets, power pellets and score.
- [ ] Engine event bus (`PelletEaten`, `PlayerCaught`, `LevelCleared`), so scoring, audio and UI don't depend on each other.
- [ ] Game state machine: Ready → Playing → Dying → LevelComplete → GameOver.

**Done when:** you can clear a level with no enemies and the level restarts.

## Phase 6 – Enemy AI

- [ ] Each enemy picks a target tile. At every intersection it takes the exit closest to that target, never reversing.
- [ ] Modes on timers: patrol, chase, frightened (after a power pellet), and returning home after being eaten.
- [ ] Four original personalities, for example a direct chaser, one that cuts you off ahead, one that pincers with an ally, and one that keeps its distance.

**Done when:** all four enemies behave differently, the modes switch correctly, and the pathing has unit tests.

## Phase 7 – Polish

- [ ] Bitmap font for score, lives and level.
- [ ] SDL3 audio with original sound effects.
- [ ] Levels get faster as you progress.
- [ ] Replace the coloured shapes with original sprite sheets.
- [ ] Pause screen and title screen.

---

## Open questions

- Game name and character designs.
- Tile size and logical resolution (8 px tiles at 224×288 is a good starting point).
