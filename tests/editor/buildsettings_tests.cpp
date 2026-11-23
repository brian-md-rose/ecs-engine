#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/json.hpp"
#include "editor/buildsettings.hpp"

namespace fs = std::filesystem;
using namespace engine::editor;

namespace
{
	void Write(const fs::path& file, const std::string& text)
	{
		fs::create_directories(file.parent_path());
		std::ofstream out(file, std::ios::binary);
		out << text;
	}

	struct Sandbox
	{
		fs::path root = fs::temp_directory_path() / "ecs-engine-buildsettings-test";

		Sandbox()
		{
			fs::remove_all(root);
			Write(root / "assets" / "scenes" / "title.json", "{}");
			Write(root / "assets" / "scenes" / "level1.json", "{}");
			Write(root / "assets" / "scenes" / "extra.json", "{}");
			Write(root / "assets" / "sprites" / "a.sheet", "x");
			Write(root / "bin" / "game.exe", "exe");
		}
		~Sandbox()
		{
			std::error_code error;
			fs::remove_all(root, error);
		}

		[[nodiscard]] BuildSettingsConfig Config() const
		{
			BuildSettingsConfig config;
			config.sceneFolder = root / "assets" / "scenes";
			config.assetFolder = root / "assets";
			config.gameName = "Test";
			config.engineVersion = "1.2.3";
			return config;
		}

		[[nodiscard]] BuildSettings Settings() const
		{
			BuildSettings settings;
			settings.scenes = {"level1", "title"};
			settings.startupScene = "title";
			settings.gameExe = (root / "bin" / "game.exe").string();
			settings.outputFolder = (root / "out").string();
			return settings;
		}
	};
} // namespace

TEST_CASE("Scenes are listed by name, sorted", "[buildsettings]")
{
	const Sandbox box;
	CHECK(ListSceneNames(box.root / "assets" / "scenes") == std::vector<std::string>{"extra", "level1", "title"});
	CHECK(ListSceneNames(box.root / "nowhere").empty());
}

TEST_CASE("Build settings round-trip through a file", "[buildsettings]")
{
	const Sandbox box;
	const fs::path file = box.root / "settings.json";
	CHECK(LoadBuildSettings(file) == BuildSettings{}); // missing: the defaults
	const BuildSettings settings = box.Settings();
	REQUIRE(SaveBuildSettings(file, settings));
	CHECK(LoadBuildSettings(file) == settings);
}

TEST_CASE("Build problems name what is wrong", "[buildsettings]")
{
	const Sandbox box;
	const std::vector<std::string> available = {"extra", "level1", "title"};
	CHECK(BuildProblems(box.Settings(), available).empty());

	BuildSettings none = box.Settings();
	none.scenes.clear();
	CHECK_FALSE(BuildProblems(none, available).empty());

	BuildSettings badStartup = box.Settings();
	badStartup.startupScene = "extra"; // not ticked
	CHECK_FALSE(BuildProblems(badStartup, available).empty());

	BuildSettings gone = box.Settings();
	gone.scenes.emplace_back("deleted");
	CHECK_FALSE(BuildProblems(gone, available).empty());

	BuildSettings noExe = box.Settings();
	noExe.gameExe = (box.root / "bin" / "missing.exe").string();
	CHECK_FALSE(BuildProblems(noExe, available).empty());

	BuildSettings noOutput = box.Settings();
	noOutput.outputFolder.clear();
	CHECK_FALSE(BuildProblems(noOutput, available).empty());
}

TEST_CASE("The manifest lists the game, the startup scene, the scenes and the files", "[buildsettings]")
{
	const Sandbox box;
	const auto manifest = BuildManifest(box.Config(), box.Settings(), {"a", "b"});
	REQUIRE(manifest.Find("game") != nullptr);
	CHECK(*manifest.Find("game")->AsString() == "Test");
	CHECK(*manifest.Find("engineVersion")->AsString() == "1.2.3");
	CHECK(*manifest.Find("startupScene")->AsString() == "title");
	CHECK(manifest.Find("scenes")->AsArray()->size() == 2);
	CHECK(manifest.Find("files")->AsArray()->size() == 2);
}

