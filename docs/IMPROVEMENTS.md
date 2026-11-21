# Engine improvement recommendations

From a read-through of the engine: layout, roadmap, registry, component pool, scheduler and CI
config. Nothing was built or run. Ordered by value.

## 1. Add sanitizers and static analysis to CI

- CI already uses `/Wall /WX /analyze` and `-Wshadow -Wconversion`.
- There is no ASan/UBSan job on Linux and no clang-tidy.
- The sparse-set and hook code has aliasing and lifetime risk, so a sanitizer job would catch real
  bugs cheaply.

## 2. Make the component pool safer against reallocation

- `ComponentPool::Add` uses `emplace_back`, which can reallocate the pool.
- `Registry::Add` returns a `T&`, and `OnAdd` hooks receive a `T&` into that vector.
- Add tests for:
  - a hook that calls `Create()`
  - a hook that reads another pool while the same pool is being added to
- Document that the reference is only valid until the next structural change.

## 3. Cache pool lookups

- Each `Each`, `Add`, `Get` and `Has` call goes through `unordered_map<type_index, unique_ptr>`.
- Replace it with a per-type integer id (a static counter per `T`) and a `vector` of pools.
- That removes hashing from the hot path.
- Add a small benchmark first so the gain is measurable.

## 4. Replace `std::function` hooks and function-pointer systems

- `System` is `void(*)(World&, const FrameContext&)`, so systems can't capture state. They rely on
  globals or `World` resources.
- A `std::function` or small callable wrapper would help once tools or tests need parameterised
  systems.
- Hooks use `std::function`; check the cost if hooks end up on hot components.

## 5. Add system ordering and dependencies

- Ordering is only phase plus registration order.
- Add `before`/`after` labels or system sets, so a game module doesn't depend on the order of `Add`
  calls.
- This also allows detecting conflicting component access later.

## 6. Add cached queries

- `Each` loops over the smallest pool and calls `HasComponent` per entity.
- A cached `View` or archetype grouping would help as entity counts grow.
- Likely premature for a maze game: only do it if a benchmark says so.

## 7. Add engine features the roadmap doesn't list

- Input actions and rebinding; only a keyboard exists today. Add gamepad support.
- A cap on fixed-timestep catch-up (spiral of death), and pause or time-scale on `Clock`, if not
  already present.
- Resource handles with hot reload for the atlas and pixel sheets, so asset iteration needs no
  rebuild.
- World serialization, covering the save system and debugging.

## 8. Make the platform layer easier to test

- Gameplay tests already avoid SDL.
- Add a headless `Renderer` or `Window` fake so `Application` and the render systems get CI
  coverage.

## 9. Close small docs and process gaps

- `ROADMAP.md` has no Phase 10 entry for what comes next.
- The decision records stop at 0003, but hooks, hierarchy and `Without` filters (Phase 9) were
  design decisions too.
- Add an ADR for hooks, since their rules are non-obvious: structural changes assert, and
  `OnRemove` runs before any of the entity's components are removed.

## Suggested first steps

Items 1 and 3, in that order. 1 is small and protects everything else. 3 is a contained change that
a benchmark can verify.
