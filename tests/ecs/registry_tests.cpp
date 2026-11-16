#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

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
	registry.Each<Position, Velocity>([&](Entity, Position& p, const Velocity& v)
	{
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
		const auto destroy = [&](Entity entity, Position&)
		{
			registry.Destroy(entity);
		};
		CHECK_THROWS_AS(registry.Each<Position>(destroy), AssertFailure);
		CHECK(registry.IsAlive(e));
	}

	SECTION("Add")
	{
		const auto add = [&](Entity entity, Position&)
		{
			registry.Add<Velocity>(entity, 0.0f, 0.0f);
		};
		CHECK_THROWS_AS(registry.Each<Position>(add), AssertFailure);
		CHECK_FALSE(registry.HasComponent<Velocity>(e));
	}

	SECTION("Remove")
	{
		const auto remove = [&](Entity entity, Position&)
		{
			registry.Remove<Position>(entity);
		};
		CHECK_THROWS_AS(registry.Each<Position>(remove), AssertFailure);
		CHECK(registry.HasComponent<Position>(e));
	}

	SECTION("inside a nested Each")
	{
		registry.Add<Velocity>(e, 0.0f, 0.0f);
		const auto nested = [&](Entity, Position&)
		{
			registry.Each<Velocity>([&](Entity, Velocity&) {});
			registry.Destroy(e); // still inside the outer loop
		};
		CHECK_THROWS_AS(registry.Each<Position>(nested), AssertFailure);
	}

	SECTION("CommandBuffer::Apply inside Each")
	{
		engine::ecs::CommandBuffer commands;
		commands.Destroy(e);
		const auto apply = [&](Entity, Position&)
		{
			commands.Apply(registry);
		};
		CHECK_THROWS_AS(registry.Each<Position>(apply), AssertFailure);
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
	CHECK_NOTHROW(registry.Each<Position>([&](Entity entity, Position& p)
	{
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
		registry.Each<Position, Velocity>([&](Entity, Position&, Velocity&)
		{
			++visited;
		});
	}

	SECTION("as the first component: visits nothing")
	{
		registry.Each<Velocity, Position>([&](Entity, Velocity&, Position&)
		{
			++visited;
		});
	}

	CHECK(visited == 0);

	// Querying didn't create the pool as a side effect.
	CHECK(registry.PoolCount() == 1);
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

TEST_CASE("Registry on a destroyed entity", "[ecs][registry]")
{
	const engine::test::AssertsThrow assertsThrow;

	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Position>(e, 1.0f, 2.0f);
	registry.Destroy(e);

	// Remove is a deliberate no-op, not an error.
	CHECK_NOTHROW(registry.Remove<Position>(e));

	CHECK_FALSE(registry.HasComponent<Position>(e));
	CHECK(registry.TryGetComponent<Position>(e) == nullptr);

	// The recycled id is a different entity; the stale handle still can't reach its component.
	const Entity reused = registry.Create();
	registry.Add<Position>(reused, 3.0f, 4.0f);
	CHECK_FALSE(registry.HasComponent<Position>(e));
	CHECK(registry.TryGetComponent<Position>(e) == nullptr);

	if constexpr (engine::kAssertsEnabled)
	{
		CHECK_THROWS_AS(registry.GetComponent<Position>(e), engine::test::AssertFailure);
	}
}

TEST_CASE("Registry::Each loops over the smallest pool", "[ecs][registry]")
{
	// Visiting order follows the driving pool's packed order, which shows which pool drove the loop.
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	const Entity c = registry.Create();

	registry.Add<Position>(a, 1.0f, 0.0f);
	registry.Add<Position>(b, 2.0f, 0.0f);
	registry.Add<Position>(c, 3.0f, 0.0f);
	registry.Add<Velocity>(c, 30.0f, 0.0f); // Velocity is the smaller pool, packed as c, a
	registry.Add<Velocity>(a, 10.0f, 0.0f);

	std::vector<Entity> order;

	SECTION("smaller pool listed second")
	{
		registry.Each<Position, Velocity>([&](Entity e, Position& p, Velocity& v)
		{
			order.push_back(e);
			CHECK(v.x == p.x * 10.0f); // arguments stay in declared order and match the entity
		});
		CHECK(order == std::vector<Entity>{c, a}); // Position's order would be a, c
	}

	SECTION("smaller pool listed first")
	{
		registry.Each<Velocity, Position>([&](Entity e, Velocity& v, Position& p)
		{
			order.push_back(e);
			CHECK(v.x == p.x * 10.0f);
		});
		CHECK(order == std::vector<Entity>{c, a}); // Velocity is First here and the smallest: fast path
	}

	SECTION("callback writes reach the stored components")
	{
		registry.Each<Position, Velocity>([](Entity, Position& p, const Velocity& v)
		{
			p.y = v.x;
		});
		CHECK(registry.GetComponent<Position>(a).y == 10.0f);
		CHECK(registry.GetComponent<Position>(b).y == 0.0f);
		CHECK(registry.GetComponent<Position>(c).y == 30.0f);
	}
}

TEST_CASE("Registry read-only queries don't create pools", "[ecs][registry]")
{
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Position>(e, 1.0f, 2.0f);
	REQUIRE(registry.PoolCount() == 1);

	const Registry& constRegistry = registry;

	CHECK_FALSE(registry.HasComponent<Velocity>(e));
	CHECK(registry.TryGetComponent<Velocity>(e) == nullptr);
	CHECK(constRegistry.TryGetComponent<Velocity>(e) == nullptr);
	registry.Remove<Velocity>(e);
	registry.Each<Velocity>([](Entity, Velocity&) {});
	registry.Each<Position, Velocity>([](Entity, Position&, Velocity&) {});
	registry.Each<Velocity, Position>([](Entity, Velocity&, Position&) {});

	CHECK(registry.PoolCount() == 1);

	registry.Add<Velocity>(e, 0.0f, 0.0f); // Add is what creates a pool
	CHECK(registry.PoolCount() == 2);
}

TEST_CASE("The registry counts live entities and each component's entities", "[ecs][registry]")
{
	Registry registry;
	CHECK(registry.EntityCount() == 0);
	CHECK(registry.ComponentCounts().empty());

	const Entity a = registry.Create();
	const Entity b = registry.Create();
	const Entity c = registry.Create();
	registry.Add<Velocity>(a);
	registry.Add<Position>(a);
	registry.Add<Position>(b);
	registry.Destroy(c);
	CHECK(registry.EntityCount() == 2);

	std::vector<Registry::ComponentCount> counts = registry.ComponentCounts();
	REQUIRE(counts.size() == 2);
	CHECK(counts[0].name == "Position"); // sorted by name
	CHECK(counts[0].count == 2);
	CHECK(counts[1].name == "Velocity");
	CHECK(counts[1].count == 1);

	registry.Destroy(a);
	counts = registry.ComponentCounts();
	CHECK(counts[0].count == 1);
	CHECK(counts[1].count == 0); // still listed once none are left
}

namespace
{
	struct Frozen
	{
	};

	struct Hidden
	{
	};
} // namespace

TEST_CASE("Registry::Each with Without leaves out entities that have the excluded components", "[ecs][registry]")
{
	using engine::ecs::Without;

	Registry registry;
	const Entity moving = registry.Create();
	const Entity frozen = registry.Create();
	const Entity hidden = registry.Create();
	for (const Entity e : {moving, frozen, hidden})
	{
		registry.Add<Position>(e);
		registry.Add<Velocity>(e, 1.0f, 0.0f);
	}
	registry.Add<Frozen>(frozen);
	registry.Add<Hidden>(hidden);

	std::vector<Entity> visited;
	const auto record = [&](Entity e, Position&, Velocity&)
	{
		visited.push_back(e);
	};

	SECTION("one excluded component")
	{
		registry.Each<Position, Velocity>(Without<Frozen>, record);
		CHECK(visited.size() == 2);
		CHECK(std::ranges::find(visited, frozen) == visited.end());
	}

	SECTION("several excluded components")
	{
		registry.Each<Position, Velocity>(Without<Frozen, Hidden>, record);
		CHECK(visited == std::vector<Entity>{moving});
	}

	SECTION("with a smaller pool driving the loop")
	{
		const Entity still = registry.Create();
		registry.Add<Position>(still);
		registry.Each<Velocity, Position>(Without<Hidden>, [&](Entity e, Velocity&, Position&)
		{
			visited.push_back(e);
		});
		CHECK(visited.size() == 2);
		CHECK(std::ranges::find(visited, hidden) == visited.end());
	}

	SECTION("an excluded component nobody has excludes nothing, and doesn't create its pool")
	{
		struct NeverAdded
		{
		};
		const std::size_t pools = registry.PoolCount();
		registry.Each<Position>(Without<NeverAdded>, [&](Entity e, Position&)
		{
			visited.push_back(e);
		});
		CHECK(visited.size() == 3);
		CHECK(registry.PoolCount() == pools);
	}
}

TEST_CASE("Registry hooks run when components are added and removed", "[ecs][registry]")
{
	Registry registry;
	std::vector<Entity> added;
	std::vector<Entity> removed;
	registry.OnAdd<Position>([&](Entity e, Position& p)
	{
		added.push_back(e);
		p.y = 5.0f; // a hook can set the component up
	});
	registry.OnRemove<Position>([&](Entity e, const Position&)
	{
		removed.push_back(e);
	});

	const Entity e = registry.Create();
	registry.Add<Position>(e, 1.0f, 2.0f);

	CHECK(added == std::vector<Entity>{e});
	CHECK(registry.GetComponent<Position>(e).x == 1.0f);
	CHECK(registry.GetComponent<Position>(e).y == 5.0f);
	CHECK(removed.empty());

	SECTION("Remove calls OnRemove while the component is still there")
	{
		registry.OnRemove<Position>([&](Entity entity, const Position& p)
		{
			CHECK(registry.HasComponent<Position>(entity));
			CHECK(p.x == 1.0f);
		});
		registry.Remove<Position>(e);
		CHECK(removed == std::vector<Entity>{e});
	}

	SECTION("removing a component the entity doesn't have calls nothing")
	{
		const Entity other = registry.Create();
		registry.Remove<Position>(other);
		CHECK(removed.empty());
	}

	SECTION("Destroy calls OnRemove for each component, before any is removed")
	{
		registry.Add<Velocity>(e, 3.0f, 0.0f);
		float seenVelocity = 0.0f;
		registry.OnRemove<Position>([&](Entity entity, const Position&)
		{
			seenVelocity = registry.GetComponent<Velocity>(entity).x;
		});
		registry.Destroy(e);
		CHECK(removed == std::vector<Entity>{e});
		CHECK(seenVelocity == 3.0f);
	}

	SECTION("a CommandBuffer's changes call the hooks too")
	{
		engine::ecs::CommandBuffer commands;
		const Entity later = registry.Create();
		commands.Add<Position>(later);
		commands.Destroy(e);
		commands.Apply(registry);
		CHECK(added == std::vector<Entity>{e, later});
		CHECK(removed == std::vector<Entity>{e});
	}

	SECTION("other component types don't call them")
	{
		registry.Add<Velocity>(e);
		registry.Remove<Velocity>(e);
		CHECK(added.size() == 1);
		CHECK(removed.empty());
	}
}

TEST_CASE("Registry asserts on structural changes inside a hook", "[ecs][registry]")
{
	if constexpr (!engine::kAssertsEnabled)
	{
		SKIP("ENGINE_ASSERT is compiled out in this configuration");
	}

	using engine::test::AssertFailure;
	const engine::test::AssertsThrow assertsThrow;

	Registry registry;
	const Entity e = registry.Create();

	SECTION("adding inside OnAdd")
	{
		registry.OnAdd<Position>([&](Entity entity, Position&)
		{
			registry.Add<Velocity>(entity);
		});
		CHECK_THROWS_AS(registry.Add<Position>(e), AssertFailure);
		CHECK_FALSE(registry.HasComponent<Velocity>(e));
	}

	SECTION("destroying inside OnRemove")
	{
		const Entity other = registry.Create();
		registry.Add<Position>(e);
		registry.OnRemove<Position>([&](Entity, const Position&)
		{
			registry.Destroy(other);
		});
		CHECK_THROWS_AS(registry.Remove<Position>(e), AssertFailure);
		CHECK(registry.IsAlive(other));
	}

	// Once the hook has unwound, structural changes are allowed again.
	CHECK_NOTHROW(registry.Add<Hidden>(e));
}
