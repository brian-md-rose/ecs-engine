# Plan: engine features for the Project Babel port

This plan covers the engine work that the Project Babel port needs. The port itself is planned in
`project-rise/docs/babel-on-ecs-engine-plan.md`, and its decisions are referred to here as D1 to
D10. That plan names three engine releases, E1, E2 and E3. This plan breaks each release into
one PR per checkbox group. Each group lists the files it touches, an API sketch, its tests and a
"Done when" line. The work continues [ROADMAP.md](ROADMAP.md) as Phases 10 to 12. **Status: all of it shipped, as v0.2.0 to v0.5.0. The checkboxes below are the original plan and were not ticked off as it was built.**

Facts below are from `v0.1.0` (commit `46b5caf`) and the vendored SDL in `thirdparty/SDL`.

## What the code shows that the migration plan misses

These points change the size or shape of the work. Each one is covered by a PR below.

1. **The input change breaks more than `ScheduledGame::Update`.** `Keyboard` is threaded through:
   - the virtual `Game::Update` and `Application`'s `keyboard` member and `PumpEvents`
     (`application.hpp`, `application.cpp`);
   - `ScheduledGame::Update` and its no-audio overload (`scheduledgame.hpp/.cpp`);
   - `ecs::FrameContext::keyboard` (`scheduler.hpp:65`), which every system reads;
   - Tangle Run (5 uses in `game/src`), and the `tests/input` and game tests.
2. **There is no screen-space drawing.** `DrawBatch::ToScreen` subtracts the camera from every
   entity. Tangle Run gets away with this because its camera never moves. Babel pans and zooms
   the map while the top bar, party bar and request card stay still, so entities need a way to
   opt out of the camera. Drawing, culling and hit-testing must all respect it.
3. **Zoom changes more than the camera struct.** `ToScreen`, `Overlaps`, sprite and text
   `scale` and shape sizes all need multiplying by the zoom. The visible area becomes the
   logical size divided by the zoom.
4. **A nine-slice needs a new draw primitive.** Textured quads only take a whole-number
   `scale`. A nine-slice panel stretches a source rectangle to any destination size.
5. **The clip rectangle depends on how the frame is submitted.** `Renderer2D::Submit` makes one
   `SDL_RenderGeometry` call per frame. A clip needs the batch split into runs, with
   `SDL_SetRenderClipRect` between them.
6. **The font is mostly art.** `BitmapFont` already has 128 glyph slots and `Text::scale`
   exists. The code work is word-wrap and measuring wrapped text.
7. **The RNG can't use `std::uniform_*_distribution`.** The MSVC and libstdc++ versions give
   different results, so a seed rolled on Windows would not reproduce on the Steam Deck. The
   engine needs its own bounded-integer and float conversions, with a golden-value test on all
   three CI compilers.
8. **JSON needs a few choices made up front:**
   - keep key order, so the writer is deterministic and data diffs stay clean;
   - store integers as 64-bit and separately from doubles, so seeds and IDs round-trip exactly;
   - support `\uXXXX` escapes;
   - share the atomic write. `WriteSaveData` already writes atomically, so pull that into a
     helper both writers use.
9. **`stb_vorbis` must build outside the warnings.** CI uses `/Wall /WX /analyze`. It needs its
   own target that doesn't link `engine::warnings`. clang-tidy only scans `engine/src` and
   `game/src`, so it needs no change.
10. **"Fit" mode means making presentation configurable.** `Renderer`'s constructor hardcodes
    `SDL_LOGICAL_PRESENTATION_INTEGER_SCALE` and VSync on.
11. **Mouse input needs logical coordinates and a way to consume a click.** SDL3 provides
    `SDL_ConvertEventToRenderCoordinates`, which handles letterboxing. A click on a button must
    not also place a tower, so the UI layer needs to mark a click as consumed.
12. **Nothing keeps the headless simulator SDL-free.** The engine is one static library that
    links SDL privately. `babel_sim` can link it without starting SDL, but nothing stops a
    system from calling into SDL. Decided (Q2): split the library in v0.3.0 (PR 11.0).

