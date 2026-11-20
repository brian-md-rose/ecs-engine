#include "editor/viewport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ecs/hierarchy.hpp"
#include "graphics/font.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/shape.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "ui/ninepatch.hpp"
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
			float offsetX = 0; // where the left edge is from the Transform (text can end or centre on it)
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
			if (const auto* patch = registry.TryGetComponent<ui::NinePatch>(entity))
				return Extent{static_cast<float>(patch->width), static_cast<float>(patch->height), patch->layer};
			if (const auto* text = registry.TryGetComponent<graphics::Text>(entity))
			{
				// The font is not known here, so the engine's default metrics (6 by 8) stand in for it.
				static const graphics::BitmapFont font;
				const graphics::TextSize size = graphics::MeasureText(font, text->value, text->wrapWidth, text->scale);
				const float left = graphics::AlignedX(0.0F, text->align, size.width);
				return Extent{static_cast<float>(size.width), static_cast<float>(size.height), text->layer, left};
			}
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
			return ScreenRect{transform->position.x + extent->offsetX, transform->position.y, extent->width,
			                  extent->height};
		const graphics::Position corner = camera.WorldToScreen(transform->position);
		const auto zoom = static_cast<float>(camera.EffectiveZoom());
		return ScreenRect{corner.x + extent->offsetX * zoom, corner.y, extent->width * zoom, extent->height * zoom};
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

	std::vector<ecs::Entity> EntitiesInBox(const ecs::Registry& registry, const graphics::Camera& camera,
	                                       const ScreenRect& box)
	{
		struct Hit
		{
			ecs::Entity entity;
			int layer;
		};
		std::vector<Hit> hits;
		for (const ecs::Entity entity : registry.AllEntities())
		{
			const std::optional<ScreenRect> rect = EntityScreenRect(registry, entity, camera);
			if (rect && rect->Overlaps(box)) hits.push_back({entity, ExtentOf(registry, entity)->layer});
		}
		std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b)
		{
			return a.layer < b.layer;
		});
		std::vector<ecs::Entity> found;
		found.reserve(hits.size());
		for (const Hit& hit : hits)
			found.push_back(hit.entity);
		return found;
	}

	std::optional<graphics::Position> MovablePosition(const ecs::Registry& registry, ecs::Entity entity)
	{
		if (!registry.IsAlive(entity)) return std::nullopt;
		if (registry.HasComponent<ecs::Parent>(entity))
		{
			if (const auto* local = registry.TryGetComponent<graphics::LocalTransform>(entity)) return local->position;
		}
		if (const auto* transform = registry.TryGetComponent<graphics::Transform>(entity)) return transform->position;
		return std::nullopt;
	}

	bool SetMovablePosition(ecs::Registry& registry, ecs::Entity entity, graphics::Position position)
	{
		if (!registry.IsAlive(entity)) return false;
		if (registry.HasComponent<ecs::Parent>(entity))
		{
			if (auto* local = registry.TryGetComponent<graphics::LocalTransform>(entity))
			{
				local->position = position;
				return true;
			}
		}
		if (auto* transform = registry.TryGetComponent<graphics::Transform>(entity))
		{
			transform->position = position;
			return true;
		}
		return false;
	}

	float UnitsPerPixel(const ecs::Registry& registry, ecs::Entity entity, const graphics::Camera& camera)
	{
		return IsScreenSpace(registry, entity) ? 1.0F : 1.0F / static_cast<float>(camera.EffectiveZoom());
	}

	graphics::Position DragTarget(graphics::Position start, graphics::Position moved, bool axisLock,
	                              float grid) noexcept
	{
		if (axisLock)
		{
			if (std::abs(moved.x) >= std::abs(moved.y))
				moved.y = 0;
			else
				moved.x = 0;
		}
		graphics::Position target{start.x + moved.x, start.y + moved.y};
		if (grid > 0)
		{
			target.x = std::round(target.x / grid) * grid;
			target.y = std::round(target.y / grid) * grid;
		}
		return target;
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

namespace engine::editor
{
	bool ViewTransform::ScrollIntoView(const ScreenRect& rect, float width, float height) noexcept
	{
		const graphics::Position a = ToWindow({rect.x, rect.y}, {0, 0});
		const graphics::Position b = ToWindow({rect.x + rect.width, rect.y + rect.height}, {0, 0});
		if (a.x < width && b.x > 0 && a.y < height && b.y > 0) return false;
		panX = width / 2.0F - (rect.x + rect.width / 2.0F) * zoom;
		panY = height / 2.0F - (rect.y + rect.height / 2.0F) * zoom;
		return true;
	}
} // namespace engine::editor
