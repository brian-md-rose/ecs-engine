#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#include "assets.hpp"
#include "components.hpp"
#include "difficulty.hpp"
#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "enemytuning.hpp"
#include "gameflow.hpp"
#include "input/keyboard.hpp"
#include "mazegame.hpp"
#include "stage.hpp"
#include "tilemap.hpp"

namespace
{
	using game::DifficultyResult;
	using game::ScheduledMode;

	// Set by tests/CMakeLists.txt to the source folder, so the tests read the files as written.
	const std::filesystem::path kAssets{GAME_ASSETS_DIR};

	constexpr std::string_view kTwoLevels = "# a comment\n"
	                                        "\n"
	                                        "level 1  7.5 6.5 4 7   6 18 6 24 4\n"
	                                        "level 2  8   7.5 5 3.5 4 30 2 40 1\n";

	// The sum of a schedule's timed stretches.
	[[nodiscard]] float TimedSeconds(const game::ModeSchedule& schedule)
	{
		float total = 0.0f;
		for (const game::ModePhase& phase : schedule)
		{
			total += phase.seconds;
		}
		return total;
	}

	[[nodiscard]] float PatrolSeconds(const game::ModeSchedule& schedule)
	{
		float total = 0.0f;
		for (const game::ModePhase& phase : schedule)
		{
			if (phase.mode == ScheduledMode::Patrol) total += phase.seconds;
		}
		return total;
	}
} // namespace

TEST_CASE("A levels file gives each level its speeds, fright and schedule", "[game][difficulty]")
{
	const DifficultyResult result = game::ParseDifficulty(kTwoLevels);
	INFO(result.error.message);
	REQUIRE(result.table.has_value());
	REQUIRE(result.table->LevelCount() == 2);

	const game::LevelTuning& first = result.table->ForLevel(1);
	CHECK(first.playerSpeed == 7.5f);
	CHECK(first.enemySpeed == 6.5f);
	CHECK(first.frightenedSpeed == 4.0f);
	CHECK(first.frightSeconds == 7.0f);
	CHECK(first.schedule[0].mode == ScheduledMode::Patrol);
	CHECK(first.schedule[0].seconds == 6.0f);
	CHECK(first.schedule[1].mode == ScheduledMode::Chase);
	CHECK(first.schedule[1].seconds == 18.0f);
	CHECK(first.schedule[4].seconds == 4.0f);
	CHECK(first.schedule[5].mode == ScheduledMode::Chase);
	CHECK(first.schedule[5].seconds == 0.0f); // for good

	CHECK(result.table->ForLevel(2).frightSeconds == 3.5f);
}

TEST_CASE("Levels below 1 play like level 1, and past the table like the last", "[game][difficulty]")
{
	const DifficultyResult result = game::ParseDifficulty(kTwoLevels);
	REQUIRE(result.table.has_value());
	CHECK(&result.table->ForLevel(0) == &result.table->ForLevel(1));
	CHECK(&result.table->ForLevel(-5) == &result.table->ForLevel(1));
	CHECK(&result.table->ForLevel(9) == &result.table->ForLevel(2));
}

TEST_CASE("Levels file errors give the line and column", "[game][difficulty]")
{
	struct Case
	{
		std::string text;
		std::size_t line;
		std::size_t column;
	};
	const std::array cases = {
	    Case{"stage 1 7 6 4 7 6 18 6 24 4\n", 1, 1}, // not a level line
	    Case{"level 1 7 6 4 7 6 18 6 24\n", 1, 0},   // a number missing
	    Case{"level 2 7 6 4 7 6 18 6 24 4\n", 1, 7}, // doesn't start at 1
	    Case{"level 1 7 6 4 7 6 18 6 24 4\n"
	         "level 3 7 6 4 7 6 18 6 24 4\n",
	         2, 7},                                    // skips a level
	    Case{"level 1 7 x 4 7 6 18 6 24 4\n", 1, 11},  // not a number
	    Case{"level 1 7 6 4 0 6 18 6 24 4\n", 1, 15},  // zero fright
	    Case{"level 1 7 6 -4 7 6 18 6 24 4\n", 1, 13}, // negative speed
	    Case{"level 1 99 6 4 7 6 18 6 24 4\n", 1, 9},  // impossibly fast
	    Case{"# nothing but comments\n", 0, 0},        // no levels
	};
	for (const Case& c : cases)
	{
		INFO(c.text);
		const DifficultyResult result = game::ParseDifficulty(c.text);
		CHECK_FALSE(result.table.has_value());
		CHECK(result.error.line == c.line);
		CHECK(result.error.column == c.column);
		CHECK_FALSE(result.error.message.empty());
	}
}

