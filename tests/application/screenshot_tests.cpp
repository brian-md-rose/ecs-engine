#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>

#include "application/application.hpp"
#include "core/clock.hpp"
#include "graphics/imagefile.hpp"
#include "graphics/renderer.hpp"
#include "graphics/screenshot.hpp"

// Live tests of F12 and ScreenshotRequest on SDL's offscreen video driver (chosen in graphics/window_tests.cpp,
// which is in the same executable). They skip if the machine can't make a window.
namespace
{
	using engine::application::Application;
	using engine::application::ApplicationConfig;
	using engine::application::ScreenshotLink;

	// Time that moves 1/60 s every time it is read.
	class SteppingClock final : public engine::core::Clock
	{
		mutable double now = 0.0;

	public:
		[[nodiscard]] double Now() const override
		{
			now += 1.0 / 60.0;
			return now;
		}
	};

	// Draws a flat colour for a few frames and then quits. Optionally asks for a screenshot on its first update.
	class FlatGame final : public engine::application::Game
	{
		int frames = 0;
		bool ask;

	public:
		engine::graphics::ScreenshotRequest request;
		engine::graphics::ScreenshotResult result;

		explicit FlatGame(bool askOnFirstUpdate) : ask(askOnFirstUpdate) {}

		void Update(double, const engine::input::Input&, engine::audio::AudioDevice*) override
		{
			if (ask)
			{
				request.pending = true;
				ask = false;
			}
		}
		void Render(engine::graphics::Renderer& renderer, double) override
		{
			renderer.Clear(10, 200, 30);
			++frames;
		}
		[[nodiscard]] bool WantsToQuit() const override { return frames >= 3; }
		[[nodiscard]] ScreenshotLink Screenshot() override { return {.request = &request, .result = &result}; }
	};

	struct TempFolder
	{
		std::filesystem::path path;
		explicit TempFolder(const char* name) : path(std::filesystem::temp_directory_path() / name)
		{
			std::filesystem::remove_all(path);
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

	[[nodiscard]] ApplicationConfig SmallConfig()
	{
		ApplicationConfig config;
		config.title = "screenshot test";
		config.windowWidth = 256;
		config.windowHeight = 144;
		config.logicalWidth = 64;
		config.logicalHeight = 36;
		return config;
	}
} // namespace

TEST_CASE("A game's screenshot request saves a PNG at the logical size", "[screenshot][application]")
{
	const TempFolder folder("ecs_screenshot_app");
	Application app(SmallConfig());
	app.SetScreenshotFolder(folder.path);
	FlatGame game(true);
	const SteppingClock clock;
	if (app.Run(game, clock) != 0) SKIP("no offscreen video driver");

	CHECK(game.result.lastError.empty());
	REQUIRE(game.result.count == 1);
	CHECK_FALSE(game.request.pending);
	REQUIRE(std::filesystem::exists(game.result.lastFile));
	CHECK(game.result.lastFile.parent_path() == folder.path);

	const engine::graphics::ImageResult loaded = engine::graphics::LoadImage(game.result.lastFile);
	REQUIRE(loaded.image.has_value());
	CHECK(loaded.image->Width() == 64);
	CHECK(loaded.image->Height() == 36);
	CHECK(loaded.image->At(0, 0) == engine::graphics::Rgba{10, 200, 30, 255});
	CHECK(loaded.image->At(63, 35) == engine::graphics::Rgba{10, 200, 30, 255});
}

TEST_CASE("Without a folder a screenshot fails with a reason and writes nothing", "[screenshot][application]")
{
	Application app(SmallConfig());
	FlatGame game(true);
	const SteppingClock clock;
	if (app.Run(game, clock) != 0) SKIP("no offscreen video driver");

	CHECK(game.result.count == 0);
	CHECK_FALSE(game.result.lastError.empty());
	CHECK_FALSE(game.request.pending);
}

TEST_CASE("The screenshot key can be turned off, and RequestScreenshot still works", "[screenshot][application]")
{
	const TempFolder folder("ecs_screenshot_key");
	Application app(SmallConfig());
	app.SetScreenshotFolder(folder.path);
	app.SetScreenshotKey(std::nullopt);
	app.RequestScreenshot();
	FlatGame game(false);
	const SteppingClock clock;
	if (app.Run(game, clock) != 0) SKIP("no offscreen video driver");

	CHECK(game.result.count == 1);
}
