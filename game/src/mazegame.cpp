#include "mazegame.hpp"
#include "menuui.hpp"

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
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/text.hpp"
#include "debug/debugpanel.hpp"
#include "difficulty.hpp"
#include "displaysettings.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/states.hpp"
#include "ecs/typeregistry.hpp"
#include "enemymodes.hpp"
#include "enemysystems.hpp"
#include "events.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "gamestates.hpp"
#include "graphics/animation.hpp"
#include "graphics/animationsystem.hpp"
#include "graphics/camera.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "gridmovement.hpp"
#include "hud.hpp"
#include "looks.hpp"
#include "nameentry.hpp"
#include "options.hpp"
#include "save.hpp"
#include "scoring.hpp"
#include "screens.hpp"
#include "sounds.hpp"
#include "systems.hpp"
#include "targeting.hpp"
#include "ui/ui.hpp"
#include "ui/uisystems.hpp"
#include "uidemo.hpp"

namespace game
{
	namespace
	{
		constexpr std::string_view kFirstMaze = "level1.txt";

		// Loads assets/mazes/<fileName> from next to the executable (see game/assets/README.md).
		// On failure, logs the error with the file, line and column and returns
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
				engine::core::LogError(message);
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
		world.resources.Get<MusicState>().file = folder / kMusicFile;

		SaveFile& save = world.resources.Get<SaveFile>();
		save.path = DefaultSaveFile();
		ApplySavedGame(world, LoadSavedGame(save.path));
	}

	MazeGame::MazeGame(const TileMap& maze, const GameAssets& assets)
	{
		Build(maze, assets);
	}

	MazeGame::~MazeGame()
	{
		// Closing the window part-way through a game ends it without a GameOver: a score that beat
		// the record still counts, under the name last used.
		if (!valid) return;
		RecordScore(world, NameOf(world.resources.Get<NameEntry>()));
		SaveHighScore(world);
		SaveSettings(world);
	}

	bool MazeGame::ShowSystemCursor() const
	{
		// The UI demo draws its own cursor.
		const auto* screen = world.resources.TryGet<engine::ecs::State<Screen>>();
		return screen == nullptr || !screen->Is(Screen::UiDemo);
	}

	bool MazeGame::WantsToQuit() const
	{
		const GameState* state = world.resources.TryGet<GameState>();
		return state != nullptr && state->quitRequested;
	}

