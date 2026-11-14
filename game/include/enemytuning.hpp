#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "direction.hpp"

// Every number that decides how the enemies move and think, in one place, so the game can be
// tuned here without hunting through the code. All of them are our own (see docs/enemies.md).
namespace game
{
	// Steering (steering.hpp)

	// When two exits are equally close to an enemy's target, the one earlier in this list wins:
	// clockwise, starting from Right.
	inline constexpr std::array<Direction, 4> kTieBreakOrder = {
	    {Direction::Right, Direction::Down, Direction::Left, Direction::Up}};

	// Speeds, in tiles per second (the player runs at kPlayerSpeed, 7.5).

	inline constexpr float kEnemySpeed = 6.75f;      // patrolling or chasing: a little slower than the player
	inline constexpr float kFrightenedSpeed = 4.25f; // frightened: easy to run down
	inline constexpr float kReturningSpeed = 13.0f;  // eaten, hurrying back to the house
	inline constexpr float kHouseSpeed = 4.0f;       // finding its way out of the house

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
	using ModeSchedule = std::array<ModePhase, kSchedulePhases>;

	// Level 1: long patrols give the player room to learn the maze.
	inline constexpr ModeSchedule kEarlySchedule = {{{ScheduledMode::Patrol, 6.0f},
	                                                 {ScheduledMode::Chase, 18.0f},
	                                                 {ScheduledMode::Patrol, 6.0f},
	                                                 {ScheduledMode::Chase, 24.0f},
	                                                 {ScheduledMode::Patrol, 4.0f},
	                                                 {ScheduledMode::Chase, 0.0f}}};

	// From kMiddleScheduleLevel: shorter breathers, longer hunts.
	inline constexpr int kMiddleScheduleLevel = 2;
	inline constexpr ModeSchedule kMiddleSchedule = {{{ScheduledMode::Patrol, 5.0f},
	                                                  {ScheduledMode::Chase, 22.0f},
	                                                  {ScheduledMode::Patrol, 4.0f},
	                                                  {ScheduledMode::Chase, 30.0f},
	                                                  {ScheduledMode::Patrol, 3.0f},
	                                                  {ScheduledMode::Chase, 0.0f}}};

	// From kLateScheduleLevel on.
	inline constexpr int kLateScheduleLevel = 4;
	inline constexpr ModeSchedule kLateSchedule = {{{ScheduledMode::Patrol, 4.0f},
	                                                {ScheduledMode::Chase, 26.0f},
	                                                {ScheduledMode::Patrol, 3.0f},
	                                                {ScheduledMode::Chase, 40.0f},
	                                                {ScheduledMode::Patrol, 2.0f},
	                                                {ScheduledMode::Chase, 0.0f}}};

	// Fright (after a power pellet)

	// How long a fright lasts on levels 1, 2, ...; every later level uses the last value.
	inline constexpr std::array<float, 5> kFrightSeconds = {{7.0f, 6.0f, 5.0f, 4.0f, 3.0f}};
	// Frightened enemies flash for this long before the fright ends, as a warning.
	inline constexpr float kFrightWarningSeconds = 2.0f;
	inline constexpr float kFrightFlashesPerSecond = 4.0f;
	// The first enemy eaten in one fright is worth this many points, the second twice as many,
	// the third three times, and so on.
	inline constexpr int kEnemyPointsStep = 60;
} // namespace game
