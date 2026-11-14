#pragma once
#include <cstdint>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "graphics/color.hpp"

namespace game
{
	// Portrait, the shape of a classic maze screen, scaled kWindowScale times into the window.
	inline constexpr int kLogicalWidth = 224;
	inline constexpr int kLogicalHeight = 288;
	inline constexpr int kWindowScale = 3;

	inline constexpr float kSquareSize = 16.0f;
	inline constexpr float kPlayerSpeed = 90.0f;    // logical pixels per second
	inline constexpr std::uint8_t kPlayerLayer = 3; // above every stress-test shape
	inline constexpr int kStressShapeCount = 1000;
	inline constexpr engine::graphics::Color kPausedTint{128, 128, 128};

	// The maze game: its resources, entities and systems. For now a test bed for rendering and
	// input: a square steered with the arrow keys or WASD. P or Escape pauses; F1 toggles the
	// rendering stress test.
	class MazeGame final : public engine::application::ScheduledGame
	{
	public:
		// The window and logical resolution the game wants.
		static constexpr engine::application::ApplicationConfig kConfig
		{
		    .title = "ecs-engine",
		    .windowWidth = kLogicalWidth * kWindowScale,
		    .windowHeight = kLogicalHeight * kWindowScale,
		    .logicalWidth = kLogicalWidth,
		    .logicalHeight = kLogicalHeight,
		};

		// Adds the resources (Controls, GameState, FrameStats, Renderer2D), spawns the player,
		// and registers the systems in run order.
		MazeGame();
	};
} // namespace game
