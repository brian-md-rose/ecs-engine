#pragma once
#include <optional>

#include "components.hpp"
#include "direction.hpp"
#include "graphics/transform.hpp"
#include "tilemap.hpp"

// Grid movement as pure functions over a TileMap and a GridMover: no registry, no SDL.
// GridMovementSystem calls them once per update for every mover.
namespace game
{
	// How far past a tile's centre, in tiles, a mover can still take a turn it was asked for:
	// a turn pressed a moment too late is still taken, so corners feel smooth. A turn asked for
	// early is remembered (GridMover::buffered) and taken on reaching the centre.
	inline constexpr float kTurnTolerance = 0.25f;

	// Floor the player can walk on: everything but walls and the enemy house's door.
	[[nodiscard]] bool IsWalkable(Tile tile) noexcept;

	// The tile one step from tile in direction. Stepping off the map from a tunnel tile wraps
	// to the opposite edge (where its pair is); stepping off anywhere else, or with
	// Direction::None, gives nothing.
	[[nodiscard]] std::optional<TilePosition> Neighbour(const TileMap& map, TilePosition tile, Direction direction);

	// The neighbour in direction exists and is walkable, or is the door and throughDoor is set.
	[[nodiscard]] bool CanEnter(const TileMap& map, TilePosition tile, Direction direction, bool throughDoor = false);

	// Moves mover for seconds at its speed and returns where it ends up. Along the way:
	// - Asked for the opposite way, it turns back at once, wherever it is.
	// - Asked to turn, it turns on reaching a tile's centre if the tile that way is walkable,
	//   or straight away if it is no more than kTurnTolerance past the centre (the distance
	//   past the centre carries on in the new direction, so the turn costs no time).
	// - Standing still, it starts moving the way it was asked if that tile is walkable.
	// - Reaching the centre of a tile with a wall ahead (or the door, unless mover.throughDoor),
	//   it stops there.
	// - Leaving the map through a tunnel, it comes in through the tunnel's pair.
	// It covers speed * seconds tiles of path unless it stops at a wall, so a second of
	// movement ends in the same place whether it is taken in one step or many.
	[[nodiscard]] GridMover StepGridMover(GridMover mover, const TileMap& map, float seconds);

	// Where to draw a tile-sized sprite for mover: the top-left corner of its tile, shifted by
	// its offset, for a maze whose top-left tile is drawn at origin (see TileTopLeft).
	[[nodiscard]] engine::graphics::Position MoverTopLeft(engine::graphics::Position origin, const GridMover& mover);
} // namespace game
