#include "enemymodes.hpp"

#include <algorithm>

namespace game
{
	const ModeSchedule& ScheduleFor(int level) noexcept
	{
		if (level >= kLateScheduleLevel) return kLateSchedule;
		if (level >= kMiddleScheduleLevel) return kMiddleSchedule;
		return kEarlySchedule;
	}

	float FrightSecondsFor(int level) noexcept
	{
		const int last = static_cast<int>(kFrightSeconds.size()) - 1;
		const int index = std::clamp(level - 1, 0, last);
		return kFrightSeconds[static_cast<std::size_t>(index)];
	}

	ScheduledMode CurrentMode(const ModeClock& clock, int level) noexcept
	{
		const ModeSchedule& schedule = ScheduleFor(level);
		return schedule[std::min(clock.phase, schedule.size() - 1)].mode;
	}

	ModeClock TickModeClock(ModeClock clock, int level, float seconds) noexcept
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

		const ModeSchedule& schedule = ScheduleFor(level);
		clock.phaseSeconds += seconds;
		// The last stretch never ends, whatever its length says.
		while (clock.phase + 1 < schedule.size() && clock.phaseSeconds >= schedule[clock.phase].seconds)
		{
			clock.phaseSeconds -= schedule[clock.phase].seconds;
			++clock.phase;
		}
		return clock;
	}

	ModeClock StartFright(ModeClock clock, int level) noexcept
	{
		clock.frightSeconds = FrightSecondsFor(level);
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

	float EnemySpeedFor(const EnemyBrain& brain) noexcept
	{
		switch (brain.state)
		{
		case EnemyState::InHouse:
			return 0.0f;
		case EnemyState::Returning:
			return kReturningSpeed;
		case EnemyState::Leaving:
			return brain.frightened ? kFrightenedSpeed : kHouseSpeed;
		case EnemyState::Roaming:
			break;
		}
		return brain.frightened ? kFrightenedSpeed : kEnemySpeed;
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
