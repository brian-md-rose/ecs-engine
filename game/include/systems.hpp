#pragma once

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

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

	// Render

	// Draws every shape, dimmed while paused, and reports frame times during the stress test.
	void RenderSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
