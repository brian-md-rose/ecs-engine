#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "input/gamepad.hpp"
#include "input/keyboard.hpp"
#include "input/mouse.hpp"

namespace engine::input
{
	// Everything the player can do to the game, one value for a whole update: every device the
	// engine knows. Systems read it through FrameContext::input; Application owns it and fills it
	// from the platform's events. No SDL here, so tests and headless runners build one by hand.
	//
	// Like Keyboard, it is latched across frames: the application calls EndUpdate() after each
	// update, so an event that arrived in a frame with no update is still seen by the next one,
	// and is not seen twice.
	// Which kind of device the player used last.
	enum class Device : std::uint8_t
	{
		Keyboard,
		Mouse,
		Pad
	};

	struct Input
	{
		Keyboard keyboard;
		Mouse mouse;
		// Up to kMaxPads controllers, in the order they were plugged in; a slot that is not connected has
		// Gamepad::connected false. A pad that is unplugged leaves its slot free for the next one.
		std::array<Gamepad, kMaxPads> pads;
		// The device that last did something real (a key or button went down, the mouse moved, a stick went
		// past its deadzone), for switching between keyboard and pad button icons. Stick drift doesn't count.
		Device lastUsed = Device::Keyboard;

		// Text typed since the last update, as UTF-8, with case, punctuation and the keyboard
		// layout applied (so "Shift+1" on a US layout is "!"). Keys alone can't give that; use
		// this for name fields and filters.
		std::string typed;

		// Any connected pad: a button is held, went down since the last update, or went up.
		[[nodiscard]] bool PadDown(PadButton button) const;
		[[nodiscard]] bool PadPressed(PadButton button) const;
		[[nodiscard]] bool PadReleased(PadButton button) const;
		// Any connected pad pushed direction since the last update, on the d-pad or the left stick.
		[[nodiscard]] bool PadPushed(PadDirection direction) const;
		// Any connected pad has direction held, on the d-pad or the left stick.
		[[nodiscard]] bool PadDirectionHeld(PadDirection direction) const;
		// The right stick of the connected pads added together, kept within the unit circle.
		[[nodiscard]] Stick PadRightStick() const;
		// The family of the first connected pad, Generic if there is none.
		[[nodiscard]] PadFamily PadFamilyInUse() const;

		// Platform layer: text arrived (SDL_EVENT_TEXT_INPUT).
		void OnText(std::string_view text) { typed += text; }

		// Releases every held key and drops pending text, e.g. when the window loses focus and
		// further key-up events won't arrive.
		void ReleaseAll();

		// Called after each game update: clears the edges and the typed text it has seen.
		void EndUpdate();
	};
} // namespace engine::input
