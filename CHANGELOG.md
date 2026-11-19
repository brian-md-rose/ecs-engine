# Changelog

All notable changes to the engine are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). While the major version is 0, minor releases
may contain breaking changes.

To cut a release, see [docs/releasing.md](docs/releasing.md).

## [Unreleased]

### Changed

- Tangle Run's title, how-to-play, options, name entry, level transition and pause screens are laid out by scene files in `game/assets/scenes/` (the game's own change; see `docs/screens.md`), so they open and play in the editor. They look and behave as before.

## [0.8.0] - 2026-11-19

### Added

- Undo and redo in the standalone editor (Edit menu, Ctrl+Z, Ctrl+Y): each finished change is a step, kept as the scene text; `EditorGame::Undo`, `Redo`, `CanUndo` and `CanRedo`.
- A Viewport panel in the standalone editor: the game is drawn into a render target and shown with zoom and pan, click-to-pick (`PickEntity`, `EntityScreenRect`) and a selection outline, and drag to move (`MoveEntity`). `Renderer::CreateRenderTarget` and `SetRenderTarget`, and `Editor::SetPanelSize`.
- `EditorGame` (`editor/editorgame.hpp`): the standalone editor, with an edit world and a play world, a Scene menu (New, Open, Save, Save as, unsaved-changes prompt) and Play/Stop that runs a copy and discards its changes. `tangle_run_editor` runs it by default; `--overlay` gives the old F1 overlay. `Editor::SetMenuBar` lets a host add to the menu bar, and `AddEntityPanels` can take a function that returns the current world.

## [0.7.0] - 2026-11-19

### Added

- An optional editor overlay: Dear ImGui as a submodule (`thirdparty/imgui`), the `engine::editor` library, and `tangle_run_editor`, all behind `ENGINE_BUILD_EDITOR` (off by default). F1 shows the overlay. `Application::SetEditorHost` and `EditorHost` (`application/editorhost.hpp`) are the hook, and cost nothing without a host. See `docs/editor.md`.
- Editor panels: `AddEntityPanels` adds a Hierarchy (tree, filter, drag to reparent, add child, duplicate, delete) and an Inspector (component fields as widgets from the type registry, add and remove components). `editor/entityops.hpp` has the operations behind them.
- `RegisterEngineComponents` also registers `Text` and the UI components (`NinePatch`, `Hitbox`, `Interactable`, `FocusOrder`, `ScrollView`), so a scene can hold a screen's layout.

### Changed

- Tangle Run's UI demo screen is laid out by `game/assets/scenes/uidemo.json` instead of spawn code (the game's own change; see `docs/scenes.md`).

## [0.6.0] - 2026-11-19

### Added

- `ecs::TypeRegistry` (`ecs/typeregistry.hpp`): component types described at run time, with a name and fields, that save to and load from `core::JsonValue`. One explicit registration block per component; `FieldCodec<T>` says how a field type is written (built in: bool, integers, floats, `std::string`, `Entity`, `std::vector<Entity>`, enums by name, and `Color`, `Position`, `PixelRect`). Entities are written as ids through `IEntityIds`, and problems come back with a JSON path. Opt-in: `Registry` is unchanged.
- `ecs::Name`, and `ecs::RegisterEngineComponents` for the engine's own components.
- `ecs::SaveScene`, `LoadScene`, `SaveSceneFile` and `LoadSceneFile` (`ecs/scene.hpp`): entities and their registered components as JSON (format 1), with scene-local ids remapped on load, `name` and `parent` keys, errors with JSON paths, unknown components as warnings, and a failed load undone whole. See `docs/scenes.md` and ADR 0004.
- `Registry::AllEntities()`, every live entity in order of id.
- `AudioDevice::Update()`, called once a frame by `Application`, releases music tracks that have finished fading out
  (they were only released by the next `PlayMusic` or `StopMusic`), and `FadingTrackCount()`.
