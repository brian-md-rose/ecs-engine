#include <catch2/catch_test_macros.hpp>

#include "components.hpp"
#include "direction.hpp"

namespace
{
	using game::Direction;
	using game::GridMover;
} // namespace

TEST_CASE("Opposite swaps up with down and left with right", "[game][direction]")
{
	CHECK(game::Opposite(Direction::Up) == Direction::Down);
	CHECK(game::Opposite(Direction::Down) == Direction::Up);
	CHECK(game::Opposite(Direction::Left) == Direction::Right);
	CHECK(game::Opposite(Direction::Right) == Direction::Left);
	CHECK(game::Opposite(Direction::None) == Direction::None);
}

TEST_CASE("Directions know their axis and sign", "[game][direction]")
{
	CHECK(game::IsHorizontal(Direction::Left));
	CHECK(game::IsHorizontal(Direction::Right));
	CHECK_FALSE(game::IsHorizontal(Direction::Up));
	CHECK_FALSE(game::IsHorizontal(Direction::None));
	CHECK(game::IsVertical(Direction::Up));
	CHECK(game::IsVertical(Direction::Down));
	CHECK_FALSE(game::IsVertical(Direction::Right));
	CHECK_FALSE(game::IsVertical(Direction::None));

	CHECK(game::Sign(Direction::Right) == 1.0f);
	CHECK(game::Sign(Direction::Down) == 1.0f);
	CHECK(game::Sign(Direction::Left) == -1.0f);
	CHECK(game::Sign(Direction::Up) == -1.0f);
	CHECK(game::Sign(Direction::None) == 0.0f);
}

TEST_CASE("A new GridMover stands still on the centre of the top-left tile", "[game][gridmovement]")
{
	const GridMover mover;
	CHECK(mover.tile == game::TilePosition{.column = 0, .row = 0});
	CHECK(mover.offset == 0.0f);
	CHECK(mover.direction == Direction::None);
	CHECK(mover.buffered == Direction::None);
	CHECK(mover.speed == 0.0f);
}

TEST_CASE("A GridMover can be placed on a tile with a speed", "[game][gridmovement]")
{
	const GridMover mover{.tile = {.column = 3, .row = 5}, .speed = 6.0f};
	CHECK(mover.tile == game::TilePosition{.column = 3, .row = 5});
	CHECK(mover.offset == 0.0f);
	CHECK(mover.direction == Direction::None);
	CHECK(mover.speed == 6.0f);
}
