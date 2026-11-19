#include "editor/viewport.hpp"

#include <algorithm>
#include <cstdint>

#include "ecs/hierarchy.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/shape.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "ui/ui.hpp"

namespace engine::editor
{
	namespace
	{
		struct Extent
		{
			float width = 0;
			float height = 0;
			std::uint8_t layer = 0;
		};

		bool IsScreenSpace(const ecs::Registry& registry, ecs::Entity entity)
		{
			// The depth limit stops a Parent loop.
			for (int depth = 0; depth < 256 && entity != ecs::Entity::Null; ++depth)
			{
				if (registry.HasComponent<graphics::ScreenSpace>(entity)) return true;
				const auto* parent = registry.TryGetComponent<ecs::Parent>(entity);
				if (parent == nullptr) return false;
				entity = parent->entity;
			}
			return false;
		}

		std::optional<Extent> ExtentOf(const ecs::Registry& registry, ecs::Entity entity)
		{
			if (const auto* shape = registry.TryGetComponent<graphics::Shape>(entity))
				return Extent{shape->BoundsWidth(), shape->BoundsHeight(), shape->layer};
			if (const auto* sprite = registry.TryGetComponent<graphics::Sprite>(entity))
			{
				const int width = sprite->width > 0 ? sprite->width : sprite->source.width;
				const int height = sprite->height > 0 ? sprite->height : sprite->source.height;
				return Extent{static_cast<float>(width), static_cast<float>(height), sprite->layer};
			}
			if (const auto* hitbox = registry.TryGetComponent<ui::Hitbox>(entity))
				return Extent{hitbox->width, hitbox->height, hitbox->layer};
			return std::nullopt;
		}
	} // namespace

	std::optional<ScreenRect> EntityScreenRect(const ecs::Registry& registry, ecs::Entity entity,
	                                           const graphics::Camera& camera)
	{
		const auto* transform = registry.TryGetComponent<graphics::Transform>(entity);
		if (transform == nullptr) return std::nullopt;
		const std::optional<Extent> extent = ExtentOf(registry, entity);
		if (!extent || extent->width <= 0 || extent->height <= 0) return std::nullopt;

		if (IsScreenSpace(registry, entity))
			return ScreenRect{transform->position.x, transform->position.y, extent->width, extent->height};
		const graphics::Position corner = camera.WorldToScreen(transform->position);
		const auto zoom = static_cast<float>(camera.EffectiveZoom());
		return ScreenRect{corner.x, corner.y, extent->width * zoom, extent->height * zoom};
	}

	ecs::Entity PickEntity(const ecs::Registry& registry, const graphics::Camera& camera, graphics::Position point)
	{
		ecs::Entity best = ecs::Entity::Null;
		int bestLayer = -1;
		for (const ecs::Entity entity : registry.AllEntities())
		{
			const std::optional<ScreenRect> rect = EntityScreenRect(registry, entity, camera);
			if (!rect || !rect->Contains(point)) continue;
			const int layer = ExtentOf(registry, entity)->layer;
			if (layer >= bestLayer)
			{
				best = entity;
				bestLayer = layer;
			}
		}
		return best;
	}

	void ViewTransform::ZoomAt(float factor, graphics::Position window, graphics::Position origin) noexcept
	{
		const float newZoom = std::clamp(zoom * factor, kMinZoom, kMaxZoom);
		const graphics::Position picture = ToPicture(window, origin);
		zoom = newZoom;
		panX = window.x - origin.x - picture.x * zoom;
		panY = window.y - origin.y - picture.y * zoom;
	}
} // namespace engine::editor
