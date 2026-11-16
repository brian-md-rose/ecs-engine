#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "application/scheduledgame.hpp"
#include "assert_guard.hpp"
#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "input/keyboard.hpp"

namespace
{
	using engine::application::ScheduledGame;
	using engine::ecs::Entity;
	using engine::ecs::FrameContext;
	using engine::ecs::Phase;
	using engine::ecs::Scheduler;
	using engine::ecs::World;
	using engine::test::AssertFailure;
	using engine::test::AssertsThrow;

	// Systems can't capture state, so the tests' systems record their runs in a resource.
	struct Log
	{
		std::vector<int> calls;
	};

	template <int N> void Record(World& world, const FrameContext&)
	{
		world.resources.Get<Log>().calls.push_back(N);
	}

	struct Gate
	{
		bool open = false;
	};

	bool IsOpen(const World& world)
	{
		return world.resources.Get<Gate>().open;
	}

	struct Marker
	{
	};

	void SpawnMarker(World& world, const FrameContext&)
	{
		world.commands.Add<Marker>(world.registry.Create());
	}

	struct MarkerCount
	{
		int value = -1;
	};

	void CountMarkers(World& world, const FrameContext&)
	{
		int count = 0;
		world.registry.Each<Marker>([&count](Entity, const Marker&)
		{
			++count;
		});
		world.resources.Get<MarkerCount>().value = count;
	}

	struct SeenDt
	{
		double value = 0;
	};

	void RecordDt(World& world, const FrameContext& context)
	{
		world.resources.Get<SeenDt>().value = context.dt;
	}

	void AddResourceMidPhase(World& world, const FrameContext&)
	{
		world.resources.Emplace<Gate>();
	}

	std::vector<int>& Calls(World& world)
	{
		return world.resources.Get<Log>().calls;
	}
} // namespace

TEST_CASE("Systems in a phase run in the order they were added", "[ecs][scheduler]")
{
	World world;
	world.resources.Emplace<Log>();
	Scheduler scheduler;
	scheduler.Add(Phase::Update, Record<2>);
	scheduler.Add(Phase::Update, Record<1>);
	scheduler.Add(Phase::Update, Record<3>);

	scheduler.Run(Phase::Update, world, {});

	CHECK(Calls(world) == std::vector<int>{2, 1, 3});
	CHECK(scheduler.SystemCount(Phase::Update) == 3);
}

TEST_CASE("Running a phase runs only that phase's systems", "[ecs][scheduler]")
{
	World world;
	world.resources.Emplace<Log>();
	Scheduler scheduler;
	scheduler.Add(Phase::Input, Record<1>);
	scheduler.Add(Phase::Update, Record<2>);
	scheduler.Add(Phase::LateUpdate, Record<3>);
	scheduler.Add(Phase::Render, Record<4>);

	scheduler.Run(Phase::LateUpdate, world, {});
	CHECK(Calls(world) == std::vector<int>{3});

	scheduler.Run(Phase::Render, world, {});
	CHECK(Calls(world) == std::vector<int>{3, 4});
}

TEST_CASE("An empty phase does nothing", "[ecs][scheduler]")
{
	World world;
	Scheduler scheduler;
	scheduler.Run(Phase::Update, world, {});
	CHECK(scheduler.SystemCount(Phase::Update) == 0);
}

TEST_CASE("A run condition skips its system while false", "[ecs][scheduler]")
{
	World world;
	world.resources.Emplace<Log>();
	world.resources.Emplace<Gate>();
	Scheduler scheduler;
	scheduler.Add(Phase::Update, Record<1>);
	scheduler.Add(Phase::Update, Record<2>, IsOpen);
	scheduler.Add(Phase::Update, Record<3>);

	scheduler.Run(Phase::Update, world, {});
	CHECK(Calls(world) == std::vector<int>{1, 3});

	world.resources.Get<Gate>().open = true;
	scheduler.Run(Phase::Update, world, {});
	CHECK(Calls(world) == std::vector<int>{1, 3, 1, 2, 3});
}

TEST_CASE("Commands are applied at the end of a phase", "[ecs][scheduler]")
{
	World world;
	world.resources.Emplace<MarkerCount>();
	Scheduler scheduler;
	scheduler.Add(Phase::Input, SpawnMarker);
	scheduler.Add(Phase::Input, CountMarkers); // same phase: not applied yet
	scheduler.Add(Phase::Update, CountMarkers);

	scheduler.Run(Phase::Input, world, {});
	CHECK(world.resources.Get<MarkerCount>().value == 0);
	CHECK(world.commands.Empty());

	scheduler.Run(Phase::Update, world, {});
	CHECK(world.resources.Get<MarkerCount>().value == 1);
}

