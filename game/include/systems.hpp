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

	// Steers player-controlled entities toward the last direction pressed. They keep moving
	// after the key is released, the way the maze game's player will.
	void PlayerInputSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	void MovementSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate (skipped while paused)

	// Stops moving entities at the screen edges.
	void ClampToScreenSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// Render

	// Draws every shape, dimmed while paused, and reports frame times during the stress test.
	void RenderSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
