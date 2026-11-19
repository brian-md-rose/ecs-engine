#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

#include "graphics/display.hpp"
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
		if (std::getenv("SDL_VIDEO_DRIVER") == nullptr) _putenv_s("SDL_VIDEO_DRIVER", "offscreen");
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
