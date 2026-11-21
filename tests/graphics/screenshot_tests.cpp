#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "graphics/image.hpp"
#include "graphics/screenshot.hpp"

namespace
{
	using engine::graphics::CropImage;
	using engine::graphics::Image;
	using engine::graphics::PixelRect;
	using engine::graphics::ResizeImage;
	using engine::graphics::Rgba;
	using engine::graphics::ScreenshotFileName;
	using engine::graphics::ScreenshotTime;
	using engine::graphics::UniqueScreenshotPath;

	constexpr Rgba kRed{255, 0, 0, 255};
	constexpr Rgba kBlue{0, 0, 255, 255};

	// A folder that is empty and removed again at the end of the test.
	struct TempFolder
	{
		std::filesystem::path path;
		explicit TempFolder(const char* name) : path(std::filesystem::temp_directory_path() / name)
		{
			std::filesystem::remove_all(path);
			std::filesystem::create_directories(path);
		}
		~TempFolder()
		{
			std::error_code error;
			std::filesystem::remove_all(path, error);
		}
		TempFolder(const TempFolder&) = delete;
		TempFolder& operator=(const TempFolder&) = delete;
		TempFolder(TempFolder&&) = delete;
		TempFolder& operator=(TempFolder&&) = delete;
	};
} // namespace

TEST_CASE("A screenshot's file name is its local time, padded", "[screenshot]")
{
	const ScreenshotTime time{.year = 2026, .month = 11, .day = 5, .hour = 9, .minute = 4, .second = 3};
	CHECK(ScreenshotFileName(time) == "screenshot-20261105-090403.png");
	CHECK(ScreenshotFileName(time, 1) == "screenshot-20261105-090403.png");
	CHECK(ScreenshotFileName(time, 2) == "screenshot-20261105-090403-2.png");
	CHECK(ScreenshotFileName(time, 17) == "screenshot-20261105-090403-17.png");
}

TEST_CASE("Out-of-range time fields are clamped, so the name keeps its shape", "[screenshot]")
{
	const ScreenshotTime time{.year = 12345, .month = 0, .day = 99, .hour = -1, .minute = 61, .second = 70};
	CHECK(ScreenshotFileName(time) == "screenshot-99990131-005960.png");
}

TEST_CASE("Two screenshots in one second get different files", "[screenshot]")
{
	const TempFolder folder("ecs_screenshot_unique");
	const ScreenshotTime time{.year = 2026, .month = 1, .day = 2, .hour = 3, .minute = 4, .second = 5};

	const std::filesystem::path first = UniqueScreenshotPath(folder.path, time);
	CHECK(first.filename() == "screenshot-20260102-030405.png");
	std::ofstream(first).put('x');

	const std::filesystem::path second = UniqueScreenshotPath(folder.path, time);
	CHECK(second.filename() == "screenshot-20260102-030405-2.png");
	std::ofstream(second).put('x');

	CHECK(UniqueScreenshotPath(folder.path, time).filename() == "screenshot-20260102-030405-3.png");
}

TEST_CASE("Screenshots go to the repository folder when it exists, else next to the log", "[screenshot]")
{
	const TempFolder repository("ecs_screenshot_repo");
	const std::filesystem::path save = "/nowhere/save";

	CHECK(engine::graphics::ScreenshotDirectory(save, repository.path) == repository.path);
	CHECK(engine::graphics::ScreenshotDirectory(save, repository.path / "missing") == save / "screenshots");
	CHECK(engine::graphics::ScreenshotDirectory(save, {}) == save / "screenshots");
	CHECK(engine::graphics::ScreenshotDirectory({}, {}).empty());
	CHECK(engine::graphics::ScreenshotDirectory({}, repository.path) == repository.path);
}

TEST_CASE("Cropping keeps the pixels inside the rectangle and clips the rest", "[screenshot]")
{
	Image image(4, 3);
	image.Set(1, 1, kRed);
	image.Set(3, 2, kBlue);

	const Image middle = CropImage(image, {.x = 1, .y = 1, .width = 3, .height = 2});
	REQUIRE(middle.Width() == 3);
	REQUIRE(middle.Height() == 2);
	CHECK(middle.At(0, 0) == kRed);
	CHECK(middle.At(2, 1) == kBlue);

	const Image clipped = CropImage(image, {.x = -2, .y = -2, .width = 4, .height = 4});
	CHECK(clipped.Width() == 2);
	CHECK(clipped.Height() == 2);

	CHECK(CropImage(image, {.x = 9, .y = 0, .width = 3, .height = 3}).IsEmpty());
	CHECK(CropImage(image, {.x = 0, .y = 0, .width = 0, .height = 3}).IsEmpty());
}

TEST_CASE("Shrinking a picture by a whole number gives back the original pixels", "[screenshot]")
{
	// A 2x2 picture drawn 4x as big.
	Image big(8, 8);
	for (int y = 0; y < 8; ++y)
	{
		for (int x = 0; x < 8; ++x)
		{
			big.Set(x, y, (x < 4) == (y < 4) ? kRed : kBlue);
		}
	}
	const Image small = ResizeImage(big, 2, 2);
	REQUIRE(small.Width() == 2);
	REQUIRE(small.Height() == 2);
	CHECK(small.At(0, 0) == kRed);
	CHECK(small.At(1, 0) == kBlue);
	CHECK(small.At(0, 1) == kBlue);
	CHECK(small.At(1, 1) == kRed);
}

TEST_CASE("Shrinking by a fraction averages the pixels it covers", "[screenshot]")
{
	Image image(3, 1);
	image.Set(0, 0, Rgba{0, 0, 0, 255});
	image.Set(1, 0, Rgba{100, 100, 100, 255});
	image.Set(2, 0, Rgba{200, 200, 200, 255});
	const Image two = ResizeImage(image, 2, 1); // pixel 0 covers source 0, pixel 1 covers sources 1 and 2
	REQUIRE(two.Width() == 2);
	CHECK(two.At(0, 0) == Rgba{0, 0, 0, 255});
	CHECK(two.At(1, 0) == Rgba{150, 150, 150, 255});
}

TEST_CASE("Resizing handles empty images, bad sizes and growing", "[screenshot]")
{
	CHECK(ResizeImage(Image(), 4, 4).IsEmpty());
	CHECK(ResizeImage(Image(2, 2), 0, 4).IsEmpty());
	CHECK(ResizeImage(Image(2, 2), 4, -1).IsEmpty());

	Image one(1, 1);
	one.Set(0, 0, kRed);
	const Image grown = ResizeImage(one, 3, 2);
	REQUIRE(grown.Width() == 3);
	REQUIRE(grown.Height() == 2);
	CHECK(grown.At(2, 1) == kRed);
}
