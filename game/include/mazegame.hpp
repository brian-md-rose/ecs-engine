#pragma once
#include <cstddef>
#include <cstdint>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "assets.hpp"
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

	inline constexpr std::uint8_t kPlayerLayer = 3; // above the maze and every stress-test shape
	inline constexpr int kStressShapeCount = 1000;
	inline constexpr engine::graphics::Color kPausedTint{128, 128, 128};

	// Where the maze's top-left tile is drawn: centred across the screen, below the HUD rows.
	[[nodiscard]] engine::graphics::Position MazeOrigin(const TileMap& maze);

	// The maze game: its resources, entities and systems. The player, steered along the maze grid
	// with the arrow keys or WASD, eats pellets for points; the HUD shows the score, the high
	// score, the lives left and the level. Four enemies leave the house one by one and hunt the
	// player, each in its own way (see docs/enemies.md); being caught costs a life. A power
	// pellet frightens them for a while, and a frightened enemy can be eaten for points. Each
	// round starts after a short Ready pause; clearing every pellet completes the level, which
	// restarts with its pellets back (see gameflow.hpp). The game opens on a title screen, and P
	// or Escape pauses it (see screens.hpp); F1 toggles the rendering stress test.
	class MazeGame final : public engine::application::ScheduledGame
	{
		bool valid = true;

		// Adds the resources (Controls, GameState, GameFlow, Score, HighScore, SoundBank,
		// SpriteClips, FrameStats, ModeClock, EnemyDice, EnemyHouse, Renderer2D with the assets,
		// and a copy of the maze),
		// spawns the maze, the player and the enemies at their spawns and the HUD, registers the
		// systems in run order and subscribes the event handlers.
		void Build(const TileMap& maze, const GameAssets& assets);

	public:
		// The window and logical resolution the game wants.
		static constexpr engine::application::ApplicationConfig kConfig{
		    .title = "Tangle Run",
		    .windowWidth = kLogicalWidth * kWindowScale,
		    .windowHeight = kLogicalHeight * kWindowScale,
		    .logicalWidth = kLogicalWidth,
		    .logicalHeight = kLogicalHeight,
		};

		// Starts on the first maze, assets/mazes/level1.txt next to the executable, with the rest
		// of assets/ (see LoadGameAssets). If something can't be loaded, the error is printed to
		// stderr and the game is not valid: don't run it.
		MazeGame();

		// Starts on the given maze, which ParseTileMap has accepted (tests build one in code).
		// Without assets nothing but shapes is drawn, which is all the tests need.
		explicit MazeGame(const TileMap& maze, const GameAssets& assets = {});

		// False if the assets couldn't be loaded.
		[[nodiscard]] bool IsValid() const noexcept { return valid; }

		// True once Quit was chosen on the title screen.
		[[nodiscard]] bool WantsToQuit() const override;
	};
} // namespace game
