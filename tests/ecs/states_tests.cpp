#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "application/scheduledgame.hpp"
#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/states.hpp"
#include "ecs/world.hpp"
#include "input/keyboard.hpp"

namespace
{
	using engine::application::ScheduledGame;
	using engine::ecs::DespawnOnEnter;
	using engine::ecs::DespawnOnExit;
	using engine::ecs::FrameContext;
	using engine::ecs::InState;
	using engine::ecs::NextState;
	using engine::ecs::NotInState;
	using engine::ecs::Phase;
	using engine::ecs::Scheduler;
	using engine::ecs::State;
	using engine::ecs::World;

	enum class Screen : std::uint8_t
	{
		Title,
		Menu,
		Playing
	};

	enum class Pause : std::uint8_t
	{
		Running,
		Paused
	};

	// What the systems below ran, in order. Systems can't capture, so they write to this.
	struct Log
	{
		std::vector<int> calls;
	};

	template <int N> void Record(World& world, const FrameContext&)
	{
		world.resources.Get<Log>().calls.push_back(N);
	}

	// Codes: 1x = enter, 2x = exit; the tens digit's units say which value.
	constexpr int kEnterTitle = 10;
	constexpr int kEnterMenu = 11;
	constexpr int kEnterPlaying = 12;
	constexpr int kExitTitle = 20;
	constexpr int kExitMenu = 21;
	constexpr int kExitPlaying = 22;
	constexpr int kEnterRunning = 30;
	constexpr int kEnterPaused = 31;
	constexpr int kExitRunning = 40;
	constexpr int kExitPaused = 41;
	constexpr int kInput = 1;
	constexpr int kUpdate = 2;

	void GoToMenu(World& world, const FrameContext&)
	{
		world.resources.Get<NextState<Screen>>().Set(Screen::Menu);
	}

	void GoToPlaying(World& world, const FrameContext&)
	{
		world.resources.Get<NextState<Screen>>().Set(Screen::Playing);
	}

	void StayOnTitle(World& world, const FrameContext&)
	{
		world.resources.Get<NextState<Screen>>().Set(Screen::Title);
	}

	void PauseNow(World& world, const FrameContext&)
	{
		world.resources.Get<NextState<Pause>>().Set(Pause::Paused);
	}

	void SpawnMenuEntities(World& world, const FrameContext&)
	{
		const auto scoped = world.registry.Create();
		world.registry.Add<DespawnOnExit<Screen>>(scoped, DespawnOnExit<Screen>{Screen::Menu});
		const auto other = world.registry.Create();
		world.registry.Add<DespawnOnExit<Screen>>(other, DespawnOnExit<Screen>{Screen::Playing});
		(void)world.registry.Create(); // not scoped at all
	}

	struct Fixture
	{
		ScheduledGame game;
		engine::input::Keyboard keyboard;

		Fixture() { game.world.resources.Emplace<Log>(); }

		void Update() { game.Update(1.0 / 60.0, keyboard); }

		[[nodiscard]] std::vector<int>& Calls() { return game.world.resources.Get<Log>().calls; }
		[[nodiscard]] Screen Current() { return game.world.resources.Get<State<Screen>>().value; }
		[[nodiscard]] NextState<Screen>& Next() { return game.world.resources.Get<NextState<Screen>>(); }

		// Forgets what has run so far, so a test can look at just the next update.
		void ForgetCalls() { Calls().clear(); }
	};

	[[nodiscard]] std::size_t Count(Fixture& f)
	{
		std::size_t count = 0;
		f.game.world.registry.Each<DespawnOnExit<Screen>>([&count](engine::ecs::Entity, const DespawnOnExit<Screen>&)
		{
			++count;
		});
		return count;
	}
} // namespace

TEST_CASE("A state starts in its initial value, active", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Menu);
	CHECK(f.Current() == Screen::Menu);
	CHECK(f.game.world.resources.Get<State<Screen>>().active);
	CHECK_FALSE(f.Next().requested.has_value());
	CHECK(InState<Screen::Menu>(f.game.world));
	CHECK_FALSE(InState<Screen::Title>(f.game.world));
}

