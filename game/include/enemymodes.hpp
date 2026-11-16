#pragma once
#include <cstddef>
#include <cstdint>

#include "difficulty.hpp"
#include "enemytuning.hpp"

// What the enemies are doing and when that changes, as pure functions: no registry, no SDL.
// The enemy systems apply them to the ModeClock resource and each enemy's EnemyBrain.
//
// All enemies share one clock. Out in the maze they Patrol or Chase together, following the
// level's schedule (enemytuning.hpp); a power pellet frightens them all for a while, and holds
// the schedule until the fright is over. Each enemy also has its own state:
//
//   InHouse ──(its release time)──► Leaving ──(out of the door)──► Roaming
//                                      ▲                               │ eaten while frightened
//                                      └──────(reached home)── Returning ◄┘
//
// Whenever an enemy roaming the maze changes what it is doing (the schedule switches between
// Patrol and Chase, or it becomes frightened) it turns back at once. That is the only time an
// enemy turns back, apart from at a dead end (see steering.hpp).
namespace game
{
	// Resource: the clock every enemy shares. A new round starts it from scratch.
	struct ModeClock
	{
		std::size_t phase = 0;      // the stretch of the schedule under way
		float phaseSeconds = 0.0f;  // time spent in it so far
		float frightSeconds = 0.0f; // time the current fright has left; 0 when there is none
		int eatenInFright = 0;      // enemies eaten since the last power pellet
	};

	[[nodiscard]] ScheduledMode CurrentMode(const ModeClock& clock, const ModeSchedule& schedule) noexcept;

	[[nodiscard]] constexpr bool IsFrightActive(const ModeClock& clock) noexcept
	{
		return clock.frightSeconds > 0.0f;
	}

	// Adds seconds. A fright counts down first, and the schedule waits for it; time left over
	// once the fright ends goes to the schedule. The schedule moves through its stretches (as
	// many as seconds covers) and stays in the last one.
	[[nodiscard]] ModeClock TickModeClock(ModeClock clock, const ModeSchedule& schedule, float seconds) noexcept;

	// A power pellet: a fright of frightSeconds (the level's), from now, whether or not one was
	// under way.
	[[nodiscard]] ModeClock StartFright(ModeClock clock, float frightSeconds) noexcept;

	// Points for eating an enemy after eatenBefore others in the same fright.
	[[nodiscard]] int EnemyPoints(int eatenBefore) noexcept;

	enum class EnemyState : std::uint8_t
	{
		InHouse,  // waiting in the house for its release time
		Leaving,  // on its way out of the house through the door
		Roaming,  // out in the maze: patrolling, chasing, or frightened
		Returning // eaten: hurrying back into the house
	};

	// Component: one enemy's state.
	struct EnemyBrain
	{
		EnemyState state = EnemyState::InHouse;
		bool frightened = false;     // slow, wanders at random, and can be eaten
		bool reversePending = false; // turn back at once (the steering system clears it)
		float houseSeconds = 0.0f;   // time spent in the house this round
		float releaseSeconds = 0.0f; // when it leaves the house, counted from the start of the round
	};

	// Adds seconds to an enemy waiting in the house, and lets it out once its release time has
	// come. Other states are unchanged.
	[[nodiscard]] EnemyBrain TickHouse(EnemyBrain brain, float seconds) noexcept;

	// The schedule switched between Patrol and Chase: a roaming enemy that isn't frightened
	// turns back.
	[[nodiscard]] EnemyBrain SwitchSchedule(EnemyBrain brain) noexcept;

	// A power pellet: every enemy but one returning home is frightened. One that was roaming
	// and wasn't frightened already turns back.
	[[nodiscard]] EnemyBrain Frighten(EnemyBrain brain) noexcept;

	// The fright is over. No turning back: it just carries on in its usual way.
	[[nodiscard]] EnemyBrain Calm(EnemyBrain brain) noexcept;

	// The player ate it: a frightened enemy that isn't already going home goes home, no longer
	// frightened. Anything else is unchanged.
	[[nodiscard]] EnemyBrain Eat(EnemyBrain brain) noexcept;

	// A returning enemy reached its home tile: it turns round and leaves again.
	[[nodiscard]] EnemyBrain ArriveHome(EnemyBrain brain) noexcept;

	// A leaving enemy came out of the door: it starts roaming.
	[[nodiscard]] EnemyBrain ArriveOutside(EnemyBrain brain) noexcept;

	// Touching it on the same tile costs the player a life.
	[[nodiscard]] bool IsDangerous(const EnemyBrain& brain) noexcept;
	// Touching it on the same tile eats it.
	[[nodiscard]] bool CanBeEaten(const EnemyBrain& brain) noexcept;

	// Tiles per second: the level's speeds while roaming or frightened, the fixed ones of
	// enemytuning.hpp going in and out of the house; 0 while it waits in the house.
	[[nodiscard]] float EnemySpeedFor(const EnemyBrain& brain, const LevelTuning& level) noexcept;

	// Going in or out of the house: the door lets it through. Roaming enemies can't go back in.
	[[nodiscard]] bool UsesDoor(const EnemyBrain& brain) noexcept;

	// Where an enemy is heading, given the schedule's mode.
	enum class Goal : std::uint8_t
	{
		Stay,   // waits where it is
		Exit,   // the tile just outside the house's door
		Home,   // the tile just inside the house's door
		Wander, // random turns
		Patrol, // its corner
		Chase   // its personality's target near the player
	};

	[[nodiscard]] Goal GoalFor(const EnemyBrain& brain, ScheduledMode mode) noexcept;
} // namespace game
