#pragma once

#include <array>
#include <optional>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/camera.hpp"
#include "input/mouse.hpp"

namespace engine::editor
{
	// A rectangle in logical screen pixels, the units the game draws in.
	struct ScreenRect
	{
		float x = 0;
		float y = 0;
		float width = 0;
		float height = 0;

		[[nodiscard]] constexpr bool Contains(graphics::Position point) const noexcept
		{
			return point.x >= x && point.y >= y && point.x < x + width && point.y < y + height;
		}
	};

	// Where an entity is drawn, for the things the editor can pick: a Shape, a Sprite or a UI Hitbox, each at the
	// entity's Transform. World-space entities are put through the camera (position, zoom and offset), and ones
	// with graphics::ScreenSpace, on themselves or an ancestor, are not. Empty if the entity has none of those, no
	// Transform, or no size.
	[[nodiscard]] std::optional<ScreenRect> EntityScreenRect(const ecs::Registry& registry, ecs::Entity entity,
	                                                         const graphics::Camera& camera);

	// The entity drawn at point (logical pixels): the highest layer wins, and on a tie the later one in the
	// registry. Entity::Null if nothing is there.
	[[nodiscard]] ecs::Entity PickEntity(const ecs::Registry& registry, const graphics::Camera& camera,
	                                     graphics::Position point);

	// Moves the entity by delta, in logical screen pixels (what the pointer moved over the picture): world-space
	// entities by delta / the camera's zoom world units, screen-space ones by delta. An entity with a Parent and a
	// LocalTransform moves its LocalTransform (its Transform is worked out from it each frame), any other its
	// Transform. False if it has neither.
	bool MoveEntity(ecs::Registry& registry, ecs::Entity entity, const graphics::Camera& camera,
	                graphics::Position delta);

	// How the picture is shown in the viewport window: scaled by zoom and moved by pan (window pixels).
	struct ViewTransform
	{
		float zoom = 1.0F;
		float panX = 0;
		float panY = 0;

		static constexpr float kMinZoom = 0.25F;
		static constexpr float kMaxZoom = 16.0F;

		// A picture pixel to a window pixel, and back. origin is where the window's content starts.
		[[nodiscard]] graphics::Position ToWindow(graphics::Position picture, graphics::Position origin) const noexcept
		{
			return {origin.x + panX + picture.x * zoom, origin.y + panY + picture.y * zoom};
		}
		[[nodiscard]] graphics::Position ToPicture(graphics::Position window, graphics::Position origin) const noexcept
		{
			return {(window.x - origin.x - panX) / zoom, (window.y - origin.y - panY) / zoom};
		}

		// Zooms by factor, keeping the picture pixel under the window point where it is.
		void ZoomAt(float factor, graphics::Position window, graphics::Position origin) noexcept;
	};
} // namespace engine::editor

namespace engine::editor
{
	// What the pointer did over the viewport this frame, for PlayMouse.
	struct PointerSample
	{
		graphics::Position window{}; // window pixels
		bool hovered = false;        // over the viewport's rectangle
		std::array<bool, input::kMouseButtonCount> down{};
		float wheel = 0; // notches this frame
	};

	// The mouse the game sees while it is played in the viewport. The editor's own window takes the real mouse events,
	// so this is built from the viewport instead: positions are in the picture's pixels (the game's logical pixels),
	// and a press that began over the viewport belongs to the game until it is released, even if the pointer leaves.
	// Feed it every frame, give `mouse` to the game's Input, and call mouse.EndUpdate() after each game update.
	class PlayMouse
	{
		std::array<bool, input::kMouseButtonCount> previous{};
		std::array<bool, input::kMouseButtonCount> owned{}; // pressed over the viewport, not yet released

	public:
		input::Mouse mouse;

		// origin is where the viewport's content starts, in window pixels; width and height are the picture's size.
		void Feed(const PointerSample& sample, const ViewTransform& view, graphics::Position origin, int width,
		          int height);
		// Releases anything held and puts the pointer outside, for when Play starts or stops.
		void Reset();
		[[nodiscard]] bool AnyOwned() const noexcept;
	};
} // namespace engine::editor
