#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/savedata.hpp"
#include "gameflow.hpp"
#include "hud.hpp"
#include "input/keyboard.hpp"
#include "mazegame.hpp"
#include "save.hpp"
#include "scoring.hpp"
#include "systems.hpp"
#include "tilemap.hpp"

namespace
{
	using game::Stage;

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

	// A save file in a fresh temp folder, removed again at the end.
	struct TempSave
	{
		std::filesystem::path folder;
		std::filesystem::path file;

		explicit TempSave(const char* name)
		    : folder(std::filesystem::temp_directory_path() / name), file(folder / "save.txt")
		{
			std::filesystem::remove_all(folder);
		}
		~TempSave() { std::filesystem::remove_all(folder); }

		TempSave(const TempSave&) = delete;
		TempSave& operator=(const TempSave&) = delete;
		TempSave(TempSave&&) = delete;
		TempSave& operator=(TempSave&&) = delete;
	};

	[[nodiscard]] int SavedHighScore(const std::filesystem::path& file)
	{
		const engine::core::SaveDataResult result = engine::core::LoadSaveData(file);
		REQUIRE(result.data.has_value());
		return result.data->GetInt(game::kHighScoreKey).value_or(-1);
	}

	// Moves the game to stage, as the game does, and runs an update so the change is handled.
	void EnterStage(game::MazeGame& maze, Stage stage)
	{
		game::GameFlow next = maze.world.resources.Get<game::GameFlow>();
		next.stage = stage;
		game::SetFlow(maze.world, next);
		maze.Update(1.0 / 60.0, engine::input::Keyboard{});
	}
} // namespace

TEST_CASE("LoadHighScore is 0 without a save, and reads the saved one", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_load");
	CHECK(game::LoadHighScore({}) == 0);
	CHECK(game::LoadHighScore(save.file) == 0);

	engine::core::SaveData data;
	data.SetInt(game::kHighScoreKey, 4200);
	REQUIRE(engine::core::WriteSaveData(save.file, data));
	CHECK(game::LoadHighScore(save.file) == 4200);
}

TEST_CASE("A broken save counts as no high score", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_broken");
	std::filesystem::create_directories(save.folder);
	{
		std::ofstream stream(save.file);
		stream << "highScore\n";
	}
	CHECK(game::LoadHighScore(save.file) == 0);
}

TEST_CASE("The high score is saved when a game ends or is abandoned, if it is a new best", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_stage");
	game::MazeGame maze{TestMaze()};
	maze.world.resources.Get<game::SaveFile>() = game::SaveFile{.path = save.file, .savedHighScore = 100};

	EnterStage(maze, Stage::Ready);
	maze.world.resources.Get<game::Score>().points = 50;
	EnterStage(maze, Stage::GameOver);
	CHECK_FALSE(std::filesystem::exists(save.file)); // 50 doesn't beat the saved 100

	EnterStage(maze, Stage::Ready);
	maze.world.resources.Get<game::Score>().points = 300;
	EnterStage(maze, Stage::Playing);
	CHECK_FALSE(std::filesystem::exists(save.file)); // only at the end of a game

	EnterStage(maze, Stage::Title); // abandoned from the pause menu
	CHECK(SavedHighScore(save.file) == 300);
	CHECK(maze.world.resources.Get<game::SaveFile>().savedHighScore == 300);
}

TEST_CASE("Saving keeps the other values in the save file", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_keep");
	engine::core::SaveData data;
	data.Set("volume", "7");
	REQUIRE(engine::core::WriteSaveData(save.file, data));

	game::MazeGame maze{TestMaze()};
	maze.world.resources.Get<game::SaveFile>().path = save.file;
	maze.world.resources.Get<game::HighScore>().points = 900;
	game::SaveHighScore(maze.world);

	const engine::core::SaveDataResult result = engine::core::LoadSaveData(save.file);
	REQUIRE(result.data.has_value());
	CHECK(result.data->GetInt(game::kHighScoreKey) == 900);
	CHECK(result.data->Get("volume") == "7");
}

TEST_CASE("A game without a save path never writes", "[game][save]")
{
	game::MazeGame maze{TestMaze()};
	maze.world.resources.Get<game::Score>().points = 500;
	game::SaveHighScore(maze.world);
	CHECK(maze.world.resources.Get<game::SaveFile>().savedHighScore == 0);
}
