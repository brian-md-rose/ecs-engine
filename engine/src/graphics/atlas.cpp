#include "graphics/atlas.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>

#include "core/assert.hpp"

namespace engine::graphics
{
	namespace
	{
		constexpr int kPadding = 1;
	} // namespace

	Atlas PackAtlas(std::span<const Image* const> images)
	{
		Atlas atlas;
		atlas.regions.resize(images.size());

		int width = kAtlasWidth;
		for (const Image* image : images)
		{
			ENGINE_VERIFY(image != nullptr, "PackAtlas was given a null image");
			width = std::max(width, image->Width() + 2 * kPadding);
		}

		// Tallest first, so each shelf wastes little height; ties keep the input order.
		std::vector<std::size_t> order(images.size());
		std::iota(order.begin(), order.end(), std::size_t{0});
		std::ranges::stable_sort(order, [&images](std::size_t a, std::size_t b)
		{
			return images[a]->Height() > images[b]->Height();
		});

		// The solid pixel opens the first shelf.
		atlas.solid = {.x = kPadding, .y = kPadding, .width = 1, .height = 1};
		int x = kPadding + 1 + kPadding;
		int y = kPadding;
		int shelfHeight = 1;
		for (const std::size_t index : order)
		{
			const Image& image = *images[index];
			if (x + image.Width() + kPadding > width)
			{
				y += shelfHeight + kPadding;
				x = kPadding;
				shelfHeight = 0;
			}
			atlas.regions[index] = {.x = x, .y = y, .width = image.Width(), .height = image.Height()};
			x += image.Width() + kPadding;
			shelfHeight = std::max(shelfHeight, image.Height());
		}

		atlas.image = Image(width, y + shelfHeight + kPadding);
		atlas.image.Set(atlas.solid.x, atlas.solid.y, Rgba{255, 255, 255, 255});
		for (std::size_t i = 0; i < images.size(); ++i)
		{
			atlas.image.Blit(*images[i], atlas.regions[i].x, atlas.regions[i].y);
		}
		return atlas;
	}
} // namespace engine::graphics
