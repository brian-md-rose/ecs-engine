#pragma once

#include "graphics/image.hpp"

namespace engine::graphics
{
	// Component: nothing of this entity, or of anything under it (through ecs::Parent), is drawn
	// outside rect. The rectangle is in logical screen pixels, so it ignores the Camera: use it for a
	// scrolling list or a panel. A clip inside a clip narrows to where both overlap, so a list in a
	// clipped panel stays inside the panel. Combine with ScreenSpace for a clipped HUD panel.
	struct Clip
	{
		PixelRect rect{};
	};
} // namespace engine::graphics
