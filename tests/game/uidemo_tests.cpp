#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "graphics/font.hpp"
#include "graphics/transform.hpp"
#include "input/input.hpp"
#include "input/mouse.hpp"
#include "mazegame.hpp"
#include "options.hpp"
#include "stage.hpp"
#include "tilemap.hpp"
#include "ui/ninepatch.hpp"
#include "ui/ui.hpp"
#include "uidemo.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::input::Key;
	using engine::input::MouseButton;
	using gametest::Stage;

	constexpr double kStep = 1.0 / 60.0;

	// The centres of the demo's three buttons, in logical pixels.
	constexpr engine::graphics::Position kCountButton{48.0f, 60.0f};
	constexpr engine::graphics::Position kResetButton{112.0f, 60.0f};
	constexpr engine::graphics::Position kBackButton{176.0f, 60.0f};
	constexpr engine::graphics::Position kListPoint{100.0f, 150.0f};

	[[nodiscard]] game::TileMap TestMaze()
	{
		game::TileMapResult result = game::ParseTileMap("#######\n"
		                                                "#P....#\n"
		                                                "#######\n"
		                                                "#E#####\n"
		                                                "#######\n");
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	struct Demo
	{
		game::MazeGame maze{TestMaze()};
		engine::input::Input input;

		Demo()
		{
			Update(2);
			maze.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(game::Screen::UiDemo);
			Update(2);
		}

		void Update(int steps = 1)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, input);
				input.EndUpdate();
			}
		}

		void Tap(Key key)
		{
			input.keyboard.OnKeyDown(key);
			Update();
			input.keyboard.OnKeyUp(key);
			Update();
		}

		void MoveTo(engine::graphics::Position where)
		{
			input.mouse.OnMove(where, true);
			Update();
		}

		void Click(engine::graphics::Position where, MouseButton button = MouseButton::Left)
		{
			MoveTo(where);
			input.mouse.OnButton(button, true);
			Update();
			input.mouse.OnButton(button, false);
			Update();
		}

		[[nodiscard]] Stage CurrentStage() const { return gametest::StageOf(maze); }

		[[nodiscard]] int Clicks() { return maze.world.resources.Get<game::UiDemoState>().clicks; }

		[[nodiscard]] std::string CounterText()
		{
			std::string text;
			maze.world.registry.Each<game::DemoCounter, engine::graphics::Text>(
			    [&text](Entity, const game::DemoCounter&, const engine::graphics::Text& shown)
			{
				text = shown.value;
			});
			return text;
		}

		template <typename T> [[nodiscard]] std::size_t Count()
		{
			std::size_t count = 0;
			maze.world.registry.Each<T>([&count](Entity, const T&)
			{
				++count;
			});
			return count;
		}

		[[nodiscard]] const engine::ui::ScrollView& List()
		{
			const engine::ui::ScrollView* found = nullptr;
			maze.world.registry.Each<engine::ui::ScrollView>([&found](Entity, const engine::ui::ScrollView& view)
			{
				found = &view;
			});
			REQUIRE(found != nullptr);
			return *found;
		}
	};
} // namespace

TEST_CASE("The demo's rows differ and some are long enough to wrap", "[game][uidemo]")
{
	std::set<std::string> rows;
	std::size_t longest = 0;
	for (int row = 1; row <= game::kDemoRows; ++row)
	{
		const std::string text = game::DemoRowText(row);
		CHECK_FALSE(text.empty());
		rows.insert(text);
		longest = std::max(longest, text.size());
	}
	CHECK(rows.size() == static_cast<std::size_t>(game::kDemoRows));
	CHECK(longest * 6 > 176); // wider than a row, so wrapped in the list
	CHECK(game::DemoCounterText(7) == "CLICKS: 7");
}

TEST_CASE("The demo opens from the Options screen and ESC goes back", "[game][uidemo]")
{
	game::MazeGame maze{TestMaze()};
	engine::input::Input input;
	const auto update = [&](int steps = 1)
	{
		for (int i = 0; i < steps; ++i)
		{
			maze.Update(kStep, input);
			input.EndUpdate();
		}
	};
	const auto tap = [&](Key key)
	{
		input.keyboard.OnKeyDown(key);
		update();
		input.keyboard.OnKeyUp(key);
		update();
	};
	update(2);
	tap(Key::Down);
	tap(Key::Enter); // OPTIONS
	REQUIRE(gametest::StageOf(maze) == Stage::Options);
	tap(Key::Down);
	tap(Key::Down); // UI DEMO
	tap(Key::Enter);
	CHECK(gametest::StageOf(maze) == Stage::UiDemo);
	CHECK_FALSE(maze.ShowSystemCursor());

	tap(Key::Escape);
	CHECK(gametest::StageOf(maze) == Stage::Options);
	CHECK(maze.ShowSystemCursor());
}

