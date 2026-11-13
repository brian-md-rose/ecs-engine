# Style guide

How code in this repo is written. It describes what the existing code already does; when
in doubt, match the surrounding file. Formatting is enforced by `.clang-format` and
`.editorconfig`, so most of the layout rules below are applied for you.

## Formatting

Run clang-format on every C++ file you touch. The config (`.clang-format`) is:

- Microsoft base style: **Allman braces**, **tabs** for indentation (width 4), 120-column limit.
- **Lambdas are Allman too.** The opening brace goes on its own line, level with the statement,
  and the body is indented one level from it. Only empty lambdas (`[&](Entity, Velocity&) {}`)
  stay on one line.
  ```cpp
  registry.Each<Position>([](Entity, Position& p)
  {
      p.x = std::clamp(p.x, 0.0f, kMaxX);
  });
  ```
  Inside a test macro, name the lambda first so the call stays on one line:
  ```cpp
  const auto destroy = [&](Entity entity, Position&)
  {
      registry.Destroy(entity);
  };
  CHECK_THROWS_AS(registry.Each<Position>(destroy), AssertFailure);
  ```
- Namespace contents are **indented**.
- `public:` / `private:` sit one level out from members.
- Pointers and references bind to the type: `const Entity& entity`, `SDL_Window* window`.
- Short functions may sit on one line only when defined inside a class body.
- A short `if` without an `else` may sit on one line: `if (!action) return;`.
- Braced initialiser lists open on the same line (`kKeyMap[] = {`): clang-format can't wrap them.
- Hand-aligned blocks (the assert macros) are fenced with `// clang-format off` / `// clang-format on`.
  Keep these rare.

Other files (`.editorconfig`): UTF-8, LF line endings, final newline, trailing whitespace trimmed.
CMake uses 4 spaces; JSON and YAML use 2 spaces.

## Naming

| Thing                          | Style                 | Examples                                         |
|--------------------------------|-----------------------|--------------------------------------------------|
| Types, enums, concepts         | `PascalCase`          | `Registry`, `ComponentPool`, `Component`         |
| Interfaces (abstract bases)    | `I` + `PascalCase`    | `IComponentPool`, `IComponentCommand`            |
| Functions and methods          | `PascalCase`          | `Create`, `IsAlive`, `WasPressed`, `EndUpdate`   |
| Member variables               | `camelCase`, no prefix | `generations`, `iterationDepth`, `keyboard`     |
| Locals and parameters          | `camelCase`           | `frameSeconds`, `renderWidth`                    |
| Constants                      | `k` + `PascalCase`    | `kNullId`, `kKeyCount`, `kLogicalWidth`          |
| Enumerators                    | `PascalCase`          | `Key::Up`, `Action::MoveLeft`, `Direction::None` |
| Namespaces                     | `lowercase`           | `engine::ecs`, `engine::input`, `game`           |
| Macros                         | `ENGINE_` + `UPPER`   | `ENGINE_ASSERT`, `ENGINE_VERIFY`                 |
| Files                          | `lowercase`, no separators | `componentpool.hpp`, `fixedtimestep.cpp`    |
| Test files                     | `<name>_tests.cpp`    | `registry_tests.cpp`, `controls_tests.cpp`       |

Name booleans and their queries as questions: `IsValid()`, `IsHeld()`, `HasComponent()`.
Units go in the name when they aren't obvious: `stepSeconds`, `maxFrameSeconds`.

## Files and layout

```
engine/include/<module>/<name>.hpp   public engine headers (core, ecs, graphics, input, application)
engine/src/<module>/<name>.cpp       their implementations
game/include/<name>.hpp              game headers
game/src/<name>.cpp                  game sources (main.cpp is the entry point)
tests/<module>/<name>_tests.cpp      Catch2 tests, mirroring the module they test
```

- Headers start with `#pragma once`.
- Include engine headers by module path with quotes: `#include "ecs/registry.hpp"`.
- In a `.cpp`, include its own header first. Then standard and third-party headers (`<...>`),
  then project headers (`"..."`), each group in alphabetical order and separated by a blank line.
  Headers follow the same order after `#pragma once`. Test files put the Catch2 headers first,
  in a group of their own.
- Everything lives in a namespace: `engine::<module>` for the engine, `game` for game code.
  File-local helpers go in an anonymous namespace, not `static`.
- Close namespaces with a comment: `} // namespace engine::ecs`.

## SDL and the platform boundary

SDL is linked `PRIVATE` to the engine. **Only engine `.cpp` files include `<SDL3/...>`.**

- Public headers may forward-declare SDL types (`struct SDL_Window;`) but never include SDL.
- Game code, game tests and engine tests never see SDL. They use engine types instead:
  `input::Key`, not `SDL_Scancode`.
- Translate SDL values with a lookup table rather than a `switch` over SDL's enums
  (`/Wall` warns about every enumerator a switch leaves out).
