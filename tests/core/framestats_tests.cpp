#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "core/framestats.hpp"

namespace
{
	using engine::core::FrameStats;
	using namespace std::chrono_literals;
} // namespace

TEST_CASE("FrameStats reports frames per second and draw time once a second", "[core][framestats]")
{
	const FrameStats::Clock::time_point start{};
	FrameStats stats;
	stats.Reset(start);
	CHECK_FALSE(stats.HasReport());

	// 59 frames in the first second: no report yet.
	for (int i = 1; i < 60; ++i)
	{
		CHECK_FALSE(stats.AddFrame(2ms, 100, start + i * 16ms));
	}
	CHECK_FALSE(stats.HasReport());

	// The 60th frame, a little past one second, closes the window.
	CHECK(stats.AddFrame(2ms, 120, start + 1000ms));
	REQUIRE(stats.HasReport());
	CHECK(stats.Fps() == Catch::Approx(60.0));
	CHECK(stats.AverageDrawMs() == Catch::Approx(2.0));
	CHECK(stats.Items() == 120);

	// The next window starts afresh; the report stays until it closes.
	CHECK_FALSE(stats.AddFrame(8ms, 5, start + 1500ms));
	CHECK(stats.Fps() == Catch::Approx(60.0));
	CHECK(stats.AddFrame(8ms, 5, start + 2000ms));
	CHECK(stats.Fps() == Catch::Approx(2.0));
	CHECK(stats.AverageDrawMs() == Catch::Approx(8.0));
}
