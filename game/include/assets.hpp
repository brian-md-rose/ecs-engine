#pragma once
#include <filesystem>
#include <optional>
#include <span>

#include "difficulty.hpp"
#include "graphics/atlas.hpp"
#include "graphics/font.hpp"
#include "graphics/pixelsheet.hpp"

// Everything MazeGame loads from assets/ besides the maze: the pictures (packed into one atlas)
// and the font cut from it, and how hard each level is.
namespace game
{
	inline constexpr int kGlyphAdvance = 6;    // pixels from one character to the next
	inline constexpr int kGlyphLineHeight = 8; // and from one line to the next

	struct GameAssets
	{
		engine::graphics::Atlas atlas; // empty until loaded: then nothing but shapes is drawn
		engine::graphics::BitmapFont font;
		DifficultyTable difficulty = FallbackDifficulty(); // assets/levels.txt once loaded
	};

	// Packs the sheets into one atlas. Every sprite of fontSheet whose name is a single
	// character becomes that character's glyph. Pure: no files, no SDL.
	[[nodiscard]] GameAssets BuildGameAssets(const engine::graphics::PixelSheet& fontSheet);

	// Reads assets/sprites/font.txt and assets/levels.txt from folder (the assets folder) and
	// builds the assets. On failure, prints the error with the file, line and column to stderr
	// and returns nothing.
	[[nodiscard]] std::optional<GameAssets> LoadGameAssets(const std::filesystem::path& folder);
} // namespace game
