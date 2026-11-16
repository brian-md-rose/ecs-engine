#pragma once

#include "graphics/image.hpp"

// Flip-book animation over a strip of frames in the atlas. Pure: no SDL, so frame stepping is
// tested directly. A game advances every Animation each update and copies AnimationSource into
// the entity's Sprite.
namespace engine::graphics
{
	// A strip of equal frames side by side in the atlas, as a pixel sheet's sprite packs them:
	// frame i is i frame-widths to the right of firstFrame.
	struct Clip
	{
		PixelRect firstFrame{};
		int frames = 1;
		float secondsPerFrame = 0.125f;
		bool loop = true; // otherwise it stops on its last frame

		friend bool operator==(const Clip&, const Clip&) = default;
	};

	// Component: which clip an entity shows, and how far into it it is.
	struct Animation
	{
		Clip clip{};
		float elapsed = 0.0f; // seconds since the clip started
	};

	// The frame being shown, from 0: it wraps round for a looping clip and stays on the last
	// frame of one that doesn't loop.
	[[nodiscard]] int AnimationFrame(const Animation& animation) noexcept;

	// The part of the atlas the frame being shown covers.
	[[nodiscard]] PixelRect AnimationSource(const Animation& animation) noexcept;

	// seconds later. Negative times count as 0.
	[[nodiscard]] Animation Advance(Animation animation, float seconds) noexcept;

	// Shows clip: from its first frame if it is a different clip, carrying on if it is the one
	// already playing (so a system can ask for the right clip every update).
	[[nodiscard]] Animation Play(Animation animation, const Clip& clip) noexcept;
} // namespace engine::graphics
