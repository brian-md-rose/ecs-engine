#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "core/framestats.hpp"
#include "debug/debugpanel.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/font.hpp"
#include "graphics/shape.hpp"
#include "input/keyboard.hpp"

namespace
{
	using engine::debug::DebugPanel;
	using engine::debug::DebugPanelView;
	using engine::ecs::Phase;
	using engine::ecs::Registry;
	using engine::ecs::Scheduler;

	[[nodiscard]] std::vector<std::string> Lines(const std::string& text)
	{
		std::vector<std::string> lines(1);
		for (const char c : text)
		{
			if (c == '\n')
			{
				lines.emplace_back();
			}
			else
			{
				lines.back() += c;
			}
		}
		return lines;
	}
} // namespace

TEST_CASE("The debug panel lists the slowest systems and every component", "[debug][panel]")
{
	const std::vector<Scheduler::SystemTiming> systems = {
	    {.phase = Phase::Update, .name = "Fast", .averageMs = 0.01},
	    {.phase = Phase::Update, .name = "Off", .averageMs = 9.0, .skipped = true},
	    {.phase = Phase::Render, .name = "Slow", .averageMs = 1.5},
	    {.phase = Phase::Input, .name = "Medium", .averageMs = 0.25},
	};
	const std::vector<Registry::ComponentCount> components = {
	    {.name = "Pellet", .count = 240}, {.name = "Sprite", .count = 300}, {.name = "Wall", .count = 7}};
	const DebugPanelView view{.entities = 550, .components = components, .systems = systems};

	const std::vector<std::string> lines = Lines(engine::debug::DebugPanelText(view, 3, 30));
	REQUIRE(lines.size() == 11);
	CHECK(lines[0] == "FPS --"); // no frame stats yet
	CHECK(lines[1] == "ENTITIES 550");
	CHECK(lines[2].empty());
	CHECK(lines[3] == "SYSTEMS 4                   MS");
	CHECK(lines[4] == "SLOW                     1.500"); // slowest first, in capitals
	CHECK(lines[5] == "MEDIUM                   0.250");
	CHECK(lines[6] == "FAST                     0.010"); // only 3: Off, skipped, comes last
	CHECK(lines[7].empty());
	CHECK(lines[8] == "COMPONENTS 3");
	CHECK(lines[9] == "PELLET     240 SPRITE      300"); // two to a line
	CHECK(lines[10] == "WALL         7");
	for (const std::string& line : lines)
	{
		CHECK(line.size() <= 30);
	}
}

TEST_CASE("The debug panel shows frame stats once there are some", "[debug][panel]")
{
	using namespace std::chrono_literals;
	engine::core::FrameStats frames;
	const engine::core::FrameStats::Clock::time_point start{};
	frames.Reset(start);
	frames.AddFrame(1ms, 42, start + 500ms);
	REQUIRE(frames.AddFrame(1ms, 42, start + 1000ms));

	const DebugPanelView view{.frames = &frames};
	CHECK(Lines(engine::debug::DebugPanelText(view, 0, 37))[0] == "FPS 2  DRAW 1.00 MS  42 ITEMS");
}

TEST_CASE("Long names are cut short to fit the panel", "[debug][panel]")
{
	const std::vector<Scheduler::SystemTiming> systems = {
	    {.phase = Phase::Update, .name = "AVeryLongSystemNameIndeed", .averageMs = 12.345}};
	const DebugPanelView view{.systems = systems};
	CHECK(Lines(engine::debug::DebugPanelText(view, 1, 20))[4] == "AVERYLONGSYST 12.345");
}

TEST_CASE("The toggle key shows the panel's entities and hides them again", "[debug][panel]")
{
	engine::ecs::World world;
	Scheduler scheduler;
	engine::debug::AddDebugPanel(world, scheduler);
	CHECK(world.resources.Has<engine::core::FrameStats>());
	CHECK(scheduler.SystemCount(Phase::Input) == 1);
	CHECK(scheduler.SystemCount(Phase::Render) == 1);

	engine::input::Keyboard keyboard;
	const auto update = [&]()
	{
		scheduler.Run(Phase::Input, world, engine::ecs::FrameContext{.keyboard = &keyboard});
		keyboard.EndUpdate();
	};

	update();
	const DebugPanel& panel = world.resources.Get<DebugPanel>();
	CHECK_FALSE(panel.visible);

	keyboard.OnKeyDown(engine::input::Key::F3);
	update();
	REQUIRE(panel.visible);
	CHECK(world.registry.HasComponent<engine::graphics::Text>(panel.text));
	CHECK(world.registry.HasComponent<engine::graphics::Shape>(panel.background));
	const engine::ecs::Entity text = panel.text;

	keyboard.OnKeyUp(engine::input::Key::F3);
	keyboard.OnKeyDown(engine::input::Key::F3);
	update();
	CHECK_FALSE(panel.visible);
	CHECK_FALSE(world.registry.IsAlive(text));
}
