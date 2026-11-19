#include <catch2/catch_test_macros.hpp>

#include "controls.hpp"
#include "input/keyboard.hpp"

namespace
{
	using engine::grid::Direction;
	using engine::input::Key;
	using engine::input::Keyboard;
	using game::Action;
	using game::Controls;

	// Key events go to the keyboard, then Update() runs one game update the way Application
	// and ControlsSystem do: Controls reads the keyboard, then the keyboard's edges are cleared.
	struct Harness
	{
		Keyboard keyboard;
		Controls controls;

		void Update()
		{
			controls.Update(keyboard);
			keyboard.EndUpdate();
		}
	};
} // namespace

TEST_CASE("Controls turns key events into action presses, holds and releases", "[game][controls]")
{
	Harness h;

	h.keyboard.OnKeyDown(Key::Up);
	h.Update();
	CHECK(h.controls.WasPressed(Action::MoveUp));
	CHECK(h.controls.IsHeld(Action::MoveUp));
	CHECK_FALSE(h.controls.WasReleased(Action::MoveUp));

	h.Update();
	CHECK_FALSE(h.controls.WasPressed(Action::MoveUp));
	CHECK(h.controls.IsHeld(Action::MoveUp));

	h.keyboard.OnKeyUp(Key::Up);
	h.Update();
	CHECK(h.controls.WasReleased(Action::MoveUp));
	CHECK_FALSE(h.controls.IsHeld(Action::MoveUp));

	h.Update();
	CHECK_FALSE(h.controls.WasReleased(Action::MoveUp));
}

TEST_CASE("Controls counts a tap between two updates as a press", "[game][controls]")
{
	Harness h;

	h.keyboard.OnKeyDown(Key::Enter);
	h.keyboard.OnKeyUp(Key::Enter);
	h.Update();
	CHECK(h.controls.WasPressed(Action::Confirm));
	CHECK(h.controls.WasReleased(Action::Confirm));
	CHECK_FALSE(h.controls.IsHeld(Action::Confirm));

	h.Update();
	CHECK_FALSE(h.controls.WasPressed(Action::Confirm));
	CHECK_FALSE(h.controls.WasReleased(Action::Confirm));
}

TEST_CASE("Controls holds an action until every key bound to it is up", "[game][controls]")
{
	Harness h;

	h.keyboard.OnKeyDown(Key::Left);
	h.Update();

	// A second key for the same action isn't a new press.
	h.keyboard.OnKeyDown(Key::A);
	h.Update();
	CHECK_FALSE(h.controls.WasPressed(Action::MoveLeft));

	h.keyboard.OnKeyUp(Key::Left);
	h.Update();
	CHECK(h.controls.IsHeld(Action::MoveLeft));
	CHECK_FALSE(h.controls.WasReleased(Action::MoveLeft));

	h.keyboard.OnKeyUp(Key::A);
	h.Update();
	CHECK_FALSE(h.controls.IsHeld(Action::MoveLeft));
	CHECK(h.controls.WasReleased(Action::MoveLeft));
}

TEST_CASE("Controls buffers the last direction pressed", "[game][controls]")
{
	Harness h;
	h.Update();
	CHECK(h.controls.BufferedDirection() == Direction::None);

	h.keyboard.OnKeyDown(Key::Right);
	h.Update();
	CHECK(h.controls.BufferedDirection() == Direction::Right);

	// A turn pressed early and released before the junction is still remembered.
	h.keyboard.OnKeyDown(Key::W);
	h.keyboard.OnKeyUp(Key::W);
	h.Update();
	h.Update();
	CHECK(h.controls.BufferedDirection() == Direction::Up);

	// Pressing a second key for a held direction counts as asking for it again.
	h.keyboard.OnKeyDown(Key::D);
	h.Update();
	CHECK(h.controls.BufferedDirection() == Direction::Right);

	// Non-direction actions don't touch it.
	h.keyboard.OnKeyDown(Key::Enter);
	h.Update();
	CHECK(h.controls.BufferedDirection() == Direction::Right);

	h.controls.ClearBufferedDirection();
	CHECK(h.controls.BufferedDirection() == Direction::None);
}

