#pragma once
#include <cstdint>

#include "graphics/color.hpp"

namespace engine::graphics
{
	enum class ShapeKind : std::uint8_t
	{
		Rectangle,
		Circle
	};

	// A filled shape drawn at its entity's Transform, which is the top-left corner of the
	// shape's bounding box for both kinds. Plain data: build one with designated initializers,
	// e.g. Shape{.kind = ShapeKind::Circle, .radius = 4.0f, .color = {255, 0, 0}}.
	struct Shape
	{
		ShapeKind kind = ShapeKind::Rectangle;
		float width = 0;  // Rectangle only
		float height = 0; // Rectangle only
		float radius = 0; // Circle only
		Color color;
		std::uint8_t layer = 0; // higher layers draw on top

		// Size of the bounding box, whichever the kind.
		[[nodiscard]] constexpr float BoundsWidth() const noexcept
		{
			return kind == ShapeKind::Circle ? radius * 2.0f : width;
		}

		[[nodiscard]] constexpr float BoundsHeight() const noexcept
		{
			return kind == ShapeKind::Circle ? radius * 2.0f : height;
		}
	};
} // namespace engine::graphics
