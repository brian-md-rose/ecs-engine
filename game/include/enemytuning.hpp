#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "grid/direction.hpp"

// Every number that decides how the enemies move and think, in one place, so the game can be
// tuned here without hunting through the code. All of them are our own (see docs/enemies.md).
namespace game
{
	// Steering (steering.hpp)

	// When two exits are equally close to an enemy's target, the one earlier in this list wins:
	// clockwise, starting from Right.
	inline constexpr std::array<engine::grid::Direction, 4> kTieBreakOrder = {
	    {engine::grid::Direction::Right, engine::grid::Direction::Down, engine::grid::Direction::Left,
	     engine::grid::Direction::Up}};

	// Speeds, in tiles per second. The player's, the roaming enemies' and the frightened ones'
	// change from level to level (difficulty.hpp, assets/levels.txt); these don't.

	inline constexpr float kReturningSpeed = 13.0f; // eaten, hurrying back to the house
	inline constexpr float kHouseSpeed = 4.0f;      // finding its way out of the house

	// The patrol/chase schedule (enemymodes.hpp)

	// What enemies out in the maze do when they are not frightened.
	enum class ScheduledMode : std::uint8_t
	{
		Patrol, // each heads for its own corner of the maze
		Chase   // each hunts the player in its own way (see targeting.hpp)
	};

	// One stretch of the schedule. The last stretch has 0 seconds: it lasts until the round ends.
	struct ModePhase
	{
		ScheduledMode mode = ScheduledMode::Patrol;
		float seconds = 0.0f;
	};

	inline constexpr std::size_t kSchedulePhases = 6;
	// The stretches of a level's schedule: patrol, chase, patrol, chase, patrol, then chase for
	// good. Their lengths are set per level (difficulty.hpp).
	using ModeSchedule = std::array<ModePhase, kSchedulePhases>;

	// Fright (after a power pellet)

	// How long a fright lasts is set per level (difficulty.hpp).
	// Frightened enemies flash for this long before the fright ends, as a warning.
	inline constexpr float kFrightWarningSeconds = 2.0f;
	inline constexpr float kFrightFlashesPerSecond = 4.0f;
	// The first enemy eaten in one fright is worth this many points, the second twice as many,
	// the third three times, and so on.
	inline constexpr int kEnemyPointsStep = 60;

	// Personalities (targeting.hpp)

	// The four enemies. Each has its own way of hunting the player, its own corner to patrol,
	// its own release time and its own look. Enemy spawns in the maze get them in this order,
	// in reading order (left to right, top to bottom), starting again after the fourth.
	enum class Personality : std::uint8_t
	{
		Grit,  // the bloodhound: goes straight for the player's tile
		Snare, // the trapper: waits at the next junction on the player's path
		Clasp, // the flanker: closes the far side of the player from Grit
		Wisp,  // the stalker: follows from afar, slips away when the player comes close
		Count
	};

	inline constexpr std::size_t kPersonalityCount = static_cast<std::size_t>(Personality::Count);

	enum class Corner : std::uint8_t
	{
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	struct PersonalityTuning
	{
		std::string_view name;
		Corner corner;           // patrols around the maze's corner tile on this side
		float releaseSeconds;    // leaves the house this long after the round starts
		std::string_view sprite; // its animation in assets/sprites/sprites.txt
	};

	inline constexpr std::uint8_t kEnemyLayer = 2; // above the pellets, below the player

	inline constexpr std::array<PersonalityTuning, kPersonalityCount> kPersonalities = {{
	    {"Grit", Corner::TopLeft, 0.0f, "grit"},
	    {"Snare", Corner::TopRight, 2.0f, "snare"},
	    {"Clasp", Corner::BottomLeft, 5.0f, "clasp"},
	    {"Wisp", Corner::BottomRight, 8.0f, "wisp"},
	}};

	[[nodiscard]] constexpr const PersonalityTuning& TuningFor(Personality personality) noexcept
	{
		return kPersonalities[static_cast<std::size_t>(personality)];
	}

	// Snare looks along the player's way for the next junction (a tile with three or more
	// open sides), at most this many tiles ahead, and waits there.
	inline constexpr int kSnareLookahead = 6;

	// Clasp aims this many tiles past the player, on the side away from Grit, along whichever
	// axis Grit is further off on.
	inline constexpr int kClaspReach = 5;
	inline constexpr Personality kClaspAlly = Personality::Grit;

	// Wisp chases while it is further than this from the player (counting tiles across plus
	// tiles down); this close or closer, it backs away instead.
	inline constexpr int kWispComfortDistance = 6;

	// Frightened enemies' random turns come from a generator seeded with this, so a game
	// plays the same way each time it is given the same input.
	inline constexpr std::uint32_t kEnemyRandomSeed = 20261114u;
} // namespace game
