#include <catch2/catch_test_macros.hpp>

#include "grid/direction.hpp"

namespace
{
	using engine::grid::Direction;
} // namespace

TEST_CASE("Opposite swaps up with down and left with right", "[grid][direction]")
{
	CHECK(engine::grid::Opposite(Direction::Up) == Direction::Down);
	CHECK(engine::grid::Opposite(Direction::Down) == Direction::Up);
	CHECK(engine::grid::Opposite(Direction::Left) == Direction::Right);
	CHECK(engine::grid::Opposite(Direction::Right) == Direction::Left);
	CHECK(engine::grid::Opposite(Direction::None) == Direction::None);
}

TEST_CASE("Directions know their axis and sign", "[grid][direction]")
{
	CHECK(engine::grid::IsHorizontal(Direction::Left));
	CHECK(engine::grid::IsHorizontal(Direction::Right));
	CHECK_FALSE(engine::grid::IsHorizontal(Direction::Up));
	CHECK_FALSE(engine::grid::IsHorizontal(Direction::None));
	CHECK(engine::grid::IsVertical(Direction::Up));
	CHECK(engine::grid::IsVertical(Direction::Down));
	CHECK_FALSE(engine::grid::IsVertical(Direction::Right));
	CHECK_FALSE(engine::grid::IsVertical(Direction::None));

	CHECK(engine::grid::Sign(Direction::Right) == 1.0f);
	CHECK(engine::grid::Sign(Direction::Down) == 1.0f);
	CHECK(engine::grid::Sign(Direction::Left) == -1.0f);
	CHECK(engine::grid::Sign(Direction::Up) == -1.0f);
	CHECK(engine::grid::Sign(Direction::None) == 0.0f);
}