## Release order

```
v0.2.0  json + random               (non-breaking; unblocks Phase 0's loaders)
v0.3.0  library split, input, mouse, font, camera, clip, screen space, UI   (breaking)
v0.4.0  window settings + gamepad   (E2)
v0.5.0  audio files + music         (E3)
```

Decided (Q1): JSON and the RNG get their own release, ahead of the larger input and UI
release. That costs one extra release cycle, but Babel's Phase 0 loaders and RNG-backed tests
can start weeks earlier, and Tangle Run and Sky Hop need no changes for it. It moves the
migration plan's version numbers up by one, so that plan needs updating to match:

| Migration plan | Content | Version |
|---|---|---|
| E1 (data part) | Phase 10: JSON, RNG | `v0.2.0` |
| E1 | Phase 11: library split, input, text, camera, UI | `v0.3.0` |
| E2 | Phase 12: window, gamepad | `v0.4.0` |
| E3 | Phase 12: audio | `v0.5.0` |

Every release follows [releasing.md](releasing.md): a changelog entry, a version bump in
`CMakeLists.txt`, a PR, CI, the tag, and then the `ENGINE_TAG` bump in Babel. A breaking release
updates Tangle Run in the same PR and notes the migration for Sky Hop in the changelog.

---

## Phase 10 – Data and randomness (E1, data part: `v0.2.0`)

### PR 10.1: `core/json`

Files: new `engine/include/core/json.hpp`, `engine/src/core/json.cpp`,
`tests/core/json_tests.cpp`. Pull the atomic write out of `savedata.cpp` into
`core/files.hpp` (`WriteFileAtomic`), and have `WriteSaveData` use it.

```cpp
namespace engine::core {
    class JsonValue;  // null, bool, int64, double, string, array, object
    using JsonArray  = std::vector<JsonValue>;
    using JsonObject = std::vector<std::pair<std::string, JsonValue>>; // keeps key order

    struct JsonResult { std::optional<JsonValue> value; ParseError error; };
    [[nodiscard]] JsonResult ParseJson(std::string_view text);
    [[nodiscard]] JsonResult LoadJson(const std::filesystem::path& file);
    [[nodiscard]] std::string FormatJson(const JsonValue& value, int indent = 2);
    bool WriteJson(const std::filesystem::path& file, const JsonValue& value); // atomic

    // On JsonValue: IsX(), AsInt() / AsDouble() / AsString() returning optional,
    // Find(key) returning const JsonValue*, operator[] on arrays, Set/Append for writing.
}
```

- [ ] Recursive-descent parser with no exceptions. Errors give line and column through
      `ParseError`. Cap nesting depth (say 64) so a corrupt save can't overflow the stack.
- [ ] Integers without a fraction or exponent are stored as `int64_t`, everything else as
      `double`. `AsDouble()` accepts both.
- [ ] Doubles print with `std::to_chars` (shortest round-trip), never `printf`.
- [ ] Keys keep their file order. Duplicate keys are a parse error.
- [ ] Strings are UTF-8 with full escapes, including `\u` surrogate pairs.
- [ ] Tests: round-trip a nested save-shaped document, check the line and column of each kind of
      error, check int64 extremes, check depth, and check that format, parse and format again
      give identical text.

**Done when:** a Babel-shaped profile (lists of captains with nested soldiers, and a map graph)
round-trips byte-for-byte, and a broken file reports the right line.

### PR 10.2: `core/random`

Files: new `engine/include/core/random.hpp`, `engine/src/core/random.cpp`,
`tests/core/random_tests.cpp`.

