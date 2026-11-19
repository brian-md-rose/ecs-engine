#pragma once
#include <cstdint>
#include <string>

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
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

	// Resource: what the demo remembers.
	struct UiDemoState
	{
		int clicks = 0;
	};

	// The text of list row number row (from 1): some short, some long enough to wrap.
	[[nodiscard]] std::string DemoRowText(int row);

	// The counter's text, "CLICKS: 3".
	[[nodiscard]] std::string DemoCounterText(int clicks);

	// Enter system for Screen::UiDemo: the panel, the counter, the buttons, the list and the cursor,
	// all scoped to the state so leaving it takes them away.
	void SpawnUiDemoScreen(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Input, after the UI systems: pause (Escape) goes back to Options.
	void UiDemoSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate: the buttons' pictures follow hover and press, the counter text follows the
	// resource, and the cursor sprite follows the mouse (and hides when it leaves the window).
	void UiDemoLookSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Event handler for ui::Clicked: acts on a demo button.
	void OnDemoClick(engine::ecs::World& world, const engine::ui::Clicked& event);
} // namespace game
