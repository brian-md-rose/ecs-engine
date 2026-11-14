#pragma once
#include <cstddef>
#include <cstdint>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "graphics/color.hpp"
#include "graphics/transform.hpp"
#include "maze.hpp"
#include "tilemap.hpp"

namespace game
{
	// Portrait, the shape of a classic maze screen, scaled kWindowScale times into the window.
	inline constexpr int kLogicalWidth = 224;
	inline constexpr int kLogicalHeight = 288;
	inline constexpr int kWindowScale = 3;

	// Tile rows kept free above the maze for the score.
	inline constexpr std::size_t kHudRows = 2;

	inline constexpr float kPlayerSize = kTileSize;
	inline constexpr float kPlayerSpeed = 90.0f;    // logical pixels per second
	inline constexpr std::uint8_t kPlayerLayer = 3; // above the maze and every stress-test shape
	inline constexpr int kStressShapeCount = 1000;
	inline constexpr engine::graphics::Color kPausedTint{128, 128, 128};

	// Where the maze's top-left tile is drawn: centred across the screen, below the HUD rows.
	[[nodiscard]] engine::graphics::Position MazeOrigin(const TileMap& maze);

	// The maze game: its resources, entities and systems. For now it draws the maze, with a
	// square for the player steered with the arrow keys or WASD; it doesn't stop at walls yet.
	// P or Escape pauses; F1 toggles the rendering stress test.
	class MazeGame final : public engine::application::ScheduledGame
	{
		bool valid = true;

		// Adds the resources (Controls, GameState, FrameStats, Renderer2D and a copy of the
		// maze), spawns the maze and the player at its spawn, and registers the systems in run
		// order.
		void Build(const TileMap& maze);

	public:
		// The window and logical resolution the game wants.
		static constexpr engine::application::ApplicationConfig kConfig{
		    .title = "ecs-engine",
		    .windowWidth = kLogicalWidth * kWindowScale,
		    .windowHeight = kLogicalHeight * kWindowScale,
		    .logicalWidth = kLogicalWidth,
		    .logicalHeight = kLogicalHeight,
		};

		// Starts on the first maze, assets/mazes/level1.txt next to the executable. If it can't
		// be loaded, the error is printed to stderr and the game is not valid: don't run it.
		MazeGame();

		// Starts on the given maze, which ParseTileMap has accepted (tests build one in code).
		explicit MazeGame(const TileMap& maze);

		// False if the first maze couldn't be loaded.
		[[nodiscard]] bool IsValid() const noexcept { return valid; }
	};
} // namespace game
