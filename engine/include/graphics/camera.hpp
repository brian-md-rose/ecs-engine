#pragma once

#include "graphics/transform.hpp"

namespace engine::graphics
{
	// The part of the world that is drawn. The view is logicalWidth x logicalHeight logical
	// pixels, the same as the Renderer's fixed logical resolution, which SDL scales up to the
	// window by whole numbers. position is the world point drawn at the screen's top-left:
	// move it to scroll or to leave room for a score bar.
	struct Camera
	{
		int logicalWidth = 0;
		int logicalHeight = 0;
		Position position;
	};
} // namespace engine::graphics
