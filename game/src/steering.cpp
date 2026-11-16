#include "steering.hpp"

#include <optional>

#include "enemytuning.hpp"
#include "gridmovement.hpp"

namespace game
{
	using engine::grid::Direction;

	int DistanceSquared(TilePosition from, Target to) noexcept
	{
		const int columns = static_cast<int>(from.column) - to.column;
		const int rows = static_cast<int>(from.row) - to.row;
		return columns * columns + rows * rows;
	}

	Exits OpenExits(const TileMap& map, TilePosition tile, Direction heading, bool throughDoor)
	{
		Exits exits;
		const Direction back = Opposite(heading);
		bool canGoBack = false;
		for (const Direction direction : kTieBreakOrder)
		{
			if (!CanEnter(map, tile, direction, throughDoor)) continue;
			if (direction == back)
			{
				canGoBack = true;
				continue;
			}
			exits.directions[exits.count] = direction;
			++exits.count;
		}
		if (exits.count == 0 && canGoBack)
		{
			exits.directions[0] = back;
			exits.count = 1;
		}
		return exits;
	}

	Direction SteerTowards(const TileMap& map, TilePosition tile, Direction heading, Target target, bool throughDoor)
	{
		const Exits exits = OpenExits(map, tile, heading, throughDoor);
		Direction best = Direction::None;
		int bestDistance = 0;
		for (std::size_t i = 0; i < exits.count; ++i)
		{
			const Direction direction = exits.directions[i];
			const std::optional<TilePosition> next = Neighbour(map, tile, direction);
			if (!next) continue; // can't happen: OpenExits only lists enterable neighbours
			const int distance = DistanceSquared(*next, target);
			// Strictly closer only, so the earlier exit in kTieBreakOrder keeps a tie.
			if (best == Direction::None || distance < bestDistance)
			{
				best = direction;
				bestDistance = distance;
			}
		}
		return best;
	}

	Direction SteerRandomly(const TileMap& map, TilePosition tile, Direction heading, bool throughDoor,
	                        std::uint32_t roll)
	{
		const Exits exits = OpenExits(map, tile, heading, throughDoor);
		if (exits.count == 0) return Direction::None;
		return exits.directions[roll % exits.count];
	}
} // namespace game
