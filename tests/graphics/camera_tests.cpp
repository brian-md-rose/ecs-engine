#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "graphics/camera.hpp"

namespace
{
	using Catch::Approx;
	using engine::graphics::Camera;
	using engine::graphics::FocusOn;
	using engine::graphics::Position;
} // namespace

TEST_CASE("A camera at zoom 1 only scrolls", "[graphics][camera]")
{
	const Camera camera{.logicalWidth = 320, .logicalHeight = 180, .position = {100.0f, 50.0f}};
	const Position screen = camera.WorldToScreen({110.0f, 70.0f});
	CHECK(screen.x == 10.0f);
	CHECK(screen.y == 20.0f);
	CHECK(camera.ViewSize().x == 320.0f);
	CHECK(camera.ViewSize().y == 180.0f);
}

TEST_CASE("Zoom magnifies about the camera position", "[graphics][camera]")
{
	const Camera camera{.logicalWidth = 320, .logicalHeight = 180, .position = {100.0f, 50.0f}, .zoom = 2};
	CHECK(camera.WorldToScreen({100.0f, 50.0f}).x == 0.0f);
	CHECK(camera.WorldToScreen({110.0f, 70.0f}).x == 20.0f);
	CHECK(camera.WorldToScreen({110.0f, 70.0f}).y == 40.0f);
	CHECK(camera.ViewSize().x == 160.0f);
	CHECK(camera.ViewSize().y == 90.0f);
}

TEST_CASE("The offset shifts the screen and is not zoomed", "[graphics][camera]")
{
	const Camera camera{
	    .logicalWidth = 320, .logicalHeight = 180, .position = {10.0f, 10.0f}, .zoom = 3, .offset = {2.0f, -1.0f}};
	CHECK(camera.WorldToScreen({11.0f, 11.0f}).x == 5.0f); // (11 - 10) * 3 + 2
	CHECK(camera.WorldToScreen({11.0f, 11.0f}).y == 2.0f); // (11 - 10) * 3 - 1
}

TEST_CASE("ScreenToWorld undoes WorldToScreen", "[graphics][camera]")
{
	for (const int zoom : {1, 2, 3})
	{
		const Camera camera{.logicalWidth = 320,
		                    .logicalHeight = 180,
		                    .position = {37.5f, -12.0f},
		                    .zoom = zoom,
		                    .offset = {3.0f, 4.0f}};
		for (const Position world : {Position{0.0f, 0.0f}, Position{100.0f, 60.0f}, Position{-5.5f, 200.0f}})
		{
			const Position back = camera.ScreenToWorld(camera.WorldToScreen(world));
			CHECK(back.x == Approx(world.x));
			CHECK(back.y == Approx(world.y));
		}
	}
}

TEST_CASE("A zoom under 1 counts as 1", "[graphics][camera]")
{
	const Camera camera{.logicalWidth = 100, .logicalHeight = 100, .zoom = 0};
	CHECK(camera.EffectiveZoom() == 1);
	CHECK(camera.WorldToScreen({4.0f, 4.0f}).x == 4.0f);
	CHECK(camera.ViewSize().x == 100.0f);
}

TEST_CASE("FocusOn puts a world point in the middle of the view", "[graphics][camera]")
{
	Camera camera{.logicalWidth = 320, .logicalHeight = 180};
	FocusOn(camera, {500.0f, 300.0f}, 2);
	CHECK(camera.zoom == 2);
	const Position middle = camera.WorldToScreen({500.0f, 300.0f});
	CHECK(middle.x == 160.0f);
	CHECK(middle.y == 90.0f);

	FocusOn(camera, {0.0f, 0.0f}, 0); // the zoom is at least 1
	CHECK(camera.zoom == 1);
	CHECK(camera.WorldToScreen({0.0f, 0.0f}).x == 160.0f);
}
