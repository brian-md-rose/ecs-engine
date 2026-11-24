#pragma once

#include <array>
#include <optional>
#include <vector>

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
		[[nodiscard]] constexpr bool Overlaps(const ScreenRect& other) const noexcept
		{
			return x < other.x + other.width && other.x < x + width && y < other.y + other.height &&
			       other.y < y + height;
		}
	};

	// Where an entity is drawn, for the things the editor can pick: a Shape, Sprite, UI Hitbox, NinePatch or Text, each
	// at the entity's Transform. World-space entities are put through the camera (position, zoom and offset), and ones
	// with graphics::ScreenSpace, on themselves or an ancestor, are not. Empty if the entity has none of those, no
	// Transform, or no size.
	[[nodiscard]] std::optional<ScreenRect> EntityScreenRect(const ecs::Registry& registry, ecs::Entity entity,
	                                                         const graphics::Camera& camera);

	// The entity drawn at point (logical pixels): the highest layer wins, and on a tie the later one in the
	// registry. Entity::Null if nothing is there.
	[[nodiscard]] ecs::Entity PickEntity(const ecs::Registry& registry, const graphics::Camera& camera,
	                                     graphics::Position point);

	// The outline of an entity as drawn, in logical screen pixels: the corners of its EntityScreenRect (top-left,
	// top-right, bottom-right, bottom-left) after the Transform's rotation and scale, which turn and stretch it about
	// its centre. Only a Shape or Sprite is turned and stretched (that is all the renderer does); anything else gives
	// the plain rectangle's corners. Empty exactly when EntityScreenRect is.
	using Corners = std::array<graphics::Position, 4>;
	[[nodiscard]] std::optional<Corners> EntityScreenCorners(const ecs::Registry& registry, ecs::Entity entity,
	                                                         const graphics::Camera& camera);

	// Whether point is inside the quadrilateral (any winding, so a mirrored scale still counts), and whether it
	// overlaps the rectangle.
	[[nodiscard]] bool QuadContains(const Corners& corners, graphics::Position point) noexcept;
	[[nodiscard]] bool QuadOverlaps(const Corners& corners, const ScreenRect& rect) noexcept;

	// Every entity drawn (as EntityScreenRect gives it) that overlaps box, lowest layer first.
	[[nodiscard]] std::vector<ecs::Entity> EntitiesInBox(const ecs::Registry& registry, const graphics::Camera& camera,
	                                                     const ScreenRect& box);

	// Moves the entity by delta, in logical screen pixels (what the pointer moved over the picture): world-space
	// entities by delta / the camera's zoom world units, screen-space ones by delta. An entity with a Parent and a
	// LocalTransform moves its LocalTransform (its Transform is worked out from it each frame; see LocalDelta for a
	// turned parent), any other its Transform. False if it has neither.
	bool MoveEntity(ecs::Registry& registry, ecs::Entity entity, const graphics::Camera& camera,
	                graphics::Position delta);

	// What the editor moves when it drags or nudges an entity: its LocalTransform if it has a Parent and one, else its
	// Transform. In world units for world-space entities, screen pixels for screen-space ones. Empty if it has neither.
	[[nodiscard]] std::optional<graphics::Position> MovablePosition(const ecs::Registry& registry, ecs::Entity entity);
	bool SetMovablePosition(ecs::Registry& registry, ecs::Entity entity, graphics::Position position);

	// What the editor turns and stretches, from the same place as MovablePosition: the LocalTransform of an entity with
	// a Parent and one, else its Transform. Rotation is in degrees. Empty if it has neither.
	[[nodiscard]] std::optional<float> MovableRotation(const ecs::Registry& registry, ecs::Entity entity);
	[[nodiscard]] std::optional<graphics::Scale> MovableScale(const ecs::Registry& registry, ecs::Entity entity);
	bool SetMovableRotation(ecs::Registry& registry, ecs::Entity entity, float degrees);
	bool SetMovableScale(ecs::Registry& registry, ecs::Entity entity, graphics::Scale scale);

	// A movement of delta (the units of the entity's Transform: world units, or screen pixels when screen-space) as the
	// entity's MovablePosition has to change to make it. The same, unless the entity follows a parent that is turned or
	// stretched: its LocalTransform position is in the parent's axes then, so delta is un-stretched and turned back.
	[[nodiscard]] graphics::Position LocalDelta(const ecs::Registry& registry, ecs::Entity entity,
	                                            graphics::Position delta);

	// How many of MovablePosition's units one logical screen pixel is: 1 / the camera zoom for world-space entities, 1
	// for screen-space ones. A pointer movement in screen pixels is that times this in units.
	[[nodiscard]] float UnitsPerPixel(const ecs::Registry& registry, ecs::Entity entity,
	                                  const graphics::Camera& camera);

	// Where a drag that began at start, and has moved the pointer by moved (in the same units), puts the entity.
	// axisLock keeps only the larger of the two movements; a grid above 0 rounds the result to its multiples.
	[[nodiscard]] graphics::Position DragTarget(graphics::Position start, graphics::Position moved, bool axisLock,
	                                            float grid) noexcept;

	// The gizmos drawn over the selection in the Viewport, one at a time (the tool). Translate: an arrow to the right
	// (X), an arrow down (Y) and a square at the pivot that moves freely. Scale: the same, with a square on the tip of
	// each arrow, the arrows turned to the selected entity's rotation; the square at the pivot scales both ways.
	// Rotate: a ring round the pivot. Sizes are in window pixels, so a gizmo stays the same size at any zoom.
	enum class GizmoTool
	{
		Translate,
		Rotate,
		Scale
	};
	enum class GizmoHandle
	{
		None,
		X,
		Y,
		Free,
		Ring
	};
	struct GizmoShape
	{
		float arrowLength = 56; // from the pivot to the tip of an arrow
		float grab = 6;         // how near an arrow's line the pointer must be to grab it
		float centre = 7;       // half the side of the square at the pivot
		float ringRadius = 48;  // the rotate ring, from the pivot to the middle of its line
	};

	// The handle under point (window pixels) for a translate or scale gizmo whose pivot is at pivot, its arrows turned
	// by rotation degrees (clockwise on screen; 0 for translate). The square wins over the arrows.
	[[nodiscard]] GizmoHandle PickGizmo(graphics::Position pivot, graphics::Position point,
	                                    const GizmoShape& shape = {}, float rotation = 0) noexcept;

	// Ring if point is on the rotate gizmo's ring, else None.
	[[nodiscard]] GizmoHandle PickRotateGizmo(graphics::Position pivot, graphics::Position point,
	                                          const GizmoShape& shape = {}) noexcept;

	// The part of a pointer movement that the handle lets through: X keeps only the horizontal, Y only the vertical,
	// Free all of it, None nothing.
	[[nodiscard]] graphics::Position GizmoMove(GizmoHandle handle, graphics::Position moved) noexcept;

	// How far the pointer has turned about pivot in going from one window point to another, in degrees clockwise on
	// screen, in (-180, 180]. Add these up over a drag so that passing the left of the pivot does not jump.
	[[nodiscard]] float AngleBetween(graphics::Position pivot, graphics::Position from, graphics::Position to) noexcept;

	// The rotation (degrees) of an entity that began at start and has been turned by turned, wrapped to (-180, 180]
	// and, if step is above 0, rounded to a multiple of it.
	[[nodiscard]] float TurnedRotation(float start, float turned, float step) noexcept;

	// What a scale drag multiplies the scale by when the pointer has moved by moved (window pixels) since the press:
	// X and Y by how far it moved along that arrow (turned by rotation degrees), a whole arrow's length being one
	// more; Free by how far it moved right or up, so the other way shrinks. 1 for None and Ring.
	[[nodiscard]] float ScaleFactor(GizmoHandle handle, float rotation, graphics::Position moved,
	                                const GizmoShape& shape = {}) noexcept;

	// start times factor, rounded to a multiple of step if that is above 0, and never closer to 0 than 0.01 (an entity
	// scaled to nothing could no longer be picked); the sign is kept, so dragging past 0 mirrors it.
	[[nodiscard]] float ScaledValue(float start, float factor, float step) noexcept;

	// The middle of what is drawn for entities (the mean of the middles of their EntityScreenRects), in logical
	// screen pixels: where the gizmos sit. Empty if none of them is drawn.
	[[nodiscard]] std::optional<graphics::Position> SelectionCentre(const ecs::Registry& registry,
	                                                                const std::vector<ecs::Entity>& entities,
	                                                                const graphics::Camera& camera);

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

		// If rect (picture pixels) is wholly outside a view of width by height window pixels, pans so its middle is in
		// the middle of the view and returns true. A rectangle that shows, even partly, is left alone (false).
		bool ScrollIntoView(const ScreenRect& rect, float width, float height) noexcept;
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
		bool primed = false; // Feed has seen a frame since the last Reset; buttons already down by then are not presses

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
