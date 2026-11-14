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

	// Tag: spawned by the rendering stress test, removed when it ends.
	struct StressShape
	{
	};
} // namespace game
