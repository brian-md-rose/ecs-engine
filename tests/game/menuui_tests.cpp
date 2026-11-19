#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <string>

#include "audio/mixing.hpp"
#include "core/random.hpp"
#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "enemysystems.hpp"
#include "enemytuning.hpp"
#include "gameflow.hpp"
#include "graphics/transform.hpp"
#include "input/input.hpp"
#include "input/mouse.hpp"
#include "mazegame.hpp"
#include "menuui.hpp"
#include "options.hpp"
#include "scoring.hpp"
#include "screens.hpp"
#include "sounds.hpp"
#include "stage.hpp"
#include "tilemap.hpp"
#include "ui/ui.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Position;
	using engine::input::Key;
	using engine::input::MouseButton;
	using gametest::Stage;

	constexpr double kStep = 1.0 / 60.0;

	game::TileMap TestMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"
		                                                "#P....#\n"
		                                                "#######\n"
		                                                "#E#####\n"
		                                                "#######\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	struct Menus
	{
		game::MazeGame maze{TestMaze()};
		engine::input::Input input;

		Menus() { Update(2); }

		void Update(int steps = 1)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, input);
				input.EndUpdate();
			}
		}

		void Tap(Key key)
		{
			input.keyboard.OnKeyDown(key);
			Update();
			input.keyboard.OnKeyUp(key);
			Update();
		}

		void MoveTo(Position where)
		{
			input.mouse.OnMove(where, true);
			Update();
		}

		void Click(Position where)
		{
			MoveTo(where);
			input.mouse.OnButton(MouseButton::Left, true);
			Update();
			input.mouse.OnButton(MouseButton::Left, false);
			Update(3); // delivered at the end of the release frame, acted on in the next, then the state changes
		}

		[[nodiscard]] Stage CurrentStage() const { return gametest::StageOf(maze); }
		[[nodiscard]] game::MenuCursor& Cursor() { return maze.world.resources.Get<game::MenuCursor>(); }
		[[nodiscard]] game::Settings& Settings() { return maze.world.resources.Get<game::Settings>(); }

		// The middle of a button of a menu.
		[[nodiscard]] Position ButtonAt(game::MenuId menu, std::size_t item)
		{
			Position found{-100.0f, -100.0f};
			maze.world.registry.Each<game::MenuButton, engine::ui::Hitbox, engine::graphics::Transform>(
			    [&](Entity, const game::MenuButton& button, const engine::ui::Hitbox& box,
			        const engine::graphics::Transform& transform)
			{
				if (button.menu == menu && button.item == item)
				{
					found = {transform.position.x + box.width / 2.0f, transform.position.y + box.height / 2.0f};
				}
			});
			return found;
		}

		[[nodiscard]] std::size_t Buttons(game::MenuId menu)
		{
			std::size_t count = 0;
			maze.world.registry.Each<game::MenuButton>([&](Entity, const game::MenuButton& button)
			{
				if (button.menu == menu) ++count;
			});
			return count;
		}

		// The items of a menu that hold the Focused tag.
		[[nodiscard]] std::vector<std::size_t> FocusedItems(game::MenuId menu)
		{
			std::vector<std::size_t> items;
			maze.world.registry.Each<game::MenuButton, engine::ui::Focused>(
			    [&](Entity, const game::MenuButton& button, const engine::ui::Focused&)
			{
				if (button.menu == menu) items.push_back(button.item);
			});
			return items;
		}

		void OpenOptions()
		{
			Tap(Key::Down);
			Tap(Key::Down);
			Tap(Key::Enter);
		}
	};
} // namespace

TEST_CASE("Each menu is a column of buttons, and the selected one has focus", "[game][menuui]")
{
	Menus m;
	CHECK(m.Buttons(game::MenuId::Title) == game::kTitleItems);
	CHECK(m.FocusedItems(game::MenuId::Title) == std::vector<std::size_t>{0});
	m.Tap(Key::Down);
	CHECK(m.FocusedItems(game::MenuId::Title) == std::vector<std::size_t>{1});

	m.Tap(Key::Down);
	m.Tap(Key::Enter);
	REQUIRE(m.CurrentStage() == Stage::Options);
	CHECK(m.Buttons(game::MenuId::Title) == 0); // the title's buttons went with it
	CHECK(m.Buttons(game::MenuId::Options) == game::kOptionItems);
	CHECK(m.FocusedItems(game::MenuId::Options) == std::vector<std::size_t>{0});
}

