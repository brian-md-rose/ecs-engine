#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "components.hpp"
#include "core/framestats.hpp"
#include "difficulty.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "grid/direction.hpp"
#include "input/keyboard.hpp"
#include "maze.hpp"
#include "mazegame.hpp"
#include "scoring.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Shape;
	using engine::graphics::Transform;
	using engine::input::Key;

	constexpr double kStep = 1.0 / 60.0;

	// Two corridors, one above the other, with the spawn at column 2, row 2: seven pellets and a
	// power pellet in the top-left corner. The enemy spawn is shut in below, so its enemy never
	// gets in the way; tests that need an enemy add one.
	[[nodiscard]] game::TileMap TestMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"
		                                                "#o....#\n"
		                                                "#.P. .#\n"
		                                                "#######\n"
		                                                "#E#####\n"
		                                                "#######\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	// The game as main.cpp runs it, minus the window: the fixed phases run on Update, and key
	// events go through a Keyboard the way Application feeds them.
	struct Harness
	{
		game::MazeGame maze{TestMaze()};
		engine::input::Keyboard keyboard;

		// Chooses START on the title screen, then waits for the round to be under way, past the
		// Ready pause, unless told not to.
		explicit Harness(bool startPlaying = true)
		{
			REQUIRE(Flow().stage == game::Stage::Title);
			Tap(Key::Enter);
			REQUIRE(Flow().stage == game::Stage::Ready);
			if (startPlaying) UpdateUntil(game::Stage::Playing);
		}

		[[nodiscard]] game::GameFlow& Flow() { return maze.world.resources.Get<game::GameFlow>(); }

		// Updates until the game reaches stage, failing if that takes over a minute.
		void UpdateUntil(game::Stage stage)
		{
			for (int i = 0; i < 60 * 60 && Flow().stage != stage; ++i)
			{
				Update();
			}
			REQUIRE(Flow().stage == stage);
		}

		void Update(int steps = 1)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, keyboard);
				keyboard.EndUpdate();
			}
		}

		void Tap(Key key)
		{
			keyboard.OnKeyDown(key);
			Update();
			keyboard.OnKeyUp(key);
			Update();
		}

		[[nodiscard]] Transform PlayerTransform()
		{
			Transform result{};
			int players = 0;
			maze.world.registry.Each<game::PlayerControlled, Transform>(
			    [&](Entity, const game::PlayerControlled&, const Transform& transform)
			{
				result = transform;
				++players;
			});
			REQUIRE(players == 1);
			return result;
		}

		[[nodiscard]] int StressShapeCount()
		{
			int count = 0;
			maze.world.registry.Each<game::StressShape>([&count](Entity, const game::StressShape&)
			{
				++count;
			});
			return count;
		}

		[[nodiscard]] bool Paused() const { return maze.world.resources.Get<game::GameState>().paused; }

		[[nodiscard]] int Points() const { return maze.world.resources.Get<game::Score>().points; }

		template <typename Tag> [[nodiscard]] int Count()
		{
			int count = 0;
			maze.world.registry.Each<Tag>([&count](Entity, const Tag&)
			{
				++count;
			});
			return count;
		}
	};

	// The top-left corner of a tile of the test maze, where the player is drawn when it stands
	// on that tile's centre.
	[[nodiscard]] engine::graphics::Position TileCorner(std::size_t column, std::size_t row)
	{
		return game::TileTopLeft(game::MazeOrigin(TestMaze()), {.column = column, .row = row});
	}

	// Where the player starts: the spawn tile.
	[[nodiscard]] engine::graphics::Position Start()
	{
		return TileCorner(2, 2);
	}
} // namespace

TEST_CASE("MazeGame adds its resources, systems, the maze and the player at its spawn", "[game][systems]")
{
	Harness h;
	CHECK(h.maze.world.resources.Has<game::GameState>());
	CHECK(h.maze.world.resources.Has<engine::core::FrameStats>());
	CHECK(h.maze.world.resources.Get<game::TileMap>().Width() == 7);

	using engine::ecs::Phase;
	CHECK(h.maze.scheduler.SystemCount(Phase::Input) == 3);
	CHECK(h.maze.scheduler.SystemCount(Phase::Update) == 5);
	CHECK(h.maze.scheduler.SystemCount(Phase::LateUpdate) == 7);
	CHECK(h.maze.scheduler.SystemCount(Phase::Render) == 1);

	const Transform start = h.PlayerTransform();
	CHECK(start.position.x == Start().x);
	CHECK(start.position.y == Start().y);
	CHECK_FALSE(h.Paused());

	int movers = 0;
	h.maze.world.registry.Each<game::PlayerControlled, game::GridMover>(
	    [&movers](Entity, const game::PlayerControlled&, const game::GridMover& mover)
	{
		CHECK(mover.tile == game::TilePosition{.column = 2, .row = 2});
		CHECK(mover.direction == engine::grid::Direction::None);
		CHECK(mover.speed == game::FallbackDifficulty().ForLevel(1).playerSpeed);
		++movers;
	});
	CHECK(movers == 1);
}

