#pragma once

// Points for what the player eats. Our own values.
namespace game
{
	inline constexpr int kPelletPoints = 5;
	inline constexpr int kPowerPelletPoints = 30;

	// Resource: the score of the game in progress.
	struct Score
	{
		int points = 0;
	};
} // namespace game
