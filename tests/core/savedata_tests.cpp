#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/savedata.hpp"

namespace
{
	using engine::core::SaveData;
	using engine::core::SaveDataResult;

	// A fresh folder under the temp directory for one test, removed again at the end.
	struct TempFolder
	{
		std::filesystem::path path;

		explicit TempFolder(const char* name) : path(std::filesystem::temp_directory_path() / name)
		{
			std::filesystem::remove_all(path);
		}
		~TempFolder() { std::filesystem::remove_all(path); }

		TempFolder(const TempFolder&) = delete;
		TempFolder& operator=(const TempFolder&) = delete;
		TempFolder(TempFolder&&) = delete;
		TempFolder& operator=(TempFolder&&) = delete;
	};
} // namespace

TEST_CASE("SaveData keeps text and whole numbers by key", "[core][savedata]")
{
	SaveData data;
	CHECK(data.Empty());
	CHECK_FALSE(data.Get("highScore").has_value());

	data.SetInt("highScore", 1200);
	data.Set("name", "Ada Lovelace");
	CHECK(data.GetInt("highScore") == 1200);
	CHECK(data.Get("name") == "Ada Lovelace");
	CHECK_FALSE(data.GetInt("name").has_value()); // not a number

	data.SetInt("highScore", 3400);
	CHECK(data.GetInt("highScore") == 3400);
}

TEST_CASE("SaveData is written one sorted 'key value' line per entry", "[core][savedata]")
{
	SaveData data;
	data.SetInt("zebra", 1);
	data.Set("apple", "two words");
	CHECK(data.Format() == "apple two words\nzebra 1\n");
}

TEST_CASE("ParseSaveData reads what Format writes, and skips blanks and comments", "[core][savedata]")
{
	const SaveDataResult result =
	    engine::core::ParseSaveData("# saved by the game\r\n\n  highScore   250  \nname Ada\n");
	REQUIRE(result.data.has_value());
	CHECK(result.data->GetInt("highScore") == 250);
	CHECK(result.data->Get("name") == "Ada");

	const SaveDataResult again = engine::core::ParseSaveData(result.data->Format());
	REQUIRE(again.data.has_value());
	CHECK(again.data->Format() == result.data->Format());
}

TEST_CASE("ParseSaveData rejects a key with no value", "[core][savedata]")
{
	const SaveDataResult result = engine::core::ParseSaveData("highScore 10\n  lonely\n");
	CHECK_FALSE(result.data.has_value());
	CHECK(result.error.line == 2);
	CHECK(result.error.column == 3);
	CHECK(result.error.message == "'lonely' has no value");
}

TEST_CASE("A save file that doesn't exist yet loads as empty data", "[core][savedata]")
{
	const TempFolder folder("ecs_engine_savedata_missing");
	const SaveDataResult result = engine::core::LoadSaveData(folder.path / "save.txt");
	REQUIRE(result.data.has_value());
	CHECK(result.data->Empty());
}

TEST_CASE("WriteSaveData makes the folder, and replaces an older save", "[core][savedata]")
{
	const TempFolder folder("ecs_engine_savedata_write");
	const std::filesystem::path file = folder.path / "nested" / "save.txt";

	SaveData data;
	data.SetInt("highScore", 10);
	REQUIRE(engine::core::WriteSaveData(file, data));
	data.SetInt("highScore", 20);
	REQUIRE(engine::core::WriteSaveData(file, data));

	const SaveDataResult result = engine::core::LoadSaveData(file);
	REQUIRE(result.data.has_value());
	CHECK(result.data->GetInt("highScore") == 20);
	CHECK_FALSE(std::filesystem::exists(folder.path / "nested" / "save.txt.tmp"));
}
