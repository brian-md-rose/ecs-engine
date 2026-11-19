#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/clip.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/transform.hpp"
#include "input/input.hpp"
#include "ui/ui.hpp"
#include "ui/uisystems.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::FrameContext;
	using engine::ecs::World;
	using engine::graphics::Position;
	using engine::graphics::Transform;
	using engine::input::Input;
	using engine::input::MouseButton;
	using engine::ui::Clicked;
	using engine::ui::Hitbox;
	using engine::ui::Hovered;
	using engine::ui::Interactable;
	using engine::ui::PointerState;
	using engine::ui::Pressed;

	struct ClickLog
	{
		std::vector<Clicked> clicks;
	};

	void Record(World& world, const Clicked& clicked)
	{
		world.resources.Get<ClickLog>().clicks.push_back(clicked);
	}

	// A world with the pointer state and a click log, and an input to drive it.
	struct Fixture
	{
		World world;
		Input input;

		Fixture()
		{
			world.resources.Emplace<PointerState>();
			world.resources.Emplace<ClickLog>();
			world.events.Subscribe<Clicked>(Record);
		}

		// One update: the pointer system, then the events it published, then the input's edges clear.
		void Update()
		{
			FrameContext context;
			context.input = &input;
			engine::ui::UiPointerSystem(world, context);
			world.events.Deliver(world);
			input.EndUpdate();
		}

		void Move(float x, float y, bool inside = true) { input.mouse.OnMove({x, y}, inside); }

		Entity Button(Position at, float width, float height, std::uint8_t layer = 0, bool interactable = true)
		{
			const Entity entity = world.registry.Create();
			world.registry.Add<Transform>(entity, at);
			world.registry.Add<Hitbox>(entity, Hitbox{.width = width, .height = height, .layer = layer});
			if (interactable) world.registry.Add<Interactable>(entity);
			return entity;
		}

		[[nodiscard]] const std::vector<Clicked>& Clicks() { return world.resources.Get<ClickLog>().clicks; }
		[[nodiscard]] PointerState& Pointer() { return world.resources.Get<PointerState>(); }
	};
} // namespace

TEST_CASE("The pointer hovers the button under it and leaves it again", "[ui]")
{
	Fixture f;
	const Entity button = f.Button({10.0f, 10.0f}, 20.0f, 10.0f);

	f.Move(5.0f, 5.0f);
	f.Update();
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(button));
	CHECK_FALSE(f.Pointer().overUi);

	f.Move(15.0f, 12.0f);
	f.Update();
	CHECK(f.world.registry.HasComponent<Hovered>(button));
	CHECK(f.Pointer().overUi);
	CHECK(f.Pointer().hovered == button);

	f.Move(30.0f, 12.0f); // the right edge is outside
	f.Update();
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(button));
}

TEST_CASE("A pointer outside the window hits nothing", "[ui]")
{
	Fixture f;
	const Entity button = f.Button({0.0f, 0.0f}, 100.0f, 100.0f);
	f.Move(10.0f, 10.0f, false);
	f.Update();
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(button));
	CHECK_FALSE(f.Pointer().overUi);
}

TEST_CASE("Pressing and releasing on a button is a click", "[ui]")
{
	Fixture f;
	const Entity button = f.Button({10.0f, 10.0f}, 20.0f, 10.0f);
	f.Move(15.0f, 12.0f);
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.Update();
	CHECK(f.world.registry.HasComponent<Pressed>(button));
	CHECK(f.Clicks().empty());
	CHECK(f.Pointer().clickConsumed);

	f.Update(); // held: still pressed, nothing consumed
	CHECK(f.world.registry.HasComponent<Pressed>(button));
	CHECK_FALSE(f.Pointer().clickConsumed);

	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK_FALSE(f.world.registry.HasComponent<Pressed>(button));
	REQUIRE(f.Clicks().size() == 1);
	CHECK(f.Clicks()[0].entity == button);
	CHECK(f.Clicks()[0].button == MouseButton::Left);
	CHECK(f.Pointer().clickConsumed);
}

