#include <catch2/catch_test_macros.hpp>

#include "assert_guard.hpp"
#include "core/assert.hpp"
#include "ecs/commandbuffer.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"

namespace
{
	struct Position
	{
		float x = 0;
		float y = 0;
	};

	struct Velocity
	{
		float x = 0;
		float y = 0;
	};

	using engine::ecs::Entity;
	using engine::ecs::Registry;
} // namespace

TEST_CASE("Registry creates entities with unique ids", "[ecs][registry]")
{
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();

	CHECK(a.id != b.id);
	CHECK(registry.IsAlive(a));
	CHECK(registry.IsAlive(b));
}

TEST_CASE("Registry recycles ids with a new generation", "[ecs][registry]")
{
	Registry registry;
	const Entity first = registry.Create();
	registry.Destroy(first);

	CHECK_FALSE(registry.IsAlive(first));

	const Entity second = registry.Create();

	CHECK(second.id == first.id);
	CHECK(second.generation == first.generation + 1);
	CHECK(registry.IsAlive(second));
	CHECK_FALSE(registry.IsAlive(first));
}

TEST_CASE("Registry::Destroy removes the entity's components", "[ecs][registry]")
{
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Position>(e, 1.0f, 2.0f);

	registry.Destroy(e);
	const Entity reused = registry.Create();

	CHECK_FALSE(registry.HasComponent<Position>(e));
	CHECK_FALSE(registry.HasComponent<Position>(reused));
}

TEST_CASE("Registry::Each visits only entities with every component", "[ecs][registry]")
{
	Registry registry;
	const Entity moving = registry.Create();
	const Entity still = registry.Create();
	registry.Add<Position>(moving, 0.0f, 0.0f);
	registry.Add<Velocity>(moving, 1.0f, 2.0f);
	registry.Add<Position>(still, 5.0f, 5.0f);

	int visited = 0;
	registry.Each<Position, Velocity>([&](Entity, Position& p, const Velocity& v) {
		++visited;
		p.x += v.x;
		p.y += v.y;
	});

	CHECK(visited == 1);
	CHECK(registry.GetComponent<Position>(moving).x == 1.0f);
	CHECK(registry.GetComponent<Position>(moving).y == 2.0f);
	CHECK(registry.GetComponent<Position>(still).x == 5.0f);
}

TEST_CASE("Registry asserts on structural changes during Each", "[ecs][registry]")
{
	if constexpr (!engine::kAssertsEnabled)
	{
		SKIP("ENGINE_ASSERT is compiled out in this configuration");
	}

	using engine::test::AssertFailure;
	const engine::test::AssertsThrow assertsThrow;

	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Position>(e, 1.0f, 2.0f);

	SECTION("Destroy")
	{
		CHECK_THROWS_AS(registry.Each<Position>([&](Entity entity, Position&) { registry.Destroy(entity); }),
		                AssertFailure);
		CHECK(registry.IsAlive(e));
	}

	SECTION("Add")
	{
		CHECK_THROWS_AS(
			registry.Each<Position>([&](Entity entity, Position&) { registry.Add<Velocity>(entity, 0.0f, 0.0f); }),
			AssertFailure);
		CHECK_FALSE(registry.HasComponent<Velocity>(e));
	}

	SECTION("Remove")
	{
		CHECK_THROWS_AS(registry.Each<Position>([&](Entity entity, Position&) { registry.Remove<Position>(entity); }),
		                AssertFailure);
		CHECK(registry.HasComponent<Position>(e));
	}

	SECTION("inside a nested Each")
	{
		registry.Add<Velocity>(e, 0.0f, 0.0f);
		CHECK_THROWS_AS(registry.Each<Position>([&](Entity, Position&) {
			registry.Each<Velocity>([&](Entity, Velocity&) {});
			registry.Destroy(e); // still inside the outer loop
		}),
		                AssertFailure);
	}

	SECTION("CommandBuffer::Apply inside Each")
	{
		engine::ecs::CommandBuffer commands;
		commands.Destroy(e);
		CHECK_THROWS_AS(registry.Each<Position>([&](Entity, Position&) { commands.Apply(registry); }),
		                AssertFailure);
	}

	// Once the throwing Each has unwound, structural changes are allowed again.
	CHECK_NOTHROW(registry.Destroy(e));
	CHECK_FALSE(registry.IsAlive(e));
}

