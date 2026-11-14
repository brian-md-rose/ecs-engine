#include <catch2/catch_test_macros.hpp>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "gamestate.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "input/keyboard.hpp"
#include "maze.hpp"
#include "mazegame.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Shape;
	using engine::graphics::Transform;
	using engine::input::Key;

	constexpr double kStep = 1.0 / 60.0;

	// A maze with room to move; walls don't stop the player yet. The spawn is column 2, row 2.
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
	};

	// Where the player starts: the top-left corner of the spawn tile.
	[[nodiscard]] engine::graphics::Position Start()
	{
		return game::TileTopLeft(game::MazeOrigin(TestMaze()), {.column = 2, .row = 2});
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

	h.Tap(Key::Up);
	const Transform turned = h.PlayerTransform();
	h.Update();
	CHECK(h.PlayerTransform().position.y < turned.position.y);
	CHECK(h.PlayerTransform().position.x == turned.position.x);
}

TEST_CASE("The player stops at the screen edge", "[game][systems]")
{
	Harness h;
	h.Tap(Key::Left);
	h.Update(5 * 60); // 5 s at 90 px/s is far past the edge

	CHECK(h.PlayerTransform().position.x == 0.0f);

	h.Tap(Key::Down);
	h.Update(5 * 60);
	CHECK(h.PlayerTransform().position.y == game::kLogicalHeight - game::kPlayerSize);
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
