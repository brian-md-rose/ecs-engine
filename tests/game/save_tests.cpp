#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/savedata.hpp"
#include "gameflow.hpp"
#include "hud.hpp"
#include "input/keyboard.hpp"
#include "mazegame.hpp"
#include "nameentry.hpp"
#include "options.hpp"
#include "save.hpp"
#include "scoring.hpp"
#include "testassets.hpp"
#include "tilemap.hpp"

namespace
{
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

	[[nodiscard]] engine::core::SaveData Saved(const std::filesystem::path& file)
	{
		engine::core::SaveDataResult result = engine::core::LoadSaveData(file);
		REQUIRE(result.data.has_value());
		return std::move(*result.data);
	}

	void WriteText(const TempSave& save, const char* text)
	{
		std::filesystem::create_directories(save.folder);
		std::ofstream stream(save.file);
		stream << text;
	}
} // namespace

TEST_CASE("LoadSavedGame gives the defaults without a save, and reads the saved values", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_load");
	const game::SavedGame none = game::LoadSavedGame(save.file);
	CHECK(none.highScore == 0);
	CHECK(none.highScoreName.empty());
	CHECK(none.settings.sound);
	CHECK(none.settings.lives == game::kStartingLives);
	CHECK(game::LoadSavedGame({}).highScore == 0);

	engine::core::SaveData data;
	data.SetInt(game::kHighScoreKey, 4200);
	data.Set(game::kHighScoreNameKey, "ADA");
	data.SetInt(game::kSoundKey, 0);
	data.SetInt(game::kLivesKey, 5);
	REQUIRE(engine::core::WriteSaveData(save.file, data));

	const game::SavedGame saved = game::LoadSavedGame(save.file);
	CHECK(saved.highScore == 4200);
	CHECK(saved.highScoreName == "ADA");
	CHECK_FALSE(saved.settings.sound);
	CHECK(saved.settings.lives == 5);
}

TEST_CASE("A broken save counts as an empty one", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_broken");
	WriteText(save, "highScore\n");
	const game::SavedGame saved = game::LoadSavedGame(save.file);
	CHECK(saved.highScore == 0);
	CHECK(saved.settings.lives == game::kStartingLives);
}

TEST_CASE("A saved name is cleaned up, and settings out of range are ignored", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_clean");
	WriteText(save, "highScoreName ada!? lovelace\nlives 99\nsound 1\n");
	const game::SavedGame saved = game::LoadSavedGame(save.file);
	CHECK(saved.highScoreName == "ADA   LO"); // upper case, odd characters blanked, cut at 8
	CHECK(saved.settings.lives == game::kStartingLives);
	CHECK(saved.settings.sound);
}

TEST_CASE("A loaded game puts its high score, name and settings in the world", "[game][save]")
{
	game::MazeGame maze{TestMaze(), gametest::SceneAssets()};
	game::ApplySavedGame(maze.world,
	                     {.highScore = 700, .highScoreName = "ADA", .settings = {.sound = false, .lives = 4}});

	const auto& high = maze.world.resources.Get<game::HighScore>();
	CHECK(high.points == 700);
	CHECK(high.record == 700);
	CHECK(high.name == "ADA");
	CHECK_FALSE(maze.world.resources.Get<game::Settings>().sound);
	CHECK(maze.world.resources.Get<game::Settings>().lives == 4);
	CHECK(maze.world.resources.Get<game::SaveFile>().savedHighScore == 700);
	CHECK(game::NameOf(maze.world.resources.Get<game::NameEntry>()) == "ADA"); // the next entry starts from it
}

TEST_CASE("RecordScore sets the record only if the score beats it", "[game][save]")
{
	game::MazeGame maze{TestMaze(), gametest::SceneAssets()};
	maze.world.resources.Get<game::HighScore>().record = 100;
	maze.world.resources.Get<game::Score>().points = 100;
	game::RecordScore(maze.world, "TIE");
	CHECK(maze.world.resources.Get<game::HighScore>().name.empty());

	maze.world.resources.Get<game::Score>().points = 101;
	game::RecordScore(maze.world, "WIN");
	CHECK(maze.world.resources.Get<game::HighScore>().record == 101);
	CHECK(maze.world.resources.Get<game::HighScore>().name == "WIN");
}

TEST_CASE("The high score and its name are saved only if the record beats the saved one", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_record");
	game::MazeGame maze{TestMaze(), gametest::SceneAssets()};
	maze.world.resources.Get<game::SaveFile>() = game::SaveFile{.path = save.file, .savedHighScore = 100};

	maze.world.resources.Get<game::HighScore>().record = 50;
	game::SaveHighScore(maze.world);
	CHECK_FALSE(std::filesystem::exists(save.file)); // 50 doesn't beat the saved 100

	maze.world.resources.Get<game::HighScore>().record = 300;
	maze.world.resources.Get<game::HighScore>().name = "ADA";
	game::SaveHighScore(maze.world);
	const engine::core::SaveData data = Saved(save.file);
	CHECK(data.GetInt(game::kHighScoreKey) == 300);
	CHECK(data.Get(game::kHighScoreNameKey) == "ADA");
	CHECK(maze.world.resources.Get<game::SaveFile>().savedHighScore == 300);
}

TEST_CASE("Saving keeps the other values in the save file", "[game][save]")
{
	const TempSave save("ecs_engine_game_save_keep");
	engine::core::SaveData data;
	data.Set("colour", "7");
	REQUIRE(engine::core::WriteSaveData(save.file, data));

	game::MazeGame maze{TestMaze(), gametest::SceneAssets()};
	maze.world.resources.Get<game::SaveFile>().path = save.file;
	maze.world.resources.Get<game::HighScore>().record = 900;
	game::SaveHighScore(maze.world);
	maze.world.resources.Get<game::Settings>() = {.sound = false, .lives = 2};
	game::SaveSettings(maze.world);

	const engine::core::SaveData result = Saved(save.file);
	CHECK(result.GetInt(game::kHighScoreKey) == 900);
	CHECK(result.GetInt(game::kSoundKey) == 0);
	CHECK(result.GetInt(game::kLivesKey) == 2);
	CHECK(result.Get("colour") == "7");
}

TEST_CASE("A game without a save path never writes", "[game][save]")
{
	game::MazeGame maze{TestMaze(), gametest::SceneAssets()};
	maze.world.resources.Get<game::HighScore>().record = 500;
	game::SaveHighScore(maze.world);
	game::SaveSettings(maze.world);
	CHECK(maze.world.resources.Get<game::SaveFile>().savedHighScore == 0);
}
