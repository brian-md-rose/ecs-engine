#include "enemymodes.hpp"

#include <algorithm>

namespace game
{
	ScheduledMode CurrentMode(const ModeClock& clock, const ModeSchedule& schedule) noexcept
	{
		return schedule[std::min(clock.phase, schedule.size() - 1)].mode;
	}

	ModeClock TickModeClock(ModeClock clock, const ModeSchedule& schedule, float seconds) noexcept
	{
		if (IsFrightActive(clock))
		{
			if (seconds < clock.frightSeconds)
			{
				clock.frightSeconds -= seconds;
				return clock;
			}
			seconds -= clock.frightSeconds;
			clock.frightSeconds = 0.0f;
		}

		clock.phaseSeconds += seconds;
		// The last stretch never ends, whatever its length says.
		while (clock.phase + 1 < schedule.size() && clock.phaseSeconds >= schedule[clock.phase].seconds)
		{
			clock.phaseSeconds -= schedule[clock.phase].seconds;
			++clock.phase;
		}
		return clock;
	}

	ModeClock StartFright(ModeClock clock, float frightSeconds) noexcept
	{
		clock.frightSeconds = frightSeconds;
		clock.eatenInFright = 0;
		return clock;
	}

	int EnemyPoints(int eatenBefore) noexcept
	{
		return kEnemyPointsStep * (std::max(eatenBefore, 0) + 1);
	}

	EnemyBrain TickHouse(EnemyBrain brain, float seconds) noexcept
	{
		if (brain.state != EnemyState::InHouse) return brain;
		brain.houseSeconds += seconds;
		if (brain.houseSeconds >= brain.releaseSeconds) brain.state = EnemyState::Leaving;
		return brain;
	}

	EnemyBrain SwitchSchedule(EnemyBrain brain) noexcept
	{
		if (brain.state == EnemyState::Roaming && !brain.frightened) brain.reversePending = true;
		return brain;
	}

	EnemyBrain Frighten(EnemyBrain brain) noexcept
	{
		if (brain.state == EnemyState::Returning) return brain;
		if (brain.state == EnemyState::Roaming && !brain.frightened) brain.reversePending = true;
		brain.frightened = true;
		return brain;
	}

	EnemyBrain Calm(EnemyBrain brain) noexcept
	{
		brain.frightened = false;
		return brain;
	}

	EnemyBrain Eat(EnemyBrain brain) noexcept
	{
		if (!CanBeEaten(brain)) return brain;
		brain.state = EnemyState::Returning;
		brain.frightened = false;
		return brain;
	}

	EnemyBrain ArriveHome(EnemyBrain brain) noexcept
	{
		if (brain.state != EnemyState::Returning) return brain;
		brain.state = EnemyState::Leaving;
		brain.reversePending = true; // it came in heading into the house; the way out is behind it
		return brain;
	}

	EnemyBrain ArriveOutside(EnemyBrain brain) noexcept
	{
		if (brain.state == EnemyState::Leaving) brain.state = EnemyState::Roaming;
		return brain;
	}

	bool IsDangerous(const EnemyBrain& brain) noexcept
	{
		return !brain.frightened && brain.state != EnemyState::Returning;
	}

	bool CanBeEaten(const EnemyBrain& brain) noexcept
	{
		return brain.frightened && brain.state != EnemyState::Returning;
	}

	float EnemySpeedFor(const EnemyBrain& brain, const LevelTuning& level) noexcept
	{
		switch (brain.state)
		{
		case EnemyState::InHouse:
			return 0.0f;
		case EnemyState::Returning:
			return kReturningSpeed;
		case EnemyState::Leaving:
			return brain.frightened ? level.frightenedSpeed : kHouseSpeed;
		case EnemyState::Roaming:
			break;
		}
		return brain.frightened ? level.frightenedSpeed : level.enemySpeed;
	}

	bool UsesDoor(const EnemyBrain& brain) noexcept
	{
		return brain.state == EnemyState::Leaving || brain.state == EnemyState::Returning;
	}

	Goal GoalFor(const EnemyBrain& brain, ScheduledMode mode) noexcept
	{
		switch (brain.state)
		{
		case EnemyState::InHouse:
			return Goal::Stay;
		case EnemyState::Leaving:
			return Goal::Exit;
		case EnemyState::Returning:
			return Goal::Home;
		case EnemyState::Roaming:
			break;
		}
		if (brain.frightened) return Goal::Wander;
		return mode == ScheduledMode::Patrol ? Goal::Patrol : Goal::Chase;
	}
} // namespace game