	void MazeGame::Build(const TileMap& maze, const GameAssets& assets)
	{
		using engine::ecs::InState;
		using engine::ecs::NotInState;
		using engine::ecs::Phase;
		using engine::graphics::Animation;
		using engine::graphics::Sprite;
		using engine::graphics::Transform;

		world.resources.Emplace<Controls>();
		world.resources.Emplace<GameState>();
		world.resources.Emplace<GameFlow>();
		world.resources.Emplace<GameWorld>(); // the maze, player, enemies and HUD wait for START
		world.resources.Emplace<Settings>();
		world.resources.Emplace<NameEntry>();
		world.resources.Emplace<MenuCursor>();
		world.resources.Emplace<UiDemoState>();
		world.resources.Emplace<engine::graphics::DisplayRequest>();
		world.resources.Emplace<engine::graphics::DisplayInfo>();
		world.resources.Emplace<DisplayAsked>();
		world.resources.Emplace<engine::ui::PointerState>();
		world.resources.Emplace<MenuPointer>();
		world.resources.Emplace<Score>();
		world.resources.Emplace<ModeClock>();
		world.resources.Emplace<EnemyDice>();
		world.resources.Emplace<HighScore>();
		world.resources.Emplace<SaveFile>(); // no path: saves nothing until MazeGame() sets one
		world.resources.Emplace<SoundBank>();
		world.resources.Emplace<MusicState>(); // no file: MazeGame() points it at assets/music/theme.ogg
		world.resources.Emplace<SoundQueue>();
		world.resources.Emplace<DifficultyTable>(assets.difficulty);
		auto& renderer2D = world.resources.Emplace<engine::graphics::Renderer2D>(
		    engine::graphics::Camera{.logicalWidth = kLogicalWidth, .logicalHeight = kLogicalHeight});
		renderer2D.SetAtlas(assets.atlas);
		renderer2D.SetFont(assets.font);
		const TileMap& map = world.resources.Emplace<TileMap>(maze);
		world.resources.Emplace<EnemyHouse>(EnemyHouse{.house = FindHouse(map)});

		world.resources.Emplace<SpriteClips>(assets.clips);
		world.resources.Emplace<UiDemoScene>(UiDemoScene{assets.uiDemoScene});
		auto& types = world.resources.Emplace<engine::ecs::TypeRegistry>();
		engine::ecs::RegisterEngineComponents(types);
		RegisterUiDemoComponents(types);

		// The whole frame, in run order. A system for one screen carries an InState condition.
		scheduler.Add(Phase::Input, "Controls", ControlsSystem);
		scheduler.Add(Phase::Input, "Title", TitleSystem, InState<Screen::Title>);
		scheduler.Add(Phase::Input, "Options", OptionsSystem, InState<Screen::Options>);
		// The pointer works on every menu (and the pause menu), and a scroll view on the how-to-play and demo
		// screens. Focus moving is only the demo's: the menus move their cursor with Controls instead (which
		// already treats the d-pad and the south button as up, down and confirm), and the selected button holds
		// the Focused tag. The nine-patch system also tidies up after a screen.
		scheduler.Add(Phase::Input, "UiPointer", engine::ui::UiPointerSystem, InMenus);
		scheduler.Add(Phase::Input, "MenuHover", MenuHoverSystem, InMenus);
		scheduler.Add(Phase::Input, "UiFocus", engine::ui::UiFocusSystem, InState<Screen::UiDemo>);
		scheduler.Add(Phase::Input, "ScrollView", engine::ui::ScrollViewSystem, InState<Screen::Help, Screen::UiDemo>);
		scheduler.Add(Phase::Input, "Help", HelpSystem, InState<Screen::Help>);
		scheduler.Add(Phase::Input, "UiDemo", UiDemoSystem, InState<Screen::UiDemo>);
		scheduler.Add(Phase::Input, "NameEntry", NameEntrySystem, InState<Screen::NameEntry>);
		scheduler.Add(Phase::Input, "GameOver", GameOverSystem, InState<Screen::GameOver>);
		scheduler.Add(Phase::Input, "Pause", PauseSystem, InState<Screen::Playing>);
		scheduler.Add(Phase::Input, "GameState", GameStateSystem);
		scheduler.Add(Phase::Update, "RoundTimer", RoundTimerSystem, InState<Pause::Running>);
		scheduler.Add(Phase::Update, "LevelTransition", LevelTransitionSystem, InState<Screen::LevelTransition>);
		scheduler.Add(Phase::Update, "PlayerInput", PlayerInputSystem, IsRoundLive);
		scheduler.Add(Phase::Update, "EnemyMode", EnemyModeSystem, IsRoundLive);
		scheduler.Add(Phase::Update, "EnemySteering", EnemySteeringSystem, IsRoundLive);
		scheduler.Add(Phase::Update, "GridMovement", GridMovementSystem, IsRoundLive);
		scheduler.Add(Phase::LateUpdate, "Collision", CollisionSystem, IsRoundLive);
		scheduler.Add(Phase::LateUpdate, "EnemyLook", EnemyLookSystem);
		scheduler.Add(Phase::LateUpdate, "PlayerLook", PlayerLookSystem);
		scheduler.Add(Phase::LateUpdate, "Animation", engine::graphics::AnimationSystem, NotInState<Pause::Paused>);
		scheduler.Add(Phase::LateUpdate, "Hud", HudSystem);
		scheduler.Add(Phase::LateUpdate, "ScreenText", ScreenTextSystem);
		scheduler.Add(Phase::LateUpdate, "Display", DisplaySystem);
		scheduler.Add(Phase::LateUpdate, "MenuLook", MenuLookSystem, InMenus);
		scheduler.Add(Phase::LateUpdate, "UiDemoLook", UiDemoLookSystem, InState<Screen::UiDemo>);
		scheduler.Add(Phase::LateUpdate, "NinePatch", engine::ui::NinePatchSystem);
		scheduler.Add(Phase::LateUpdate, "Music", MusicSystem);
		scheduler.Add(Phase::LateUpdate, "Audio", AudioSystem);
		// F3 toggles the debug panel; it goes before RenderSystem, so its text is current.
		engine::debug::AddDebugPanel(world, scheduler);
		scheduler.Add(Phase::Render, "Render", RenderSystem);

		AddGameStates(world, scheduler);

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
		world.events.Subscribe<engine::ui::Clicked>(OnMenuClick);
		world.events.Subscribe<engine::ui::Clicked>(OnDemoClick);
	}
} // namespace game
