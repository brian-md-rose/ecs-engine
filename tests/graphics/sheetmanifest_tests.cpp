#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "graphics/image.hpp"
#include "graphics/imagefile.hpp"
#include "graphics/sheetmanifest.hpp"

namespace
{
	using engine::graphics::Image;
	using engine::graphics::PixelSheetResult;
	using engine::graphics::Rgba;

	constexpr Rgba kRed{255, 0, 0, 255};
	constexpr Rgba kBlue{0, 0, 255, 255};
	constexpr Rgba kHalf{10, 20, 30, 128};

	// A fresh folder for one test's files, removed afterwards.
	struct TempFolder
	{
		std::filesystem::path path;

		explicit TempFolder(const std::string& name)
		    : path(std::filesystem::temp_directory_path() / ("ecs_engine_sheetmanifest_" + name))
		{
			std::filesystem::remove_all(path);
			std::filesystem::create_directories(path);
		}
		~TempFolder()
		{
			std::error_code ignored;
			std::filesystem::remove_all(path, ignored);
		}
		TempFolder(const TempFolder&) = delete;
		TempFolder& operator=(const TempFolder&) = delete;
	};

	// width x height, red on the left half and blue on the right, with one half-transparent pixel.
	[[nodiscard]] Image TwoTone(int width, int height)
	{
		Image image(width, height);
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				image.Set(x, y, x < width / 2 ? kRed : kBlue);
			}
		}
		image.Set(0, 0, kHalf);
		return image;
	}

	void Save(const Image& image, const std::filesystem::path& file)
	{
		REQUIRE(engine::graphics::SaveImage(image, file));
	}

	[[nodiscard]] PixelSheetResult Parse(const std::string& manifest, const TempFolder& folder)
	{
		return engine::graphics::ParseSheetManifest(manifest, folder.path);
	}
} // namespace

TEST_CASE("A PNG round-trips through SaveImage and LoadImage", "[graphics][image]")
{
	const TempFolder folder("roundtrip");
	const Image original = TwoTone(6, 3);
	Save(original, folder.path / "a.png");

	const engine::graphics::ImageResult loaded = engine::graphics::LoadImage(folder.path / "a.png");
	REQUIRE(loaded.image.has_value());
	REQUIRE(loaded.image->Width() == 6);
	REQUIRE(loaded.image->Height() == 3);
	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 6; ++x)
		{
			REQUIRE(loaded.image->At(x, y) == original.At(x, y));
		}
	}
}

TEST_CASE("LoadImage reports a missing file", "[graphics][image]")
{
	const TempFolder folder("missing");
	const engine::graphics::ImageResult loaded = engine::graphics::LoadImage(folder.path / "nope.png");
	REQUIRE_FALSE(loaded.image.has_value());
	REQUIRE_FALSE(loaded.error.empty());
}

TEST_CASE("A manifest sprite is a PNG of its own", "[graphics][sheet]")
{
	const TempFolder folder("sprite");
	const Image art = TwoTone(8, 4); // two frames of 4 x 4
	Save(art, folder.path / "walk.png");

	const PixelSheetResult result = Parse("# a comment\n\nsprite walk walk.png 4 4 2\n", folder);
	REQUIRE(result.sheet.has_value());
	REQUIRE(result.sheet->sprites.size() == 1);
	const auto& sprite = result.sheet->sprites[0];
	REQUIRE(sprite.name == "walk");
	REQUIRE(sprite.frameWidth == 4);
	REQUIRE(sprite.frameHeight == 4);
	REQUIRE(sprite.frames == 2);
	REQUIRE(sprite.image.Width() == 8);
	REQUIRE(sprite.image.At(0, 0) == kHalf);
	REQUIRE(sprite.image.At(7, 3) == kBlue);
}

TEST_CASE("Manifest regions are cut from the latest image", "[graphics][sheet]")
{
	const TempFolder folder("region");
	Image sheet(10, 6);
	sheet.Set(1, 1, kRed);
	sheet.Set(6, 2, kBlue);
	Save(sheet, folder.path / "atlas.png");

	const PixelSheetResult result = Parse("image atlas.png\nregion a 0 0 3 3\nregion b 5 1 2 2 2\n", folder);
	REQUIRE(result.sheet.has_value());
	REQUIRE(result.sheet->sprites.size() == 2);

	const auto* a = result.sheet->Find("a");
	REQUIRE(a != nullptr);
	REQUIRE(a->frames == 1);
	REQUIRE(a->image.Width() == 3);
	REQUIRE(a->image.At(1, 1) == kRed);

	const auto* b = result.sheet->Find("b");
	REQUIRE(b != nullptr);
	REQUIRE(b->frames == 2);
	REQUIRE(b->image.Width() == 4);
	REQUIRE(b->image.Height() == 2);
	REQUIRE(b->image.At(1, 1) == kBlue);
}

TEST_CASE("A bad manifest is rejected with the line and column", "[graphics][sheet]")
{
	const TempFolder folder("errors");
	Save(TwoTone(8, 4), folder.path / "walk.png");

	struct Case
	{
		const char* text;
		std::size_t line;
		const char* mentions; // part of the message
	};
	const Case cases[] = {
	    {"jump a b\n", 1, "expected 'sprite'"},
	    {"sprite walk nope.png 4 4 2\n", 1, "nope.png"},
	    {"sprite walk walk.png 4 4 3\n", 1, "need 12 x 4"},
	    {"sprite walk walk.png 4 x 2\n", 1, "whole number"},
	    {"sprite walk walk.png 4 4 2\nsprite walk walk.png 4 4 2\n", 2, "already defined"},
	    {"region a 0 0 1 1\n", 1, "'image' line"},
	    {"image walk.png\nregion a 6 0 4 4\n", 2, "outside"},
	    {"image walk.png\nregion a 0 0\n", 2, "expected: region"},
	};
	for (const Case& c : cases)
	{
		INFO(c.text);
		const PixelSheetResult result = Parse(c.text, folder);
		REQUIRE_FALSE(result.sheet.has_value());
		REQUIRE(result.error.line == c.line);
		REQUIRE(result.error.message.find(c.mentions) != std::string::npos);
	}
}

TEST_CASE("LoadSheetFile picks the loader by extension", "[graphics][sheet]")
{
	const TempFolder folder("dispatch");
	Save(TwoTone(2, 2), folder.path / "dot.png");
	{
		std::ofstream manifest(folder.path / "pics.sheet");
		manifest << "sprite dot dot.png 2 2\n";
	}
	{
		std::ofstream text(folder.path / "pics.txt");
		text << "color X 255 0 0\nsprite dot 2 2\nXX\nXX\n";
	}

	const PixelSheetResult fromPng = engine::graphics::LoadSheetFile(folder.path / "pics.sheet");
	const PixelSheetResult fromText = engine::graphics::LoadSheetFile(folder.path / "pics.txt");
	REQUIRE(fromPng.sheet.has_value());
	REQUIRE(fromText.sheet.has_value());
	REQUIRE(fromPng.sheet->sprites[0].image.At(1, 1) == kBlue);
	REQUIRE(fromText.sheet->sprites[0].image.At(1, 1) == kRed);
}
