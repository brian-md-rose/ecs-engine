#include "editor/viewport.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

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

		// Only a Shape or Sprite is turned and stretched when drawn (the renderer does nothing for the rest).
		bool IsTurned(const ecs::Registry& registry, ecs::Entity entity)
		{
			const auto* transform = registry.TryGetComponent<graphics::Transform>(entity);
			if (transform == nullptr) return false;
			if (transform->rotation == 0.0F && transform->scale.x == 1.0F && transform->scale.y == 1.0F) return false;
			return registry.HasComponent<graphics::Shape>(entity) || registry.HasComponent<graphics::Sprite>(entity);
		}

		constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0F;

		float Cross(graphics::Position a, graphics::Position b, graphics::Position p) noexcept
		{
			return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
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

	std::optional<Corners> EntityScreenCorners(const ecs::Registry& registry, ecs::Entity entity,
	                                           const graphics::Camera& camera)
	{
		const std::optional<ScreenRect> rect = EntityScreenRect(registry, entity, camera);
		if (!rect) return std::nullopt;
		const float right = rect->x + rect->width;
		const float bottom = rect->y + rect->height;
		if (!IsTurned(registry, entity))
			return Corners{{{rect->x, rect->y}, {right, rect->y}, {right, bottom}, {rect->x, bottom}}};
		// Turned and stretched about the middle of the box, in screen pixels (the camera's zoom is the same both ways,
		// so it does not matter whether that is done before or after it).
		const auto& transform = *registry.TryGetComponent<graphics::Transform>(entity);
		const graphics::Position centre{rect->x + rect->width * 0.5F, rect->y + rect->height * 0.5F};
		const float halfWidth = rect->width * 0.5F;
		const float halfHeight = rect->height * 0.5F;
		const auto corner = [&](float dx, float dy)
		{
			return graphics::TransformPoint(centre, transform.rotation, transform.scale, {dx, dy});
		};
		return Corners{{corner(-halfWidth, -halfHeight), corner(halfWidth, -halfHeight), corner(halfWidth, halfHeight),
		                corner(-halfWidth, halfHeight)}};
	}

	bool QuadContains(const Corners& corners, graphics::Position point) noexcept
	{
		float area = 0;
		for (std::size_t i = 0; i < 4; ++i)
		{
			const graphics::Position a = corners[i];
			const graphics::Position b = corners[(i + 1) % 4];
			area += a.x * b.y - b.x * a.y;
		}
		if (std::abs(area) < 1e-6F) return false; // squashed flat: nothing to hit
		bool positive = false;
		bool negative = false;
		for (std::size_t i = 0; i < 4; ++i)
		{
			const float side = Cross(corners[i], corners[(i + 1) % 4], point);
			positive = positive || side > 0;
			negative = negative || side < 0;
		}
		return !(positive && negative);
	}

	bool QuadOverlaps(const Corners& corners, const ScreenRect& rect) noexcept
	{
		const std::array<graphics::Position, 4> box{{{rect.x, rect.y},
		                                             {rect.x + rect.width, rect.y},
		                                             {rect.x + rect.width, rect.y + rect.height},
		                                             {rect.x, rect.y + rect.height}}};
		// Separating axis test: the two box axes and the normals of two edges of the quad.
		const std::array<graphics::Position, 4> axes{{{1, 0},
		                                              {0, 1},
		                                              {-(corners[1].y - corners[0].y), corners[1].x - corners[0].x},
		                                              {-(corners[2].y - corners[1].y), corners[2].x - corners[1].x}}};
		for (const graphics::Position axis : axes)
		{
			if (axis.x == 0.0F && axis.y == 0.0F) continue;
			float quadLow = std::numeric_limits<float>::max();
			float quadHigh = std::numeric_limits<float>::lowest();
			float boxLow = quadLow;
			float boxHigh = quadHigh;
			for (std::size_t i = 0; i < 4; ++i)
			{
				const float onQuad = corners[i].x * axis.x + corners[i].y * axis.y;
				const float onBox = box[i].x * axis.x + box[i].y * axis.y;
				quadLow = std::min(quadLow, onQuad);
				quadHigh = std::max(quadHigh, onQuad);
				boxLow = std::min(boxLow, onBox);
				boxHigh = std::max(boxHigh, onBox);
			}
			if (quadHigh <= boxLow || boxHigh <= quadLow) return false;
		}
		return true;
	}

	ecs::Entity PickEntity(const ecs::Registry& registry, const graphics::Camera& camera, graphics::Position point)
	{
		ecs::Entity best = ecs::Entity::Null;
		int bestLayer = -1;
		for (const ecs::Entity entity : registry.AllEntities())
		{
			const std::optional<ScreenRect> rect = EntityScreenRect(registry, entity, camera);
			if (!rect) continue;
			if (IsTurned(registry, entity))
			{
				if (!QuadContains(*EntityScreenCorners(registry, entity, camera), point)) continue;
			}
			else if (!rect->Contains(point))
			{
				continue;
			}
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
			if (!rect) continue;
			const bool overlaps = IsTurned(registry, entity)
			                          ? QuadOverlaps(*EntityScreenCorners(registry, entity, camera), box)
			                          : rect->Overlaps(box);
			if (overlaps) hits.push_back({entity, ExtentOf(registry, entity)->layer});
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

	std::optional<float> MovableRotation(const ecs::Registry& registry, ecs::Entity entity)
	{
		if (!registry.IsAlive(entity)) return std::nullopt;
		if (registry.HasComponent<ecs::Parent>(entity))
		{
			if (const auto* local = registry.TryGetComponent<graphics::LocalTransform>(entity)) return local->rotation;
		}
		if (const auto* transform = registry.TryGetComponent<graphics::Transform>(entity)) return transform->rotation;
		return std::nullopt;
	}

	std::optional<graphics::Scale> MovableScale(const ecs::Registry& registry, ecs::Entity entity)
	{
		if (!registry.IsAlive(entity)) return std::nullopt;
		if (registry.HasComponent<ecs::Parent>(entity))
		{
			if (const auto* local = registry.TryGetComponent<graphics::LocalTransform>(entity)) return local->scale;
		}
		if (const auto* transform = registry.TryGetComponent<graphics::Transform>(entity)) return transform->scale;
		return std::nullopt;
	}

	bool SetMovableRotation(ecs::Registry& registry, ecs::Entity entity, float degrees)
	{
		if (!registry.IsAlive(entity)) return false;
		if (registry.HasComponent<ecs::Parent>(entity))
		{
			if (auto* local = registry.TryGetComponent<graphics::LocalTransform>(entity))
			{
				local->rotation = degrees;
				return true;
			}
		}
		if (auto* transform = registry.TryGetComponent<graphics::Transform>(entity))
		{
			transform->rotation = degrees;
			return true;
		}
		return false;
	}

	bool SetMovableScale(ecs::Registry& registry, ecs::Entity entity, graphics::Scale scale)
	{
		if (!registry.IsAlive(entity)) return false;
		if (registry.HasComponent<ecs::Parent>(entity))
		{
			if (auto* local = registry.TryGetComponent<graphics::LocalTransform>(entity))
			{
				local->scale = scale;
				return true;
			}
		}
		if (auto* transform = registry.TryGetComponent<graphics::Transform>(entity))
		{
			transform->scale = scale;
			return true;
		}
		return false;
	}

	graphics::Position LocalDelta(const ecs::Registry& registry, ecs::Entity entity, graphics::Position delta)
	{
		if (!registry.IsAlive(entity) || !registry.HasComponent<ecs::Parent>(entity) ||
		    !registry.HasComponent<graphics::LocalTransform>(entity))
			return delta;
		const auto* parent = registry.TryGetComponent<ecs::Parent>(entity);
		const auto* origin = registry.TryGetComponent<graphics::Transform>(parent->entity);
		if (origin == nullptr) return delta;
		if (origin->rotation == 0.0F && origin->scale.x == 1.0F && origin->scale.y == 1.0F) return delta;
		// The world movement is turn(scale(local)), so the local one is the other way round: turn back, then
		// un-stretch.
		const float radians = origin->rotation * kDegreesToRadians;
		const float c = std::cos(radians);
		const float s = std::sin(radians);
		const float x = delta.x * c + delta.y * s;
		const float y = -delta.x * s + delta.y * c;
		return {origin->scale.x != 0.0F ? x / origin->scale.x : 0.0F,
		        origin->scale.y != 0.0F ? y / origin->scale.y : 0.0F};
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

	GizmoHandle PickGizmo(graphics::Position pivot, graphics::Position point, const GizmoShape& shape,
	                      float rotation) noexcept
	{
		float dx = point.x - pivot.x;
		float dy = point.y - pivot.y;
		if (rotation != 0.0F)
		{
			// Into the gizmo's own axes: turn back by its rotation.
			const float radians = rotation * kDegreesToRadians;
			const float c = std::cos(radians);
			const float s = std::sin(radians);
			const float x = dx * c + dy * s;
			dy = -dx * s + dy * c;
			dx = x;
		}
		if (std::abs(dx) <= shape.centre && std::abs(dy) <= shape.centre) return GizmoHandle::Free;
		if (std::abs(dy) <= shape.grab && dx > 0 && dx <= shape.arrowLength) return GizmoHandle::X;
		if (std::abs(dx) <= shape.grab && dy > 0 && dy <= shape.arrowLength) return GizmoHandle::Y;
		return GizmoHandle::None;
	}

	GizmoHandle PickRotateGizmo(graphics::Position pivot, graphics::Position point, const GizmoShape& shape) noexcept
	{
		const float distance = std::hypot(point.x - pivot.x, point.y - pivot.y);
		return std::abs(distance - shape.ringRadius) <= shape.grab ? GizmoHandle::Ring : GizmoHandle::None;
	}

	float AngleBetween(graphics::Position pivot, graphics::Position from, graphics::Position to) noexcept
	{
		const float before = std::atan2(from.y - pivot.y, from.x - pivot.x);
		const float after = std::atan2(to.y - pivot.y, to.x - pivot.x);
		float degrees = (after - before) / kDegreesToRadians;
		while (degrees > 180.0F)
			degrees -= 360.0F;
		while (degrees <= -180.0F)
			degrees += 360.0F;
		return degrees;
	}

	float TurnedRotation(float start, float turned, float step) noexcept
	{
		float degrees = start + turned;
		if (step > 0) degrees = std::round(degrees / step) * step;
		degrees = std::fmod(degrees, 360.0F);
		if (degrees > 180.0F) degrees -= 360.0F;
		if (degrees <= -180.0F) degrees += 360.0F;
		return degrees;
	}

	float ScaleFactor(GizmoHandle handle, float rotation, graphics::Position moved, const GizmoShape& shape) noexcept
	{
		const float radians = rotation * kDegreesToRadians;
		const float c = std::cos(radians);
		const float s = std::sin(radians);
		float along = 0;
		if (handle == GizmoHandle::X)
			along = moved.x * c + moved.y * s;
		else if (handle == GizmoHandle::Y)
			along = -moved.x * s + moved.y * c;
		else if (handle == GizmoHandle::Free)
			along = (moved.x - moved.y) * 0.5F;
		else
			return 1.0F;
		return 1.0F + along / shape.arrowLength;
	}

	float ScaledValue(float start, float factor, float step) noexcept
	{
		float value = start * factor;
		if (step > 0) value = std::round(value / step) * step;
		constexpr float kSmallest = 0.01F;
		if (std::abs(value) < kSmallest) value = value < 0 ? -kSmallest : kSmallest;
		return value;
	}

	graphics::Position GizmoMove(GizmoHandle handle, graphics::Position moved) noexcept
	{
		if (handle == GizmoHandle::X) return {moved.x, 0};
		if (handle == GizmoHandle::Y) return {0, moved.y};
		if (handle == GizmoHandle::Free) return moved;
		return {};
	}

	std::optional<graphics::Position> SelectionCentre(const ecs::Registry& registry,
	                                                  const std::vector<ecs::Entity>& entities,
	                                                  const graphics::Camera& camera)
	{
		float sumX = 0;
		float sumY = 0;
		int count = 0;
		for (const ecs::Entity entity : entities)
		{
			const auto rect = EntityScreenRect(registry, entity, camera);
			if (!rect) continue;
			sumX += rect->x + rect->width * 0.5F;
			sumY += rect->y + rect->height * 0.5F;
			++count;
		}
		if (count == 0) return std::nullopt;
		const float n = static_cast<float>(count);
		return graphics::Position{sumX / n, sumY / n};
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
				const graphics::Position by = LocalDelta(registry, entity, move);
				local->position.x += by.x;
				local->position.y += by.y;
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
