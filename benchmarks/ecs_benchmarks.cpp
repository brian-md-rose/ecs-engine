// Benchmarks for the ECS hot paths: Registry::Each iteration and component add/remove churn.
//
// Timings only mean something in an optimized build. Use the ninja-bench preset (or the MSVC
// "bench" build preset, which builds Release), then run the executable:
//
//     cmake --preset ninja-bench
//     cmake --build --preset ninja-bench
//     ./build/ninja-bench/benchmarks/engine_benchmarks
//
// Catch2 options apply, e.g. "[iteration]" or "[churn]" to run one group, and
// --benchmark-samples 20 for a quicker run.

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

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
		float x = 1;
		float y = 1;
	};

	struct Health
	{
		int hp = 100;
	};

	struct Frozen
	{
	};

	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::ecs::Without;

	// Entity counts every benchmark runs at: a small scene, a busy one, and a stress case.
	constexpr std::array<std::size_t, 3> kEntityCounts = {{1'000, 10'000, 100'000}};

	// Catch2's console reporter cuts names at about 36 characters, so labels stay short:
	// "Each<Pos,Vel> 10% 100k".
	std::string Label(const char* what, std::size_t count)
	{
		return std::string(what) + " " + std::to_string(count / 1000) + "k";
	}

	// Creates count entities that all have a Position. Every velocityEvery-th one also gets a
	// Velocity and every frozenEvery-th one a Frozen tag (0 means none).
	std::vector<Entity> Populate(Registry& registry, std::size_t count, std::size_t velocityEvery,
	                             std::size_t frozenEvery)
	{
		std::vector<Entity> entities;
		entities.reserve(count);
		for (std::size_t i = 0; i < count; ++i)
		{
			const Entity entity = registry.Create();
			registry.Add<Position>(entity, static_cast<float>(i), 0.0f);
			if (velocityEvery != 0 && i % velocityEvery == 0) registry.Add<Velocity>(entity);
			if (frozenEvery != 0 && i % frozenEvery == 0) registry.Add<Frozen>(entity);
			entities.push_back(entity);
		}
		return entities;
	}
} // namespace

TEST_CASE("Registry iteration", "[benchmark][iteration]")
{
	for (const std::size_t count : kEntityCounts)
	{
		// One component: the packed array walk with no lookups.
		{
			Registry registry;
			Populate(registry, count, 0, 0);
			BENCHMARK(Label("Each<Pos>", count))
			{
				float sum = 0;
				registry.Each<Position>([&sum](Entity, Position& p) { sum += p.x; });
				return sum;
			};
		}

		// Two components on every entity: drives Position's packed array, looks up Velocity.
		{
			Registry registry;
			Populate(registry, count, 1, 0);
			BENCHMARK(Label("Each<Pos,Vel> all", count))
			{
				registry.Each<Position, Velocity>([](Entity, Position& p, const Velocity& v)
				{
					p.x += v.x;
					p.y += v.y;
				});
				return registry.EntityCount();
			};
		}

		// Velocity on 1 in 10: the loop is driven by the smaller Velocity pool, so it should cost
		// about a tenth of the case above, plus a Position lookup per entity.
		{
			Registry registry;
			Populate(registry, count, 10, 0);
			BENCHMARK(Label("Each<Pos,Vel> 10%", count))
			{
				registry.Each<Position, Velocity>([](Entity, Position& p, const Velocity& v)
				{
					p.x += v.x;
					p.y += v.y;
				});
				return registry.EntityCount();
			};
		}

		// Half the movers are Frozen and skipped by the Without filter.
		{
			Registry registry;
			Populate(registry, count, 1, 2);
			BENCHMARK(Label("Each<Pos,Vel> Without<Frozen>", count))
			{
				registry.Each<Position, Velocity>(Without<Frozen>, [](Entity, Position& p, const Velocity& v)
				{
					p.x += v.x;
					p.y += v.y;
				});
				return registry.EntityCount();
			};
		}
	}
}

TEST_CASE("Component add/remove churn", "[benchmark][churn]")
{
	for (const std::size_t count : kEntityCounts)
	{
		// Each sample adds a Velocity to every entity and removes it again, so the registry is
		// back where it started and every sample measures the same work.
		{
			Registry registry;
			const std::vector<Entity> entities = Populate(registry, count, 0, 0);
			BENCHMARK(Label("Add+Remove<Vel>", count))
			{
				for (const Entity entity : entities) registry.Add<Velocity>(entity);
				for (const Entity entity : entities) registry.Remove<Velocity>(entity);
				return registry.EntityCount();
			};
		}

		// The same, removing in reverse so every swap-and-pop removes the last element.
		{
			Registry registry;
			const std::vector<Entity> entities = Populate(registry, count, 0, 0);
			BENCHMARK(Label("Add+Remove<Vel> reverse", count))
			{
				for (const Entity entity : entities) registry.Add<Velocity>(entity);
				for (auto it = entities.rbegin(); it != entities.rend(); ++it) registry.Remove<Velocity>(*it);
				return registry.EntityCount();
			};
		}

		// The same with an OnAdd and an OnRemove hook registered: the cost of the hook calls.
		{
			Registry registry;
			const std::vector<Entity> entities = Populate(registry, count, 0, 0);
			std::size_t calls = 0;
			registry.OnAdd<Velocity>([&calls](Entity, Velocity&) { ++calls; });
			registry.OnRemove<Velocity>([&calls](Entity, Velocity&) { ++calls; });
			BENCHMARK(Label("Add+Remove<Vel> hooks", count))
			{
				for (const Entity entity : entities) registry.Add<Velocity>(entity);
				for (const Entity entity : entities) registry.Remove<Velocity>(entity);
				return calls;
			};
		}

		// Spawning and despawning: Create, three Adds, then Destroy, which visits every pool.
		// The first warm-up sample grows the pools; later samples reuse the freed ids.
		{
			Registry registry;
			std::vector<Entity> entities;
			entities.reserve(count);
			BENCHMARK(Label("Create+3 Adds+Destroy", count))
			{
				entities.clear();
				for (std::size_t i = 0; i < count; ++i)
				{
					const Entity entity = registry.Create();
					registry.Add<Position>(entity);
					registry.Add<Velocity>(entity);
					registry.Add<Health>(entity);
					entities.push_back(entity);
				}
				for (const Entity entity : entities) registry.Destroy(entity);
				return registry.EntityCount();
			};
		}
	}
}
