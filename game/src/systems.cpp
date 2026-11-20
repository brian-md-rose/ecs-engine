#include "systems.hpp"

#include <cstdint>

#include "assets.hpp"
#include "components.hpp"
#include "controls.hpp"
#include "core/assert.hpp"
#include "core/random.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/states.hpp"
#include "enemymodes.hpp"
#include "enemysystems.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/color.hpp"
#include "graphics/renderer.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "gridmovement.hpp"
#include "hud.hpp"
#include "maze.hpp"
#include "mazegame.hpp"
#include "mazescene.hpp"
#include "options.hpp"
#include "scoring.hpp"
#include "screens.hpp"
#include "tilemap.hpp"

namespace game
{
	using engine::ecs::InState;
	using engine::ecs::NextState;
	using engine::ecs::State;
	using engine::grid::Direction;

	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::FrameContext;
		using engine::ecs::World;
		using engine::graphics::Color;
		using engine::graphics::Position;
		using engine::graphics::Shape;
		using engine::graphics::ShapeKind;
		using engine::graphics::Transform;

		// Debug: fills the screen with random rectangles and circles on layers below the
		// player, to check that hundreds of shapes draw at a steady frame rate. A fixed seed
		// keeps runs comparable.
		void SpawnStressShapes(World& world)
		{
			engine::ecs::Registry& registry = world.registry;
			engine::core::Random random(12345u);
			const auto between = [&random](float low, float high)
			{
				return low + random.Unit() * (high - low);
			};
			const auto channel = [&random]()
			{
				return static_cast<std::uint8_t>(random.Range(40, 255));
			};

			for (int i = 0; i < kStressShapeCount; ++i)
			{
				const Color color{channel(), channel(), channel()};
				const auto shapeLayer = static_cast<std::uint8_t>(random.Range(0, kPlayerLayer - 1));
				const float extent = between(2.0f, 8.0f);
				const Shape shape =
				    random.Chance(0.5f)
				        ? Shape{.kind = ShapeKind::Circle, .radius = extent / 2.0f, .color = color, .layer = shapeLayer}
				        : Shape{.width = extent, .height = extent, .color = color, .layer = shapeLayer};

				// Queued, like every structural change a system makes; the scheduler applies
				// them at the end of the phase.
				const Entity entity = registry.Create();
				world.commands.Add<Transform>(entity,
				                              Position{between(0.0f, static_cast<float>(kLogicalWidth) - 8.0f),
				                                       between(0.0f, static_cast<float>(kLogicalHeight) - 8.0f)});
				world.commands.Add<Shape>(entity, shape);
				world.commands.Add<StressShape>(entity);
			}
		}

		void DestroyPellets(World& world)
		{
			world.registry.Each<Pellet>([&world](Entity entity, const Pellet&)
			{
				world.commands.Destroy(entity);
			});
			world.registry.Each<PowerPellet>([&world](Entity entity, const PowerPellet&)
			{
				world.commands.Destroy(entity);
			});
		}

