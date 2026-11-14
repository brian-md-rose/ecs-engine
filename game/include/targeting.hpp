#pragma once
#include <cstddef>
#include <optional>

#include "direction.hpp"
#include "enemytuning.hpp"
#include "steering.hpp"
#include "tilemap.hpp"

// Where each enemy aims, as pure functions over a TileMap: no registry, no SDL. The steering
// system picks the target for an enemy's goal (enemymodes.hpp) here and steers towards it.
namespace game
{
	// Component: which of the four enemies this is (enemytuning.hpp).
	struct EnemyPersonality
	{
		Personality kind = Personality::Grit;
	};

	// The personality of the enemy at the given spawn, counting spawns in reading order from 0.
	[[nodiscard]] constexpr Personality PersonalityForSpawn(std::size_t spawnIndex) noexcept
	{
		return static_cast<Personality>(spawnIndex % kPersonalityCount);
	}

	// The maze's corner tile on that side. Usually a wall: an enemy patrolling towards it
	// circles the block of walls nearest to it.
	[[nodiscard]] Target CornerTarget(const TileMap& map, Corner corner) noexcept;

	// Where a personality patrols.
	[[nodiscard]] Target PatrolTarget(const TileMap& map, Personality personality) noexcept;

	// What an enemy knows when it picks its chase target.
	struct ChaseView
	{
		TilePosition self;
		TilePosition player;
		Direction playerHeading = Direction::None;
		TilePosition ally; // where kClaspAlly is; Clasp's own tile if there is none
	};

	// The first junction (a tile with three or more sides the player can enter) on the way
	// from tile along heading, at most lookahead tiles on. Stops short of a wall, so it is
	// always a tile the player can reach; tile itself if heading is None or a wall is next.
	[[nodiscard]] TilePosition JunctionAhead(const TileMap& map, TilePosition tile, Direction heading, int lookahead);

	// The four chase rules, one per personality.

	// Grit: the player's tile.
	[[nodiscard]] Target GritTarget(const ChaseView& view) noexcept;
	// Snare: the next junction on the player's way, within kSnareLookahead tiles.
	[[nodiscard]] Target SnareTarget(const TileMap& map, const ChaseView& view);
	// Clasp: kClaspReach tiles past the player on the side away from its ally, along the axis
	// on which the ally is further from the player (across, on a tie). The player's tile when
	// the ally is on it.
	[[nodiscard]] Target ClaspTarget(const ChaseView& view) noexcept;
	// Wisp: the player's tile while further than kWispComfortDistance (across plus down);
	// closer than that, the tile as far again beyond itself, directly away from the player.
	[[nodiscard]] Target WispTarget(const ChaseView& view) noexcept;

	[[nodiscard]] Target ChaseTarget(const TileMap& map, Personality personality, const ChaseView& view);

	// The enemy house's way in and out, found from its door.
	struct House
	{
		TilePosition exit; // just outside the door: leaving enemies are out once they get here
		TilePosition home; // just inside the door: returning enemies are home once they get here
	};

	// The house of the first door in reading order. Its inside is the side of the door nearer
	// the enemy spawns, and its outside the opposite side. Nothing if the maze has no door, or
	// the door doesn't have open floor on two opposite sides.
	[[nodiscard]] std::optional<House> FindHouse(const TileMap& map);
} // namespace game
