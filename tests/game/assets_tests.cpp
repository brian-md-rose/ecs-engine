#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <queue>
#include <string>
#include <vector>

#include "maze.hpp"
#include "mazegame.hpp"
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
		const std::filesystem::path file = kAssets / "mazes" / name;
		const game::TileMapResult result = game::LoadTileMap(file);
		INFO(game::FormatError(name, result.error));
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
	const TileMap map = LoadMaze("level1.txt");
	CHECK(static_cast<float>(map.Width()) * game::kTileSize <= static_cast<float>(game::kLogicalWidth));
	CHECK(static_cast<float>(game::kHudRows + map.Height()) * game::kTileSize <=
	      static_cast<float>(game::kLogicalHeight));
}

TEST_CASE("Level 1 has four power pellets, a tunnel and a house with a door", "[game][assets]")
{
	const TileMap map = LoadMaze("level1.txt");
	CHECK(map.Find(Tile::PowerPellet).size() == 4);
	CHECK_FALSE(map.Find(Tile::Tunnel).empty());
	CHECK_FALSE(map.Find(Tile::Door).empty());
	CHECK(map.Find(Tile::EnemySpawn).size() == 4);
}

TEST_CASE("Level 1 has no dead ends", "[game][assets]")
{
	const TileMap map = LoadMaze("level1.txt");
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
	const TileMap map = LoadMaze("level1.txt");
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
