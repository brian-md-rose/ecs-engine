#pragma once
#include <cstdint>

#include "color.hpp"

namespace game
{
	struct Shape
	{
		float width = 0;
		float height = 0;
		Color color;
		std::uint8_t layer = 0;
	};
} // namespace game
