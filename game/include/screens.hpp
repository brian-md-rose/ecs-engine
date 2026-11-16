#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "gameflow.hpp"

// The title screen (start a game, or quit) and the pause screen (carry on, or give up and go
// back to the title), on top of the state machine in gameflow.hpp. The decisions are pure
// functions (StepScreens); ScreenSystem applies them and ScreenTextSystem shows them.
namespace game
{
	inline constexpr std::uint8_t kMenuLayer = 20; // above the HUD, and left undimmed

	inline constexpr std::string_view kGameTitle = "TANGLE RUN";

	enum class TitleItem : std::uint8_t
	{
		Start,
		Quit,
		Count
	};

	enum class PauseItem : std::uint8_t
	{
		Resume,
		QuitToTitle,
		Count
	};

	inline constexpr std::size_t kTitleItems = static_cast<std::size_t>(TitleItem::Count);
	inline constexpr std::size_t kPauseItems = static_cast<std::size_t>(PauseItem::Count);

	// Resource: which item each menu has selected.
	struct MenuCursor
	{
		std::size_t title = 0;
		std::size_t pause = 0;
	};

	// This update's menu keys.
	struct ScreenInput
	{
		bool up = false;
		bool down = false;
		bool confirm = false;
		bool pause = false;
	};

	enum class ScreenAction : std::uint8_t
	{
		None,
		StartGame,  // title: a new game
		Quit,       // title: close the game
		Pause,      // during a game
		Resume,     // pause screen: carry on
		QuitToTitle // pause screen: abandon the game
	};

	struct ScreenStep
	{
		MenuCursor cursor{};
		ScreenAction action = ScreenAction::None;
	};

	// The item selected after up or down, wrapping round at either end.
	[[nodiscard]] std::size_t MoveSelection(std::size_t selected, std::size_t count, bool up, bool down) noexcept;

	// A game can be paused in every stage but Title and GameOver, which wait for a key anyway.
	[[nodiscard]] bool CanPause(Stage stage) noexcept;

	// One update of the menus. On the title screen, up and down choose and confirm acts. While
	// paused, the same on the pause menu, and pause resumes. Otherwise, pause pauses (when
	// CanPause) and opens the pause menu on its first item. Pure.
	[[nodiscard]] ScreenStep StepScreens(MenuCursor cursor, Stage stage, bool paused,
	                                     const ScreenInput& input) noexcept;

	// A menu line: "> LABEL" when selected, "  LABEL" when not, so items stay in line.
	[[nodiscard]] std::string MenuLine(std::string_view label, bool selected);

	// Which screen a piece of text belongs to.
	enum class Screen : std::uint8_t
	{
		Title,
		Pause
	};

	// Component: text shown only while its screen is up. A menu item (item < its menu's count)
	// shows the cursor when selected; anything else (a heading, a hint) is shown as it is.
	struct ScreenText
	{
		Screen screen = Screen::Title;
		std::string label{};
		std::size_t item = static_cast<std::size_t>(-1);
	};

	// Setup: creates the title and pause screens' text. Creates entities directly, so call it
	// during setup.
	void SpawnScreens(engine::ecs::Registry& registry);

	// Input, after ControlsSystem: steps the menus and carries out what they say (starts a game,
	// asks to quit, pauses, resumes, goes back to the title).
	void ScreenSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate (always): shows the text of the screen that is up, and hides the rest.
	void ScreenTextSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