TEST_CASE("Packaging copies the exe and assets, leaves out unpicked scenes and writes the manifest", "[buildsettings]")
{
	const Sandbox box;
	const BuildSettings settings = box.Settings();
	const PackageResult result = PackageGame(box.Config(), settings);
	REQUIRE(result.ok);
	const fs::path out = box.root / "out";
	CHECK(fs::exists(out / "game.exe"));
	CHECK(fs::exists(out / "assets" / "scenes" / "title.json"));
	CHECK(fs::exists(out / "assets" / "scenes" / "level1.json"));
	CHECK_FALSE(fs::exists(out / "assets" / "scenes" / "extra.json"));
	CHECK(fs::exists(out / "assets" / "sprites" / "a.sheet"));

	const auto manifest = engine::core::LoadJson(out / "build.json");
	REQUIRE(manifest.value.has_value());
	CHECK(*manifest.value->Find("startupScene")->AsString() == "title");
	CHECK(manifest.value->Find("files")->AsArray()->size() == result.files);

	// A second package replaces the assets: a scene dropped from the list is gone.
	BuildSettings fewer = settings;
	fewer.scenes = {"title"};
	REQUIRE(PackageGame(box.Config(), fewer).ok);
	CHECK_FALSE(fs::exists(out / "assets" / "scenes" / "level1.json"));
}

TEST_CASE("Packaging refuses an output that would overwrite the source assets or the exe", "[buildsettings]")
{
	const Sandbox box;
	BuildSettings inAssets = box.Settings();
	inAssets.outputFolder = box.root.string(); // out/assets would be the source assets
	const PackageResult first = PackageGame(box.Config(), inAssets);
	CHECK_FALSE(first.ok);
	CHECK(fs::exists(box.root / "assets" / "scenes" / "title.json"));

	BuildSettings onExe = box.Settings();
	onExe.outputFolder = (box.root / "bin").string();
	CHECK_FALSE(PackageGame(box.Config(), onExe).ok);
	CHECK(fs::exists(box.root / "bin" / "game.exe"));
}

TEST_CASE("Packaging leaves alone an assets folder it did not make, and keeps the last package if the source is gone",
          "[buildsettings]")
{
	const Sandbox box;
	Write(box.root / "out" / "assets" / "mine.txt", "keep");
	CHECK_FALSE(PackageGame(box.Config(), box.Settings()).ok);
	CHECK(fs::exists(box.root / "out" / "assets" / "mine.txt"));

	fs::remove_all(box.root / "out");
	REQUIRE(PackageGame(box.Config(), box.Settings()).ok);
	BuildSettingsConfig missing = box.Config();
	missing.assetFolder = box.root / "nope";
	missing.sceneFolder = box.root / "nope" / "scenes";
	CHECK_FALSE(PackageGame(missing, box.Settings()).ok);
	CHECK(fs::exists(box.root / "out" / "assets" / "scenes" / "title.json"));
}

TEST_CASE("Unpicked scenes are left out wherever the scene folder is below the assets", "[buildsettings]")
{
	const Sandbox box;
	Write(box.root / "assets" / "levels" / "one.json", "{}");
	Write(box.root / "assets" / "levels" / "two.json", "{}");
	BuildSettingsConfig config = box.Config();
	config.sceneFolder = box.root / "assets" / "levels";
	BuildSettings settings = box.Settings();
	settings.scenes = {"one"};
	settings.startupScene = "one";
	REQUIRE(PackageGame(config, settings).ok);
	CHECK(fs::exists(box.root / "out" / "assets" / "levels" / "one.json"));
	CHECK_FALSE(fs::exists(box.root / "out" / "assets" / "levels" / "two.json"));

	config.sceneFolder = box.root / "elsewhere";
	CHECK_FALSE(PackageGame(config, settings).ok);
}
