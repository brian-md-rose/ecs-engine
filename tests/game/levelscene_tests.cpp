#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "enemysystems.hpp"
#include "gameflow.hpp"
#include "graphics/font.hpp"
#include "hud.hpp"
#include "input/input.hpp"
#include "mazegame.hpp"
#include "mazescene.hpp"
#include "scenes.hpp"
#include "stage.hpp"
#include "testassets.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;

	constexpr double kStep = 1.0 / 60.0;

	[[nodiscard]] game::TileMap SmallMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"
		                                                "#P....#\n"
		                                                "#######\n"
		                                                "#E#####\n"
		                                                "#######\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	struct LevelGame
	{
		game::MazeGame maze{SmallMaze(), gametest::SceneAssets()};
		engine::input::Input input;

		void Update(int steps)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, input);
				input.EndUpdate();
			}
		}

		template <typename T> [[nodiscard]] std::size_t Count()
		{
			std::size_t count = 0;
			maze.world.registry.Each<T>([&](Entity, const T&)
			{
				++count;
			});
			return count;
		}
	};
} // namespace

TEST_CASE("Play on a maze scene starts a game on that maze")
{
	LevelGame game;
	// What the editor does for Play: empty world, the scene loaded, then onPlay.
	REQUIRE_FALSE(game::SpawnScene(game.maze.world, "level1").empty());
	REQUIRE(game::AdoptSceneMaze(game.maze.world).empty());

	const auto& map = game.maze.world.resources.Get<game::TileMap>();
	CHECK(map.Width() == 28);
	CHECK(map.Find(game::Tile::PlayerSpawn).size() == 1);

	game.Update(3);
	CHECK(gametest::StageOf(game.maze) != gametest::Stage::Title);

	// The scene's tiles are the maze: no second set was spawned beside them.
	CHECK(game.Count<game::Wall>() == map.Find(game::Tile::Wall).size());
	CHECK(game.Count<game::Pellet>() == map.Find(game::Tile::Pellet).size());
	CHECK(game.Count<game::PowerPellet>() == map.Find(game::Tile::PowerPellet).size());
	CHECK(game.Count<game::Door>() == map.Find(game::Tile::Door).size());
	CHECK(game.Count<game::PlayerControlled>() == 1);
	CHECK(game.Count<game::Enemy>() == map.Find(game::Tile::EnemySpawn).size());
	CHECK(game.maze.world.resources.Get<game::EnemyHouse>().house.has_value());

	// The HUD is the scene's, not a second one: three labels and five fields.
	CHECK(game.Count<game::HudLabel>() == 3);
	CHECK(game.Count<game::HudText>() == 5);
	CHECK(game.Count<engine::graphics::Text>() == 8);
}

TEST_CASE("the scene's maze goes away when the game returns to the title")
{
	LevelGame game;
	REQUIRE_FALSE(game::SpawnScene(game.maze.world, "level1").empty());
	REQUIRE(game::AdoptSceneMaze(game.maze.world).empty());
	game.Update(3);
	REQUIRE(game.Count<game::Wall>() > 0);

	game.maze.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(game::Screen::Title);
	game.Update(2);
	CHECK(game.Count<game::Wall>() == 0);
	CHECK(game.Count<game::MazeLayout>() == 0);
	CHECK(game.Count<game::HudText>() == 0);
	CHECK(game.Count<game::HudLabel>() == 0);
	CHECK_FALSE(game::HasSceneMaze(game.maze.world.registry));
}

TEST_CASE("a maze scene that is not valid is not adopted")
{
	LevelGame game;
	REQUIRE_FALSE(game::SpawnScene(game.maze.world, "level1").empty());
	Entity player = Entity::Null;
	game.maze.world.registry.Each<game::PlayerStart>([&](Entity entity, const game::PlayerStart&)
	{
		player = entity;
	});
	game.maze.world.registry.Destroy(player);

	CHECK(game::AdoptSceneMaze(game.maze.world).find("no player spawn") != std::string::npos);
	CHECK(game.maze.world.resources.Get<game::TileMap>().Width() == 7); // the game's own maze is kept
	CHECK_FALSE(game.maze.world.resources.Get<game::GameWorld>().fromScene);
}

TEST_CASE("scoping a scene's HUD leaves other texts alone")
{
	LevelGame game;
	REQUIRE_FALSE(game::SpawnScene(game.maze.world, "level1").empty());
	auto& registry = game.maze.world.registry;
	const Entity stray = registry.Create();
	registry.Add<engine::graphics::Text>(stray);
	REQUIRE(game::AdoptSceneMaze(game.maze.world).empty());
	game.Update(3);

	CHECK_FALSE(registry.HasComponent<engine::ecs::DespawnOnEnter<game::Screen>>(stray));
	CHECK(game.Count<game::HudText>() == 5);
}
