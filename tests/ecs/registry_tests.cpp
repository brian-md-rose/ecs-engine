#include <catch2/catch_test_macros.hpp>

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
