#pragma once
#include <cstdint>

#include "graphics/color.hpp"
#include "graphics/image.hpp"

namespace engine::graphics
{
	// Component: a picture cut from the Renderer2D's atlas, drawn at its entity's Transform (the
	// picture's top-left) at its own size. Positions are rounded to whole logical pixels, so
	// pixel art stays sharp.
	struct Sprite
	{
		PixelRect source{};     // the part of the atlas to draw
		Color tint{};           // multiplies the picture's colours; white leaves them as drawn
		std::uint8_t layer = 0; // as Shape::layer: shapes, sprites and text share one order
		bool flipX = false;     // mirrored left to right
		// The size to draw at, in world units (so the camera's zoom applies). 0 draws the picture at its own
		// size; set one or both to stretch it, for the pieces of a panel or a bar that fills.
		int width = 0;
		int height = 0;
	};
} // namespace engine::graphics
