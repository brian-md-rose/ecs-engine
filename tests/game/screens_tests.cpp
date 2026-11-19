#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "graphics/font.hpp"
#include "hud.hpp"
#include "input/input.hpp"
#include "mazegame.hpp"
#include "nameentry.hpp"
#include "options.hpp"
#include "save.hpp"
#include "scoring.hpp"
#include "screens.hpp"
#include "stage.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::input::Key;
	using gametest::Stage;

	constexpr double kStep = 1.0 / 60.0;

	[[nodiscard]] game::TileMap TestMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"
		                                                "#P....#\n"
		                                                "#######\n"
		                                                "#E#####\n"
		                                                "#######\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	// The game with a keyboard, as in systems_tests.cpp.
	struct Harness
	{
		game::MazeGame maze{TestMaze()};
		engine::input::Input input;

		[[nodiscard]] Stage CurrentStage() const { return gametest::StageOf(maze); }
		[[nodiscard]] bool Paused() const { return gametest::IsPaused(maze); }
		[[nodiscard]] game::Settings& Settings() { return maze.world.resources.Get<game::Settings>(); }

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

		void GoTo(game::Screen screen)
		{
			maze.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(screen);
			Update(2);
		}

		// Every piece of screen text shown right now, one per line.
		[[nodiscard]] std::string ShownText()
		{
			std::string shown;
			maze.world.registry.Each<engine::graphics::Text>([&shown](Entity, const engine::graphics::Text& text)
			{
				if (text.layer == game::kMenuLayer && !text.value.empty()) shown += text.value + "\n";
			});
			return shown;
		}

		[[nodiscard]] bool Shows(const std::string& text) { return ShownText().find(text) != std::string::npos; }

		// What the name-entry slots show, one string per slot.
		[[nodiscard]] std::vector<std::string> Slots()
		{
			std::vector<std::string> slots(game::kNameLength);
			maze.world.registry.Each<game::ScreenText, engine::graphics::Text>(
			    [&slots](Entity, const game::ScreenText& screen, const engine::graphics::Text& text)
			{
				if (screen.kind == game::TextKind::NameSlot) slots[screen.item] = text.value;
			});
			return slots;
		}
	};
} // namespace

TEST_CASE("MoveSelection wraps round at both ends", "[game][screens]")
{
	CHECK(game::MoveSelection(0, 3, false, true) == 1);
	CHECK(game::MoveSelection(2, 3, false, true) == 0);
	CHECK(game::MoveSelection(0, 3, true, false) == 2);
	CHECK(game::MoveSelection(1, 3, true, true) == 1); // both at once cancel out
	CHECK(game::MoveSelection(9, 3, false, false) == 2);
	CHECK(game::MoveSelection(0, 0, false, true) == 0);
}

TEST_CASE("MenuLine marks the selected item and keeps items in line", "[game][screens]")
{
	CHECK(game::MenuLine("START", true) == "> START");
	CHECK(game::MenuLine("START", false) == "  START");
}

TEST_CASE("The game opens on the title screen and starts with Enter", "[game][screens]")
{
	Harness h;
	h.Update(5 * 60);
	CHECK(h.CurrentStage() == Stage::Title); // waits for a key
	CHECK(h.Shows(std::string(game::kGameTitle)));
	CHECK(h.Shows("> START"));
	CHECK(h.Shows("  OPTIONS"));
	CHECK_FALSE(h.maze.WantsToQuit());

	h.Tap(Key::Enter);
	CHECK(h.CurrentStage() == Stage::Ready);
	CHECK_FALSE(h.Shows(std::string(game::kGameTitle))); // the title's text went with the title
	CHECK(h.ShownText().empty());
}

TEST_CASE("The title screen shows the high score with its name once there is one", "[game][screens]")
{
	Harness h;
	h.Update(2);
	CHECK_FALSE(h.Shows("HIGH SCORE"));

	h.Tap(Key::Enter); // into the game...
	h.maze.world.resources.Get<game::HighScore>().record = 1200;
	h.maze.world.resources.Get<game::HighScore>().name = "ADA";
	h.Tap(Key::P);
	h.Tap(Key::Down);
	h.Tap(Key::Enter); // ...and out to the title (no new best)
	REQUIRE(h.CurrentStage() == Stage::Title);
	CHECK(h.Shows("HIGH SCORE"));
	CHECK(h.Shows("ADA  1200"));
}

TEST_CASE("Choosing Quit on the title screen asks the application to close", "[game][screens]")
{
	Harness h;
	h.Tap(Key::Down);
	h.Tap(Key::Down);
	CHECK(h.Shows("> QUIT"));
	h.Tap(Key::Space);
	CHECK(h.maze.WantsToQuit());
	CHECK(h.CurrentStage() == Stage::Title);
}

TEST_CASE("A game can be paused, resumed, and abandoned for the title", "[game][screens]")
{
	Harness h;
	h.Tap(Key::Enter);
	h.Tap(Key::P);
	CHECK(h.Paused());
	CHECK(h.Shows("PAUSED"));
	CHECK(h.Shows("> CONTINUE"));

	h.Tap(Key::Enter); // Continue
	CHECK_FALSE(h.Paused());
	CHECK_FALSE(h.Shows("PAUSED"));
	CHECK(h.CurrentStage() == Stage::Ready);

	h.Tap(Key::Escape);
	h.Tap(Key::Down);
	h.Tap(Key::Enter); // Quit to title
	CHECK_FALSE(h.Paused());
	CHECK(h.CurrentStage() == Stage::Title);
	CHECK(h.Shows("> START"));
}

