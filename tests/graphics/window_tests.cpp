#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

#include "graphics/display.hpp"
#include "graphics/image.hpp"
#include "graphics/renderer.hpp"
#include "graphics/window.hpp"

// Live tests of the window and renderer settings, on SDL's offscreen video driver so they need no
// display. If a machine can't make even that window, they skip rather than fail.
namespace
{
	using engine::graphics::Renderer;
	using engine::graphics::Scaling;
	using engine::graphics::Window;
	using engine::graphics::WindowMode;

	// Chosen before main, so SDL has not looked for a video driver yet whatever test runs first. A driver the
	// user picked in the environment wins.
	[[maybe_unused]] const bool kOffscreenChosen = []
	{
#ifdef _WIN32
		char* chosen = nullptr;
		std::size_t length = 0;
		if (_dupenv_s(&chosen, &length, "SDL_VIDEO_DRIVER") != 0 || chosen == nullptr)
		{
			_putenv_s("SDL_VIDEO_DRIVER", "offscreen");
		}
		std::free(chosen);
#else
		setenv("SDL_VIDEO_DRIVER", "offscreen", 0);
#endif
		return true;
	}();
} // namespace

TEST_CASE("The renderer changes its scaling and VSync after it is made", "[window][display]")
{
	Window window("test", 320, 180);
	if (!window.IsValid()) SKIP("no offscreen video driver");
	Renderer renderer(window, 160, 90, Scaling::Integer);
	if (!renderer.IsValid()) SKIP("no renderer");

	CHECK(renderer.GetScaling() == Scaling::Integer);
	CHECK(renderer.SetScaling(Scaling::Fit));
	CHECK(renderer.GetScaling() == Scaling::Fit);
	CHECK(renderer.SetScaling(Scaling::Integer));
	CHECK(renderer.GetScaling() == Scaling::Integer);
	renderer.SetVSync(false); // a driver may refuse; it must not crash
	renderer.SetVSync(true);
}

TEST_CASE("A window can switch between its modes without failing", "[window][display]")
{
	Window window("test", 320, 180);
	if (!window.IsValid()) SKIP("no offscreen video driver");

	// The offscreen driver may not support every mode, so only check that nothing throws or crashes and
	// that the window is still usable afterwards.
	window.SetMode(WindowMode::Borderless, 0, 0);
	window.SetMode(WindowMode::Fullscreen, 640, 360);
	window.SetMode(WindowMode::Windowed, 400, 300);
	CHECK(window.IsValid());

	for (const engine::graphics::Resolution& size : window.Resolutions())
	{
		CHECK(size.width > 0);
		CHECK(size.height > 0);
	}
}

TEST_CASE("The renderer reads back the window and the logical picture", "[window][screenshot]")
{
	Window window("test", 320, 180);
	if (!window.IsValid()) SKIP("no offscreen video driver");
	Renderer renderer(window, 80, 45, Scaling::Integer); // 4x in a 320x180 window
	if (!renderer.IsValid()) SKIP("no renderer");

	renderer.Clear(200, 100, 50);
	const engine::graphics::Image whole = renderer.ReadWindowPixels();
	REQUIRE_FALSE(whole.IsEmpty());
	CHECK(whole.At(0, 0) == engine::graphics::Rgba{200, 100, 50, 255});

	const engine::graphics::Image logical = renderer.ReadLogicalPixels();
	REQUIRE_FALSE(logical.IsEmpty());
	CHECK(logical.Width() == 80);
	CHECK(logical.Height() == 45);
	CHECK(logical.At(79, 44) == engine::graphics::Rgba{200, 100, 50, 255});
}

TEST_CASE("Reading the logical picture leaves out the letterbox bars", "[window][screenshot]")
{
	Window window("test", 320, 200);
	if (!window.IsValid()) SKIP("no offscreen video driver");
	Renderer renderer(window, 80, 45, Scaling::Integer); // 4x is 320x180: 10 pixel bars above and below
	if (!renderer.IsValid()) SKIP("no renderer");

	renderer.Clear(200, 100, 50);
	const engine::graphics::Image whole = renderer.ReadWindowPixels();
	if (whole.Width() != 320 || whole.Height() != 200) SKIP("the driver's window is not the size asked for");

	const engine::graphics::Image logical = renderer.ReadLogicalPixels();
	REQUIRE_FALSE(logical.IsEmpty());
	CHECK(logical.Width() == 80);
	CHECK(logical.Height() == 45);
	// What is in the picture is what is under the 320x180 area, so it matches the window's pixel inside it.
	CHECK(logical.At(0, 0) == whole.At(0, 10));
	CHECK(logical.At(79, 44) == whole.At(319, 189));
}
