#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>

#include "enemymodes.hpp"
#include "enemytuning.hpp"

namespace
{
	using Catch::Matchers::WithinAbs;
	using game::EnemyBrain;
	using game::EnemyState;
	using game::Goal;
	using game::ModeClock;
	using game::ScheduledMode;

	constexpr double kMargin = 1e-4;

	[[nodiscard]] EnemyBrain InState(EnemyState state, bool frightened = false)
	{
		return {.state = state, .frightened = frightened};
	}

	// Seconds from the start of a round to the end of the schedule's stretch number phase.
	[[nodiscard]] float EndOf(const game::ModeSchedule& schedule, std::size_t phase)
	{
		float total = 0.0f;
		for (std::size_t i = 0; i <= phase; ++i)
		{
			total += schedule[i].seconds;
		}
		return total;
	}
} // namespace

TEST_CASE("Every schedule starts on patrol, alternates, and ends chasing for good", "[game][enemymodes]")
{
	for (const int level : {1, 2, 3, 4, 9})
	{
		CAPTURE(level);
		const game::ModeSchedule& schedule = game::ScheduleFor(level);
		for (std::size_t i = 0; i < schedule.size(); ++i)
		{
			CHECK(schedule[i].mode == (i % 2 == 0 ? ScheduledMode::Patrol : ScheduledMode::Chase));
			if (i + 1 < schedule.size()) CHECK(schedule[i].seconds > 0.0f);
		}
		CHECK(schedule.back().seconds == 0.0f);
	}
}

TEST_CASE("Later levels have their own schedules", "[game][enemymodes]")
{
	CHECK(&game::ScheduleFor(1) == &game::kEarlySchedule);
	CHECK(&game::ScheduleFor(game::kMiddleScheduleLevel) == &game::kMiddleSchedule);
	CHECK(&game::ScheduleFor(game::kLateScheduleLevel - 1) == &game::kMiddleSchedule);
	CHECK(&game::ScheduleFor(game::kLateScheduleLevel) == &game::kLateSchedule);
	CHECK(&game::ScheduleFor(50) == &game::kLateSchedule);
	CHECK(&game::ScheduleFor(0) == &game::kEarlySchedule); // out of range: the gentlest
}

TEST_CASE("The mode clock follows the level's schedule", "[game][enemymodes]")
{
	const game::ModeSchedule& schedule = game::ScheduleFor(1);
	ModeClock clock;
	CHECK(game::CurrentMode(clock, 1) == ScheduledMode::Patrol);

	clock = game::TickModeClock(clock, 1, schedule[0].seconds - 0.1f);
	CHECK(game::CurrentMode(clock, 1) == ScheduledMode::Patrol);
	clock = game::TickModeClock(clock, 1, 0.2f);
	CHECK(game::CurrentMode(clock, 1) == ScheduledMode::Chase);
	CHECK(clock.phase == 1);
	CHECK_THAT(clock.phaseSeconds, WithinAbs(0.1, kMargin));

	// One long step can cross several stretches.
	ModeClock jump = game::TickModeClock(ModeClock{}, 1, EndOf(schedule, 2) + 0.5f);
	CHECK(jump.phase == 3);
	CHECK(game::CurrentMode(jump, 1) == ScheduledMode::Chase);
	CHECK_THAT(jump.phaseSeconds, WithinAbs(0.5, kMargin));

	// The last stretch lasts for good.
	jump = game::TickModeClock(jump, 1, 10000.0f);
	CHECK(jump.phase == schedule.size() - 1);
	CHECK(game::CurrentMode(jump, 1) == ScheduledMode::Chase);
}

TEST_CASE("The same moment can be patrol on one level and chase on another", "[game][enemymodes]")
{
	// 5.5 s in: level 1 still patrols (6 s), the middle schedule already chases (5 s).
	const ModeClock clock = game::TickModeClock(ModeClock{}, 2, 5.5f);
	CHECK(game::CurrentMode(clock, 2) == ScheduledMode::Chase);
	CHECK(game::CurrentMode(game::TickModeClock(ModeClock{}, 1, 5.5f), 1) == ScheduledMode::Patrol);
}

TEST_CASE("A fright holds the schedule until it is over", "[game][enemymodes]")
{
	const float fright = game::FrightSecondsFor(1);
	ModeClock clock = game::TickModeClock(ModeClock{}, 1, 1.0f);
	clock = game::StartFright(clock, 1);
	CHECK(game::IsFrightActive(clock));
	CHECK(clock.frightSeconds == fright);

	clock = game::TickModeClock(clock, 1, fright - 0.5f);
	CHECK(game::IsFrightActive(clock));
	CHECK_THAT(clock.phaseSeconds, WithinAbs(1.0, kMargin)); // held

	clock = game::TickModeClock(clock, 1, 1.0f); // 0.5 s of fright, then 0.5 s of schedule
	CHECK_FALSE(game::IsFrightActive(clock));
	CHECK(clock.frightSeconds == 0.0f);
	CHECK_THAT(clock.phaseSeconds, WithinAbs(1.5, kMargin));
}

TEST_CASE("Another power pellet restarts the fright and its points", "[game][enemymodes]")
{
	ModeClock clock = game::StartFright(ModeClock{}, 1);
	clock.eatenInFright = 2;
	clock = game::TickModeClock(clock, 1, 3.0f);
	clock = game::StartFright(clock, 1);
	CHECK(clock.frightSeconds == game::FrightSecondsFor(1));
	CHECK(clock.eatenInFright == 0);
}

