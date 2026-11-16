#include "enemysystems.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "gameflow.hpp"
#include "graphics/sprite.hpp"
#include "grid/direction.hpp"
#include "gridmovement.hpp"
#include "mazegame.hpp"
#include "scoring.hpp"
#include "steering.hpp"
#include "systems.hpp"

namespace game
{
	using engine::grid::Direction;

	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::FrameContext;
		using engine::ecs::World;
		using engine::graphics::Animation;
		using engine::graphics::Position;
		using engine::graphics::Sprite;
		using engine::graphics::Transform;

		// Where every personality is, for Clasp's ally. The first of each if a maze has several.
		using Whereabouts = std::array<std::optional<TilePosition>, kPersonalityCount>;

		[[nodiscard]] Whereabouts FindPersonalities(engine::ecs::Registry& registry)
		{
			Whereabouts where{};
			registry.Each<EnemyPersonality, GridMover>(
			    [&where](Entity, const EnemyPersonality& who, const GridMover& mover)
			{
				std::optional<TilePosition>& slot = where[static_cast<std::size_t>(who.kind)];
				if (!slot) slot = mover.tile;
			});
			return where;
		}

		// The player's mover; nothing if there is no player.
		[[nodiscard]] std::optional<GridMover> FindPlayer(engine::ecs::Registry& registry)
		{
			std::optional<GridMover> player;
			registry.Each<PlayerControlled, GridMover>(
			    [&player](Entity, const PlayerControlled&, const GridMover& mover)
			{
				player = mover;
			});
			return player;
		}
	} // namespace

	void SpawnEnemies(engine::ecs::Registry& registry, const TileMap& map, Position origin, const SpriteClips& clips)
	{
		const std::vector<TilePosition> spawns = map.Find(Tile::EnemySpawn);
		for (std::size_t i = 0; i < spawns.size(); ++i)
		{
			const Personality personality = PersonalityForSpawn(i);
			const GridMover mover{.tile = spawns[i]};

			const Entity enemy = registry.Create();
			registry.Add<Transform>(enemy, MoverTopLeft(origin, mover));
			registry.Add<Sprite>(enemy, Sprite{.layer = kEnemyLayer});
			registry.Add<Animation>(enemy, Animation{.clip = EnemyClip(clips, personality, EnemyAppearance::Normal)});
			registry.Add<GridMover>(enemy, mover);
			registry.Add<engine::graphics::Interpolation>(enemy, kMoverInterpolation);
			registry.Add<Spawn>(enemy, Spawn{.tile = mover.tile});
			registry.Add<Enemy>(enemy);
			registry.Add<EnemyPersonality>(enemy, EnemyPersonality{.kind = personality});
			registry.Add<EnemyBrain>(enemy, EnemyBrain{.releaseSeconds = TuningFor(personality).releaseSeconds});
		}
	}

	EnemyAppearance EnemyLook(const EnemyBrain& brain, const ModeClock& clock) noexcept
	{
		if (brain.state == EnemyState::Returning) return EnemyAppearance::Returning;
		if (!brain.frightened) return EnemyAppearance::Normal;
		if (clock.frightSeconds <= kFrightWarningSeconds)
		{
			// Half a flash each way: the flash on every other half.
			const auto halves = static_cast<int>(clock.frightSeconds * kFrightFlashesPerSecond * 2.0f);
			if (halves % 2 == 1) return EnemyAppearance::FrightFlash;
		}
		return EnemyAppearance::Frightened;
	}

	const engine::graphics::Clip& EnemyClip(const SpriteClips& clips, Personality personality,
	                                        EnemyAppearance appearance) noexcept
	{
		switch (appearance)
		{
		case EnemyAppearance::Frightened:
			return clips.frightened;
		case EnemyAppearance::FrightFlash:
			return clips.frightenedFlash;
		case EnemyAppearance::Returning:
			return clips.returning;
		case EnemyAppearance::Normal:
			break;
		}
		return clips.enemies[static_cast<std::size_t>(personality)];
	}

	void EnemyModeSystem(World& world, const FrameContext& context)
	{
		const auto dt = static_cast<float>(context.dt);
		const ModeSchedule& schedule = LevelTuningOf(world).schedule;
		ModeClock& clock = world.resources.Get<ModeClock>();

		const ModeClock before = clock;
		clock = TickModeClock(clock, schedule, dt);
		const bool switched = CurrentMode(before, schedule) != CurrentMode(clock, schedule);
		const bool calmed = IsFrightActive(before) && !IsFrightActive(clock);

		world.registry.Each<EnemyBrain>([&](Entity, EnemyBrain& brain)
		{
			if (calmed) brain = Calm(brain);
			if (switched) brain = SwitchSchedule(brain);
			brain = TickHouse(brain, dt);
		});
	}

	void EnemySteeringSystem(World& world, const FrameContext&)
	{
		engine::ecs::Registry& registry = world.registry;
		const auto& maze = world.resources.Get<TileMap>();
		const std::optional<House>& house = world.resources.Get<EnemyHouse>().house;
		const LevelTuning& tuning = LevelTuningOf(world);
		const ScheduledMode mode = CurrentMode(world.resources.Get<ModeClock>(), tuning.schedule);
		std::mt19937& dice = world.resources.Get<EnemyDice>().engine;
		const std::optional<GridMover> player = FindPlayer(registry);
		const Whereabouts where = FindPersonalities(registry);

		registry.Each<EnemyBrain, EnemyPersonality, GridMover, Spawn>(
		    [&](Entity, EnemyBrain& brain, const EnemyPersonality& who, GridMover& mover, const Spawn& spawn)
		{
			// Arrivals first: they change where it is going. Without a house there is no door to
			// pass, and an eaten enemy's home is its spawn.
			if (brain.state == EnemyState::Leaving && (!house || mover.tile == house->exit))
			{
				brain = ArriveOutside(brain);
			}
			if (brain.state == EnemyState::Returning && mover.tile == (house ? house->home : spawn.tile))
			{
				brain = ArriveHome(brain);
			}
			mover.speed = EnemySpeedFor(brain, tuning);
			mover.throughDoor = UsesDoor(brain);

			if (brain.reversePending)
			{
				brain.reversePending = false;
				if (mover.direction != Direction::None)
				{
					mover.buffered = Opposite(mover.direction); // StepGridMover turns back at once
					return;
				}
			}

			// Past the centre, this tile's turn has been taken: go on to the next tile.
			const float along = mover.offset * Sign(mover.direction);
			if (mover.direction != Direction::None && along > 0.0f)
			{
				mover.buffered = mover.direction;
				return;
			}

			Direction turn = Direction::None;
			const Goal goal = GoalFor(brain, mode);
			if (goal == Goal::Wander)
			{
				turn = SteerRandomly(maze, mover.tile, mover.direction, mover.throughDoor,
				                     static_cast<std::uint32_t>(dice()));
			}
			else if (goal != Goal::Stay)
			{
				Target target = ToTarget(spawn.tile);
				switch (goal)
				{
				case Goal::Exit:
					if (house) target = ToTarget(house->exit);
					break;
				case Goal::Home:
					target = ToTarget(house ? house->home : spawn.tile);
					break;
				case Goal::Patrol:
					target = PatrolTarget(maze, who.kind);
					break;
				case Goal::Chase:
					if (player)
					{
						const std::optional<TilePosition>& ally = where[static_cast<std::size_t>(kClaspAlly)];
						const ChaseView view{.self = mover.tile,
						                     .player = player->tile,
						                     .playerHeading = player->direction,
						                     .ally = ally.value_or(mover.tile)};
						target = ChaseTarget(maze, who.kind, view);
					}
					break;
				case Goal::Stay:
				case Goal::Wander:
					break;
				}
				turn = SteerTowards(maze, mover.tile, mover.direction, target, mover.throughDoor);
			}

			// The way back is only ever offered at a dead end. Asking StepGridMover for it would
			// turn the enemy round on the spot, so carry on to the centre instead: it stops there
			// against the wall, and next time it leaves by the only way open.
			if (turn == Opposite(mover.direction) && along < 0.0f) turn = mover.direction;
			mover.buffered = turn;
		});
	}

	void EnemyLookSystem(World& world, const FrameContext&)
	{
		const ModeClock& clock = world.resources.Get<ModeClock>();
		const Position origin = MazeOrigin(world.resources.Get<TileMap>());
		const SpriteClips& clips = world.resources.Get<SpriteClips>();
		world.registry.Each<EnemyPersonality, EnemyBrain, GridMover, Animation, Transform>(
		    [&](Entity, const EnemyPersonality& who, const EnemyBrain& brain, const GridMover& mover,
		        Animation& animation, Transform& transform)
		{
			animation = Play(animation, EnemyClip(clips, who.kind, EnemyLook(brain, clock)));
			transform.position = MoverTopLeft(origin, mover);
		});
	}

	void FrightenEnemies(World& world, const PowerPelletEaten&)
	{
		ModeClock& clock = world.resources.Get<ModeClock>();
		clock = StartFright(clock, LevelTuningOf(world).frightSeconds);
		world.registry.Each<EnemyBrain>([](Entity, EnemyBrain& brain)
		{
			brain = Frighten(brain);
		});
	}

	void AddEnemyScore(World& world, const EnemyEaten&)
	{
		ModeClock& clock = world.resources.Get<ModeClock>();
		world.resources.Get<Score>().points += EnemyPoints(clock.eatenInFright);
		++clock.eatenInFright;
	}

	void SendEnemyHome(World& world, const EnemyEaten& event)
	{
		EnemyBrain* brain = world.registry.TryGetComponent<EnemyBrain>(event.enemy);
		if (brain != nullptr) *brain = Eat(*brain);
	}

	void ResetEnemies(World& world, const StageChanged& event)
	{
		if (!ResetFor(event.from, event.to).actors) return;
		world.resources.Get<ModeClock>() = ModeClock{};
		world.registry.Each<EnemyBrain>([](Entity, EnemyBrain& brain)
		{
			brain = EnemyBrain{.releaseSeconds = brain.releaseSeconds};
		});
	}
} // namespace game
