#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "assets.hpp"
#include "graphics/image.hpp"
#include "graphics/sheetmanifest.hpp"
#include "maze.hpp"
#include "mazegame.hpp"
#include "mazescene.hpp"
#include "tilemap.hpp"

namespace
{
	using game::Tile;
	using game::TileMap;
	using game::TilePosition;

	// Set by tests/CMakeLists.txt to the source folder, so the tests read the mazes as written.
	const std::filesystem::path kAssets{GAME_ASSETS_DIR};

	[[nodiscard]] TileMap LoadMaze(const std::string& name)
	{
		const std::filesystem::path file = kAssets / "scenes" / name;
		const game::TileMapResult result = game::LoadMazeScene(file);
		INFO(engine::core::FormatError(name, result.error));
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	[[nodiscard]] bool IsPassable(const TileMap& map, TilePosition tile, bool doorIsOpen)
	{
		const Tile kind = map.At(tile);
		return kind != Tile::Wall && (doorIsOpen || kind != Tile::Door);
	}

	// The tiles an actor can step to from tile: up, down, left and right, plus the far end
	// of a tunnel.
	[[nodiscard]] std::vector<TilePosition> Neighbours(const TileMap& map, TilePosition tile, bool doorIsOpen)
	{
		std::vector<TilePosition> candidates;
		if (tile.row > 0) candidates.push_back({.column = tile.column, .row = tile.row - 1});
		if (tile.row + 1 < map.Height()) candidates.push_back({.column = tile.column, .row = tile.row + 1});
		if (tile.column > 0) candidates.push_back({.column = tile.column - 1, .row = tile.row});
		if (tile.column + 1 < map.Width()) candidates.push_back({.column = tile.column + 1, .row = tile.row});
		if (map.At(tile) == Tile::Tunnel)
		{
			const bool onLeftOrRight = tile.column == 0 || tile.column == map.Width() - 1;
			candidates.push_back(onLeftOrRight
			                         ? TilePosition{.column = map.Width() - 1 - tile.column, .row = tile.row}
			                         : TilePosition{.column = tile.column, .row = map.Height() - 1 - tile.row});
		}

		std::vector<TilePosition> result;
		for (const TilePosition& candidate : candidates)
		{
			if (IsPassable(map, candidate, doorIsOpen)) result.push_back(candidate);
		}
		return result;
	}

	// Every tile reachable from start, as a row-by-row grid of flags.
	[[nodiscard]] std::vector<bool> Reachable(const TileMap& map, TilePosition start, bool doorIsOpen)
	{
		std::vector<bool> seen(map.Width() * map.Height(), false);
		std::queue<TilePosition> frontier;
		seen[start.row * map.Width() + start.column] = true;
		frontier.push(start);
		while (!frontier.empty())
		{
			const TilePosition tile = frontier.front();
			frontier.pop();
			for (const TilePosition& next : Neighbours(map, tile, doorIsOpen))
			{
				const std::size_t index = next.row * map.Width() + next.column;
				if (seen[index]) continue;
				seen[index] = true;
				frontier.push(next);
			}
		}
		return seen;
	}
} // namespace

TEST_CASE("Level 1 loads and fits on screen below the HUD", "[game][assets]")
{
	const TileMap map = LoadMaze("level1.json");
	CHECK(static_cast<float>(map.Width()) * game::kTileSize <= static_cast<float>(game::kLogicalWidth));
	CHECK(static_cast<float>(game::kHudRows + map.Height()) * game::kTileSize <=
	      static_cast<float>(game::kLogicalHeight));
}

TEST_CASE("Level 1 has four power pellets, a tunnel and a house with a door", "[game][assets]")
{
	const TileMap map = LoadMaze("level1.json");
	CHECK(map.Find(Tile::PowerPellet).size() == 4);
	CHECK_FALSE(map.Find(Tile::Tunnel).empty());
	CHECK_FALSE(map.Find(Tile::Door).empty());
	CHECK(map.Find(Tile::EnemySpawn).size() == 4);
}

TEST_CASE("Level 1 has no dead ends", "[game][assets]")
{
	const TileMap map = LoadMaze("level1.json");
	for (std::size_t row = 0; row < map.Height(); ++row)
	{
		for (std::size_t column = 0; column < map.Width(); ++column)
		{
			const TilePosition tile{.column = column, .row = row};
			if (!IsPassable(map, tile, true)) continue;
			INFO("line " << row + 1 << ", column " << column + 1);
			CHECK(Neighbours(map, tile, true).size() >= 2);
		}
	}
}

TEST_CASE("Every open tile of level 1 is reachable, and the house only through its door", "[game][assets]")
{
	const TileMap map = LoadMaze("level1.json");
	const TilePosition player = map.Find(Tile::PlayerSpawn).front();

	const std::vector<bool> withDoor = Reachable(map, player, true);
	for (std::size_t row = 0; row < map.Height(); ++row)
	{
		for (std::size_t column = 0; column < map.Width(); ++column)
		{
			const TilePosition tile{.column = column, .row = row};
			if (!IsPassable(map, tile, true)) continue;
			INFO("line " << row + 1 << ", column " << column + 1);
			CHECK(withDoor[row * map.Width() + column]);
		}
	}

	const std::vector<bool> doorClosed = Reachable(map, player, false);
	for (const TilePosition& enemy : map.Find(Tile::EnemySpawn))
	{
		CHECK_FALSE(doorClosed[enemy.row * map.Width() + enemy.column]);
	}
}

TEST_CASE("The player can reach every pellet of level 1, so the level can be cleared", "[game][assets]")
{
	const TileMap map = LoadMaze("level1.json");
	const std::vector<bool> reachable = Reachable(map, map.Find(Tile::PlayerSpawn).front(), false);
	for (const Tile kind : {Tile::Pellet, Tile::PowerPellet})
	{
		for (const TilePosition& tile : map.Find(kind))
		{
			INFO("line " << tile.row + 1 << ", column " << tile.column + 1);
			CHECK(reachable[tile.row * map.Width() + tile.column]);
		}
	}
}

TEST_CASE("The game's font and sprites load, with a glyph for every character the HUD uses", "[game][assets]")
{
	const std::optional<game::GameAssets> assets = game::LoadGameAssets(kAssets);
	REQUIRE(assets.has_value());
	CHECK_FALSE(assets->atlas.image.IsEmpty());

	const std::string used = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!-:./<>@";
	for (const char character : used)
	{
		INFO(character);
		const engine::graphics::PixelRect* glyph = assets->font.Glyph(character);
		REQUIRE(glyph != nullptr);
		CHECK(glyph->width <= game::kGlyphAdvance);
		CHECK(glyph->height <= game::kGlyphLineHeight);
	}
}

TEST_CASE("The PNG sheets draw exactly what the text sheets do", "[game][assets]")
{
	for (const std::string& name : {std::string("font"), std::string("sprites")})
	{
		INFO(name);
		const std::filesystem::path folder = kAssets / "sprites";
		const engine::graphics::PixelSheetResult text = engine::graphics::LoadSheetFile(folder / (name + ".txt"));
		const engine::graphics::PixelSheetResult png = engine::graphics::LoadSheetFile(folder / (name + ".sheet"));
		INFO(engine::core::FormatError(name, png.error));
		REQUIRE(text.sheet.has_value());
		REQUIRE(png.sheet.has_value());
		REQUIRE(text.sheet->sprites.size() == png.sheet->sprites.size());

		for (std::size_t i = 0; i < text.sheet->sprites.size(); ++i)
		{
			const engine::graphics::SheetSprite& a = text.sheet->sprites[i];
			const engine::graphics::SheetSprite& b = png.sheet->sprites[i];
			INFO(a.name);
			REQUIRE(a.name == b.name);
			REQUIRE(a.frameWidth == b.frameWidth);
			REQUIRE(a.frameHeight == b.frameHeight);
			REQUIRE(a.frames == b.frames);
			REQUIRE(a.image.Width() == b.image.Width());
			REQUIRE(a.image.Height() == b.image.Height());
			CHECK(std::equal(a.image.Pixels().begin(), a.image.Pixels().end(), b.image.Pixels().begin()));
		}
	}
}

TEST_CASE("The game still loads from the text sheets when there are no PNG sheets", "[game][assets]")
{
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / "ecs_engine_text_only_assets";
	std::filesystem::remove_all(folder);
	std::filesystem::copy(kAssets, folder, std::filesystem::copy_options::recursive);
	std::filesystem::remove(folder / "sprites" / "font.sheet");
	std::filesystem::remove(folder / "sprites" / "sprites.sheet");

	const std::optional<game::GameAssets> assets = game::LoadGameAssets(folder);
	std::filesystem::remove_all(folder);
	REQUIRE(assets.has_value());
	CHECK_FALSE(assets->atlas.image.IsEmpty());
	CHECK(assets->font.Glyph('A') != nullptr);
}
