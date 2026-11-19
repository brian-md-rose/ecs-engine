#pragma once

#include <array>
#include <cstdint>

#include "ecs/entity.hpp"
#include "graphics/camera.hpp"
#include "graphics/transform.hpp"
#include "input/mouse.hpp"

namespace engine::ecs
{
	class Registry;
}

// Mechanisms for building screens: what the pointer is over, what was clicked, which entity has
// focus, a stretchable panel and a scrolling list. No screens or look: the game builds those from
// shapes, sprites and text. The systems are in ui/uisystems.hpp.
namespace engine::ui
{
	// Component: a rectangle the pointer can hit, with its top-left at the entity's Transform and
	// the size below, in the entity's own coordinates: world units normally, screen pixels if the
	// entity (or an ancestor) has graphics::ScreenSpace. Parts outside the entity's graphics::ClipRect
	// don't count. Put one on anything that should stop clicks reaching the world, even a plain panel.
	struct Hitbox
	{
		float width = 0;
		float height = 0;
		// Overlapping hitboxes: the higher layer wins, and on a tie the one later in the registry. Use
		// the layer the thing is drawn on so the topmost drawn thing is the one hit.
		std::uint8_t layer = 0;
		// Whether what is under this hitbox is out of reach of the pointer. Set false for something
		// the pointer passes over, such as a label on top of a button.
		bool blocksBelow = true;
	};

	// Component: this hitbox is a button, which can be hovered, pressed and clicked. Without it a
	// hitbox only blocks. A disabled one still blocks what is below it, but gets no state or clicks.
	struct Interactable
	{
		bool enabled = true;
	};

	// Components (tags) set by the systems: the pointer is over the entity, a mouse button went down
	// on it and is still held, and it has keyboard focus. Style a button by looking for these.
	struct Hovered
	{
	};
	struct Pressed
	{
	};
	struct Focused
	{
	};

	// Component: where an entity is in the focus order: Tab goes through them lower first, ties in the registry's
	// order. A pad's d-pad or left stick moves Focused toward the nearest one in that direction (by the middle of
	// its Transform and Hitbox, all in the one space), and South presses an Interactable one, as a click. Needs
	// to be Interactable and enabled, or have no Interactable, to be reached.
	struct FocusOrder
	{
		int order = 0;
	};

	// Event: a mouse button was pressed on entity and released on it too. Subscribe with
	// world.events.Subscribe<Clicked>(handler).
	struct Clicked
	{
		ecs::Entity entity;
		input::MouseButton button = input::MouseButton::Left;
	};

	// Resource: what the pointer is doing this update, for game systems that act on clicks. Emplace
	// one at setup (resources can't be added while systems run).
	struct PointerState
	{
		// The pointer is over a hitbox that blocks, or over an interactable one.
		bool overUi = false;
		// A mouse button went down over the UI this update, or came up after going down over it. A
		// world-click system (placing a tower) should ignore the click when this is set, so one click
		// does not press a button and also act on the world beneath.
		bool clickConsumed = false;
		// The interactable entity the pointer is over, or Null.
		ecs::Entity hovered = ecs::Entity::Null;
		// The entity each mouse button went down on, while it is held, or Null.
		std::array<ecs::Entity, input::kMouseButtonCount> pressed{};
		std::array<bool, input::kMouseButtonCount> pressedOverUi{};

		PointerState() { pressed.fill(ecs::Entity::Null); }
	};

	// Resource (optional): the camera the world is drawn with, so hitboxes in world space are hit where
	// they appear. Keep it equal to Renderer2D::camera. Without it, the world is taken to be drawn
	// unscrolled and unzoomed.
	struct UiCamera
	{
		graphics::Camera camera{};
	};

	// Component: a scrolling area. Its entity has a Hitbox (the view), usually a graphics::ClipRect of the
	// same rectangle, and `content` is a child (with graphics::Parent and LocalTransform) holding the
	// rows. The wheel over the view, or dragging it with the left button, moves the content up and
	// down; ScrollViewSystem keeps the content's LocalTransform y at -offset.
	// It tests only its own Hitbox, so something drawn over it (a modal panel) doesn't stop it scrolling: a
	// game with such panels should skip the scrolling while one is open.
	struct ScrollView
	{
		ecs::Entity content = ecs::Entity::Null;
		float viewHeight = 0;    // how much of the content shows
		float contentHeight = 0; // how tall the content is
		float offset = 0;        // how far down the content is scrolled, 0 to contentHeight - viewHeight
		float wheelStep = 12;    // per wheel notch, in the view's units
		float dragThreshold = 4; // a drag shorter than this doesn't scroll
		float padSpeed = 120;    // units a second with a pad's right stick fully down; only while the view has Focused

		// Drag state, kept by the system.
		bool dragging = false;
		bool dragMoved = false;
		float dragGrabY = 0;
		float dragGrabOffset = 0;
	};

	// The scroll offset limited to the range the content allows: 0 when it all fits.
	[[nodiscard]] float ClampScroll(float offset, float contentHeight, float viewHeight) noexcept;

	// Where the pointer is in the coordinates entity's Transform and Hitbox use: its screen position
	// for a screen-space entity, else the world point under it. A pointer outside the window has none.
	struct PointerPoint
	{
		graphics::Position position{};
		bool inClip = true; // false when the entity is clipped and the pointer is outside the clip
	};
	[[nodiscard]] PointerPoint PointerInSpaceOf(ecs::Registry& registry, ecs::Entity entity, const input::Mouse& mouse,
	                                            const graphics::Camera& camera);

	// Whether the pointer is over hitbox, placed by transform. False if the pointer is outside the
	// window, or outside the entity's clip.
	[[nodiscard]] bool PointerHits(ecs::Registry& registry, ecs::Entity entity, const graphics::Transform& transform,
	                               const Hitbox& hitbox, const input::Mouse& mouse, const graphics::Camera& camera);
} // namespace engine::ui
