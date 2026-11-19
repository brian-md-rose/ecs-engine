#pragma once
#include <cstdint>
#include <string>

#include "assets.hpp"
#include "core/json.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/typeregistry.hpp"
#include "ecs/world.hpp"
#include "input/input.hpp"
#include "ui/ui.hpp"

// The UI demo screen, opened from Options: the engine's UI module (ui/ui.hpp) put to work with
// the mouse. A nine-slice panel holds a click counter, three buttons (COUNT, RESET, BACK), and a
// list of 30 wrapped-text rows that scrolls with the wheel or by dragging. A cursor sprite
// follows the pointer in place of the system's. See docs/ui.md for how each part works.
namespace game
{
	// Layers, low to high: the panel, the list's background, the rows and buttons, their text, the cursor.
	inline constexpr std::uint8_t kDemoPanelLayer = 25;
	inline constexpr std::uint8_t kDemoListLayer = 26;
	inline constexpr std::uint8_t kDemoRowLayer = 27;
	inline constexpr std::uint8_t kDemoTextLayer = 28;
	inline constexpr std::uint8_t kDemoCursorLayer = 60;

	inline constexpr int kDemoRows = 30;
	inline constexpr int kDemoButtons = 3;

	enum class DemoAction : std::uint8_t
	{
		Count, // add one to the counter
		Reset, // set it back to 0
		Back   // return to Options
	};

	// Components: a button of the demo, what the counter text shows, and the mouse cursor sprite.
	struct DemoButton
	{
		DemoAction action = DemoAction::Count;
	};
	struct DemoCounter
	{
	};
	struct DemoCursor
	{
	};
	// The background frame of the list, which lights up while the list has focus.
	struct DemoListFrame
	{
	};
	// One of the two lines of help at the bottom; which keys it names follows the device last used.
	struct DemoHint
	{
		int line = 0; // 0 or 1
	};

	// Resource: what the demo remembers.
	// Component: which of the game's pictures (a SpriteClips name: "panel", "button", "cursor") a NinePatch or Sprite
	// shows. A scene file names its pictures this way, since where they sit in the atlas is only known
	// once the assets are loaded; ResolveClipNames fills in the rectangle.
	struct ClipName
	{
		std::string value;
	};

	struct UiDemoState
	{
		int clicks = 0;
	};

	// Resource: the UI demo screen's scene file, parsed (assets/scenes/uidemo.json). Null until loaded.
	struct UiDemoScene
	{
		engine::core::JsonValue json;
	};

	// The text of list row number row (from 1): some short, some long enough to wrap.
	// Describes the UI demo's components (and ClipName) to a type registry, so its scene file can be loaded.
	void RegisterUiDemoComponents(engine::ecs::TypeRegistry& types);

	// Sets the source rectangle of every NinePatch and Sprite that has a ClipName from the clip of that name.
	// An unknown name is left as it is (empty, which draws nothing).
	void ResolveClipNames(engine::ecs::Registry& registry, const SpriteClips& clips);

	[[nodiscard]] std::string DemoRowText(int row);

	// A line (0 or 1) of the help text: the mouse and keyboard controls, or the pad's, named with the buttons
	// of the pad's family ("A: PRESS" on an Xbox pad, "CROSS: PRESS" on a PlayStation one).
	[[nodiscard]] std::string DemoHintText(int line, engine::input::Device device, engine::input::PadFamily family);

	// The counter's text, "CLICKS: 3".
	[[nodiscard]] std::string DemoCounterText(int clicks);

	// Enter system for Screen::UiDemo: the panel, the counter, the buttons, the list and the cursor,
	// all scoped to the state so leaving it takes them away.
	void SpawnUiDemoScreen(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Input, after the UI systems: pause (Escape) goes back to Options.
	void UiDemoSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate: the buttons' pictures follow hover, press and focus, the list lights up when focused, the help
	// text follows the device last used, the counter text follows the resource, and the cursor sprite follows
	// the mouse (and hides when it leaves the window). With a pad: the d-pad or left stick moves focus between
	// the buttons and the list, South presses, the right stick scrolls the focused list.
	void UiDemoLookSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Event handler for ui::Clicked: acts on a demo button.
	void OnDemoClick(engine::ecs::World& world, const engine::ui::Clicked& event);
} // namespace game
