#pragma once

#include "direction.hpp"
#include "tilemap.hpp"

namespace game
{
	struct Velocity
	{
		float x = 0; // logical pixels per second
		float y = 0;
	};

	// Moves an entity along the maze grid, one lane at a time (see gridmovement.hpp).
	//
	// A mover is always on the centre line of a row or a column. tile is the tile it is in, and
	// offset is how far it is from that tile's centre, in tiles, along the axis it moves on:
	// positive towards Right or Down, negative towards Left or Up. A mover leaves a tile when
	// it is half a tile from its centre, so offset stays within [-0.5, 0.5]. A mover that stands
	// still is on a tile's centre: offset is 0 whenever direction is None.
	struct GridMover
	{
		TilePosition tile;
		float offset = 0.0f;
		Direction direction = Direction::None; // the way it is moving; None when stopped
		Direction buffered = Direction::None;  // the way it has been asked to go next
		float speed = 0.0f;                    // tiles per second
	};

	// Tag: moved by the player's controls.
	struct PlayerControlled
	{
	};

	// Tags for the maze's tiles (see SpawnMaze).
	struct Wall
	{
	};

	struct Pellet
	{
	};

	struct PowerPellet
	{
	};

	// The enemy house's door: enemies pass through it, the player doesn't.
	struct Door
	{
	};

	// Tag: spawned by the rendering stress test, removed when it ends.
	struct StressShape
	{
	};
} // namespace game