TEST_CASE("Frights get shorter on later levels, down to a floor", "[game][enemymodes]")
{
	for (int level = 1; level < 8; ++level)
	{
		CAPTURE(level);
		CHECK(game::FrightSecondsFor(level + 1) <= game::FrightSecondsFor(level));
		CHECK(game::FrightSecondsFor(level) > game::kFrightWarningSeconds);
	}
	CHECK(game::FrightSecondsFor(1) == game::kFrightSeconds.front());
	CHECK(game::FrightSecondsFor(99) == game::kFrightSeconds.back());
}

TEST_CASE("Each enemy eaten in one fright is worth more than the last", "[game][enemymodes]")
{
	CHECK(game::EnemyPoints(0) == game::kEnemyPointsStep);
	CHECK(game::EnemyPoints(1) == 2 * game::kEnemyPointsStep);
	CHECK(game::EnemyPoints(3) == 4 * game::kEnemyPointsStep);
}

TEST_CASE("An enemy waits in the house until its release time, then leaves", "[game][enemymodes]")
{
	EnemyBrain brain{.releaseSeconds = 2.0f};
	brain = game::TickHouse(brain, 1.5f);
	CHECK(brain.state == EnemyState::InHouse);
	CHECK(game::GoalFor(brain, ScheduledMode::Chase) == Goal::Stay);
	CHECK(game::EnemySpeedFor(brain) == 0.0f);

	brain = game::TickHouse(brain, 0.5f);
	CHECK(brain.state == EnemyState::Leaving);
	CHECK(game::GoalFor(brain, ScheduledMode::Chase) == Goal::Exit);
	CHECK(game::UsesDoor(brain));
	CHECK(game::EnemySpeedFor(brain) == game::kHouseSpeed);

	CHECK(game::TickHouse(brain, 1.0f).houseSeconds == brain.houseSeconds); // only counts in the house

	brain = game::ArriveOutside(brain);
	CHECK(brain.state == EnemyState::Roaming);
	CHECK_FALSE(game::UsesDoor(brain));
	CHECK(game::EnemySpeedFor(brain) == game::kEnemySpeed);
	CHECK_FALSE(brain.reversePending);
}

TEST_CASE("A roaming enemy patrols or chases with the schedule", "[game][enemymodes]")
{
	const EnemyBrain brain = InState(EnemyState::Roaming);
	CHECK(game::GoalFor(brain, ScheduledMode::Patrol) == Goal::Patrol);
	CHECK(game::GoalFor(brain, ScheduledMode::Chase) == Goal::Chase);
	CHECK(game::IsDangerous(brain));
	CHECK_FALSE(game::CanBeEaten(brain));
}

TEST_CASE("A schedule switch turns roaming enemies back, and only them", "[game][enemymodes]")
{
	CHECK(game::SwitchSchedule(InState(EnemyState::Roaming)).reversePending);
	CHECK_FALSE(game::SwitchSchedule(InState(EnemyState::Roaming, true)).reversePending);
	CHECK_FALSE(game::SwitchSchedule(InState(EnemyState::InHouse)).reversePending);
	CHECK_FALSE(game::SwitchSchedule(InState(EnemyState::Leaving)).reversePending);
	CHECK_FALSE(game::SwitchSchedule(InState(EnemyState::Returning)).reversePending);
}

TEST_CASE("A power pellet frightens enemies, and roaming ones turn back", "[game][enemymodes]")
{
	const EnemyBrain roaming = game::Frighten(InState(EnemyState::Roaming));
	CHECK(roaming.frightened);
	CHECK(roaming.reversePending);
	CHECK(game::GoalFor(roaming, ScheduledMode::Chase) == Goal::Wander);
	CHECK(game::EnemySpeedFor(roaming) == game::kFrightenedSpeed);
	CHECK(game::CanBeEaten(roaming));
	CHECK_FALSE(game::IsDangerous(roaming));

	// Already frightened: a second pellet doesn't turn it back again.
	EnemyBrain again = roaming;
	again.reversePending = false;
	CHECK_FALSE(game::Frighten(again).reversePending);

	const EnemyBrain inHouse = game::Frighten(InState(EnemyState::InHouse));
	CHECK(inHouse.frightened);
	CHECK_FALSE(inHouse.reversePending);

	const EnemyBrain returning = game::Frighten(InState(EnemyState::Returning));
	CHECK_FALSE(returning.frightened);
}

TEST_CASE("When the fright ends, enemies carry on without turning back", "[game][enemymodes]")
{
	const EnemyBrain calm = game::Calm(InState(EnemyState::Roaming, true));
	CHECK_FALSE(calm.frightened);
	CHECK_FALSE(calm.reversePending);
	CHECK(game::IsDangerous(calm));
}

TEST_CASE("An eaten enemy hurries home, then turns round and leaves again", "[game][enemymodes]")
{
	CHECK(game::Eat(InState(EnemyState::Roaming)).state == EnemyState::Roaming); // not frightened

	EnemyBrain brain = game::Eat(InState(EnemyState::Roaming, true));
	CHECK(brain.state == EnemyState::Returning);
	CHECK_FALSE(brain.frightened);
	CHECK_FALSE(game::IsDangerous(brain));
	CHECK_FALSE(game::CanBeEaten(brain));
	CHECK(game::UsesDoor(brain));
	CHECK(game::EnemySpeedFor(brain) == game::kReturningSpeed);
	CHECK(game::GoalFor(brain, ScheduledMode::Patrol) == Goal::Home);

	brain = game::ArriveHome(brain);
	CHECK(brain.state == EnemyState::Leaving);
	CHECK(brain.reversePending);
	CHECK(game::IsDangerous(brain));
	CHECK(game::ArriveHome(InState(EnemyState::Roaming)).state == EnemyState::Roaming);
}
