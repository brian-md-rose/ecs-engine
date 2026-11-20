#include <catch2/catch_test_macros.hpp>

#include "editor/gridtool.hpp"

using engine::editor::CellAt;
using engine::editor::CellLine;
using engine::editor::GridCell;
using engine::editor::GridInfo;

TEST_CASE("CellAt finds the cell under a picture pixel", "[editor][grid]")
{
	const GridInfo grid{.columns = 4, .rows = 3, .x = 10.0F, .y = 20.0F, .cellWidth = 8.0F, .cellHeight = 8.0F};

	CHECK(CellAt(grid, {10.0F, 20.0F}) == GridCell{0, 0});
	CHECK(CellAt(grid, {17.9F, 27.9F}) == GridCell{0, 0});
	CHECK(CellAt(grid, {18.0F, 28.0F}) == GridCell{1, 1});
	CHECK(CellAt(grid, {41.9F, 43.9F}) == GridCell{3, 2});

	CHECK_FALSE(CellAt(grid, {9.9F, 20.0F}).has_value());  // left of the grid
	CHECK_FALSE(CellAt(grid, {10.0F, 19.9F}).has_value()); // above it
	CHECK_FALSE(CellAt(grid, {42.0F, 20.0F}).has_value()); // past the last column
	CHECK_FALSE(CellAt(grid, {10.0F, 44.0F}).has_value()); // past the last row
	CHECK_FALSE(CellAt(GridInfo{}, {0.0F, 0.0F}).has_value());
}

TEST_CASE("CellLine lists every cell on the way, in order", "[editor][grid]")
{
	CHECK(CellLine({2, 3}, {2, 3}) == std::vector<GridCell>{{2, 3}});
	CHECK(CellLine({0, 0}, {3, 0}) == std::vector<GridCell>{{0, 0}, {1, 0}, {2, 0}, {3, 0}});
	CHECK(CellLine({3, 0}, {0, 0}) == std::vector<GridCell>{{3, 0}, {2, 0}, {1, 0}, {0, 0}});
	CHECK(CellLine({0, 0}, {0, 2}) == std::vector<GridCell>{{0, 0}, {0, 1}, {0, 2}});
	CHECK(CellLine({0, 0}, {2, 2}) == std::vector<GridCell>{{0, 0}, {1, 1}, {2, 2}});

	// A shallow line: each step moves one column, and the row changes about halfway.
	const std::vector<GridCell> shallow = CellLine({0, 0}, {6, 2});
	REQUIRE(shallow.size() == 7);
	CHECK(shallow.front() == GridCell{0, 0});
	CHECK(shallow.back() == GridCell{6, 2});
	for (std::size_t i = 1; i < shallow.size(); ++i)
	{
		CHECK(shallow[i].column == shallow[i - 1].column + 1);
		const int rowStep = shallow[i].row - shallow[i - 1].row;
		CHECK((rowStep == 0 || rowStep == 1));
	}
}
