#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "ecs/enginecomponents.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "ecs/scene.hpp"
#include "ecs/states.hpp"
#include "ecs/typeregistry.hpp"
#include "graphics/font.hpp"
#include "hud.hpp"
#include "input/input.hpp"
#include "mazegame.hpp"
#include "menuui.hpp"
#include "scenes.hpp"
#include "screens.hpp"
#include "testassets.hpp"
#include "tilemap.hpp"
#include "ui/ui.hpp"

namespace
{
	using engine::ecs::Entity;

	constexpr double kStep = 1.0 / 60.0;

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

	void Update(game::MazeGame& maze, engine::input::Input& input, int steps = 1)
	{
		for (int i = 0; i < steps; ++i)
		{
			maze.Update(kStep, input);
			input.EndUpdate();
		}
	}

	[[nodiscard]] int CountTexts(game::MazeGame& maze)
	{
		int count = 0;
		maze.world.registry.Each<engine::graphics::Text>([&count](Entity, const engine::graphics::Text&)
		{
			++count;
		});
		return count;
	}
} // namespace

TEST_CASE("Every scene file loads with no errors and no warnings", "[game][scenes]")
{
	engine::ecs::TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	game::RegisterGameComponents(types);

	const game::GameAssets assets = gametest::SceneAssets();
	for (const char* name : {"title", "help", "options", "nameentry", "leveltransition", "pause", "uidemo"})
	{
		CAPTURE(name);
		const auto found = assets.scenes.find(name);
		REQUIRE(found != assets.scenes.end());

		engine::ecs::Registry registry;
		const auto result = engine::ecs::LoadScene(registry, types, found->second);
		CHECK(result.Ok());
		CHECK(result.issues.warnings.empty());
		CHECK_FALSE(result.entities.empty());
	}
}

TEST_CASE("A scene that is missing leaves its screen empty instead of crashing", "[game][scenes]")
{
	game::MazeGame maze{TestMaze()}; // no scene files at all
	engine::input::Input input;
	Update(maze, input, 3);
	CHECK(CountTexts(maze) == 0);
}

TEST_CASE("The title screen hides the high score until there is one", "[game][scenes]")
{
	game::MazeGame maze{TestMaze(), gametest::SceneAssets()};
	engine::input::Input input;
	Update(maze, input, 3);

	int highScoreTexts = 0;
	maze.world.registry.Each<game::ScreenText, engine::graphics::Text>(
	    [&](Entity, const game::ScreenText& screen, const engine::graphics::Text& text)
	{
		if (screen.kind != game::TextKind::HighScoreLabel && screen.kind != game::TextKind::HighScoreValue) return;
		++highScoreTexts;
		CHECK(text.value.empty());
	});
	CHECK(highScoreTexts == 2);
}

TEST_CASE("The how-to-play list is spawned under the scene's content, and scrolls", "[game][scenes]")
{
	game::MazeGame maze{TestMaze(), gametest::SceneAssets()};
	engine::input::Input input;
	Update(maze, input, 2);
	maze.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(game::Screen::Help);
	Update(maze, input, 2);

	auto& registry = maze.world.registry;
	int views = 0;
	registry.Each<game::HelpList, engine::ui::ScrollView>(
	    [&](Entity view, const game::HelpList&, const engine::ui::ScrollView& scroll)
	{
		++views;
		CHECK(registry.TryGetComponent<engine::ui::Focused>(view) != nullptr);
		CHECK(scroll.contentHeight > scroll.viewHeight);

		int rows = 0;
		registry.Each<engine::ecs::Parent>([&](Entity, const engine::ecs::Parent& parent)
		{
			if (parent.entity == scroll.content) ++rows;
		});
		CHECK(rows == static_cast<int>(game::HelpLines().size()));
	});
	CHECK(views == 1);
}
