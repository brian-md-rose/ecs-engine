#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "ui/ui.hpp"

// The real menus (title, options, pause and how to play) are made of the engine's UI pieces: a
// nine-patch panel behind them, a nine-patch button for each item, and, on the how-to-play
// screen, a scroll view. The pointer hovers and clicks the buttons. The keyboard and pad still
// move a cursor (MenuCursor, in screens.hpp), because Controls already turns the d-pad and the
// south button into the same actions as the keys; the cursor's button is the one that has focus.
// So all three kinds of input end up changing the same cursor and acting on the same items.
namespace game
{
	// Which menu a button belongs to.
	enum class MenuId : std::uint8_t
	{
		Title,
		Options,
		Pause,
		Help
	};

	// Component: a button of a menu, and which item of it.
	struct MenuButton
	{
		MenuId menu = MenuId::Title;
		std::size_t item = 0;
	};

	// Component: a scroll view of the how-to-play screen.
	struct HelpList
	{
	};

	// Resource: what the pointer did to the menus this frame.
	struct MenuPointer
	{
		// The button clicked: delivered after the Input phase it was published in, so the next frame's menu systems
		// are the ones that take it. offered says that frame has started; MenuLookSystem then drops what is left.
		std::optional<MenuButton> clicked;
		bool offered = false;
		engine::ecs::Entity lastHovered = engine::ecs::Entity::Null; // so only a new hover moves the cursor
	};

	// The item of menu that was clicked this frame, if one was; clears it.
	[[nodiscard]] std::optional<std::size_t> TakeMenuClick(engine::ecs::World& world, MenuId menu);

	// Event handler for engine::ui::Clicked: remembers a click on a menu button.
	void OnMenuClick(engine::ecs::World& world, const engine::ui::Clicked& event);

	// Run condition: the pointer UI is live (a menu screen, or the pause menu).
	[[nodiscard]] bool InMenus(const engine::ecs::World& world);

	// Input, after UiPointerSystem and before the menu systems: a button the pointer has just moved onto
	// becomes the menu's selected item, and a click waiting is offered to the menu systems.
	void MenuHoverSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate, before NinePatchSystem: each button looks hovered, pressed or normal (the selected one
	// looks hovered and holds Focused), and a click the menu systems were offered and left is dropped.
	void MenuLookSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
