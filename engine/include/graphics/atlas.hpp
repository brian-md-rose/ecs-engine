#pragma once
#include <span>
#include <vector>

#include "graphics/image.hpp"

namespace engine::graphics
{
	// Many small images packed into one, so everything can be drawn from a single texture in a
	// single call. Besides the packed images it holds one opaque white pixel (solid), which
	// untextured shapes sample so they can share the texture.
	struct Atlas
	{
		Image image;
		std::vector<PixelRect> regions; // where each input image went, in input order
		PixelRect solid;                // a 1 x 1 opaque white pixel
	};

	// Atlases are this wide, or as wide as the widest image if that is wider.
	inline constexpr int kAtlasWidth = 256;

	// Packs images into rows ("shelves"), tallest first, with a pixel of transparent space
	// around each so that neighbours never bleed into each other when drawn. Pure: no SDL.
	[[nodiscard]] Atlas PackAtlas(std::span<const Image* const> images);
} // namespace engine::graphics
