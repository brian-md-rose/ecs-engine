#include "graphics/interpolation.hpp"

#include <algorithm>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"

namespace engine::graphics
{
	Position InterpolatedPosition(const Transform& transform, const Interpolation& interpolation, float alpha) noexcept
	{
		const Position current = transform.position;
		if (!interpolation.hasPrevious) return current;

		const float dx = current.x - interpolation.previous.x;
		const float dy = current.y - interpolation.previous.y;
		const float snap = interpolation.snapDistance;
		if (dx * dx + dy * dy > snap * snap) return current;

		const float t = std::clamp(alpha, 0.0f, 1.0f);
		return {interpolation.previous.x + dx * t, interpolation.previous.y + dy * t};
	}

	void RecordPreviousPositions(ecs::Registry& registry)
	{
		registry.Each<Interpolation, Transform>(
		    [](ecs::Entity, Interpolation& interpolation, const Transform& transform)
		{
			interpolation.previous = transform.position;
			interpolation.hasPrevious = true;
		});
	}
} // namespace engine::graphics
