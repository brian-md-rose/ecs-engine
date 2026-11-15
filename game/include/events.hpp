#pragma once

#include "ecs/entity.hpp"
#include "gameflow.hpp"
#include "tilemap.hpp"

// What the game announces on world.events (see docs/decisions/0002-events.md). Systems publish
// them; scoring, the HUD, the state machine and later audio subscribe in MazeGame's setup.
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

	// The state machine went from one stage to another (see gameflow.hpp).
	struct StageChanged
	{
		Stage from = Stage::Ready;
		Stage to = Stage::Ready;
	};
} // namespace game
