#include "targeting.hpp"

#include <cstdlib>
#include <vector>

#include "gridmovement.hpp"

namespace game
{
	using engine::grid::Direction;

	namespace
	{
		// How many sides of tile the player could leave by.
		[[nodiscard]] int OpenSides(const TileMap& map, TilePosition tile)
		{
			int sides = 0;
			for (const Direction direction : kTieBreakOrder)
			{
				if (CanEnter(map, tile, direction)) ++sides;
			}
			return sides;
		}

		[[nodiscard]] constexpr int SignOf(int value) noexcept
		{
			return (value > 0) - (value < 0);
		}
	} // namespace

	Target CornerTarget(const TileMap& map, Corner corner) noexcept
	{
		const int right = static_cast<int>(map.Width()) - 1;
		const int bottom = static_cast<int>(map.Height()) - 1;
		switch (corner)
		{
		case Corner::TopLeft:
			return {.column = 0, .row = 0};
		case Corner::TopRight:
			return {.column = right, .row = 0};
		case Corner::BottomLeft:
			return {.column = 0, .row = bottom};
		case Corner::BottomRight:
			break;
		}
		return {.column = right, .row = bottom};
	}

	Target PatrolTarget(const TileMap& map, Personality personality) noexcept
	{
		return CornerTarget(map, TuningFor(personality).corner);
	}

	TilePosition JunctionAhead(const TileMap& map, TilePosition tile, Direction heading, int lookahead)
	{
		for (int step = 0; step < lookahead && CanEnter(map, tile, heading); ++step)
		{
			const std::optional<TilePosition> next = Neighbour(map, tile, heading);
			if (!next) break; // can't happen: CanEnter checked it
			tile = *next;
			if (OpenSides(map, tile) >= 3) break;
		}
		return tile;
	}

	Target GritTarget(const ChaseView& view) noexcept
	{
		return ToTarget(view.player);
	}

	Target SnareTarget(const TileMap& map, const ChaseView& view)
	{
		return ToTarget(JunctionAhead(map, view.player, view.playerHeading, kSnareLookahead));
	}

	Target ClaspTarget(const ChaseView& view) noexcept
	{
		const Target player = ToTarget(view.player);
		const Target ally = ToTarget(view.ally);
		const int across = player.column - ally.column;
		const int down = player.row - ally.row;
		if (std::abs(across) >= std::abs(down))
		{
			return {.column = player.column + SignOf(across) * kClaspReach, .row = player.row};
		}
		return {.column = player.column, .row = player.row + SignOf(down) * kClaspReach};
	}

	Target WispTarget(const ChaseView& view) noexcept
	{
		const Target self = ToTarget(view.self);
		const Target player = ToTarget(view.player);
		const int across = self.column - player.column;
		const int down = self.row - player.row;
		if (std::abs(across) + std::abs(down) > kWispComfortDistance) return player;
		return {.column = self.column + across, .row = self.row + down};
	}

	Target ChaseTarget(const TileMap& map, Personality personality, const ChaseView& view)
	{
		switch (personality)
		{
		case Personality::Snare:
			return SnareTarget(map, view);
		case Personality::Clasp:
			return ClaspTarget(view);
		case Personality::Wisp:
			return WispTarget(view);
		case Personality::Grit:
		case Personality::Count:
			break;
		}
		return GritTarget(view);
	}

	std::optional<House> FindHouse(const TileMap& map)
	{
		const std::vector<TilePosition> doors = map.Find(Tile::Door);
		const std::vector<TilePosition> spawns = map.Find(Tile::EnemySpawn);
		if (doors.empty() || spawns.empty()) return std::nullopt;
		const TilePosition door = doors.front();

		// Compare against the spawns' centre without dividing: scale each side by the number of
		// spawns instead.
		const int count = static_cast<int>(spawns.size());
		Target sum;
		for (const TilePosition spawn : spawns)
		{
			sum.column += static_cast<int>(spawn.column);
			sum.row += static_cast<int>(spawn.row);
		}

		std::optional<Direction> inward;
		int nearest = 0;
		for (const Direction direction : kTieBreakOrder)
		{
			const std::optional<TilePosition> side = Neighbour(map, door, direction);
			if (!side || !IsWalkable(map.At(*side))) continue;
			const int columns = static_cast<int>(side->column) * count - sum.column;
			const int rows = static_cast<int>(side->row) * count - sum.row;
			const int distance = columns * columns + rows * rows;
			if (!inward || distance < nearest)
			{
				inward = direction;
				nearest = distance;
			}
		}
		if (!inward) return std::nullopt;

		const std::optional<TilePosition> home = Neighbour(map, door, *inward);
		const std::optional<TilePosition> exit = Neighbour(map, door, Opposite(*inward));
		if (!home || !exit || !IsWalkable(map.At(*exit))) return std::nullopt;
		return House{.exit = *exit, .home = *home};
	}
} // namespace game
