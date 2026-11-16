#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/text.hpp"
#include "graphics/image.hpp"

// Pixel art written as text, so sprites and font glyphs can be drawn by hand, read in a diff
// and loaded without an image library. The format (docs/pixel-sheets.md):
//
//     # A comment. Blank lines and comments may go between entries.
//     color X 255 210 0          character, red, green, blue and an optional alpha (0-255)
//     sprite player 8 8 2        name, frame width, frame height, and optionally frames (1)
//     ...XX.......XX...          then frame-height rows, each frames * frame-width characters
//     ..XXXX.....XXXX..          ('.' is transparent; any other character must be a colour)
//
// The frames of a sprite sit side by side in its rows, so they read like a strip of film, and
// become one image frames * width pixels wide.
namespace engine::graphics
{
	struct SheetSprite
	{
		std::string name{};
		int frameWidth = 0;
		int frameHeight = 0;
		int frames = 1;
		Image image{}; // frames * frameWidth by frameHeight pixels, frame 0 on the left
	};

	// The sprites of one file, in the order they were written.
	struct PixelSheet
	{
		std::vector<SheetSprite> sprites;

		// The sprite with this name, or nullptr.
		[[nodiscard]] const SheetSprite* Find(std::string_view name) const noexcept;
	};

	// Either a sheet, or the first error found and no sheet.
	struct PixelSheetResult
	{
		std::optional<PixelSheet> sheet;
		core::ParseError error;
	};

	// Parses a pixel sheet. Pure: no files. Sprite names must be unique; a colour character
	// may be defined once and must be defined before it is used.
	[[nodiscard]] PixelSheetResult ParsePixelSheet(std::string_view text);

	// Reads a file and parses it. A file that can't be read is an error about the whole file.
	[[nodiscard]] PixelSheetResult LoadPixelSheet(const std::filesystem::path& file);
} // namespace engine::graphics
