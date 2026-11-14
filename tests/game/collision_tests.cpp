#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "events.hpp"
#include "gamestate.hpp"
#include "scoring.hpp"
#include "systems.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::FrameContext;
	using engine::ecs::Phase;
	using engine::ecs::Scheduler;
	using engine::ecs::World;
	using game::TilePosition;

	// Handlers can't capture state, so the tests' handlers record events in a resource.
	struct Seen
	{
		std::vector<TilePosition> pellets;
		std::vector<TilePosition> powerPellets;
		int caught = 0;
		int cleared = 0;
	};

	void SeePellet(World& world, const game::PelletEaten& event)
	{
		world.resources.Get<Seen>().pellets.push_back(event.tile);
	}

	void SeePowerPellet(World& world, const game::PowerPelletEaten& event)
	{
		world.resources.Get<Seen>().powerPellets.push_back(event.tile);
	}

	void SeeCaught(World& world, const game::PlayerCaught&)
	{
		++world.resources.Get<Seen>().caught;
	}

	void SeeCleared(World& world, const game::LevelCleared&)
	{
		++world.resources.Get<Seen>().cleared;
	}

	// A world with just what collision and scoring use, subscribed the way MazeGame does it.
	struct Fixture
	{
		World world;
		Scheduler scheduler;

		Fixture()
		{
			world.resources.Emplace<game::Score>();
			world.resources.Emplace<game::ConsoleHud>().enabled = false;
			world.resources.Emplace<Seen>();
			scheduler.Add(Phase::LateUpdate, game::CollisionSystem);

			world.events.Subscribe<game::PelletEaten>(game::AddPelletScore);
			world.events.Subscribe<game::PelletEaten>(game::CheckLevelCleared<game::PelletEaten>);
			world.events.Subscribe<game::PelletEaten>(SeePellet);
			world.events.Subscribe<game::PowerPelletEaten>(game::AddPowerPelletScore);
			world.events.Subscribe<game::PowerPelletEaten>(game::CheckLevelCleared<game::PowerPelletEaten>);
			world.events.Subscribe<game::PowerPelletEaten>(SeePowerPellet);
			world.events.Subscribe<game::PlayerCaught>(SeeCaught);
			world.events.Subscribe<game::LevelCleared>(SeeCleared);
		}

		Entity Player(TilePosition tile)
		{
			const Entity entity = world.registry.Create();
			world.registry.Add<game::PlayerControlled>(entity);
			world.registry.Add<game::GridMover>(entity, game::GridMover{.tile = tile});
			return entity;
		}

		Entity Enemy(TilePosition tile)
		{
			const Entity entity = world.registry.Create();
			world.registry.Add<game::Enemy>(entity);
			world.registry.Add<game::GridMover>(entity, game::GridMover{.tile = tile});
			return entity;
		}

		template <typename Tag> Entity Edible(TilePosition tile)
		{
			const Entity entity = world.registry.Create();
			world.registry.Add<Tag>(entity);
			world.registry.Add<game::OnTile>(entity, game::OnTile{.tile = tile});
			return entity;
		}

		void Run() { scheduler.Run(Phase::LateUpdate, world, FrameContext{}); }

		[[nodiscard]] Seen& Events() { return world.resources.Get<Seen>(); }
		[[nodiscard]] int Points() { return world.resources.Get<game::Score>().points; }
	};

	constexpr TilePosition kHere{.column = 2, .row = 1};
	constexpr TilePosition kThere{.column = 3, .row = 1};
} // namespace

TEST_CASE("The player eats the pellet on its tile and scores for it", "[game][collision]")
{
	Fixture f;
	f.Player(kHere);
	const Entity eaten = f.Edible<game::Pellet>(kHere);
	const Entity other = f.Edible<game::Pellet>(kThere);

	f.Run();
	CHECK_FALSE(f.world.registry.IsAlive(eaten));
	CHECK(f.world.registry.IsAlive(other));
	CHECK(f.Events().pellets == std::vector<TilePosition>{kHere});
	CHECK(f.Points() == game::kPelletPoints);
	CHECK(f.Events().cleared == 0); // one pellet is left

	f.Run(); // nothing left on this tile
	CHECK(f.Points() == game::kPelletPoints);
}

TEST_CASE("A power pellet scores more than a pellet", "[game][collision]")
{
	Fixture f;
	f.Player(kHere);
	const Entity power = f.Edible<game::PowerPellet>(kHere);
	f.Edible<game::Pellet>(kThere);

	f.Run();
	CHECK_FALSE(f.world.registry.IsAlive(power));
	CHECK(f.Events().powerPellets == std::vector<TilePosition>{kHere});
	CHECK(f.Events().pellets.empty());
	CHECK(f.Points() == game::kPowerPelletPoints);
	CHECK(game::kPowerPelletPoints > game::kPelletPoints);
}

TEST_CASE("Eating the last pellet clears the level", "[game][collision]")
{
	Fixture f;
	const Entity player = f.Player(kHere);
	f.Edible<game::Pellet>(kHere);
	f.Edible<game::PowerPellet>(kThere);

	f.Run();
	CHECK(f.Events().cleared == 0);

	f.world.registry.GetComponent<game::GridMover>(player).tile = kThere;
	f.Run();
	CHECK(f.Events().cleared == 1);
	CHECK(f.Points() == game::kPelletPoints + game::kPowerPelletPoints);

	f.Run(); // cleared once, not every update after
	CHECK(f.Events().cleared == 1);
}

TEST_CASE("An enemy on the player's tile catches it", "[game][collision]")
{
	Fixture f;
	f.Player(kHere);
	const Entity enemy = f.Enemy(kThere);

	f.Run();
	CHECK(f.Events().caught == 0);

	f.world.registry.GetComponent<game::GridMover>(enemy).tile = kHere;
	f.Run();
	CHECK(f.Events().caught == 1);
}

TEST_CASE("Without a player nothing is eaten", "[game][collision]")
{
	Fixture f;
	const Entity pellet = f.Edible<game::Pellet>(kHere);
	f.Enemy(kHere);
	f.Run();
	CHECK(f.world.registry.IsAlive(pellet));
	CHECK(f.Events().caught == 0);
}
