#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "components.hpp"
#include "direction.hpp"
#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "gamestate.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
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

	// Two corridors, one above the other, with the spawn at column 2, row 2.
	[[nodiscard]] game::TileMap TestMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"
		                                                "#.....#\n"
		                                                "#.P.E.#\n"
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

		Harness() { maze.world.resources.Get<game::ConsoleHud>().enabled = false; }

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
	CHECK(h.maze.world.resources.Has<game::FrameStats>());
	CHECK(h.maze.world.resources.Get<game::TileMap>().Width() == 7);

	using engine::ecs::Phase;
	CHECK(h.maze.scheduler.SystemCount(Phase::Input) == 2);
	CHECK(h.maze.scheduler.SystemCount(Phase::Update) == 2);
	CHECK(h.maze.scheduler.SystemCount(Phase::LateUpdate) == 1);
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
		CHECK(mover.direction == game::Direction::None);
		CHECK(mover.speed == game::kPlayerSpeed);
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
	CHECK(h.Count<game::Pellet>() == 8);

	h.Tap(Key::Right);
	h.Update(2 * 60); // to the wall at column 5, over the pellets at columns 3 and 5
	CHECK(h.Count<game::Pellet>() == 6);
	CHECK(h.Points() == 2 * game::kPelletPoints);
}
