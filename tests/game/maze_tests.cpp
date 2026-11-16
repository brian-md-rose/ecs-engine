#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <vector>

#include "assets.hpp"
#include "components.hpp"
#include "ecs/commandbuffer.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/animation.hpp"
#include "graphics/image.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "maze.hpp"
#include "tilemap.hpp"
#include "walltiles.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::graphics::Image;
	using engine::graphics::Position;
	using engine::graphics::Rgba;
	using engine::graphics::Sprite;
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

	// The game's real clips, so each kind of tile has its own picture.
	[[nodiscard]] const game::SpriteClips& Clips()
	{
		static const std::optional<game::GameAssets> assets =
		    game::LoadGameAssets(std::filesystem::path{GAME_ASSETS_DIR});
		REQUIRE(assets.has_value());
		return assets->clips;
	}

	struct Spawned
	{
		Entity entity;
		Transform transform;
		Sprite sprite;
	};

	template <typename Tag> [[nodiscard]] std::vector<Spawned> Tagged(Registry& registry)
	{
		std::vector<Spawned> found;
		registry.Each<Tag, Transform, Sprite>(
		    [&found](Entity entity, const Tag&, const Transform& transform, const Sprite& sprite)
		{
			found.push_back({entity, transform, sprite});
		});
		return found;
	}

	[[nodiscard]] Image Filled(Rgba colour)
	{
		Image image(8, 8);
		for (int y = 0; y < 8; ++y)
		{
			for (int x = 0; x < 8; ++x)
			{
				image.Set(x, y, colour);
			}
		}
		return image;
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
	game::SpawnMaze(registry, SmallMaze(), kOrigin, Clips());

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

TEST_CASE("SpawnMaze draws every tile with its sprite on the tile", "[game][maze]")
{
	Registry registry;
	const game::TileMap map = SmallMaze();
	game::SpawnMaze(registry, map, kOrigin, Clips());

	// Each wall gets the tile that joins its neighbours: the top-left corner has walls right
	// and below.
	for (const Spawned& wall : Tagged<game::Wall>(registry))
	{
		CHECK(wall.sprite.layer == game::kWallLayer);
	}
	const std::vector<Spawned> walls = Tagged<game::Wall>(registry);
	const auto corner = std::ranges::find_if(walls, [](const Spawned& wall)
	{
		return wall.transform.position.x == kOrigin.x && wall.transform.position.y == kOrigin.y;
	});
	REQUIRE(corner != walls.end());
	CHECK(corner->sprite.source == Clips().walls[game::kWallRight | game::kWallDown].firstFrame);

	// The pellet's tile is column 3, row 1.
	const std::vector<Spawned> pellets = Tagged<game::Pellet>(registry);
	REQUIRE(pellets.size() == 1);
	CHECK(pellets.front().transform.position.x == kOrigin.x + 3 * kTileSize);
	CHECK(pellets.front().transform.position.y == kOrigin.y + 1 * kTileSize);
	CHECK(pellets.front().sprite.source == Clips().pellet.firstFrame);
	CHECK_FALSE(registry.HasComponent<engine::graphics::Animation>(pellets.front().entity));

	// The power pellet pulses.
	const std::vector<Spawned> power = Tagged<game::PowerPellet>(registry);
	REQUIRE(power.size() == 1);
	CHECK(power.front().transform.position.x == kOrigin.x + 2 * kTileSize);
	REQUIRE(registry.HasComponent<engine::graphics::Animation>(power.front().entity));
	CHECK(registry.GetComponent<engine::graphics::Animation>(power.front().entity).clip == Clips().powerPellet);

	// The door is on column 2, row 2.
	const std::vector<Spawned> doors = Tagged<game::Door>(registry);
	REQUIRE(doors.size() == 1);
	CHECK(doors.front().transform.position.x == kOrigin.x + 2 * kTileSize);
	CHECK(doors.front().transform.position.y == kOrigin.y + 2 * kTileSize);
	CHECK(doors.front().sprite.source == Clips().door.firstFrame);
}

TEST_CASE("Pellets and power pellets know their tile", "[game][maze]")
{
	Registry registry;
	game::SpawnMaze(registry, SmallMaze(), kOrigin, Clips());

	std::vector<game::TilePosition> pellets;
	registry.Each<game::Pellet, game::OnTile>([&pellets](Entity, const game::Pellet&, const game::OnTile& on)
	{
		pellets.push_back(on.tile);
	});
	CHECK(pellets == std::vector<game::TilePosition>{{.column = 3, .row = 1}});

	std::vector<game::TilePosition> power;
	registry.Each<game::PowerPellet, game::OnTile>([&power](Entity, const game::PowerPellet&, const game::OnTile& on)
	{
		power.push_back(on.tile);
	});
	CHECK(power == std::vector<game::TilePosition>{{.column = 2, .row = 1}});
}

TEST_CASE("SpawnPellets records the pellets in a command buffer", "[game][maze]")
{
	Registry registry;
	engine::ecs::CommandBuffer commands;
	game::SpawnPellets(registry, commands, SmallMaze(), kOrigin, Clips());
	CHECK(Tagged<game::Pellet>(registry).empty()); // nothing until the commands are applied
	CHECK_FALSE(commands.Empty());

	commands.Apply(registry);
	CHECK(Tagged<game::Pellet>(registry).size() == 1);
	CHECK(Tagged<game::PowerPellet>(registry).size() == 1);
	CHECK(Tagged<game::Wall>(registry).empty()); // pellets only
}

TEST_CASE("WallMask marks the sides with a wall next door", "[game][walls]")
{
	const game::TileMap map = SmallMaze();
	CHECK(game::WallMask(map, {.column = 0, .row = 0}) == (game::kWallRight | game::kWallDown));
	CHECK(game::WallMask(map, {.column = 2, .row = 0}) == (game::kWallLeft | game::kWallRight)); // above floor
	CHECK(game::WallMask(map, {.column = 0, .row = 1}) == (game::kWallUp | game::kWallDown));
	CHECK(game::WallMask(map, {.column = 2, .row = 2}) == game::kWallDown); // the door: only the wall below
}

TEST_CASE("Wall tiles have an edge on every side that doesn't face a wall", "[game][walls]")
{
	constexpr Rgba kCore{1, 1, 1, 255};
	constexpr Rgba kEdge{9, 9, 9, 255};
	const Image core = Filled(kCore);
	std::array<Image, 4> edges{Image(8, 8), Image(8, 8), Image(8, 8), Image(8, 8)};
	for (int i = 0; i < 8; ++i)
	{
		edges[0].Set(i, 0, kEdge); // up
		edges[1].Set(7, i, kEdge); // right
		edges[2].Set(i, 7, kEdge); // down
		edges[3].Set(0, i, kEdge); // left
	}
	const game::WallPieces pieces{.core = &core, .edges = {&edges[0], &edges[1], &edges[2], &edges[3]}};
	const auto tiles = game::BuildWallTiles(pieces);

	// Alone: an edge all round.
	CHECK(tiles[0].At(3, 0) == kEdge);
	CHECK(tiles[0].At(7, 3) == kEdge);
	CHECK(tiles[0].At(3, 7) == kEdge);
	CHECK(tiles[0].At(0, 3) == kEdge);
	CHECK(tiles[0].At(3, 3) == kCore);

	// Walls left and right: edges only top and bottom.
	const Image& corridor = tiles[game::kWallLeft | game::kWallRight];
	CHECK(corridor.At(3, 0) == kEdge);
	CHECK(corridor.At(3, 7) == kEdge);
	CHECK(corridor.At(0, 3) == kCore);
	CHECK(corridor.At(7, 3) == kCore);

	// Walled in on every side: core only.
	const Image& inside = tiles[game::kWallUp | game::kWallRight | game::kWallDown | game::kWallLeft];
	CHECK(inside.At(0, 0) == kCore);
	CHECK(inside.At(7, 7) == kCore);
}