TEST_CASE("The first update runs the initial value's enter systems before Input", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Title, "EnterTitle", Record<kEnterTitle>);
	f.game.scheduler.AddOnEnter(Screen::Menu, "EnterMenu", Record<kEnterMenu>);
	f.game.scheduler.Add(Phase::Input, "Input", Record<kInput>);

	CHECK(f.Calls().empty()); // nothing runs until the first update
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kEnterTitle, kInput});
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kEnterTitle, kInput, kInput}); // only once
}

TEST_CASE("A change runs the old value's exit systems, then the new value's enter systems", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Title, "EnterTitle", Record<kEnterTitle>);
	f.game.scheduler.AddOnExit(Screen::Title, "ExitTitle", Record<kExitTitle>);
	f.game.scheduler.AddOnEnter(Screen::Menu, "EnterMenu", Record<kEnterMenu>);
	f.game.scheduler.AddOnExit(Screen::Menu, "ExitMenu", Record<kExitMenu>);
	f.Update();
	f.ForgetCalls();

	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kExitTitle, kEnterMenu});
	CHECK(f.Current() == Screen::Menu);
	CHECK_FALSE(f.Next().requested.has_value()); // the request is used up
}

TEST_CASE("Several systems on one value run in the order they were added", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnExit(Screen::Title, "First", Record<1>);
	f.game.scheduler.AddOnExit(Screen::Title, "Second", Record<2>);
	f.game.scheduler.AddOnEnter(Screen::Menu, "Third", Record<3>);
	f.game.scheduler.AddOnEnter(Screen::Menu, "Fourth", Record<4>);
	f.Update();
	f.ForgetCalls();

	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("The state still has the old value while the exit systems run, and the new one for enter", "[ecs][states]")
{
	struct Seen
	{
		std::vector<Screen> values;
	};
	Fixture f;
	f.game.world.resources.Emplace<Seen>();
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnExit(Screen::Title, "Exit", [](World& world, const FrameContext&)
	{
		world.resources.Get<Seen>().values.push_back(world.resources.Get<State<Screen>>().value);
	});
	f.game.scheduler.AddOnEnter(Screen::Menu, "Enter", [](World& world, const FrameContext&)
	{
		world.resources.Get<Seen>().values.push_back(world.resources.Get<State<Screen>>().value);
	});
	f.Update();
	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK(f.game.world.resources.Get<Seen>().values == std::vector<Screen>{Screen::Title, Screen::Menu});
}

TEST_CASE("A change requested during a phase takes effect at the start of the next update", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnExit(Screen::Title, "ExitTitle", Record<kExitTitle>);
	f.game.scheduler.AddOnEnter(Screen::Menu, "EnterMenu", Record<kEnterMenu>);
	f.game.scheduler.Add(Phase::Input, "Request", GoToMenu, InState<Screen::Title>);
	f.game.scheduler.Add(Phase::Update, "Update", Record<kUpdate>);
	f.Update();
	f.ForgetCalls();

	// The request was made in the first update's Input phase; this update's Update still sees
	// the title... no: the change is applied first, so it starts in the menu.
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kExitTitle, kEnterMenu, kUpdate});
	CHECK(f.Current() == Screen::Menu);
}

TEST_CASE("Within one update every system sees the same state", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.Add(Phase::Input, "Request", GoToMenu);
	f.game.scheduler.Add(Phase::Update, "OnTitle", Record<1>, InState<Screen::Title>);
	f.game.scheduler.Add(Phase::LateUpdate, "OnTitleLate", Record<2>, InState<Screen::Title>);
	f.game.scheduler.Add(Phase::LateUpdate, "OnMenuLate", Record<3>, InState<Screen::Menu>);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{1, 2}); // the request in Input changed nothing yet
	f.ForgetCalls();
	f.Update();
	CHECK(f.Calls() == std::vector<int>{3});
}

