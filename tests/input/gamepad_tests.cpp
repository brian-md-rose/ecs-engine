#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "input/gamepad.hpp"
#include "input/input.hpp"

namespace
{
	using Catch::Matchers::WithinAbs;
	using namespace engine::input;
} // namespace

TEST_CASE("A deadzone removes the dead centre and rescales the rest", "[input][gamepad]")
{
	CHECK(ApplyDeadzone(0.1f, 0.1f, 0.2f) == Stick{}); // drift
	CHECK(ApplyDeadzone(0.0f, 0.0f, 0.2f) == Stick{});
	CHECK(ApplyDeadzone(0.2f, 0.0f, 0.2f) == Stick{}); // right on the edge is still inside

	const Stick full = ApplyDeadzone(1.0f, 0.0f, 0.2f);
	CHECK_THAT(full.x, WithinAbs(1.0, 1e-5));
	CHECK(full.y == 0.0f);

	const Stick half = ApplyDeadzone(0.6f, 0.0f, 0.2f); // (0.6 - 0.2) / 0.8
	CHECK_THAT(half.x, WithinAbs(0.5, 1e-5));

	// A diagonal keeps its direction, and a corner beyond the circle is held to 1.
	const Stick diagonal = ApplyDeadzone(0.5f, -0.5f, 0.2f);
	CHECK(diagonal.x > 0.0f);
	CHECK_THAT(diagonal.x, WithinAbs(-diagonal.y, 1e-5));
	const Stick corner = ApplyDeadzone(1.0f, 1.0f, 0.2f);
	CHECK_THAT(corner.x * corner.x + corner.y * corner.y, WithinAbs(1.0, 1e-4));
}

TEST_CASE("Trigger deadzones rescale 0 to 1", "[input][gamepad]")
{
	CHECK(ApplyTriggerDeadzone(0.1f, 0.2f) == 0.0f);
	CHECK_THAT(ApplyTriggerDeadzone(0.6f, 0.2f), WithinAbs(0.5, 1e-5));
	CHECK_THAT(ApplyTriggerDeadzone(1.0f, 0.2f), WithinAbs(1.0, 1e-5));
}

TEST_CASE("Buttons have latched edges like the keyboard's", "[input][gamepad]")
{
	Gamepad pad;
	pad.Connect(PadFamily::Xbox);
	CHECK_FALSE(pad.IsDown(PadButton::South));

	pad.OnButton(PadButton::South, true);
	CHECK(pad.IsDown(PadButton::South));
	CHECK(pad.WasPressed(PadButton::South));
	pad.EndUpdate();
	CHECK(pad.IsDown(PadButton::South));
	CHECK_FALSE(pad.WasPressed(PadButton::South));

	pad.OnButton(PadButton::South, true); // a repeat changes nothing
	CHECK_FALSE(pad.WasPressed(PadButton::South));
	pad.OnButton(PadButton::South, false);
	CHECK(pad.WasReleased(PadButton::South));
	pad.EndUpdate();
	CHECK_FALSE(pad.WasReleased(PadButton::South));

	// A tap that is over before the update still counts once.
	pad.OnButton(PadButton::East, true);
	pad.OnButton(PadButton::East, false);
	CHECK(pad.WasPressed(PadButton::East));
	CHECK(pad.WasReleased(PadButton::East));
	CHECK_FALSE(pad.IsDown(PadButton::East));
}

TEST_CASE("Stick axes are deadzoned, and only real movement counts as input", "[input][gamepad]")
{
	Gamepad pad;
	pad.Connect(PadFamily::Generic);

	CHECK_FALSE(pad.OnAxis(PadAxis::LeftX, 0.1f)); // drift
	CHECK(pad.Left() == Stick{});
	CHECK(pad.OnAxis(PadAxis::LeftX, 1.0f));
	CHECK_THAT(pad.Left().x, WithinAbs(1.0, 1e-5));
	CHECK_FALSE(pad.OnAxis(PadAxis::LeftX, 0.0f)); // back to rest
	CHECK(pad.Left() == Stick{});

	CHECK(pad.OnAxis(PadAxis::RightY, -0.8f));
	CHECK(pad.Right().y < 0.0f); // y is down, so this is up
	CHECK(pad.Left() == Stick{});

	// The two axes of one stick share a circular deadzone: 0.15 either way is inside it, 0.15 on both is not (0.21).
	CHECK_FALSE(pad.OnAxis(PadAxis::LeftX, 0.15f));
	CHECK(pad.OnAxis(PadAxis::LeftY, 0.15f));
	CHECK(pad.Left() != Stick{});

	CHECK(pad.OnAxis(PadAxis::LeftTrigger, 1.0f));
	CHECK_THAT(pad.LeftTrigger(), WithinAbs(1.0, 1e-5));
	CHECK_FALSE(pad.OnAxis(PadAxis::RightTrigger, 0.05f));
	CHECK(pad.RightTrigger() == 0.0f);
}