TEST_CASE("Registry allows reads, writes and Create during Each", "[ecs][registry]")
{
	const engine::test::AssertsThrow assertsThrow;

	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Position>(e, 1.0f, 2.0f);

	Entity created{};
	CHECK_NOTHROW(registry.Each<Position>([&](Entity entity, Position& p) {
		p.x += 1.0f;
		CHECK(registry.HasComponent<Position>(entity));
		created = registry.Create();
	}));

	CHECK(registry.GetComponent<Position>(e).x == 2.0f);
	CHECK(registry.IsAlive(created));

	// And structural changes work normally afterwards.
	CHECK_NOTHROW(registry.Add<Velocity>(created, 1.0f, 1.0f));
}

TEST_CASE("Registry::Each with a component type that was never added", "[ecs][registry]")
{
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Position>(e, 1.0f, 2.0f);

	int visited = 0;

	SECTION("as an extra component: visits nothing")
	{
		registry.Each<Position, Velocity>([&](Entity, Position&, Velocity&) { ++visited; });
	}

	SECTION("as the first component: visits nothing")
	{
		registry.Each<Velocity, Position>([&](Entity, Velocity&, Position&) { ++visited; });
	}

	CHECK(visited == 0);

	// Querying didn't create the pool as a side effect.
	CHECK_FALSE(registry.HasComponent<Velocity>(e));
}

TEST_CASE("Registry::TryGetComponent returns nullptr when missing", "[ecs][registry]")
{
	Registry registry;
	const Entity with = registry.Create();
	const Entity without = registry.Create();

	CHECK(registry.TryGetComponent<Position>(with) == nullptr); // type never added

	registry.Add<Position>(with, 3.0f, 4.0f);

	Position* found = registry.TryGetComponent<Position>(with);
	REQUIRE(found != nullptr);
	CHECK(found->x == 3.0f);
	CHECK(found == &registry.GetComponent<Position>(with));

	CHECK(registry.TryGetComponent<Position>(without) == nullptr); // pool exists, entity not in it
}

TEST_CASE("Registry::HasComponent for a type that was never added", "[ecs][registry]")
{
	Registry registry;
	const Entity e = registry.Create();

	CHECK_FALSE(registry.HasComponent<Velocity>(e));
	registry.Remove<Velocity>(e); // removing from a missing pool is a no-op
	CHECK_FALSE(registry.HasComponent<Velocity>(e));
}

TEST_CASE("Registry::GetComponent asserts when the component is missing", "[ecs][registry]")
{
	if constexpr (!engine::kAssertsEnabled)
	{
		SKIP("ENGINE_ASSERT is compiled out in this configuration");
	}

	using engine::test::AssertFailure;
	const engine::test::AssertsThrow assertsThrow;

	Registry registry;
	const Entity with = registry.Create();
	const Entity without = registry.Create();

	CHECK_THROWS_AS(registry.GetComponent<Position>(with), AssertFailure); // type never added

	registry.Add<Position>(with, 0.0f, 0.0f);
	CHECK_THROWS_AS(registry.GetComponent<Position>(without), AssertFailure); // entity not in pool

	const Registry& constRegistry = registry;
	CHECK_THROWS_AS(constRegistry.GetComponent<Position>(without), AssertFailure);
}

TEST_CASE("Registry component access through a const reference", "[ecs][registry]")
{
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Position>(e, 5.0f, 6.0f);

	const Registry& constRegistry = registry;

	CHECK(constRegistry.HasComponent<Position>(e));
	CHECK(constRegistry.GetComponent<Position>(e).y == 6.0f);

	const Position* found = constRegistry.TryGetComponent<Position>(e);
	REQUIRE(found != nullptr);
	CHECK(found->x == 5.0f);

	CHECK(constRegistry.TryGetComponent<Velocity>(e) == nullptr);
}
