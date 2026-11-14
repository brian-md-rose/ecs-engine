#include "mazegame.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components.hpp"
#include "controls.hpp"
#include "core/assert.hpp"
#include "core/paths.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "gamestate.hpp"
#include "graphics/camera.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "gridmovement.hpp"
#include "systems.hpp"

namespace game
{
	namespace
	{
		constexpr std::string_view kFirstMaze = "level1.txt";

		// Loads assets/mazes/<fileName> from next to the executable (see game/assets/README.md).
		// On failure, prints the error with the file, line and column to stderr and returns
		// nothing.
		[[nodiscard]] std::optional<TileMap> LoadMazeAsset(std::string_view fileName)
		{
			const std::filesystem::path file = engine::core::ExecutableDirectory() / "assets" / "mazes" / fileName;
			TileMapResult result = LoadTileMap(file);
			if (!result.map)
			{
				// The full path, so a missing asset shows where the game looked. UTF-8 like the rest.
				const std::u8string path = file.u8string();
				const std::string source(reinterpret_cast<const char*>(path.c_str()), path.size());
				std::fprintf(stderr, "%s\n", FormatError(source, result.error).c_str());
			}
			return std::move(result.map);
		}
	} // namespace

	engine::graphics::Position MazeOrigin(const TileMap& maze)
	{
		const float mazeWidth = static_cast<float>(maze.Width()) * kTileSize;
		return {(static_cast<float>(kLogicalWidth) - mazeWidth) / 2.0f, static_cast<float>(kHudRows) * kTileSize};
	}

	MazeGame::MazeGame()
	{
		const std::optional<TileMap> maze = LoadMazeAsset(kFirstMaze);
		if (!maze)
		{
			valid = false;
			return;
		}
		Build(*maze);
	}

	MazeGame::MazeGame(const TileMap& maze)
	{
		Build(maze);
	}

	void MazeGame::Build(const TileMap& maze)
	{
		using engine::ecs::Phase;
		using engine::graphics::Shape;
		using engine::graphics::Transform;

		world.resources.Emplace<Controls>();
		world.resources.Emplace<GameState>();
		world.resources.Emplace<FrameStats>();
		world.resources.Emplace<engine::graphics::Renderer2D>(
		    engine::graphics::Camera{.logicalWidth = kLogicalWidth, .logicalHeight = kLogicalHeight});
		const TileMap& map = world.resources.Emplace<TileMap>(maze);

		engine::ecs::Registry& registry = world.registry;
		const engine::graphics::Position origin = MazeOrigin(map);
		SpawnMaze(registry, map, origin);

		const std::vector<TilePosition> spawns = map.Find(Tile::PlayerSpawn);
		ENGINE_VERIFY(spawns.size() == 1, "the maze needs exactly one player spawn; ParseTileMap checks this");
		const engine::ecs::Entity player = registry.Create();
		const GridMover mover{.tile = spawns.front(), .speed = kPlayerSpeed};
		registry.Add<Transform>(player, MoverTopLeft(origin, mover));
		registry.Add<GridMover>(player, mover);
		registry.Add<PlayerControlled>(player);
		registry.Add<Shape>(
		    player, Shape{.width = kPlayerSize, .height = kPlayerSize, .color = {255, 210, 0}, .layer = kPlayerLayer});

		// The whole frame, in run order.
		scheduler.Add(Phase::Input, ControlsSystem);
		scheduler.Add(Phase::Input, GameStateSystem);
		scheduler.Add(Phase::Update, PlayerInputSystem, NotPaused);
		scheduler.Add(Phase::Update, GridMovementSystem, NotPaused);
		scheduler.Add(Phase::Render, RenderSystem);
	}
} // namespace game