TEST_CASE("Pause does nothing on the title screen", "[game][screens]")
{
	Harness h;
	h.Tap(Key::P);
	CHECK_FALSE(h.Paused());
	CHECK(h.CurrentStage() == Stage::Title);
}

TEST_CASE("OPTIONS opens the options screen, and Escape goes back to the title", "[game][screens][options]")
{
	Harness h;
	h.Tap(Key::Down);
	h.Tap(Key::Enter);
	REQUIRE(h.CurrentStage() == Stage::Options);
	CHECK(h.Shows("OPTIONS"));
	CHECK(h.Shows("> SOUND: ON"));
	CHECK(h.Shows("  LIVES: 3"));
	CHECK(h.Shows("  BACK"));
	CHECK_FALSE(h.Shows("START")); // the title's text is gone

	h.Tap(Key::Escape);
	CHECK(h.CurrentStage() == Stage::Title);
	CHECK(h.Shows("> OPTIONS")); // the cursor is where it was
	CHECK_FALSE(h.Shows("LIVES"));
}

TEST_CASE("The options change with left, right and Enter", "[game][screens][options]")
{
	Harness h;
	h.Tap(Key::Down);
	h.Tap(Key::Enter);
	REQUIRE(h.CurrentStage() == Stage::Options);

	h.Tap(Key::Enter); // SOUND toggles
	CHECK_FALSE(h.Settings().sound);
	CHECK(h.Shows("> SOUND: OFF"));
	h.Tap(Key::Left);
	CHECK(h.Settings().sound);

	h.Tap(Key::Down);
	h.Tap(Key::Right); // LIVES up
	CHECK(h.Settings().lives == game::kStartingLives + 1);
	h.Tap(Key::Left);
	h.Tap(Key::Left);
	CHECK(h.Settings().lives == game::kStartingLives - 1);
	CHECK(h.Shows("> LIVES: 2"));
	h.Tap(Key::Enter);
	h.Tap(Key::Enter);
	CHECK(h.Settings().lives == game::kStartingLives + 1);

	h.Tap(Key::Down); // UI DEMO: left and right do nothing
	h.Tap(Key::Right);
	CHECK(h.CurrentStage() == Stage::Options);

	h.Tap(Key::Down); // BACK: left and right do nothing, Enter goes back
	h.Tap(Key::Right);
	CHECK(h.CurrentStage() == Stage::Options);
	h.Tap(Key::Enter);
	CHECK(h.CurrentStage() == Stage::Title);
}

TEST_CASE("The options wrap round with up and down", "[game][screens][options]")
{
	Harness h;
	h.Tap(Key::Down);
	h.Tap(Key::Enter);
	h.Tap(Key::Up);
	CHECK(h.Shows("> BACK"));
	h.Tap(Key::Down);
	CHECK(h.Shows("> SOUND"));
}

TEST_CASE("The options are saved when the options screen is left", "[game][screens][options]")
{
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / "ecs_engine_game_options_save";
	std::filesystem::remove_all(folder);
	Harness h;
	h.maze.world.resources.Get<game::SaveFile>().path = folder / "save.txt";

	h.Tap(Key::Down);
	h.Tap(Key::Enter);
	h.Tap(Key::Enter);                                         // sound off
	CHECK_FALSE(std::filesystem::exists(folder / "save.txt")); // not until the screen is left
	h.Tap(Key::Escape);

	const game::SavedGame saved = game::LoadSavedGame(folder / "save.txt");
	CHECK_FALSE(saved.settings.sound);
	std::filesystem::remove_all(folder);
}

TEST_CASE("The name-entry screen shows the slots, edited with the arrow keys", "[game][screens][nameentry]")
{
	Harness h;
	h.GoTo(game::Screen::NameEntry);
	REQUIRE(h.CurrentStage() == Stage::NameEntry);
	CHECK(h.Shows("NEW HIGH SCORE!"));
	CHECK(h.Shows("ENTER YOUR NAME"));

	std::vector<std::string> slots = h.Slots();
	CHECK(slots[0] == "A");
	CHECK(slots[1] == "-"); // a blank slot
	CHECK(slots[7] == "-");

	h.Tap(Key::Up);
	h.Tap(Key::Right);
	h.Tap(Key::Up);
	slots = h.Slots();
	CHECK(slots[0] == "B");
	CHECK(slots[1] == "A");
}

TEST_CASE("The level transition shows the next level and the score", "[game][screens]")
{
	Harness h;
	h.Tap(Key::Enter);
	h.maze.world.resources.Get<game::Score>().points = 345;
	h.GoTo(game::Screen::LevelTransition);
	REQUIRE(h.CurrentStage() == Stage::LevelTransition);
	CHECK(h.Shows("LEVEL 2"));
	CHECK(h.Shows("SCORE 345"));

	h.Update(static_cast<int>(game::kLevelTransitionSeconds * 60.0f) + 5);
	CHECK(h.CurrentStage() == Stage::Ready);
	CHECK_FALSE(h.Shows("LEVEL 2"));
}
