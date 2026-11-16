#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "enemytuning.hpp"
#include "grid/direction.hpp"
#include "steering.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::grid::Direction;
	using game::Target;
	using game::TileMap;
	using game::TilePosition;

	// A junction at column 3 of row 3, with the door below it; a tunnel through row 3; a dead
	// end at column 1 of row 5.
	[[nodiscard]] TileMap Grid()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"   // 0
		                                                "#..P..#\n"   // 1
		                                                "#.#.#.#\n"   // 2
		                                                "T.....T\n"   // 3
		                                                "###-###\n"   // 4
		                                                "#..E..#\n"   // 5
		                                                "#######\n"); // 6
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	[[nodiscard]] TilePosition At(std::size_t column, std::size_t row)
	{
		return {.column = column, .row = row};
	}

	[[nodiscard]] Direction Steer(const TileMap& map, TilePosition tile, Direction heading, Target target,
	                              bool throughDoor = false)
	{
		return game::SteerTowards(map, tile, heading, target, throughDoor);
	}

	constexpr TilePosition kJunction{.column = 3, .row = 3};
} // namespace

TEST_CASE("Distances to a target are squared straight lines, also to targets off the maze", "[game][steering]")
{
	CHECK(game::DistanceSquared(At(3, 3), Target{.column = 3, .row = 3}) == 0);
	CHECK(game::DistanceSquared(At(3, 3), Target{.column = 6, .row = 7}) == 25);
	CHECK(game::DistanceSquared(At(0, 0), Target{.column = -2, .row = -1}) == 5);
	CHECK(game::ToTarget(At(4, 9)) == Target{.column = 4, .row = 9});
}

TEST_CASE("At a junction an enemy takes the exit closest to its target", "[game][steering]")
{
	const TileMap map = Grid();
	// Heading right from column 2: it may go on right or turn up (the door is shut).
	CHECK(Steer(map, kJunction, Direction::Right, {.column = 3, .row = 0}) == Direction::Up);
	CHECK(Steer(map, kJunction, Direction::Right, {.column = 6, .row = 3}) == Direction::Right);
	CHECK(Steer(map, kJunction, Direction::Right, {.column = 9, .row = 2}) == Direction::Right);
}

TEST_CASE("An enemy never turns back, even when its target is behind it", "[game][steering]")
{
	const TileMap map = Grid();
	// The target is straight back to the left; up is the closer of the ways it may take.
	CHECK(Steer(map, kJunction, Direction::Right, {.column = 0, .row = 3}) == Direction::Up);

	const game::Exits exits = game::OpenExits(map, kJunction, Direction::Right, false);
	REQUIRE(exits.count == 2);
	CHECK(exits.directions[0] == Direction::Right);
	CHECK(exits.directions[1] == Direction::Up);
}

TEST_CASE("In a corridor an enemy goes on, and at a dead end it turns back", "[game][steering]")
{
	const TileMap map = Grid();
	CHECK(Steer(map, At(2, 3), Direction::Right, {.column = 0, .row = 3}) == Direction::Right);
	CHECK(Steer(map, At(1, 5), Direction::Left, {.column = 0, .row = 5}) == Direction::Right);
}

TEST_CASE("An enemy standing still may leave any open way", "[game][steering]")
{
	const TileMap map = Grid();
	CHECK(Steer(map, kJunction, Direction::None, {.column = 0, .row = 3}) == Direction::Left);
	CHECK(game::OpenExits(map, kJunction, Direction::None, false).count == 3);
}

TEST_CASE("Ties go to the exit earlier in the tie-break order: right, down, left, up", "[game][steering]")
{
	CHECK(game::kTieBreakOrder[0] == Direction::Right);
	CHECK(game::kTieBreakOrder[1] == Direction::Down);
	CHECK(game::kTieBreakOrder[2] == Direction::Left);
	CHECK(game::kTieBreakOrder[3] == Direction::Up);

	const TileMap map = Grid();
	// Every pair of neighbours in the order, each pair one tile from the target.
	CHECK(Steer(map, kJunction, Direction::Up, {.column = 3, .row = 3}) == Direction::Right);   // right, left, up
	CHECK(Steer(map, At(3, 1), Direction::Right, {.column = 4, .row = 2}) == Direction::Right); // right before down
	CHECK(Steer(map, At(3, 1), Direction::Left, {.column = 2, .row = 2}) == Direction::Down);   // down before left
	CHECK(Steer(map, kJunction, Direction::Left, {.column = 2, .row = 2}) == Direction::Left);  // left before up
}

TEST_CASE("Only an enemy allowed through the door can steer through it", "[game][steering]")
{
	const TileMap map = Grid();
	const Target inside{.column = 3, .row = 6};
	CHECK(Steer(map, kJunction, Direction::Left, inside) == Direction::Left);
	CHECK(Steer(map, kJunction, Direction::Left, inside, true) == Direction::Down);
	CHECK(Steer(map, At(3, 5), Direction::None, {.column = 3, .row = 0}, true) == Direction::Up);
}

TEST_CASE("An enemy can steer out through a tunnel", "[game][steering]")
{
	const TileMap map = Grid();
	CHECK(Steer(map, At(6, 3), Direction::Right, {.column = 0, .row = 0}) == Direction::Right);
	CHECK(Steer(map, At(0, 3), Direction::Left, {.column = 6, .row = 0}) == Direction::Left);
}

TEST_CASE("A frightened enemy's random turn is one of its open exits, never back", "[game][steering]")
{
	const TileMap map = Grid();
	// Heading right at the junction, the exits are right then up.
	for (std::uint32_t roll = 0; roll < 8; ++roll)
	{
		const Direction turn = game::SteerRandomly(map, kJunction, Direction::Right, false, roll);
		CHECK(turn == (roll % 2 == 0 ? Direction::Right : Direction::Up));
	}
	CHECK(game::SteerRandomly(map, At(1, 5), Direction::Left, false, 7) == Direction::Right); // dead end
}

TEST_CASE("An enemy shut in on every side stays put", "[game][steering]")
{
	game::TileMapResult result = game::ParseTileMap("#####\n"
	                                                "#P#E#\n"
	                                                "#####\n");
	REQUIRE(result.map.has_value());
	const TileMap& map = *result.map;
	CHECK(game::OpenExits(map, At(3, 1), Direction::Left, true).count == 0);
	CHECK(Steer(map, At(3, 1), Direction::None, {.column = 0, .row = 0}) == Direction::None);
	CHECK(game::SteerRandomly(map, At(3, 1), Direction::Up, false, 3) == Direction::None);
}
