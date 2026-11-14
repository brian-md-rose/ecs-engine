#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

#include "direction.hpp"
#include "enemytuning.hpp"
#include "steering.hpp"
#include "targeting.hpp"
#include "tilemap.hpp"

namespace
{
	using game::ChaseView;
	using game::Direction;
	using game::Personality;
	using game::Target;
	using game::TileMap;
	using game::TilePosition;

	[[nodiscard]] TileMap Parse(std::string_view text)
	{
		game::TileMapResult result = game::ParseTileMap(text);
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	// Junctions at columns 1, 5 and 9 of row 3 and at column 5 of rows 1 and 5.
	[[nodiscard]] TileMap Grid()
	{
		return Parse("###########\n"   // 0
		             "#.........#\n"   // 1
		             "#.###.###.#\n"   // 2
		             "#.........#\n"   // 3
		             "#.###P###.#\n"   // 4
		             "#....E....#\n"   // 5
		             "###########\n"); // 6
	}

	[[nodiscard]] TilePosition At(std::size_t column, std::size_t row)
	{
		return {.column = column, .row = row};
	}

	[[nodiscard]] Target To(int column, int row)
	{
		return {.column = column, .row = row};
	}

	[[nodiscard]] ChaseView View(TilePosition self, TilePosition player, Direction heading = Direction::None)
	{
		return {.self = self, .player = player, .playerHeading = heading, .ally = self};
	}
} // namespace

TEST_CASE("Spawns get the four personalities in turn", "[game][targeting]")
{
	CHECK(game::PersonalityForSpawn(0) == Personality::Grit);
	CHECK(game::PersonalityForSpawn(1) == Personality::Snare);
	CHECK(game::PersonalityForSpawn(2) == Personality::Clasp);
	CHECK(game::PersonalityForSpawn(3) == Personality::Wisp);
	CHECK(game::PersonalityForSpawn(4) == Personality::Grit);
}

TEST_CASE("The four personalities differ in name, look, corner and release time", "[game][targeting]")
{
	for (std::size_t a = 0; a < game::kPersonalityCount; ++a)
	{
		for (std::size_t b = a + 1; b < game::kPersonalityCount; ++b)
		{
			CAPTURE(a, b);
			const game::PersonalityTuning& first = game::kPersonalities[a];
			const game::PersonalityTuning& second = game::kPersonalities[b];
			CHECK(first.name != second.name);
			CHECK(first.corner != second.corner);
			CHECK(first.releaseSeconds < second.releaseSeconds);
			const bool sameColour = first.look.color.r == second.look.color.r &&
			                        first.look.color.g == second.look.color.g &&
			                        first.look.color.b == second.look.color.b;
			CHECK_FALSE(sameColour);
		}
	}
}

TEST_CASE("Each personality patrols towards its own corner of the maze", "[game][targeting]")
{
	const TileMap map = Grid();
	CHECK(game::PatrolTarget(map, Personality::Grit) == To(0, 0));
	CHECK(game::PatrolTarget(map, Personality::Snare) == To(10, 0));
	CHECK(game::PatrolTarget(map, Personality::Clasp) == To(0, 6));
	CHECK(game::PatrolTarget(map, Personality::Wisp) == To(10, 6));
}

TEST_CASE("Grit goes straight for the player's tile", "[game][targeting]")
{
	const TileMap map = Grid();
	const ChaseView view = View(At(9, 5), At(2, 1), Direction::Right);
	CHECK(game::GritTarget(view) == To(2, 1));
	CHECK(game::ChaseTarget(map, Personality::Grit, view) == To(2, 1));
}

TEST_CASE("Snare waits at the next junction on the player's way", "[game][targeting]")
{
	const TileMap map = Grid();
	CHECK(game::JunctionAhead(map, At(2, 1), Direction::Right, 6) == At(5, 1));
	CHECK(game::JunctionAhead(map, At(1, 2), Direction::Down, 6) == At(1, 3));
	CHECK(game::JunctionAhead(map, At(1, 1), Direction::Left, 6) == At(1, 1));  // a wall next
	CHECK(game::JunctionAhead(map, At(2, 1), Direction::None, 6) == At(2, 1));  // standing still
	CHECK(game::JunctionAhead(map, At(2, 1), Direction::Right, 2) == At(4, 1)); // not that far
	CHECK(game::JunctionAhead(map, At(6, 1), Direction::Right, 6) == At(9, 1)); // stops at the wall

	const ChaseView view = View(At(9, 5), At(2, 3), Direction::Right);
	CHECK(game::SnareTarget(map, view) == To(5, 3));
	CHECK(game::ChaseTarget(map, Personality::Snare, view) == To(5, 3));
	CHECK(game::SnareTarget(map, View(At(9, 5), At(2, 3))) == To(2, 3)); // standing still: its tile
}

TEST_CASE("Snare looks no further than its lookahead down a long corridor", "[game][targeting]")
{
	const TileMap corridor = Parse("############\n"
	                               "#P........E#\n"
	                               "############\n");
	const ChaseView view = View(At(10, 1), At(1, 1), Direction::Right);
	CHECK(game::SnareTarget(corridor, view) == To(1 + game::kSnareLookahead, 1));
}

TEST_CASE("Clasp closes the far side of the player from Grit", "[game][targeting]")
{
	const TileMap map = Grid();
	ChaseView view = View(At(1, 5), At(5, 3));
	const int reach = game::kClaspReach;

	view.ally = At(1, 3); // Grit to the left: Clasp aims right of the player
	CHECK(game::ClaspTarget(view) == To(5 + reach, 3));
	view.ally = At(8, 2); // mostly to the right: aims left
	CHECK(game::ClaspTarget(view) == To(5 - reach, 3));
	view.ally = At(4, 0); // mostly above: aims below
	CHECK(game::ClaspTarget(view) == To(5, 3 + reach));
	view.ally = At(6, 6); // mostly below: aims above, off the maze
	CHECK(game::ClaspTarget(view) == To(5, 3 - reach));
	view.ally = At(3, 1); // as far across as down: across wins
	CHECK(game::ClaspTarget(view) == To(5 + reach, 3));
	view.ally = At(5, 3); // on the player: the player's tile
	CHECK(game::ClaspTarget(view) == To(5, 3));
	CHECK(game::ChaseTarget(map, Personality::Clasp, view) == To(5, 3));
}

TEST_CASE("Wisp chases from afar and backs away when close", "[game][targeting]")
{
	const TileMap map = Grid();
	const int comfort = game::kWispComfortDistance;

	// Further than its comfort distance: it heads for the player.
	const ChaseView far = View(At(1, 1), At(1 + static_cast<std::size_t>(comfort) - 2, 4));
	CHECK(game::WispTarget(far) == To(comfort - 1, 4));
	CHECK(game::ChaseTarget(map, Personality::Wisp, far) == To(comfort - 1, 4));

	// Exactly at it, or closer: it aims directly away, as far again beyond itself.
	const ChaseView edge = View(At(4, 3), At(4 + static_cast<std::size_t>(comfort), 3));
	CHECK(game::WispTarget(edge) == To(4 - comfort, 3));
	const ChaseView close = View(At(5, 3), At(6, 5));
	CHECK(game::WispTarget(close) == To(4, 1));
}

TEST_CASE("The house is found from its door, inside towards the enemy spawns", "[game][targeting]")
{
	const std::optional<game::House> below = game::FindHouse(Parse("#######\n"
	                                                               "#..P..#\n"
	                                                               "###-###\n"
	                                                               "#.E.E.#\n"
	                                                               "#######\n"));
	REQUIRE(below.has_value());
	CHECK(below->exit == At(3, 1));
	CHECK(below->home == At(3, 3));

	// The example from docs/maze-format.md: the house is above its door.
	const std::optional<game::House> above = game::FindHouse(Parse("#############\n"
	                                                               "#o....#....o#\n"
	                                                               "#.##.###.##.#\n"
	                                                               "#....#E#....#\n"
	                                                               "T.##.#-#.##.T\n"
	                                                               "#.....P.....#\n"
	                                                               "#o#.#####.#o#\n"
	                                                               "#...........#\n"
	                                                               "#############\n"));
	REQUIRE(above.has_value());
	CHECK(above->exit == At(6, 5));
	CHECK(above->home == At(6, 3));

	CHECK_FALSE(game::FindHouse(Parse("#####\n"
	                                  "#P.E#\n"
	                                  "#####\n"))
	                .has_value());
}
