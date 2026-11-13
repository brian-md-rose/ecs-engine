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
