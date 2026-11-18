#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "components.hpp"
#include "core/framestats.hpp"
#include "debug/debugpanel.hpp"
#include "difficulty.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "grid/direction.hpp"
#include "hud.hpp"
#include "input/input.hpp"
#include "maze.hpp"
#include "mazegame.hpp"
#include "nameentry.hpp"
#include "options.hpp"
#include "scoring.hpp"
#include "stage.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Shape;
	using engine::graphics::Transform;
	using engine::input::Key;
	using gametest::Stage;

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
		engine::input::Input input;

		// Chooses START on the title screen, then waits for the round to be under way, past the
		// Ready pause, unless told not to.
		explicit Harness(bool startPlaying = true)
		{
			REQUIRE(Now() == Stage::Title);
			Tap(Key::Enter);
			REQUIRE(Now() == Stage::Ready);
			if (startPlaying) UpdateUntil(Stage::Playing);
		}

		[[nodiscard]] game::GameFlow& Flow() { return maze.world.resources.Get<game::GameFlow>(); }

		[[nodiscard]] Stage Now() const { return gametest::StageOf(maze); }

		// Updates until the game reaches stage, failing if that takes over a minute.
		void UpdateUntil(Stage stage)
		{
			for (int i = 0; i < 60 * 60 && Now() != stage; ++i)
			{
				Update();
			}
			REQUIRE(Now() == stage);
		}

		void Update(int steps = 1)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, input);
				input.EndUpdate();
			}
		}

		void Tap(Key key)
		{
			input.keyboard.OnKeyDown(key);
			Update();
			input.keyboard.OnKeyUp(key);
			Update();
		}

		[[nodiscard]] int PlayerCount()
		{
			int players = 0;
			maze.world.registry.Each<game::PlayerControlled>([&players](Entity, const game::PlayerControlled&)
			{
				++players;
			});
			return players;
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

		[[nodiscard]] bool Paused() const { return gametest::IsPaused(maze); }

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
	CHECK(h.maze.world.resources.Has<engine::debug::DebugPanel>());
	CHECK(h.maze.world.resources.Get<game::TileMap>().Width() == 7);

	using engine::ecs::Phase;
	CHECK(h.maze.scheduler.SystemCount(Phase::Input) == 8); // with the debug panel's toggle
	CHECK(h.maze.scheduler.SystemCount(Phase::Update) == 6);
	CHECK(h.maze.scheduler.SystemCount(Phase::LateUpdate) == 7);
	CHECK(h.maze.scheduler.SystemCount(Phase::Render) == 2); // the debug panel, then Render

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
	CHECK(h.Now() == Stage::Ready);

	h.Tap(Key::Right); // pressed during Ready: remembered, not acted on
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == Start().x);

	h.UpdateUntil(Stage::Playing);
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
	CHECK(h.Now() == Stage::Playing);
	CHECK(h.Count<game::Pellet>() == 1);
	CHECK(h.Count<game::PowerPellet>() == 0);

	walk(Key::Down); // the last pellet, at column 1 of row 2
	CHECK(h.Now() == Stage::LevelTransition);
	CHECK(h.Count<game::Pellet>() == 0);
	CHECK(h.Points() == 7 * game::kPelletPoints + game::kPowerPelletPoints);

	// Nothing moves while the level is complete.
	const Transform done = h.PlayerTransform();
	h.Tap(Key::Right);
	h.Update(30);
	CHECK(h.PlayerTransform().position.x == done.position.x);

	h.UpdateUntil(Stage::Ready);
	CHECK(h.Flow().level == 2);
	CHECK(h.Flow().lives == game::kStartingLives);
	CHECK(h.Count<game::Pellet>() == 7);
	CHECK(h.Count<game::PowerPellet>() == 1);
	CHECK(h.PlayerTransform().position.x == Start().x);
	CHECK(h.PlayerTransform().position.y == Start().y);
	CHECK(h.Points() == 7 * game::kPelletPoints + game::kPowerPelletPoints); // the score carries on

	// The new round plays like the first: the player stays put until told to move, then eats.
	h.UpdateUntil(Stage::Playing);
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
	CHECK(h.Now() == Stage::Dying);
	CHECK(h.Flow().lives == game::kStartingLives - 1);

	h.UpdateUntil(Stage::Ready);
	CHECK(h.PlayerTransform().position.x == Start().x);
	CHECK(h.Count<game::Pellet>() == 6); // the pellet eaten on the way stays eaten
	CHECK(h.Points() == game::kPelletPoints);
}

TEST_CASE("Losing the last life ends the game; Confirm goes to the title, which starts a new one",
          "[game][systems][gameflow]")
{
	Harness h;
	h.Flow().lives = 1;
	h.maze.world.resources.Get<game::HighScore>().record = 1000; // out of reach: no name entry
	engine::ecs::Registry& registry = h.maze.world.registry;
	const Entity enemy = registry.Create();
	registry.Add<game::Enemy>(enemy);
	registry.Add<game::GridMover>(enemy, game::GridMover{.tile = {.column = 4, .row = 2}});

	h.Tap(Key::Right);
	h.UpdateUntil(Stage::GameOver);
	CHECK(h.Points() == game::kPelletPoints);

	h.Update(5 * 60); // waits for a key
	CHECK(h.Now() == Stage::GameOver);

	h.Tap(Key::Enter);
	CHECK(h.Now() == Stage::Title);
	h.Tap(Key::Enter);
	CHECK(h.Now() == Stage::Ready);
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
	CHECK(h.Now() == Stage::Ready);

	h.Tap(Key::P);
	h.UpdateUntil(Stage::Playing);
}

