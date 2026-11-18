#include <catch2/catch_test_macros.hpp>

#include "input/input.hpp"
#include "input/mouse.hpp"

namespace
{
	using engine::input::Mouse;
	using engine::input::MouseButton;
} // namespace

TEST_CASE("Mouse reports a press, the hold, and the release", "[input]")
{
	Mouse mouse;

	mouse.OnButton(MouseButton::Left, true);
	CHECK(mouse.WasPressed(MouseButton::Left));
	CHECK(mouse.IsDown(MouseButton::Left));
	CHECK_FALSE(mouse.WasReleased(MouseButton::Left));
	CHECK_FALSE(mouse.IsDown(MouseButton::Right));
	mouse.EndUpdate();

	CHECK_FALSE(mouse.WasPressed(MouseButton::Left));
	CHECK(mouse.IsDown(MouseButton::Left));

	mouse.OnButton(MouseButton::Left, false);
	CHECK(mouse.WasReleased(MouseButton::Left));
	CHECK_FALSE(mouse.IsDown(MouseButton::Left));
	mouse.EndUpdate();
	CHECK_FALSE(mouse.WasReleased(MouseButton::Left));
}

TEST_CASE("A press and release inside one update still counts as a click", "[input]")
{
	Mouse mouse;
	mouse.OnButton(MouseButton::Right, true);
	mouse.OnButton(MouseButton::Right, false);

	CHECK(mouse.WasPressed(MouseButton::Right));
	CHECK(mouse.WasReleased(MouseButton::Right));
	CHECK_FALSE(mouse.IsDown(MouseButton::Right));

	mouse.EndUpdate();
	CHECK_FALSE(mouse.WasPressed(MouseButton::Right));
	CHECK_FALSE(mouse.WasReleased(MouseButton::Right));
}

TEST_CASE("Repeated button events don't make new edges", "[input]")
{
	Mouse mouse;
	mouse.OnButton(MouseButton::Middle, true);
	mouse.EndUpdate();
	mouse.OnButton(MouseButton::Middle, true);
	CHECK_FALSE(mouse.WasPressed(MouseButton::Middle));

	mouse.OnButton(MouseButton::Left, false);
	CHECK_FALSE(mouse.WasReleased(MouseButton::Left));
}

TEST_CASE("Mouse position, inside flag and wheel", "[input]")
{
	Mouse mouse;
	CHECK_FALSE(mouse.inside);

	mouse.OnMove({10.5F, 20.0F}, true);
	CHECK(mouse.position.x == 10.5F);
	CHECK(mouse.position.y == 20.0F);
	CHECK(mouse.inside);

	mouse.OnWheel(1);
	mouse.OnWheel(2);
	CHECK(mouse.wheel == 3.0F);
	mouse.EndUpdate();
	CHECK(mouse.wheel == 0.0F);
	// The position is kept between updates.
	CHECK(mouse.position.x == 10.5F);

	mouse.OnMove({-4.0F, 3.0F}, false);
	CHECK_FALSE(mouse.inside);
}

TEST_CASE("Input forwards EndUpdate and ReleaseAll to the mouse", "[input]")
{
	engine::input::Input input;
	input.mouse.OnButton(MouseButton::Left, true);
	input.mouse.OnWheel(1);
	input.EndUpdate();
	CHECK_FALSE(input.mouse.WasPressed(MouseButton::Left));
	CHECK(input.mouse.IsDown(MouseButton::Left));
	CHECK(input.mouse.wheel == 0.0F);

	input.ReleaseAll();
	CHECK_FALSE(input.mouse.IsDown(MouseButton::Left));
}
