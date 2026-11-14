#include "gridmovement.hpp"

#include <cstddef>

#include "core/assert.hpp"
#include "maze.hpp"

namespace game
{
	namespace
	{
		[[nodiscard]] bool IsPerpendicular(Direction a, Direction b) noexcept
		{
			return (IsHorizontal(a) && IsVertical(b)) || (IsVertical(a) && IsHorizontal(b));
		}
	} // namespace

	bool IsWalkable(Tile tile) noexcept
	{
		return tile != Tile::Wall && tile != Tile::Door;
	}

	std::optional<TilePosition> Neighbour(const TileMap& map, TilePosition tile, Direction direction)
	{
		if (!map.Contains(tile)) return std::nullopt;
		const bool wraps = map.At(tile) == Tile::Tunnel;
		const std::size_t lastColumn = map.Width() - 1;
		const std::size_t lastRow = map.Height() - 1;

		// Unsigned, so each edge is handled before stepping across it.
		switch (direction)
		{
		case Direction::Up:
			if (tile.row == 0) return wraps ? std::optional{TilePosition{tile.column, lastRow}} : std::nullopt;
			return TilePosition{tile.column, tile.row - 1};
		case Direction::Down:
			if (tile.row == lastRow) return wraps ? std::optional{TilePosition{tile.column, 0}} : std::nullopt;
			return TilePosition{tile.column, tile.row + 1};
		case Direction::Left:
			if (tile.column == 0) return wraps ? std::optional{TilePosition{lastColumn, tile.row}} : std::nullopt;
			return TilePosition{tile.column - 1, tile.row};
		case Direction::Right:
			if (tile.column == lastColumn) return wraps ? std::optional{TilePosition{0, tile.row}} : std::nullopt;
			return TilePosition{tile.column + 1, tile.row};
		case Direction::None:
			break;
		}
		return std::nullopt;
	}

	bool CanEnter(const TileMap& map, TilePosition tile, Direction direction)
	{
		const std::optional<TilePosition> next = Neighbour(map, tile, direction);
		return next && IsWalkable(map.At(*next));
	}

	GridMover StepGridMover(GridMover mover, const TileMap& map, float seconds)
	{
		float remaining = mover.speed * seconds; // tiles of path left to cover in this step

		// Each pass either ends the step or moves the mover to the next point where it may
		// have to decide something: a tile's centre or a tile's edge.
		while (true)
		{
			// Turning back is always allowed: the way it came is open.
			if (mover.direction != Direction::None && mover.buffered == Opposite(mover.direction))
			{
				mover.direction = mover.buffered;
			}

			// How far past the centre it is, in its direction of travel: negative before the
			// centre, positive after it, 0 on it (and whenever it stands still).
			const float along = mover.offset * Sign(mover.direction);

			if (along == 0.0f)
			{
				mover.offset = 0.0f;
				if (mover.buffered != Direction::None && CanEnter(map, mover.tile, mover.buffered))
				{
					mover.direction = mover.buffered;
				}
				if (mover.direction != Direction::None && !CanEnter(map, mover.tile, mover.direction))
				{
					mover.direction = Direction::None; // a wall ahead: stop on the centre
				}
				if (mover.direction == Direction::None) break;
			}
			else if (along > 0.0f && along <= kTurnTolerance && IsPerpendicular(mover.direction, mover.buffered) &&
			         CanEnter(map, mover.tile, mover.buffered))
			{
				// Asked to turn just after passing the centre: turn as if it had happened on the
				// centre, carrying the distance already covered into the new direction.
				mover.direction = mover.buffered;
				mover.offset = along * Sign(mover.direction);
				continue;
			}

			if (remaining <= 0.0f) break;

			// Next stop: this tile's centre if it is still ahead, otherwise the edge into the next tile.
			const float target = along < 0.0f ? 0.0f : 0.5f;
			const float distance = target - along;
			if (remaining < distance)
			{
				mover.offset += Sign(mover.direction) * remaining;
				break;
			}
			remaining -= distance;

			if (target == 0.0f)
			{
				mover.offset = 0.0f;
			}
			else
			{
				// Past the centre only after checking the next tile, or by turning back the
				// way it came, so the next tile is always there.
				const std::optional<TilePosition> next = Neighbour(map, mover.tile, mover.direction);
				ENGINE_VERIFY(next.has_value(), "grid mover walked off the map; it must start on a tile's centre");
				mover.tile = *next;
				mover.offset = -0.5f * Sign(mover.direction);
			}
		}
		return mover;
	}

	engine::graphics::Position MoverTopLeft(engine::graphics::Position origin, const GridMover& mover)
	{
		engine::graphics::Position position = TileTopLeft(origin, mover.tile);
		const float shift = mover.offset * kTileSize;
		if (IsHorizontal(mover.direction))
		{
			position.x += shift;
		}
		else
		{
			position.y += shift;
		}
		return position;
	}
} // namespace game
