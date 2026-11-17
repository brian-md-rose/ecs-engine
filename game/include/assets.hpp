#pragma once
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "difficulty.hpp"
#include "enemytuning.hpp"
#include "graphics/animation.hpp"
#include "graphics/atlas.hpp"
#include "graphics/font.hpp"
#include "graphics/pixelsheet.hpp"
#include "walltiles.hpp"

// Everything MazeGame loads from assets/ besides the maze: the pictures (packed into one atlas),
// the font and the animation clips cut from it, and how hard each level is.
namespace game
{
	inline constexpr int kGlyphAdvance = 6;    // pixels from one character to the next
	inline constexpr int kGlyphLineHeight = 8; // and from one line to the next

	// Resource: every animation the game shows, cut from the atlas, with its timing. A clip that
	// was never loaded is empty (0 x 0) and draws nothing, which is what the tests run with.
	struct SpriteClips
	{
		engine::graphics::Clip playerRight; // drawn mirrored for left
		engine::graphics::Clip playerDown;
		engine::graphics::Clip playerUp;
		engine::graphics::Clip playerDeath; // plays once while Dying
		std::array<engine::graphics::Clip, kPersonalityCount> enemies{};
		engine::graphics::Clip frightened;
		engine::graphics::Clip frightenedFlash; // the fright's warning, alternating with frightened
		engine::graphics::Clip returning;
		engine::graphics::Clip pellet;
		engine::graphics::Clip powerPellet;
		engine::graphics::Clip door;
		std::array<engine::graphics::Clip, kWallTileCount> walls{}; // by WallMask
	};

	struct GameAssets
	{
		engine::graphics::Atlas atlas; // empty until loaded: then nothing but shapes is drawn
		engine::graphics::BitmapFont font;
		SpriteClips clips;
		DifficultyTable difficulty = FallbackDifficulty(); // assets/levels.txt once loaded
	};

	// The sprites the game needs that spriteSheet doesn't have.
	[[nodiscard]] std::vector<std::string> MissingSprites(const engine::graphics::PixelSheet& spriteSheet);

	// Packs the sheets into one atlas, with the sixteen wall tiles built from the wall pieces.
	// Every sprite of fontSheet whose name is a single character becomes that character's glyph;
	// spriteSheet's sprites become the clips. Sprites that are missing (MissingSprites) give
	// empty clips. Pure: no files, no SDL.
	[[nodiscard]] GameAssets BuildGameAssets(const engine::graphics::PixelSheet& fontSheet,
	                                         const engine::graphics::PixelSheet& spriteSheet);

	// Reads sprites/font and sprites/sprites (each a .sheet manifest of PNGs if there is one,
	// else the .txt pixel sheet) and levels.txt from folder (the assets folder) and builds the
	// assets. On failure, prints the error with the file, line and column (or the
	// missing sprites) to stderr and returns nothing.
	[[nodiscard]] std::optional<GameAssets> LoadGameAssets(const std::filesystem::path& folder);
} // namespace game
