#pragma once
#include <array>
#include <cstddef>

#include "graphics/image.hpp"
#include "tilemap.hpp"

// Maze walls drawn so neighbouring walls join up: each wall tile shows an edge only on the sides
// that face something other than a wall. The sixteen possible tiles are built when the game
// starts from a core and four edges in the sprite sheet. Pure: no SDL.
namespace game
{
	// One bit per side whose neighbour is also a wall.
	enum WallSide : unsigned
	{
		kWallUp = 1u,
		kWallRight = 2u,
		kWallDown = 4u,
		kWallLeft = 8u
	};

	inline constexpr std::size_t kWallTileCount = 16;

	// Which neighbours of tile are walls, as WallSide bits. Off the map counts as not a wall, so
	// the maze's outer border gets an edge on its outside too.
	[[nodiscard]] unsigned WallMask(const TileMap& map, TilePosition tile);

	// The pieces a wall tile is built from: a core filling the tile, and an edge for each side,
	// in the order up, right, down, left.
	struct WallPieces
	{
		const engine::graphics::Image* core = nullptr;
		std::array<const engine::graphics::Image*, 4> edges{};
	};

	// Tile mask is the core with the edge of every side mask does not have.
	[[nodiscard]] std::array<engine::graphics::Image, kWallTileCount> BuildWallTiles(const WallPieces& pieces);
} // namespace game