```cpp
namespace engine::core {
    class Random {                // PCG32 (XSH RR)
    public:
        Random(std::uint64_t seed, std::uint64_t stream = 0);
        std::uint32_t Next();
        int   Range(int min, int maxInclusive);   // Lemire's method, no modulo bias
        float Unit();                             // [0, 1), from the top 24 bits
        bool  Chance(float probability);
        template <class T> void Shuffle(std::span<T> items);   // Fisher-Yates using Range
        std::size_t PickWeighted(std::span<const float> weights);
        [[nodiscard]] Random Fork(std::string_view name) const; // derives an independent stream
    };
    struct RandomState { std::uint64_t state, increment; }; // for saving a run mid-way
}
```

- [ ] No `<random>` distributions anywhere in the implementation (point 7).
- [ ] `Fork(name)` hashes the name (FNV-1a) and runs SplitMix64 over the seed XOR the hash to
      make **both** the new state and the new increment. Two PCG32s that differ only in the
      increment give correlated output, so changing the increment alone isn't enough. The
      `"offers"`, `"shop"` and `"map"` streams then stay independent and keep their sequences
      when a new stream is added.
- [ ] State can be saved and restored (`RandomState`), for run saves.
- [ ] Tests: golden values for seed 42 (the first 8 of `Next`, `Range(1,6)`, `Unit`), run on
      MSVC, GCC and Clang. Also a rough uniformity check on `Range`, plus `Shuffle` and
      `PickWeighted` edge cases (one item, zero weights).

**Done when:** the golden test passes on all three CI compilers. Then tag `v0.2.0`.

---

## Phase 11 – Input, text, camera and UI mechanisms (E1: `v0.3.0`, breaking)

Order: 11.0, then 11.1. Then 11.2, 11.3 and 11.4 can be done in parallel. Then 11.5 (its
`ClipRect` is screen-space, so it needs 11.4), then 11.6 (it needs 11.2, 11.4 and 11.5), then 11.7.

### PR 11.0: Split the library (breaking for CMake consumers)

Decided (Q2). Files: `engine/CMakeLists.txt`, the top-level `CMakeLists.txt`, `tests/` and
`game/` CMake files, `docs/releasing.md` (the consumer snippet).

- [ ] `engine_core` (alias `engine::core`): ECS, `core/` (json, random, text, savedata,
      fixedtimestep, clock), grid, the `input` types, and the SDL-free parts of `graphics`
      (transform, camera, colour, shape, sprite, font layout, `DrawBatch`) and of `ui`. It does
      **not** link SDL, so an SDL include there fails the build.
- [ ] `engine_platform` (alias `engine::platform`) links `engine_core` publicly and adds the platform:
      `Application`, `Window`, `Renderer`, `Renderer2D`, image loading, `AudioDevice` and the
      debug panel. Consumers that linked `engine::engine` change to `engine::platform` (Brian chose the name; v0.3.0 is breaking anyway).
- [ ] Engine tests that need no SDL link only `engine_core`. This is the check that stays
      honest.
- [ ] Changelog: `babel_sim`-style headless tools link `engine::core`.

**Done when:** `engine_core` builds and its tests pass with SDL absent from its link line, on
all CI presets.

### PR 11.1: the `Input` aggregate (breaking)

Files: `input/keyboard.hpp/.cpp`, new `input/input.hpp`, `application/application.hpp/.cpp`,
`application/scheduledgame.hpp/.cpp`, `ecs/scheduler.hpp`, Tangle Run's systems and tests.

```cpp
namespace engine::input {
    struct Input {
        Keyboard keyboard;
        Mouse mouse;          // PR 11.2
        // Gamepads gamepads;  arrives in v0.4.0
        void EndUpdate();     // clears pressed/released edges on every device
    };
}
// Game::Update(double dt, const input::Input& input, audio::AudioDevice* audio)
// FrameContext: const input::Input* input;  replaces  const input::Keyboard* keyboard;
```

- [ ] `Application` owns an `Input`. `PumpEvents` fills it, and focus loss releases everything.
- [ ] `Game::Update` and `ScheduledGame::Update` take `const input::Input&`. The no-audio
      overload stays for tests.
