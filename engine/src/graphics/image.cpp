#include "graphics/image.hpp"

#include <algorithm>
#include <cstddef>

#include "core/assert.hpp"

namespace engine::graphics
{
	Image::Image(int imageWidth, int imageHeight)
	    : width(std::max(imageWidth, 0)), height(std::max(imageHeight, 0)),
	      pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
	{
	}

	std::size_t Image::Index(int x, int y) const
	{
		ENGINE_VERIFY(Contains(x, y), "pixel outside the image");
		return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
	}

	Rgba Image::At(int x, int y) const
	{
		return pixels[Index(x, y)];
	}

	void Image::Set(int x, int y, Rgba pixel)
	{
		pixels[Index(x, y)] = pixel;
	}

	void Image::Blit(const Image& source, int x, int y)
	{
		for (int sy = 0; sy < source.height; ++sy)
		{
			for (int sx = 0; sx < source.width; ++sx)
			{
				const Rgba pixel = source.At(sx, sy);
				if (pixel.a == 0 || !Contains(x + sx, y + sy)) continue;
				Set(x + sx, y + sy, pixel);
			}
		}
	}
} // namespace engine::graphics
