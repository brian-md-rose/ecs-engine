#include <catch2/catch_test_macros.hpp>

#include <string>

#include "ecs/entity.hpp"
#include "gameflow.hpp"
#include "graphics/font.hpp"
#include "hud.hpp"
#include "input/keyboard.hpp"
#include "mazegame.hpp"
#include "scoring.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using game::Align;
	using game::HudField;
	using game::HudView;
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

	// The text the HUD shows for field.
	[[nodiscard]] std::string Shown(game::MazeGame& maze, HudField field)
	{
		std::string shown;
		maze.world.registry.Each<game::HudText, engine::graphics::Text>(
		    [&](Entity, const game::HudText& hud, const engine::graphics::Text& text)
		{
			if (hud.field == field) shown = text.value;
		});
		return shown;
	}

	void Update(game::MazeGame& maze, int steps = 1)
	{
		engine::input::Keyboard keyboard;
		for (int i = 0; i < steps; ++i)
		{
			maze.Update(kStep, keyboard);
		}
	}
} // namespace

TEST_CASE("HudLine shows the numbers as they are", "[game][hud]")
{
	const HudView view{.score = 1230, .highScore = 4500, .level = 3, .lives = 2, .stage = Stage::Playing};
	CHECK(game::HudLine(HudField::Score, view) == "1230");
	CHECK(game::HudLine(HudField::HighScore, view) == "4500");
	CHECK(game::HudLine(HudField::Level, view) == "3");
	CHECK(game::HudLine(HudField::Lives, view) == "@@");
	CHECK(game::HudLine(HudField::Lives, HudView{.lives = -1}).empty());
}

TEST_CASE("HudLine's message follows the stage", "[game][hud]")
{
	CHECK(game::HudLine(HudField::Message, HudView{.stage = Stage::Ready}) == "READY!");
	CHECK(game::HudLine(HudField::Message, HudView{.stage = Stage::Playing}).empty());
	CHECK(game::HudLine(HudField::Message, HudView{.stage = Stage::Dying}).empty());
	CHECK(game::HudLine(HudField::Message, HudView{.stage = Stage::LevelComplete}) == "WELL DONE!");
	CHECK(game::HudLine(HudField::Message, HudView{.stage = Stage::GameOver}) == "GAME OVER");
}

TEST_CASE("AlignedX places text against its anchor on whole pixels", "[game][hud]")
{
	CHECK(game::AlignedX(16.0f, Align::Left, 30) == 16.0f);
	CHECK(game::AlignedX(112.0f, Align::Centre, 30) == 97.0f);
	CHECK(game::AlignedX(112.0f, Align::Centre, 31) == 96.0f);
	CHECK(game::AlignedX(208.0f, Align::Right, 30) == 178.0f);
}

TEST_CASE("The HUD follows the score, the high score, the lives and the level", "[game][hud]")
{
	game::MazeGame maze{TestMaze()};
	Update(maze);
	CHECK(Shown(maze, HudField::Message).empty()); // the title screen has its own text

	maze.world.resources.Get<game::GameFlow>() = game::GameFlow{};
	Update(maze);
	CHECK(Shown(maze, HudField::Score) == "0");
	CHECK(Shown(maze, HudField::Level) == "1");
	CHECK(Shown(maze, HudField::Lives) == std::string(static_cast<std::size_t>(game::kStartingLives), '@'));
	CHECK(Shown(maze, HudField::Message) == "READY!");

	maze.world.resources.Get<game::Score>().points = 250;
	maze.world.resources.Get<game::GameFlow>().lives = 1;
	maze.world.resources.Get<game::GameFlow>().level = 4;
	Update(maze);
	CHECK(Shown(maze, HudField::Score) == "250");
	CHECK(Shown(maze, HudField::HighScore) == "250");
	CHECK(Shown(maze, HudField::Lives) == "@");
	CHECK(Shown(maze, HudField::Level) == "4");

	// The high score stays when the score starts again.
	maze.world.resources.Get<game::Score>().points = 10;
	Update(maze);
	CHECK(Shown(maze, HudField::Score) == "10");
	CHECK(Shown(maze, HudField::HighScore) == "250");
	CHECK(maze.world.resources.Get<game::HighScore>().points == 250);
}
