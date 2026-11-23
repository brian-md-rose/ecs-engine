#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "components.hpp"
#include "core/json.hpp"
#include "ecs/entity.hpp"
#include "ecs/scene.hpp"
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

TEST_CASE("A scene with settings sets them on the world, and one without leaves them alone")
{
	LevelGame game;
	auto& world = game.maze.world;
	auto& scenes = world.resources.Get<game::GameScenes>();
	const auto parsed = engine::core::ParseJson(
	    R"({"format": 1, "entities": [], "settings": {"camera": {"x": 3, "y": 4, "zoom": 2}, "background": [9, 8, 7]}})");
	REQUIRE(parsed.value.has_value());
	scenes.files["lookscene"] = *parsed.value;
	scenes.files["plainscene"] = *engine::core::ParseJson(R"({"format": 1, "entities": []})").value;

	static_cast<void>(game::SpawnScene(world, "lookscene"));
	const auto* settings = world.resources.TryGet<engine::ecs::SceneSettings>();
	REQUIRE(settings != nullptr);
	REQUIRE(settings->camera.has_value());
	CHECK(settings->camera->zoom == 2);
	REQUIRE(settings->background.has_value());
	CHECK((*settings->background)[0] == 9);

	static_cast<void>(game::SpawnScene(world, "plainscene"));
	CHECK(world.resources.Get<engine::ecs::SceneSettings>().camera.has_value()); // not cleared by a scene without
}

TEST_CASE("Play from here starts the player on the chosen cell of the scene's maze")
{
	LevelGame game;
	REQUIRE_FALSE(game::SpawnScene(game.maze.world, "level1").empty());
	REQUIRE(game::AdoptSceneMaze(game.maze.world).empty());
	const auto& map = game.maze.world.resources.Get<game::TileMap>();
	const std::vector<game::TilePosition> pellets = map.Find(game::Tile::Pellet);
	REQUIRE_FALSE(pellets.empty());
	const game::TilePosition target = pellets.front();
	const std::vector<game::TilePosition> oldStart = map.Find(game::Tile::PlayerSpawn);
	REQUIRE(oldStart.size() == 1);

	REQUIRE(
	    game::PlacePlayerStart(game.maze.world, static_cast<int>(target.column), static_cast<int>(target.row)).empty());
	const auto& moved = game.maze.world.resources.Get<game::TileMap>();
	REQUIRE(moved.Find(game::Tile::PlayerSpawn).size() == 1);
	CHECK(moved.Find(game::Tile::PlayerSpawn).front() == target);
	CHECK(moved.At(oldStart.front()) == game::Tile::Empty);

	game.Update(3);
	std::size_t players = 0;
	game.maze.world.registry.Each<game::PlayerControlled, game::Spawn>(
	    [&](Entity, const game::PlayerControlled&, const game::Spawn& spawn)
	{
		++players;
		CHECK(spawn.tile == target);
	});
	CHECK(players == 1);
}

TEST_CASE("Play from here refuses a wall, a door, an enemy start and a cell outside the maze")
{
	LevelGame game;
	REQUIRE_FALSE(game::SpawnScene(game.maze.world, "level1").empty());
	REQUIRE(game::AdoptSceneMaze(game.maze.world).empty());
	const auto& map = game.maze.world.resources.Get<game::TileMap>();
	const game::TilePosition wall = map.Find(game::Tile::Wall).front();
	const game::TilePosition enemy = map.Find(game::Tile::EnemySpawn).front();
	CHECK_FALSE(
	    game::PlacePlayerStart(game.maze.world, static_cast<int>(wall.column), static_cast<int>(wall.row)).empty());
	CHECK_FALSE(
	    game::PlacePlayerStart(game.maze.world, static_cast<int>(enemy.column), static_cast<int>(enemy.row)).empty());
	CHECK_FALSE(game::PlacePlayerStart(game.maze.world, -1, 0).empty());
	CHECK_FALSE(game::PlacePlayerStart(game.maze.world, 999, 0).empty());
	CHECK(game.maze.world.resources.Get<game::TileMap>().Find(game::Tile::PlayerSpawn).size() == 1); // unchanged
}