- `audio::DecodedSizeAllowed` and `kMaxDecodedSoundBytes`: `LoadSound` refuses an Ogg that would unpack to more than
  256 MB of samples, checking its length before decoding it.
- `VoiceState::usable`: `ChooseVoice` never picks a voice whose stream could not be made.

### Fixed

- `PlayMusic` with no audio device now says "no audio device" in `error` instead of an empty string.
- A music fade request is one atomic word, so the audio thread can't read half of one request and half of the next.

### Changed

- Tangle Run now uses what Phases 10 to 12 added to the engine. Its menus (title, options, pause) are UI panels and
  buttons that the mouse can hover and click, and a new HOW TO PLAY screen is a scroll view. Sound effects play on the
  effects bus with priorities (the death wail and level fanfare are never cut off by pellet ticks, which vary a little in
  pitch), there is looping Ogg music with fades, and the options gain MUSIC and VOLUME. The enemies' random turns come from
  `engine::core::Random` instead of `std::mt19937`.

## [0.5.0] - 2026-11-19

### Added

- `audio/soundfile.hpp`: `LoadSound(path)` returns a `SoundResult` (the `Sound`, or an error message; a bad file never
  crashes). `.wav` goes through `SDL_LoadWAV` and `.ogg` through the vendored stb_vorbis (`thirdparty/stb_vorbis`, built
  without the engine's warning flags). The `Sound` keeps the file's own sample rate and channel count.
- `audio::Sound` gains `sampleRate` and `channels` (defaults match `Synthesize`: mono, 44.1 kHz) and `Frames()`.
- `audio/mixing.hpp` (no SDL): `Bus` (`Music`, `Sfx`, `Ui`), `PlayOptions` (bus, volume, pitch variance, priority) and the
  mixer's decisions as pure functions: `ChooseVoice` (free voice, else the oldest of the lowest priority), `VoiceGain`,
  `PitchRatio`, `Crossfade`, `FadeGain` and `EqualPowerFade`.
- `AudioDevice::Play(sound, PlayOptions)`, `SetBusVolume`, `SetMasterVolume`, and music: `PlayMusic(path, fadeSeconds, error)`
  and `StopMusic(fadeSeconds)`. Music streams Ogg Vorbis from memory on the audio thread (`audio/music.hpp`, `MusicTrack`),
  loops without a gap and crossfades on an equal-power curve. See [docs/audio.md](docs/audio.md).

### Changed

- `AudioDevice` opens a stereo device and gives each voice the sound's own format, so SDL resamples and remixes it. Mono
  sounds play on both sides as before.
- `AudioDevice` has 32 voices instead of 8, and a new sound when all are busy replaces the oldest of the lowest priority
  (or is dropped if every voice matters more) instead of the next in turn.

## [0.4.0] - 2026-11-19

### Added

- `graphics/display.hpp`: display settings. `DisplaySettings` (window mode `Windowed`, `Borderless` or `Fullscreen`, size, VSync,
  frame cap, `Scaling::Integer` or `Fit`), asked for through the `DisplayRequest` resource and reported in `DisplayInfo`,
  which `Application` applies and refreshes after each frame through the new `Game::Display()` hook (`ScheduledGame`
  implements it from the world's resources). `Window::SetMode` and `Resolutions`, `Renderer::SetScaling` and `SetVSync`, and a
  frame cap in the main loop for when VSync is off. `ApplicationConfig` gains `scaling`, `vsync`, `frameCap` and
  `maxUpdatesPerFrame`; `Renderer`'s constructor takes an optional `Scaling`. Pure helpers `Presentation`,
  `NormalizeResolutions`, `FrameDelaySeconds` and `Sanitized`. See [docs/display.md](docs/display.md).
- `core::FixedTimestep::SetMaxUpdates`: limits the updates one frame runs and drops the backlog beyond it, so a slow frame at a time
  scale above 1 can't spiral. `Application` sets it to 15 by default (what a stall gets at normal speed).
- Tangle Run: DISPLAY and SCALING options, saved with the other settings.
- `input/gamepad.hpp`: gamepads. `PadButton` named by position, `PadFamily`, `Gamepad` with latched edges, circular deadzones and
  stick-as-direction helpers, and `Input::pads` (four slots), `Input::lastUsed` and `Device`. `Application` handles hotplug.
  See [docs/gamepad.md](docs/gamepad.md).
- `ui::PickFocusNeighbour`: `UiFocusSystem` moves `Focused` by d-pad or left stick and clicks on South;
  `ScrollView::padSpeed` lets the right stick scroll a focused view.
- Tangle Run: pad bindings in `Controls` (South confirms, Start and Back pause, d-pad and left stick move), and a UI demo
  screen that is fully navigable by pad with hints for the pad family in use.

## [0.3.0] - 2026-11-19

The JSON and random number work (Phase 10) and the UI work (Phase 11) of [docs/babel-support.md](docs/babel-support.md) in one release: there is no 0.2.0. Games moving from 0.1.0 should read the **Breaking** entries under Changed first.

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
- `graphics::Camera` gains `zoom` (whole numbers; sprites, text and shapes come out that many times bigger about `position`),
  `offset` (screen shake that isn't zoomed), `WorldToScreen`, `ScreenToWorld`, `ViewSize`, and `FocusOn(camera, point, zoom)`.
  The `ScreenSpace` tag (`graphics/screenspace.hpp`) makes an entity ignore the camera; its children, through
  `ecs::Parent`, inherit it, so a HUD panel needs one tag. A `Text`'s `wrapWidth` is in world units, so it zooms too.
- `graphics/clip.hpp`: the `ClipRect` component confines an entity and its descendants to a rectangle of screen pixels; nested
  clips intersect and things wholly outside are culled. `DrawBatch::Runs()` splits the indices wherever the clip changes (in
  layer order, so a clip never reorders what is drawn), and `Renderer2D` sets SDL's clip rect once per run. `Sprite` gains
  `width` and `height` to stretch a picture to a size (0 keeps its own), for panel pieces and bars.
- `ui/`: mechanisms for screens, with no look of their own. `Hitbox` and `Interactable` give `Hovered`, `Pressed` and a
  `Clicked` event (press and release on the same enabled entity; the top-most hitbox by layer wins, blocking panels hide
  what is under them, and clipped-out parts don't count, in world or screen space). `PointerState::clickConsumed` lets a world
  click, like placing a tower, skip a click the UI took. `FocusOrder` and `Focused` give Tab and Shift+Tab navigation,
  `NinePatch` draws a stretchable panel from nine child sprites (`NinePatchLayout` is the pure part), and `ScrollView` scrolls
  its content child by wheel or drag, clamped. Systems: `UiPointerSystem`, `UiFocusSystem`, `ScrollViewSystem`,
  `NinePatchSystem` (which also destroys the pieces of a nine-patch whose entity is gone). Phase 11 of
  [docs/babel-support.md](docs/babel-support.md); see [docs/ui.md](docs/ui.md).
- `Game::ShowSystemCursor()` (default `true`): return `false` to hide the operating system's mouse pointer while the game draws
  its own. `Application` applies it each frame and restores the pointer on exit.
- Tangle Run: a UI demo screen (OPTIONS, then UI DEMO) with a nine-slice panel, three buttons, a clipped list of 30
  wrapped rows that scrolls by wheel or drag, and a cursor sprite; the new art is in `sprites.txt` and its PNG sheet.

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

[Unreleased]: https://github.com/brian-md-rose/ecs-engine/compare/v0.8.0...HEAD
[0.8.0]: https://github.com/brian-md-rose/ecs-engine/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/brian-md-rose/ecs-engine/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/brian-md-rose/ecs-engine/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/brian-md-rose/ecs-engine/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/brian-md-rose/ecs-engine/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/brian-md-rose/ecs-engine/compare/v0.1.0...v0.3.0
[0.1.0]: https://github.com/brian-md-rose/ecs-engine/releases/tag/v0.1.0