TEST_CASE("The player stays put until a direction is pressed", "[game][systems]")
{
	Harness h;
	h.Update(10);
	CHECK(h.PlayerTransform().position.x == Start().x);
	CHECK(h.PlayerTransform().position.y == Start().y);
}

TEST_CASE("The player keeps moving in the last direction tapped", "[game][systems]")
{
	Harness h;
	h.Tap(Key::Right);
	const float afterTap = h.PlayerTransform().position.x;
	CHECK(afterTap > Start().x);
	CHECK(h.PlayerTransform().position.y == Start().y);

	h.Update(10); // key released, still moving
	CHECK(h.PlayerTransform().position.x > afterTap);
	CHECK(h.PlayerTransform().position.y == Start().y);
}

TEST_CASE("The player turns on the grid at the next open tile", "[game][systems]")
{
	Harness h;
	h.Tap(Key::Right);
	h.Update(10); // 12 updates at 7.5 tiles/s: 1.5 tiles, half-way into column 4

	h.Tap(Key::Up); // asked before the centre of column 4: the turn is taken there
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == TileCorner(4, 2).x);
	CHECK(h.PlayerTransform().position.y == TileCorner(4, 1).y); // stopped by the outer wall
}

TEST_CASE("The player stops at walls", "[game][systems]")
{
	Harness h;
	h.Tap(Key::Right);
	h.Update(2 * 60); // far more than the 3 tiles to the wall
	CHECK(h.PlayerTransform().position.x == TileCorner(5, 2).x);
	CHECK(h.PlayerTransform().position.y == TileCorner(5, 2).y);

	h.Tap(Key::Down); // a wall below: stays put
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == TileCorner(5, 2).x);
	CHECK(h.PlayerTransform().position.y == TileCorner(5, 2).y);

	h.Tap(Key::Left);
	h.Update(2 * 60);
	CHECK(h.PlayerTransform().position.x == TileCorner(1, 2).x);
}

TEST_CASE("Pausing stops movement until unpaused", "[game][systems]")
{
	Harness h;
	h.Tap(Key::Right);
	h.Tap(Key::P);
	REQUIRE(h.Paused());

	const float pausedAt = h.PlayerTransform().position.x;
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == pausedAt);

	h.Tap(Key::Escape); // also bound to Pause
	CHECK_FALSE(h.Paused());
	h.Update();
	CHECK(h.PlayerTransform().position.x > pausedAt);
}

TEST_CASE("F1 adds and removes the stress-test shapes", "[game][systems]")
{
	Harness h;
	CHECK(h.StressShapeCount() == 0);

	h.Tap(Key::F1);
	CHECK(h.StressShapeCount() == game::kStressShapeCount);
	CHECK(h.maze.world.resources.Get<game::GameState>().stressTest);

	// Each stress shape has a transform and a shape below the player's layer.
	int checked = 0;
	h.maze.world.registry.Each<game::StressShape, Transform, Shape>(
	    [&checked](Entity, const game::StressShape&, const Transform&, const Shape& shape)
	{
		CHECK(shape.layer < game::kPlayerLayer);
		++checked;
	});
	CHECK(checked == game::kStressShapeCount);

	h.Tap(Key::F1);
	CHECK(h.StressShapeCount() == 0);
	CHECK_FALSE(h.maze.world.resources.Get<game::GameState>().stressTest);
	CHECK(h.PlayerTransform().position.x == Start().x); // the player is untouched
}

TEST_CASE("The stress test toggles while paused", "[game][systems]")
{
	Harness h;
	h.Tap(Key::P);
	h.Tap(Key::F1);
	CHECK(h.StressShapeCount() == game::kStressShapeCount);
}

TEST_CASE("The player eats the pellets it walks over", "[game][systems]")
{
	Harness h;
	CHECK(h.Points() == 0);
	CHECK(h.Count<game::Pellet>() == 7);

	h.Tap(Key::Right);
	h.Update(2 * 60); // to the wall at column 5, over the pellets at columns 3 and 5
	CHECK(h.Count<game::Pellet>() == 5);
	CHECK(h.Points() == 2 * game::kPelletPoints);
}