TEST_CASE("A press and release in one update still clicks", "[ui]")
{
	Fixture f;
	const Entity button = f.Button({0.0f, 0.0f}, 10.0f, 10.0f);
	f.Move(5.0f, 5.0f);
	f.input.mouse.OnButton(MouseButton::Right, true);
	f.input.mouse.OnButton(MouseButton::Right, false);
	f.Update();
	REQUIRE(f.Clicks().size() == 1);
	CHECK(f.Clicks()[0].entity == button);
	CHECK(f.Clicks()[0].button == MouseButton::Right);
	CHECK_FALSE(f.world.registry.HasComponent<Pressed>(button));
}

TEST_CASE("Pressing on a button and releasing elsewhere is not a click", "[ui]")
{
	Fixture f;
	const Entity first = f.Button({0.0f, 0.0f}, 10.0f, 10.0f);
	const Entity second = f.Button({20.0f, 0.0f}, 10.0f, 10.0f);
	f.Move(5.0f, 5.0f);
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.Update();

	f.Move(25.0f, 5.0f); // drag onto the other button
	f.Update();
	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK(f.Clicks().empty());
	CHECK_FALSE(f.world.registry.HasComponent<Pressed>(first));
	CHECK_FALSE(f.world.registry.HasComponent<Pressed>(second));

	// Releasing off every button is no click either.
	f.Move(5.0f, 5.0f);
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.Update();
	f.Move(50.0f, 50.0f);
	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK(f.Clicks().empty());
}

TEST_CASE("Pressing off every button and releasing on one is not a click", "[ui]")
{
	Fixture f;
	f.Button({0.0f, 0.0f}, 10.0f, 10.0f);
	f.Move(50.0f, 50.0f);
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.Update();
	CHECK_FALSE(f.Pointer().clickConsumed); // the world gets this press

	f.Move(5.0f, 5.0f);
	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK(f.Clicks().empty());
	CHECK_FALSE(f.Pointer().clickConsumed);
}

TEST_CASE("Of overlapping hitboxes the higher layer wins, then the later one", "[ui]")
{
	Fixture f;
	const Entity low = f.Button({0.0f, 0.0f}, 20.0f, 20.0f, 1);
	const Entity high = f.Button({0.0f, 0.0f}, 20.0f, 20.0f, 5);
	const Entity highLater = f.Button({0.0f, 0.0f}, 20.0f, 20.0f, 5);
	f.Move(10.0f, 10.0f);
	f.Update();
	CHECK(f.Pointer().hovered == highLater);
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(low));
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(high));
}

TEST_CASE("A blocking panel hides the buttons under it, a non-blocking one doesn't", "[ui]")
{
	Fixture f;
	const Entity button = f.Button({0.0f, 0.0f}, 20.0f, 20.0f, 1);
	const Entity panel = f.Button({0.0f, 0.0f}, 20.0f, 20.0f, 2, false); // a hitbox that is no button
	f.Move(10.0f, 10.0f);
	f.Update();
	CHECK(f.Pointer().overUi);
	CHECK(f.Pointer().hovered == Entity::Null);
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(button));

	f.world.registry.GetComponent<Hitbox>(panel).blocksBelow = false; // now the pointer passes through
	f.Update();
	CHECK(f.Pointer().hovered == button);
}

TEST_CASE("A disabled button blocks but gets no state or clicks", "[ui]")
{
	Fixture f;
	const Entity below = f.Button({0.0f, 0.0f}, 20.0f, 20.0f, 1);
	const Entity disabled = f.Button({0.0f, 0.0f}, 20.0f, 20.0f, 2);
	f.world.registry.GetComponent<Interactable>(disabled).enabled = false;

	f.Move(10.0f, 10.0f);
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK(f.Pointer().overUi);
	CHECK(f.Pointer().clickConsumed);
	CHECK(f.Clicks().empty());
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(disabled));
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(below));
}