- [ ] `FrameContext::keyboard` is **removed** and replaced by `input` (Q4, a clean break).
      Systems read `context.input->keyboard`. Every system that reads keys changes in this PR,
      and the changelog gives Sky Hop the one-line search and replace.
- [ ] Keys: replace the hand-picked enum with the full set a PC game needs: the letters A to Z,
      digits 0 to 9, F1 to F12, Tab, Backspace, Delete, Shift, Ctrl, Alt, Home, End, PageUp and
      PageDown, plus the current keys. Generate the SDL scancode map from one table, so adding
      a key is a one-line change.
- [ ] Text entry: `Input::typed` (a short UTF-8 string per update, from `SDL_EVENT_TEXT_INPUT`),
      for name fields and filters. Keys alone can't give case or punctuation reliably.
- [ ] `Input` and every device type have no SDL in their headers and can be built in tests and
      in a headless runner.
- [ ] Tangle Run moves to `Input` in the same PR. Changelog: a migration note for Sky Hop.

**Done when:** Tangle Run plays as before, and a test drives a system through a hand-built
`Input`.

### PR 11.2: Mouse

Files: new `input/mouse.hpp/.cpp`, `application.cpp`, `tests/input/mouse_tests.cpp`.

```cpp
struct Mouse {
    graphics::Position position;   // logical (screen) pixels, after letterboxing
    bool inside = false;           // over the logical area, not the letterbox bars
    float wheel = 0;               // notches this update
    bool IsDown(MouseButton), WasPressed(MouseButton), WasReleased(MouseButton);
    void OnMove(Position), OnButton(MouseButton, bool down), OnWheel(float);
    void EndUpdate();
};
```

- [ ] `PumpEvents` runs `SDL_ConvertEventToRenderCoordinates` before reading mouse events, so
      positions are in logical pixels under both integer and fit presentation (point 11).
- [ ] Buttons: Left, Right and Middle. Edges work like `Keyboard`'s, so a press and release in
      the same frame still counts as one click.
- [ ] World coordinates are not stored here. They come from `Camera::ScreenToWorld` (PR 11.4),
      because the camera belongs to the game.
- [ ] Tests: fake events through `OnMove` and `OnButton`, edge clearing, and a press and release
      inside one update.

### PR 11.3: Font and text layout

Files: `game/assets/sprites/font.png` and `font.txt` (the art), `graphics/font.hpp/.cpp`,
`tests/graphics/font_tests.cpp`.

- [ ] Art: lowercase a to z and the rest of printable ASCII (32 to 126) in the existing 5 x 7
      style (D6), drawn into Tangle Run's font sheet.
- [ ] The engine ships no assets (Q5). The font sheet stays a game asset, and each game copies
      it into its own `assets/`. Babel takes a copy of Tangle Run's sheet. The engine's part is
      the code: loading a font from a sheet manifest, layout, wrapping and measuring.
- [ ] `WrapText(font, text, maxWidth, scale)` returns lines, breaking at spaces. A word longer
      than the width is broken by characters. Explicit `\n` is respected.
- [ ] `MeasureText(font, text, maxWidth, scale)` returns the width and height after wrapping.
- [ ] `Text` gains `wrapWidth` (0 means no wrap) and `align`, so `DrawBatch` lays out wrapped,
      aligned text itself.
- [ ] Unknown characters draw as a visible placeholder glyph, not nothing.
- [ ] Tests: wrapping at exact widths, long words, empty strings, and newlines.

### PR 11.4: Camera zoom and screen space

Files: `graphics/camera.hpp`, `graphics/drawbatch.cpp`, new `graphics/screenspace.hpp`,
`tests/graphics/camera_tests.cpp`, `drawbatch_tests.cpp`.

