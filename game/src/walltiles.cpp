#include "walltiles.hpp"

#include "core/assert.hpp"
#include "direction.hpp"
#include "gridmovement.hpp"

namespace game
{
	namespace
	{
		constexpr std::array<Direction, 4> kSides = {
		    {Direction::Up, Direction::Right, Direction::Down, Direction::Left}};
	} // namespace

	unsigned WallMask(const TileMap& map, TilePosition tile)
	{
		unsigned mask = 0;
		for (std::size_t i = 0; i < kSides.size(); ++i)
		{
			const std::optional<TilePosition> next = Neighbour(map, tile, kSides[i]);
			if (next && map.At(*next) == Tile::Wall) mask |= 1u << i;
		}
		return mask;
	}

	std::array<engine::graphics::Image, kWallTileCount> BuildWallTiles(const WallPieces& pieces)
	{
		ENGINE_VERIFY(pieces.core != nullptr, "BuildWallTiles needs a core");
		std::array<engine::graphics::Image, kWallTileCount> tiles;
		for (std::size_t mask = 0; mask < kWallTileCount; ++mask)
		{
			engine::graphics::Image& tile = tiles[mask];
			tile = *pieces.core;
			for (std::size_t side = 0; side < pieces.edges.size(); ++side)
			{
				const bool wallThatWay = (mask & (std::size_t{1} << side)) != 0;
				if (!wallThatWay && pieces.edges[side] != nullptr) tile.Blit(*pieces.edges[side], 0, 0);
			}
		}
		return tiles;
	}
} // namespace game
