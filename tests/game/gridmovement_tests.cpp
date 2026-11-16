#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>

#include "components.hpp"
#include "graphics/transform.hpp"
#include "grid/direction.hpp"
#include "gridmovement.hpp"
#include "maze.hpp"
#include "tilemap.hpp"

namespace
{
	using Catch::Matchers::WithinAbs;
	using engine::graphics::Position;
	using engine::grid::Direction;
	using game::GridMover;
	using game::TileMap;
	using game::TilePosition;

	constexpr float kSpeed = 4.0f; // tiles per second, so 0.25 s is one tile
	constexpr double kMargin = 1e-4;

	// Junctions on row 3 at columns 1, 4 and 7; a tunnel through row 4; the door below P.
	[[nodiscard]] TileMap Maze()
	{
		game::TileMapResult result = game::ParseTileMap("#########\n"   // 0
		                                                "#.......#\n"   // 1
		                                                "#.##.##.#\n"   // 2
		                                                "#.......#\n"   // 3
		                                                "T...P...T\n"   // 4
		                                                "#.##-##.#\n"   // 5
		                                                "#.E.....#\n"   // 6
		                                                "#########\n"); // 7
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	// A tunnel through column 1, top to bottom.
	[[nodiscard]] TileMap VerticalTunnelMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#T###\n"
		                                                "#.P.#\n"
		                                                "#E..#\n"
		                                                "#T###\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	[[nodiscard]] TilePosition At(std::size_t column, std::size_t row)
	{
		return {.column = column, .row = row};
	}

	[[nodiscard]] GridMover Mover(TilePosition tile, Direction direction, Direction buffered, float offset = 0.0f)
	{
		return {.tile = tile, .offset = offset, .direction = direction, .buffered = buffered, .speed = kSpeed};
	}

	// Steps the mover over the given number of tiles of path at kSpeed, in one step.
	[[nodiscard]] GridMover Travel(const GridMover& mover, const TileMap& map, float tiles)
	{
		return game::StepGridMover(mover, map, tiles / kSpeed);
	}

	void CheckAt(const GridMover& mover, TilePosition tile, float offset)
	{
		CAPTURE(mover.tile.column, mover.tile.row, mover.offset);
		CHECK(mover.tile == tile);
		CHECK_THAT(mover.offset, WithinAbs(offset, kMargin));
	}
} // namespace

TEST_CASE("A new GridMover stands still on the centre of the top-left tile", "[game][gridmovement]")
{
	const GridMover mover;
	CHECK(mover.tile == At(0, 0));
	CHECK(mover.offset == 0.0f);
	CHECK(mover.direction == Direction::None);
	CHECK(mover.buffered == Direction::None);
	CHECK(mover.speed == 0.0f);
}

TEST_CASE("A GridMover can be placed on a tile with a speed", "[game][gridmovement]")
{
	const GridMover mover{.tile = At(3, 5), .speed = 6.0f};
	CHECK(mover.tile == At(3, 5));
	CHECK(mover.offset == 0.0f);
	CHECK(mover.direction == Direction::None);
	CHECK(mover.speed == 6.0f);
}

TEST_CASE("Walls and the door are not walkable; everything else is", "[game][gridmovement]")
{
	using game::Tile;
	CHECK_FALSE(game::IsWalkable(Tile::Wall));
	CHECK_FALSE(game::IsWalkable(Tile::Door));
	CHECK(game::IsWalkable(Tile::Empty));
	CHECK(game::IsWalkable(Tile::Pellet));
	CHECK(game::IsWalkable(Tile::PowerPellet));
	CHECK(game::IsWalkable(Tile::PlayerSpawn));
	CHECK(game::IsWalkable(Tile::EnemySpawn));
	CHECK(game::IsWalkable(Tile::Tunnel));
}

TEST_CASE("Neighbour steps one tile and wraps only through tunnels", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	CHECK(game::Neighbour(maze, At(4, 3), Direction::Up) == At(4, 2));
	CHECK(game::Neighbour(maze, At(4, 3), Direction::Down) == At(4, 4));
	CHECK(game::Neighbour(maze, At(4, 3), Direction::Left) == At(3, 3));
	CHECK(game::Neighbour(maze, At(4, 3), Direction::Right) == At(5, 3));
	CHECK_FALSE(game::Neighbour(maze, At(4, 3), Direction::None).has_value());

	CHECK(game::Neighbour(maze, At(0, 4), Direction::Left) == At(8, 4));
	CHECK(game::Neighbour(maze, At(8, 4), Direction::Right) == At(0, 4));
	CHECK(game::Neighbour(maze, At(0, 4), Direction::Right) == At(1, 4));      // inwards as usual
	CHECK_FALSE(game::Neighbour(maze, At(0, 3), Direction::Left).has_value()); // a wall on the edge
	CHECK_FALSE(game::Neighbour(maze, At(4, 0), Direction::Up).has_value());
	CHECK_FALSE(game::Neighbour(maze, At(9, 0), Direction::Left).has_value()); // off the map

	const TileMap vertical = VerticalTunnelMaze();
	CHECK(game::Neighbour(vertical, At(1, 0), Direction::Up) == At(1, 3));
	CHECK(game::Neighbour(vertical, At(1, 3), Direction::Down) == At(1, 0));
}

TEST_CASE("CanEnter needs a walkable neighbour", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	CHECK(game::CanEnter(maze, At(4, 3), Direction::Up));
	CHECK_FALSE(game::CanEnter(maze, At(3, 3), Direction::Up));   // wall
	CHECK_FALSE(game::CanEnter(maze, At(4, 4), Direction::Down)); // door
	CHECK(game::CanEnter(maze, At(0, 4), Direction::Left));       // tunnel
	CHECK_FALSE(game::CanEnter(maze, At(4, 3), Direction::None));
}

TEST_CASE("A mover goes straight at its speed, across tile edges", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	const GridMover start = Mover(At(2, 3), Direction::Right, Direction::Right);

	CheckAt(Travel(start, maze, 0.25f), At(2, 3), 0.25f);
	CheckAt(Travel(start, maze, 0.75f), At(3, 3), -0.25f); // left tile 2 at its edge
	CheckAt(Travel(start, maze, 1.0f), At(3, 3), 0.0f);
	CheckAt(Travel(start, maze, 2.4f), At(4, 3), 0.4f);
	CheckAt(Travel(start, maze, 2.5f), At(5, 3), -0.5f); // exactly on an edge counts as the next tile

	const GridMover up = Travel(Mover(At(4, 3), Direction::Up, Direction::Up), maze, 0.75f);
	CheckAt(up, At(4, 2), 0.25f); // up is negative, so coming in from below starts at +0.5
	CHECK(up.direction == Direction::Up);
}

TEST_CASE("A mover goes straight through a junction when no turn is asked for", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	const GridMover moved = Travel(Mover(At(3, 3), Direction::Right, Direction::Right), maze, 2.0f);
	CheckAt(moved, At(5, 3), 0.0f);
	CHECK(moved.direction == Direction::Right);
}

TEST_CASE("A mover standing still starts the way it is asked if that way is open", "[game][gridmovement]")
{
	const TileMap maze = Maze();

	const GridMover idle = Travel(Mover(At(1, 1), Direction::None, Direction::None), maze, 1.0f);
	CheckAt(idle, At(1, 1), 0.0f);
	CHECK(idle.direction == Direction::None);

	const GridMover started = Travel(Mover(At(1, 1), Direction::None, Direction::Right), maze, 0.25f);
	CheckAt(started, At(1, 1), 0.25f);
	CHECK(started.direction == Direction::Right);

	const GridMover blocked = Travel(Mover(At(1, 1), Direction::None, Direction::Up), maze, 1.0f);
	CheckAt(blocked, At(1, 1), 0.0f);
	CHECK(blocked.direction == Direction::None);
}

TEST_CASE("A mover stops on the centre of the last tile before a wall", "[game][gridmovement]")
{
	const TileMap maze = Maze();

	const GridMover stopped = Travel(Mover(At(5, 1), Direction::Right, Direction::Right), maze, 10.0f);
	CheckAt(stopped, At(7, 1), 0.0f);
	CHECK(stopped.direction == Direction::None);
	CHECK(stopped.offset == 0.0f);

	// Arriving exactly on the centre stops too.
	const GridMover exact = Travel(Mover(At(5, 1), Direction::Right, Direction::Right), maze, 2.0f);
	CheckAt(exact, At(7, 1), 0.0f);
	CHECK(exact.direction == Direction::None);

	// Then it waits there until asked for an open way.
	CHECK(Travel(stopped, maze, 1.0f).direction == Direction::None);
	GridMover asked = stopped;
	asked.buffered = Direction::Down;
	const GridMover down = Travel(asked, maze, 0.4f);
	CheckAt(down, At(7, 1), 0.4f);
	CHECK(down.direction == Direction::Down);
}

TEST_CASE("The door stops the player", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	const GridMover moved = Travel(Mover(At(4, 3), Direction::Down, Direction::Down), maze, 3.0f);
	CheckAt(moved, At(4, 4), 0.0f);
	CHECK(moved.direction == Direction::None);
}

TEST_CASE("A mover allowed through the door passes it", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	CHECK(game::CanEnter(maze, At(4, 4), Direction::Down, true));
	CHECK_FALSE(game::CanEnter(maze, At(3, 3), Direction::Up, true)); // walls still stop it

	GridMover enemy = Mover(At(4, 4), Direction::Down, Direction::Down);
	enemy.throughDoor = true;
	CheckAt(Travel(enemy, maze, 2.0f), At(4, 6), 0.0f);
}

TEST_CASE("A mover can turn back at any point", "[game][gridmovement]")
{
	const TileMap maze = Maze();

	const GridMover midTile = Travel(Mover(At(2, 3), Direction::Right, Direction::Left, 0.3f), maze, 0.1f);
	CheckAt(midTile, At(2, 3), 0.2f);
	CHECK(midTile.direction == Direction::Left);

	// Just into a new tile, turning back crosses into the tile it came from.
	const GridMover backAcross = Travel(Mover(At(3, 3), Direction::Right, Direction::Left, -0.4f), maze, 0.2f);
	CheckAt(backAcross, At(2, 3), 0.4f);
	CHECK(backAcross.direction == Direction::Left);

	// Vertically too.
	const GridMover vertical = Travel(Mover(At(4, 2), Direction::Up, Direction::Down, -0.25f), maze, 1.0f);
	CheckAt(vertical, At(4, 3), -0.25f);
	CHECK(vertical.direction == Direction::Down);
}

TEST_CASE("A turn asked for early is taken on reaching the junction's centre", "[game][gridmovement]")
{
	const TileMap maze = Maze();

	// Up is asked for in tile 3, which has a wall above; the next junction is tile 4.
	const GridMover turned = Travel(Mover(At(3, 3), Direction::Right, Direction::Up, -0.4f), maze, 1.65f);
	CheckAt(turned, At(4, 3), -0.25f);
	CHECK(turned.direction == Direction::Up);

	// Landing exactly on the centre turns too, ready for the next step.
	const GridMover onCentre = Travel(Mover(At(2, 1), Direction::Right, Direction::Down), maze, 2.0f);
	CheckAt(onCentre, At(4, 1), 0.0f);
	CHECK(onCentre.direction == Direction::Down);

	// The turn waits through tiles that are closed that way.
	const GridMover waited = Travel(Mover(At(2, 1), Direction::Right, Direction::Down, 0.1f), maze, 1.9f);
	CheckAt(waited, At(4, 1), 0.0f);
	CHECK(waited.direction == Direction::Down);
}

TEST_CASE("A turn asked for just past the centre is still taken", "[game][gridmovement]")
{
	const TileMap maze = Maze();

	// 0.2 past the centre, within kTurnTolerance: the distance carries over upwards.
	const GridMover late = Travel(Mover(At(4, 3), Direction::Right, Direction::Up, 0.2f), maze, 0.1f);
	CheckAt(late, At(4, 3), -0.3f);
	CHECK(late.direction == Direction::Up);

	// Exactly at the tolerance still counts.
	const GridMover edge = Travel(Mover(At(4, 3), Direction::Right, Direction::Up, game::kTurnTolerance), maze, 0.0f);
	CHECK(edge.direction == Direction::Up);
	CHECK_THAT(edge.offset, WithinAbs(-game::kTurnTolerance, kMargin));

	// Too late: it goes on, and keeps the turn for the next junction.
	const GridMover tooLate = Travel(Mover(At(4, 3), Direction::Right, Direction::Up, 0.3f), maze, 0.1f);
	CheckAt(tooLate, At(4, 3), 0.4f);
	CHECK(tooLate.direction == Direction::Right);
	CHECK(tooLate.buffered == Direction::Up);

	// Not into a wall.
	const GridMover walled = Travel(Mover(At(3, 3), Direction::Right, Direction::Up, 0.1f), maze, 0.1f);
	CheckAt(walled, At(3, 3), 0.2f);
	CHECK(walled.direction == Direction::Right);

	// Before the centre the tolerance doesn't apply: it turns on reaching the centre.
	const GridMover early = Travel(Mover(At(4, 3), Direction::Right, Direction::Up, -0.2f), maze, 0.1f);
	CheckAt(early, At(4, 3), -0.1f);
	CHECK(early.direction == Direction::Right);
}

TEST_CASE("Leaving through a tunnel comes back through its pair", "[game][gridmovement]")
{
	const TileMap maze = Maze();

	const GridMover left = Travel(Mover(At(1, 4), Direction::Left, Direction::Left), maze, 1.75f);
	CheckAt(left, At(8, 4), 0.25f);
	CHECK(left.direction == Direction::Left);
	CheckAt(Travel(left, maze, 1.25f), At(7, 4), 0.0f);

	const GridMover right = Travel(Mover(At(7, 4), Direction::Right, Direction::Right), maze, 1.75f);
	CheckAt(right, At(0, 4), -0.25f);
	CheckAt(Travel(right, maze, 1.25f), At(1, 4), 0.0f);

	// Turning back inside the tunnel goes back out the way it came in.
	GridMover back = right;
	back.buffered = Direction::Left;
	CheckAt(Travel(back, maze, 0.5f), At(8, 4), 0.25f);

	const TileMap vertical = VerticalTunnelMaze();
	CheckAt(Travel(Mover(At(1, 1), Direction::Up, Direction::Up), vertical, 1.75f), At(1, 3), 0.25f);
	CheckAt(Travel(Mover(At(1, 2), Direction::Down, Direction::Down), vertical, 1.75f), At(1, 0), -0.25f);
}

TEST_CASE("Movement doesn't depend on the frame rate", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	// Right along row 1, down at column 4 (the first tile open below), then 2 tiles down.
	const GridMover start = Mover(At(2, 1), Direction::Right, Direction::Down);

	for (const int framesPerSecond : {1, 7, 30, 60, 144, 240})
	{
		CAPTURE(framesPerSecond);
		GridMover mover = start;
		const float dt = 1.0f / static_cast<float>(framesPerSecond);
		for (int frame = 0; frame < framesPerSecond; ++frame)
		{
			mover = game::StepGridMover(mover, maze, dt);
		}
		CheckAt(mover, At(4, 3), 0.0f);
		CHECK(mover.direction == Direction::Down);
	}

	// Uneven steps adding up to a second get there too.
	GridMover uneven = start;
	for (const float dt : {0.013f, 0.2f, 0.05f, 0.337f, 0.001f, 0.399f})
	{
		uneven = game::StepGridMover(uneven, maze, dt);
	}
	CheckAt(uneven, At(4, 3), 0.0f);
	CHECK(uneven.direction == Direction::Down);
}

TEST_CASE("A zero-length step changes nothing on a plain stretch", "[game][gridmovement]")
{
	const TileMap maze = Maze();
	const GridMover moved = game::StepGridMover(Mover(At(2, 3), Direction::Right, Direction::Right, 0.3f), maze, 0.0f);
	CheckAt(moved, At(2, 3), 0.3f);
	CHECK(moved.direction == Direction::Right);
}

TEST_CASE("MoverTopLeft shifts the tile's corner by the offset along the way it moves", "[game][gridmovement]")
{
	constexpr Position kOrigin{16.0f, 24.0f};
	using game::kTileSize;

	const Position still = game::MoverTopLeft(kOrigin, Mover(At(2, 3), Direction::None, Direction::None));
	CHECK(still.x == 16.0f + 2 * kTileSize);
	CHECK(still.y == 24.0f + 3 * kTileSize);

	const Position right = game::MoverTopLeft(kOrigin, Mover(At(2, 3), Direction::Right, Direction::Right, 0.25f));
	CHECK(right.x == 16.0f + 2.25f * kTileSize);
	CHECK(right.y == 24.0f + 3 * kTileSize);

	const Position up = game::MoverTopLeft(kOrigin, Mover(At(2, 3), Direction::Up, Direction::Up, -0.5f));
	CHECK(up.x == 16.0f + 2 * kTileSize);
	CHECK(up.y == 24.0f + 2.5f * kTileSize);
}