TEST_CASE("A change requested by an enter system takes effect at the start of the next update", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Menu, "EnterMenu", GoToPlaying);
	f.game.scheduler.AddOnEnter(Screen::Playing, "EnterPlaying", Record<kEnterPlaying>);
	f.Update();
	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK(f.Current() == Screen::Menu);
	CHECK(f.Calls().empty());
	f.Update();
	CHECK(f.Current() == Screen::Playing);
	CHECK(f.Calls() == std::vector<int>{kEnterPlaying});
}

TEST_CASE("Asking for the current value runs no exit or enter systems", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Title, "EnterTitle", Record<kEnterTitle>);
	f.game.scheduler.AddOnExit(Screen::Title, "ExitTitle", Record<kExitTitle>);
	f.game.scheduler.Add(Phase::Input, "Stay", StayOnTitle);
	f.Update();
	f.ForgetCalls();

	f.Update();
	f.Update();
	CHECK(f.Calls().empty());
	CHECK(f.Current() == Screen::Title);
}

TEST_CASE("When a value is requested twice the last request wins", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Menu, "EnterMenu", Record<kEnterMenu>);
	f.game.scheduler.AddOnEnter(Screen::Playing, "EnterPlaying", Record<kEnterPlaying>);
	f.Update();
	f.ForgetCalls();

	f.Next().Set(Screen::Menu);
	f.Next().Set(Screen::Playing);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kEnterPlaying});
}

TEST_CASE("Entities scoped to a value are destroyed when it is left, and no sooner", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Menu, "SpawnMenu", SpawnMenuEntities);
	f.Update();
	f.Next().Set(Screen::Menu);
	f.Update();
	REQUIRE(Count(f) == 2);
	const std::size_t entities = f.game.world.registry.EntityCount();

	f.Update();
	CHECK(Count(f) == 2); // still in the menu

	f.Next().Set(Screen::Playing);
	f.Update();
	CHECK(Count(f) == 1); // the one scoped to Menu is gone; the one scoped to Playing stays
	CHECK(f.game.world.registry.EntityCount() == entities - 1);

	f.Next().Set(Screen::Title);
	f.Update();
	CHECK(Count(f) == 0);
}

TEST_CASE("Scoped entities are destroyed after the exit systems have run", "[ecs][states]")
{
	struct Seen
	{
		std::size_t scoped = 99;
	};
	Fixture f;
	f.game.world.resources.Emplace<Seen>();
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Menu, "SpawnMenu", SpawnMenuEntities);
	f.game.scheduler.AddOnExit(Screen::Menu, "Count", [](World& world, const FrameContext&)
	{
		std::size_t count = 0;
		world.registry.Each<DespawnOnExit<Screen>>([&count](engine::ecs::Entity, const DespawnOnExit<Screen>&)
		{
			++count;
		});
		world.resources.Get<Seen>().scoped = count;
	});
	f.Update();
	f.Next().Set(Screen::Menu);
	f.Update();
	f.Next().Set(Screen::Title);
	f.Update();
	CHECK(f.game.world.resources.Get<Seen>().scoped == 2);
	CHECK(Count(f) == 1); // the Playing-scoped one stays; the Menu-scoped one is gone
}

TEST_CASE("An enter system's entities exist for the systems after it and are not destroyed", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Menu, "SpawnMenu", SpawnMenuEntities);
	f.game.scheduler.AddOnEnter(Screen::Menu, "Check", [](World& world, const FrameContext&)
	{
		std::size_t count = 0;
		world.registry.Each<DespawnOnExit<Screen>>([&count](engine::ecs::Entity, const DespawnOnExit<Screen>&)
		{
			++count;
		});
		world.resources.Get<Log>().calls.push_back(static_cast<int>(count));
	});
	f.Update();
	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{2});
}

TEST_CASE("InState is true for any of the values it is given", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Menu);
	const World& world = f.game.world;
	CHECK(InState<Screen::Menu>(world));
	CHECK(InState<Screen::Title, Screen::Menu>(world));
	CHECK_FALSE(InState<Screen::Title, Screen::Playing>(world));
	CHECK(NotInState<Screen::Title, Screen::Playing>(world));
	CHECK_FALSE(NotInState<Screen::Menu>(world));
}