TEST_CASE("A stick pushed past the threshold acts like a d-pad press, once", "[input][gamepad]")
{
	Gamepad pad;
	pad.Connect(PadFamily::Generic);

	pad.OnAxis(PadAxis::LeftX, 0.3f); // past the deadzone, short of the threshold
	CHECK_FALSE(pad.StickHeld(PadDirection::Right));
	pad.EndUpdate();

	pad.OnAxis(PadAxis::LeftX, 1.0f);
	CHECK(pad.StickHeld(PadDirection::Right));
	CHECK(pad.StickWasPushed(PadDirection::Right));
	CHECK(pad.DirectionWasPushed(PadDirection::Right));
	CHECK_FALSE(pad.StickWasPushed(PadDirection::Left));
	pad.EndUpdate();

	CHECK(pad.StickHeld(PadDirection::Right));            // still held...
	CHECK_FALSE(pad.StickWasPushed(PadDirection::Right)); // ...but not pushed again
	CHECK(pad.DirectionHeld(PadDirection::Right));

	pad.OnAxis(PadAxis::LeftY, -1.0f);
	CHECK(pad.StickWasPushed(PadDirection::Up)); // y is down, so negative is up
	pad.OnButton(PadButton::DDown, true);
	CHECK(pad.DirectionWasPushed(PadDirection::Down)); // the d-pad counts too
	CHECK_FALSE(pad.StickHeld(PadDirection::Down));
}

TEST_CASE("A stick flick between two updates is still a push", "[input][gamepad]")
{
	Gamepad pad;
	pad.Connect(PadFamily::Xbox);
	pad.OnAxis(PadAxis::LeftX, 1.0f);
	pad.OnAxis(PadAxis::LeftX, 0.0f); // back to centre before the update runs
	CHECK_FALSE(pad.StickHeld(PadDirection::Right));
	CHECK(pad.StickWasPushed(PadDirection::Right));
	pad.EndUpdate();
	CHECK_FALSE(pad.StickWasPushed(PadDirection::Right));
}

TEST_CASE("Disconnecting or releasing everything clears a pad", "[input][gamepad]")
{
	Gamepad pad;
	pad.Connect(PadFamily::PlayStation);
	CHECK(pad.connected);
	CHECK(pad.family == PadFamily::PlayStation);
	pad.OnButton(PadButton::North, true);
	pad.OnAxis(PadAxis::LeftX, 1.0f);

	pad.ReleaseAll();
	CHECK(pad.connected);
	CHECK_FALSE(pad.IsDown(PadButton::North));
	CHECK(pad.WasReleased(PadButton::North)); // the game sees it let go
	CHECK(pad.Left() == Stick{});

	pad.OnButton(PadButton::West, true);
	pad.Disconnect();
	CHECK_FALSE(pad.connected);
	CHECK_FALSE(pad.IsDown(PadButton::West));
	CHECK(pad.family == PadFamily::Generic);
	CHECK(pad.WasReleased(PadButton::West)); // unplugged mid-hold: the release is still seen once
	pad.EndUpdate();
	CHECK_FALSE(pad.WasReleased(PadButton::West));
}