TEST_CASE("Controls bindings can be changed, even while keys are held", "[game][controls]")
{
	Harness h;
	CHECK(h.controls.GetBinding(Key::W) == Action::MoveUp);

	h.keyboard.OnKeyDown(Key::W);
	h.Update();
	CHECK(h.controls.IsHeld(Action::MoveUp));

	// Rebinding a held key: the old action is released, not left stuck down.
	h.controls.Bind(Key::W, Action::Confirm);
	h.Update();
	CHECK_FALSE(h.controls.IsHeld(Action::MoveUp));
	CHECK(h.controls.WasReleased(Action::MoveUp));
	CHECK(h.controls.IsHeld(Action::Confirm));

	// An unbound key does nothing.
	h.keyboard.OnKeyUp(Key::W);
	h.controls.Unbind(Key::Enter);
	CHECK_FALSE(h.controls.GetBinding(Key::Enter).has_value());
	h.keyboard.OnKeyDown(Key::Enter);
	h.Update();
	CHECK_FALSE(h.controls.IsHeld(Action::Confirm));
	CHECK_FALSE(h.controls.WasPressed(Action::Confirm));
}

TEST_CASE("A pad's d-pad, left stick and buttons drive the same actions", "[game][controls][pad]")
{
	using engine::input::Input;
	using engine::input::PadAxis;
	using engine::input::PadButton;
	using engine::input::PadFamily;

	game::Controls controls;
	Input input;
	input.pads[0].Connect(PadFamily::Xbox);

	input.pads[0].OnButton(PadButton::DUp, true);
	controls.Update(input);
	CHECK(controls.WasPressed(game::Action::MoveUp));
	CHECK(controls.IsHeld(game::Action::MoveUp));
	CHECK(controls.BufferedDirection() == engine::grid::Direction::Up);
	input.EndUpdate();
	controls.Update(input);
	CHECK_FALSE(controls.WasPressed(game::Action::MoveUp));
	CHECK(controls.IsHeld(game::Action::MoveUp));
	input.pads[0].OnButton(PadButton::DUp, false);
	input.EndUpdate();
	controls.Update(input);
	CHECK_FALSE(controls.IsHeld(game::Action::MoveUp));
	CHECK(controls.WasReleased(game::Action::MoveUp));

	input.pads[0].OnAxis(PadAxis::LeftX, -1.0f); // the stick, to the left
	controls.Update(input);
	CHECK(controls.WasPressed(game::Action::MoveLeft));
	CHECK(controls.IsHeld(game::Action::MoveLeft));
	input.pads[0].OnAxis(PadAxis::LeftX, 0.0f);
	input.EndUpdate();

	input.pads[0].OnButton(PadButton::South, true);
	input.pads[0].OnButton(PadButton::Start, true);
	controls.Update(input);
	CHECK(controls.WasPressed(game::Action::Confirm));
	CHECK(controls.WasPressed(game::Action::Pause));
}

TEST_CASE("Pad buttons can be rebound, and the keyboard alone ignores pads", "[game][controls][pad]")
{
	using engine::input::Input;
	using engine::input::PadButton;
	using engine::input::PadFamily;

	game::Controls controls;
	CHECK(controls.GetPadBinding(PadButton::South) == game::Action::Confirm);
	controls.BindPad(PadButton::East, game::Action::Pause);
	controls.UnbindPad(PadButton::Start);
	CHECK(controls.GetPadBinding(PadButton::East) == game::Action::Pause);
	CHECK_FALSE(controls.GetPadBinding(PadButton::Start).has_value());

	Input input;
	input.pads[0].Connect(PadFamily::Xbox);
	input.pads[0].OnButton(PadButton::East, true);
	input.pads[0].OnButton(PadButton::Start, true);
	controls.Update(input);
	CHECK(controls.WasPressed(game::Action::Pause)); // East, not Start

	game::Controls keyboardOnly;
	keyboardOnly.Update(input.keyboard); // the old call: no pads
	CHECK_FALSE(keyboardOnly.IsHeld(game::Action::Confirm));

	// A pad that is not connected does nothing.
	game::Controls other;
	Input empty;
	empty.pads[2].OnButton(PadButton::South, true);
	other.Update(empty);
	CHECK_FALSE(other.IsHeld(game::Action::Confirm));
}
