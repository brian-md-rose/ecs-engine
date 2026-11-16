#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "grid/direction.hpp"
#include "tilemap.hpp"

// How an enemy picks its way through the maze, as pure functions over a TileMap: no registry,
// no SDL. Each time an enemy comes to a tile it chooses which way to leave it, and never turns
// back the way it came unless that is the only way out (a dead end). Turning back otherwise
// happens only when its mode changes (see enemymodes.hpp).
namespace game
{
	// A tile an enemy steers towards. Signed, because a target may lie outside the maze (a
	// tile some way ahead of a player who is near the edge): enemies still head that way.
	struct Target
	{
		int column = 0;
		int row = 0;

		friend bool operator==(const Target&, const Target&) = default;
	};

	[[nodiscard]] constexpr Target ToTarget(TilePosition tile) noexcept
	{
		return {.column = static_cast<int>(tile.column), .row = static_cast<int>(tile.row)};
	}

	// The square of the straight-line distance, in tiles, from a tile to a target. Squared so
	// it stays an exact integer; it orders distances just the same.
	[[nodiscard]] int DistanceSquared(TilePosition from, Target to) noexcept;

	// The ways an enemy may leave a tile, in kTieBreakOrder.
	struct Exits
	{
		std::array<engine::grid::Direction, 4> directions{};
		std::size_t count = 0;
	};

	// Every direction from tile into a walkable neighbour (the door counts when throughDoor)
	// except the way back, Opposite(heading). At a dead end the way back is the only exit. An
	// enemy standing still (heading None) may leave any open way.
	[[nodiscard]] Exits OpenExits(const TileMap& map, TilePosition tile, engine::grid::Direction heading,
	                              bool throughDoor);

	// Of the OpenExits, the one whose next tile is closest to target in a straight line; on a
	// tie, the one earlier in kTieBreakOrder. None if the tile has no open neighbour at all.
	[[nodiscard]] engine::grid::Direction SteerTowards(const TileMap& map, TilePosition tile,
	                                                   engine::grid::Direction heading, Target target,
	                                                   bool throughDoor);

	// Of the OpenExits, the one roll picks (roll modulo their number), for a frightened enemy's
	// random turns. None if the tile has no open neighbour at all.
	[[nodiscard]] engine::grid::Direction SteerRandomly(const TileMap& map, TilePosition tile,
	                                                    engine::grid::Direction heading, bool throughDoor,
	                                                    std::uint32_t roll);
} // namespace game
