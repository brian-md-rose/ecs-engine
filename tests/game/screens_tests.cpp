#include <catch2/catch_test_macros.hpp>

#include <string>

#include "ecs/entity.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/font.hpp"
#include "input/keyboard.hpp"
#include "mazegame.hpp"
#include "screens.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::input::Key;
	using game::MenuCursor;
	using game::ScreenAction;
	using game::ScreenInput;
	using game::Stage;

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
		engine::input::Keyboard keyboard;

		[[nodiscard]] Stage CurrentStage() const { return maze.world.resources.Get<game::GameFlow>().stage; }
		[[nodiscard]] bool Paused() const { return maze.world.resources.Get<game::GameState>().paused; }

		void Update(int steps = 1)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, keyboard);
				keyboard.EndUpdate();
			}
		}

		void Tap(Key key)
		{
			keyboard.OnKeyDown(key);
			Update();
			keyboard.OnKeyUp(key);
			Update();
		}

		// Every screen text shown right now, one per line.
		[[nodiscard]] std::string ShownText()
		{
			std::string shown;
			maze.world.registry.Each<game::ScreenText, engine::graphics::Text>(
			    [&shown](Entity, const game::ScreenText&, const engine::graphics::Text& text)
			{
				if (!text.value.empty()) shown += text.value + "\n";
			});
			return shown;
		}

		[[nodiscard]] bool Shows(const std::string& text) { return ShownText().find(text) != std::string::npos; }
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

TEST_CASE("The title menu starts a game or quits", "[game][screens]")
{
	const auto start = game::StepScreens(MenuCursor{}, Stage::Title, false, ScreenInput{.confirm = true});
	CHECK(start.action == ScreenAction::StartGame);

	const auto moved = game::StepScreens(MenuCursor{}, Stage::Title, false, ScreenInput{.down = true});
	CHECK(moved.action == ScreenAction::None);
	CHECK(moved.cursor.title == 1);
	const auto quit = game::StepScreens(moved.cursor, Stage::Title, false, ScreenInput{.confirm = true});
	CHECK(quit.action == ScreenAction::Quit);

	// Pause does nothing on the title screen.
	const auto pause = game::StepScreens(MenuCursor{}, Stage::Title, false, ScreenInput{.pause = true});
	CHECK(pause.action == ScreenAction::None);
}

TEST_CASE("Pause opens the pause menu on Continue, in stages that can pause", "[game][screens]")
{
	const auto paused = game::StepScreens(MenuCursor{.pause = 1}, Stage::Playing, false, ScreenInput{.pause = true});
	CHECK(paused.action == ScreenAction::Pause);
	CHECK(paused.cursor.pause == 0);

	CHECK(game::CanPause(Stage::Ready));
	CHECK(game::CanPause(Stage::Dying));
	CHECK(game::CanPause(Stage::LevelComplete));
	CHECK_FALSE(game::CanPause(Stage::Title));
	CHECK_FALSE(game::CanPause(Stage::GameOver));
	const auto gameOver = game::StepScreens(MenuCursor{}, Stage::GameOver, false, ScreenInput{.pause = true});
	CHECK(gameOver.action == ScreenAction::None);
}

TEST_CASE("The pause menu resumes or goes back to the title", "[game][screens]")
{
	const auto pauseKey = game::StepScreens(MenuCursor{}, Stage::Playing, true, ScreenInput{.pause = true});
	CHECK(pauseKey.action == ScreenAction::Resume);
	const auto confirm = game::StepScreens(MenuCursor{}, Stage::Playing, true, ScreenInput{.confirm = true});
	CHECK(confirm.action == ScreenAction::Resume);

	const auto moved = game::StepScreens(MenuCursor{}, Stage::Playing, true, ScreenInput{.up = true});
	CHECK(moved.cursor.pause == 1);
	const auto quit = game::StepScreens(moved.cursor, Stage::Playing, true, ScreenInput{.confirm = true});
	CHECK(quit.action == ScreenAction::QuitToTitle);
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
	CHECK_FALSE(h.maze.WantsToQuit());

	h.Tap(Key::Enter);
	CHECK(h.CurrentStage() == Stage::Ready);
	CHECK(h.ShownText().empty());
}

TEST_CASE("Choosing Quit on the title screen asks the application to close", "[game][screens]")
{
	Harness h;
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
	CHECK(h.CurrentStage() == Stage::Ready);

	h.Tap(Key::Escape);
	h.Tap(Key::Down);
	h.Tap(Key::Enter); // Quit to title
	CHECK_FALSE(h.Paused());
	CHECK(h.CurrentStage() == Stage::Title);
	CHECK(h.Shows("> START"));
}