```cpp
struct Camera {
    int logicalWidth = 0, logicalHeight = 0;
    Position position{};    // world point at the top-left of the view
    int zoom = 1;           // whole numbers only (D9)
    Position offset{};      // screen-space shake, not affected by zoom
    [[nodiscard]] Position WorldToScreen(Position world) const;
    [[nodiscard]] Position ScreenToWorld(Position screen) const;
    [[nodiscard]] Position ViewSize() const;   // logical size / zoom, in world units
};
struct ScreenSpace {};   // tag: this entity ignores the camera (HUD, panels, cursors)
```

- [ ] `ToScreen` and `Overlaps` in `drawbatch.cpp` use `WorldToScreen`. Sprite and text scale,
      and shape sizes, are multiplied by `zoom` for world-space entities.
- [ ] Entities with `ScreenSpace` skip the camera entirely. A child inherits it from its parent
      through the hierarchy, so a panel's children are screen-space too (point 2).
- [ ] A helper `FocusOn(camera, worldPoint, zoom)` centres a point, for the request focus.
      Smooth panning stays game-side.
- [ ] Tests: round-trip `ScreenToWorld(WorldToScreen(p))` at zoom 1 and 2, culling at zoom 2,
      and a screen-space entity that stays put while the camera moves.

### PR 11.5: Clip rectangles and stretched quads

Files: `graphics/drawbatch.hpp/.cpp`, `graphics/renderer2d.cpp`, new `graphics/clip.hpp`,
`graphics/sprite.hpp`, tests.

```cpp
struct ClipRect { PixelRect rect; };   // screen-space; applies to this entity and its children
struct Sprite { ...; int width = 0, height = 0; };  // 0 = source size x scale; else stretched
```

- [ ] `DrawBatch` sorts by (clip, layer) and records **runs**: `{firstIndex, indexCount,
      clip}`. `Renderer2D::Submit` calls `SDL_SetRenderClipRect` and `SDL_RenderGeometry` once
      per run, then clears the clip (point 5). Check that SDL3's clip rect is in logical
      coordinates under logical presentation before settling the API.
- [ ] Nested clips intersect, so a list inside a clipped panel still clips correctly.
- [ ] Sprites can stretch to an explicit size (point 4). Needed for nine-slice pieces and
      progress bars.
- [ ] Tests: run splitting (same clip merges, different clips split), clip intersection, and
      culling of items wholly outside their clip.

### PR 11.6: UI mechanisms

Files: new `engine/include/ui/` and `engine/src/ui/` (`ui.hpp`, `ninepatch.hpp`, `uisystems.hpp`),
`tests/ui/`.

Mechanisms only. The game builds the screens.

```cpp
namespace engine::ui {
    struct Hitbox { float width, height; bool blocksBelow = true; }; // at the entity's position
    struct Interactable { bool enabled = true; };
    struct Hovered {}; struct Pressed {}; struct Focused {};     // state tags, set by systems
    struct Clicked { ecs::Entity entity; MouseButton button; };   // world.events
    struct PointerState { bool overUi = false; bool clickConsumed = false; };  // a resource
    struct NinePatch { PixelRect source; int left, right, top, bottom; float width, height; };
    struct ScrollView { float offset, contentHeight; };          // with a Clip; wheel scrolls

    void UiPointerSystem(ecs::World&, const ecs::FrameContext&);   // Input phase
    void NinePatchSystem(ecs::World&, const ecs::FrameContext&);   // spawns or updates 9 sprites
    void ScrollViewSystem(ecs::World&, const ecs::FrameContext&);
}
```

- [ ] Hit-testing: the top-most `Hitbox` under the mouse wins, by layer and then by draw order.
      Screen-space and world-space hitboxes are both tested, each in its own coordinates.
      Hitboxes outside their `ClipRect` don't count.
- [ ] `UiPointerSystem` sets `Hovered` and `Pressed` and sends `Clicked` (press and release on
      the same entity). It sets `PointerState::clickConsumed`, so the game's world-click
      systems (tower placement) can skip that click (point 11).
- [ ] `Focused` and a `FocusOrder` component exist now, for Tab and Shift+Tab. Gamepad
      navigation builds on them in v0.4.0.