TEST_CASE("Moving the pointer onto a button selects it, and keys carry on from there", "[game][menuui]")
{
	Menus m;
	m.MoveTo(m.ButtonAt(game::MenuId::Title, 2));
	CHECK(m.Cursor().title == 2);
	CHECK(m.FocusedItems(game::MenuId::Title) == std::vector<std::size_t>{2});

	// The pointer is still on OPTIONS, but it has not moved onto anything new, so the keys win.
	m.Tap(Key::Up);
	CHECK(m.Cursor().title == 1);
	m.Update(3);
	CHECK(m.Cursor().title == 1);

	m.MoveTo(m.ButtonAt(game::MenuId::Title, 3));
	CHECK(m.Cursor().title == 3);
}

TEST_CASE("Clicking a title button acts on it", "[game][menuui]")
{
	{
		Menus m;
		m.Click(m.ButtonAt(game::MenuId::Title, 2));
		CHECK(m.CurrentStage() == Stage::Options);
	}
	{
		Menus m;
		m.Click(m.ButtonAt(game::MenuId::Title, 1));
		CHECK(m.CurrentStage() == Stage::Help);
	}
	{
		Menus m;
		m.Click(m.ButtonAt(game::MenuId::Title, 3));
		CHECK(m.maze.WantsToQuit());
	}
	{
		Menus m;
		m.Click(m.ButtonAt(game::MenuId::Title, 0));
		CHECK(m.CurrentStage() == Stage::Ready);
	}
}

TEST_CASE("A click that misses every button does nothing", "[game][menuui]")
{
	Menus m;
	m.Click({4.0f, 4.0f});
	CHECK(m.CurrentStage() == Stage::Title);
	CHECK(m.Cursor().title == 0);
}

TEST_CASE("Clicking an option changes it, and BACK goes back", "[game][menuui][options]")
{
	Menus m;
	m.OpenOptions();
	REQUIRE(m.CurrentStage() == Stage::Options);

	m.Click(m.ButtonAt(game::MenuId::Options, static_cast<std::size_t>(game::OptionItem::Sound)));
	CHECK_FALSE(m.Settings().sound);
	m.Click(m.ButtonAt(game::MenuId::Options, static_cast<std::size_t>(game::OptionItem::Volume)));
	CHECK(m.Settings().volume == game::kDefaultVolume + 1);
	m.Click(m.ButtonAt(game::MenuId::Options, static_cast<std::size_t>(game::OptionItem::Lives)));
	CHECK(m.Settings().lives == game::kStartingLives + 1);

	m.Click(m.ButtonAt(game::MenuId::Options, static_cast<std::size_t>(game::OptionItem::Back)));
	CHECK(m.CurrentStage() == Stage::Title);
}

TEST_CASE("A click acts once, even when it lands with the confirm key", "[game][menuui]")
{
	Menus m;
	m.OpenOptions();
	m.MoveTo(m.ButtonAt(game::MenuId::Options, static_cast<std::size_t>(game::OptionItem::Lives)));
	m.input.mouse.OnButton(MouseButton::Left, true);
	m.input.keyboard.OnKeyDown(Key::Enter);
	m.Update();
	CHECK(m.Settings().lives == game::kStartingLives + 1);
}

TEST_CASE("The pause menu works with the pointer", "[game][menuui]")
{
	Menus m;
	gametest::RequestPlaying(m.maze);
	m.Update(2);
	m.Tap(Key::P);
	REQUIRE(gametest::IsPaused(m.maze));
	CHECK(m.Buttons(game::MenuId::Pause) == game::kPauseItems);

	m.Click(m.ButtonAt(game::MenuId::Pause, 0));
	CHECK_FALSE(gametest::IsPaused(m.maze));
	CHECK(m.Buttons(game::MenuId::Pause) == 0);

	m.Tap(Key::P);
	REQUIRE(gametest::IsPaused(m.maze));
	m.Click(m.ButtonAt(game::MenuId::Pause, 1));
	CHECK(m.CurrentStage() == Stage::Title);
}

TEST_CASE("Clicks do nothing while playing", "[game][menuui]")
{
	Menus m;
	gametest::RequestPlaying(m.maze);
	m.Update(2);
	m.Click({100.0f, 100.0f});
	CHECK_FALSE(gametest::IsPaused(m.maze));
}

