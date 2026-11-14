#pragma once

#include "tilemap.hpp"

// What the game announces on world.events (see docs/decisions/0002-events.md). Systems publish
// them; scoring, the HUD and later the state machine and audio subscribe in MazeGame's setup.
namespace game
{
	// The player ate the pellet on tile.
	struct PelletEaten
	{
		TilePosition tile;
	};

	// The player ate the power pellet on tile.
	struct PowerPelletEaten
	{
		TilePosition tile;
	};

	// An enemy reached the player's tile.
	struct PlayerCaught
	{
	};

	// The last pellet or power pellet of the level was eaten.
	struct LevelCleared
	{
	};
} // namespace game
