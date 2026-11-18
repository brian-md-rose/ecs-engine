#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "graphics/transform.hpp"

namespace engine::input
{
	enum class MouseButton : std::uint8_t
	{
		Left,
		Right,
		Middle,
		Count
	};

	inline constexpr std::size_t kMouseButtonCount = static_cast<std::size_t>(MouseButton::Count);

	// Mouse state for fixed-rate game updates. Positions are in logical (screen) pixels, already
	// corrected for letterboxing by the platform layer. World coordinates come from the camera
	// (Camera::ScreenToWorld), since the camera belongs to the game.
	//
	// Edges are latched like Keyboard's: the application calls EndUpdate() after each update, so a
	// press and release inside one frame still counts as one click, and is not seen twice.
	class Mouse
	{
		std::array<bool, kMouseButtonCount> down{};
		std::array<bool, kMouseButtonCount> pressed{};
		std::array<bool, kMouseButtonCount> released{};

	public:
		// Last known pointer position, in logical pixels.
		graphics::Position position{};
		// Over the logical area, not the letterbox bars or outside the window.
		bool inside = false;
		// Wheel notches since the last update; positive is away from the user (scroll up).
		float wheel = 0;

		// Platform layer: report events.
		void OnMove(graphics::Position where, bool isInside);
		void OnButton(MouseButton button, bool isDown);
		void OnWheel(float notches) { wheel += notches; }

		// Releases every held button, e.g. when the window loses focus.
		void ReleaseAll();

		// Called after each game update: clears the edges and the wheel it has seen.
		void EndUpdate();

		[[nodiscard]] bool IsDown(MouseButton button) const;
		// Went down since the last update (it may already be back up).
		[[nodiscard]] bool WasPressed(MouseButton button) const;
		// Went up since the last update.
		[[nodiscard]] bool WasReleased(MouseButton button) const;
	};
} // namespace engine::input
