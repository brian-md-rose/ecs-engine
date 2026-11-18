#pragma once

namespace engine::graphics
{
	// Component (a tag): this entity is placed on the screen, not in the world, so the Camera's
	// position, zoom and offset don't touch it. Use it for the HUD, panels, tooltips and cursors.
	// Its Transform is then in logical screen pixels.
	//
	// Children inherit it: an entity whose ecs::Parent, or any ancestor, has ScreenSpace is
	// screen-space too, so putting the tag on a panel is enough for everything inside it.
	struct ScreenSpace
	{
	};
} // namespace engine::graphics
