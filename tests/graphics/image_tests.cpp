#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "graphics/atlas.hpp"
#include "graphics/image.hpp"
#include "graphics/pixelsheet.hpp"

namespace
{
	using engine::graphics::Atlas;
	using engine::graphics::Image;
	using engine::graphics::PixelRect;
	using engine::graphics::PixelSheetResult;
	using engine::graphics::Rgba;

	constexpr Rgba kRed{255, 0, 0, 255};
	constexpr Rgba kBlue{0, 0, 255, 255};
	constexpr Rgba kClear{};

	[[nodiscard]] Image Filled(int width, int height, Rgba colour)
	{
		Image image(width, height);
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				image.Set(x, y, colour);
			}
		}
		return image;
	}

	[[nodiscard]] bool Overlap(const PixelRect& a, const PixelRect& b)
	{
		return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
	}
} // namespace

TEST_CASE("A new image is transparent", "[graphics][image]")
{
	const Image image(3, 2);
	CHECK(image.Width() == 3);
	CHECK(image.Height() == 2);
	CHECK(image.Pixels().size() == 6);
	CHECK(image.At(2, 1) == kClear);
}

TEST_CASE("Blit copies opaque pixels, skips transparent ones and clips at the edges", "[graphics][image]")
{
	Image target = Filled(4, 4, kBlue);
	Image source(2, 2);
	source.Set(0, 0, kRed);
	source.Set(1, 1, kRed);

	target.Blit(source, 3, 3); // only source (0, 0) lands inside
	CHECK(target.At(3, 3) == kRed);

	target.Blit(source, 0, 0);
	CHECK(target.At(0, 0) == kRed);
	CHECK(target.At(1, 0) == kBlue); // transparent in the source
	CHECK(target.At(1, 1) == kRed);
}

TEST_CASE("A pixel sheet turns rows of characters into frames side by side", "[graphics][pixelsheet]")
{
	const PixelSheetResult result = engine::graphics::ParsePixelSheet("# two frames of a blinking dot\n"
	                                                                  "color R 255 0 0\n"
	                                                                  "color b 0 0 255 128\n"
	                                                                  "\n"
	                                                                  "sprite dot 2 2 2\n"
	                                                                  "R.b.\n"
	                                                                  ".R..\n");
	INFO(result.error.message);
	REQUIRE(result.sheet.has_value());
	REQUIRE(result.sheet->sprites.size() == 1);

	const auto* dot = result.sheet->Find("dot");
	REQUIRE(dot != nullptr);
	CHECK(dot->frameWidth == 2);
	CHECK(dot->frameHeight == 2);
	CHECK(dot->frames == 2);
	CHECK(dot->image.Width() == 4);
	CHECK(dot->image.At(0, 0) == kRed);
	CHECK(dot->image.At(1, 1) == kRed);
	CHECK(dot->image.At(2, 0) == Rgba{0, 0, 255, 128});
	CHECK(dot->image.At(1, 0) == kClear);
	CHECK(result.sheet->Find("nothing") == nullptr);
}

TEST_CASE("A sprite has one frame unless it says otherwise", "[graphics][pixelsheet]")
{
	const PixelSheetResult result = engine::graphics::ParsePixelSheet("color X 1 2 3\nsprite a 1 1\nX\n");
	REQUIRE(result.sheet.has_value());
	CHECK(result.sheet->sprites[0].frames == 1);
}

TEST_CASE("Pixel sheet errors give the line and column", "[graphics][pixelsheet]")
{
	struct Case
	{
		std::string text;
		std::size_t line;
		std::size_t column;
	};
	const std::array cases = {
	    Case{"colour X 1 2 3\n", 1, 1},                   // unknown directive
	    Case{"color XY 1 2 3\n", 1, 7},                   // colour key too long
	    Case{"color . 1 2 3\n", 1, 7},                    // '.' is reserved
	    Case{"color X 1 2 300\n", 1, 13},                 // channel out of range
	    Case{"color X 1 2\n", 1, 1},                      // too few channels
	    Case{"color X 1 2 3\ncolor X 1 2 3\n", 2, 7},     // defined twice
	    Case{"sprite a 0 1\n", 1, 10},                    // zero width
	    Case{"sprite a 2 2\n..\n", 1, 0},                 // file ends early
	    Case{"sprite a 2 1\n...\n", 2, 0},                // row too long
	    Case{"sprite a 2 1\n.Q\n", 2, 2},                 // undefined colour
	    Case{"sprite a 1 1\n.\nsprite a 1 1\n.\n", 3, 8}, // duplicate name
	    Case{"sprite a 1 1 2\n.\n", 2, 0},                // row too short for two frames
	};
	for (const Case& c : cases)
	{
		INFO(c.text);
		const PixelSheetResult result = engine::graphics::ParsePixelSheet(c.text);
		CHECK_FALSE(result.sheet.has_value());
		CHECK(result.error.line == c.line);
		CHECK(result.error.column == c.column);
		CHECK_FALSE(result.error.message.empty());
	}
}

TEST_CASE("A pixel sheet that can't be read is an error about the whole file", "[graphics][pixelsheet]")
{
	const PixelSheetResult result = engine::graphics::LoadPixelSheet("no/such/sheet.txt");
	CHECK_FALSE(result.sheet.has_value());
	CHECK(result.error.line == 0);
}

TEST_CASE("PackAtlas places every image without overlap and copies its pixels", "[graphics][atlas]")
{
	const Image a = Filled(10, 4, kRed);
	const Image b = Filled(3, 9, kBlue);
	const Image c = Filled(250, 2, kRed); // too wide to share a shelf with the others
	const std::array<const Image*, 3> images = {&a, &b, &c};

	const Atlas atlas = engine::graphics::PackAtlas(images);
	REQUIRE(atlas.regions.size() == 3);
	CHECK(atlas.image.Width() == engine::graphics::kAtlasWidth);
	CHECK(atlas.image.At(atlas.solid.x, atlas.solid.y) == Rgba{255, 255, 255, 255});

	for (std::size_t i = 0; i < images.size(); ++i)
	{
		const PixelRect& region = atlas.regions[i];
		CHECK(region.width == images[i]->Width());
		CHECK(region.height == images[i]->Height());
		CHECK(region.x >= 0);
		CHECK(region.y >= 0);
		CHECK(region.x + region.width <= atlas.image.Width());
		CHECK(region.y + region.height <= atlas.image.Height());
		CHECK(atlas.image.At(region.x, region.y) == images[i]->At(0, 0));
		CHECK_FALSE(Overlap(region, atlas.solid));
		for (std::size_t j = i + 1; j < images.size(); ++j)
		{
			CHECK_FALSE(Overlap(region, atlas.regions[j]));
		}
	}
}

TEST_CASE("PackAtlas grows wider for an image wider than the usual atlas", "[graphics][atlas]")
{
	const Image wide(400, 1);
	const std::array<const Image*, 1> images = {&wide};
	const Atlas atlas = engine::graphics::PackAtlas(images);
	CHECK(atlas.image.Width() >= 400);
	CHECK(atlas.regions[0].x + 400 <= atlas.image.Width());
}
