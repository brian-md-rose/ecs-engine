#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "editor/profilerpanel.hpp"

using engine::editor::CountsByPopulation;
using engine::editor::Profiler;
using engine::editor::ProfilerHistory;
using engine::editor::ProfilerSample;
using engine::editor::Summarize;
using engine::editor::SlowestFirst;

TEST_CASE("The Profiler history keeps the newest frames, oldest first", "[profiler]")
{
	ProfilerHistory history(3);
	for (int i = 1; i <= 5; ++i)
		history.Add({.frameMs = static_cast<float>(i), .updateMs = 0.0F, .entities = 0});
	const auto ordered = history.Ordered();
	REQUIRE(ordered.size() == 3);
	CHECK(ordered[0].frameMs == 3.0F);
	CHECK(ordered[2].frameMs == 5.0F);

	history.Clear();
	CHECK(history.Size() == 0);
	CHECK(history.Ordered().empty());
}

TEST_CASE("The Profiler summary averages and finds the worst frame", "[profiler]")
{
	CHECK(Summarize({}).fps == 0.0F);
	const auto summary = Summarize({{.frameMs = 10.0F, .updateMs = 2.0F}, {.frameMs = 30.0F, .updateMs = 4.0F}});
	CHECK(summary.averageFrameMs == 20.0F);
	CHECK(summary.maxFrameMs == 30.0F);
	CHECK(summary.averageUpdateMs == 3.0F);
	CHECK(summary.fps == 50.0F);
}

TEST_CASE("Component counts are listed biggest first, ties by name", "[profiler]")
{
	const auto sorted = CountsByPopulation({{"b", 2}, {"a", 2}, {"c", 9}, {"d", 0}});
	REQUIRE(sorted.size() == 4);
	CHECK(sorted[0].name == "c");
	CHECK(sorted[1].name == "a");
	CHECK(sorted[2].name == "b");
	CHECK(sorted[3].name == "d");
}

TEST_CASE("The Profiler hands the update time to the next frame once", "[profiler]")
{
	Profiler profiler;
	CHECK(profiler.TakeUpdateMs(10) == 0.0); // the first frame has no earlier one
	profiler.AddUpdateMs(1.5);
	profiler.AddUpdateMs(0.5);
	CHECK(profiler.TakeUpdateMs(11) == 2.0);
	CHECK(profiler.TakeUpdateMs(12) == 0.0);
}

TEST_CASE("The Profiler drops update time that piled up while the panel was closed", "[profiler]")
{
	Profiler profiler;
	CHECK(profiler.TakeUpdateMs(1) == 0.0);
	profiler.AddUpdateMs(5.0);
	profiler.AddUpdateMs(5.0);
	CHECK(profiler.TakeUpdateMs(50) == 0.0); // frames 2..49 were not drawn
	profiler.AddUpdateMs(1.0);
	CHECK(profiler.TakeUpdateMs(51) == 1.0);
}

TEST_CASE("Systems are listed slowest first, equal times keeping run order", "[profiler]")
{
	using engine::ecs::Phase;
	const auto sorted = engine::editor::SlowestFirst({{.phase = Phase::Update, .name = "a", .averageMs = 0.5},
	                                                  {.phase = Phase::Update, .name = "b", .averageMs = 2.0},
	                                                  {.phase = Phase::Render, .name = "c", .averageMs = 0.5},
	                                                  {.phase = Phase::Input, .name = "d", .averageMs = 0.0}});
	REQUIRE(sorted.size() == 4);
	CHECK(sorted[0].name == "b");
	CHECK(sorted[1].name == "a");
	CHECK(sorted[2].name == "c");
	CHECK(sorted[3].name == "d");
}
