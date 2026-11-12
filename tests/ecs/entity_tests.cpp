#include <catch2/catch_test_macros.hpp>

#include <unordered_set>

#include "assert_guard.hpp"
#include "ecs/componentpool.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"

namespace
{
	struct Position
	{
		float x = 0;
		float y = 0;
	};

	using engine::ecs::ComponentPool;
	using engine::ecs::Entity;
	using engine::ecs::Registry;
} // namespace

TEST_CASE("A default-constructed Entity is null", "[ecs][entity]")
{
	const Entity e{};

	CHECK(e.IsNull());
	CHECK(e == Entity::Null);
	CHECK_FALSE(Entity{0, 0}.IsNull());
	CHECK(Entity{Entity::kNullId, 7}.IsNull()); // the id alone decides
}

TEST_CASE("Registry treats Entity::Null as never alive", "[ecs][entity][registry]")
{
	Registry registry;
	const Entity first = registry.Create();

	CHECK_FALSE(first.IsNull());
	CHECK_FALSE(registry.IsAlive(Entity::Null));
	CHECK_FALSE(registry.IsAlive(Entity{}));

	// Read-only queries and no-op removals are safe on a null entity.
	registry.Add<Position>(first, 1.0f, 2.0f);
	CHECK_FALSE(registry.HasComponent<Position>(Entity::Null));
	CHECK(registry.TryGetComponent<Position>(Entity::Null) == nullptr);
	CHECK_NOTHROW(registry.Remove<Position>(Entity::Null));
	CHECK_NOTHROW(registry.Destroy(Entity::Null));
	CHECK(registry.IsAlive(first));
}

// ENGINE_VERIFY is on in every configuration, so these run in Release too.
TEST_CASE("Adding a component to a dead or null entity fails", "[ecs][entity][registry]")
{
	using engine::test::AssertFailure;
	const engine::test::AssertsThrow assertsThrow;

	Registry registry;

	SECTION("Registry::Add with Entity::Null")
	{
		CHECK_THROWS_AS(registry.Add<Position>(Entity::Null, 1.0f, 2.0f), AssertFailure);
		CHECK_FALSE(registry.HasComponent<Position>(Entity::Null));
	}

	SECTION("Registry::Add with a destroyed entity")
	{
		const Entity e = registry.Create();
		registry.Destroy(e);
		CHECK_THROWS_AS(registry.Add<Position>(e, 1.0f, 2.0f), AssertFailure);

		// The recycled id must not have picked up an orphaned component.
		const Entity reused = registry.Create();
		CHECK_FALSE(registry.HasComponent<Position>(reused));
	}

	SECTION("ComponentPool::Add with Entity::Null")
	{
		ComponentPool<Position> pool;
		CHECK_THROWS_AS(pool.Add(Entity::Null, 1.0f, 2.0f), AssertFailure);
		CHECK(pool.Entities().empty());
	}
}

TEST_CASE("Entity works as a hash key", "[ecs][entity]")
{
	std::unordered_set<Entity> set;
	set.insert(Entity{1, 0});
	set.insert(Entity{1, 1}); // same id, different generation: a different entity
	set.insert(Entity{0, 1});
	set.insert(Entity{1, 0}); // duplicate

	CHECK(set.size() == 3);
	CHECK(set.contains(Entity{1, 1}));
	CHECK_FALSE(set.contains(Entity::Null));
	CHECK(std::hash<Entity>{}(Entity{1, 0}) != std::hash<Entity>{}(Entity{0, 1}));
}
