#include "graphics/ancestry.hpp"

#include <algorithm>

#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "graphics/clip.hpp"
#include "graphics/screenspace.hpp"

namespace engine::graphics
{
	PixelRect Intersect(const PixelRect& a, const PixelRect& b)
	{
		const int left = std::max(a.x, b.x);
		const int top = std::max(a.y, b.y);
		const int right = std::min(a.x + a.width, b.x + b.width);
		const int bottom = std::min(a.y + a.height, b.y + b.height);
		return {.x = left, .y = top, .width = std::max(right - left, 0), .height = std::max(bottom - top, 0)};
	}

	Ancestry ResolveAncestry(ecs::Registry& registry, ecs::Entity entity)
	{
		Ancestry result;
		for (int depth = 0; depth < 256 && entity != ecs::Entity::Null; ++depth)
		{
			if (registry.HasComponent<ScreenSpace>(entity)) result.screenSpace = true;
			if (const Clip* clip = registry.TryGetComponent<Clip>(entity))
			{
				result.clip = result.clipped ? Intersect(result.clip, clip->rect) : clip->rect;
				result.clipped = true;
			}
			const ecs::Parent* parent = registry.TryGetComponent<ecs::Parent>(entity);
			if (parent == nullptr) break;
			entity = parent->entity;
		}
		return result;
	}
} // namespace engine::graphics