- [ ] Nine-slice: built from stretched sprites (PR 11.5) as child entities, re-laid when
      `width` or `height` changes.
- [ ] Scroll view: the wheel and a drag move the content child, clamped to the content height.
- [ ] Tests: overlapping hitboxes, clip-hidden hitboxes, press-drag-release-elsewhere (not a
      click), disabled buttons, scroll clamping, and nine-slice piece sizes.

### PR 11.7: Demo, docs and release

- [ ] A UI demo screen in Tangle Run (from Options, say): a nine-slice panel, three buttons, a
      scrolling list of 30 wrapped-text rows, and a mouse cursor.
- [ ] `docs/ui.md`: screen space, clipping, hit-testing and consuming clicks.
- [ ] Changelog with the migration notes. Tag `v0.3.0`.

**Done when:** the demo works by mouse, Tangle Run still plays, CI passes on all presets, and
`v0.3.0` is tagged.

---

## Phase 12 – Platform: window, gamepad and audio (E2: `v0.4.0`, E3: `v0.5.0`)

The two releases are independent. Ship them together if both are ready at once.

### PR 12.1: Window and presentation settings (v0.4.0)

Files: `application.hpp` (`ApplicationConfig`), `graphics/window.hpp/.cpp`,
`graphics/renderer.hpp/.cpp`.

```cpp
enum class Scaling : std::uint8_t { Integer, Fit };          // Fit = letterbox, any scale
enum class WindowMode : std::uint8_t { Windowed, Borderless, Fullscreen };
struct DisplaySettings { WindowMode mode; int width, height; bool vsync; int frameCap;
                         Scaling scaling; };
struct DisplayRequest { std::optional<DisplaySettings> pending; };  // a world resource
struct DisplayInfo { DisplaySettings current; std::vector<Resolution> resolutions; }; // resource
```

- [x] `Renderer` takes the scaling mode instead of hardcoding `INTEGER_SCALE` (point 10), and
      can change it live.
- [x] VSync on or off, and a frame cap in the main loop (sleep until the next frame) for when
      VSync is off.
- [x] Borderless uses `SDL_SetWindowFullscreen` with the desktop mode. Exclusive fullscreen uses
      `SDL_SetWindowFullscreenMode`. The resolution list comes from `SDL_GetFullscreenDisplayModes`.
- [x] The game asks for a change through a resource (Q6). A settings screen sets
      `DisplayRequest::pending`. After each frame, `Application` applies it, clears it, and
      refreshes `DisplayInfo`, which the settings screen reads for the current mode and the
      resolution list. `Application` reaches the world through a small hook on `Game` (for
      example `virtual DisplayRequest* Display()`), which `ScheduledGame` implements from its
      resources. The game never calls into `Application` directly, so headless runs simply
      ignore the request.
- [x] Check `FixedTimestep`'s 0.25 s stall cap at x2 and x4 time scale. At x2, 0.25 s of real
      time is 30 updates a frame. Add a per-frame update limit if a slow frame can spiral.

### PR 12.2: Gamepads (v0.4.0)

Files: new `input/gamepad.hpp/.cpp`, `input/input.hpp`, `application.cpp`, `ui/` focus
navigation.

```cpp
enum class PadButton : std::uint8_t { South, East, West, North, Start, Back, L1, R1,
                                      LStick, RStick, DUp, DDown, DLeft, DRight, Count };
enum class PadFamily : std::uint8_t { Xbox, PlayStation, Switch, SteamDeck, Generic };
struct Gamepad { buttons + edges, sticks (deadzoned), triggers, PadFamily family; };
struct Input { ...; std::array<Gamepad, 4> pads; Device lastUsed; };  // Keyboard/Mouse/Pad
```

- [x] Hotplug through `SDL_EVENT_GAMEPAD_ADDED` and `REMOVED`.
- [x] Positional button names (South and so on), so the game maps glyphs by family. The family
      comes from `SDL_GetGamepadType`, plus a Steam Deck check by name or VID/PID.
