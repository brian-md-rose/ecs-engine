#include <catch2/catch_test_macros.hpp>

#include "ecs/hierarchy.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/transform.hpp"
#include "input/input.hpp"
#include "ui/ui.hpp"
#include "ui/uisystems.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::FrameContext;
	using engine::ecs::World;
	using engine::input::Input;
	using engine::input::Key;
	using engine::input::MouseButton;
	using engine::ui::ScrollView;

	struct ScrollFixture
	{
		World world;
		Input input;
		Entity view;
		Entity content;

		// A view at (0, 0), 100 x 50, over content 200 tall.
		ScrollFixture()
		{
			view = world.registry.Create();
			content = world.registry.Create();
			world.registry.Add<engine::graphics::Transform>(view, engine::graphics::Position{});
			world.registry.Add<engine::ui::Hitbox>(view, engine::ui::Hitbox{.width = 100.0f, .height = 50.0f});
			world.registry.Add<ScrollView>(
			    view, ScrollView{.content = content, .viewHeight = 50.0f, .contentHeight = 200.0f});
			world.registry.Add<engine::graphics::LocalTransform>(content, engine::graphics::LocalTransform{});
			input.mouse.OnMove({10.0f, 10.0f}, true);
		}

		void Update()
		{
			FrameContext context;
			context.input = &input;
			engine::ui::ScrollViewSystem(world, context);
			input.EndUpdate();
		}

		[[nodiscard]] ScrollView& View() { return world.registry.GetComponent<ScrollView>(view); }
		[[nodiscard]] float ContentY()
		{
			return world.registry.GetComponent<engine::graphics::LocalTransform>(content).position.y;
		}
	};
} // namespace

TEST_CASE("ClampScroll keeps the offset inside the content", "[ui][scroll]")
{
	CHECK(engine::ui::ClampScroll(-5.0f, 200.0f, 50.0f) == 0.0f);
	CHECK(engine::ui::ClampScroll(40.0f, 200.0f, 50.0f) == 40.0f);
	CHECK(engine::ui::ClampScroll(500.0f, 200.0f, 50.0f) == 150.0f);
	CHECK(engine::ui::ClampScroll(10.0f, 30.0f, 50.0f) == 0.0f); // it all fits
}

TEST_CASE("The wheel over a scroll view moves its content, clamped at both ends", "[ui][scroll]")
{
	ScrollFixture f;
	f.input.mouse.OnWheel(-2.0f); // two notches down
	f.Update();
	CHECK(f.View().offset == 24.0f); // wheelStep 12
	CHECK(f.ContentY() == -24.0f);

	f.input.mouse.OnWheel(-100.0f);
	f.Update();
	CHECK(f.View().offset == 150.0f);
	CHECK(f.ContentY() == -150.0f);

	f.input.mouse.OnWheel(100.0f);
	f.Update();
	CHECK(f.View().offset == 0.0f);
	CHECK(f.ContentY() == 0.0f);
}

TEST_CASE("The wheel does nothing when the pointer is elsewhere", "[ui][scroll]")
{
	ScrollFixture f;
	f.input.mouse.OnMove({500.0f, 500.0f}, true);
	f.input.mouse.OnWheel(-2.0f);
	f.Update();
	CHECK(f.View().offset == 0.0f);
}

TEST_CASE("Shrinking the content pulls the offset back in range", "[ui][scroll]")
{
	ScrollFixture f;
	f.View().offset = 150.0f;
	f.View().contentHeight = 80.0f;
	f.Update();
	CHECK(f.View().offset == 30.0f);
	CHECK(f.ContentY() == -30.0f);
}

TEST_CASE("Dragging scrolls once past the threshold", "[ui][scroll]")
{
	ScrollFixture f;
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.Update();
	CHECK_FALSE(f.View().dragMoved);

	f.input.mouse.OnMove({10.0f, 8.0f}, true); // up 2: under the threshold
	f.Update();
	CHECK(f.View().offset == 0.0f);
	CHECK_FALSE(f.View().dragMoved);

	f.input.mouse.OnMove({10.0f, 0.0f}, true); // 10 up: the content follows, so it scrolls down by 10
	f.Update();
	CHECK(f.View().dragMoved);
	CHECK(f.View().offset == 10.0f);

	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK_FALSE(f.View().dragging);
	f.input.mouse.OnMove({10.0f, 40.0f}, true);
	f.Update();
	CHECK(f.View().offset == 10.0f); // no longer dragging
}

TEST_CASE("Tab walks the focus order and Shift+Tab goes back, wrapping", "[ui][focus]")
{
	World world;
	Input input;
	const auto make = [&](int order)
	{
		const Entity entity = world.registry.Create();
		world.registry.Add<engine::ui::FocusOrder>(entity, engine::ui::FocusOrder{.order = order});
		return entity;
	};
	const Entity third = make(30);
	const Entity first = make(10);
	const Entity second = make(20);
	const Entity disabled = make(15);
	world.registry.Add<engine::ui::Interactable>(disabled, engine::ui::Interactable{.enabled = false});

	FrameContext context;
	context.input = &input;
	const auto press = [&](bool shift)
	{
		if (shift) input.keyboard.OnKeyDown(Key::Shift);
		input.keyboard.OnKeyDown(Key::Tab);
		engine::ui::UiFocusSystem(world, context);
		input.keyboard.OnKeyUp(Key::Tab);
		input.keyboard.OnKeyUp(Key::Shift);
		input.EndUpdate();
	};
	const auto focused = [&]() -> Entity
	{
		Entity found = Entity::Null;
		world.registry.Each<engine::ui::Focused>([&](Entity entity, engine::ui::Focused&)
		{
			found = entity;
		});
		return found;
	};

	engine::ui::UiFocusSystem(world, context); // no Tab pressed
	CHECK(focused() == Entity::Null);

	press(false);
	CHECK(focused() == first);
	press(false);
	CHECK(focused() == second); // the disabled one is skipped
	press(false);
	CHECK(focused() == third);
	press(false);
	CHECK(focused() == first); // wraps
	press(true);
	CHECK(focused() == third); // and back
	press(true);
	CHECK(focused() == second);
}

TEST_CASE("Shift+Tab with nothing focused starts at the end", "[ui][focus]")
{
	World world;
	Input input;
	const Entity a = world.registry.Create();
	const Entity b = world.registry.Create();
	world.registry.Add<engine::ui::FocusOrder>(a, engine::ui::FocusOrder{.order = 1});
	world.registry.Add<engine::ui::FocusOrder>(b, engine::ui::FocusOrder{.order = 2});
	input.keyboard.OnKeyDown(Key::Shift);
	input.keyboard.OnKeyDown(Key::Tab);
	FrameContext context;
	context.input = &input;
	engine::ui::UiFocusSystem(world, context);
	CHECK(world.registry.HasComponent<engine::ui::Focused>(b));
	CHECK_FALSE(world.registry.HasComponent<engine::ui::Focused>(a));
}