TEST_CASE("A system with an InState condition runs only in that state", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.Add(Phase::Update, "OnlyMenu", Record<kUpdate>, InState<Screen::Menu>);
	f.Update();
	CHECK(f.Calls().empty());
	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kUpdate});
}

TEST_CASE("A sub-state exists only while its parent is in the value it hangs off", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddSubState(f.game.world, Screen::Playing, Pause::Running);
	const World& world = f.game.world;
	CHECK_FALSE(InState<Pause::Running>(world));
	CHECK(NotInState<Pause::Running, Pause::Paused>(world));

	f.Update();
	CHECK_FALSE(InState<Pause::Running>(world)); // the title: no pause state

	f.Next().Set(Screen::Playing);
	f.Update();
	CHECK(InState<Pause::Running>(world));
	CHECK_FALSE(InState<Pause::Paused>(world));

	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK_FALSE(InState<Pause::Running>(world));
}

TEST_CASE("Entering and leaving a sub-state's parent runs the sub-state's systems around the parent's", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddSubState(f.game.world, Screen::Playing, Pause::Running);
	f.game.scheduler.AddOnEnter(Screen::Playing, "EnterPlaying", Record<kEnterPlaying>);
	f.game.scheduler.AddOnExit(Screen::Playing, "ExitPlaying", Record<kExitPlaying>);
	f.game.scheduler.AddOnEnter(Pause::Running, "EnterRunning", Record<kEnterRunning>);
	f.game.scheduler.AddOnExit(Pause::Running, "ExitRunning", Record<kExitRunning>);
	f.game.scheduler.AddOnEnter(Pause::Paused, "EnterPaused", Record<kEnterPaused>);
	f.game.scheduler.AddOnExit(Pause::Paused, "ExitPaused", Record<kExitPaused>);
	f.Update();
	f.ForgetCalls();

	f.Next().Set(Screen::Playing);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kEnterPlaying, kEnterRunning}); // parent first
	f.ForgetCalls();

	f.game.world.resources.Get<NextState<Pause>>().Set(Pause::Paused);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kExitRunning, kEnterPaused});
	f.ForgetCalls();

	f.Next().Set(Screen::Title);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{kExitPaused, kExitPlaying}); // sub-state first
}

TEST_CASE("A sub-state starts from its initial value each time its parent value is entered", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddSubState(f.game.world, Screen::Playing, Pause::Running);
	f.game.scheduler.Add(Phase::Input, "Pause", PauseNow, InState<Pause::Running>);
	f.Update();
	f.Next().Set(Screen::Playing);
	f.Update(); // Running; the pause is requested during it
	f.Update();
	CHECK(InState<Pause::Paused>(f.game.world));

	f.Next().Set(Screen::Title);
	f.Update();
	f.Next().Set(Screen::Playing);
	f.Update();
	CHECK(InState<Pause::Running>(f.game.world));
}

TEST_CASE("A request for an inactive sub-state is dropped", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddSubState(f.game.world, Screen::Playing, Pause::Running);
	f.game.scheduler.AddOnEnter(Pause::Paused, "EnterPaused", Record<kEnterPaused>);
	f.Update();
	f.game.world.resources.Get<NextState<Pause>>().Set(Pause::Paused);
	f.Update();
	f.Next().Set(Screen::Playing);
	f.Update();
	CHECK(InState<Pause::Running>(f.game.world));
	CHECK(f.Calls().empty());
}

TEST_CASE("Entities scoped to a sub-state go when the parent leaves", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddSubState(f.game.world, Screen::Playing, Pause::Running);
	f.game.scheduler.AddOnEnter(Pause::Paused, "SpawnPauseMenu", [](World& world, const FrameContext&)
	{
		const auto e = world.registry.Create();
		world.registry.Add<DespawnOnExit<Pause>>(e, DespawnOnExit<Pause>{Pause::Paused});
	});
	f.Update();
	f.Next().Set(Screen::Playing);
	f.Update();
	f.game.world.resources.Get<NextState<Pause>>().Set(Pause::Paused);
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 1);
	f.Next().Set(Screen::Title);
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 0);
}

