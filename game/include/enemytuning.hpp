#pragma once
#include <array>

#include "direction.hpp"

// Every number that decides how the enemies move and think, in one place, so the game can be
// tuned here without hunting through the code. All of them are our own (see docs/enemies.md).
namespace game
{
	// Steering (steering.hpp)

	// When two exits are equally close to an enemy's target, the one earlier in this list wins:
	// clockwise, starting from Right.
	inline constexpr std::array<Direction, 4> kTieBreakOrder = {
	    {Direction::Right, Direction::Down, Direction::Left, Direction::Up}};
} // namespace game
