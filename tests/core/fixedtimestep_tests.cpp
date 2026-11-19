#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/clock.hpp"
#include "core/fixedtimestep.hpp"

namespace
{
	using Catch::Matchers::WithinAbs;
	using engine::core::FixedTimestep;

	constexpr double kStep = 1.0 / 60.0;

	// A clock the test moves by hand.
	class FakeClock final : public engine::core::Clock
	{
		double now = 100.0;

	public:
		[[nodiscard]] double Now() const override { return now; }
		void Advance(double seconds) { now += seconds; }
	};

	// Runs the same loop as Application::Run for `seconds` of real time at `fps`, and
	// returns how many fixed updates happened.
	int SimulateUpdates(double fps, double seconds)
	{
		FakeClock clock;
		FixedTimestep timestep(kStep);
		double previous = clock.Now();
		int updates = 0;

		const int frames = static_cast<int>(fps * seconds);
		for (int i = 0; i < frames; ++i)
		{
			clock.Advance(1.0 / fps);
			const double now = clock.Now();
			updates += timestep.Advance(now - previous);
			previous = now;
		}
		return updates;
	}
} // namespace

TEST_CASE("FixedTimestep runs one update per step of elapsed time", "[core][timestep]")
{
	FixedTimestep timestep(kStep);

	CHECK(timestep.Step() == kStep);
	CHECK(timestep.Advance(kStep) == 1);
	CHECK_THAT(timestep.Alpha(), WithinAbs(0.0, 1e-9));
}

TEST_CASE("FixedTimestep carries short frames over", "[core][timestep]")
{
	FixedTimestep timestep(kStep);

	CHECK(timestep.Advance(kStep / 2.0) == 0);
	CHECK_THAT(timestep.Alpha(), WithinAbs(0.5, 1e-9));

	CHECK(timestep.Advance(kStep / 2.0) == 1);
	CHECK_THAT(timestep.Alpha(), WithinAbs(0.0, 1e-9));
}

TEST_CASE("FixedTimestep catches up after a long frame", "[core][timestep]")
{
	FixedTimestep timestep(kStep);

	// 3.3 steps of time: three updates now, 0.3 of a step left over.
	CHECK(timestep.Advance(kStep * 3.3) == 3);
	CHECK_THAT(timestep.Alpha(), WithinAbs(0.3, 1e-9));

	// The leftover counts towards the next frame: 0.3 + 0.75 crosses one step. (Not 0.3 + 0.7:
	// landing exactly on a step boundary is at the mercy of floating-point rounding.)
	CHECK(timestep.Advance(kStep * 0.75) == 1);
	CHECK_THAT(timestep.Alpha(), WithinAbs(0.05, 1e-9));
}

TEST_CASE("FixedTimestep clamps a stall instead of spiralling", "[core][timestep]")
{
	FixedTimestep timestep(kStep, 0.25);

	// A 10-second breakpoint is treated as 0.25 s: 15 updates, not 600.
	CHECK(timestep.Advance(10.0) == 15);
}

TEST_CASE("FixedTimestep ignores a clock going backwards", "[core][timestep]")
{
	FixedTimestep timestep(kStep);

	CHECK(timestep.Advance(-1.0) == 0);
	CHECK_THAT(timestep.Alpha(), WithinAbs(0.0, 1e-9));
	CHECK(timestep.Advance(kStep) == 1);
}

TEST_CASE("FixedTimestep gives the same update rate at any frame rate", "[core][timestep]")
{
	// One second of play is 60 updates whether the screen runs at 30, 60, 144 or 240 Hz.
	// Allow one update of slack for floating-point rounding at the boundary.
	for (const double fps : {30.0, 60.0, 144.0, 240.0})
	{
		CAPTURE(fps);
		const int updates = SimulateUpdates(fps, 1.0);
		CHECK(updates >= 59);
		CHECK(updates <= 60);
	}
}

TEST_CASE("SystemClock never goes backwards", "[core][clock]")
{
	const engine::core::SystemClock clock;
	const double first = clock.Now();
	const double second = clock.Now();
	CHECK(second >= first);
}

TEST_CASE("FixedTimestep scales game speed", "[core][timestep]")
{
	FixedTimestep timestep(kStep);

	timestep.SetTimeScale(0.5);
	CHECK(timestep.Advance(kStep * 4.0) == 2);

	timestep.SetTimeScale(2.0);
	CHECK(timestep.Advance(kStep * 2.0) == 4);

	timestep.SetTimeScale(0.0);
	CHECK(timestep.Advance(1.0) == 0);
}

TEST_CASE("FixedTimestep still caps a stall when scaled up", "[core][timestep]")
{
	FixedTimestep timestep(kStep, 0.25);
	timestep.SetTimeScale(2.0);

	// The cap applies to real time first: 0.25 s of it at double speed is 30 updates, not 1200.
	CHECK(timestep.Advance(10.0) == 30);
}

TEST_CASE("FixedTimestep pauses without replaying the paused time", "[core][timestep]")
{
	FixedTimestep timestep(kStep);

	CHECK(timestep.Advance(kStep * 0.5) == 0);
	const double alpha = timestep.Alpha();

	timestep.SetPaused(true);
	CHECK(timestep.Paused());
	CHECK(timestep.Advance(5.0) == 0);
	CHECK_THAT(timestep.Alpha(), WithinAbs(alpha, 1e-9));

	timestep.SetPaused(false);
	CHECK(timestep.Advance(kStep * 0.5) == 1);
}

TEST_CASE("A limit on updates per frame stops a slow frame snowballing", "[fixedtimestep]")
{
	FixedTimestep timestep(kStep, 0.25);
	timestep.SetTimeScale(4.0);
	CHECK(timestep.MaxUpdates() == 0);
	CHECK(timestep.Advance(10.0) >= 59); // 0.25 s at x4 is 60 updates (59 with rounding) without a limit

	timestep.SetMaxUpdates(15);
	CHECK(timestep.Advance(10.0) == 15);
	// The backlog beyond the limit is dropped, not carried to the next frame.
	CHECK(timestep.Advance(0.0) == 0);
	CHECK(timestep.Alpha() < 1.0);
	CHECK(timestep.Alpha() >= 0.0);

	// Frames within the limit are untouched.
	CHECK(timestep.Advance(kStep) >= 3); // 4 steps of game time, give or take rounding
}

TEST_CASE("The update limit leaves normal frames alone", "[fixedtimestep]")
{
	FixedTimestep timestep(kStep, 0.25);
	timestep.SetMaxUpdates(15);
	CHECK(timestep.Advance(10.0) == 15); // the 0.25 s stall cap gives exactly 15 at x1 anyway
	CHECK(timestep.Advance(kStep * 3.5) == 3);
	timestep.SetMaxUpdates(-4); // nonsense means no limit
	CHECK(timestep.MaxUpdates() == 0);
}
