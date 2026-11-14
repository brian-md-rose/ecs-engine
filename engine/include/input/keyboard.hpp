#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace engine::input
{
	// Engine key codes. The platform layer translates its own key events into these, so game
	// code and tests never see SDL types. Keys are physical positions (scancodes): WASD stays
	// in the same place on AZERTY or Dvorak keyboards. Add keys here as games need them.
	enum class Key : std::uint8_t
	{
		Up,
		Down,
		Left,
		Right,
		W,
		A,
		S,
		D,
		P,
		Space,
		Enter,
		Escape,
		F1,
		Count
	};

	inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);

	// Raw keyboard state for fixed-rate game updates. Knows nothing about what keys mean;
	// games map keys to their own actions.
	//
	// Events arrive once per frame, but the game may update zero, one or several times per
	// frame. So "pressed" and "released" are latched until the next update has seen them:
	// the application calls EndUpdate() after each update. A key tapped during a frame with
	// no update is still reported as pressed by the next one, and is not reported twice.
	class Keyboard
	{
		std::array<bool, kKeyCount> down{};
		std::array<bool, kKeyCount> pressed{};
		std::array<bool, kKeyCount> released{};

	public:
		// Platform layer: report key transitions. Repeated downs of a held key (OS key
		// repeat) are ignored, as are ups of keys that aren't down.
		void OnKeyDown(Key key);
		void OnKeyUp(Key key);

		// Releases every held key, e.g. when the window loses focus and further key-up
		// events won't arrive.
		void ReleaseAll();

		// Called after each game update: clears the pressed/released edges it has seen.
		void EndUpdate();

		[[nodiscard]] bool IsDown(Key key) const;
		// Went down since the last update (it may already be back up).
		[[nodiscard]] bool WasPressed(Key key) const;
		// Went up since the last update.
		[[nodiscard]] bool WasReleased(Key key) const;
	};
} // namespace engine::input
