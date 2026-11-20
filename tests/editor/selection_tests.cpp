#include <catch2/catch_test_macros.hpp>

#include "ecs/enginecomponents.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/entityops.hpp"
#include "editor/entitypanels.hpp"
#include "editor/viewport.hpp"
#include "graphics/camera.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::editor::PanelState;

	Entity AddBox(Registry& registry, float x, float y, std::uint8_t layer = 0)
	{
		const Entity entity = registry.Create();
		registry.Add<engine::graphics::Transform>(entity, engine::graphics::Transform{{x, y}});
		registry.Add<engine::graphics::Shape>(entity,
		                                      engine::graphics::Shape{.width = 10, .height = 10, .layer = layer});
		return entity;
	}
} // namespace

TEST_CASE("Toggle adds to the selection and makes the newest the primary", "[editor][selection]")
{
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	const Entity c = registry.Create();
	PanelState state;

	state.Select(a);
	state.Toggle(b);
	state.Toggle(c);
	CHECK(state.selected == c);
	CHECK(state.All().size() == 3);
	CHECK(state.IsSelected(a));

	state.Toggle(c); // taking the primary out hands it to the next
	CHECK(state.selected == b);
	CHECK(state.All().size() == 2);
	CHECK_FALSE(state.IsSelected(c));

	state.Select(a);
	CHECK(state.extra.empty());
	state.Select(Entity::Null);
	CHECK(state.All().empty());
}

TEST_CASE("Prune drops dead entities from the selection", "[editor][selection]")
{
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	PanelState state;
	state.Select(a);
	state.Toggle(b);
	registry.Destroy(b); // the primary
	state.Prune(registry);
	CHECK(state.selected == a);
	CHECK(state.extra.empty());
}

TEST_CASE("Deleting and duplicating act on the whole selection", "[editor][selection]")
{
	engine::ecs::TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	Registry registry;
	const Entity a = AddBox(registry, 0, 0);
	const Entity b = AddBox(registry, 20, 0);
	const Entity child = AddBox(registry, 1, 1);
	registry.Add<engine::ecs::Parent>(child, engine::ecs::Parent{a});
	const Entity other = AddBox(registry, 50, 0);

	PanelState state;
	state.Select(a);
	state.Toggle(child); // a and its own child: the child goes with a's copy, not twice
	state.Toggle(b);
	engine::editor::DuplicateSelection(registry, types, state);
	CHECK(registry.AllEntities().size() == 4 + 3); // a+child copied once, b copied once
	CHECK(state.All().size() == 2);                // the copies of a and b

	state.Select(other);
	state.Toggle(a);
	engine::editor::DeleteSelection(registry, state);
	CHECK_FALSE(registry.IsAlive(other));
	CHECK_FALSE(registry.IsAlive(a));
	CHECK_FALSE(registry.IsAlive(child)); // deleted with a
	CHECK(registry.IsAlive(b));
	CHECK(state.All().empty());
}

TEST_CASE("EntitiesInBox finds what a box overlaps, lowest layer first", "[editor][selection]")
{
	Registry registry;
	engine::graphics::Camera camera;
	camera.logicalWidth = 320;
	camera.logicalHeight = 180;
	const Entity high = AddBox(registry, 5, 5, 3);
	const Entity low = AddBox(registry, 8, 8, 1);
	(void)AddBox(registry, 100, 100);

	const auto found = engine::editor::EntitiesInBox(registry, camera, {0, 0, 20, 20});
	REQUIRE(found.size() == 2);
	CHECK(found[0] == low);
	CHECK(found[1] == high);
	CHECK(engine::editor::EntitiesInBox(registry, camera, {30, 30, 5, 5}).empty());

	CHECK(engine::editor::ScreenRect{0, 0, 10, 10}.Overlaps({9, 9, 5, 5}));
	CHECK_FALSE(engine::editor::ScreenRect{0, 0, 10, 10}.Overlaps({10, 0, 5, 5})); // touching is not overlapping
}
