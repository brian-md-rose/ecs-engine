#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "ecs/enginecomponents.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/assetbrowser.hpp"
#include "editor/assetcatalog.hpp"
#include "graphics/image.hpp"
#include "graphics/imagefile.hpp"

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

TEST_CASE("A sheet mixing glyphs and longer names is sprites, not a font", "[editor][assets]")
{
	const auto folder = FreshFolder("mixed");
	Write(folder / "sprites" / "mixed.sheet", "image m.png\nregion a 0 0 5 7\nregion heart 6 0 9 9\n");
	const AssetCatalog catalog(folder);
	CHECK(catalog.Names(AssetKind::Font).empty());
	CHECK(catalog.Names(AssetKind::Sprite) == std::vector<std::string>{"a", "heart"});
}

TEST_CASE("Sprite::source is marked as a sprite asset, so the Inspector can pick it by name", "[editor][assets]")
{
	engine::ecs::TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	const engine::ecs::TypeInfo* sprite = types.Find("Sprite");
	REQUIRE(sprite != nullptr);
	bool found = false;
	for (const engine::ecs::FieldInfo& field : sprite->Fields())
	{
		if (field.name != "source") continue;
		found = true;
		CHECK(field.hints.assetKind == AssetKind::Sprite);
	}
	CHECK(found);
}

TEST_CASE("Entries know their manifest line and numbers", "[editor][assets]")
{
	const auto folder = FreshFolder("settings");
	Write(folder / "sprites" / "s.sheet", "# top\n"
	                                      "\n"
	                                      "sprite player sprites/player.png 8 8 2\n"
	                                      "image icons.png\n"
	                                      "region heart 3 4 9 10\n");
	const AssetCatalog catalog(folder);
	const auto& entries = catalog.Entries(AssetKind::Sprite);
	REQUIRE(entries.size() == 2);
	CHECK(entries[0].name == "heart");
	CHECK(entries[0].region);
	CHECK(entries[0].line == 5);
	CHECK(entries[0].settings ==
	      engine::editor::SpriteSettings{.x = 3, .y = 4, .frameWidth = 9, .frameHeight = 10, .frames = 1});
	CHECK(entries[1].line == 3);
	CHECK_FALSE(entries[1].region);
	CHECK(entries[1].settings == engine::editor::SpriteSettings{.frameWidth = 8, .frameHeight = 8, .frames = 2});
}

TEST_CASE("ApplySpriteSettings rewrites one line and keeps the rest", "[editor][assets]")
{
	using engine::editor::ApplySpriteSettings;
	using engine::editor::SpriteSettings;
	const std::string text = "# c\nsprite a sprites/a.png 8 8\r\n  region b 1 2 3 4 2\nimage x.png\n";

	const auto sprite =
	    ApplySpriteSettings(text, 2, "a", SpriteSettings{.frameWidth = 4, .frameHeight = 5, .frames = 3});
	REQUIRE(sprite);
	CHECK(*sprite == "# c\nsprite a sprites/a.png 4 5 3\r\n  region b 1 2 3 4 2\nimage x.png\n");

	const auto region = ApplySpriteSettings(
	    text, 3, "b", SpriteSettings{.x = 9, .y = 8, .frameWidth = 7, .frameHeight = 6, .frames = 1});
	REQUIRE(region);
	CHECK(*region == "# c\nsprite a sprites/a.png 8 8\r\n  region b 9 8 7 6 1\nimage x.png\n");

	CHECK_FALSE(ApplySpriteSettings(text, 1, "a", SpriteSettings{}));     // a comment
	CHECK_FALSE(ApplySpriteSettings(text, 4, "x", SpriteSettings{}));     // an image line
	CHECK_FALSE(ApplySpriteSettings(text, 2, "b", SpriteSettings{}));     // another sprite's line
	CHECK_FALSE(ApplySpriteSettings(text, 9, "NAME9", SpriteSettings{})); // past the end
}

TEST_CASE("SaveSpriteSettings writes a manifest that loads and refuses one that does not", "[editor][assets]")
{
	using engine::editor::SaveSpriteSettings;
	using engine::editor::SpriteSettings;
	const auto folder = FreshFolder("save");
	REQUIRE(engine::graphics::SaveImage(engine::graphics::Image(16, 8), folder / "sprites" / "strip.png"));
	const auto sheet = folder / "sprites" / "s.sheet";
	Write(sheet, "sprite strip strip.png 8 8 2\n");

	// A different cut of the same 16 x 8 picture is fine.
	CHECK(
	    SaveSpriteSettings(sheet, 1, "strip", SpriteSettings{.frameWidth = 4, .frameHeight = 8, .frames = 4}).empty());
	{
		const AssetCatalog rescanned(folder);
		const auto& found = rescanned.Entries(AssetKind::Sprite);
		REQUIRE(found.size() == 1);
		const int frames = found.front().settings.frames;
		CHECK(frames == 4);
	}

	// One that does not match the picture is refused and the file stays as it was.
	const std::string reason =
	    SaveSpriteSettings(sheet, 1, "strip", SpriteSettings{.frameWidth = 8, .frameHeight = 8, .frames = 5});
	CHECK_FALSE(reason.empty());
	{
		const AssetCatalog rescanned(folder);
		const auto& found = rescanned.Entries(AssetKind::Sprite);
		REQUIRE(found.size() == 1);
		const int frames = found.front().settings.frames;
		CHECK(frames == 4);
	}
	CHECK_FALSE(SaveSpriteSettings(sheet, 1, "other", SpriteSettings{.frameWidth = 8, .frameHeight = 8})
	                .empty()); // a different sprite is on that line now
	CHECK_FALSE(SaveSpriteSettings(sheet, 7, "strip", SpriteSettings{}).empty());
}
