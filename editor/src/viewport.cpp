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

	bool MoveEntity(ecs::Registry& registry, ecs::Entity entity, const graphics::Camera& camera,
	                graphics::Position delta)
	{
		if (!registry.IsAlive(entity)) return false;
		const float scale = IsScreenSpace(registry, entity) ? 1.0F : static_cast<float>(camera.EffectiveZoom());
		const graphics::Position move{delta.x / scale, delta.y / scale};
		if (registry.HasComponent<ecs::Parent>(entity))
		{
			if (auto* local = registry.TryGetComponent<graphics::LocalTransform>(entity))
			{
				local->position.x += move.x;
				local->position.y += move.y;
				return true;
			}
		}
		if (auto* transform = registry.TryGetComponent<graphics::Transform>(entity))
		{
			transform->position.x += move.x;
			transform->position.y += move.y;
			return true;
		}
		return false;
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

namespace engine::editor
{
	void PlayMouse::Feed(const PointerSample& sample, const ViewTransform& view, graphics::Position origin, int width,
	                     int height)
	{
		if (!primed)
		{
			previous = sample.down; // a button already held when Play starts belongs to the editor
			primed = true;
		}
		for (std::size_t i = 0; i < input::kMouseButtonCount; ++i)
		{
			const auto button = static_cast<input::MouseButton>(i);
			if (sample.down[i] && !previous[i])
			{
				if (sample.hovered)
				{
					owned[i] = true;
					mouse.OnButton(button, true);
				}
			}
			else if (!sample.down[i] && previous[i] && owned[i])
			{
				owned[i] = false;
				mouse.OnButton(button, false);
			}
			previous[i] = sample.down[i];
		}

		if (sample.hovered || AnyOwned())
		{
			const graphics::Position picture = view.ToPicture(sample.window, origin);
			const ScreenRect area{0, 0, static_cast<float>(width), static_cast<float>(height)};
			mouse.OnMove(picture, sample.hovered && area.Contains(picture));
		}
		else if (mouse.inside)
		{
			mouse.OnMove(mouse.position, false);
		}
		if (sample.hovered && sample.wheel != 0.0F) mouse.OnWheel(sample.wheel);
	}

	void PlayMouse::Reset()
	{
		mouse.ReleaseAll();
		mouse.OnMove(mouse.position, false);
		mouse.EndUpdate();
		previous.fill(false);
		owned.fill(false);
		primed = false;
	}

	bool PlayMouse::AnyOwned() const noexcept
	{
		for (const bool held : owned)
			if (held) return true;
		return false;
	}
} // namespace engine::editor
