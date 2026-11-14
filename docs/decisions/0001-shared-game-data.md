# 0001 – Where one-of-a-kind game data lives

**Status:** accepted · **Issue:** #15 · **Phase:** 2

## Context

Some game data exists exactly once rather than per entity: the controls, whether the game is
paused, the renderer, and from Phase 3 the maze, score, lives, level and random number
generator. Systems need to reach it, and the engine's scheduler (#14) needs one system
signature that works for every game without knowing the game's types. Globals and singletons
are ruled out: they hide state from tests and make two worlds in one process impossible.

## Options

1. **Singleton components** on a dedicated entity. No new machinery, but every system has to
   find "the" entity first, nothing stops a second copy, and the `Component` concept requires
   `std::movable`, which rules out types like `Renderer2D`.
2. **A game-defined context struct** passed to systems. Simple and explicit, but the engine's
   scheduler would have to be a template on the game's struct, or pass a `void*` that systems
   cast back to the game's type. Either way game types leak into the engine.
3. **Resources stored by type** alongside the registry. One value per type, looked up by type.

## Decision

Option 3. `engine::ecs::Resources` (`engine/include/ecs/resources.hpp`) holds one value of each
type, and `engine::ecs::World` bundles it with the `Registry` and a `CommandBuffer`. Systems
take a `World&`:

```cpp
void PlayerInputSystem(World& world)
{
	const auto& controls = world.resources.Get<game::Controls>();
	world.registry.Each<PlayerControlled, Velocity>(...);
}
```

and the game adds its resources once at start-up:

```cpp
world.resources.Emplace<game::Controls>();
world.resources.Emplace<GameState>();
world.resources.Emplace<Renderer2D>(Camera{.logicalWidth = 224, .logicalHeight = 288});
```

### How it works

- Stored as `std::type_index` → `std::unique_ptr<Holder<T>>`, the same pattern as the
  registry's component pools.
- Each resource is built in place in its own allocation, so it needn't be copyable or movable,
  and a reference from `Get` stays valid while other resources are added.
- Resources are destroyed newest first, so a resource may refer to any resource added before it.
- A lookup is one hash. Systems fetch a resource once per run, not once per entity.

## Rules

1. **Resources are owned by the World.** Things lent for a single call (`dt`, the keyboard, the
   renderer during Render, `alpha`) are passed as arguments, never stored as resources, so
   nothing in a World can dangle between frames.
2. **Add and remove resources only during setup or between phases**, never inside a system:
   removing one would leave other systems' references dangling.
3. **One value per type.** Two values of the same underlying type get distinct wrappers
   (`struct Score { int value; }`, `struct HighScore { int value; }`).
4. **A missing resource fails loudly.** `Get<T>` uses `ENGINE_VERIFY`, so it aborts with the
   type's name in every configuration. `TryGet<T>` returns `nullptr` and is for genuinely
   optional resources.
5. **Fetch resources at the top of a system**, so they read like parameters.

## Consequences

- The engine never needs the game's types, and tests build a World with exactly the resources a
  system uses.
- A system's signature no longer shows what it reads (rule 5 is the mitigation). If the
  scheduler ever runs systems in parallel, systems will need to declare their access.
- A forgotten `Emplace` is found at run time, not compile time. Rule 4 makes it fail at once
  with the type named, and each system's tests catch it first.