namespace
{
	// Walks the player into an enemy at column 4 of the test maze with one life left.
	void LoseTheLastLife(Harness& h)
	{
		h.Flow().lives = 1;
		engine::ecs::Registry& registry = h.maze.world.registry;
		const Entity enemy = registry.Create();
		registry.Add<game::Enemy>(enemy);
		registry.Add<game::GridMover>(enemy, game::GridMover{.tile = {.column = 4, .row = 2}});
		h.Tap(Key::Right);
	}

	[[nodiscard]] game::HighScore& High(Harness& h)
	{
		return h.maze.world.resources.Get<game::HighScore>();
	}

	[[nodiscard]] std::size_t ScreenEntities(Harness& h)
	{
		std::size_t count = 0;
		h.maze.world.registry.Each<engine::ecs::DespawnOnExit<game::Screen>>(
		    [&count](Entity, const engine::ecs::DespawnOnExit<game::Screen>&)
		{
			++count;
		});
		return count;
	}
} // namespace

TEST_CASE("A game that beats the high score goes to the name entry before GAME OVER", "[game][systems][gameflow]")
{
	Harness h;
	LoseTheLastLife(h);
	h.UpdateUntil(Stage::NameEntry);
	CHECK(h.Points() == game::kPelletPoints);
	CHECK(High(h).record == 0); // not recorded until a name is entered

	h.Tap(Key::Up); // the first slot's A becomes B
	h.Tap(Key::Right);
	h.Tap(Key::Up);
	h.Tap(Key::Up); // the second slot, a space, becomes A and then B
	CHECK(h.maze.world.resources.Get<game::NameEntry>().slots.substr(0, 2) == "BB");

	h.Tap(Key::Enter);
	CHECK(h.Now() == Stage::GameOver);
	CHECK(High(h).record == game::kPelletPoints);
	CHECK(High(h).name == "BB");

	h.Tap(Key::Enter);
	CHECK(h.Now() == Stage::Title);
}

TEST_CASE("The name-entry screen is spawned on entry and gone on exit", "[game][systems][gameflow]")
{
	Harness h;
	const std::size_t title = ScreenEntities(h);
	CHECK(title == 0); // the title's entities went when it was left
	LoseTheLastLife(h);
	h.UpdateUntil(Stage::NameEntry);
	CHECK(ScreenEntities(h) > 0);
	h.Tap(Key::Enter);
	h.Tap(Key::Enter);
	REQUIRE(h.Now() == Stage::Title);
	CHECK(ScreenEntities(h) > 0); // the title again
}

TEST_CASE("Leaving a game with a new best score from the pause menu goes to the name entry",
          "[game][systems][gameflow]")
{
	Harness h;
	h.Tap(Key::Right);
	h.Update(30); // a pellet or two
	REQUIRE(h.Points() > 0);
	h.Tap(Key::P);
	h.Tap(Key::Down); // QUIT TO TITLE
	h.Tap(Key::Enter);
	CHECK(h.Now() == Stage::NameEntry);
	CHECK_FALSE(h.Paused()); // Playing was left, and with it the pause
}

TEST_CASE("Leaving a game from the pause menu without a new best score goes back to the title",
          "[game][systems][gameflow]")
{
	Harness h;
	High(h).record = 1000;
	h.Tap(Key::P);
	h.Tap(Key::Down);
	h.Tap(Key::Enter);
	CHECK(h.Now() == Stage::Title);
	CHECK(h.PlayerCount() == 0); // the world goes with the game
}

TEST_CASE("The pause menu exists only while paused", "[game][systems][gameflow]")
{
	Harness h;
	const std::size_t before = h.maze.world.registry.EntityCount();
	h.Tap(Key::P);
	REQUIRE(h.Paused());
	CHECK(h.maze.world.registry.EntityCount() > before);
	h.Tap(Key::Enter); // CONTINUE
	CHECK_FALSE(h.Paused());
	CHECK(h.maze.world.registry.EntityCount() == before);
}

TEST_CASE("Clearing a level shows the level screen, then starts the next level", "[game][systems][gameflow]")
{
	Harness h;
	const auto walk = [&h](Key key)
	{
		h.Tap(key);
		h.Update(60);
	};
	walk(Key::Right);
	walk(Key::Up);
	walk(Key::Left);
	walk(Key::Down);
	REQUIRE(h.Now() == Stage::LevelTransition);
	CHECK(h.Flow().level == 2);
	CHECK(ScreenEntities(h) > 0);

	h.Update(static_cast<int>(game::kLevelTransitionSeconds * 60.0f) - 60); // just short of the end
	CHECK(h.Now() == Stage::LevelTransition);
	h.UpdateUntil(Stage::Ready);
	CHECK(ScreenEntities(h) == 0);
	CHECK(h.Flow().level == 2);
}

TEST_CASE("A game starts with the lives chosen in the options", "[game][systems][gameflow]")
{
	game::MazeGame maze{TestMaze()};
	engine::input::Input input;
	maze.world.resources.Get<game::Settings>().lives = 5;
	input.keyboard.OnKeyDown(Key::Enter);
	maze.Update(kStep, input);
	input.EndUpdate();
	maze.Update(kStep, input); // the change to Playing takes effect
	CHECK(gametest::StageOf(maze) == Stage::Ready);
	CHECK(maze.world.resources.Get<game::GameFlow>().lives == 5);
}