TEST_CASE("A levels file that can't be read is an error about the whole file", "[game][difficulty]")
{
	const DifficultyResult result = game::LoadDifficulty("no/such/levels.txt");
	CHECK_FALSE(result.table.has_value());
	CHECK(result.error.line == 0);
}

TEST_CASE("The game's levels get harder: faster enemies, shorter frights, fewer breathers", "[game][difficulty]")
{
	const DifficultyResult result = game::LoadDifficulty(kAssets / "levels.txt");
	const std::string error = engine::core::FormatError("levels.txt", result.error);
	INFO(error);
	REQUIRE(result.table.has_value());
	const game::DifficultyTable& table = *result.table;
	REQUIRE(table.LevelCount() >= 5);

	const int last = static_cast<int>(table.LevelCount());
	for (int level = 1; level < last; ++level)
	{
		CAPTURE(level);
		const game::LevelTuning& now = table.ForLevel(level);
		const game::LevelTuning& next = table.ForLevel(level + 1);
		CHECK(next.enemySpeed >= now.enemySpeed);
		CHECK(next.enemySpeed - next.playerSpeed >= now.enemySpeed - now.playerSpeed); // the gap closes
		CHECK(next.frightSeconds <= now.frightSeconds);
		CHECK(PatrolSeconds(next.schedule) <= PatrolSeconds(now.schedule));
		CHECK(next.frightSeconds > 0.0f);
		CHECK(TimedSeconds(next.schedule) > 0.0f);
	}

	// Noticeably: by the last level the enemies have caught up with the player, and a power
	// pellet buys less than half the time it did.
	const game::LevelTuning& first = table.ForLevel(1);
	const game::LevelTuning& hardest = table.ForLevel(last);
	CHECK(first.enemySpeed < first.playerSpeed);
	CHECK(hardest.enemySpeed >= hardest.playerSpeed);
	CHECK(hardest.frightSeconds < first.frightSeconds / 2.0f);
}

TEST_CASE("A new level sets the player to that level's speed", "[game][difficulty]")
{
	game::TileMapResult maze = game::ParseTileMap("#######\n"
	                                              "#P....#\n"
	                                              "#######\n"
	                                              "#E#####\n"
	                                              "#######\n");
	REQUIRE(maze.map.has_value());
	const DifficultyResult levels = game::ParseDifficulty(kTwoLevels);
	REQUIRE(levels.table.has_value());
	game::GameAssets assets;
	assets.difficulty = *levels.table;

	game::MazeGame game{*maze.map, assets};
	const auto playerSpeed = [&game]()
	{
		float speed = 0.0f;
		game.world.registry.Each<game::PlayerControlled, game::GridMover>(
		    [&speed](engine::ecs::Entity, const game::PlayerControlled&, const game::GridMover& mover)
		{
			speed = mover.speed;
		});
		return speed;
	};
	engine::input::Keyboard keyboard;
	game.Update(1.0 / 60.0, keyboard); // enters the title
	gametest::RequestPlaying(game);
	game.Update(1.0 / 60.0, keyboard);
	CHECK(playerSpeed() == 7.5f);

	// Straight to the end of level 1.
	game.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(game::Screen::LevelTransition);
	game.Update(1.0 / 60.0, keyboard); // enters the level screen
	for (int i = 0; i < 10 * 60 && gametest::StageOf(game) != gametest::Stage::Ready; ++i)
	{
		game.Update(1.0 / 60.0, keyboard);
	}
	REQUIRE(game.world.resources.Get<game::GameFlow>().level == 2);
	CHECK(playerSpeed() == 8.0f);
}
