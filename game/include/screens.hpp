#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

// The screens that sit on top of the game: title, options, name entry, the pause menu and the
// level transition. Each is spawned by an enter system when its state begins, as entities that
// carry DespawnOnExit, so they vanish when it ends (see gamestates.cpp for what is registered
// where). The decisions are pure functions (MoveSelection, options.hpp, nameentry.hpp); the
// input systems below apply them, and ScreenTextSystem keeps the changing text up to date.
namespace game
{
	inline constexpr std::uint8_t kMenuLayer = 20; // above the HUD, and left undimmed

	inline constexpr std::string_view kGameTitle = "TANGLE RUN";

	enum class TitleItem : std::uint8_t
	{
		Start,
		Options,
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
		std::size_t options = 0;
		std::size_t pause = 0;
	};

	// The item selected after up or down, wrapping round at either end.
	[[nodiscard]] std::size_t MoveSelection(std::size_t selected, std::size_t count, bool up, bool down) noexcept;

	// A menu line: "> LABEL" when selected, "  LABEL" when not, so items stay in line.
	[[nodiscard]] std::string MenuLine(std::string_view label, bool selected);

	// What a piece of changing screen text shows.
	enum class TextKind : std::uint8_t
	{
		TitleItem,  // a title menu line, with its cursor
		PauseItem,  // a pause menu line, with its cursor
		OptionItem, // an options line (OptionText), with its cursor
		NameSlot    // one character of the name being entered
	};

	// Component: text that follows the game's resources. item is the menu item or name slot it
	// belongs to; label is the menu item's own text. Text that never changes is a plain Text.
	struct ScreenText
	{
		TextKind kind = TextKind::TitleItem;
		std::string label{};
		std::size_t item = 0;
	};

	// Enter systems: spawn what a screen shows. Everything they create is scoped to the state
	// they are entered for, so leaving it removes it.
	void SpawnTitleScreen(engine::ecs::World& world, const engine::ecs::FrameContext& context);
	void SpawnOptionsScreen(engine::ecs::World& world, const engine::ecs::FrameContext& context);
	void SpawnNameEntryScreen(engine::ecs::World& world, const engine::ecs::FrameContext& context);
	void SpawnLevelTransitionScreen(engine::ecs::World& world, const engine::ecs::FrameContext& context);
	void SpawnPauseMenu(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Input systems, each for its own screen (see MazeGame for the run conditions).

	// Title: up and down choose, confirm acts (START begins a game, OPTIONS opens the options,
	// QUIT asks the game to close).
	void TitleSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Options: up and down choose; left, right or confirm change the item (BACK: confirm goes
	// back); pause (Escape) goes back to the title.
	void OptionsSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Name entry: edits the name (nameentry.hpp); confirm records the score under it, saves it
	// and goes on to GameOver.
	void NameEntrySystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// GameOver: confirm goes back to the title.
	void GameOverSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// While Playing: pause pauses, and on the pause menu resumes; up, down and confirm work the
	// menu (CONTINUE, or QUIT TO TITLE, which abandons the game).
	void PauseSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate (always): fills in every ScreenText from the cursor, the settings and the name.
	void ScreenTextSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
