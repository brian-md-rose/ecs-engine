#pragma once
#include <optional>
#include <random>

#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "enemymodes.hpp"
#include "enemytuning.hpp"
#include "events.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "targeting.hpp"
#include "tilemap.hpp"

// The enemies' resources, setup, systems and event handlers. The decisions themselves are the
// pure functions of steering.hpp, enemymodes.hpp and targeting.hpp; these apply them to the
// world. MazeGame registers the systems in this order (see docs/enemies.md).
namespace game
{
	// Resource: the maze's house, found once at setup. Nothing if the maze has no door; then
	// enemies start out in the maze, and an eaten one goes back to its spawn.
	struct EnemyHouse
	{
		std::optional<House> house;
	};

	// Resource: where frightened enemies' random turns come from. Seeded, so a game plays the
	// same way each time it gets the same input.
	struct EnemyDice
	{
		std::mt19937 engine{kEnemyRandomSeed};
	};

	// Setup: creates an enemy on every enemy spawn of map, with the personalities in turn
	// (PersonalityForSpawn), each waiting in the house for its release time. Creates entities
	// directly, so call it during setup, not from a system.
	void SpawnEnemies(engine::ecs::Registry& registry, const TileMap& map, engine::graphics::Position origin);

	// How an enemy looks right now: its personality's shape, drained of colour while it is
	// frightened (flashing kFrightFlashColor in the fright's last kFrightWarningSeconds), or a
	// small dot on its way home.
	[[nodiscard]] engine::graphics::Shape EnemyLook(Personality personality, const EnemyBrain& brain,
	                                                const ModeClock& clock) noexcept;

	// Update (only while playing), after PlayerInputSystem and before GridMovementSystem

	// Steps the ModeClock on the level's schedule. When the schedule switches, roaming enemies
	// turn back; when a fright ends, they calm down. Lets enemies out of the house on time.
	void EnemyModeSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Sets each enemy's speed, whether the door lets it through, and the way it turns next:
	// towards the target its goal calls for (GoalFor, ChaseTarget, PatrolTarget, the house) or,
	// while frightened, at random. An enemy decides each tile's turn on its way to the tile's
	// centre; GridMovementSystem takes the turn there. Also notices arrivals at the house's
	// exit (out) and home (back in), and carries out pending turn-backs.
	void EnemySteeringSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate (always, so a reset shows at once)

	// Gives every enemy its EnemyLook and centres it on its tile.
	void EnemyLookSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Event handlers. MazeGame subscribes them in this order.

	// PowerPelletEaten: starts a fright, and frightens every enemy that isn't going home.
	void FrightenEnemies(engine::ecs::World& world, const PowerPelletEaten& event);

	// EnemyEaten: adds EnemyPoints for this fright to the score (then counts the enemy as
	// eaten), and sends the enemy home.
	void AddEnemyScore(engine::ecs::World& world, const EnemyEaten& event);
	void SendEnemyHome(engine::ecs::World& world, const EnemyEaten& event);

	// StageChanged: when the actors are put back (ResetFor), the enemies go back into the house
	// to wait for their release times again, and the ModeClock starts over.
	void ResetEnemies(engine::ecs::World& world, const StageChanged& event);
} // namespace game