- [x] `Input::lastUsed` changes on real input only (past the deadzone), for switching button
      icons.
- [x] UI focus navigation: the d-pad or stick moves `Focused` to the nearest `FocusOrder`
      neighbour in that direction, and South clicks it.
- [x] Tests: deadzones, edges, `lastUsed` switching, and directional focus picking.

**Done when (v0.4.0):** the demo screen is fully navigable by pad, the window modes switch live,
and 960 x 540 fills a 1280 x 800 window in fit mode.

### PR 12.3: Audio files (v0.5.0)

Files: `thirdparty/stb_vorbis/` (one vendored `.c` file and its licence), a `stb_vorbis` CMake
target **without** `engine::warnings` (point 9), `audio/sound.hpp/.cpp`, new
`audio/soundfile.hpp/.cpp`.

- [x] `LoadSound(path)` returns a `SoundResult`. WAV goes through `SDL_LoadWAV` and OGG through
      `stb_vorbis`. `Sound` keeps the file's own sample rate and channel count, and the voice
      stream is given that format, so SDL does the resampling. Don't pre-convert to 44.1 kHz.
- [x] Tests: decode a tiny committed WAV and OGG, and report a bad file as an error, not a crash.

### PR 12.4: Mixer, channels and music (v0.5.0)

Files: `audio/audiodevice.hpp/.cpp`, new `audio/music.hpp/.cpp`.

```cpp
enum class Bus : std::uint8_t { Music, Sfx, Ui, Count };
struct PlayOptions { Bus bus = Bus::Sfx; float volume = 1, pitchVariance = 0; int priority = 0; };
// AudioDevice::Play(const Sound&, PlayOptions); SetBusVolume(Bus, float);
// AudioDevice::PlayMusic(path, float fadeSeconds); StopMusic(float fadeSeconds);
```

- [x] Volume per voice uses `SDL_SetAudioStreamGain`, and pitch uses
      `SDL_SetAudioStreamFrequencyRatio`. Both exist in the vendored SDL3. A bus volume
      multiplies into each voice's gain.
- [x] Raise `kVoices` to 32, with priority stealing: when every voice is busy, a new sound
      replaces the lowest-priority, oldest voice.
- [x] **A change from the migration plan:** E3 lists per-sound cooldowns as engine work. They
      stay in the game, as part of its audio key table, because a cooldown is a rule about
      which sounds the game plays, not about mixing.
- [x] Music streams from disk. `stb_vorbis` decodes chunks inside an
      `SDL_SetAudioStreamGetCallback` callback, which runs on the audio thread, so a slow frame
      on the main loop can't starve the music. Two music streams make a crossfade. Loops are
      seamless.
- [x] Tests (SDL-free parts): voice-stealing choice, crossfade gain curve, bus volume maths.

**Done when (v0.5.0):** a test scene plays WAV and OGG clips on three buses with live volume,
and music crossfades between two tracks with no gap or click.

---

## Decisions

| # | Question | Decision | Where |
|---|---|---|---|
| Q1 | Release JSON and the RNG on their own, ahead of input and UI? | **Yes:** `v0.2.0`. Later releases move up by one | Release order, Phase 10 |
| Q2 | Split the engine into an SDL-free `engine_core` and the platform `engine`? | **Yes**, in v0.3.0 | PR 11.0 |
| Q3 | Screen space: a tag or a layer threshold? | **`ScreenSpace` tag**, inherited through the hierarchy | PR 11.4 |
| Q4 | Keep `FrameContext::keyboard` for one release? | **No, a clean break** in v0.3.0 | PR 11.1 |
| Q5 | Should the engine ship assets? | **No.** The font sheet is a game asset that each game copies | PR 11.3 |
| Q6 | How does the game change display settings? | **Through a resource** that `Application` applies after each frame | PR 12.1 |

Follow-up outside this repo: update the version numbers in
`project-rise/docs/babel-on-ecs-engine-plan.md` to match the table in "Release order".
