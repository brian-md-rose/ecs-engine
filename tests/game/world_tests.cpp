#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "graphics/font.hpp"
#include "hud.hpp"
#include "input/input.hpp"
#include "mazegame.hpp"
#include "stage.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::input::Key;
	using gametest::Stage;

	constexpr double kStep = 1.0 / 60.0;

	[[nodiscard]] game::TileMap TestMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"
		                                                "#o..-.#\n"
		                                                "#.P. .#\n"
		                                                "#######\n"
		                                                "#E#####\n"
		                                                "#######\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	struct Harness
	{
		game::MazeGame maze{TestMaze()};
		engine::input::Input input;

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

		void GoTo(game::Screen screen)
		{
			maze.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(screen);
			Update(2);
		}

		void UpdateUntil(Stage stage)
		{
			for (int i = 0; i < 60 * 60 && gametest::StageOf(maze) != stage; ++i)
				Update();
			REQUIRE(gametest::StageOf(maze) == stage);
		}

		template <typename T> [[nodiscard]] std::size_t Count()
		{
			std::size_t count = 0;
			maze.world.registry.Each<T>([&count](engine::ecs::Entity, const T&)
			{
				++count;
			});
			return count;
		}

		[[nodiscard]] std::size_t Entities() const { return maze.world.registry.EntityCount(); }

		// Every part of the game's world, by kind: how many kinds are missing, and how many are there.
		[[nodiscard]] std::size_t WorldKindsPresent()
		{
			std::size_t kinds = 0;
			kinds += Count<game::Wall>() > 0 ? std::size_t{1} : std::size_t{0};
			kinds += Count<game::Door>() > 0 ? std::size_t{1} : std::size_t{0};
			kinds += Count<game::Pellet>() > 0 ? std::size_t{1} : std::size_t{0};
			kinds += Count<game::PowerPellet>() > 0 ? std::size_t{1} : std::size_t{0};
			kinds += Count<game::PlayerControlled>() > 0 ? std::size_t{1} : std::size_t{0};
			kinds += Count<game::Enemy>() > 0 ? std::size_t{1} : std::size_t{0};
			kinds += Count<game::HudText>() > 0 ? std::size_t{1} : std::size_t{0};
			return kinds;
		}
	};

	constexpr std::size_t kAllKinds = 7;
} // namespace

TEST_CASE("The title and options screens show no part of the game's world", "[game][world]")
{
	Harness h;
	h.Update(2);
	REQUIRE(gametest::StageOf(h.maze) == Stage::Title);
	CHECK(h.WorldKindsPresent() == 0);

	h.GoTo(game::Screen::Options);
	REQUIRE(gametest::StageOf(h.maze) == Stage::Options);
	CHECK(h.WorldKindsPresent() == 0);
}

TEST_CASE("START creates the whole world, pellets included, by the next update", "[game][world]")
{
	Harness h;
	h.Update(2);
	h.Tap(Key::Enter);
	REQUIRE(gametest::StageOf(h.maze) == Stage::Ready);
	CHECK(h.WorldKindsPresent() == kAllKinds);
	CHECK(h.Count<game::Pellet>() > 0);
}

TEST_CASE("A level transition keeps one copy of the world", "[game][world]")
{
	Harness h;
	gametest::RequestPlaying(h.maze);
	h.Update(2);
	REQUIRE(gametest::StageOf(h.maze) == Stage::Ready);
	const std::size_t pellets = h.Count<game::Pellet>();
	const std::size_t entities = h.Entities();

	// A pellet eaten in the level is back in the next.
	h.maze.world.registry.Each<game::Pellet>([&h](engine::ecs::Entity pellet, const game::Pellet&)
	{
		h.maze.world.commands.Destroy(pellet);
	});
	h.maze.world.commands.Apply(h.maze.world.registry);

	h.GoTo(game::Screen::LevelTransition);
	h.UpdateUntil(Stage::Ready);
	CHECK(h.Entities() == entities);
	CHECK(h.Count<game::Pellet>() == pellets);
	CHECK(h.Count<game::PlayerControlled>() == 1);
}

TEST_CASE("Quitting from the pause menu removes the world again", "[game][world]")
{
	Harness h;
	h.Update(2);
	h.Tap(Key::Enter);
	h.UpdateUntil(Stage::Playing);
	h.maze.world.resources.Get<game::HighScore>().record = 1000;
	h.Tap(Key::P);
	h.Tap(Key::Down);
	h.Tap(Key::Enter);
	REQUIRE(gametest::StageOf(h.maze) == Stage::Title);
	CHECK(h.WorldKindsPresent() == 0);

	// And a new game makes a new world, with its pellets.
	h.Tap(Key::Enter);
	CHECK(h.WorldKindsPresent() == kAllKinds);
}

TEST_CASE("Going from the game over screen to the title removes the world", "[game][world]")
{
	Harness h;
	h.Update(2);
	h.Tap(Key::Enter);
	h.GoTo(game::Screen::GameOver);
	CHECK(h.WorldKindsPresent() == kAllKinds); // still shown, dimmed, behind the end screen
	h.GoTo(game::Screen::Title);
	CHECK(h.WorldKindsPresent() == 0);
}
