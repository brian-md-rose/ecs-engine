#pragma once

#include "ecs/entity.hpp"
#include "graphics/image.hpp"

namespace engine::ecs
{
	class Registry;
}

namespace engine::graphics
{
	// What an entity inherits from itself and its ancestors (through ecs::Parent): ScreenSpace if any
	// of them has it, and every Clip among them, intersected.
	struct Ancestry
	{
		bool screenSpace = false;
		bool clipped = false;
		PixelRect clip{}; // meaningful when clipped; zero-sized if the clips don't overlap
	};

	// Walks from entity up its parents. It stops after a few hundred steps so a loop in the hierarchy
	// can't hang the frame.
	[[nodiscard]] Ancestry ResolveAncestry(ecs::Registry& registry, ecs::Entity entity);

	// The part of a and b that both cover; empty (zero size) if they don't meet.
	[[nodiscard]] PixelRect Intersect(const PixelRect& a, const PixelRect& b);
} // namespace engine::graphics
