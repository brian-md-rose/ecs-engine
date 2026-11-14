#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "assert_guard.hpp"
#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "ecs/events.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::EventBus;
	using engine::ecs::FrameContext;
	using engine::ecs::Phase;
	using engine::ecs::Scheduler;
	using engine::ecs::World;
	using engine::test::AssertFailure;
	using engine::test::AssertsThrow;

	struct Ping
	{
		int value = 0;
	};

	struct Pong
	{
		int value = 0;
	};

	// Handlers can't capture state, so they record what they saw in a resource.
	struct Log
	{
		std::vector<std::string> entries;
	};

	void Record(World& world, const std::string& entry)
	{
		world.resources.Get<Log>().entries.push_back(entry);
	}

	void FirstPing(World& world, const Ping& ping)
	{
		Record(world, "first ping " + std::to_string(ping.value));
	}

	void SecondPing(World& world, const Ping& ping)
	{
		Record(world, "second ping " + std::to_string(ping.value));
	}

	void OnPong(World& world, const Pong& pong)
	{
		Record(world, "pong " + std::to_string(pong.value));
	}

	// Answers every ping below 3 with a pong and the next ping.
	void Rally(World& world, const Ping& ping)
	{
		if (ping.value >= 3) return;
		world.events.Publish(Pong{ping.value});
		world.events.Publish(Ping{ping.value + 1});
	}

	void SubscribeInsideHandler(World& world, const Ping&)
	{
		world.events.Subscribe<Pong>(OnPong);
	}

	void DeliverInsideHandler(World& world, const Ping&)
	{
		world.events.Deliver(world);
	}

	struct Marker
	{
	};

	struct MarkerCount
	{
		int value = -1;
	};

	void CountMarkers(World& world, const Ping&)
	{
		int count = 0;
		world.registry.Each<Marker>([&count](Entity, const Marker&)
		{
			++count;
		});
		world.resources.Get<MarkerCount>().value = count;
	}

	void SpawnMarkerOnPong(World& world, const Pong&)
	{
		world.commands.Add<Marker>(world.registry.Create());
	}

	void SpawnMarkerAndPing(World& world, const FrameContext&)
	{
		world.commands.Add<Marker>(world.registry.Create());
		world.events.Publish(Ping{1});
	}

	void PublishPong(World& world, const FrameContext&)
	{
		world.events.Publish(Pong{7});
	}

	void CountMarkersSystem(World& world, const FrameContext&)
	{
		CountMarkers(world, Ping{});
	}

	[[nodiscard]] std::vector<std::string> Entries(World& world)
	{
		return world.resources.Get<Log>().entries;
	}
} // namespace

TEST_CASE("Published events wait for Deliver", "[ecs][events]")
{
	World world;
	world.resources.Emplace<Log>();
	world.events.Subscribe<Ping>(FirstPing);

	world.events.Publish(Ping{1});
	CHECK(world.events.PendingCount() == 1);
	CHECK(Entries(world).empty());

	world.events.Deliver(world);
	CHECK(Entries(world) == std::vector<std::string>{"first ping 1"});
	CHECK(world.events.PendingCount() == 0);

	world.events.Deliver(world); // each event is delivered once
	CHECK(Entries(world).size() == 1);
}

TEST_CASE("Handlers get each event in the order they subscribed", "[ecs][events]")
{
	World world;
	world.resources.Emplace<Log>();
	world.events.Subscribe<Ping>(SecondPing);
	world.events.Subscribe<Ping>(FirstPing);
	CHECK(world.events.SubscriberCount<Ping>() == 2);
	CHECK(world.events.SubscriberCount<Pong>() == 0);

	world.events.Publish(Ping{5});
	world.events.Deliver(world);
	CHECK(Entries(world) == std::vector<std::string>{"second ping 5", "first ping 5"});
}

TEST_CASE("Events are delivered in the order they were published, across types", "[ecs][events]")
{
	World world;
	world.resources.Emplace<Log>();
	world.events.Subscribe<Ping>(FirstPing);
	world.events.Subscribe<Pong>(OnPong);

	world.events.Publish(Ping{1});
	world.events.Publish(Pong{2});
	world.events.Publish(Ping{3});
	world.events.Deliver(world);
	CHECK(Entries(world) == std::vector<std::string>{"first ping 1", "pong 2", "first ping 3"});
}