- Check every SDL call that can fail and report it with `SDL_Log("X failed: %s", SDL_GetError())`.

The engine provides mechanisms; the game decides what they mean. For example, the engine's
`Keyboard` knows keys, while the game's `Controls` maps keys to `MoveUp`, `Pause` and so on.

## Classes and ownership

- Use RAII for every resource. The constructor acquires it and the destructor releases it.
  When objects depend on each other, rely on member declaration order: members are
  destroyed in reverse order (see `Application`: `SdlScope`, then `Window`, then `Renderer`).
- Classes that own a resource delete copy and move explicitly, all four:
  ```cpp
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;
  ```
- Default member initialisers over constructor init where possible: `bool paused = false;`.
- Order inside a class: private data and helpers first, then `public:`.
- Mark concrete implementations of an interface `final` (`SystemClock`, `ComponentPool<T>`,
  `MazeGame`), and every overriding function `override`.
- Use `explicit` on single-argument constructors.
- `[[nodiscard]]` on queries and on functions whose result must be used (`Advance`, `PoolCount`).
- `noexcept` on trivial accessors that can't fail.
- `const` locals by default: `const Entity player = registry.Create();`.
- Components are plain structs with default member initialisers. They must satisfy the
  `Component` concept: a non-const, movable class type. Empty tag structs are fine.

## Errors and invariants

| Situation                                          | Use                                       |
|----------------------------------------------------|-------------------------------------------|
| Programmer error, cheap to skip in Release         | `ENGINE_ASSERT(cond, "message")` (Debug only) |
| Continuing would corrupt memory (out-of-range write) | `ENGINE_VERIFY(cond, "message")` (always on, aborts) |
| Runtime failure outside our control (SDL, files)   | Log it and report through a return value or `IsValid()` |
| Request that is already satisfied                   | A documented no-op (e.g. `Registry::Remove` on a dead entity) |

- Assert messages are lower-case phrases that say what went wrong and, where useful, what to do
  instead: `"structural change during Registry::Each; record it in a CommandBuffer instead"`.
- No exceptions in engine code. Tests make asserts throw with the `AssertsThrow` guard
  (`tests/assert_guard.hpp`) so they can be checked with `CHECK_THROWS`.
- When behaviour is a deliberate choice (a no-op instead of an assert), say why in a comment.

## Comments

- Comments explain **why** and **contract**, not what the next line does.
- Every public class gets a short comment saying what it is for. Public functions get one when
  the name doesn't say everything: units, ranges, ownership, what happens at the edges.
  ```cpp
  // Adds elapsed real time; returns how many fixed updates to run now.
  // Negative times (a clock that went backwards) are treated as zero.
  [[nodiscard]] int Advance(double frameSeconds);
  ```
- Short usage examples in header comments are welcome (see `CommandBuffer`).
- Write in plain English sentences with normal capitalisation and full stops.

## Tests

- Catch2 v3. Engine tests go in `engine_tests`; game logic tests go in `game_tests`.
- Test names are plain sentences that state the behaviour:
  `"Registry recycles ids with a new generation"`.
- Tag by area: `[ecs][registry]`, `[core][timestep]`, `[input]`, `[game][controls]`.
- Test helpers and component types go in an anonymous namespace at the top, with `using`
  declarations for the types under test.
- Prefer `CHECK` over `REQUIRE` unless later lines can't run after a failure.
- Use `WithinAbs` for floating-point checks, and avoid values that land exactly on a
  rounding boundary.
- Anything with time or input is tested with fakes (`FakeClock`, key events fed to
  `Keyboard`), never with a real window or clock.
- Tests that rely on `ENGINE_ASSERT` skip themselves when `kAssertsEnabled` is false (Release).

## Build and warnings

- C++20, no compiler extensions.
- Every target links `engine::warnings`: `/Wall /WX /analyze` on MSVC, and
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror` on GCC and Clang.
  Code must build cleanly on all three. Silence a warning only in CMake, globally, with a comment
  saying why it is noise.
- Convert between numeric types explicitly with `static_cast`. Use `std::size_t` for indices.
- Use `constexpr` values at namespace scope when a lambda needs them (`std::clamp` and friends
  take references, which forces a capture).
- New engine `.cpp` files are listed in `engine/CMakeLists.txt`. Game files are picked up by glob.
- Before pushing, build and test Debug and Release: `cmake --build --preset debug`,
  `ctest --preset debug`, and the same for `release`.

## Commits and pull requests

- One logical change per commit.
- Subject line: imperative mood, sentence case, no full stop, about 50–70 characters.
  `Add keyboard input and game-side action controls`.
- The body explains what changed and why, grouped by area with short bullet lists, and ends with
  the issue reference: `Part of #9` while work continues, or `Closes #9` when it's finished.
- Work happens on a branch (e.g. `PhaseOneTasks`) and reaches `main` through a pull request.
  CI must pass on Windows (MSVC Debug and Release) and Linux (GCC and Clang).