TEST_CASE("Systems receive the frame context", "[ecs][scheduler]")
{
	World world;
	world.resources.Emplace<SeenDt>();
	Scheduler scheduler;
	scheduler.Add(Phase::Update, RecordDt);

	scheduler.Run(Phase::Update, world, FrameContext{.dt = 0.25});
	CHECK(world.resources.Get<SeenDt>().value == 0.25);
}

TEST_CASE("Resources can't be added or removed while a phase runs", "[ecs][scheduler][assert]")
{
	if constexpr (!engine::kAssertsEnabled)
	{
		SKIP("asserts are compiled out in this configuration");
	}

	const AssertsThrow guard;
	World world;
	Scheduler scheduler;
	scheduler.Add(Phase::Update, AddResourceMidPhase);

	const auto run = [&]
	{
		scheduler.Run(Phase::Update, world, {});
	};
	CHECK_THROWS_AS(run(), AssertFailure);

	// The lock is released even though the system threw.
	CHECK_FALSE(world.resources.IsLocked());
	world.resources.Emplace<Gate>();
	CHECK(world.resources.Has<Gate>());
}

TEST_CASE("ScheduledGame runs the fixed phases in order on Update", "[ecs][scheduler]")
{
	ScheduledGame game;
	game.world.resources.Emplace<Log>();
	game.world.resources.Emplace<SeenDt>();
	// Added out of order on purpose: phase order wins over registration order.
	game.scheduler.Add(Phase::LateUpdate, Record<3>);
	game.scheduler.Add(Phase::Render, Record<4>);
	game.scheduler.Add(Phase::Update, Record<2>);
	game.scheduler.Add(Phase::Input, Record<1>);
	game.scheduler.Add(Phase::Update, RecordDt);

	const engine::input::Keyboard keyboard;
	game.Update(1.0 / 60.0, keyboard);

	CHECK(Calls(game.world) == std::vector<int>{1, 2, 3}); // never Render
	CHECK(game.world.resources.Get<SeenDt>().value == 1.0 / 60.0);
}

namespace
{
	// Records whether it was handed the scheduler running it.
	struct SawScheduler
	{
		const Scheduler* scheduler = nullptr;
	};

	void RecordScheduler(World& world, const FrameContext& context)
	{
		world.resources.Get<SawScheduler>().scheduler = context.scheduler;
	}

	bool Never(const World&)
	{
		return false;
	}
} // namespace

TEST_CASE("Systems are given the scheduler running them", "[ecs][scheduler]")
{
	World world;
	world.resources.Emplace<SawScheduler>();
	Scheduler scheduler;
	scheduler.Add(Phase::Update, RecordScheduler);
	scheduler.Run(Phase::Update, world, FrameContext{});
	CHECK(world.resources.Get<SawScheduler>().scheduler == &scheduler);
}

TEST_CASE("The scheduler times each system and names it", "[ecs][scheduler]")
{
	World world;
	world.resources.Emplace<Log>();
	Scheduler scheduler;
	scheduler.Add(Phase::Input, Record<1>);
	scheduler.Add(Phase::Update, "Named", Record<2>);
	scheduler.Add(Phase::Update, "Skipped", Record<3>, Never);

	CHECK(scheduler.Timings().size() == 3);
	CHECK(scheduler.Timings()[1].averageMs == 0.0); // never run yet

	scheduler.Run(Phase::Input, world, FrameContext{});
	scheduler.Run(Phase::Update, world, FrameContext{});

	const std::vector<Scheduler::SystemTiming> timings = scheduler.Timings();
	REQUIRE(timings.size() == 3);
	CHECK(timings[0].phase == Phase::Input);
	CHECK(timings[0].name == "Input #0"); // unnamed: its phase and place
	CHECK(timings[1].phase == Phase::Update);
	CHECK(timings[1].name == "Named");
	CHECK(timings[1].averageMs >= 0.0);
	CHECK_FALSE(timings[1].skipped);
	CHECK(timings[2].name == "Skipped");
	CHECK(timings[2].skipped);
	CHECK(timings[2].averageMs == 0.0);
}

TEST_CASE("PhaseName names every phase", "[ecs][scheduler]")
{
	CHECK(engine::ecs::PhaseName(Phase::Input) == "Input");
	CHECK(engine::ecs::PhaseName(Phase::Update) == "Update");
	CHECK(engine::ecs::PhaseName(Phase::LateUpdate) == "LateUpdate");
	CHECK(engine::ecs::PhaseName(Phase::Render) == "Render");
}
