#pragma once

#include <limits>

#include "graphics/transform.hpp"

namespace engine::ecs
{
	class Registry;
}

// Smooth drawing between fixed updates. The game updates at a fixed rate (60 Hz) but the screen
// may refresh faster, so a frame usually falls between two updates. An entity with an
// Interpolation is drawn part of the way from where it was at the previous update to where it
// is now, by the frame's alpha, instead of jumping once per update. This draws the world one
// update (about 17 ms) behind, which is the usual price.
namespace engine::graphics
{
	// Component: where the entity was at the start of the current update.
	// ScheduledGame records it before every update (RecordPreviousPositions).
	struct Interpolation
	{
		Position previous{};
		bool hasPrevious = false; // false until the first update has recorded previous

		// A move longer than this in one update (in logical pixels, straight-line) is a jump,
		// e.g. through a wrap tunnel or back to a spawn: it is drawn at once rather than slid
		// across the screen. Infinite (the default) never snaps.
		float snapDistance = std::numeric_limits<float>::infinity();
	};

	// Where to draw the entity: previous + (current - previous) * alpha, with alpha in [0, 1].
	// The current position as it is if nothing has been recorded yet, or if the move since the
	// previous update is longer than snapDistance. Pure.
	[[nodiscard]] Position InterpolatedPosition(const Transform& transform, const Interpolation& interpolation,
	                                            float alpha) noexcept;

	// Copies every Interpolation entity's Transform position into its previous. Call once at the
	// start of each fixed update, before anything moves; ScheduledGame::Update does.
	void RecordPreviousPositions(ecs::Registry& registry);
} // namespace engine::graphics