TEST_CASE("The how-to-play screen is a scrolling list with a Back button", "[game][menuui][help]")
{
	Menus m;
	m.Tap(Key::Down);
	m.Tap(Key::Enter);
	REQUIRE(m.CurrentStage() == Stage::Help);
	CHECK(m.Buttons(game::MenuId::Help) == 1);

	engine::ui::ScrollView* view = nullptr;
	m.maze.world.registry.Each<engine::ui::ScrollView>([&view, &m](Entity entity, engine::ui::ScrollView&)
	{
		view = m.maze.world.registry.TryGetComponent<engine::ui::ScrollView>(entity);
	});
	REQUIRE(view != nullptr);
	CHECK(view->contentHeight > view->viewHeight); // it needs scrolling
	CHECK(view->offset == 0.0f);

	m.input.mouse.OnMove({100.0f, 120.0f}, true);
	m.input.mouse.OnWheel(-2.0f);
	m.Update(2);
	CHECK(view->offset > 0.0f);

	m.Click(m.ButtonAt(game::MenuId::Help, 0));
	CHECK(m.CurrentStage() == Stage::Title);
	CHECK(m.Cursor().title == 1); // the cursor is where it was
}

TEST_CASE("Escape and confirm leave the how-to-play screen", "[game][menuui][help]")
{
	Menus m;
	m.Tap(Key::Down);
	m.Tap(Key::Enter);
	REQUIRE(m.CurrentStage() == Stage::Help);
	m.Tap(Key::Escape);
	CHECK(m.CurrentStage() == Stage::Title);

	m.Tap(Key::Enter);
	REQUIRE(m.CurrentStage() == Stage::Help);
	m.Tap(Key::Enter);
	CHECK(m.CurrentStage() == Stage::Title);
}

TEST_CASE("The how-to-play text names the real scores", "[game][menuui][help]")
{
	std::string all;
	for (const std::string_view line : game::HelpLines())
		all += std::string(line) + "\n";
	CHECK(all.find(std::to_string(game::kPelletPoints) + " POINTS") != std::string::npos);
	CHECK(all.find(std::to_string(game::kPowerPelletPoints) + " AND") != std::string::npos);
}

TEST_CASE("Sound effects go on the effects bus, and the long ones win over the short", "[game][sounds]")
{
	using engine::audio::Bus;
	for (std::size_t i = 0; i < game::kSfxCount; ++i)
	{
		CHECK(game::PlayOptionsFor(static_cast<game::Sfx>(i)).bus == Bus::Sfx);
	}
	CHECK(game::PlayOptionsFor(game::Sfx::Death).priority > game::PlayOptionsFor(game::Sfx::Eat).priority);
	CHECK(game::PlayOptionsFor(game::Sfx::LevelClear).priority > game::PlayOptionsFor(game::Sfx::PowerUp).priority);
	CHECK(game::PlayOptionsFor(game::Sfx::Eat).pitchVariance > 0.0f);
	CHECK(game::PlayOptionsFor(game::Sfx::Death).pitchVariance == 0.0f);
}

TEST_CASE("Music plays when it is on and the volume is up", "[game][sounds]")
{
	CHECK(game::MusicWanted(true, 5));
	CHECK_FALSE(game::MusicWanted(false, 5));
	CHECK_FALSE(game::MusicWanted(true, 0));
	CHECK(game::VolumeGain(game::Settings{.volume = game::kMaxVolume}) == 1.0f);
	CHECK(game::VolumeGain(game::Settings{.volume = 0}) == 0.0f);
	CHECK(game::VolumeGain(game::Settings{.volume = 5}) == 0.5f);
}

TEST_CASE("The game has no music file until it loads one, and runs without an audio device", "[game][sounds]")
{
	Menus m;
	CHECK(m.maze.world.resources.Get<game::MusicState>().file.empty());
	m.Update(5); // MusicSystem returns early with no device
	CHECK_FALSE(m.maze.world.resources.Get<game::MusicState>().playing.has_value());
}

TEST_CASE("The enemies' dice are the engine's PCG32, so a seed plays the same on every compiler", "[game][enemies]")
{
	game::EnemyDice dice;
	engine::core::Random same(game::kEnemyRandomSeed);
	for (int i = 0; i < 8; ++i)
		CHECK(dice.random.Next() == same.Next());
}
