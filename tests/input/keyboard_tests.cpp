#include <catch2/catch_test_macros.hpp>

#include "input/keyboard.hpp"

namespace
{
	using engine::input::Key;
	using engine::input::Keyboard;
} // namespace

TEST_CASE("Keyboard reports a press, the hold, and the release", "[input]")
{
	Keyboard keyboard;

	keyboard.OnKeyDown(Key::Up);
	CHECK(keyboard.WasPressed(Key::Up));
	CHECK(keyboard.IsDown(Key::Up));
	CHECK_FALSE(keyboard.WasReleased(Key::Up));
	keyboard.EndUpdate();

	// Still down on the next update, but no longer "just pressed".
	CHECK_FALSE(keyboard.WasPressed(Key::Up));
	CHECK(keyboard.IsDown(Key::Up));
	keyboard.EndUpdate();

	keyboard.OnKeyUp(Key::Up);
	CHECK(keyboard.WasReleased(Key::Up));
	CHECK_FALSE(keyboard.IsDown(Key::Up));
	keyboard.EndUpdate();

	CHECK_FALSE(keyboard.WasReleased(Key::Up));
}

TEST_CASE("Keyboard keeps a tap for the next update", "[input]")
{
	Keyboard keyboard;

	// Down and up between two updates: the next update must still see the press, once.
	keyboard.OnKeyDown(Key::Enter);
	keyboard.OnKeyUp(Key::Enter);
	CHECK(keyboard.WasPressed(Key::Enter));
	CHECK(keyboard.WasReleased(Key::Enter));
	CHECK_FALSE(keyboard.IsDown(Key::Enter));

	keyboard.EndUpdate();
	CHECK_FALSE(keyboard.WasPressed(Key::Enter));
}

TEST_CASE("Keyboard ignores OS key repeat and stray key-ups", "[input]")
{
	Keyboard keyboard;

	keyboard.OnKeyDown(Key::P);
	keyboard.EndUpdate();
	keyboard.OnKeyDown(Key::P); // repeat
	CHECK_FALSE(keyboard.WasPressed(Key::P));

	keyboard.OnKeyUp(Key::Space); // never went down
	CHECK_FALSE(keyboard.WasReleased(Key::Space));
}

TEST_CASE("Keyboard releases every held key when focus is lost", "[input]")
{
	Keyboard keyboard;

	keyboard.OnKeyDown(Key::Up);
	keyboard.OnKeyDown(Key::A);
	keyboard.EndUpdate();

	keyboard.ReleaseAll();
	CHECK_FALSE(keyboard.IsDown(Key::Up));
	CHECK_FALSE(keyboard.IsDown(Key::A));
	CHECK(keyboard.WasReleased(Key::Up));
	CHECK(keyboard.WasReleased(Key::A));
	CHECK_FALSE(keyboard.WasReleased(Key::Space)); // wasn't down
}
