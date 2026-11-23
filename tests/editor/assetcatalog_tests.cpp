#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>

#include "editor/assetbrowser.hpp"
#include "editor/assetcatalog.hpp"

using engine::ecs::AssetKind;
using engine::editor::AssetCatalog;

namespace
{
	std::filesystem::path FreshFolder(const std::string& name)
	{
		const auto folder = std::filesystem::temp_directory_path() / "ecs_engine_assetcatalog" / name;
		std::error_code ignored;
		std::filesystem::remove_all(folder, ignored);
		std::filesystem::create_directories(folder / "sprites");
		std::filesystem::create_directories(folder / "music");
		return folder;
	}

	void Write(const std::filesystem::path& file, const std::string& text)
	{
		std::ofstream(file, std::ios::binary) << text;
	}
} // namespace

TEST_CASE("The catalog lists sprites, fonts and sounds by kind", "[editor][assets]")
{
	const auto folder = FreshFolder("kinds");
	Write(folder / "sprites" / "sprites.sheet", "# a comment\n"
	                                            "sprite player sprites/player.png 8 8 2\n"
	                                            "sprite door sprites/door.png 8 8\n");
	Write(folder / "sprites" / "font.sheet", "image font.png\nregion A 0 0 5 7 1\nregion B 6 0 5 7 1\n");
	Write(folder / "sprites" / "icons.sheet", "image icons.png\nregion heart 0 0 9 9\n");
	Write(folder / "sprites" / "more.sheet", "sprite door sprites/door2.png 8 8\n");
	Write(folder / "music" / "theme.OGG", "x");
	Write(folder / "jump.wav", "x");
	Write(folder / "notes.txt", "x");

	const AssetCatalog catalog(folder);

	CHECK(catalog.Names(AssetKind::Sprite) == std::vector<std::string>{"door", "heart", "player"});
	CHECK(catalog.Names(AssetKind::Font) == std::vector<std::string>{"font"});
	CHECK(catalog.Names(AssetKind::Sound) == std::vector<std::string>{"jump.wav", "music/theme.OGG"});
	CHECK(catalog.Entries(AssetKind::Sprite).size() == 4); // both doors are listed, the pick-list names it once
	REQUIRE(catalog.Entries(AssetKind::Sprite).size() == 4);
	CHECK(catalog.Entries(AssetKind::Sprite)[3].detail == "8 x 8, 2 frames");
	CHECK(catalog.Entries(AssetKind::Sprite)[0].detail == "8 x 8");
	CHECK(catalog.Entries(AssetKind::Font)[0].detail == "2 glyphs");
	CHECK(catalog.Contains(AssetKind::Sprite, "door"));
	CHECK_FALSE(catalog.Contains(AssetKind::Sound, "door"));
	CHECK(catalog.Entries(AssetKind::None).empty());
}

TEST_CASE("The catalog tolerates a missing folder, and Rescan sees new files", "[editor][assets]")
{
	AssetCatalog missing(std::filesystem::temp_directory_path() / "ecs_engine_assetcatalog" / "does_not_exist");
	CHECK(missing.Names(AssetKind::Sprite).empty());
	CHECK(AssetCatalog().Names(AssetKind::Sound).empty());

	const auto folder = FreshFolder("rescan");
	AssetCatalog catalog(folder);
	CHECK(catalog.Names(AssetKind::Sound).empty());
	Write(folder / "music" / "boss.ogg", "x");
	CHECK(catalog.Names(AssetKind::Sound).empty());
	catalog.Rescan();
	CHECK(catalog.Names(AssetKind::Sound) == std::vector<std::string>{"music/boss.ogg"});
}

TEST_CASE("The asset search ignores case", "[editor][assets]")
{
	const engine::editor::AssetEntry entry{.name = "Button_Hover", .file = {}, .detail = {}};
	CHECK(engine::editor::AssetMatches(entry, ""));
	CHECK(engine::editor::AssetMatches(entry, "hover"));
	CHECK_FALSE(engine::editor::AssetMatches(entry, "pressed"));
}

TEST_CASE("The real game assets are found", "[editor][assets]")
{
	const AssetCatalog catalog{std::filesystem::path(GAME_ASSETS_DIR)};
	for (const char* name : {"panel", "button", "button_hover", "button_pressed", "cursor", "player_right"})
		CHECK(catalog.Contains(AssetKind::Sprite, name));
	CHECK(catalog.Contains(AssetKind::Font, "font"));
	CHECK(catalog.Contains(AssetKind::Sound, "music/theme.ogg"));
}
