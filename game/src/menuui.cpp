#include "menuui.hpp"

#include <vector>

#include "assets.hpp"
#include "ecs/registry.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "screens.hpp"
#include "ui/ninepatch.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::World;

		[[nodiscard]] std::size_t SelectedItem(const MenuCursor& cursor, MenuId menu) noexcept
		{
			switch (menu)
			{
			case MenuId::Title:
				return cursor.title;
			case MenuId::Options:
				return cursor.options;
			case MenuId::Pause:
				return cursor.pause;
			case MenuId::Help:
				return 0; // its one button, Back
			}
			return 0;
		}

		void Select(MenuCursor& cursor, MenuId menu, std::size_t item) noexcept
		{
			switch (menu)
			{
			case MenuId::Title:
				cursor.title = item;
				break;
			case MenuId::Options:
				cursor.options = item;
				break;
			case MenuId::Pause:
				cursor.pause = item;
				break;
			case MenuId::Help:
				break;
			}
		}
	} // namespace

	std::optional<std::size_t> TakeMenuClick(World& world, MenuId menu)
	{
		MenuPointer& pointer = world.resources.Get<MenuPointer>();
		if (!pointer.clicked || pointer.clicked->menu != menu) return std::nullopt;
		const std::size_t item = pointer.clicked->item;
		pointer.clicked.reset();
		return item;
	}

	void OnMenuClick(World& world, const engine::ui::Clicked& event)
	{
		if (event.button != engine::input::MouseButton::Left) return;
		if (!world.registry.IsAlive(event.entity)) return;
		const MenuButton* button = world.registry.TryGetComponent<MenuButton>(event.entity);
		if (button == nullptr) return;
		world.resources.Get<MenuPointer>().clicked = *button;
	}

	bool InMenus(const World& world)
	{
		return engine::ecs::InState<Screen::Title, Screen::Options, Screen::Help, Screen::UiDemo>(world) ||
		       engine::ecs::InState<Pause::Paused>(world);
	}

	void MenuHoverSystem(World& world, const engine::ecs::FrameContext&)
	{
		MenuPointer& pointer = world.resources.Get<MenuPointer>();
		MenuCursor& cursor = world.resources.Get<MenuCursor>();
		pointer.offered = pointer.clicked.has_value();
		Entity hovered = Entity::Null;
		world.registry.Each<MenuButton, engine::ui::Hovered>(
		    [&](Entity entity, const MenuButton& button, const engine::ui::Hovered&)
		{
			hovered = entity;
			if (entity != pointer.lastHovered) Select(cursor, button.menu, button.item);
		});
		pointer.lastHovered = hovered;
	}

	void MenuLookSystem(World& world, const engine::ecs::FrameContext&)
	{
		MenuPointer& pointer = world.resources.Get<MenuPointer>();
		if (pointer.offered) pointer.clicked.reset();
		pointer.offered = false;

		const MenuCursor& cursor = world.resources.Get<MenuCursor>();
		const SpriteClips& clips = world.resources.Get<SpriteClips>();
		engine::ecs::Registry& registry = world.registry;
		std::vector<Entity> focus;
		std::vector<Entity> unfocus;
		registry.Each<MenuButton, engine::ui::NinePatch>(
		    [&](Entity entity, const MenuButton& button, engine::ui::NinePatch& patch)
		{
			const bool selected = button.item == SelectedItem(cursor, button.menu);
			const bool pressed = registry.TryGetComponent<engine::ui::Pressed>(entity) != nullptr;
			const bool hovered = registry.TryGetComponent<engine::ui::Hovered>(entity) != nullptr;
			const engine::graphics::Clip& clip =
			    pressed ? clips.buttonPressed : (selected || hovered ? clips.buttonHover : clips.button);
			patch.source = clip.firstFrame;

			const bool focused = registry.TryGetComponent<engine::ui::Focused>(entity) != nullptr;
			if (selected && !focused) focus.push_back(entity);
			if (!selected && focused) unfocus.push_back(entity);
		});
		for (const Entity entity : unfocus)
			registry.Remove<engine::ui::Focused>(entity);
		for (const Entity entity : focus)
			registry.Add<engine::ui::Focused>(entity);
	}
} // namespace game
