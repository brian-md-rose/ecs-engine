#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "startup.hpp"

namespace
{
	// A manifest with the given text in a fresh temporary folder.
	struct TempManifest
	{
		std::filesystem::path folder;
		std::filesystem::path file;

		explicit TempManifest(const std::string& name, const std::string& text)
		    : folder(std::filesystem::temp_directory_path() / name), file(folder / "build.json")
		{
			std::filesystem::remove_all(folder);
			std::filesystem::create_directories(folder);
			std::ofstream(file) << text;
		}
		~TempManifest()
		{
			std::error_code error;
			std::filesystem::remove_all(folder, error);
		}
		TempManifest(const TempManifest&) = delete;
		TempManifest& operator=(const TempManifest&) = delete;
	};
} // namespace

TEST_CASE("ReadStartupScene reads the scene the manifest names")
{
	const TempManifest manifest("ecs_startup_named", R"({"game": "g", "startupScene": "level2", "scenes": []})");
	CHECK(game::ReadStartupScene(manifest.file) == "level2");
}

TEST_CASE("ReadStartupScene gives nothing for a missing, empty or unreadable manifest")
{
	CHECK(game::ReadStartupScene(std::filesystem::temp_directory_path() / "ecs_startup_nothing" / "build.json").empty());
	CHECK(game::ReadStartupScene(TempManifest("ecs_startup_empty", R"({"startupScene": ""})").file).empty());
	CHECK(game::ReadStartupScene(TempManifest("ecs_startup_absent", R"({"game": "g"})").file).empty());
	CHECK(game::ReadStartupScene(TempManifest("ecs_startup_broken", "{ not json").file).empty());
	CHECK(game::ReadStartupScene(TempManifest("ecs_startup_number", R"({"startupScene": 3})").file).empty());
}

TEST_CASE("ReadStartupScene refuses a name that is not a plain scene name")
{
	CHECK(game::ReadStartupScene(TempManifest("ecs_startup_up", R"({"startupScene": "../secret"})").file).empty());
	CHECK(game::ReadStartupScene(TempManifest("ecs_startup_dir", R"({"startupScene": "sub/level"})").file).empty());
	CHECK(game::ReadStartupScene(TempManifest("ecs_startup_back", R"({"startupScene": "sub\\level"})").file).empty());
}
