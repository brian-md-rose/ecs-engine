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

		// Empties the cell.
		std::function<void(ecs::World&, GridCell cell)> erase;

		// Called after each frame that changed cells, for what depends on the neighbours (wall pictures).
		std::function<void(ecs::World&)> refresh;

		// What is wrong with the scene as it stands, or an empty string; shown above the Viewport.
		std::function<std::string(ecs::World&)> problems;
	};
} // namespace engine::editor
