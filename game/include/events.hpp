#pragma once

#include "ecs/entity.hpp"
#include "tilemap.hpp"

// What the game announces on world.events (see docs/decisions/0002-events.md). Systems publish
// them; scoring, the HUD, the flow and audio subscribe in MazeGame's setup. Changes of screen
// are not events: they are engine states (see gameflow.hpp).
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

	// A dangerous enemy (not frightened, not on its way home) reached the player's tile.
	struct PlayerCaught
	{
	};

	// The player ate a frightened enemy.
	struct EnemyEaten
	{
		engine::ecs::Entity enemy;
	};

	// The last pellet or power pellet of the level was eaten.
	struct LevelCleared
	{
	};
} // namespace game