TEST_CASE("Enter and exit systems appear in the timings as Transition systems", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddOnEnter(Screen::Title, "EnterTitle", Record<kEnterTitle>);
	f.game.scheduler.AddOnExit(Screen::Title, "ExitTitle", Record<kExitTitle>);
	f.game.scheduler.Add(Phase::Update, "Update", Record<kUpdate>);

	std::vector<Scheduler::SystemTiming> timings = f.game.scheduler.Timings();
	REQUIRE(timings.size() == 3);
	CHECK(timings[0].phase == Phase::Transition);
	CHECK(timings[2].phase == Phase::Update);

	f.Update(); // runs EnterTitle only
	timings = f.game.scheduler.Timings();
	for (const Scheduler::SystemTiming& timing : timings)
	{
		if (timing.name == "EnterTitle") CHECK_FALSE(timing.skipped);
		if (timing.name == "ExitTitle") CHECK(timing.skipped);
	}
	f.Update(); // nothing changes
	for (const Scheduler::SystemTiming& timing : f.game.scheduler.Timings())
	{
		if (timing.name == "EnterTitle") CHECK(timing.skipped);
	}
}

TEST_CASE("Events published by an enter system are delivered in the same update", "[ecs][states]")
{
	struct Ping
	{
	};
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.world.events.Subscribe<Ping>(+[](World& world, const Ping&)
	{
		world.resources.Get<Log>().calls.push_back(99);
	});
	f.game.scheduler.AddOnEnter(Screen::Menu, "Ping", [](World& world, const FrameContext&)
	{
		world.events.Publish(Ping{});
	});
	f.Update();
	f.Next().Set(Screen::Menu);
	f.Update();
	CHECK(f.Calls() == std::vector<int>{99});
}

TEST_CASE("Entities scoped to entering a value go when the state enters it", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Menu);
	for (const Screen scope : {Screen::Title, Screen::Playing})
	{
		const auto e = f.game.world.registry.Create();
		f.game.world.registry.Add<DespawnOnEnter<Screen>>(e, DespawnOnEnter<Screen>{scope});
	}
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 2);

	f.Next().Set(Screen::Title); // only the entity scoped to Title goes
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 1);

	f.Next().Set(Screen::Menu); // entering another value leaves the rest alone
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 1);
}

TEST_CASE("Entities scoped to entering a value are destroyed before its enter systems run", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Menu);
	f.game.scheduler.AddOnEnter(Screen::Title, "SpawnScoped", [](World& world, const FrameContext&)
	{
		const auto e = world.registry.Create();
		world.registry.Add<DespawnOnEnter<Screen>>(e, DespawnOnEnter<Screen>{Screen::Title});
	});
	f.Update();
	f.Next().Set(Screen::Title);
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 1); // spawned by the entry, so it stays...
	f.Next().Set(Screen::Menu);
	f.Update();
	f.Next().Set(Screen::Title);
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 1); // ...and the next entry replaces it
}

TEST_CASE("Entities scoped to a sub-state value go when the sub-state enters it", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	f.game.scheduler.AddSubState(f.game.world, Screen::Playing, Pause::Running);
	f.Update();
	const auto e = f.game.world.registry.Create();
	f.game.world.registry.Add<DespawnOnEnter<Pause>>(e, DespawnOnEnter<Pause>{Pause::Paused});
	f.Next().Set(Screen::Playing);
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 1);
	f.game.world.resources.Get<NextState<Pause>>().Set(Pause::Paused);
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 0);
}

TEST_CASE("Entities scoped to the initial value go on the first update", "[ecs][states]")
{
	Fixture f;
	f.game.scheduler.AddState(f.game.world, Screen::Title);
	const auto e = f.game.world.registry.Create();
	f.game.world.registry.Add<DespawnOnEnter<Screen>>(e, DespawnOnEnter<Screen>{Screen::Title});
	f.Update();
	CHECK(f.game.world.registry.EntityCount() == 0);
}