		void DestroyStressShapes(World& world)
		{
			world.registry.Each<StressShape>([&world](Entity entity, const StressShape&)
			{
				world.commands.Destroy(entity);
			});
		}
	} // namespace

	const LevelTuning& LevelTuningOf(const World& world)
	{
		return world.resources.Get<DifficultyTable>().ForLevel(world.resources.Get<GameFlow>().level);
	}

	bool IsRoundLive(const World& world)
	{
		return InState<Round::Active>(world) && InState<Pause::Running>(world);
	}

	void StartNewGame(World& world)
	{
		world.resources.Get<GameFlow>() = GameFlow{.lives = world.resources.Get<Settings>().lives};
		world.resources.Get<Score>().points = 0;
	}

	void EndGame(World& world)
	{
		const int score = world.resources.Get<Score>().points;
		const int record = world.resources.Get<HighScore>().record;
		world.resources.Get<NextState<Screen>>().Set(ScreenAfterGame(score, record));
	}

	void AbandonGame(World& world)
	{
		const bool record = world.resources.Get<Score>().points > world.resources.Get<HighScore>().record;
		world.resources.Get<NextState<Screen>>().Set(record ? Screen::NameEntry : Screen::Title);
	}

	void ResetPellets(World& world)
	{
		const auto& maze = world.resources.Get<TileMap>();
		DestroyPellets(world);
		SpawnPellets(world.registry, world.commands, maze, MazeOrigin(maze), world.resources.Get<SpriteClips>());
	}

	void SpawnGameWorld(World& world)
	{
		GameWorld& flag = world.resources.Get<GameWorld>();
		if (flag.spawned) return;
		flag.spawned = true;

		const auto& map = world.resources.Get<TileMap>();
		const Position origin = MazeOrigin(map);
		const auto& clips = world.resources.Get<SpriteClips>();
		// Entering the Title destroys a scene maze, so a game started after that spawns its own.
		if (flag.fromScene && HasSceneMaze(world.registry))
			ScopeMazeTiles(world.registry);
		else
			SpawnMaze(world.registry, map, origin, clips);
		SpawnPlayer(world.registry, map, origin, clips, LevelTuningOf(world).playerSpeed);
		SpawnEnemies(world.registry, map, origin, clips);
		if (flag.fromScene && HasSceneHud(world.registry))
			ScopeSceneHud(world.registry);
		else
			SpawnHud(world.registry, map, origin);
	}

	void ResetActors(World& world)
	{
		const Position origin = MazeOrigin(world.resources.Get<TileMap>());
		world.registry.Each<Spawn, GridMover, Transform>(
		    [origin](Entity, const Spawn& spawn, GridMover& mover, Transform& transform)
		{
			mover = GridMover{.tile = spawn.tile, .speed = mover.speed};
			transform.position = MoverTopLeft(origin, mover);
		});
		const float playerSpeed = LevelTuningOf(world).playerSpeed;
		world.registry.Each<PlayerControlled, GridMover>(
		    [playerSpeed](Entity, const PlayerControlled&, GridMover& mover)
		{
			mover.speed = playerSpeed;
		});
		// A direction pressed before the reset would send the player off at once.
		world.resources.Get<Controls>().ClearBufferedDirection();
		ResetEnemies(world);
	}

	void ControlsSystem(World& world, const FrameContext& context)
	{
		ENGINE_VERIFY(context.input != nullptr, "ControlsSystem must run in a fixed-rate phase");
		world.resources.Get<Controls>().Update(*context.input);
	}

	void GameStateSystem(World& world, const FrameContext&)
	{
		const auto& controls = world.resources.Get<Controls>();
		auto& state = world.resources.Get<GameState>();

		if (controls.WasPressed(Action::ToggleStressTest))
		{
			state.stressTest = !state.stressTest;
			if (state.stressTest)
			{
				SpawnStressShapes(world);
			}
			else
			{
				DestroyStressShapes(world);
			}
		}
	}

	void RoundTimerSystem(World& world, const FrameContext& context)
	{
		GameFlow& flow = world.resources.Get<GameFlow>();
		flow.seconds += static_cast<float>(context.dt);

		const Round round = world.resources.Get<State<Round>>().value;
		if (!RoundTimeUp(round, flow.seconds)) return;
		if (round == Round::Ready)
		{
			world.resources.Get<NextState<Round>>().Set(Round::Active);
		}
		else if (flow.lives > 0)
		{
			world.resources.Get<NextState<Round>>().Set(Round::Ready);
		}
		else
		{
			EndGame(world);
		}
	}

	void LevelTransitionSystem(World& world, const FrameContext& context)
	{
		GameFlow& flow = world.resources.Get<GameFlow>();
		flow.seconds += static_cast<float>(context.dt);
		if (flow.seconds >= kLevelTransitionSeconds) world.resources.Get<NextState<Screen>>().Set(Screen::Playing);
	}

	void PlayerInputSystem(World& world, const FrameContext&)
	{
		const Direction asked = world.resources.Get<Controls>().BufferedDirection();
		world.registry.Each<PlayerControlled, GridMover>([asked](Entity, const PlayerControlled&, GridMover& mover)
		{
			mover.buffered = asked;
		});
	}

	void GridMovementSystem(World& world, const FrameContext& context)
	{
		const auto dt = static_cast<float>(context.dt);
		const auto& maze = world.resources.Get<TileMap>();
		const Position origin = MazeOrigin(maze);
		world.registry.Each<GridMover, Transform>([&](Entity, GridMover& mover, Transform& transform)
		{
			mover = StepGridMover(mover, maze, dt);
			transform.position = MoverTopLeft(origin, mover);
		});
	}

	void CollisionSystem(World& world, const FrameContext&)
	{
		engine::ecs::Registry& registry = world.registry;
		registry.Each<PlayerControlled, GridMover>([&](Entity, const PlayerControlled&, const GridMover& player)
		{
			registry.Each<Pellet, OnTile>([&](Entity pellet, const Pellet&, const OnTile& on)
			{
				if (on.tile != player.tile) return;
				world.commands.Destroy(pellet);
				world.events.Publish(PelletEaten{.tile = on.tile});
			});
			registry.Each<PowerPellet, OnTile>([&](Entity pellet, const PowerPellet&, const OnTile& on)
			{
				if (on.tile != player.tile) return;
				world.commands.Destroy(pellet);
				world.events.Publish(PowerPelletEaten{.tile = on.tile});
			});
			registry.Each<Enemy, GridMover>([&](Entity enemy, const Enemy&, const GridMover& mover)
			{
				if (mover.tile != player.tile) return;
				// Enemies without a brain (tests place bare ones) are always dangerous.
				const EnemyBrain* brain = registry.TryGetComponent<EnemyBrain>(enemy);
				if (brain == nullptr || IsDangerous(*brain))
				{
					world.events.Publish(PlayerCaught{});
				}
				else if (CanBeEaten(*brain))
				{
					world.events.Publish(EnemyEaten{.enemy = enemy});
				}
			});
		});
	}

	void AddPelletScore(World& world, const PelletEaten&)
	{
		world.resources.Get<Score>().points += kPelletPoints;
	}

	void AddPowerPelletScore(World& world, const PowerPelletEaten&)
	{
		world.resources.Get<Score>().points += kPowerPelletPoints;
	}

	template <typename Eaten> void CheckLevelCleared(World& world, const Eaten&)
	{
		bool anyLeft = false;
		world.registry.Each<Pellet>([&anyLeft](Entity, const Pellet&)
		{
			anyLeft = true;
		});
		world.registry.Each<PowerPellet>([&anyLeft](Entity, const PowerPellet&)
		{
			anyLeft = true;
		});
		if (!anyLeft) world.events.Publish(LevelCleared{});
	}

	namespace
	{
		// True once the round has already been asked to change, or the game to leave Playing: the
		// first thing that happened this update decides, as the later events are about a round
		// that is already over.
		bool AlreadyEnding(const World& world)
		{
			return world.resources.Get<NextState<Round>>().requested.has_value() ||
			       world.resources.Get<NextState<Screen>>().requested.has_value();
		}
	} // namespace

	void LoseLife(World& world, const PlayerCaught&)
	{
		if (!InState<Round::Active>(world) || AlreadyEnding(world)) return;
		--world.resources.Get<GameFlow>().lives;
		world.resources.Get<NextState<Round>>().Set(Round::Dying);
	}

	void CompleteLevel(World& world, const LevelCleared&)
	{
		if (!InState<Round::Active>(world) || AlreadyEnding(world)) return;
		world.resources.Get<NextState<Screen>>().Set(Screen::LevelTransition);
	}

	template void CheckLevelCleared<PelletEaten>(World&, const PelletEaten&);
	template void CheckLevelCleared<PowerPelletEaten>(World&, const PowerPelletEaten&);

	void RenderSystem(World& world, const FrameContext& context)
	{
		ENGINE_VERIFY(context.renderer != nullptr, "RenderSystem must run in the Render phase");
		engine::graphics::Renderer& renderer = *context.renderer;
		auto& renderer2D = world.resources.Get<engine::graphics::Renderer2D>();

		renderer.Clear(0, 0, 0);
		// Behind the screens that sit on top of the game the game is dimmed; their text, above it,
		// is not.
		const bool behindScreen =
		    InState<Pause::Paused>(world) || InState<Screen::NameEntry, Screen::LevelTransition>(world);
		renderer2D.SetTint(behindScreen ? kPausedTint : Color{}, kMenuLayer);

		renderer2D.Draw(renderer, world.registry, static_cast<float>(context.alpha));
	}
} // namespace game