TEST_CASE("Hitboxes outside their clip don't count", "[ui]")
{
	Fixture f;
	const Entity row = f.Button({0.0f, 40.0f}, 100.0f, 10.0f); // a row scrolled below the list's view
	f.world.registry.Add<engine::graphics::ClipRect>(
	    row, engine::graphics::ClipRect{.rect = {.x = 0, .y = 0, .width = 100, .height = 30}});
	f.Move(10.0f, 45.0f);
	f.Update();
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(row));
	CHECK_FALSE(f.Pointer().overUi);

	f.world.registry.GetComponent<engine::graphics::ClipRect>(row).rect.height = 60;
	f.Update();
	CHECK(f.world.registry.HasComponent<Hovered>(row));
}

TEST_CASE("A clip on a parent applies to its children's hitboxes", "[ui]")
{
	Fixture f;
	const Entity list = f.world.registry.Create();
	f.world.registry.Add<Transform>(list, Position{});
	f.world.registry.Add<engine::graphics::ClipRect>(
	    list, engine::graphics::ClipRect{.rect = {.x = 0, .y = 0, .width = 50, .height = 50}});
	const Entity child = f.Button({0.0f, 0.0f}, 200.0f, 200.0f);
	f.world.registry.Add<engine::ecs::Parent>(child, engine::ecs::Parent{.entity = list});

	f.Move(100.0f, 10.0f);
	f.Update();
	CHECK_FALSE(f.world.registry.HasComponent<Hovered>(child));
	f.Move(10.0f, 10.0f);
	f.Update();
	CHECK(f.world.registry.HasComponent<Hovered>(child));
}

TEST_CASE("World hitboxes are hit where the camera draws them, screen-space ones where they are", "[ui]")
{
	Fixture f;
	engine::graphics::Camera camera;
	camera.position = {100.0f, 0.0f};
	camera.zoom = 2;
	f.world.resources.Emplace<engine::ui::UiCamera>(engine::ui::UiCamera{.camera = camera});

	// A world button at world (110, 10), 10 x 10: on screen at (20, 20), 20 x 20.
	const Entity worldButton = f.Button({110.0f, 10.0f}, 10.0f, 10.0f);
	// A screen-space button at the same screen place.
	const Entity hudButton = f.Button({60.0f, 0.0f}, 10.0f, 10.0f);
	f.world.registry.Add<engine::graphics::ScreenSpace>(hudButton);

	f.Move(30.0f, 30.0f);
	f.Update();
	CHECK(f.Pointer().hovered == worldButton);

	f.Move(65.0f, 5.0f);
	f.Update();
	CHECK(f.Pointer().hovered == hudButton);

	f.Move(112.0f, 12.0f); // where the world button would be if the camera were ignored
	f.Update();
	CHECK(f.Pointer().hovered == Entity::Null);
}

TEST_CASE("clickConsumed is set for a click that starts or ends over the UI, not one on the world", "[ui]")
{
	Fixture f;
	f.Button({0.0f, 0.0f}, 10.0f, 10.0f);

	f.Move(50.0f, 50.0f);
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK_FALSE(f.Pointer().clickConsumed); // an ordinary world click

	f.Move(5.0f, 5.0f);
	f.input.mouse.OnButton(MouseButton::Left, true);
	f.Update();
	CHECK(f.Pointer().clickConsumed);
	f.Move(50.0f, 50.0f); // dragged off before letting go
	f.input.mouse.OnButton(MouseButton::Left, false);
	f.Update();
	CHECK(f.Pointer().clickConsumed); // that press began on the UI, so the release isn't the world's
}

TEST_CASE("The pointer system does nothing without its resource or an input", "[ui]")
{
	World world;
	FrameContext context;
	engine::ui::UiPointerSystem(world, context);

	world.resources.Emplace<PointerState>();
	engine::ui::UiPointerSystem(world, context); // no input
	CHECK_FALSE(world.resources.Get<PointerState>().overUi);
}
