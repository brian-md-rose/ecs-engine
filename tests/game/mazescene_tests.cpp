#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "components.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/registry.hpp"
#include "ecs/scene.hpp"
#include "ecs/typeregistry.hpp"
#include "graphics/animation.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "maze.hpp"
#include "mazescene.hpp"
#include "scenes.hpp"
#include "tilemap.hpp"
#include "walltiles.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::graphics::Position;

	constexpr const char* kSmall = "#############\n"
	                               "#o....#....o#\n"
	                               "#.##.###.##.#\n"
	                               "#....#E#....#\n"
	                               "T.##.#-#.##.T\n"
	                               "#.....P.....#\n"
	                               "#o#.#####.#o#\n"
	                               "#...........#\n"
	                               "#############\n";

	game::TileMap Parse(const char* text)
	{
		game::TileMapResult result = game::ParseTileMap(text);
		REQUIRE(result.map.has_value());
		return std::move(*result.map);
	}

	void RequireSameTiles(const game::TileMap& a, const game::TileMap& b)
	{
		REQUIRE(a.Width() == b.Width());
		REQUIRE(a.Height() == b.Height());
		for (std::size_t row = 0; row < a.Height(); ++row)
		{
			for (std::size_t column = 0; column < a.Width(); ++column)
			{
				const game::TilePosition at{.column = column, .row = row};
				INFO("tile " << column << "," << row);
				CHECK(a.At(at) == b.At(at));
			}
		}
	}

	game::SpriteClips Clips()
	{
		game::SpriteClips clips;
		for (std::size_t i = 0; i < clips.walls.size(); ++i)
			clips.walls[i].firstFrame = {.x = static_cast<int>(i) * 8, .y = 0, .width = 8, .height = 8};
		clips.door.firstFrame = {.x = 0, .y = 8, .width = 8, .height = 8};
		clips.pellet.firstFrame = {.x = 8, .y = 8, .width = 8, .height = 8};
		clips.powerPellet.firstFrame = {.x = 16, .y = 8, .width = 8, .height = 8};
		clips.powerPellet.frames = 2;
		return clips;
	}
} // namespace

TEST_CASE("a maze made into scene entities gives the same TileMap back")
{
	const game::TileMap map = Parse(kSmall);
	Registry registry;
	game::SpawnMazeScene(registry, map, {.x = 4.0F, .y = 16.0F});

	const game::TileMapResult built = game::BuildTileMap(registry);
	REQUIRE(built.map.has_value());
	RequireSameTiles(map, *built.map);
}

TEST_CASE("tile entities are drawn at their cell from the layout's origin")
{
	const game::TileMap map = Parse(kSmall);
	Registry registry;
	game::SpawnMazeScene(registry, map, {.x = 4.0F, .y = 16.0F});

	bool found = false;
	registry.Each<game::PlayerStart, game::TileCell, engine::graphics::Transform>(
	    [&](Entity, const game::PlayerStart&, const game::TileCell& cell, const engine::graphics::Transform& at)
	{
		found = true;
		CHECK(cell.column == 6);
		CHECK(cell.row == 5);
		CHECK(at.position.x == 4.0F + 6 * game::kTileSize);
		CHECK(at.position.y == 16.0F + 5 * game::kTileSize);
	});
	CHECK(found);
}

TEST_CASE("a maze scene survives being saved and loaded")
{
	const game::TileMap map = Parse(kSmall);
	engine::ecs::TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	game::RegisterGameComponents(types);

	Registry source;
	game::SpawnMazeScene(source, map, {.x = 0.0F, .y = 16.0F});
	engine::ecs::Issues issues;
	const engine::core::JsonValue json = engine::ecs::SaveScene(source, types, issues);
	REQUIRE(issues.errors.empty());

	Registry loaded;
	const engine::ecs::SceneLoadResult result = engine::ecs::LoadScene(loaded, types, json);
	REQUIRE(result.Ok());
	const game::TileMapResult built = game::BuildTileMap(loaded);
	REQUIRE(built.map.has_value());
	RequireSameTiles(map, *built.map);
}