TEST_CASE("A round starts Ready, and the player waits for Playing", "[game][systems][gameflow]")
{
	Harness h(false);
	CHECK(h.Flow().stage == game::Stage::Ready);

	h.Tap(Key::Right); // pressed during Ready: remembered, not acted on
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == Start().x);

	h.UpdateUntil(game::Stage::Playing);
	h.Update(10);
	CHECK(h.PlayerTransform().position.x > Start().x);
}

TEST_CASE("Clearing every pellet completes the level, which restarts with the pellets back",
          "[game][systems][gameflow]")
{
	Harness h;
	const auto walk = [&h](Key key)
	{
		h.Tap(key);
		h.Update(60); // a second is enough for the longest straight in the test maze
	};

	walk(Key::Right); // columns 3 and 5 of row 2
	walk(Key::Up);
	walk(Key::Left); // row 1, the power pellet last
	CHECK(h.Flow().stage == game::Stage::Playing);
	CHECK(h.Count<game::Pellet>() == 1);
	CHECK(h.Count<game::PowerPellet>() == 0);

	walk(Key::Down); // the last pellet, at column 1 of row 2
	CHECK(h.Flow().stage == game::Stage::LevelComplete);
	CHECK(h.Count<game::Pellet>() == 0);
	CHECK(h.Points() == 7 * game::kPelletPoints + game::kPowerPelletPoints);

	// Nothing moves while the level is complete.
	const Transform done = h.PlayerTransform();
	h.Tap(Key::Right);
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == done.position.x);

	h.UpdateUntil(game::Stage::Ready);
	CHECK(h.Flow().level == 2);
	CHECK(h.Flow().lives == game::kStartingLives);
	CHECK(h.Count<game::Pellet>() == 7);
	CHECK(h.Count<game::PowerPellet>() == 1);
	CHECK(h.PlayerTransform().position.x == Start().x);
	CHECK(h.PlayerTransform().position.y == Start().y);
	CHECK(h.Points() == 7 * game::kPelletPoints + game::kPowerPelletPoints); // the score carries on

	// The new round plays like the first: the player stays put until told to move, then eats.
	h.UpdateUntil(game::Stage::Playing);
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == Start().x);
	walk(Key::Right);
	CHECK(h.Count<game::Pellet>() == 5);
}

TEST_CASE("An enemy catching the player costs a life and puts the player back", "[game][systems][gameflow]")
{
	Harness h;
	engine::ecs::Registry& registry = h.maze.world.registry;
	const Entity enemy = registry.Create();
	registry.Add<game::Enemy>(enemy);
	registry.Add<game::GridMover>(enemy, game::GridMover{.tile = {.column = 4, .row = 2}});

	h.Tap(Key::Right);
	h.Update(30); // through column 3 into the enemy at column 4
	CHECK(h.Flow().stage == game::Stage::Dying);
	CHECK(h.Flow().lives == game::kStartingLives - 1);

	h.UpdateUntil(game::Stage::Ready);
	CHECK(h.PlayerTransform().position.x == Start().x);
	CHECK(h.Count<game::Pellet>() == 6); // the pellet eaten on the way stays eaten
	CHECK(h.Points() == game::kPelletPoints);
}

TEST_CASE("Losing the last life ends the game; Confirm goes to the title, which starts a new one",
          "[game][systems][gameflow]")
{
	Harness h;
	h.Flow().lives = 1;
	engine::ecs::Registry& registry = h.maze.world.registry;
	const Entity enemy = registry.Create();
	registry.Add<game::Enemy>(enemy);
	registry.Add<game::GridMover>(enemy, game::GridMover{.tile = {.column = 4, .row = 2}});

	h.Tap(Key::Right);
	h.UpdateUntil(game::Stage::GameOver);
	CHECK(h.Points() == game::kPelletPoints);

	h.Update(5 * 60); // waits for a key
	CHECK(h.Flow().stage == game::Stage::GameOver);

	h.Tap(Key::Enter);
	CHECK(h.Flow().stage == game::Stage::Title);
	h.Tap(Key::Enter);
	CHECK(h.Flow().stage == game::Stage::Ready);
	CHECK(h.Flow().lives == game::kStartingLives);
	CHECK(h.Flow().level == 1);
	CHECK(h.Points() == 0);
	CHECK(h.Count<game::Pellet>() == 7);
	CHECK(h.PlayerTransform().position.x == Start().x);
}

TEST_CASE("Pausing stops the Ready timer", "[game][systems][gameflow]")
{
	Harness h(false);
	h.Tap(Key::P);
	h.Update(10 * 60);
	CHECK(h.Flow().stage == game::Stage::Ready);

	h.Tap(Key::P);
	h.UpdateUntil(game::Stage::Playing);
}
