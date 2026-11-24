#include <catch2/catch_test_macros.hpp>

#include <vector>

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

using engine::editor::CaptureStamp;
using engine::editor::FloodCells;
using engine::editor::GridShape;
using engine::editor::GridStamp;
using engine::editor::ShapeCells;
using engine::editor::StampCells;

TEST_CASE("Rectangles and lines cover the cells between two corners", "[editor][grid]")
{
	// Either corner order gives the same cells; the outline leaves the middle out.
	CHECK(ShapeCells(GridShape::FilledRectangle, {2, 1}, {0, 0}) ==
	      std::vector<GridCell>{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}});
	CHECK(ShapeCells(GridShape::Rectangle, {0, 0}, {2, 2}) ==
	      std::vector<GridCell>{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {2, 1}, {0, 2}, {1, 2}, {2, 2}});
	CHECK(ShapeCells(GridShape::Rectangle, {1, 1}, {1, 1}) == std::vector<GridCell>{{1, 1}});
	CHECK(ShapeCells(GridShape::Rectangle, {0, 0}, {1, 3}).size() == 8); // a 2 wide rectangle is all outline
	CHECK(ShapeCells(GridShape::Line, {0, 0}, {2, 0}) == std::vector<GridCell>{{0, 0}, {1, 0}, {2, 0}});
	CHECK(ShapeCells(GridShape::Freehand, {0, 0}, {2, 0}).empty());
	CHECK(ShapeCells(GridShape::Fill, {0, 0}, {2, 0}).empty());
}

TEST_CASE("Flood fill takes the connected cells that hold the same thing", "[editor][grid]")
{
	// 0 = empty (-1), 1 = a wall:   . . # .
	//                               . # # .
	//                               . . . .
	const std::vector<int> cells{-1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1};
	const GridInfo grid{.columns = 4, .rows = 3};
	const auto kindOf = [&](GridCell cell)
	{
		return cells[static_cast<std::size_t>(cell.row * 4 + cell.column)];
	};

	// The empty cells are one region (they go round the wall), listed row by row.
	const std::vector<GridCell> empty = FloodCells(grid, {0, 0}, kindOf);
	CHECK(empty.size() == 9);
	CHECK(empty.front() == GridCell{0, 0});
	CHECK(empty.back() == GridCell{3, 2});

	CHECK(FloodCells(grid, {2, 0}, kindOf) == std::vector<GridCell>{{2, 0}, {1, 1}, {2, 1}}); // the wall cells
	// Any cell of a region gives the same region, and cells that only touch at a corner are not connected.
	CHECK(FloodCells(grid, {3, 0}, kindOf).size() == 9);
	const auto diagonal = [](GridCell cell)
	{
		return cell.column == cell.row ? 1 : -1;
	};
	CHECK(FloodCells(grid, {1, 1}, diagonal) == std::vector<GridCell>{{1, 1}});
	CHECK(FloodCells(grid, {9, 9}, kindOf).empty());
	CHECK(FloodCells(grid, {-1, 0}, kindOf).empty());
}

TEST_CASE("A stamp copies a rectangle of cells and puts it down clipped to the grid", "[editor][grid]")
{
	const GridInfo grid{.columns = 4, .rows = 3};
	const auto kindOf = [](GridCell cell)
	{
		return cell.column == cell.row ? 5 : -1; // a diagonal of kind 5
	};
	const GridStamp stamp = CaptureStamp({2, 2}, {0, 1}, kindOf); // corners in any order
	REQUIRE(stamp.columns == 3);
	REQUIRE(stamp.rows == 2);
	CHECK(stamp.kinds == std::vector<int>{-1, 5, -1, -1, -1, 5});

	const auto placed = StampCells(stamp, grid, {1, 0});
	REQUIRE(placed.size() == 6);
	CHECK(placed[1].cell == GridCell{2, 0});
	CHECK(placed[1].kind == 5);
	CHECK(placed[5].cell == GridCell{3, 1});

	// Cells that fall off the edge are left out, and so is everything for a stamp far away.
	CHECK(StampCells(stamp, grid, {3, 2}).size() == 1);
	CHECK(StampCells(stamp, grid, {-1, 0}).size() == 4);
	CHECK(StampCells(stamp, grid, {9, 9}).empty());
	CHECK(StampCells(GridStamp{}, grid, {0, 0}).empty());
}
