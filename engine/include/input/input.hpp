#pragma once

#include <string>
#include <string_view>

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
	struct Input
	{
		Keyboard keyboard;
		Mouse mouse;

		// Text typed since the last update, as UTF-8, with case, punctuation and the keyboard
		// layout applied (so "Shift+1" on a US layout is "!"). Keys alone can't give that; use
		// this for name fields and filters.
		std::string typed;

		// Platform layer: text arrived (SDL_EVENT_TEXT_INPUT).
		void OnText(std::string_view text) { typed += text; }

		// Releases every held key and drops pending text, e.g. when the window loses focus and
		// further key-up events won't arrive.
		void ReleaseAll();

		// Called after each game update: clears the edges and the typed text it has seen.
		void EndUpdate();
	};
} // namespace engine::input
