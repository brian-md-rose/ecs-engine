#include "editor/gridtool.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace engine::editor
{
	std::optional<GridCell> CellAt(const GridInfo& grid, graphics::Position picture)
	{
		if (grid.columns <= 0 || grid.rows <= 0 || grid.cellWidth <= 0 || grid.cellHeight <= 0) return std::nullopt;
		const float column = std::floor((picture.x - grid.x) / grid.cellWidth);
		const float row = std::floor((picture.y - grid.y) / grid.cellHeight);
		if (column < 0 || row < 0 || column >= static_cast<float>(grid.columns) || row >= static_cast<float>(grid.rows))
			return std::nullopt;
		return GridCell{.column = static_cast<int>(column), .row = static_cast<int>(row)};
	}

	std::vector<GridCell> CellLine(GridCell a, GridCell b)
	{
		// Bresenham, so the cells are connected side to side or corner to corner.
		std::vector<GridCell> cells;
		const int dx = std::abs(b.column - a.column);
		const int dy = -std::abs(b.row - a.row);
		const int stepX = a.column < b.column ? 1 : -1;
		const int stepY = a.row < b.row ? 1 : -1;
		int error = dx + dy;
		GridCell at = a;
		for (;;)
		{
			cells.push_back(at);
			if (at == b) break;
			const int doubled = 2 * error;
			if (doubled >= dy)
			{
				error += dy;
				at.column += stepX;
			}
			if (doubled <= dx)
			{
				error += dx;
				at.row += stepY;
			}
		}
		return cells;
	}

	std::vector<GridCell> ShapeCells(GridShape shape, GridCell a, GridCell b)
	{
		if (shape == GridShape::Line) return CellLine(a, b);
		if (shape != GridShape::Rectangle && shape != GridShape::FilledRectangle) return {};
		const int left = std::min(a.column, b.column);
		const int right = std::max(a.column, b.column);
		const int top = std::min(a.row, b.row);
		const int bottom = std::max(a.row, b.row);
		const bool filled = shape == GridShape::FilledRectangle;
		std::vector<GridCell> cells;
		for (int row = top; row <= bottom; ++row)
		{
			for (int column = left; column <= right; ++column)
			{
				const bool edge = row == top || row == bottom || column == left || column == right;
				if (filled || edge) cells.push_back({column, row});
			}
		}
		return cells;
	}

	std::vector<GridCell> FloodCells(const GridInfo& grid, GridCell start, const std::function<int(GridCell)>& kindOf)
	{
		const auto inside = [&](GridCell cell)
		{
			return cell.column >= 0 && cell.row >= 0 && cell.column < grid.columns && cell.row < grid.rows;
		};
		if (!inside(start)) return {};
		const int wanted = kindOf(start);
		std::vector<GridCell> found;
		std::vector<bool> seen(static_cast<std::size_t>(grid.columns) * static_cast<std::size_t>(grid.rows), false);
		const auto index = [&](GridCell cell)
		{
			return static_cast<std::size_t>(cell.row) * static_cast<std::size_t>(grid.columns) +
			       static_cast<std::size_t>(cell.column);
		};
		std::vector<GridCell> pending{start};
		seen[index(start)] = true;
		while (!pending.empty())
		{
			const GridCell cell = pending.back();
			pending.pop_back();
			found.push_back(cell);
			for (const GridCell next : {GridCell{cell.column - 1, cell.row}, GridCell{cell.column + 1, cell.row},
			                            GridCell{cell.column, cell.row - 1}, GridCell{cell.column, cell.row + 1}})
			{
				if (!inside(next) || seen[index(next)]) continue;
				seen[index(next)] = true;
				if (kindOf(next) == wanted) pending.push_back(next);
			}
		}
		std::sort(found.begin(), found.end(), [](GridCell x, GridCell y)
		{
			return x.row != y.row ? x.row < y.row : x.column < y.column;
		});
		return found;
	}

	GridStamp CaptureStamp(GridCell a, GridCell b, const std::function<int(GridCell)>& kindOf)
	{
		const int left = std::min(a.column, b.column);
		const int top = std::min(a.row, b.row);
		GridStamp stamp;
		stamp.columns = std::abs(b.column - a.column) + 1;
		stamp.rows = std::abs(b.row - a.row) + 1;
		stamp.kinds.reserve(static_cast<std::size_t>(stamp.columns) * static_cast<std::size_t>(stamp.rows));
		for (int row = 0; row < stamp.rows; ++row)
			for (int column = 0; column < stamp.columns; ++column)
				stamp.kinds.push_back(kindOf({left + column, top + row}));
		return stamp;
	}

	std::vector<StampedCell> StampCells(const GridStamp& stamp, const GridInfo& grid, GridCell at)
	{
		std::vector<StampedCell> cells;
		for (int row = 0; row < stamp.rows; ++row)
		{
			for (int column = 0; column < stamp.columns; ++column)
			{
				const GridCell cell{at.column + column, at.row + row};
				if (cell.column < 0 || cell.row < 0 || cell.column >= grid.columns || cell.row >= grid.rows) continue;
				const auto index = static_cast<std::size_t>(row) * static_cast<std::size_t>(stamp.columns) +
				                   static_cast<std::size_t>(column);
				if (index < stamp.kinds.size()) cells.push_back({cell, stamp.kinds[index]});
			}
		}
		return cells;
	}
} // namespace engine::editor