TEST_CASE("The demo's buttons count, reset and go back", "[game][uidemo]")
{
	Demo d;
	REQUIRE(d.CurrentStage() == Stage::UiDemo);
	CHECK(d.Count<game::DemoButton>() == 3);
	CHECK(d.CounterText() == "CLICKS: 0");

	d.Click(kCountButton);
	d.Click(kCountButton);
	d.Click(kCountButton);
	CHECK(d.Clicks() == 3);
	CHECK(d.CounterText() == "CLICKS: 3");

	d.Click(kCountButton, MouseButton::Right); // only the left button counts
	CHECK(d.Clicks() == 3);

	d.Click(kResetButton);
	CHECK(d.Clicks() == 0);

	d.Click(kBackButton);
	d.Update(2);
	CHECK(d.CurrentStage() == Stage::Options);
}

TEST_CASE("A button changes its picture while the pointer is on it and while it is held", "[game][uidemo]")
{
	Demo d;
	d.maze.world.resources.Get<game::SpriteClips>().button.firstFrame = {.x = 1, .y = 0, .width = 9, .height = 9};
	d.maze.world.resources.Get<game::SpriteClips>().buttonHover.firstFrame = {.x = 2, .y = 0, .width = 9, .height = 9};
	d.maze.world.resources.Get<game::SpriteClips>().buttonPressed.firstFrame = {
	    .x = 3, .y = 0, .width = 9, .height = 9};

	const auto sourceX = [&d]
	{
		int x = -1;
		d.maze.world.registry.Each<game::DemoButton, engine::ui::NinePatch>(
		    [&x](Entity, const game::DemoButton& button, const engine::ui::NinePatch& patch)
		{
			if (button.action == game::DemoAction::Count) x = patch.source.x;
		});
		return x;
	};

	d.MoveTo({300.0f, 300.0f});
	CHECK(sourceX() == 1);
	d.MoveTo(kCountButton);
	CHECK(sourceX() == 2);
	d.input.mouse.OnButton(MouseButton::Left, true);
	d.Update();
	CHECK(sourceX() == 3);
	d.input.mouse.OnButton(MouseButton::Left, false);
	d.Update();
}

TEST_CASE("The wheel scrolls the list, and not past either end", "[game][uidemo]")
{
	Demo d;
	const engine::ui::ScrollView& list = d.List();
	CHECK(list.contentHeight > list.viewHeight);
	CHECK(list.offset == 0.0f);

	d.MoveTo(kListPoint);
	d.input.mouse.OnWheel(-2.0f); // away from the user is up, toward is down
	d.Update();
	CHECK(list.offset == 2.0f * list.wheelStep);

	d.input.mouse.OnWheel(1000.0f);
	d.Update();
	CHECK(list.offset == 0.0f);

	d.input.mouse.OnWheel(-1000.0f);
	d.Update();
	CHECK(list.offset == list.contentHeight - list.viewHeight);

	d.MoveTo({200.0f, 270.0f}); // off the list: the wheel does nothing
	const float where = list.offset;
	d.input.mouse.OnWheel(1.0f);
	d.Update();
	CHECK(list.offset == where);
}

TEST_CASE("The cursor follows the mouse and leaves the screen with it", "[game][uidemo]")
{
	Demo d;
	const auto cursorAt = [&d]
	{
		engine::graphics::Position at{};
		d.maze.world.registry.Each<game::DemoCursor, engine::graphics::Transform>(
		    [&at](Entity, const game::DemoCursor&, const engine::graphics::Transform& transform)
		{
			at = transform.position;
		});
		return at;
	};

	d.MoveTo({90.0f, 120.0f});
	CHECK(cursorAt().x == 90.0f);
	CHECK(cursorAt().y == 120.0f);

	d.input.mouse.OnMove({90.0f, 120.0f}, false);
	d.Update();
	CHECK(cursorAt().x < 0.0f);
}

TEST_CASE("Leaving the demo takes everything it spawned with it, nine-patch pieces too", "[game][uidemo]")
{
	Demo d;
	CHECK(d.Count<engine::ui::NinePatchChild>() > 0);
	CHECK(d.Count<game::DemoButton>() == 3);

	d.Tap(Key::Escape);
	d.Update(2);
	CHECK(d.CurrentStage() == Stage::Options);
	CHECK(d.Count<game::DemoButton>() == 0);
	CHECK(d.Count<game::DemoCursor>() == 0);
	CHECK(d.Count<engine::ui::ScrollView>() == 0);
	CHECK(d.Count<engine::ui::NinePatchChild>() == 0);

	// And it can be entered again from a clean start.
	d.maze.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(game::Screen::UiDemo);
	d.Update(2);
	CHECK(d.Count<game::DemoButton>() == 3);
	CHECK(d.Clicks() == 0);
}
