#pragma once

#include "difficulty.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "events.hpp"

// The game's systems, in the phase each belongs to. MazeGame registers them in this order. The
// screens' own systems are in screens.hpp, and what happens when a screen starts or ends is in
// gamestates.hpp.
namespace game
{
	// The current level's tuning, from the DifficultyTable and GameFlow resources.
	[[nodiscard]] const LevelTuning& LevelTuningOf(const engine::ecs::World& world);

	// Run conditions. Everything else uses the engine's InState (ecs/states.hpp).

	// True while the round is being played: Round::Active, and not paused. The player moves,
	// eats and can be caught only then.
	[[nodiscard]] bool IsRoundLive(const engine::ecs::World& world);

	// What the screens do to the game in progress. They request the next screen through
	// NextState<Screen> and leave the rest to the enter systems.

	// Starts a game: the score back to 0, every life (Settings), level 1, and the pellets
	// respawned.
	void StartNewGame(engine::ecs::World& world);

	// The last life is gone: asks for the screen after a game (ScreenAfterGame).
	void EndGame(engine::ecs::World& world);

	// The player gave up from the pause menu: the name entry if the score beats the record, else
	// the title.
	void AbandonGame(engine::ecs::World& world);

	// Puts every mover back at its Spawn, running at the level's player speed if it is the
	// player, and the enemies back in the house to wait for their release times again.
	void ResetActors(engine::ecs::World& world);

	// Respawns every pellet and power pellet.
	void ResetPellets(engine::ecs::World& world);

	// Input

	// Steps Controls from the keyboard. Runs first, so every later system sees this update's
	// actions.
	void ControlsSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// The stress-test toggle. Runs on every screen. (The screens' input is in screens.hpp.)
	void GameStateSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Update

	// While Playing and not paused: counts the time in Ready and Dying and moves on when it is
	// up. Ready becomes Active; Dying becomes Ready, or ends the game once no lives are left.
	void RoundTimerSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// While on the level transition: moves on to the next level when its time is up.
	void LevelTransitionSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Only while the round is live:

	// Passes the last direction pressed to player-controlled grid movers. The player keeps
	// moving after the key is released, and a turn pressed before a junction is taken there.
	void PlayerInputSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Moves every grid mover through the maze (StepGridMover) and places its Transform to match.
	void GridMovementSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate (only while the round is live)

	// Collision by shared tile: the player eats the pellet or power pellet on its tile
	// (destroyed through world.commands; publishes PelletEaten or PowerPelletEaten), a
	// frightened enemy on its tile is eaten (publishes EnemyEaten), and any other enemy there
	// catches it unless it is on its way home (publishes PlayerCaught). Runs after everything
	// has moved.
	void CollisionSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Event handlers, delivered at the end of the phase that published the event (see
	// docs/decisions/0002-events.md). MazeGame subscribes them in this order.

	// Score: adds kPelletPoints or kPowerPelletPoints (enemies: see enemysystems.hpp).
	void AddPelletScore(engine::ecs::World& world, const PelletEaten& event);
	void AddPowerPelletScore(engine::ecs::World& world, const PowerPelletEaten& event);

	// Publishes LevelCleared once no pellet or power pellet is left. Events are delivered after
	// the phase's commands are applied, so the one just eaten is already gone.
	template <typename Eaten> void CheckLevelCleared(engine::ecs::World& world, const Eaten& event);

	// Flow: being caught costs a life and asks for Round::Dying, clearing the level asks for
	// Screen::LevelTransition. Whichever comes first in an update wins; the round must be
	// Active.
	void LoseLife(engine::ecs::World& world, const PlayerCaught& event);
	void CompleteLevel(engine::ecs::World& world, const LevelCleared& event);

	// Render

	// Draws everything, dimmed behind the screens that sit on top of the game, and reports frame
	// times during the stress test.
	void RenderSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
