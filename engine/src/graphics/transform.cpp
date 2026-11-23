#include "graphics/transform.hpp"

#include <cmath>
#include <numbers>
#include <optional>

#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "graphics/shape.hpp"
#include "graphics/sprite.hpp"

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

		// Half the size of the box an entity is drawn as, in world units: what it turns and stretches about.
		// Zero for anything without a Shape or Sprite (text included), which then turns about its position.
		Position HalfSize(ecs::Registry& registry, ecs::Entity entity)
		{
			if (const Shape* shape = registry.TryGetComponent<Shape>(entity))
			{
				return {shape->BoundsWidth() * 0.5f, shape->BoundsHeight() * 0.5f};
			}
			if (const Sprite* sprite = registry.TryGetComponent<Sprite>(entity))
			{
				const int width = sprite->width > 0 ? sprite->width : sprite->source.width;
				const int height = sprite->height > 0 ? sprite->height : sprite->source.height;
				return {static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f};
			}
			return {};
		}

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
			if (origin->rotation == 0.0f && origin->scale.x == 1.0f && origin->scale.y == 1.0f)
			{
				// Nothing to turn or stretch about, so no centres to work out (and no rounding).
				return Transform{
				    .position = {origin->position.x + local->position.x, origin->position.y + local->position.y},
				    .rotation = local->rotation,
				    .scale = local->scale};
			}
			// Parent and child move as one body: the child's centre sits at its offset from the parent's
			// centre, and that offset is stretched and turned about the parent's centre.
			const Position parentHalf = HalfSize(registry, parent->entity);
			const Position childHalf = HalfSize(registry, entity);
			const Position parentCentre{origin->position.x + parentHalf.x, origin->position.y + parentHalf.y};
			const Position childCentre = TransformPoint(
			    parentCentre, origin->rotation, origin->scale,
			    {local->position.x + childHalf.x - parentHalf.x, local->position.y + childHalf.y - parentHalf.y});
			return Transform{.position = {childCentre.x - childHalf.x, childCentre.y - childHalf.y},
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
