#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "maze.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::graphics::Position;
	using engine::graphics::Shape;
	using engine::graphics::ShapeKind;
	using engine::graphics::Transform;
	using game::kTileSize;

	constexpr Position kOrigin{16.0f, 24.0f};

	// 14 walls, one each of pellet, power pellet and door.
	[[nodiscard]] game::TileMap SmallMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#####\n"
		                                                "#Po.#\n"
		                                                "#E- #\n"
		                                                "#####\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	struct Spawned
	{
		Transform transform;
		Shape shape;
	};

	template <typename Tag> [[nodiscard]] std::vector<Spawned> Tagged(Registry& registry)
	{
		std::vector<Spawned> found;
		registry.Each<Tag, Transform, Shape>(
		    [&found](Entity, const Tag&, const Transform& transform, const Shape& shape)
		{
			found.push_back({transform, shape});
		});
		return found;
	}
} // namespace

TEST_CASE("TileTopLeft places tiles on an 8-pixel grid from the origin", "[game][maze]")
{
	const Position position = game::TileTopLeft(kOrigin, {.column = 3, .row = 2});
	CHECK(position.x == 16.0f + 3 * kTileSize);
	CHECK(position.y == 24.0f + 2 * kTileSize);
}

TEST_CASE("SpawnMaze creates one entity per wall, pellet, power pellet and door", "[game][maze]")
{
	Registry registry;
	game::SpawnMaze(registry, SmallMaze(), kOrigin);

	CHECK(Tagged<game::Wall>(registry).size() == 14);
	CHECK(Tagged<game::Pellet>(registry).size() == 1);
	CHECK(Tagged<game::PowerPellet>(registry).size() == 1);
	CHECK(Tagged<game::Door>(registry).size() == 1);

	// Spawns and empty floor get nothing.
	int total = 0;
	registry.Each<Transform>([&total](Entity, const Transform&)
	{
		++total;
	});
	CHECK(total == 17);
}

TEST_CASE("SpawnMaze fills wall tiles and centres pellets on theirs", "[game][maze]")
{
	Registry registry;
	game::SpawnMaze(registry, SmallMaze(), kOrigin);

	const std::vector<Spawned> walls = Tagged<game::Wall>(registry);
	REQUIRE_FALSE(walls.empty());
	for (const Spawned& wall : walls)
	{
		CHECK(wall.shape.kind == ShapeKind::Rectangle);
		CHECK(wall.shape.width == kTileSize);
		CHECK(wall.shape.height == kTileSize);
	}

	// The pellet's tile is column 3, row 1; its centre is the tile's centre.
	const std::vector<Spawned> pellets = Tagged<game::Pellet>(registry);
	REQUIRE(pellets.size() == 1);
	const Spawned& pellet = pellets.front();
	CHECK(pellet.shape.kind == ShapeKind::Circle);
	CHECK(pellet.transform.position.x + pellet.shape.radius == kOrigin.x + 3.5f * kTileSize);
	CHECK(pellet.transform.position.y + pellet.shape.radius == kOrigin.y + 1.5f * kTileSize);

	const std::vector<Spawned> power = Tagged<game::PowerPellet>(registry);
	REQUIRE(power.size() == 1);
	CHECK(power.front().shape.radius > pellet.shape.radius);
	CHECK(power.front().transform.position.x + power.front().shape.radius == kOrigin.x + 2.5f * kTileSize);

	// The door is a bar across the middle of column 2, row 2.
	const std::vector<Spawned> doors = Tagged<game::Door>(registry);
	REQUIRE(doors.size() == 1);
	const Spawned& door = doors.front();
	CHECK(door.shape.width == kTileSize);
	CHECK(door.transform.position.x == kOrigin.x + 2 * kTileSize);
	CHECK(door.transform.position.y + door.shape.height / 2.0f == kOrigin.y + 2.5f * kTileSize);
}
