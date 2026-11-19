#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ui/focusnav.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::input::PadDirection;
	using engine::ui::FocusCandidate;
	using engine::ui::PickFocusNeighbour;

	// A 3 x 2 grid of buttons, 60 apart: index = row * 3 + column.
	[[nodiscard]] std::vector<FocusCandidate> Grid()
	{
		std::vector<FocusCandidate> grid;
		for (int row = 0; row < 2; ++row)
		{
			for (int column = 0; column < 3; ++column)
			{
				grid.push_back({.entity = Entity::Null,
				                .centre = {static_cast<float>(column) * 60.0f, static_cast<float>(row) * 60.0f}});
			}
		}
		return grid;
	}
} // namespace

TEST_CASE("Focus moves to the neighbour in the direction pushed", "[ui][focus]")
{
	const std::vector<FocusCandidate> grid = Grid();
	CHECK(PickFocusNeighbour(grid, 0, PadDirection::Right) == 1);
	CHECK(PickFocusNeighbour(grid, 1, PadDirection::Right) == 2);
	CHECK(PickFocusNeighbour(grid, 4, PadDirection::Left) == 3);
	CHECK(PickFocusNeighbour(grid, 1, PadDirection::Down) == 4);
	CHECK(PickFocusNeighbour(grid, 5, PadDirection::Up) == 2);
}

TEST_CASE("Focus stays put at the edge, and does not wrap", "[ui][focus]")
{
	const std::vector<FocusCandidate> grid = Grid();
	CHECK_FALSE(PickFocusNeighbour(grid, 2, PadDirection::Right).has_value());
	CHECK_FALSE(PickFocusNeighbour(grid, 0, PadDirection::Left).has_value());
	CHECK_FALSE(PickFocusNeighbour(grid, 0, PadDirection::Up).has_value());
	CHECK_FALSE(PickFocusNeighbour(grid, 3, PadDirection::Down).has_value());
}

TEST_CASE("A candidate in line beats a nearer one off to the side", "[ui][focus]")
{
	// From the origin, pushing down: one straight below at 100, one nearly beside it at 30 down and 40 right.
	const std::vector<FocusCandidate> candidates = {
	    {.entity = Entity::Null, .centre = {0.0f, 0.0f}},
	    {.entity = Entity::Null, .centre = {40.0f, 30.0f}},
	    {.entity = Entity::Null, .centre = {0.0f, 100.0f}},
	};
	CHECK(PickFocusNeighbour(candidates, 0, PadDirection::Down) == 2);
}

TEST_CASE("Things level with the focus are not up or down from it", "[ui][focus]")
{
	const std::vector<FocusCandidate> candidates = {
	    {.entity = Entity::Null, .centre = {0.0f, 50.0f}},
	    {.entity = Entity::Null, .centre = {80.0f, 50.0f}},
	};
	CHECK_FALSE(PickFocusNeighbour(candidates, 0, PadDirection::Down).has_value());
	CHECK_FALSE(PickFocusNeighbour(candidates, 0, PadDirection::Up).has_value());
	CHECK(PickFocusNeighbour(candidates, 0, PadDirection::Right) == 1);
}

TEST_CASE("A bad starting index or an empty list gives nothing", "[ui][focus]")
{
	const std::vector<FocusCandidate> grid = Grid();
	CHECK_FALSE(PickFocusNeighbour(grid, 99, PadDirection::Right).has_value());
	CHECK_FALSE(PickFocusNeighbour({}, 0, PadDirection::Right).has_value());
}
