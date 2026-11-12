#include <catch2/catch_test_macros.hpp>

#include "ecs/componentpool.hpp"
#include "ecs/entity.hpp"

namespace
{
	struct Health
	{
		int value = 0;
	};

	using engine::ecs::ComponentPool;
	using engine::ecs::Entity;
}

TEST_CASE("ComponentPool stores and returns components", "[ecs][pool]")
{
	ComponentPool<Health> pool;
	const Entity e{ 3, 0 };

	REQUIRE_FALSE(pool.HasComponent(e));

	pool.Add(e, 42);

	REQUIRE(pool.HasComponent(e));
	CHECK(pool.GetComponent(e).value == 42);
	CHECK(pool.Data().size() == 1);
	CHECK(pool.Entities().size() == 1);
}

TEST_CASE("ComponentPool rejects a stale generation", "[ecs][pool]")
{
	ComponentPool<Health> pool;
	pool.Add(Entity{ 0, 1 }, 5);

	CHECK(pool.HasComponent(Entity{ 0, 1 }));
	CHECK_FALSE(pool.HasComponent(Entity{ 0, 0 }));
}

TEST_CASE("ComponentPool::Remove keeps the array packed", "[ecs][pool]")
{
	ComponentPool<Health> pool;
	const Entity a{ 0, 0 };
	const Entity b{ 1, 0 };
	const Entity c{ 2, 0 };
	pool.Add(a, 1);
	pool.Add(b, 2);
	pool.Add(c, 3);

	SECTION("removing the last element just pops it")
	{
		pool.Remove(c);

		CHECK_FALSE(pool.HasComponent(c));
		REQUIRE(pool.Data().size() == 2);
		CHECK(pool.GetComponent(a).value == 1);
		CHECK(pool.GetComponent(b).value == 2);
	}

	SECTION("removing from the middle moves the last element into the gap")
	{
		pool.Remove(a);

		CHECK_FALSE(pool.HasComponent(a));
		REQUIRE(pool.Data().size() == 2);
		CHECK(pool.GetComponent(b).value == 2);
		CHECK(pool.GetComponent(c).value == 3);
		CHECK(pool.Entities()[0] == c);
	}

	SECTION("removing a missing entity does nothing")
	{
		pool.Remove(Entity{ 99, 0 });

		CHECK(pool.Data().size() == 3);
	}
}
