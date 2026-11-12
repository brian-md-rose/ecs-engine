#include <catch2/catch_test_macros.hpp>

#include "assert_guard.hpp"
#include "core/assert.hpp"
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
} // namespace

TEST_CASE("ComponentPool stores and returns components", "[ecs][pool]")
{
	ComponentPool<Health> pool;
	const Entity e{3, 0};

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
	pool.Add(Entity{0, 1}, 5);

	CHECK(pool.HasComponent(Entity{0, 1}));
	CHECK_FALSE(pool.HasComponent(Entity{0, 0}));
}

TEST_CASE("ComponentPool::Remove keeps the array packed", "[ecs][pool]")
{
	ComponentPool<Health> pool;
	const Entity a{0, 0};
	const Entity b{1, 0};
	const Entity c{2, 0};
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
		pool.Remove(Entity{99, 0});

		CHECK(pool.Data().size() == 3);
	}
}

TEST_CASE("ComponentPool::Add does not allow duplicate components", "[ecs][pool]")
{
	ComponentPool<Health> pool;
	const Entity e{3, 0};

	const engine::test::AssertsThrow assertsThrow;

	Health& first = pool.Add(e, 1);

	if constexpr (engine::kAssertsEnabled)
	{
		// Debug: adding a second Health to the same entity is a programming error.
		CHECK_THROWS_AS(pool.Add(e, 2), engine::test::AssertFailure);
	}
	else
	{
		// Release: the assert is compiled out; Add falls back to returning the
		// existing component and ignores the new arguments.
		Health& second = pool.Add(e, 2);
		CHECK(&second == &first);
	}

	// Either way the original component is untouched...
	CHECK(pool.GetComponent(e).value == 1);
	CHECK(&pool.GetComponent(e) == &first);

	// ...and only one component and one entity entry are stored.
	CHECK(pool.Data().size() == 1);
	REQUIRE(pool.Entities().size() == 1);
	CHECK(pool.Entities()[0] == e);

	// Removing once fully removes it; no orphaned duplicate is left behind.
	pool.Remove(e);
	CHECK_FALSE(pool.HasComponent(e));
	CHECK(pool.Data().empty());
}

TEST_CASE("ComponentPool::GetComponent asserts when the component is missing", "[ecs][pool]")
{
	if constexpr (!engine::kAssertsEnabled)
	{
		SKIP("ENGINE_ASSERT is compiled out in this configuration");
	}

	using engine::test::AssertFailure;
	const engine::test::AssertsThrow assertsThrow;

	ComponentPool<Health> pool;
	pool.Add(Entity{1, 0}, 10);
	const ComponentPool<Health>& constPool = pool;

	CHECK_THROWS_AS(pool.GetComponent(Entity{99, 0}), AssertFailure);     // id past the end of sparse
	CHECK_THROWS_AS(pool.GetComponent(Entity{0, 0}), AssertFailure);      // id in range, no component
	CHECK_THROWS_AS(pool.GetComponent(Entity{1, 1}), AssertFailure);      // stale generation
	CHECK_THROWS_AS(constPool.GetComponent(Entity{0, 0}), AssertFailure); // const overload
	CHECK(pool.GetComponent(Entity{1, 0}).value == 10);
}