TEST_CASE("BuildTileMap reports what is wrong with the tiles")
{
	const game::TileMap map = Parse(kSmall);

	SECTION("no layout")
	{
		Registry registry;
		const game::TileMapResult built = game::BuildTileMap(registry);
		CHECK_FALSE(built.map.has_value());
		CHECK(built.error.message.find("no MazeLayout") != std::string::npos);
	}
	SECTION("two layouts")
	{
		Registry registry;
		game::SpawnMazeScene(registry, map, {});
		registry.Add<game::MazeLayout>(registry.Create(), game::MazeLayout{.columns = 3, .rows = 3});
		CHECK_FALSE(game::BuildTileMap(registry).map.has_value());
	}
	SECTION("a tile outside the layout")
	{
		Registry registry;
		game::SpawnMazeScene(registry, map, {});
		const Entity stray = registry.Create();
		registry.Add<game::TileCell>(stray, game::TileCell{.column = 20, .row = 1});
		registry.Add<game::Wall>(stray);
		const game::TileMapResult built = game::BuildTileMap(registry);
		CHECK_FALSE(built.map.has_value());
		CHECK(built.error.message.find("outside the maze") != std::string::npos);
	}
	SECTION("two tiles on one cell")
	{
		Registry registry;
		game::SpawnMazeScene(registry, map, {});
		const Entity twin = registry.Create();
		registry.Add<game::TileCell>(twin, game::TileCell{.column = 0, .row = 0});
		registry.Add<game::Wall>(twin);
		const game::TileMapResult built = game::BuildTileMap(registry);
		CHECK_FALSE(built.map.has_value());
		CHECK(built.error.message.find("same cell") != std::string::npos);
	}
	SECTION("a tile with no kind, or two")
	{
		Registry registry;
		game::SpawnMazeScene(registry, map, {});
		const Entity bare = registry.Create();
		registry.Add<game::TileCell>(bare, game::TileCell{.column = 1, .row = 1 + 6});
		// (1, 7) is a pellet cell in the small maze, so this also checks the kind is reported before the clash.
		CHECK_FALSE(game::BuildTileMap(registry).map.has_value());

		Registry two;
		game::SpawnMazeScene(two, map, {});
		const Entity both = two.Create();
		two.Add<game::TileCell>(both, game::TileCell{.column = 1, .row = 2});
		two.Add<game::Wall>(both);
		two.Add<game::Pellet>(both);
		CHECK_FALSE(game::BuildTileMap(two).map.has_value());
	}
	SECTION("the maze format's own rules still apply")
	{
		Registry registry;
		game::SpawnMazeScene(registry, map, {});
		Entity player = engine::ecs::Entity::Null;
		registry.Each<game::PlayerStart>([&](Entity entity, const game::PlayerStart&)
		{
			player = entity;
		});
		registry.Destroy(player);
		const game::TileMapResult built = game::BuildTileMap(registry);
		CHECK_FALSE(built.map.has_value());
		CHECK(built.error.message.find("no player spawn") != std::string::npos);
	}
}

TEST_CASE("ResolveMazeTiles gives the same pictures as SpawnMaze")
{
	const game::TileMap map = Parse(kSmall);
	const game::SpriteClips clips = Clips();
	const Position origin{.x = 0.0F, .y = 16.0F};

	Registry classic;
	game::SpawnMaze(classic, map, origin, clips);

	Registry scene;
	game::SpawnMazeScene(scene, map, origin);
	REQUIRE(game::ResolveMazeTiles(scene, clips));

	const auto count = [](Registry& registry)
	{
		std::size_t sprites = 0;
		registry.Each<engine::graphics::Sprite, engine::graphics::Transform>(
		    [&](Entity, const engine::graphics::Sprite&, const engine::graphics::Transform&)
		{
			++sprites;
		});
		return sprites;
	};
	CHECK(count(scene) == count(classic));

	// Every drawn tile of the classic maze has a scene entity at the same place with the same source and layer.
	classic.Each<engine::graphics::Sprite, engine::graphics::Transform>(
	    [&](Entity, const engine::graphics::Sprite& expected, const engine::graphics::Transform& at)
	{
		bool match = false;
		scene.Each<engine::graphics::Sprite, engine::graphics::Transform>(
		    [&](Entity, const engine::graphics::Sprite& sprite, const engine::graphics::Transform& where)
		{
			if (where.position.x == at.position.x && where.position.y == at.position.y &&
			    sprite.source.x == expected.source.x && sprite.source.y == expected.source.y &&
			    sprite.layer == expected.layer)
				match = true;
		});
		INFO("at " << at.position.x << "," << at.position.y);
		CHECK(match);
	});

	// Pellets can be eaten (OnTile), and power pellets blink.
	std::size_t pellets = 0;
	scene.Each<game::Pellet, game::OnTile>([&](Entity, const game::Pellet&, const game::OnTile&)
	{
		++pellets;
	});
	CHECK(pellets == map.Find(game::Tile::Pellet).size());
	std::size_t blinking = 0;
	scene.Each<game::PowerPellet, engine::graphics::Animation>(
	    [&](Entity, const game::PowerPellet&, const engine::graphics::Animation&)
	{
		++blinking;
	});
	CHECK(blinking == map.Find(game::Tile::PowerPellet).size());
}

TEST_CASE("ResolveMazeTiles changes nothing when the tiles are not a valid maze")
{
	Registry registry;
	CHECK_FALSE(game::ResolveMazeTiles(registry, Clips()));
}

TEST_CASE("the checked-in maze scene is the same maze as the text file")
{
	const std::filesystem::path assets = GAME_ASSETS_DIR;
	const game::TileMapResult text = game::LoadTileMap(assets / "mazes" / "level1.txt");
	REQUIRE(text.map.has_value());

	engine::ecs::TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	game::RegisterGameComponents(types);
	Registry registry;
	const engine::ecs::SceneLoadResult loaded =
	    engine::ecs::LoadSceneFile(registry, types, assets / "scenes" / "level1.json");
	REQUIRE(loaded.Ok());
	const game::TileMapResult built = game::BuildTileMap(registry);
	REQUIRE(built.map.has_value());
	RequireSameTiles(*text.map, *built.map);
}
