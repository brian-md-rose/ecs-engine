#include "mazegame.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets.hpp"
#include "components.hpp"
#include "controls.hpp"
#include "core/assert.hpp"
#include "core/paths.hpp"
#include "difficulty.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "enemymodes.hpp"
#include "enemysystems.hpp"
#include "events.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/camera.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "gridmovement.hpp"
#include "hud.hpp"
#include "scoring.hpp"
#include "screens.hpp"
#include "systems.hpp"
#include "targeting.hpp"

namespace game
{
	namespace
	{
		constexpr std::string_view kFirstMaze = "level1.txt";

		// Loads assets/mazes/<fileName> from next to the executable (see game/assets/README.md).
		// On failure, prints the error with the file, line and column to stderr and returns
		// nothing.
		[[nodiscard]] std::optional<TileMap> LoadMazeAsset(const std::filesystem::path& folder,
		                                                   std::string_view fileName)
		{
			const std::filesystem::path file = folder / "mazes" / fileName;
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
		const std::filesystem::path folder = engine::core::ExecutableDirectory() / "assets";
		const std::optional<TileMap> maze = LoadMazeAsset(folder, kFirstMaze);
		const std::optional<GameAssets> assets = LoadGameAssets(folder);
		if (!maze || !assets)
		{
			valid = false;
			return;
		}
		Build(*maze, *assets);
	}

	MazeGame::MazeGame(const TileMap& maze, const GameAssets& assets)
	{
		Build(maze, assets);
	}

	bool MazeGame::WantsToQuit() const
	{
		const GameState* state = world.resources.TryGet<GameState>();
		return state != nullptr && state->quitRequested;
	}

	void MazeGame::Build(const TileMap& maze, const GameAssets& assets)
	{
		using engine::ecs::Phase;
		using engine::graphics::Shape;
		using engine::graphics::Transform;

		world.resources.Emplace<Controls>();
		world.resources.Emplace<GameState>();
		world.resources.Emplace<GameFlow>(GameFlow{.stage = Stage::Title});
		world.resources.Emplace<MenuCursor>();
		world.resources.Emplace<Score>();
		world.resources.Emplace<FrameStats>();
		world.resources.Emplace<ModeClock>();
		world.resources.Emplace<EnemyDice>();
		world.resources.Emplace<HighScore>();
		const DifficultyTable& difficulty = world.resources.Emplace<DifficultyTable>(assets.difficulty);
		auto& renderer2D = world.resources.Emplace<engine::graphics::Renderer2D>(
		    engine::graphics::Camera{.logicalWidth = kLogicalWidth, .logicalHeight = kLogicalHeight});
		renderer2D.SetAtlas(assets.atlas);
		renderer2D.SetFont(assets.font);
		const TileMap& map = world.resources.Emplace<TileMap>(maze);
		world.resources.Emplace<EnemyHouse>(EnemyHouse{.house = FindHouse(map)});

		engine::ecs::Registry& registry = world.registry;
		const engine::graphics::Position origin = MazeOrigin(map);
		SpawnMaze(registry, map, origin);

		const std::vector<TilePosition> spawns = map.Find(Tile::PlayerSpawn);
		ENGINE_VERIFY(spawns.size() == 1, "the maze needs exactly one player spawn; ParseTileMap checks this");
		const engine::ecs::Entity player = registry.Create();
		const GridMover mover{.tile = spawns.front(), .speed = difficulty.ForLevel(1).playerSpeed};
		registry.Add<Transform>(player, MoverTopLeft(origin, mover));
		registry.Add<GridMover>(player, mover);
		registry.Add<Spawn>(player, Spawn{.tile = mover.tile});
		registry.Add<PlayerControlled>(player);
		registry.Add<Shape>(
		    player, Shape{.width = kPlayerSize, .height = kPlayerSize, .color = {255, 210, 0}, .layer = kPlayerLayer});
		SpawnEnemies(registry, map, origin);
		SpawnHud(registry, map, origin);
		SpawnScreens(registry);

		// The whole frame, in run order.
		scheduler.Add(Phase::Input, ControlsSystem);
		scheduler.Add(Phase::Input, ScreenSystem);
		scheduler.Add(Phase::Input, GameStateSystem);
		scheduler.Add(Phase::Update, GameFlowSystem, NotPaused);
		scheduler.Add(Phase::Update, PlayerInputSystem, IsPlaying);
		scheduler.Add(Phase::Update, EnemyModeSystem, IsPlaying);
		scheduler.Add(Phase::Update, EnemySteeringSystem, IsPlaying);
		scheduler.Add(Phase::Update, GridMovementSystem, IsPlaying);
		scheduler.Add(Phase::LateUpdate, CollisionSystem, IsPlaying);
		scheduler.Add(Phase::LateUpdate, EnemyLookSystem);
		scheduler.Add(Phase::LateUpdate, HudSystem);
		scheduler.Add(Phase::LateUpdate, ScreenTextSystem);
		scheduler.Add(Phase::Render, RenderSystem);

		// Who reacts to what, in delivery order.
		world.events.Subscribe<PelletEaten>(AddPelletScore);
		world.events.Subscribe<PelletEaten>(CheckLevelCleared<PelletEaten>);
		world.events.Subscribe<PowerPelletEaten>(AddPowerPelletScore);
		world.events.Subscribe<PowerPelletEaten>(CheckLevelCleared<PowerPelletEaten>);
		world.events.Subscribe<PowerPelletEaten>(FrightenEnemies);
		world.events.Subscribe<EnemyEaten>(AddEnemyScore);
		world.events.Subscribe<EnemyEaten>(SendEnemyHome);
		world.events.Subscribe<PlayerCaught>(LoseLife);
		world.events.Subscribe<LevelCleared>(CompleteLevel);
		world.events.Subscribe<StageChanged>(ResetRound);
		world.events.Subscribe<StageChanged>(ResetEnemies);
	}
} // namespace game
