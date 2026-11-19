#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "application/scheduledgame.hpp"
#include "graphics/display.hpp"

namespace
{
	using Catch::Matchers::WithinAbs;
	using engine::graphics::DisplaySettings;
	using engine::graphics::Presentation;
	using engine::graphics::PresentationRect;
	using engine::graphics::Resolution;
	using engine::graphics::Scaling;
	using engine::graphics::WindowMode;
} // namespace

TEST_CASE("Fit mode fills a 1280 x 800 window with a 960 x 540 picture, letterboxed", "[display]")
{
	const PresentationRect rect = Presentation(960, 540, 1280, 800, Scaling::Fit);
	CHECK_THAT(rect.scale, WithinAbs(4.0 / 3.0, 1e-5));
	CHECK_THAT(rect.width, WithinAbs(1280.0, 1e-3)); // the full width...
	CHECK_THAT(rect.height, WithinAbs(720.0, 1e-3));
	CHECK_THAT(rect.x, WithinAbs(0.0, 1e-3));
	CHECK_THAT(rect.y, WithinAbs(40.0, 1e-3)); // ...with equal bars above and below
}

TEST_CASE("Integer mode keeps whole-number scales and centres the picture", "[display]")
{
	// 960 x 540 in 1280 x 800: only x1 fits, so the picture is smaller than the window.
	const PresentationRect small = Presentation(960, 540, 1280, 800, Scaling::Integer);
	CHECK(small.scale == 1.0f);
	CHECK(small.x == 160.0f);
	CHECK(small.y == 130.0f);

	// 224 x 288 in 672 x 864: exactly x3.
	const PresentationRect exact = Presentation(224, 288, 672, 864, Scaling::Integer);
	CHECK(exact.scale == 3.0f);
	CHECK(exact.width == 672.0f);
	CHECK(exact.x == 0.0f);

	// 320 x 180 in 1000 x 700: x3 (960 x 540), the limit being the width.
	CHECK(Presentation(320, 180, 1000, 700, Scaling::Integer).scale == 3.0f);
}

TEST_CASE("A window smaller than the picture still shows all of it", "[display]")
{
	const PresentationRect rect = Presentation(320, 180, 160, 90, Scaling::Integer);
	CHECK_THAT(rect.scale, WithinAbs(0.5, 1e-6));
	CHECK_THAT(rect.width, WithinAbs(160.0, 1e-3));
	CHECK(Presentation(320, 180, 0, 90, Scaling::Fit) == PresentationRect{});
	CHECK(Presentation(0, 180, 100, 90, Scaling::Integer) == PresentationRect{});
}

TEST_CASE("Sanitized keeps a request's size and frame cap in range", "[display]")
{
	const DisplaySettings low = engine::graphics::Sanitized({.width = 0, .height = -5, .frameCap = -1});
	CHECK(low.width == engine::graphics::kMinWindowSize);
	CHECK(low.height == engine::graphics::kMinWindowSize);
	CHECK(low.frameCap == 0);

	const DisplaySettings high =
	    engine::graphics::Sanitized({.width = 1'000'000, .height = 99'999, .frameCap = 99'999});
	CHECK(high.width == engine::graphics::kMaxWindowSize);
	CHECK(high.height == engine::graphics::kMaxWindowSize);
	CHECK(high.frameCap == engine::graphics::kMaxFrameCap);

	const DisplaySettings fine{.mode = WindowMode::Fullscreen, .width = 1920, .height = 1080, .frameCap = 144};
	CHECK(engine::graphics::Sanitized(fine) == fine);
}

TEST_CASE("NormalizeResolutions lists each size once, largest first, and drops nonsense", "[display]")
{
	const std::vector<Resolution> sizes = engine::graphics::NormalizeResolutions(
	    {{1280, 720}, {1920, 1080}, {1280, 720}, {0, 600}, {800, 600}, {1920, 1080}, {-1, -1}, {1600, 900}});
	const std::vector<Resolution> expected = {{1920, 1080}, {1600, 900}, {1280, 720}, {800, 600}};
	CHECK(sizes == expected);
	CHECK(engine::graphics::NormalizeResolutions({}).empty());
}

TEST_CASE("NormalizeResolutions orders equal areas by width", "[display]")
{
	const std::vector<Resolution> sizes = engine::graphics::NormalizeResolutions({{600, 800}, {800, 600}});
	CHECK(sizes.front() == Resolution{800, 600});
}

TEST_CASE("FrameDelaySeconds waits out what is left of the frame", "[display]")
{
	CHECK_THAT(engine::graphics::FrameDelaySeconds(10.0, 10.004, 100), WithinAbs(0.006, 1e-9));
	CHECK(engine::graphics::FrameDelaySeconds(10.0, 10.02, 100) == 0.0); // already slower than the cap
	CHECK(engine::graphics::FrameDelaySeconds(10.0, 10.0, 0) == 0.0);    // no cap
	CHECK(engine::graphics::FrameDelaySeconds(10.0, 10.0, -5) == 0.0);
	CHECK_THAT(engine::graphics::FrameDelaySeconds(10.0, 10.0, 60), WithinAbs(1.0 / 60.0, 1e-9));
}

TEST_CASE("A ScheduledGame hands Application the display resources it has", "[display]")
{
	engine::application::ScheduledGame game;
	engine::application::DisplayLink link = game.Display();
	CHECK(link.request == nullptr);
	CHECK(link.info == nullptr);

	game.world.resources.Emplace<engine::graphics::DisplayRequest>();
	link = game.Display();
	CHECK(link.request != nullptr);
	CHECK(link.info == nullptr);

	game.world.resources.Emplace<engine::graphics::DisplayInfo>();
	link = game.Display();
	CHECK(link.info != nullptr);
	link.request->pending = DisplaySettings{.mode = WindowMode::Borderless};
	CHECK(game.world.resources.Get<engine::graphics::DisplayRequest>().pending.has_value());
}
