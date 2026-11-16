#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace engine::graphics
{
	// One pixel with straight (not premultiplied) alpha: 0 is fully transparent, 255 opaque.
	struct Rgba
	{
		std::uint8_t r = 0;
		std::uint8_t g = 0;
		std::uint8_t b = 0;
		std::uint8_t a = 0;

		friend bool operator==(const Rgba&, const Rgba&) = default;
	};

	static_assert(sizeof(Rgba) == 4, "Rgba is uploaded to textures as packed R, G, B, A bytes");

	// A rectangle of pixels in an image or texture, origin top-left.
	struct PixelRect
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;

		friend bool operator==(const PixelRect&, const PixelRect&) = default;
	};

	// Pixels in memory, row by row from the top-left. Pure: no SDL. The Renderer turns one into
	// a texture (Renderer::CreateTexture); until then it can be built and checked in tests.
	class Image
	{
		int width = 0;
		int height = 0;
		std::vector<Rgba> pixels;

		[[nodiscard]] std::size_t Index(int x, int y) const;

	public:
		Image() = default;
		// A width x height image, every pixel transparent. Negative sizes count as 0.
		Image(int width, int height);

		[[nodiscard]] int Width() const noexcept { return width; }
		[[nodiscard]] int Height() const noexcept { return height; }
		[[nodiscard]] bool IsEmpty() const noexcept { return pixels.empty(); }
		[[nodiscard]] bool Contains(int x, int y) const noexcept { return x >= 0 && y >= 0 && x < width && y < height; }

		// x, y must be inside the image (Contains); out of range aborts.
		[[nodiscard]] Rgba At(int x, int y) const;
		void Set(int x, int y, Rgba pixel);

		// Copies source onto this image with its top-left at (x, y). Fully transparent source
		// pixels are skipped, so the image underneath shows through; every other pixel replaces
		// what was there. Parts that fall outside this image are clipped.
		void Blit(const Image& source, int x, int y);

		// Row by row from the top-left, width * height pixels.
		[[nodiscard]] std::span<const Rgba> Pixels() const noexcept { return pixels; }
	};
} // namespace engine::graphics
