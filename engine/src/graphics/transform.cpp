#include "graphics/transform.hpp"

#include <cmath>
#include <numbers>
#include <optional>

#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"

namespace engine::graphics
{
	Position TransformPoint(Position origin, float rotation, Scale scale, Position offset) noexcept
	{
		const float sx = offset.x * scale.x;
		const float sy = offset.y * scale.y;
		if (rotation == 0.0f) return {origin.x + sx, origin.y + sy};
		const float radians = rotation * (std::numbers::pi_v<float> / 180.0f);
		const float c = std::cos(radians);
		const float s = std::sin(radians);
		return {origin.x + sx * c - sy * s, origin.y + sx * s + sy * c};
	}

	namespace
	{
		// Deeper than any real tree; a chain this long is a loop of parents.
		constexpr int kMaxDepth = 64;

		// Where entity is in the world: its parent's world transform combined with its
		// LocalTransform, or its own Transform if it has no parent to follow. Nothing if it is dead
		// or has no Transform.
		std::optional<Transform> WorldTransform(ecs::Registry& registry, ecs::Entity entity, int depth)
		{
			ENGINE_VERIFY(depth < kMaxDepth, "ecs::Parent links form a loop");
			if (!registry.IsAlive(entity)) return std::nullopt;
			const Transform* transform = registry.TryGetComponent<Transform>(entity);
			if (transform == nullptr) return std::nullopt;

			const ecs::Parent* parent = registry.TryGetComponent<ecs::Parent>(entity);
			const LocalTransform* local = registry.TryGetComponent<LocalTransform>(entity);
			if (parent == nullptr || local == nullptr) return *transform;

			const std::optional<Transform> origin = WorldTransform(registry, parent->entity, depth + 1);
			if (!origin) return *transform;
			return Transform{.position =
			                     TransformPoint(origin->position, origin->rotation, origin->scale, local->position),
			                 .rotation = origin->rotation + local->rotation,
			                 .scale = {origin->scale.x * local->scale.x, origin->scale.y * local->scale.y}};
		}
	} // namespace

	void UpdateWorldTransforms(ecs::Registry& registry)
	{
		registry.Each<ecs::Parent, LocalTransform, Transform>(
		    [&registry](ecs::Entity entity, const ecs::Parent&, const LocalTransform&, Transform& transform)
		{
			if (const std::optional<Transform> world = WorldTransform(registry, entity, 0)) transform = *world;
		});
	}
} // namespace engine::graphics
