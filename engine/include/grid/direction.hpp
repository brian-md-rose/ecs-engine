#pragma once
#include <cstdint>

namespace engine::grid
{
	// One of the four ways along the grid, or none (standing still, nothing asked for).
	enum class Direction : std::uint8_t
	{
		None,
		Up,
		Down,
		Left,
		Right
	};

	// The way back: Up and Down swap, Left and Right swap; None stays None.
	[[nodiscard]] constexpr Direction Opposite(Direction direction) noexcept
	{
		switch (direction)
		{
		case Direction::Up:
			return Direction::Down;
		case Direction::Down:
			return Direction::Up;
		case Direction::Left:
			return Direction::Right;
		case Direction::Right:
			return Direction::Left;
		case Direction::None:
			break;
		}
		return Direction::None;
	}

	[[nodiscard]] constexpr bool IsHorizontal(Direction direction) noexcept
	{
		return direction == Direction::Left || direction == Direction::Right;
	}

	[[nodiscard]] constexpr bool IsVertical(Direction direction) noexcept
	{
		return direction == Direction::Up || direction == Direction::Down;
	}

	// +1 for Right and Down (the way screen coordinates grow), -1 for Left and Up, 0 for None.
	[[nodiscard]] constexpr float Sign(Direction direction) noexcept
	{
		switch (direction)
		{
		case Direction::Right:
		case Direction::Down:
			return 1.0f;
		case Direction::Left:
		case Direction::Up:
			return -1.0f;
		case Direction::None:
			break;
		}
		return 0.0f;
	}
} // namespace engine::grid
