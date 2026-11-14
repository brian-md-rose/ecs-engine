#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "tilemap.hpp"

namespace
{
	using game::FormatError;
	using game::LoadTileMap;
	using game::ParseTileMap;
	using game::Tile;
	using game::TileMap;
	using game::TileMapError;
	using game::TilePosition;

	// The example from docs/maze-format.md.
	constexpr std::string_view kExampleMaze = "#############\n"
	                                          "#o....#....o#\n"
	                                          "#.##.###.##.#\n"
	                                          "#....#E#....#\n"
	                                          "T.##.#-#.##.T\n"
	                                          "#.....P.....#\n"
	                                          "#o#.#####.#o#\n"
	                                          "#...........#\n"
	                                          "#############\n";

	// Joins rows with LF, without a final newline.
	[[nodiscard]] std::string Rows(std::initializer_list<std::string_view> rows)
	{
		std::string text;
		for (const std::string_view row : rows)
		{
			if (!text.empty()) text += '\n';
			text += row;
		}
		return text;
	}

	// Parses text that must be rejected, and returns why.
	[[nodiscard]] TileMapError ErrorFor(std::string_view text)
	{
		const game::TileMapResult result = ParseTileMap(text);
		REQUIRE_FALSE(result.map.has_value());
		return result.error;
	}
} // namespace

TEST_CASE("ParseTileMap reads every kind of tile into a grid", "[game][tilemap]")
{
	const game::TileMapResult result = ParseTileMap(kExampleMaze);
	REQUIRE(result.map.has_value());
	const TileMap& map = *result.map;

	CHECK(map.Width() == 13);
	CHECK(map.Height() == 9);
	CHECK(map.At({.column = 0, .row = 0}) == Tile::Wall);
	CHECK(map.At({.column = 1, .row = 1}) == Tile::PowerPellet);
	CHECK(map.At({.column = 2, .row = 1}) == Tile::Pellet);
	CHECK(map.At({.column = 6, .row = 3}) == Tile::EnemySpawn);
	CHECK(map.At({.column = 6, .row = 4}) == Tile::Door);
	CHECK(map.At({.column = 0, .row = 4}) == Tile::Tunnel);
	CHECK(map.At({.column = 6, .row = 5}) == Tile::PlayerSpawn);
	CHECK(result.error.message.empty());
}

TEST_CASE("ParseTileMap reads spaces as empty floor", "[game][tilemap]")
{
	const game::TileMapResult result = ParseTileMap(Rows({"#####", "#P E#", "#####"}));
	REQUIRE(result.map.has_value());
	CHECK(result.map->At({.column = 2, .row = 1}) == Tile::Empty);
}

TEST_CASE("TileMap finds tiles in reading order", "[game][tilemap]")
{
	const game::TileMapResult result = ParseTileMap(kExampleMaze);
	REQUIRE(result.map.has_value());

	const std::vector<TilePosition> power = result.map->Find(Tile::PowerPellet);
	REQUIRE(power.size() == 4);
	CHECK(power[0] == TilePosition{.column = 1, .row = 1});
	CHECK(power[1] == TilePosition{.column = 11, .row = 1});
	CHECK(power[2] == TilePosition{.column = 1, .row = 6});
	CHECK(power[3] == TilePosition{.column = 11, .row = 6});

	CHECK(result.map->Find(Tile::PlayerSpawn) == std::vector<TilePosition>{{.column = 6, .row = 5}});
	CHECK(result.map->Contains({.column = 12, .row = 8}));
	CHECK_FALSE(result.map->Contains({.column = 13, .row = 0}));
	CHECK_FALSE(result.map->Contains({.column = 0, .row = 9}));
}

TEST_CASE("ParseTileMap accepts CRLF line endings and blank lines at the end", "[game][tilemap]")
{
	const game::TileMapResult result = ParseTileMap("#####\r\n#P E#\r\n#####\r\n\r\n\n");
	REQUIRE(result.map.has_value());
	CHECK(result.map->Width() == 5);
	CHECK(result.map->Height() == 3);
}

TEST_CASE("ParseTileMap rejects an empty maze", "[game][tilemap]")
{
	for (const std::string_view text : {"", "\n", "\r\n\n"})
	{
		const TileMapError error = ErrorFor(text);
		CHECK(error.line == 0);
		CHECK(error.message == "maze is empty");
	}
}

TEST_CASE("ParseTileMap reports an unknown tile at its line and column", "[game][tilemap]")
{
	const TileMapError error = ErrorFor(Rows({"#####", "#P E#", "#.x.#", "#####"}));
	CHECK(error.line == 3);
	CHECK(error.column == 3);
	CHECK(error.message == "unknown tile 'x'");
}

TEST_CASE("ParseTileMap shows invisible unknown characters as codes", "[game][tilemap]")
{
	const TileMapError error = ErrorFor(Rows({"#####", "#P\tE#", "#####"}));
	CHECK(error.column == 3);
	CHECK(error.message == "unknown tile character 0x09");
}

