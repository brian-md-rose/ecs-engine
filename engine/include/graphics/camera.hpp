#pragma once

#include <algorithm>

#include "graphics/transform.hpp"

namespace engine::graphics
{
	// The part of the world that is drawn. The view is logicalWidth x logicalHeight logical
	// pixels, the same as the Renderer's fixed logical resolution, which SDL scales up to the
	// window by whole numbers. position is the world point drawn at the screen's top-left:
	// move it to scroll or to leave room for a score bar.
	//
	// zoom magnifies the world: a world unit covers zoom logical pixels, so sprites, text and shapes
	// come out zoom times as big. Whole numbers only, so pixel art stays sharp (a value under 1
	// counts as 1). The view then shows logicalWidth / zoom by logicalHeight / zoom world units from
	// position. Entities with ScreenSpace ignore the camera altogether.
	struct Camera
	{
		int logicalWidth = 0;
		int logicalHeight = 0;
		Position position{};
		int zoom = 1;
		// Added to every world-space position after zooming, in screen pixels: screen shake that
		// is the same size at any zoom.
		Position offset{};

		// The zoom in effect: never below 1.
		[[nodiscard]] constexpr int EffectiveZoom() const noexcept { return std::max(zoom, 1); }

		// Where a world point lands on the screen, in logical pixels.
		[[nodiscard]] constexpr Position WorldToScreen(Position world) const noexcept
		{
			const auto scale = static_cast<float>(EffectiveZoom());
			return {(world.x - position.x) * scale + offset.x, (world.y - position.y) * scale + offset.y};
		}

		// The world point under a screen position, such as the mouse's.
		[[nodiscard]] constexpr Position ScreenToWorld(Position screen) const noexcept
		{
			const auto scale = static_cast<float>(EffectiveZoom());
			return {(screen.x - offset.x) / scale + position.x, (screen.y - offset.y) / scale + position.y};
		}

		// How much of the world the view shows, in world units.
		[[nodiscard]] constexpr Position ViewSize() const noexcept
		{
			const auto scale = static_cast<float>(EffectiveZoom());
			return {static_cast<float>(logicalWidth) / scale, static_cast<float>(logicalHeight) / scale};
		}
	};

	// Points the camera at worldPoint: sets the zoom (at least 1) and scrolls so the point is in the
	// middle of the view. Smooth panning is up to the game; call this each frame with where it
	// wants to look.
	constexpr void FocusOn(Camera& camera, Position worldPoint, int zoom) noexcept
	{
		camera.zoom = std::max(zoom, 1);
		const Position view = camera.ViewSize();
		camera.position = {worldPoint.x - view.x / 2.0f, worldPoint.y - view.y / 2.0f};
	}
} // namespace engine::graphics
