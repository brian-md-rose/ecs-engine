#include "graphics/transform.hpp"

#include <optional>

#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"

namespace engine::graphics
{
	namespace
	{
		// Deeper than any real tree; a chain this long is a loop of parents.
		constexpr int kMaxDepth = 64;

		// Where entity is in the world: its parent's world position plus its LocalTransform, or
		// its own Transform if it has no parent to follow. Nothing if it is dead or has no
		// Transform.
		std::optional<Position> WorldPosition(ecs::Registry& registry, ecs::Entity entity, int depth)
		{
			ENGINE_VERIFY(depth < kMaxDepth, "ecs::Parent links form a loop");
			if (!registry.IsAlive(entity)) return std::nullopt;
			const Transform* transform = registry.TryGetComponent<Transform>(entity);
			if (transform == nullptr) return std::nullopt;

			const ecs::Parent* parent = registry.TryGetComponent<ecs::Parent>(entity);
			const LocalTransform* local = registry.TryGetComponent<LocalTransform>(entity);
			if (parent == nullptr || local == nullptr) return transform->position;

			const std::optional<Position> origin = WorldPosition(registry, parent->entity, depth + 1);
			if (!origin) return transform->position;
			return Position{origin->x + local->position.x, origin->y + local->position.y};
		}
	} // namespace

	void UpdateWorldTransforms(ecs::Registry& registry)
	{
		registry.Each<ecs::Parent, LocalTransform, Transform>(
		    [&registry](ecs::Entity entity, const ecs::Parent&, const LocalTransform&, Transform& transform)
		{
			if (const std::optional<Position> world = WorldPosition(registry, entity, 0)) transform.position = *world;
		});
	}
} // namespace engine::graphics