TEST_CASE("Events published by handlers are delivered in the same pass, after earlier events", "[ecs][events]")
{
	World world;
	world.resources.Emplace<Log>();
	world.events.Subscribe<Ping>(FirstPing);
	world.events.Subscribe<Ping>(Rally);
	world.events.Subscribe<Pong>(OnPong);

	world.events.Publish(Ping{1});
	world.events.Publish(Pong{0});
	world.events.Deliver(world);
	CHECK(Entries(world) ==
	      std::vector<std::string>{"first ping 1", "pong 0", "pong 1", "first ping 2", "pong 2", "first ping 3"});
	CHECK(world.events.PendingCount() == 0);
}

TEST_CASE("Events nobody subscribes to are dropped", "[ecs][events]")
{
	World world;
	world.resources.Emplace<Log>();
	world.events.Publish(Pong{1});
	CHECK(world.events.PendingCount() == 0);

	world.events.Subscribe<Pong>(OnPong); // too late for the first one
	world.events.Deliver(world);
	CHECK(Entries(world).empty());
}

TEST_CASE("Subscribing or delivering from inside a handler asserts", "[ecs][events]")
{
	if constexpr (!engine::kAssertsEnabled)
	{
		SKIP("asserts are compiled out in this configuration");
	}
	const AssertsThrow guard;

	SECTION("subscribe")
	{
		World world;
		world.events.Subscribe<Ping>(SubscribeInsideHandler);
		world.events.Publish(Ping{});
		CHECK_THROWS_AS(world.events.Deliver(world), AssertFailure);
		CHECK(world.events.PendingCount() == 0); // the queue is dropped, not left half-delivered
	}
	SECTION("deliver")
	{
		World world;
		world.events.Subscribe<Ping>(DeliverInsideHandler);
		world.events.Publish(Ping{});
		CHECK_THROWS_AS(world.events.Deliver(world), AssertFailure);
	}
}

TEST_CASE("The scheduler delivers events after a phase's commands are applied", "[ecs][events][scheduler]")
{
	World world;
	world.resources.Emplace<MarkerCount>();
	world.events.Subscribe<Ping>(CountMarkers);

	Scheduler scheduler;
	scheduler.Add(Phase::Update, SpawnMarkerAndPing);
	scheduler.Run(Phase::Update, world, FrameContext{});
	CHECK(world.resources.Get<MarkerCount>().value == 1); // the marker queued with the ping exists
	CHECK(world.events.PendingCount() == 0);
}

TEST_CASE("Commands queued by handlers are applied before the next phase", "[ecs][events][scheduler]")
{
	World world;
	world.resources.Emplace<MarkerCount>();
	world.events.Subscribe<Pong>(SpawnMarkerOnPong);

	Scheduler scheduler;
	scheduler.Add(Phase::Update, PublishPong);
	scheduler.Add(Phase::LateUpdate, CountMarkersSystem);
	scheduler.Run(Phase::Update, world, FrameContext{});
	CHECK(world.commands.Empty());
	scheduler.Run(Phase::LateUpdate, world, FrameContext{});
	CHECK(world.resources.Get<MarkerCount>().value == 1);
}

TEST_CASE("Handlers can't add resources while the scheduler delivers", "[ecs][events][scheduler]")
{
	World world;
	world.events.Subscribe<Pong>([](World& w, const Pong&)
	{
		CHECK(w.resources.IsLocked());
	});

	Scheduler scheduler;
	scheduler.Add(Phase::Update, PublishPong);
	scheduler.Run(Phase::Update, world, FrameContext{});
	CHECK_FALSE(world.resources.IsLocked());
}

TEST_CASE("An EventBus starts empty", "[ecs][events]")
{
	const EventBus bus;
	CHECK(bus.PendingCount() == 0);
	CHECK(bus.SubscriberCount<Ping>() == 0);
}