TEST_CASE("Button labels follow the pad's family", "[input][gamepad]")
{
	CHECK(PadButtonLabel(PadFamily::Xbox, PadButton::South) == "A");
	CHECK(PadButtonLabel(PadFamily::PlayStation, PadButton::South) == "CROSS");
	CHECK(PadButtonLabel(PadFamily::PlayStation, PadButton::North) == "TRIANGLE");
	CHECK(PadButtonLabel(PadFamily::Switch, PadButton::South) == "B"); // A and B swap places on a Switch pad
	CHECK(PadButtonLabel(PadFamily::Switch, PadButton::East) == "A");
	CHECK(PadButtonLabel(PadFamily::SteamDeck, PadButton::Start) == "MENU");
	CHECK(PadButtonLabel(PadFamily::Generic, PadButton::South) == "SOUTH");
	for (std::size_t i = 0; i < kPadButtonCount; ++i)
	{
		for (const PadFamily family :
		     {PadFamily::Xbox, PadFamily::PlayStation, PadFamily::Switch, PadFamily::SteamDeck, PadFamily::Generic})
		{
			CHECK_FALSE(PadButtonLabel(family, static_cast<PadButton>(i)).empty());
		}
	}
	CHECK(PadButtonLabel(PadFamily::Xbox, PadButton::Count).empty());
}

TEST_CASE("The Steam Deck is picked out by its ids or its name", "[input][gamepad]")
{
	CHECK(IsSteamDeck("Steam Deck Controller", 0, 0));
	CHECK(IsSteamDeck("Xbox Controller", 0x28de, 0x1205));
	CHECK_FALSE(IsSteamDeck("Xbox Controller", 0x045e, 0x02ea));
	CHECK_FALSE(IsSteamDeck("", 0x28de, 0x1142)); // a Steam Controller is not a Deck
}

TEST_CASE("Input answers for any connected pad", "[input][gamepad]")
{
	Input input;
	CHECK(input.lastUsed == Device::Keyboard);
	CHECK_FALSE(input.PadPressed(PadButton::South));
	CHECK(input.PadFamilyInUse() == PadFamily::Generic);

	input.pads[1].Connect(PadFamily::Switch);
	input.pads[1].OnButton(PadButton::South, true);
	CHECK(input.PadPressed(PadButton::South));
	CHECK(input.PadDown(PadButton::South));
	CHECK(input.PadFamilyInUse() == PadFamily::Switch);

	input.pads[0].Connect(PadFamily::Xbox);
	CHECK(input.PadFamilyInUse() == PadFamily::Xbox); // the first slot wins

	input.EndUpdate();
	CHECK_FALSE(input.PadPressed(PadButton::South));
	CHECK(input.PadDown(PadButton::South));
	input.pads[1].OnButton(PadButton::South, false);
	CHECK(input.PadReleased(PadButton::South));

	// A pad that is not connected is ignored, even with state left in it.
	input.pads[2].OnButton(PadButton::East, true);
	CHECK_FALSE(input.PadDown(PadButton::East));
}

TEST_CASE("Input pushes, held directions and the right stick come from every pad", "[input][gamepad]")
{
	Input input;
	input.pads[0].Connect(PadFamily::Xbox);
	input.pads[1].Connect(PadFamily::Xbox);

	input.pads[1].OnButton(PadButton::DLeft, true);
	CHECK(input.PadPushed(PadDirection::Left));
	CHECK(input.PadDirectionHeld(PadDirection::Left));
	CHECK_FALSE(input.PadPushed(PadDirection::Right));

	// Two pads pushing the right stick down add up, but stay inside the unit circle.
	input.pads[0].OnAxis(PadAxis::RightY, 1.0f);
	input.pads[1].OnAxis(PadAxis::RightY, 1.0f);
	CHECK_THAT(input.PadRightStick().y, WithinAbs(1.0, 1e-5));
	input.pads[1].OnAxis(PadAxis::RightY, 0.0f);
	CHECK_THAT(input.PadRightStick().y, WithinAbs(1.0, 1e-5));
	input.pads[0].OnAxis(PadAxis::RightY, 0.0f);
	CHECK(input.PadRightStick() == Stick{});
}

TEST_CASE("Losing focus lets go of every pad button too", "[input][gamepad]")
{
	Input input;
	input.pads[0].Connect(PadFamily::Xbox);
	input.pads[0].OnButton(PadButton::R1, true);
	input.ReleaseAll();
	CHECK_FALSE(input.PadDown(PadButton::R1));
	CHECK(input.pads[0].connected);
}
