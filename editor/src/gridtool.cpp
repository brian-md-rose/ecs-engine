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
} // namespace engine::editor