TEST_CASE("ParseTileMap rejects rows of a different width", "[game][tilemap]")
{
	SECTION("A short row points at the first missing tile")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#P E", "#####"}));
		CHECK(error.line == 2);
		CHECK(error.column == 5);
		CHECK(error.message == "row 2 has 4 tiles; expected 5 like row 1");
	}
	SECTION("A long row points at the first tile too many")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#P E##", "#####"}));
		CHECK(error.line == 2);
		CHECK(error.column == 6);
		CHECK(error.message == "row 2 has 6 tiles; expected 5 like row 1");
	}
	SECTION("A blank line inside the maze is an empty row")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#P E#", "", "#####"}));
		CHECK(error.line == 3);
		CHECK(error.column == 1);
	}
}

TEST_CASE("ParseTileMap needs exactly one player spawn", "[game][tilemap]")
{
	SECTION("None")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#. E#", "#####"}));
		CHECK(error.line == 0);
		CHECK(error.column == 0);
		CHECK(error.message == "no player spawn 'P'");
	}
	SECTION("Two, reported at the second")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#P E#", "#.P.#", "#####"}));
		CHECK(error.line == 3);
		CHECK(error.column == 3);
		CHECK(error.message == "second player spawn 'P'; the first is at line 2, column 2");
	}
}

TEST_CASE("ParseTileMap needs at least one enemy spawn", "[game][tilemap]")
{
	CHECK(ErrorFor(Rows({"#####", "#P .#", "#####"})).message == "no enemy spawn 'E'");
	CHECK(ParseTileMap(Rows({"#####", "#PEE#", "#####"})).map.has_value());
}

TEST_CASE("ParseTileMap accepts tunnels paired on opposite edges", "[game][tilemap]")
{
	const game::TileMapResult result = ParseTileMap(Rows({"##T##", "T.P.T", "#.E.#", "##T##"}));
	REQUIRE(result.map.has_value());
	CHECK(result.map->Find(Tile::Tunnel).size() == 4);
}

TEST_CASE("ParseTileMap rejects tunnels without a pair on the opposite edge", "[game][tilemap]")
{
	SECTION("Left edge")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "TP E#", "#####"}));
		CHECK(error.line == 2);
		CHECK(error.column == 1);
		CHECK(error.message == "tunnel on the left edge has no pair on the right edge");
	}
	SECTION("Right edge")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#P ET", "#####"}));
		CHECK(error.column == 5);
		CHECK(error.message == "tunnel on the right edge has no pair on the left edge");
	}
	SECTION("Top edge")
	{
		const TileMapError error = ErrorFor(Rows({"##T##", "#P E#", "#####"}));
		CHECK(error.line == 1);
		CHECK(error.column == 3);
		CHECK(error.message == "tunnel on the top edge has no pair on the bottom edge");
	}
	SECTION("Bottom edge")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#P E#", "##T##"}));
		CHECK(error.line == 3);
		CHECK(error.message == "tunnel on the bottom edge has no pair on the top edge");
	}
}

TEST_CASE("ParseTileMap rejects tunnels in corners or inside the maze", "[game][tilemap]")
{
	SECTION("Corner")
	{
		const TileMapError error = ErrorFor(Rows({"T###T", "#P E#", "T###T"}));
		CHECK(error.line == 1);
		CHECK(error.column == 1);
		CHECK(error.message == "tunnel in a corner; a tunnel goes on one edge, facing a tunnel on the opposite edge");
	}
	SECTION("Inside")
	{
		const TileMapError error = ErrorFor(Rows({"#####", "#PTE#", "#####"}));
		CHECK(error.line == 2);
		CHECK(error.column == 3);
		CHECK(error.message == "tunnel is not on an edge of the maze");
	}
}

TEST_CASE("FormatError writes errors the way compilers do", "[game][tilemap]")
{
	CHECK(FormatError("level1.txt", {.line = 4, .column = 7, .message = "unknown tile 'x'"}) ==
	      "level1.txt:4:7: unknown tile 'x'");
	CHECK(FormatError("level1.txt", {.line = 0, .column = 0, .message = "no player spawn 'P'"}) ==
	      "level1.txt: no player spawn 'P'");
}

TEST_CASE("LoadTileMap reads and parses a file", "[game][tilemap]")
{
	const std::filesystem::path file = std::filesystem::temp_directory_path() / "ecs_engine_tilemap_test.txt";
	{
		std::ofstream stream(file, std::ios::binary);
		stream << kExampleMaze;
	}
	const game::TileMapResult result = LoadTileMap(file);
	std::filesystem::remove(file);

	REQUIRE(result.map.has_value());
	CHECK(result.map->Width() == 13);
	CHECK(result.map->Height() == 9);
}

TEST_CASE("LoadTileMap reports a file that can't be opened", "[game][tilemap]")
{
	const game::TileMapResult result = LoadTileMap(std::filesystem::path("no_such_dir") / "no_such_maze.txt");
	CHECK_FALSE(result.map.has_value());
	CHECK(result.error.line == 0);
	CHECK(result.error.message == "cannot open the file");
}
