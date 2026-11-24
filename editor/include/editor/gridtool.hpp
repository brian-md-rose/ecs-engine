#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ecs/world.hpp"
#include "graphics/transform.hpp"

namespace engine::editor
{
	// A grid laid over the scene, in the picture's pixels: columns by rows cells of cellWidth by cellHeight, the
	// top-left one starting at (x, y).
	struct GridInfo
	{
		int columns = 0;
		int rows = 0;
		float x = 0;
		float y = 0;
		float cellWidth = 8.0F;
		float cellHeight = 8.0F;
	};

	struct GridCell
	{
		int column = 0;
		int row = 0;

		friend bool operator==(const GridCell&, const GridCell&) = default;
	};

	// The cell the picture pixel is in, or nothing if it is outside the grid (or the grid has no size).
	[[nodiscard]] std::optional<GridCell> CellAt(const GridInfo& grid, graphics::Position picture);

	// Every cell on the straight line from a to b, both included, in order, so a fast drag leaves no gaps. A cell
	// is listed once.
	[[nodiscard]] std::vector<GridCell> CellLine(GridCell a, GridCell b);

	// How a drag with a brush puts it down: Freehand paints the cells the pointer crosses; Line, Rectangle (outline)
	// and FilledRectangle paint from where the button went down to where it comes up; Fill paints the connected cells
	// that hold what the clicked one does.
	enum class GridShape
	{
		Freehand,
		Line,
		Rectangle,
		FilledRectangle,
		Fill
	};

	// The cells a Line, Rectangle or FilledRectangle drag from a to b covers, each once. Empty for Freehand and Fill,
	// which have no corner to corner shape.
	[[nodiscard]] std::vector<GridCell> ShapeCells(GridShape shape, GridCell a, GridCell b);

	// The cells connected (side to side) to start that hold the same kind of thing, start included, kindOf giving what
	// a cell holds (-1 for empty). Only cells inside the grid are listed; empty if start is outside it.
	[[nodiscard]] std::vector<GridCell> FloodCells(const GridInfo& grid, GridCell start,
	                                               const std::function<int(GridCell)>& kindOf);

	// A rectangle of cells copied to be put down again: what each held (-1 for empty), row by row.
	struct GridStamp
	{
		int columns = 0;
		int rows = 0;
		std::vector<int> kinds;

		[[nodiscard]] bool Empty() const noexcept { return kinds.empty(); }
	};
	struct StampedCell
	{
		GridCell cell;
		int kind = -1;
	};

	// The cells inside the rectangle with corners a and b (in either order), and what they hold.
	[[nodiscard]] GridStamp CaptureStamp(GridCell a, GridCell b, const std::function<int(GridCell)>& kindOf);

	// The stamp's cells as they land with its top-left on at, leaving out any that fall outside the grid.
	[[nodiscard]] std::vector<StampedCell> StampCells(const GridStamp& stamp, const GridInfo& grid, GridCell at);

	// What a game gives the standalone editor to paint a scene made of grid cells (Tangle Run's maze). The editor
	// shows a palette in the Viewport and calls these between frames, on the edit world only, never while playing.
	struct GridTool
	{
		// The grid of the scene in the edit world, or nothing if it is not a grid scene (the tool is then hidden).
		std::function<std::optional<GridInfo>(ecs::World&)> grid;

		// What can be painted, by name; paint's kind is an index into this.
		std::vector<std::string> palette;

		// Puts kind into the cell (replacing what is there; painting what is already there changes nothing).
		std::function<void(ecs::World&, std::size_t kind, GridCell cell)> paint;

		// What the cell holds, as an index into palette, or nothing if it is empty. Fill and Copy need this; a game
		// that leaves it out gets neither.
		std::function<std::optional<std::size_t>(ecs::World&, GridCell cell)> kindAt;

		// Empties the cell.
		std::function<void(ecs::World&, GridCell cell)> erase;

		// Called after each frame that changed cells, for what depends on the neighbours (wall pictures).
		std::function<void(ecs::World&)> refresh;

		// What is wrong with the scene as it stands, or an empty string; shown above the Viewport.
		std::function<std::string(ecs::World&)> problems;
	};
} // namespace engine::editor
