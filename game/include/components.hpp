#pragma once

namespace game
{
	struct Velocity
	{
		float x = 0; // logical pixels per second
		float y = 0;
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
