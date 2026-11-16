#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "core/text.hpp"

// The maze as a grid of tiles, read from the text format in docs/maze-format.md.
namespace game
{
	enum class Tile : std::uint8_t
	{
		Empty,
		Wall,
		Pellet,
		PowerPellet,
		PlayerSpawn,
		EnemySpawn,
		Door,
		Tunnel
	};

	// A tile's place in the grid, counted from the top-left tile at column 0, row 0.
	struct TilePosition
	{
		std::size_t column = 0;
		std::size_t row = 0;

		friend bool operator==(const TilePosition&, const TilePosition&) = default;
	};

	// A rectangular grid of tiles, stored row by row. Holds the layout only; MazeGame turns
	// it into entities.
	class TileMap
	{
		std::size_t width = 0;
		std::size_t height = 0;
		std::vector<Tile> tiles;

	public:
		TileMap() = default;
		// grid holds columns * rows tiles, row by row.
		TileMap(std::size_t columns, std::size_t rows, std::vector<Tile> grid);

		[[nodiscard]] std::size_t Width() const noexcept { return width; }
		[[nodiscard]] std::size_t Height() const noexcept { return height; }
		[[nodiscard]] bool Contains(TilePosition position) const noexcept
		{
			return position.column < width && position.row < height;
		}

		// position must be inside the map (see Contains).
		[[nodiscard]] Tile At(TilePosition position) const;

		// Every tile of this kind, in reading order (left to right, top to bottom).
		[[nodiscard]] std::vector<TilePosition> Find(Tile tile) const;
	};

	// Either a map, or the first error found and no map. An error's line and column point at
	// the offending character; both are 0 for errors about the maze as a whole (a missing
	// spawn). engine::core::FormatError writes it out.
	struct TileMapResult
	{
		std::optional<TileMap> map;
		engine::core::ParseError error;
	};

	// Parses a maze and checks the rules in docs/maze-format.md. Pure: no files, no SDL.
	[[nodiscard]] TileMapResult ParseTileMap(std::string_view text);

	// Reads a maze file and parses it. A file that can't be read is an error about the whole
	// maze (line 0).
	[[nodiscard]] TileMapResult LoadTileMap(const std::filesystem::path& file);
} // namespace game
