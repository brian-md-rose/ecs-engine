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
#include "core/text.hpp"
#include "debug/debugpanel.hpp"
#include "difficulty.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "enemymodes.hpp"
#include "enemysystems.hpp"
#include "events.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/animation.hpp"
#include "graphics/animationsystem.hpp"
#include "graphics/camera.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "gridmovement.hpp"
#include "hud.hpp"
#include "looks.hpp"
#include "save.hpp"
#include "scoring.hpp"
#include "screens.hpp"
#include "sounds.hpp"
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
				// The full path, so a missing asset shows where the game looked.
				const std::string message = engine::core::FormatError(engine::core::PathText(file), result.error);
				std::fprintf(stderr, "%s\n", message.c_str());
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

		SaveFile& save = world.resources.Get<SaveFile>();
		save.path = DefaultSaveFile();
		save.savedHighScore = LoadHighScore(save.path);
		world.resources.Get<HighScore>().points = save.savedHighScore;
	}

	MazeGame::MazeGame(const TileMap& maze, const GameAssets& assets)
	{
		Build(maze, assets);
	}

	MazeGame::~MazeGame()
	{
		// Closing the window part-way through a game ends it without a GameOver.
		if (valid) SaveHighScore(world);
	}

	bool MazeGame::WantsToQuit() const
	{
		const GameState* state = world.resources.TryGet<GameState>();
		return state != nullptr && state->quitRequested;
	}

	void MazeGame::Build(const TileMap& maze, const GameAssets& assets)
	{
		using engine::ecs::Phase;
		using engine::graphics::Animation;
		using engine::graphics::Sprite;
		using engine::graphics::Transform;

		world.resources.Emplace<Controls>();
		world.resources.Emplace<GameState>();
		world.resources.Emplace<GameFlow>(GameFlow{.stage = Stage::Title});
		world.resources.Emplace<MenuCursor>();
		world.resources.Emplace<Score>();
		world.resources.Emplace<ModeClock>();
		world.resources.Emplace<EnemyDice>();
		world.resources.Emplace<HighScore>();
		world.resources.Emplace<SaveFile>(); // no path: saves nothing until MazeGame() sets one
		world.resources.Emplace<SoundBank>();
		world.resources.Emplace<SoundQueue>();
		const DifficultyTable& difficulty = world.resources.Emplace<DifficultyTable>(assets.difficulty);
		auto& renderer2D = world.resources.Emplace<engine::graphics::Renderer2D>(
		    engine::graphics::Camera{.logicalWidth = kLogicalWidth, .logicalHeight = kLogicalHeight});
		renderer2D.SetAtlas(assets.atlas);
		renderer2D.SetFont(assets.font);
		const TileMap& map = world.resources.Emplace<TileMap>(maze);
		world.resources.Emplace<EnemyHouse>(EnemyHouse{.house = FindHouse(map)});

		engine::ecs::Registry& registry = world.registry;
		const engine::graphics::Position origin = MazeOrigin(map);
		const SpriteClips& clips = world.resources.Emplace<SpriteClips>(assets.clips);
		SpawnMaze(registry, map, origin, clips);

		const std::vector<TilePosition> spawns = map.Find(Tile::PlayerSpawn);
		ENGINE_VERIFY(spawns.size() == 1, "the maze needs exactly one player spawn; ParseTileMap checks this");
		const engine::ecs::Entity player = registry.Create();
		const GridMover mover{.tile = spawns.front(), .speed = difficulty.ForLevel(1).playerSpeed};
		registry.Add<Transform>(player, MoverTopLeft(origin, mover));
		registry.Add<GridMover>(player, mover);
		registry.Add<engine::graphics::Interpolation>(player, kMoverInterpolation);
		registry.Add<Spawn>(player, Spawn{.tile = mover.tile});
		registry.Add<PlayerControlled>(player);
		registry.Add<Facing>(player);
		registry.Add<Sprite>(player, Sprite{.layer = kPlayerLayer});
		registry.Add<Animation>(player, Animation{.clip = clips.playerDown});
		SpawnEnemies(registry, map, origin, clips);
		SpawnHud(registry, map, origin);
		SpawnScreens(registry);

		// The whole frame, in run order.
		scheduler.Add(Phase::Input, "Controls", ControlsSystem);
		scheduler.Add(Phase::Input, "Screen", ScreenSystem);
		scheduler.Add(Phase::Input, "GameState", GameStateSystem);
		scheduler.Add(Phase::Update, "GameFlow", GameFlowSystem, NotPaused);
		scheduler.Add(Phase::Update, "PlayerInput", PlayerInputSystem, IsPlaying);
		scheduler.Add(Phase::Update, "EnemyMode", EnemyModeSystem, IsPlaying);
		scheduler.Add(Phase::Update, "EnemySteering", EnemySteeringSystem, IsPlaying);
		scheduler.Add(Phase::Update, "GridMovement", GridMovementSystem, IsPlaying);
		scheduler.Add(Phase::LateUpdate, "Collision", CollisionSystem, IsPlaying);
		scheduler.Add(Phase::LateUpdate, "EnemyLook", EnemyLookSystem);
		scheduler.Add(Phase::LateUpdate, "PlayerLook", PlayerLookSystem);
		scheduler.Add(Phase::LateUpdate, "Animation", engine::graphics::AnimationSystem, NotPaused);
		scheduler.Add(Phase::LateUpdate, "Hud", HudSystem);
		scheduler.Add(Phase::LateUpdate, "ScreenText", ScreenTextSystem);
		scheduler.Add(Phase::LateUpdate, "Audio", AudioSystem);
		engine::debug::AddDebugPanel(world, scheduler); // F3; before RenderSystem, so its text is current
		scheduler.Add(Phase::Render, "Render", RenderSystem);

		// Who reacts to what, in delivery order.
		world.events.Subscribe<PelletEaten>(AddPelletScore);
		world.events.Subscribe<PelletEaten>(CheckLevelCleared<PelletEaten>);
		world.events.Subscribe<PelletEaten>(QueueSound<PelletEaten>);
		world.events.Subscribe<PowerPelletEaten>(AddPowerPelletScore);
		world.events.Subscribe<PowerPelletEaten>(CheckLevelCleared<PowerPelletEaten>);
		world.events.Subscribe<PowerPelletEaten>(FrightenEnemies);
		world.events.Subscribe<PowerPelletEaten>(QueueSound<PowerPelletEaten>);
		world.events.Subscribe<EnemyEaten>(AddEnemyScore);
		world.events.Subscribe<EnemyEaten>(SendEnemyHome);
		world.events.Subscribe<EnemyEaten>(QueueSound<EnemyEaten>);
		world.events.Subscribe<PlayerCaught>(LoseLife);
		world.events.Subscribe<PlayerCaught>(QueueSound<PlayerCaught>);
		world.events.Subscribe<LevelCleared>(CompleteLevel);
		world.events.Subscribe<LevelCleared>(QueueSound<LevelCleared>);
		world.events.Subscribe<StageChanged>(SaveHighScoreOnStageChange); // first: before ResetRound clears the score
		world.events.Subscribe<StageChanged>(ResetRound);
		world.events.Subscribe<StageChanged>(ResetEnemies);
	}
} // namespace game
