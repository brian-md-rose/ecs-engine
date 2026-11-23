#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Parent;
	using engine::ecs::Registry;
	using engine::graphics::LocalTransform;
	using engine::graphics::Position;
	using engine::graphics::Transform;

	Entity Spawn(Registry& registry, Position at)
	{
		const Entity entity = registry.Create();
		registry.Add<Transform>(entity, at);
		return entity;
	}

	Entity SpawnChild(Registry& registry, Entity parent, Position offset)
	{
		const Entity child = Spawn(registry, {});
		registry.Add<Parent>(child, Parent{parent});
		registry.Add<LocalTransform>(child, offset);
		return child;
	}

	Position At(Registry& registry, Entity entity)
	{
		return registry.GetComponent<Transform>(entity).position;
	}
} // namespace

TEST_CASE("UpdateWorldTransforms places children relative to their parents", "[graphics][transform]")
{
	Registry registry;
	const Entity panel = Spawn(registry, {100.0f, 50.0f});
	const Entity label = SpawnChild(registry, panel, {4.0f, 2.0f});

	engine::graphics::UpdateWorldTransforms(registry);
	CHECK(At(registry, label).x == 104.0f);
	CHECK(At(registry, label).y == 52.0f);

	SECTION("moving the parent moves the child")
	{
		registry.GetComponent<Transform>(panel).position = {10.0f, 20.0f};
		engine::graphics::UpdateWorldTransforms(registry);
		CHECK(At(registry, label).x == 14.0f);
		CHECK(At(registry, label).y == 22.0f);
	}

	SECTION("grandchildren add up every offset, whatever order they were created in")
	{
		// The icon is linked to its parent after the shadow is linked to the icon, so the shadow is
		// visited first.
		const Entity icon = registry.Create();
		const Entity shadow = SpawnChild(registry, icon, {1.0f, 1.0f});
		registry.Add<Transform>(icon);
		registry.Add<Parent>(icon, Parent{label});
		registry.Add<LocalTransform>(icon, Position{8.0f, 0.0f});

		engine::graphics::UpdateWorldTransforms(registry);
		CHECK(At(registry, icon).x == 112.0f);
		CHECK(At(registry, shadow).x == 113.0f);
		CHECK(At(registry, shadow).y == 53.0f);
	}

	SECTION("a child whose parent is gone stays where it is")
	{
		registry.Destroy(panel);
		engine::graphics::UpdateWorldTransforms(registry);
		CHECK(At(registry, label).x == 104.0f);
		CHECK(At(registry, label).y == 52.0f);
	}

	SECTION("an entity without a LocalTransform isn't moved")
	{
		const Entity loose = Spawn(registry, {7.0f, 7.0f});
		registry.Add<Parent>(loose, Parent{panel});
		engine::graphics::UpdateWorldTransforms(registry);
		CHECK(At(registry, loose).x == 7.0f);
	}
}

TEST_CASE("A Transform defaults to no rotation and a scale of one", "[graphics][transform]")
{
	const Transform transform;
	CHECK(transform.rotation == 0.0f);
	CHECK(transform.scale.x == 1.0f);
	CHECK(transform.scale.y == 1.0f);
	const LocalTransform local;
	CHECK(local.rotation == 0.0f);
	CHECK(local.scale.x == 1.0f);
	CHECK(local.scale.y == 1.0f);
}

TEST_CASE("TransformPoint stretches, then turns clockwise, then moves", "[graphics][transform]")
{
	using engine::graphics::Scale;
	using engine::graphics::TransformPoint;

	const Position unchanged = TransformPoint({10.0f, 20.0f}, 0.0f, {}, {3.0f, 4.0f});
	CHECK(unchanged.x == 13.0f);
	CHECK(unchanged.y == 24.0f);

	const Position stretched = TransformPoint({0.0f, 0.0f}, 0.0f, Scale{2.0f, 3.0f}, {1.0f, 1.0f});
	CHECK(stretched.x == 2.0f);
	CHECK(stretched.y == 3.0f);

	// 90 degrees clockwise on a y-down screen takes right to down.
	const Position turned = TransformPoint({0.0f, 0.0f}, 90.0f, {}, {1.0f, 0.0f});
	CHECK(turned.x == Catch::Approx(0.0f).margin(1e-5));
	CHECK(turned.y == Catch::Approx(1.0f));
}

TEST_CASE("UpdateWorldTransforms carries rotation and scale to children", "[graphics][transform]")
{
	Registry registry;
	const Entity parent = Spawn(registry, {100.0f, 50.0f});
	registry.GetComponent<Transform>(parent).rotation = 90.0f;
	registry.GetComponent<Transform>(parent).scale = {2.0f, 2.0f};
	const Entity child = SpawnChild(registry, parent, {10.0f, 0.0f});
	registry.GetComponent<LocalTransform>(child).rotation = 15.0f;
	registry.GetComponent<LocalTransform>(child).scale = {0.5f, 3.0f};

	engine::graphics::UpdateWorldTransforms(registry);

	const Transform& world = registry.GetComponent<Transform>(child);
	CHECK(world.position.x == Catch::Approx(100.0f).margin(1e-4)); // 10 * 2, turned from right to down
	CHECK(world.position.y == Catch::Approx(70.0f));
	CHECK(world.rotation == Catch::Approx(105.0f));
	CHECK(world.scale.x == Catch::Approx(1.0f));
	CHECK(world.scale.y == Catch::Approx(6.0f));
	// The parent is untouched.
	CHECK(registry.GetComponent<Transform>(parent).rotation == 90.0f);
}
