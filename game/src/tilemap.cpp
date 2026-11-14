#include "tilemap.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

#include "core/assert.hpp"

namespace game
{
	namespace
	{
		[[nodiscard]] std::optional<Tile> ToTile(char character)
		{
			switch (character)
			{
			case ' ':
				return Tile::Empty;
			case '#':
				return Tile::Wall;
			case '.':
				return Tile::Pellet;
			case 'o':
				return Tile::PowerPellet;
			case 'P':
				return Tile::PlayerSpawn;
			case 'E':
				return Tile::EnemySpawn;
			case '-':
				return Tile::Door;
			case 'T':
				return Tile::Tunnel;
			default:
				return std::nullopt;
			}
		}

		// 'x' for printable characters; a code for tabs and the like, which would be invisible.
		[[nodiscard]] std::string Describe(char character)
		{
			const auto code = static_cast<unsigned char>(character);
			if (code >= 0x20 && code < 0x7F)
			{
				return std::string{'\'', character, '\''};
			}
			constexpr char kHexDigits[] = "0123456789ABCDEF";
			return std::string{"character 0x"} + kHexDigits[code >> 4] + kHexDigits[code & 0x0F];
		}

		// One entry per line, without the line ending (LF or CRLF). Blank lines at the end are
		// dropped, including the empty "line" after a final newline.
		[[nodiscard]] std::vector<std::string_view> SplitLines(std::string_view text)
		{
			std::vector<std::string_view> lines;
			std::size_t start = 0;
			while (true)
			{
				const std::size_t end = text.find('\n', start);
				std::string_view line = text.substr(start, end == std::string_view::npos ? end : end - start);
				if (!line.empty() && line.back() == '\r')
				{
					line.remove_suffix(1);
				}
				lines.push_back(line);
				if (end == std::string_view::npos) break;
				start = end + 1;
			}
			while (!lines.empty() && lines.back().empty())
			{
				lines.pop_back();
			}
			return lines;
		}

		[[nodiscard]] TileMapResult Fail(std::size_t line, std::size_t column, std::string message)
		{
			return {.map = std::nullopt, .error = {.line = line, .column = column, .message = std::move(message)}};
		}

		// Rule 5: a tunnel sits on one edge, not a corner, with a tunnel facing it on the
		// opposite edge. Returns the message for a broken rule, or an empty string.
		[[nodiscard]] std::string CheckTunnel(const TileMap& map, TilePosition position)
		{
			const std::size_t lastColumn = map.Width() - 1;
			const std::size_t lastRow = map.Height() - 1;
			const bool onLeftOrRight = position.column == 0 || position.column == lastColumn;
			const bool onTopOrBottom = position.row == 0 || position.row == lastRow;

			if (onLeftOrRight && onTopOrBottom)
			{
				return "tunnel in a corner; a tunnel goes on one edge, facing a tunnel on the opposite edge";
			}
			if (onLeftOrRight)
			{
				const bool onLeft = position.column == 0;
				const TilePosition pair{.column = onLeft ? lastColumn : 0, .row = position.row};
				if (map.At(pair) == Tile::Tunnel) return {};
				return onLeft ? "tunnel on the left edge has no pair on the right edge"
				              : "tunnel on the right edge has no pair on the left edge";
			}
			if (onTopOrBottom)
			{
				const bool onTop = position.row == 0;
				const TilePosition pair{.column = position.column, .row = onTop ? lastRow : 0};
				if (map.At(pair) == Tile::Tunnel) return {};
				return onTop ? "tunnel on the top edge has no pair on the bottom edge"
				             : "tunnel on the bottom edge has no pair on the top edge";
			}
			return "tunnel is not on an edge of the maze";
		}
	} // namespace

	TileMap::TileMap(std::size_t columns, std::size_t rows, std::vector<Tile> grid)
	    : width(columns), height(rows), tiles(std::move(grid))
	{
		ENGINE_VERIFY(tiles.size() == width * height, "a TileMap needs exactly width * height tiles");
	}

	Tile TileMap::At(TilePosition position) const
	{
		ENGINE_ASSERT(Contains(position), "tile position outside the map; check Contains first");
		return tiles[position.row * width + position.column];
	}

	std::vector<TilePosition> TileMap::Find(Tile tile) const
	{
		std::vector<TilePosition> found;
		for (std::size_t i = 0; i < tiles.size(); ++i)
		{
			if (tiles[i] == tile)
			{
				found.push_back({.column = i % width, .row = i / width});
			}
		}
		return found;
	}

	TileMapResult ParseTileMap(std::string_view text)
	{
		const std::vector<std::string_view> lines = SplitLines(text);
		if (lines.empty())
		{
			return Fail(0, 0, "maze is empty");
		}

		const std::size_t width = lines.front().size();
		const std::size_t height = lines.size();
		std::vector<Tile> tiles;
		tiles.reserve(width * height);
		std::optional<TilePosition> playerSpawn;
		bool hasEnemySpawn = false;

		for (std::size_t row = 0; row < height; ++row)
		{
			const std::string_view line = lines[row];
			const std::size_t lineNumber = row + 1;
			for (std::size_t column = 0; column < line.size(); ++column)
			{
				const std::optional<Tile> tile = ToTile(line[column]);
				if (!tile)
				{
					return Fail(lineNumber, column + 1, "unknown tile " + Describe(line[column]));
				}
				if (*tile == Tile::PlayerSpawn)
				{
					if (playerSpawn)
					{
						return Fail(lineNumber, column + 1,
						            "second player spawn 'P'; the first is at line " +
						                std::to_string(playerSpawn->row + 1) + ", column " +
						                std::to_string(playerSpawn->column + 1));
					}
					playerSpawn = TilePosition{.column = column, .row = row};
				}
				hasEnemySpawn = hasEnemySpawn || *tile == Tile::EnemySpawn;
				tiles.push_back(*tile);
			}
			if (line.size() != width)
			{
				// Point at the first missing tile, or the first one too many.
				return Fail(lineNumber, std::min(line.size(), width) + 1,
				            "row " + std::to_string(lineNumber) + " has " + std::to_string(line.size()) +
				                " tiles; expected " + std::to_string(width) + " like row 1");
			}
		}

		TileMap map(width, height, std::move(tiles));
		for (const TilePosition& tunnel : map.Find(Tile::Tunnel))
		{
			std::string message = CheckTunnel(map, tunnel);
			if (!message.empty())
			{
				return Fail(tunnel.row + 1, tunnel.column + 1, std::move(message));
			}
		}
		if (!playerSpawn)
		{
			return Fail(0, 0, "no player spawn 'P'");
		}
		if (!hasEnemySpawn)
		{
			return Fail(0, 0, "no enemy spawn 'E'");
		}
		return {.map = std::move(map), .error = {}};
	}

	TileMapResult LoadTileMap(const std::filesystem::path& file)
	{
		// Binary, so CRLF reaches the parser unchanged on every platform; it handles both.
		std::ifstream stream(file, std::ios::binary);
		if (!stream)
		{
			return Fail(0, 0, "cannot open the file");
		}
		const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		if (stream.bad())
		{
			return Fail(0, 0, "cannot read the file");
		}
		return ParseTileMap(text);
	}

	std::string FormatError(std::string_view source, const TileMapError& error)
	{
		std::string result{source};
		if (error.line != 0)
		{
			result += ':' + std::to_string(error.line) + ':' + std::to_string(error.column);
		}
		return result + ": " + error.message;
	}
} // namespace game
