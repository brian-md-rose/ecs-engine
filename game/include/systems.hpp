#pragma once

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "events.hpp"

// The game's systems, in the phase each belongs to. MazeGame registers them in this order.
namespace game
{
	// Run condition: false while the game is paused.
	[[nodiscard]] bool NotPaused(const engine::ecs::World& world);

	// Input

	// Steps Controls from the keyboard. Runs first, so every later system sees this update's
	// actions.
	void ControlsSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Pause and the stress-test toggle. Runs even while paused, so the game can be unpaused.
	void GameStateSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Update (skipped while paused)

	// Passes the last direction pressed to player-controlled grid movers. The player keeps
	// moving after the key is released, and a turn pressed before a junction is taken there.
	void PlayerInputSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Moves every grid mover through the maze (StepGridMover) and places its Transform to match.
	void GridMovementSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate (skipped while paused)

	// Collision by shared tile: the player eats the pellet or power pellet on its tile
	// (destroyed through world.commands; publishes PelletEaten or PowerPelletEaten), and an
	// enemy on the player's tile catches it (publishes PlayerCaught). Runs after everything
	// has moved.
	void CollisionSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Event handlers, delivered at the end of the phase that published the event (see
	// docs/decisions/0002-events.md). MazeGame subscribes them in this order.

	// Score: adds kPelletPoints or kPowerPelletPoints.
	void AddPelletScore(engine::ecs::World& world, const PelletEaten& event);
	void AddPowerPelletScore(engine::ecs::World& world, const PowerPelletEaten& event);

	// HUD: prints the score, if the ConsoleHud is enabled.
	template <typename Eaten> void PrintScore(engine::ecs::World& world, const Eaten& event);

	// Publishes LevelCleared once no pellet or power pellet is left. Events are delivered after
	// the phase's commands are applied, so the one just eaten is already gone.
	template <typename Eaten> void CheckLevelCleared(engine::ecs::World& world, const Eaten& event);

	// Render

	// Draws every shape, dimmed while paused, and reports frame times during the stress test.
	void RenderSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
